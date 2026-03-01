#pragma once

// =============================================================================
// LineEditor — multiline editor with cursor movement, word ops, etc.
// =============================================================================
// Manages a multiline string buffer with cursor navigation.
// Enter = newline (add line), Shift+Enter or Alt+Enter = submit.
// Arrow keys navigate within and between lines.
// =============================================================================

#include "terminal.hpp"
#include "history.hpp"
#include "completer.hpp"
#include <string>
#include <vector>
#include <algorithm>

namespace xell
{

    class LineEditor
    {
    public:
        LineEditor(Terminal &term, History &hist, Completer &comp)
            : term_(term), history_(hist), completer_(comp) {}

        /// Enable terminal mode: Enter=execute, Shift+Enter=newline.
        /// In default mode (false): Enter=newline, Shift+Enter=execute.
        void setTerminalMode(bool enabled) { terminalMode_ = enabled; }

        /// Read multiline input from the user.
        /// Enter = newline, Shift+Enter or Alt+Enter = submit.
        /// Returns false on EOF (Ctrl+D on empty buffer).
        bool readLine(const std::string &prompt, const std::string &contPrompt, std::string &result)
        {
            lines_.clear();
            lines_.push_back("");
            row_ = 0;
            col_ = 0;
            prompt_ = prompt;
            contPrompt_ = contPrompt;
            savedInput_.clear();
            lastDisplayLines_ = 0;
            lastCursorRow_ = 0;

            refresh();

            while (true)
            {
                KeyEvent ev = term_.readKey();

                switch (ev.key)
                {
                case Key::SHIFT_ENTER:
                case Key::ALT_ENTER:
                    if (terminalMode_)
                    {
                        // Terminal mode: Shift+Enter = insert newline
                        insertNewline();
                    }
                    else
                    {
                        // Default mode: Shift+Enter = submit
                        Terminal::write("\r\n");
                        result = joinLines();
                        history_.resetCursor();
                        return true;
                    }
                    break;

                case Key::ENTER:
                {
                    if (terminalMode_)
                    {
                        // Terminal mode: Enter = execute (like a normal shell).
                        // Exception: if the code has unclosed blocks (: without ;),
                        // treat Enter as newline so user can continue typing.
                        std::string content = joinLines();
                        bool hasContent = !content.empty() &&
                                          content.find_first_not_of(" \t\n") != std::string::npos;

                        if (!hasContent)
                        {
                            // Empty input — just show a new prompt
                            Terminal::write("\r\n");
                            result.clear();
                            history_.resetCursor();
                            return true;
                        }

                        // Check for unclosed blocks
                        // Simple heuristic: count unmatched : vs ;
                        int depth = 0;
                        bool inStr = false;
                        for (size_t ci = 0; ci < content.size(); ci++)
                        {
                            char cc = content[ci];
                            if (cc == '"')
                                inStr = !inStr;
                            if (inStr)
                                continue;
                            if (cc == '#')
                            {
                                while (ci < content.size() && content[ci] != '\n')
                                    ci++;
                                continue;
                            }
                            if (cc == ':')
                                depth++;
                            if (cc == ';')
                                depth--;
                        }

                        if (depth > 0)
                        {
                            // Unclosed block — add newline for continuation
                            insertNewline();
                        }
                        else
                        {
                            // Complete expression — execute!
                            Terminal::write("\r\n");
                            // Remove trailing empty lines
                            while (!lines_.empty() && lines_.back().empty())
                                lines_.pop_back();
                            result = joinLines();
                            history_.resetCursor();
                            return true;
                        }
                    }
                    else
                    {
                        // Default REPL mode: Enter = newline.
                        // Enter on an empty last line = submit (fallback for
                        // terminals that can't distinguish Shift+Enter)
                        std::string content = joinLines();
                        bool isLastLine = (row_ == lines_.size() - 1);
                        bool lineIsEmpty = lines_[row_].empty();
                        bool hasContent = !content.empty() &&
                                          content.find_first_not_of(" \t\n") != std::string::npos;
                        if (isLastLine && lineIsEmpty && hasContent)
                        {
                            Terminal::write("\r\n");
                            // Remove trailing empty line
                            while (!lines_.empty() && lines_.back().empty())
                                lines_.pop_back();
                            result = joinLines();
                            history_.resetCursor();
                            return true;
                        }
                        // Otherwise: newline — add a new line
                        insertNewline();
                    }
                    break;
                }

                case Key::CTRL_D:
                    if (joinLines().empty())
                    {
                        Terminal::write("\r\n");
                        return false; // EOF
                    }
                    deleteAtCursor();
                    break;

                case Key::CTRL_C:
                    // Cancel current input
                    lines_.clear();
                    lines_.push_back("");
                    row_ = 0;
                    col_ = 0;
                    Terminal::write("^C\r\n");
                    result.clear();
                    refresh();
                    break;

                case Key::BACKSPACE:
                    if (col_ > 0)
                    {
                        lines_[row_].erase(col_ - 1, 1);
                        col_--;
                    }
                    else if (row_ > 0)
                    {
                        // Merge with previous line
                        col_ = lines_[row_ - 1].size();
                        lines_[row_ - 1] += lines_[row_];
                        lines_.erase(lines_.begin() + row_);
                        row_--;
                    }
                    break;

                case Key::DELETE_KEY:
                    deleteAtCursor();
                    break;

                case Key::LEFT:
                    if (col_ > 0)
                    {
                        col_--;
                    }
                    else if (row_ > 0)
                    {
                        row_--;
                        col_ = lines_[row_].size();
                    }
                    break;

                case Key::RIGHT:
                    if (col_ < lines_[row_].size())
                    {
                        col_++;
                    }
                    else if (row_ < lines_.size() - 1)
                    {
                        row_++;
                        col_ = 0;
                    }
                    break;

                case Key::UP:
                    if (row_ > 0)
                    {
                        // Move up within buffer
                        row_--;
                        col_ = std::min(col_, lines_[row_].size());
                    }
                    else
                    {
                        // At top line — navigate history
                        if (savedInput_.empty() && !history_.entries().empty())
                            savedInput_ = joinLines();
                        std::string h;
                        if (history_.up(h))
                        {
                            setContent(h);
                        }
                    }
                    break;

                case Key::DOWN:
                    if (row_ < lines_.size() - 1)
                    {
                        // Move down within buffer
                        row_++;
                        col_ = std::min(col_, lines_[row_].size());
                    }
                    else
                    {
                        // At bottom line — navigate history
                        std::string h;
                        if (history_.down(h))
                        {
                            setContent(h.empty() ? savedInput_ : h);
                        }
                    }
                    break;

                case Key::HOME:
                case Key::CTRL_A:
                    col_ = 0;
                    break;

                case Key::END:
                case Key::CTRL_E:
                    col_ = lines_[row_].size();
                    break;

                case Key::CTRL_U:
                    // Delete from cursor to start of current line
                    lines_[row_].erase(0, col_);
                    col_ = 0;
                    break;

                case Key::CTRL_K:
                    // Delete from cursor to end of current line
                    lines_[row_].erase(col_);
                    break;

                case Key::CTRL_W:
                    deleteWordBackward();
                    break;

                case Key::CTRL_L:
                    Terminal::clearScreen();
                    break;

                case Key::TAB:
                    handleTab();
                    break;

                case Key::CHAR:
                    lines_[row_].insert(col_, 1, ev.ch);
                    col_++;
                    break;

                default:
                    break;
                }

                refresh();
            }
        }

