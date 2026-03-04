#pragma once

// =============================================================================
// editor_view.hpp — Editor viewport for the Xell Terminal IDE
// =============================================================================
// Maps a TextBuffer to a rectangular region of the screen. Handles:
//   - Scroll offset (vertical + horizontal)
//   - Cursor position & rendering
//   - Text selection
//   - Line numbers & gutter
//   - Syntax highlighting integration
//   - Visible range tracking
//
// Does NOT own the TextBuffer — takes a reference.
// Does NOT own the ScreenBuffer — writes cells into a provided region.
// =============================================================================

#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include "text_buffer.hpp"
#include "../terminal/types.hpp"
#include "../highlight/highlighter.hpp"
#include "../theme/theme_loader.hpp"

namespace xterm
{

    // ─── Selection state ─────────────────────────────────────────────────────

    struct Selection
    {
        BufferPos anchor; // where selection started
        BufferPos cursor; // where selection ends (cursor position)
        bool active = false;

        BufferPos start() const { return anchor < cursor ? anchor : cursor; }
        BufferPos end() const { return anchor < cursor ? cursor : anchor; }

        bool contains(BufferPos pos) const
        {
            if (!active)
                return false;
            BufferPos s = start(), e = end();
            if (pos.row < s.row || pos.row > e.row)
                return false;
            if (pos.row == s.row && pos.col < s.col)
                return false;
            if (pos.row == e.row && pos.col >= e.col)
                return false;
            return true;
        }
    };

    // ─── Rect: a screen region ───────────────────────────────────────────────

    struct Rect
    {
        int x, y, w, h; // in cell coordinates
    };

    // ─── Editor View ─────────────────────────────────────────────────────────

    class EditorView
    {
    public:
        EditorView(TextBuffer &buffer, const ThemeData &theme)
            : buffer_(buffer), theme_(theme), highlighter_(theme)
        {
            loadThemeColors();
        }

        // ── Layout: set the screen region this editor occupies ───────────

        void setRect(Rect rect)
        {
            rect_ = rect;
            recalcLayout();
        }

        Rect getRect() const { return rect_; }

        // ── Cursor ──────────────────────────────────────────────────────

        BufferPos cursor() const { return cursor_; }

        void setCursor(BufferPos pos)
        {
            clampCursor(pos);
            cursor_ = pos;
            selection_.cursor = pos;
            ensureCursorVisible();
        }

        void moveCursorUp(bool select = false)
        {
            if (select && !selection_.active)
                startSelection();
            if (cursor_.row > 0)
            {
                cursor_.row--;
                cursor_.col = std::min(cursor_.col, buffer_.lineLength(cursor_.row));
            }
            if (select)
                selection_.cursor = cursor_;
            else
                selection_.active = false;
            ensureCursorVisible();
        }

        void moveCursorDown(bool select = false)
        {
            if (select && !selection_.active)
                startSelection();
            if (cursor_.row < buffer_.lineCount() - 1)
            {
                cursor_.row++;
                cursor_.col = std::min(cursor_.col, buffer_.lineLength(cursor_.row));
            }
            if (select)
                selection_.cursor = cursor_;
            else
                selection_.active = false;
            ensureCursorVisible();
        }

        void moveCursorLeft(bool select = false)
        {
            if (select && !selection_.active)
                startSelection();
            if (cursor_.col > 0)
            {
                cursor_.col--;
            }
            else if (cursor_.row > 0)
            {
                cursor_.row--;
                cursor_.col = buffer_.lineLength(cursor_.row);
            }
            if (select)
                selection_.cursor = cursor_;
            else
                selection_.active = false;
            ensureCursorVisible();
        }

        void moveCursorRight(bool select = false)
        {
            if (select && !selection_.active)
                startSelection();
            if (cursor_.col < buffer_.lineLength(cursor_.row))
            {
                cursor_.col++;
            }
            else if (cursor_.row < buffer_.lineCount() - 1)
            {
                cursor_.row++;
                cursor_.col = 0;
            }
            if (select)
                selection_.cursor = cursor_;
            else
                selection_.active = false;
            ensureCursorVisible();
        }

        void moveCursorWordLeft(bool select = false)
        {
            if (select && !selection_.active)
                startSelection();
            cursor_ = buffer_.wordStart(cursor_);
            if (select)
                selection_.cursor = cursor_;
            else
                selection_.active = false;
            ensureCursorVisible();
        }

