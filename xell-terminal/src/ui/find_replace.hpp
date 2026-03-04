#pragma once

// =============================================================================
// find_replace.hpp — Find & Replace bar for the Xell Terminal IDE
// =============================================================================
// Provides:
//   - Incremental search with highlighting
//   - Case-sensitive / case-insensitive toggle
//   - Whole-word toggle
//   - Replace single / replace all
//   - Rendered as a top overlay bar in the editor
// =============================================================================

#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include "../terminal/types.hpp"
#include "../editor/text_buffer.hpp"
#include "../theme/theme_loader.hpp"

namespace xterm
{

    // ─── Match result ────────────────────────────────────────────────────

    struct SearchMatch
    {
        int row;
        int colStart;
        int colEnd;
    };

    // ─── Find & Replace ──────────────────────────────────────────────────

    class FindReplace
    {
    public:
        FindReplace(const ThemeData &theme) : theme_(theme)
        {
            loadColors();
        }

        // ── Visibility ──────────────────────────────────────────────

        void show(bool withReplace = false)
        {
            visible_ = true;
            showReplace_ = withReplace;
            cursorInFind_ = true;
        }

        void hide()
        {
            visible_ = false;
            matches_.clear();
        }

        bool isVisible() const { return visible_; }
        bool isReplaceMode() const { return showReplace_; }

        // ── Search query editing ────────────────────────────────────

        void setFindText(const std::string &text)
        {
            findText_ = text;
            findCursor_ = (int)text.size();
        }

        void setReplaceText(const std::string &text)
        {
            replaceText_ = text;
            replaceCursor_ = (int)text.size();
        }

        const std::string &findText() const { return findText_; }
        const std::string &replaceText() const { return replaceText_; }

        // ── Options ─────────────────────────────────────────────────

        bool caseSensitive() const { return caseSensitive_; }
        void toggleCaseSensitive() { caseSensitive_ = !caseSensitive_; }

        bool wholeWord() const { return wholeWord_; }
        void toggleWholeWord() { wholeWord_ = !wholeWord_; }

        // ── Search execution ────────────────────────────────────────

        void findAll(const TextBuffer &buffer)
        {
            matches_.clear();
            currentMatch_ = -1;

            if (findText_.empty())
                return;

            std::string needle = findText_;
            if (!caseSensitive_)
                for (auto &c : needle)
                    c = std::tolower(c);

            for (int row = 0; row < buffer.lineCount(); row++)
            {
                std::string line = buffer.getLine(row);
                std::string searchLine = line;
                if (!caseSensitive_)
                    for (auto &c : searchLine)
                        c = std::tolower(c);

                size_t pos = 0;
                while ((pos = searchLine.find(needle, pos)) != std::string::npos)
                {
                    int colStart = (int)pos;
                    int colEnd = colStart + (int)needle.size();

                    // Whole word check
                    if (wholeWord_)
                    {
                        bool leftOk = (colStart == 0 || !isWordChar(line[colStart - 1]));
                        bool rightOk = (colEnd >= (int)line.size() || !isWordChar(line[colEnd]));
                        if (!leftOk || !rightOk)
                        {
                            pos++;
                            continue;
                        }
                    }

                    matches_.push_back({row, colStart, colEnd});
                    pos = colEnd;
                }
            }

            if (!matches_.empty())
                currentMatch_ = 0;
        }

        // ── Navigation ──────────────────────────────────────────────

        int matchCount() const { return (int)matches_.size(); }
        int currentMatchIndex() const { return currentMatch_; }

        const SearchMatch *currentMatchResult() const
        {
            if (currentMatch_ >= 0 && currentMatch_ < (int)matches_.size())
                return &matches_[currentMatch_];
            return nullptr;
        }

        void nextMatch()
        {
            if (matches_.empty())
                return;
            currentMatch_ = (currentMatch_ + 1) % (int)matches_.size();
        }

        void prevMatch()
        {
            if (matches_.empty())
                return;
            currentMatch_ = (currentMatch_ - 1 + (int)matches_.size()) % (int)matches_.size();
        }

        // Find closest match to a cursor position
        void findClosestMatch(BufferPos cursor)
        {
            if (matches_.empty())
                return;
            for (int i = 0; i < (int)matches_.size(); i++)
            {
                if (matches_[i].row > cursor.row ||
                    (matches_[i].row == cursor.row && matches_[i].colStart >= cursor.col))
                {
                    currentMatch_ = i;
                    return;
                }
            }
            currentMatch_ = 0; // wrap around
        }