        /// Overload for backward compatibility — single prompt, Enter=submit
        bool readLine(const std::string &prompt, std::string &result)
        {
            return readLine(prompt, prompt, result);
        }

        /// Get current buffer content
        std::string buffer() const { return joinLines(); }

    private:
        Terminal &term_;
        History &history_;
        Completer &completer_;
        bool terminalMode_ = false;

        std::vector<std::string> lines_;
        size_t row_ = 0;
        size_t col_ = 0;
        std::string prompt_;
        std::string contPrompt_;
        std::string savedInput_;

        std::string joinLines() const
        {
            std::string result;
            for (size_t i = 0; i < lines_.size(); i++)
            {
                if (i > 0)
                    result += '\n';
                result += lines_[i];
            }
            return result;
        }

        void setContent(const std::string &text)
        {
            lines_.clear();
            if (text.empty())
            {
                lines_.push_back("");
            }
            else
            {
                size_t start = 0;
                while (start <= text.size())
                {
                    size_t nl = text.find('\n', start);
                    if (nl == std::string::npos)
                    {
                        lines_.push_back(text.substr(start));
                        break;
                    }
                    lines_.push_back(text.substr(start, nl - start));
                    start = nl + 1;
                }
            }
            row_ = lines_.size() - 1;
            col_ = lines_[row_].size();
        }

        void insertNewline()
        {
            // Split current line at cursor
            std::string after = lines_[row_].substr(col_);
            lines_[row_].erase(col_);

            // Auto-indent: copy leading whitespace from current line
            std::string indent;
            for (char c : lines_[row_])
            {
                if (c == ' ' || c == '\t')
                    indent += c;
                else
                    break;
            }

            // If current line ends with ':', add extra indent
            std::string trimmed = lines_[row_];
            // Trim trailing spaces
            while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\t'))
                trimmed.pop_back();
            if (!trimmed.empty() && trimmed.back() == ':')
                indent += "    ";

            lines_.insert(lines_.begin() + row_ + 1, indent + after);
            row_++;
            col_ = indent.size();
        }