        void moveCursorWordRight(bool select = false)
        {
            if (select && !selection_.active)
                startSelection();
            cursor_ = buffer_.wordEnd(cursor_);
            if (select)
                selection_.cursor = cursor_;
            else
                selection_.active = false;
            ensureCursorVisible();
        }

        void moveCursorHome(bool select = false)
        {
            if (select && !selection_.active)
                startSelection();

            // Smart home: first non-whitespace, then column 0
            const auto &line = buffer_.getLine(cursor_.row);
            int firstNonWs = 0;
            while (firstNonWs < (int)line.size() && (line[firstNonWs] == ' ' || line[firstNonWs] == '\t'))
                firstNonWs++;

            if (cursor_.col == firstNonWs)
                cursor_.col = 0;
            else
                cursor_.col = firstNonWs;

            if (select)
                selection_.cursor = cursor_;
            else
                selection_.active = false;
            ensureCursorVisible();
        }

        void moveCursorEnd(bool select = false)
        {
            if (select && !selection_.active)
                startSelection();
            cursor_.col = buffer_.lineLength(cursor_.row);
            if (select)
                selection_.cursor = cursor_;
            else
                selection_.active = false;
            ensureCursorVisible();
        }

        void moveCursorToStart(bool select = false)
        {
            if (select && !selection_.active)
                startSelection();
            cursor_ = {0, 0};
            if (select)
                selection_.cursor = cursor_;
            else
                selection_.active = false;
            ensureCursorVisible();
        }

        void moveCursorToEnd(bool select = false)
        {
            if (select && !selection_.active)
                startSelection();
            cursor_.row = buffer_.lineCount() - 1;
            cursor_.col = buffer_.lineLength(cursor_.row);
            if (select)
                selection_.cursor = cursor_;
            else
                selection_.active = false;
            ensureCursorVisible();
        }

        void pageUp(bool select = false)
        {
            if (select && !selection_.active)
                startSelection();
            int visibleLines = codeAreaHeight();
            cursor_.row = std::max(0, cursor_.row - visibleLines);
            cursor_.col = std::min(cursor_.col, buffer_.lineLength(cursor_.row));
            scrollTopLine_ = std::max(0, scrollTopLine_ - visibleLines);
            if (select)
                selection_.cursor = cursor_;
            else
                selection_.active = false;
        }

        void pageDown(bool select = false)
        {
            if (select && !selection_.active)
                startSelection();
            int visibleLines = codeAreaHeight();
            cursor_.row = std::min(buffer_.lineCount() - 1, cursor_.row + visibleLines);
            cursor_.col = std::min(cursor_.col, buffer_.lineLength(cursor_.row));
            scrollTopLine_ += visibleLines;
            clampScroll();
            if (select)
                selection_.cursor = cursor_;
            else
                selection_.active = false;
        }

        // ── Selection ───────────────────────────────────────────────────

        Selection &selection() { return selection_; }
        const Selection &selection() const { return selection_; }

        void selectAll()
        {
            selection_.active = true;
            selection_.anchor = {0, 0};
            cursor_.row = buffer_.lineCount() - 1;
            cursor_.col = buffer_.lineLength(cursor_.row);
            selection_.cursor = cursor_;
        }

        void clearSelection() { selection_.active = false; }

        std::string getSelectedText() const
        {
            if (!selection_.active)
                return "";
            return buffer_.extractText(selection_.start(), selection_.end());
        }

        void deleteSelected()
        {
            if (!selection_.active)
                return;
            auto s = selection_.start();
            buffer_.deleteRange(selection_.start(), selection_.end());
            cursor_ = s;
            selection_.active = false;
        }

        // ── Editing commands ────────────────────────────────────────────

        void insertChar(char ch)
        {
            if (selection_.active)
                deleteSelected();
            buffer_.insertChar(cursor_, ch);
            cursor_.col++;
            ensureCursorVisible();
        }

        void insertText(const std::string &text)
        {
            if (selection_.active)
                deleteSelected();
            buffer_.insertText(cursor_, text);
            // Advance cursor past inserted text
            for (char ch : text)
            {
                if (ch == '\n')
                {
                    cursor_.row++;
                    cursor_.col = 0;
                }
                else
                {
                    cursor_.col++;
                }
            }
            ensureCursorVisible();
        }

