#pragma once

// =============================================================================
// text_buffer.hpp — Text storage for the Xell Terminal IDE editor
// =============================================================================
// A line-based text buffer with:
//   - Line-indexed access (getLine, insertLine, deleteLine)
//   - Character-level editing (insertAt, deleteAt, deleteRange)
//   - Undo/redo stack (command pattern)
//   - Save/load from disk
//   - Modified flag tracking
//   - UTF-8 aware (stores raw UTF-8 strings)
//
// Uses a simple vector<string> for now — can be upgraded to a rope or
// piece table later for very large files.
// =============================================================================

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <memory>
#include <filesystem>

namespace xterm
{

    // ─── Cursor position in the buffer ───────────────────────────────────────

    struct BufferPos
    {
        int row = 0; // 0-based line index
        int col = 0; // 0-based column (byte offset within the line)

        bool operator==(const BufferPos &o) const { return row == o.row && col == o.col; }
        bool operator!=(const BufferPos &o) const { return !(*this == o); }
        bool operator<(const BufferPos &o) const
        {
            return row < o.row || (row == o.row && col < o.col);
        }
        bool operator<=(const BufferPos &o) const { return *this == o || *this < o; }
        bool operator>(const BufferPos &o) const { return o < *this; }
        bool operator>=(const BufferPos &o) const { return o <= *this; }
    };

    // ─── Undo/Redo command types ─────────────────────────────────────────────

    enum class EditAction
    {
        Insert,     // inserted text at position
        Delete,     // deleted text at position
        Replace,    // replaced text range
    };

    struct EditCommand
    {
        EditAction action;
        BufferPos pos;           // position where edit happened
        BufferPos endPos;        // end position (for delete/replace)
        std::string text;        // inserted/deleted text
        std::string replacedText; // old text (for replace/undo)
    };

    // ─── Text Buffer ─────────────────────────────────────────────────────────

    class TextBuffer
    {
    public:
        TextBuffer() { lines_.push_back(""); }

        // ── File I/O ────────────────────────────────────────────────────

        bool loadFromFile(const std::string &path)
        {
            std::ifstream f(path);
            if (!f.is_open())
                return false;

            lines_.clear();
            std::string line;
            while (std::getline(f, line))
            {
                // Remove \r from Windows line endings
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                lines_.push_back(std::move(line));
            }

            if (lines_.empty())
                lines_.push_back("");

            filePath_ = path;
            modified_ = false;
            undoStack_.clear();
            redoStack_.clear();
            return true;
        }

        bool saveToFile(const std::string &path = "") const
        {
            std::string savePath = path.empty() ? filePath_ : path;
            if (savePath.empty())
                return false;

            std::ofstream f(savePath);
            if (!f.is_open())
                return false;

            for (size_t i = 0; i < lines_.size(); i++)
            {
                f << lines_[i];
                if (i + 1 < lines_.size())
                    f << '\n';
            }

            return true;
        }

        void markSaved()
        {
            modified_ = false;
            savedUndoSize_ = undoStack_.size();
        }

        // ── Line access ─────────────────────────────────────────────────

        int lineCount() const { return (int)lines_.size(); }

        const std::string &getLine(int row) const
        {
            static const std::string empty;
            if (row < 0 || row >= (int)lines_.size())
                return empty;
            return lines_[row];
        }

        int lineLength(int row) const
        {
            if (row < 0 || row >= (int)lines_.size())
                return 0;
            return (int)lines_[row].size();
        }

        const std::vector<std::string> &lines() const { return lines_; }

        // ── Character editing ───────────────────────────────────────────

        // Insert a single character at position
        void insertChar(BufferPos pos, char ch)
        {
            insertText(pos, std::string(1, ch));
        }

        // Insert text (may contain newlines) at position
        void insertText(BufferPos pos, const std::string &text)
        {
            if (text.empty())
                return;

            clampPos(pos);

            // Record for undo
            EditCommand cmd;
            cmd.action = EditAction::Insert;
            cmd.pos = pos;
            cmd.text = text;

            // Perform the insert
            doInsert(pos, text);

            // Calculate end position after insert
            cmd.endPos = calcEndPos(pos, text);

            pushUndo(cmd);
            modified_ = true;
        }