        // ── Replace ─────────────────────────────────────────────────

        // Replace current match. Returns the new cursor position.
        BufferPos replaceCurrent(TextBuffer &buffer)
        {
            if (currentMatch_ < 0 || currentMatch_ >= (int)matches_.size())
                return {0, 0};

            auto &m = matches_[currentMatch_];
            buffer.replaceRange({m.row, m.colStart}, {m.row, m.colEnd}, replaceText_);

            BufferPos newPos = {m.row, m.colStart + (int)replaceText_.size()};

            // Re-search
            findAll(buffer);
            findClosestMatch(newPos);

            return newPos;
        }

        // Replace all matches
        int replaceAll(TextBuffer &buffer)
        {
            if (matches_.empty())
                return 0;

            int count = 0;
            // Replace from bottom to top to preserve positions
            for (int i = (int)matches_.size() - 1; i >= 0; i--)
            {
                auto &m = matches_[i];
                buffer.replaceRange({m.row, m.colStart}, {m.row, m.colEnd}, replaceText_);
                count++;
            }

            findAll(buffer);
            return count;
        }

        // ── Highlight matches in editor ─────────────────────────────

        bool isMatch(int row, int col) const
        {
            for (auto &m : matches_)
            {
                if (m.row == row && col >= m.colStart && col < m.colEnd)
                    return true;
            }
            return false;
        }

        bool isCurrentMatch(int row, int col) const
        {
            if (currentMatch_ < 0 || currentMatch_ >= (int)matches_.size())
                return false;
            auto &m = matches_[currentMatch_];
            return m.row == row && col >= m.colStart && col < m.colEnd;
        }

        Color matchHighlight() const { return matchBg_; }
        Color currentMatchHighlight() const { return currentMatchBg_; }

        // ── Keyboard handling in the find bar ───────────────────────

        // Returns true if the event was consumed
        bool handleKeyDown(const SDL_Event &event)
        {
            if (!visible_)
                return false;

            auto key = event.key.keysym.sym;
            bool ctrl = (event.key.keysym.mod & KMOD_CTRL) != 0;

            if (key == SDLK_ESCAPE)
            {
                hide();
                return true;
            }

            if (key == SDLK_TAB)
            {
                cursorInFind_ = !cursorInFind_;
                return true;
            }

            if (key == SDLK_RETURN || key == SDLK_KP_ENTER)
            {
                if (event.key.keysym.mod & KMOD_SHIFT)
                    prevMatch();
                else
                    nextMatch();
                return true;
            }

            if (ctrl && key == SDLK_r)
            {
                toggleCaseSensitive();
                return true;
            }

            if (ctrl && key == SDLK_w)
            {
                toggleWholeWord();
                return true;
            }

            // Text editing in find/replace fields
            std::string &field = cursorInFind_ ? findText_ : replaceText_;
            int &cursor = cursorInFind_ ? findCursor_ : replaceCursor_;

            if (key == SDLK_BACKSPACE)
            {
                if (cursor > 0 && !field.empty())
                {
                    field.erase(cursor - 1, 1);
                    cursor--;
                }
                return true;
            }

            if (key == SDLK_DELETE)
            {
                if (cursor < (int)field.size())
                    field.erase(cursor, 1);
                return true;
            }

            if (key == SDLK_LEFT)
            {
                if (cursor > 0)
                    cursor--;
                return true;
            }

            if (key == SDLK_RIGHT)
            {
                if (cursor < (int)field.size())
                    cursor++;
                return true;
            }

            if (key == SDLK_HOME)
            {
                cursor = 0;
                return true;
            }

            if (key == SDLK_END)
            {
                cursor = (int)field.size();
                return true;
            }

            return false;
        }

        bool handleTextInput(const SDL_Event &event)
        {
            if (!visible_)
                return false;

            std::string text = event.text.text;
            std::string &field = cursorInFind_ ? findText_ : replaceText_;
            int &cursor = cursorInFind_ ? findCursor_ : replaceCursor_;

            field.insert(cursor, text);
            cursor += (int)text.size();
            return true;
        }

        // ── Render the find bar ─────────────────────────────────────

        struct FindBarRender
        {
            std::vector<std::vector<Cell>> cells;
            int height; // 1 for find-only, 2 for find+replace
        };

