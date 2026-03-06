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
#include <unordered_map>
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

            // ── Pre-compute block comment state for lines before viewport ──
            highlighter_.resetMultilineState();
            for (int i = 0; i < scrollTopLine_ && i < buffer_.lineCount(); ++i)
            {
                const auto &preLine = buffer_.getLine(i);
                // Quick scan for --> and <-- to track block comment state
                // without full tokenization overhead
                bool inBC = highlighter_.inBlockComment();
                size_t pos = 0;
                while (pos < preLine.size())
                {
                    if (inBC)
                    {
                        auto endPos = preLine.find("<--", pos);
                        if (endPos != std::string::npos)
                        {
                            inBC = false;
                            pos = endPos + 3;
                        }
                        else
                        {
                            break; // rest of line is comment
                        }
                    }
                    else
                    {
                        auto startPos = preLine.find("-->", pos);
                        if (startPos != std::string::npos)
                        {
                            inBC = true;
                            pos = startPos + 3;
                        }
                        else
                        {
                            break;
                        }
                    }
                }
                highlighter_.setBlockComment(inBC);
            }

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

                    // Check if this line has a diagnostic
                    auto diagIt = diagnosticLines_.find(bufferRow);
                    bool hasDiag = (diagIt != diagnosticLines_.end());
                    int diagSeverity = hasDiag ? diagIt->second : -1;
                    Color diagColor = (diagSeverity == 0)   ? Color{255, 80, 80}   // error: red
                                      : (diagSeverity == 1) ? Color{229, 192, 123} // warning: yellow
                                                            : Color{255, 80, 80};

                    // Write code cells with syntax highlighting
                    for (int screenCol = 0; screenCol < codeW; screenCol++)
                    {
                        int bufCol = scrollLeftCol_ + screenCol;
                        auto &cell = out.cells[screenRow][gutterW + screenCol];

                        if (bufCol < (int)line.size())
                        {
                            // For source code, treat as byte-indexed (ASCII dominant).
                            // Safely cast through unsigned to avoid sign-extension.
                            cell.ch = static_cast<char32_t>(static_cast<unsigned char>(line[bufCol]));

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

                        // Inline diagnostic underline (only on actual text, not trailing space)
                        if (hasDiag && bufCol < (int)line.size())
                        {
                            cell.underline = true;
                            cell.fg = diagColor;
                        }

                        // Active snippet tab stop highlight
                        if (tabStopLine_ >= 0 && bufferRow == tabStopLine_ &&
                            bufCol >= tabStopCol_ && bufCol < tabStopCol_ + tabStopLen_)
                        {
                            cell.bg = {60, 60, 90}; // subtle blue highlight for tab stop
                        }

                        cell.dirty = true;
                    }

                    // ── Ghost text (dim inline suggestion after cursor) ──
                    if (bufferRow == ghostLine_ && !ghostText_.empty())
                    {
                        int ghostStartCol = ghostCol_ - scrollLeftCol_;
                        Color ghostFg = {100, 100, 100}; // dim grey
                        for (int gi = 0; gi < (int)ghostText_.size(); gi++)
                        {
                            int screenCol2 = gutterW + ghostStartCol + gi;
                            if (screenCol2 >= gutterW && screenCol2 < rect_.w)
                            {
                                auto &gc = out.cells[screenRow][screenCol2];
                                gc.ch = static_cast<char32_t>(static_cast<unsigned char>(ghostText_[gi]));
                                gc.fg = ghostFg;
                                gc.italic = true;
                                gc.dirty = true;
                            }
                        }
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

            // ── Scrollbar (rightmost column) ──────────────────────────────
            if (showScrollbar_ && visibleLines > 0 && rect_.w > 0)
            {
                int scrollCol = rect_.w - 1;
                int totalLines = buffer_.lineCount();
                if (totalLines > visibleLines)
                {
                    // Calculate thumb position and size
                    int trackHeight = visibleLines;
                    int thumbHeight = std::max(1, trackHeight * visibleLines / totalLines);
                    int thumbStart = (totalLines - visibleLines > 0)
                                         ? scrollTopLine_ * (trackHeight - thumbHeight) / (totalLines - visibleLines)
                                         : 0;

                    Color trackColor = {35, 35, 35};
                    Color thumbColor = {80, 80, 80};

                    for (int r = 0; r < visibleLines; r++)
                    {
                        auto &cell = out.cells[r][scrollCol];
                        bool isThumb = (r >= thumbStart && r < thumbStart + thumbHeight);
                        cell.ch = isThumb ? U'█' : U'░';
                        cell.fg = isThumb ? thumbColor : trackColor;
                        cell.bg = bgColor_;
                        cell.dirty = true;
                    }
                }
            }

            // ── Horizontal scrollbar (bottom row of code area) ──────────
            if (showScrollbar_ && visibleLines > 1 && rect_.w > 0)
            {
                int maxWidth = maxLineWidth();
                int codeW2 = codeAreaWidth();
                if (maxWidth > codeW2)
                {
                    int hBarRow = visibleLines - 1; // last row of code area
                    int trackWidth = codeW2 - 1;    // leave rightmost col for vscrollbar
                    if (trackWidth > 2)
                    {
                        int thumbWidth = std::max(2, trackWidth * codeW2 / maxWidth);
                        int maxScrollLeft = maxWidth - codeW2;
                        int thumbStart = (maxScrollLeft > 0)
                                             ? scrollLeftCol_ * (trackWidth - thumbWidth) / maxScrollLeft
                                             : 0;

                        Color trackColor = {35, 35, 35};
                        Color thumbColor = {100, 100, 100};

                        for (int c = 0; c < trackWidth; c++)
                        {
                            auto &cell = out.cells[hBarRow][gutterW + c];
                            bool isThumb = (c >= thumbStart && c < thumbStart + thumbWidth);
                            cell.ch = isThumb ? U'\u2501' : U'\u2500'; // ━ (thumb) / ─ (track)
                            cell.fg = isThumb ? thumbColor : trackColor;
                            cell.bg = bgColor_;
                            cell.dirty = true;
                        }
                    }
                }
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

        // Max line width in the visible window (for horizontal scrollbar sizing)
        int maxLineWidth() const
        {
            int maxW = 0;
            int lineCount = buffer_.lineCount();
            for (int i = 0; i < lineCount; i++)
            {
                int len = buffer_.lineLength(i);
                if (len > maxW)
                    maxW = len;
            }
            return maxW;
        }

        int scrollLeftCol() const { return scrollLeftCol_; }

        void scrollHorizontalTo(int col)
        {
            int maxW = maxLineWidth();
            int codeW = codeAreaWidth();
            int maxScroll = std::max(0, maxW - codeW + 1);
            scrollLeftCol_ = std::clamp(col, 0, maxScroll);
            clampScroll();
        }

        // ── Settings ────────────────────────────────────────────────────

        void setShowLineNumbers(bool show) { showLineNumbers_ = show; }
        bool showLineNumbers() const { return showLineNumbers_; }

        // ── Inline diagnostics ──────────────────────────────────────────

        // Set diagnostic markers: map of line (0-based) → severity (0=error, 1=warning)
        void setDiagnostics(const std::unordered_map<int, int> &diags)
        {
            diagnosticLines_ = diags;
        }

        void clearDiagnostics() { diagnosticLines_.clear(); }

        // ── Ghost text (autocomplete inline suggestion) ─────────────

        void setGhostText(const std::string &text, int line, int col)
        {
            ghostText_ = text;
            ghostLine_ = line;
            ghostCol_ = col;
        }

        void clearGhostText()
        {
            ghostText_.clear();
            ghostLine_ = -1;
            ghostCol_ = -1;
        }

        bool hasGhostText() const { return !ghostText_.empty() && ghostLine_ >= 0; }
        const std::string &ghostText() const { return ghostText_; }
        int ghostLine() const { return ghostLine_; }
        int ghostCol() const { return ghostCol_; }

        // ── Active snippet tab stop highlighting ────────────────────

        void setActiveTabStop(int line, int col, int length)
        {
            tabStopLine_ = line;
            tabStopCol_ = col;
            tabStopLen_ = length;
        }

        void clearActiveTabStop()
        {
            tabStopLine_ = -1;
            tabStopCol_ = -1;
            tabStopLen_ = 0;
        }

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
        mutable Highlighter highlighter_;

        Rect rect_ = {0, 0, 80, 24};
        BufferPos cursor_ = {0, 0};
        Selection selection_;

        int scrollTopLine_ = 0;
        int scrollLeftCol_ = 0;
        bool showLineNumbers_ = true;
        bool showScrollbar_ = true;

        // Inline diagnostics: line → severity (0=error, 1=warning)
        std::unordered_map<int, int> diagnosticLines_;

        // Ghost text (autocomplete suggestion)
        std::string ghostText_;
        int ghostLine_ = -1;
        int ghostCol_ = -1;

        // Active snippet tab stop highlight
        int tabStopLine_ = -1;
        int tabStopCol_ = -1;
        int tabStopLen_ = 0;

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

            // Check for diagnostic on this line
            auto diagIt = diagnosticLines_.find(bufferRow);
            bool hasDiag = (diagIt != diagnosticLines_.end());
            Color diagMarkerColor = (hasDiag && diagIt->second == 0)   ? Color{255, 80, 80}   // error red
                                    : (hasDiag && diagIt->second == 1) ? Color{229, 192, 123} // warning yellow
                                                                       : Color{255, 80, 80};

            int numStart = gutterW - 2 - (int)numStr.size(); // right-aligned before separator

            for (int col = 0; col < gutterW; col++)
            {
                auto &cell = row[col];
                cell.bg = gutterBg_;
                cell.dirty = true;

                if (col == 0 && hasDiag)
                {
                    // Diagnostic marker in first gutter column
                    cell.ch = U'●';
                    cell.fg = diagMarkerColor;
                }
                else if (col == gutterW - 1)
                {
                    // Separator: thin vertical bar
                    cell.ch = U'│';
                    cell.fg = {51, 51, 51}; // dim border
                }
                else if (col >= numStart && col < numStart + (int)numStr.size())
                {
                    // numStr is ASCII digits, safe single-byte decode
                    cell.ch = static_cast<char32_t>(static_cast<unsigned char>(numStr[col - numStart]));
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