        // Delete a single character at position (backspace-like: deletes char before pos)
        void deleteCharBefore(BufferPos pos)
        {
            if (pos.col > 0)
            {
                BufferPos from = {pos.row, pos.col - 1};
                deleteRange(from, pos);
            }
            else if (pos.row > 0)
            {
                // Join with previous line
                BufferPos from = {pos.row - 1, lineLength(pos.row - 1)};
                deleteRange(from, pos);
            }
        }

        // Delete character at position (delete key: deletes char at pos)
        void deleteCharAt(BufferPos pos)
        {
            clampPos(pos);
            if (pos.col < lineLength(pos.row))
            {
                BufferPos to = {pos.row, pos.col + 1};
                deleteRange(pos, to);
            }
            else if (pos.row + 1 < lineCount())
            {
                // Join with next line
                BufferPos to = {pos.row + 1, 0};
                deleteRange(pos, to);
            }
        }

        // Delete text in range [from, to)
        void deleteRange(BufferPos from, BufferPos to)
        {
            clampPos(from);
            clampPos(to);
            if (to < from)
                std::swap(from, to);
            if (from == to)
                return;

            // Extract deleted text for undo
            std::string deleted = extractText(from, to);

            EditCommand cmd;
            cmd.action = EditAction::Delete;
            cmd.pos = from;
            cmd.endPos = to;
            cmd.text = deleted;

            doDelete(from, to);

            pushUndo(cmd);
            modified_ = true;
        }

        // Insert a newline at position (Enter key)
        void insertNewline(BufferPos pos)
        {
            insertText(pos, "\n");
        }

        // Delete an entire line
        void deleteLine(int row)
        {
            if (row < 0 || row >= lineCount())
                return;

            if (lineCount() == 1)
            {
                // Can't delete the last line — just clear it
                deleteRange({0, 0}, {0, lineLength(0)});
                return;
            }

            BufferPos from, to;
            if (row + 1 < lineCount())
            {
                from = {row, 0};
                to = {row + 1, 0};
            }
            else
            {
                // Last line — delete including the preceding newline
                from = {row - 1, lineLength(row - 1)};
                to = {row, lineLength(row)};
            }
            deleteRange(from, to);
        }

        // Replace text in range
        void replaceRange(BufferPos from, BufferPos to, const std::string &newText)
        {
            clampPos(from);
            clampPos(to);
            if (to < from)
                std::swap(from, to);

            std::string oldText = extractText(from, to);

            EditCommand cmd;
            cmd.action = EditAction::Replace;
            cmd.pos = from;
            cmd.endPos = to;
            cmd.text = newText;
            cmd.replacedText = oldText;

            doDelete(from, to);
            doInsert(from, newText);

            pushUndo(cmd);
            modified_ = true;
        }

        // ── Undo / Redo ─────────────────────────────────────────────────

        bool canUndo() const { return !undoStack_.empty(); }
        bool canRedo() const { return !redoStack_.empty(); }

        BufferPos undo()
        {
            if (undoStack_.empty())
                return {0, 0};

            auto cmd = undoStack_.back();
            undoStack_.pop_back();

            switch (cmd.action)
            {
            case EditAction::Insert:
                doDelete(cmd.pos, cmd.endPos);
                redoStack_.push_back(cmd);
                modified_ = (undoStack_.size() != savedUndoSize_);
                return cmd.pos;

            case EditAction::Delete:
                doInsert(cmd.pos, cmd.text);
                redoStack_.push_back(cmd);
                modified_ = (undoStack_.size() != savedUndoSize_);
                return cmd.endPos;

            case EditAction::Replace:
                doDelete(cmd.pos, calcEndPos(cmd.pos, cmd.text));
                doInsert(cmd.pos, cmd.replacedText);
                redoStack_.push_back(cmd);
                modified_ = (undoStack_.size() != savedUndoSize_);
                return cmd.endPos;
            }
            return {0, 0};
        }