        void insertNewline()
        {
            if (selection_.active)
                deleteSelected();

            // Auto-indent: if current line ends with ':', add indent
            int currentIndent = buffer_.getIndentLevel(cursor_.row);
            bool addIndent = buffer_.lineEndsWithColon(cursor_.row);

            buffer_.insertNewline(cursor_);
            cursor_.row++;
            cursor_.col = 0;

            // Insert indentation
            int indent = currentIndent + (addIndent ? buffer_.tabSize() : 0);
            if (indent > 0)
            {
                std::string indentStr = buffer_.makeIndent(indent);
                buffer_.insertText(cursor_, indentStr);
                cursor_.col = indent;
            }

            ensureCursorVisible();
        }

        void backspace()
        {
            if (selection_.active)
            {
                deleteSelected();
                return;
            }
            if (cursor_.col > 0 || cursor_.row > 0)
            {
                buffer_.deleteCharBefore(cursor_);
                if (cursor_.col > 0)
                    cursor_.col--;
                else if (cursor_.row > 0)
                {
                    cursor_.row--;
                    cursor_.col = buffer_.lineLength(cursor_.row);
                }
            }
            ensureCursorVisible();
        }

        void deleteForward()
        {
            if (selection_.active)
            {
                deleteSelected();
                return;
            }
            buffer_.deleteCharAt(cursor_);
            ensureCursorVisible();
        }

        void deleteLine()
        {
            buffer_.deleteLine(cursor_.row);
            if (cursor_.row >= buffer_.lineCount())
                cursor_.row = buffer_.lineCount() - 1;
            cursor_.col = std::min(cursor_.col, buffer_.lineLength(cursor_.row));
            ensureCursorVisible();
        }

        void duplicateLine()
        {
            const auto &line = buffer_.getLine(cursor_.row);
            BufferPos eol = {cursor_.row, buffer_.lineLength(cursor_.row)};
            buffer_.insertText(eol, "\n" + line);
            cursor_.row++;
            ensureCursorVisible();
        }

        void undo()
        {
            if (buffer_.canUndo())
            {
                cursor_ = buffer_.undo();
                selection_.active = false;
                ensureCursorVisible();
            }
        }

        void redo()
        {
            if (buffer_.canRedo())
            {
                cursor_ = buffer_.redo();
                selection_.active = false;
                ensureCursorVisible();
            }
        }

        void indent()
        {
            if (selection_.active)
            {
                // Indent all selected lines
                auto s = selection_.start();
                auto e = selection_.end();
                for (int r = s.row; r <= e.row; r++)
                {
                    buffer_.insertText({r, 0}, buffer_.makeIndent(buffer_.tabSize()));
                }
                selection_.anchor.col += buffer_.tabSize();
                selection_.cursor.col += buffer_.tabSize();
                cursor_.col += buffer_.tabSize();
            }
            else
            {
                std::string tab = buffer_.makeIndent(buffer_.tabSize());
                buffer_.insertText(cursor_, tab);
                cursor_.col += buffer_.tabSize();
            }
            ensureCursorVisible();
        }

        void outdent()
        {
            if (selection_.active)
            {
                auto s = selection_.start();
                auto e = selection_.end();
                for (int r = s.row; r <= e.row; r++)
                {
                    const auto &line = buffer_.getLine(r);
                    int remove = 0;
                    for (int i = 0; i < buffer_.tabSize() && i < (int)line.size(); i++)
                    {
                        if (line[i] == ' ')
                            remove++;
                        else
                            break;
                    }
                    if (remove > 0)
                        buffer_.deleteRange({r, 0}, {r, remove});
                }
            }
            else
            {
                const auto &line = buffer_.getLine(cursor_.row);
                int remove = 0;
                for (int i = 0; i < buffer_.tabSize() && i < (int)line.size(); i++)
                {
                    if (line[i] == ' ')
                        remove++;
                    else
                        break;
                }
                if (remove > 0)
                {
                    buffer_.deleteRange({cursor_.row, 0}, {cursor_.row, remove});
                    cursor_.col = std::max(0, cursor_.col - remove);
                }
            }
            ensureCursorVisible();
        }

        // ── Scrolling ───────────────────────────────────────────────────

        int scrollTopLine() const { return scrollTopLine_; }

        void scrollTo(int line)
        {
            scrollTopLine_ = line;
            clampScroll();
        }

        void scrollBy(int delta)
        {
            scrollTopLine_ += delta;
            clampScroll();
        }

