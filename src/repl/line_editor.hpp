#pragma once

// =============================================================================
// LineEditor — single-line editor with cursor movement, word ops, etc.
// =============================================================================
// Manages a string buffer and cursor position. The REPL drives this by
// feeding KeyEvents from Terminal and calling refresh() to redraw.
// =============================================================================

#include "terminal.hpp"
#include "history.hpp"
#include "completer.hpp"
#include <string>
#include <vector>

namespace xell
{

    class LineEditor
    {
    public:
        LineEditor(Terminal &term, History &hist, Completer &comp)
            : term_(term), history_(hist), completer_(comp) {}

        /// Read a full line from the user. Returns false on EOF (Ctrl+D).
        bool readLine(const std::string &prompt, std::string &result)
        {
            buf_.clear();
            cursor_ = 0;
            prompt_ = prompt;
            savedInput_.clear();

            refresh();

            while (true)
            {
                KeyEvent ev = term_.readKey();

                switch (ev.key)
                {
                case Key::ENTER:
                    Terminal::write("\r\n");
                    result = buf_;
                    history_.resetCursor();
                    return true;

                case Key::CTRL_D:
                    if (buf_.empty())
                    {
                        Terminal::write("\r\n");
                        return false; // EOF
                    }
                    // Delete char at cursor (like Delete key)
                    deleteAtCursor();
                    break;

                case Key::CTRL_C:
                    // Cancel current line
                    buf_.clear();
                    cursor_ = 0;
                    Terminal::write("^C\r\n");
                    result.clear();
                    refresh();
                    break;

                case Key::BACKSPACE:
                    if (cursor_ > 0)
                    {
                        buf_.erase(cursor_ - 1, 1);
                        cursor_--;
                    }
                    break;

                case Key::DELETE_KEY:
                    deleteAtCursor();
                    break;

                case Key::LEFT:
                    if (cursor_ > 0)
                        cursor_--;
                    break;

                case Key::RIGHT:
                    if (cursor_ < buf_.size())
                        cursor_++;
                    break;

                case Key::HOME:
                case Key::CTRL_A:
                    cursor_ = 0;
                    break;

                case Key::END:
                case Key::CTRL_E:
                    cursor_ = buf_.size();
                    break;

                case Key::UP:
                {
                    if (savedInput_.empty() && history_.entries().size() > 0)
                        savedInput_ = buf_;
                    std::string h;
                    if (history_.up(h))
                    {
                        buf_ = h;
                        cursor_ = buf_.size();
                    }
                    break;
                }

                case Key::DOWN:
                {
                    std::string h;
                    if (history_.down(h))
                    {
                        buf_ = h.empty() ? savedInput_ : h;
                        cursor_ = buf_.size();
                    }
                    break;
                }

                case Key::CTRL_U:
                    // Delete from cursor to start
                    buf_.erase(0, cursor_);
                    cursor_ = 0;
                    break;

                case Key::CTRL_K:
                    // Delete from cursor to end
                    buf_.erase(cursor_);
                    break;

                case Key::CTRL_W:
                    // Delete word backward
                    deleteWordBackward();
                    break;

                case Key::CTRL_L:
                    // Clear screen, redraw
                    Terminal::clearScreen();
                    break;

                case Key::TAB:
                    handleTab();
                    break;

                case Key::CHAR:
                    buf_.insert(cursor_, 1, ev.ch);
                    cursor_++;
                    break;

                default:
                    break;
                }

                refresh();
            }
        }

        /// Get current buffer content (for multi-line detection)
        const std::string &buffer() const { return buf_; }

    private:
        Terminal &term_;
        History &history_;
        Completer &completer_;

        std::string buf_;
        size_t cursor_ = 0;
        std::string prompt_;
        std::string savedInput_;

        void refresh()
        {
            Terminal::clearLine();
            Terminal::write(prompt_ + buf_);
            // Move cursor back to the right position
            int backSteps = (int)buf_.size() - (int)cursor_;
            if (backSteps > 0)
                Terminal::cursorBackward(backSteps);
        }

        void deleteAtCursor()
        {
            if (cursor_ < buf_.size())
                buf_.erase(cursor_, 1);
        }

        void deleteWordBackward()
        {
            if (cursor_ == 0)
                return;
            size_t end = cursor_;
            // Skip trailing spaces
            while (cursor_ > 0 && buf_[cursor_ - 1] == ' ')
                cursor_--;
            // Skip word chars
            while (cursor_ > 0 && buf_[cursor_ - 1] != ' ')
                cursor_--;
            buf_.erase(cursor_, end - cursor_);
        }

        /// Extract the word being typed at cursor position
        std::string wordAtCursor() const
        {
            if (cursor_ == 0)
                return "";
            size_t start = cursor_;
            while (start > 0 && isIdentChar(buf_[start - 1]))
                start--;
            return buf_.substr(start, cursor_ - start);
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
                // Insert spaces for indent
                buf_.insert(cursor_, "    ");
                cursor_ += 4;
                return;
            }

            auto matches = completer_.complete(prefix);
            if (matches.empty())
                return;

            if (matches.size() == 1)
            {
                // Complete fully
                std::string suffix = matches[0].substr(prefix.size());
                buf_.insert(cursor_, suffix);
                cursor_ += suffix.size();
            }
            else
            {
                // Complete common prefix
                std::string common = Completer::commonPrefix(matches);
                if (common.size() > prefix.size())
                {
                    std::string suffix = common.substr(prefix.size());
                    buf_.insert(cursor_, suffix);
                    cursor_ += suffix.size();
                }
                else
                {
                    // Show all matches
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