        BufferPos redo()
        {
            if (redoStack_.empty())
                return {0, 0};

            auto cmd = redoStack_.back();
            redoStack_.pop_back();

            switch (cmd.action)
            {
            case EditAction::Insert:
                doInsert(cmd.pos, cmd.text);
                undoStack_.push_back(cmd);
                modified_ = (undoStack_.size() != savedUndoSize_);
                return cmd.endPos;

            case EditAction::Delete:
                doDelete(cmd.pos, cmd.endPos);
                undoStack_.push_back(cmd);
                modified_ = (undoStack_.size() != savedUndoSize_);
                return cmd.pos;

            case EditAction::Replace:
                doDelete(cmd.pos, calcEndPos(cmd.pos, cmd.replacedText));
                doInsert(cmd.pos, cmd.text);
                undoStack_.push_back(cmd);
                modified_ = (undoStack_.size() != savedUndoSize_);
                return calcEndPos(cmd.pos, cmd.text);
            }
            return {0, 0};
        }

        // ── Text extraction ─────────────────────────────────────────────

        std::string extractText(BufferPos from, BufferPos to) const
        {
            clampPosConst(from);
            clampPosConst(to);
            if (to < from)
                std::swap(from, to);

            if (from.row == to.row)
            {
                return lines_[from.row].substr(from.col, to.col - from.col);
            }

            std::string result;
            // First line (from col to end)
            result += lines_[from.row].substr(from.col);
            result += '\n';

            // Middle lines (full)
            for (int r = from.row + 1; r < to.row; r++)
            {
                result += lines_[r];
                result += '\n';
            }

            // Last line (start to col)
            result += lines_[to.row].substr(0, to.col);

            return result;
        }

        // Get entire buffer as a string
        std::string getText() const
        {
            std::string result;
            for (size_t i = 0; i < lines_.size(); i++)
            {
                result += lines_[i];
                if (i + 1 < lines_.size())
                    result += '\n';
            }
            return result;
        }

        // ── Word navigation helpers ─────────────────────────────────────

        // Find the start of the word at or before pos
        BufferPos wordStart(BufferPos pos) const
        {
            clampPosConst(pos);
            if (pos.col == 0)
                return pos;

            const auto &line = lines_[pos.row];
            int c = pos.col;

            // Skip whitespace backwards
            while (c > 0 && (line[c - 1] == ' ' || line[c - 1] == '\t'))
                c--;

            // Skip word characters backwards
            while (c > 0 && isWordChar(line[c - 1]))
                c--;

            return {pos.row, c};
        }

        // Find the end of the word at or after pos
        BufferPos wordEnd(BufferPos pos) const
        {
            clampPosConst(pos);
            const auto &line = lines_[pos.row];
            int len = (int)line.size();
            int c = pos.col;

            // Skip word characters forward
            while (c < len && isWordChar(line[c]))
                c++;

            // Skip whitespace forward
            while (c < len && (line[c] == ' ' || line[c] == '\t'))
                c++;

            return {pos.row, c};
        }

        // ── Indentation helpers ─────────────────────────────────────────

        int getIndentLevel(int row) const
        {
            if (row < 0 || row >= lineCount())
                return 0;
            const auto &line = lines_[row];
            int indent = 0;
            for (char ch : line)
            {
                if (ch == ' ')
                    indent++;
                else if (ch == '\t')
                    indent += tabSize_;
                else
                    break;
            }
            return indent;
        }

        std::string makeIndent(int spaces) const
        {
            return std::string(spaces, ' ');
        }

        // Check if a line ends with ':' (Xell block opener)
        bool lineEndsWithColon(int row) const
        {
            if (row < 0 || row >= lineCount())
                return false;
            const auto &line = lines_[row];
            // Find last non-whitespace character
            for (int i = (int)line.size() - 1; i >= 0; i--)
            {
                if (line[i] == ' ' || line[i] == '\t')
                    continue;
                return line[i] == ':';
            }
            return false;
        }

        // ── Properties ──────────────────────────────────────────────────