        // ── Rendering to a cell grid ────────────────────────────────────

        // Render the editor view into a 2D grid of cells.
        // The grid should be rect_.w × rect_.h cells.
        // Returns the cells to be written to the screen buffer.
        struct RenderOutput
        {
            std::vector<std::vector<Cell>> cells; // [row][col]
            int cursorScreenRow = -1;
            int cursorScreenCol = -1;
        };

        RenderOutput render() const
        {
            RenderOutput out;
            out.cells.resize(rect_.h);
            for (auto &row : out.cells)
                row.resize(rect_.w);

            int gutterW = gutterWidth();
            int codeW = rect_.w - gutterW;
            int visibleLines = codeAreaHeight();

            for (int screenRow = 0; screenRow < visibleLines; screenRow++)
            {
                int bufferRow = scrollTopLine_ + screenRow;
                bool isCurrentLine = (bufferRow == cursor_.row);

                // ── Gutter (line numbers) ────────────────────────────────
                renderGutter(out.cells[screenRow], gutterW, bufferRow, isCurrentLine);

                // ── Code area ────────────────────────────────────────────
                if (bufferRow < buffer_.lineCount())
                {
                    const auto &line = buffer_.getLine(bufferRow);
                    auto spans = highlighter_.highlightLine(line);

                    // Write code cells with syntax highlighting
                    for (int screenCol = 0; screenCol < codeW; screenCol++)
                    {
                        int bufCol = scrollLeftCol_ + screenCol;
                        auto &cell = out.cells[screenRow][gutterW + screenCol];

                        if (bufCol < (int)line.size())
                        {
                            cell.ch = (char32_t)line[bufCol];

                            // Find which span this column belongs to
                            for (auto &span : spans)
                            {
                                if (bufCol >= span.startCol && bufCol < span.endCol)
                                {
                                    cell.fg = span.fg;
                                    cell.bold = span.bold;
                                    cell.italic = span.italic;
                                    break;
                                }
                            }
                        }
                        else
                        {
                            cell.ch = U' ';
                            cell.fg = fgColor_;
                        }

                        // Background: selection, current line, or default
                        if (selection_.active && selection_.contains({bufferRow, bufCol}))
                        {
                            cell.bg = selectionBg_;
                        }
                        else if (isCurrentLine)
                        {
                            cell.bg = lineHighlightBg_;
                        }
                        else
                        {
                            cell.bg = bgColor_;
                        }

                        cell.dirty = true;
                    }
                }
                else
                {
                    // Past end of file — render tilde in gutter, empty code area
                    for (int screenCol = 0; screenCol < codeW; screenCol++)
                    {
                        auto &cell = out.cells[screenRow][gutterW + screenCol];
                        cell.ch = U' ';
                        cell.fg = fgColor_;
                        cell.bg = bgColor_;
                        cell.dirty = true;
                    }
                }
            }

            // ── Cursor screen position ───────────────────────────────────
            int cursorScreenRow = cursor_.row - scrollTopLine_;
            int cursorScreenCol = cursor_.col - scrollLeftCol_ + gutterW;
            if (cursorScreenRow >= 0 && cursorScreenRow < visibleLines &&
                cursorScreenCol >= gutterW && cursorScreenCol < rect_.w)
            {
                out.cursorScreenRow = cursorScreenRow;
                out.cursorScreenCol = cursorScreenCol;
            }

            return out;
        }

        // ── Layout queries ──────────────────────────────────────────────

        int gutterWidth() const
        {
            if (!showLineNumbers_)
                return 0;
            int digits = 1;
            int lines = buffer_.lineCount();
            while (lines >= 10)
            {
                lines /= 10;
                digits++;
            }
            return std::max(digits + 2, 4); // min 4 columns (space + digits + space + separator)
        }

        int codeAreaWidth() const { return rect_.w - gutterWidth(); }
        int codeAreaHeight() const { return rect_.h; }

        // ── Settings ────────────────────────────────────────────────────

        void setShowLineNumbers(bool show) { showLineNumbers_ = show; }
        bool showLineNumbers() const { return showLineNumbers_; }

        // ── Mouse → buffer position mapping ─────────────────────────────