        FindBarRender render(int width) const
        {
            FindBarRender out;
            out.height = showReplace_ ? 2 : 1;
            out.cells.resize(out.height);

            for (auto &row : out.cells)
            {
                row.resize(width);
                for (auto &c : row)
                {
                    c.ch = U' ';
                    c.bg = barBg_;
                    c.fg = barFg_;
                    c.dirty = true;
                }
            }

            // Find row
            renderField(out.cells[0], "Find: ", findText_, findCursor_,
                        cursorInFind_, width);

            // Status: "N of M"
            if (!matches_.empty())
            {
                std::string status = std::to_string(currentMatch_ + 1) + " of " +
                                     std::to_string(matches_.size());
                int statusCol = width - (int)status.size() - 2;
                {
                    size_t si = 0;
                    int i = 0;
                    while (si < status.size() && statusCol + i < width)
                    {
                        out.cells[0][statusCol + i].ch = utf8Decode(status, si);
                        out.cells[0][statusCol + i].fg = {128, 128, 128};
                        i++;
                    }
                }
            }

            // Options icons
            int optCol = width - 20;
            if (optCol > 0)
            {
                // Case sensitivity indicator
                Color optFg = caseSensitive_ ? Color::white() : Color{80, 80, 80};
                out.cells[0][optCol].ch = U'A';
                out.cells[0][optCol].fg = optFg;
                out.cells[0][optCol].bold = caseSensitive_;
                out.cells[0][optCol].underline = caseSensitive_;

                // Whole word indicator
                optFg = wholeWord_ ? Color::white() : Color{80, 80, 80};
                out.cells[0][optCol + 2].ch = U'W';
                out.cells[0][optCol + 2].fg = optFg;
                out.cells[0][optCol + 2].bold = wholeWord_;
                out.cells[0][optCol + 2].underline = wholeWord_;
            }

            // Replace row
            if (showReplace_)
            {
                renderField(out.cells[1], "Replace: ", replaceText_, replaceCursor_,
                            !cursorInFind_, width);
            }

            return out;
        }

    private:
        const ThemeData &theme_;

        bool visible_ = false;
        bool showReplace_ = false;
        bool cursorInFind_ = true;

        std::string findText_;
        std::string replaceText_;
        int findCursor_ = 0;
        int replaceCursor_ = 0;

        bool caseSensitive_ = false;
        bool wholeWord_ = false;

        std::vector<SearchMatch> matches_;
        int currentMatch_ = -1;

        Color barBg_ = {51, 51, 51};
        Color barFg_ = {204, 204, 204};
        Color fieldBg_ = {30, 30, 30};
        Color matchBg_ = {90, 75, 30};
        Color currentMatchBg_ = {120, 100, 40};

        void loadColors()
        {
            barBg_ = getUIColor(theme_, "find_bar_bg", barBg_);
            matchBg_ = getUIColor(theme_, "find_match_bg", matchBg_);
            currentMatchBg_ = getUIColor(theme_, "find_current_match_bg", currentMatchBg_);
        }

        void renderField(std::vector<Cell> &row, const std::string &label,
                         const std::string &text, int cursor, bool active, int width) const
        {
            int col = 1;

            // Label
            {
                size_t si = 0;
                while (si < label.size() && col < width)
                {
                    row[col].ch = utf8Decode(label, si);
                    row[col].fg = {128, 128, 128};
                    col++;
                }
            }

            // Field background
            int fieldStart = col;
            int fieldWidth = std::min(width - fieldStart - 25, 50);
            for (int i = 0; i < fieldWidth && fieldStart + i < width; i++)
            {
                row[fieldStart + i].bg = fieldBg_;
            }

            // Field text
            {
                size_t si = 0;
                int i = 0;
                while (si < text.size() && fieldStart + i < fieldStart + fieldWidth)
                {
                    row[fieldStart + i].ch = utf8Decode(text, si);
                    row[fieldStart + i].fg = barFg_;
                    row[fieldStart + i].bg = fieldBg_;
                    i++;
                }
            }

            // Cursor (underline the character)
            if (active && fieldStart + cursor < width)
            {
                row[fieldStart + cursor].underline = true;
                row[fieldStart + cursor].fg = Color::white();
            }
        }

        static bool isWordChar(char c)
        {
            return std::isalnum(c) || c == '_';
        }
    };

} // namespace xterm