        bool isModified() const { return modified_; }
        const std::string &filePath() const { return filePath_; }
        void setFilePath(const std::string &path) { filePath_ = path; }
        int tabSize() const { return tabSize_; }
        void setTabSize(int size) { tabSize_ = size; }

        std::string fileName() const
        {
            if (filePath_.empty())
                return "Untitled";
            return std::filesystem::path(filePath_).filename().string();
        }

    private:
        std::vector<std::string> lines_;
        std::string filePath_;
        bool modified_ = false;
        int tabSize_ = 4;

        std::vector<EditCommand> undoStack_;
        std::vector<EditCommand> redoStack_;
        size_t savedUndoSize_ = 0;

        // ── Internal operations (no undo tracking) ──────────────────────

        void doInsert(BufferPos pos, const std::string &text)
        {
            if (text.empty())
                return;

            // Split text into lines
            std::vector<std::string> newLines;
            std::istringstream ss(text);
            std::string segment;
            bool first = true;
            while (std::getline(ss, segment, '\n'))
            {
                if (!first)
                    newLines.push_back(segment);
                else
                {
                    newLines.push_back(segment);
                    first = false;
                }
            }
            // If text ends with newline, add empty line
            if (!text.empty() && text.back() == '\n')
                newLines.push_back("");

            if (newLines.size() == 1)
            {
                // Single line insert — just splice into the line
                lines_[pos.row].insert(pos.col, newLines[0]);
            }
            else
            {
                // Multi-line insert
                std::string tail = lines_[pos.row].substr(pos.col);
                lines_[pos.row] = lines_[pos.row].substr(0, pos.col) + newLines[0];

                for (size_t i = 1; i < newLines.size() - 1; i++)
                {
                    lines_.insert(lines_.begin() + pos.row + i, newLines[i]);
                }

                // Last new line + tail of original line
                lines_.insert(lines_.begin() + pos.row + newLines.size() - 1,
                              newLines.back() + tail);
            }
        }

        void doDelete(BufferPos from, BufferPos to)
        {
            if (from == to)
                return;
            if (to < from)
                std::swap(from, to);

            if (from.row == to.row)
            {
                // Same line — simple erase
                lines_[from.row].erase(from.col, to.col - from.col);
            }
            else
            {
                // Multi-line delete
                std::string merged = lines_[from.row].substr(0, from.col) +
                                     lines_[to.row].substr(to.col);

                // Remove lines from from.row+1 to to.row (inclusive)
                lines_.erase(lines_.begin() + from.row + 1,
                             lines_.begin() + to.row + 1);

                lines_[from.row] = merged;
            }
        }

        BufferPos calcEndPos(BufferPos start, const std::string &text) const
        {
            int row = start.row;
            int col = start.col;
            for (char ch : text)
            {
                if (ch == '\n')
                {
                    row++;
                    col = 0;
                }
                else
                {
                    col++;
                }
            }
            return {row, col};
        }

        void clampPos(BufferPos &pos) const
        {
            if (pos.row < 0)
                pos.row = 0;
            if (pos.row >= (int)lines_.size())
                pos.row = (int)lines_.size() - 1;
            if (pos.col < 0)
                pos.col = 0;
            if (pos.col > (int)lines_[pos.row].size())
                pos.col = (int)lines_[pos.row].size();
        }

        void clampPosConst(BufferPos &pos) const
        {
            if (pos.row < 0)
                pos.row = 0;
            if (pos.row >= (int)lines_.size())
                pos.row = (int)lines_.size() - 1;
            if (pos.col < 0)
                pos.col = 0;
            if (pos.col > (int)lines_[pos.row].size())
                pos.col = (int)lines_[pos.row].size();
        }

        void pushUndo(const EditCommand &cmd)
        {
            undoStack_.push_back(cmd);
            redoStack_.clear(); // any new edit invalidates redo
        }

        static bool isWordChar(char ch)
        {
            return (ch >= 'a' && ch <= 'z') ||
                   (ch >= 'A' && ch <= 'Z') ||
                   (ch >= '0' && ch <= '9') ||
                   ch == '_';
        }
    };

} // namespace xterm