        void deleteAtCursor()
        {
            if (col_ < lines_[row_].size())
            {
                lines_[row_].erase(col_, 1);
            }
            else if (row_ < lines_.size() - 1)
            {
                // Merge next line into current
                lines_[row_] += lines_[row_ + 1];
                lines_.erase(lines_.begin() + row_ + 1);
            }
        }

        void refresh()
        {
            // Move cursor to start of our edit area (row 0 of the buffer)
            // We need to figure out how many display rows we've already drawn
            // and move up to row 0.

            // First: move up from current display position to first line
            // The cursor should currently be on display row = row_
            // (it was left there after last refresh)
            if (lastDisplayLines_ > 0)
            {
                // Move up to first line of our edit area
                int upToTop = lastCursorRow_;
                if (upToTop > 0)
                    Terminal::write("\033[" + std::to_string(upToTop) + "A");
                Terminal::write("\r");
                // Clear everything from here to end of screen
                Terminal::write("\033[J");
            }

            // Draw all lines
            for (size_t i = 0; i < lines_.size(); i++)
            {
                const std::string &p = (i == 0) ? prompt_ : contPrompt_;
                Terminal::write(p + lines_[i]);
                if (i < lines_.size() - 1)
                    Terminal::write("\r\n");
            }

            // Remember state for next refresh
            lastDisplayLines_ = lines_.size();
            lastCursorRow_ = row_;

            // Position cursor at (row_, col_)
            // Currently at end of last line — move up to row_ if needed
            int upMoves = (int)(lines_.size() - 1 - row_);
            if (upMoves > 0)
                Terminal::write("\033[" + std::to_string(upMoves) + "A");

            // Move to correct column
            const std::string &activePrompt = (row_ == 0) ? prompt_ : contPrompt_;
            int promptLen = visibleLength(activePrompt);
            Terminal::write("\r");
            int targetCol = promptLen + (int)col_;
            if (targetCol > 0)
                Terminal::write("\033[" + std::to_string(targetCol) + "C");
        }

        size_t lastDisplayLines_ = 0;
        size_t lastCursorRow_ = 0;

        /// Calculate visible length of a string (ignoring ANSI escape codes
        /// and counting multi-byte UTF-8 characters as single columns)
        static int visibleLength(const std::string &s)
        {
            int len = 0;
            bool inEscape = false;
            for (size_t i = 0; i < s.size(); i++)
            {
                unsigned char c = (unsigned char)s[i];
                if (inEscape)
                {
                    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
                        inEscape = false;
                    continue;
                }
                if (c == '\033')
                {
                    inEscape = true;
                    continue;
                }
                // UTF-8: continuation bytes (10xxxxxx) don't count as visible
                if ((c & 0xC0) == 0x80)
                    continue;
                len++;
            }
            return len;
        }

        void deleteWordBackward()
        {
            if (col_ == 0)
                return;
            size_t end = col_;
            while (col_ > 0 && lines_[row_][col_ - 1] == ' ')
                col_--;
            while (col_ > 0 && lines_[row_][col_ - 1] != ' ')
                col_--;
            lines_[row_].erase(col_, end - col_);
        }

        std::string wordAtCursor() const
        {
            if (col_ == 0 || row_ >= lines_.size())
                return "";
            const std::string &line = lines_[row_];
            size_t start = col_;
            while (start > 0 && isIdentChar(line[start - 1]))
                start--;
            return line.substr(start, col_ - start);
        }

        static bool isIdentChar(char c)
        {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') || c == '_';
        }

        void handleTab()
        {
            std::string prefix = wordAtCursor();
            if (prefix.empty())
            {
                lines_[row_].insert(col_, "    ");
                col_ += 4;
                return;
            }

            auto matches = completer_.complete(prefix);
            if (matches.empty())
                return;

            if (matches.size() == 1)
            {
                std::string suffix = matches[0].substr(prefix.size());
                lines_[row_].insert(col_, suffix);
                col_ += suffix.size();
            }
            else
            {
                std::string common = Completer::commonPrefix(matches);
                if (common.size() > prefix.size())
                {
                    std::string suffix = common.substr(prefix.size());
                    lines_[row_].insert(col_, suffix);
                    col_ += suffix.size();
                }
                else
                {
                    Terminal::write("\r\n");
                    for (size_t i = 0; i < matches.size(); i++)
                    {
                        Terminal::write(matches[i]);
                        if (i < matches.size() - 1)
                            Terminal::write("  ");
                    }
                    Terminal::write("\r\n");
                }
            }
        }
    };

} // namespace xell