        BufferPos screenToBuffer(int screenRow, int screenCol) const
        {
            int gutterW = gutterWidth();
            int bufRow = scrollTopLine_ + screenRow;
            int bufCol = scrollLeftCol_ + (screenCol - gutterW);

            bufRow = std::clamp(bufRow, 0, buffer_.lineCount() - 1);
            bufCol = std::clamp(bufCol, 0, buffer_.lineLength(bufRow));

            return {bufRow, bufCol};
        }

    private:
        TextBuffer &buffer_;
        const ThemeData &theme_;
        Highlighter highlighter_;

        Rect rect_ = {0, 0, 80, 24};
        BufferPos cursor_ = {0, 0};
        Selection selection_;

        int scrollTopLine_ = 0;
        int scrollLeftCol_ = 0;
        bool showLineNumbers_ = true;

        // Theme colors
        Color bgColor_ = Color::default_bg();
        Color fgColor_ = Color::default_fg();
        Color gutterBg_ = {26, 26, 26};
        Color gutterFg_ = {85, 85, 85};
        Color gutterActiveFg_ = {204, 204, 204};
        Color lineHighlightBg_ = {30, 30, 46};
        Color selectionBg_ = {38, 79, 120};
        Color cursorColor_ = Color::white();

        void loadThemeColors()
        {
            bgColor_ = getUIColor(theme_, "editor_bg", bgColor_);
            fgColor_ = getUIColor(theme_, "editor_fg", fgColor_);
            gutterBg_ = getUIColor(theme_, "gutter_bg", gutterBg_);
            gutterFg_ = getUIColor(theme_, "gutter_fg", gutterFg_);
            gutterActiveFg_ = getUIColor(theme_, "gutter_active_fg", gutterActiveFg_);
            lineHighlightBg_ = getUIColor(theme_, "line_highlight", lineHighlightBg_);
            selectionBg_ = getUIColor(theme_, "selection_bg", selectionBg_);
            cursorColor_ = getUIColor(theme_, "cursor", cursorColor_);
        }

        void recalcLayout()
        {
            ensureCursorVisible();
        }

        void startSelection()
        {
            selection_.active = true;
            selection_.anchor = cursor_;
            selection_.cursor = cursor_;
        }

        void clampCursor(BufferPos &pos) const
        {
            pos.row = std::clamp(pos.row, 0, buffer_.lineCount() - 1);
            pos.col = std::clamp(pos.col, 0, buffer_.lineLength(pos.row));
        }

        void ensureCursorVisible()
        {
            // Vertical scroll
            if (cursor_.row < scrollTopLine_)
                scrollTopLine_ = cursor_.row;
            if (cursor_.row >= scrollTopLine_ + codeAreaHeight())
                scrollTopLine_ = cursor_.row - codeAreaHeight() + 1;

            // Horizontal scroll
            int codeW = codeAreaWidth();
            if (cursor_.col < scrollLeftCol_)
                scrollLeftCol_ = cursor_.col;
            if (cursor_.col >= scrollLeftCol_ + codeW)
                scrollLeftCol_ = cursor_.col - codeW + 1;

            clampScroll();
        }

        void clampScroll()
        {
            scrollTopLine_ = std::max(0, scrollTopLine_);
            int maxTop = std::max(0, buffer_.lineCount() - codeAreaHeight());
            scrollTopLine_ = std::min(scrollTopLine_, maxTop);
            scrollLeftCol_ = std::max(0, scrollLeftCol_);
        }

        void renderGutter(std::vector<Cell> &row, int gutterW, int bufferRow, bool isCurrentLine) const
        {
            // Line number with right-alignment
            std::string numStr;
            if (bufferRow < buffer_.lineCount())
            {
                numStr = std::to_string(bufferRow + 1); // 1-based
            }

            int numStart = gutterW - 2 - (int)numStr.size(); // right-aligned before separator

            for (int col = 0; col < gutterW; col++)
            {
                auto &cell = row[col];
                cell.bg = gutterBg_;
                cell.dirty = true;

                if (col == gutterW - 1)
                {
                    // Separator: thin vertical bar
                    cell.ch = U'│';
                    cell.fg = {51, 51, 51}; // dim border
                }
                else if (col >= numStart && col < numStart + (int)numStr.size())
                {
                    cell.ch = (char32_t)numStr[col - numStart];
                    cell.fg = isCurrentLine ? gutterActiveFg_ : gutterFg_;
                    cell.bold = isCurrentLine;
                }
                else
                {
                    cell.ch = U' ';
                    cell.fg = gutterFg_;
                }
            }
        }
    };

} // namespace xterm
