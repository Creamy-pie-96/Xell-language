#pragma once

// =============================================================================
// Terminal — cross-platform raw terminal I/O (no external deps)
// =============================================================================
// Provides raw-mode input (character-at-a-time, no echo), ANSI escape sequence
// reading for arrow keys / special keys, and terminal size queries.
//
// Linux/macOS: uses termios + ANSI escape codes.
// Windows:     uses Win32 Console API (TODO — currently POSIX only).
// =============================================================================

#include <string>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#endif

namespace xell
{

    // ---- Key codes returned by readKey() ------------------------------------
    enum class Key
    {
        // Printable chars come through as their ASCII value via Char(c)
        CHAR, // .ch holds the actual character

        // Navigation
        UP,
        DOWN,
        LEFT,
        RIGHT,
        HOME,
        END,
        DELETE_KEY,

        // Editing
        BACKSPACE,
        TAB,
        ENTER,       // Plain Enter → newline in multiline mode
        SHIFT_ENTER, // Shift+Enter or Alt+Enter → execute
        ALT_ENTER,   // Alias for SHIFT_ENTER

        // Control
        CTRL_C,
        CTRL_D,
        CTRL_L, // clear screen
        CTRL_A, // home
        CTRL_E, // end
        CTRL_W, // delete word backward
        CTRL_U, // delete to start of line
        CTRL_K, // delete to end of line

        // Unknown / unhandled
        UNKNOWN
    };

    struct KeyEvent
    {
        Key key = Key::UNKNOWN;
        char ch = 0; // valid when key == Key::CHAR
    };

    // ========================================================================
    // Terminal class
    // ========================================================================

    class Terminal
    {
    public:
        Terminal() = default;
        ~Terminal() { disableRawMode(); }

        /// Switch stdin to raw mode (no echo, no line buffering)
        bool enableRawMode()
        {
            if (rawEnabled_)
                return true;
#ifdef _WIN32
            // TODO: Win32 console mode
            rawEnabled_ = true;
            return true;
#else
            if (tcgetattr(STDIN_FILENO, &origTermios_) == -1)
                return false;
            struct termios raw = origTermios_;
            // Input: no break, no CR-to-NL, no parity, no strip, no flow ctrl
            raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
            // Output: leave alone (we want \n → \r\n)
            // raw.c_oflag &= ~(OPOST);  // keep output processing
            // Control: 8-bit chars
            raw.c_cflag |= CS8;
            // Local: no echo, no canonical, no signals, no extended
            raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
            // Read returns after 1 byte, 100ms timeout
            raw.c_cc[VMIN] = 1;
            raw.c_cc[VTIME] = 0;
            if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
                return false;
            rawEnabled_ = true;

            // Enable kitty keyboard protocol (progressive enhancement, flags=1)
            // This allows distinguishing Shift+Enter from plain Enter.
            // Terminals that don't support it will silently ignore this.
            write("\033[>1u");

            return true;
#endif
        }

        /// Restore original terminal settings
        void disableRawMode()
        {
            if (!rawEnabled_)
                return;
#ifdef _WIN32
            // TODO
#else
            // Disable kitty keyboard protocol
            write("\033[<u");
            tcsetattr(STDIN_FILENO, TCSAFLUSH, &origTermios_);
#endif
            rawEnabled_ = false;
        }

        /// Read a single key event (blocking)
        KeyEvent readKey()
        {
            KeyEvent ev;
#ifdef _WIN32
            int c = _getch();
            // TODO: full Win32 key handling
            ev.key = Key::CHAR;
            ev.ch = (char)c;
            return ev;
#else
            char c;
            if (::read(STDIN_FILENO, &c, 1) != 1)
            {
                ev.key = Key::CTRL_D;
                return ev;
            }

            // Control characters
            if (c == '\r' || c == '\n')
            {
                ev.key = Key::ENTER;
                return ev;
            }
            if (c == 127 || c == 8)
            {
                ev.key = Key::BACKSPACE;
                return ev;
            }
            if (c == '\t')
            {
                ev.key = Key::TAB;
                return ev;
            }
            if (c == 1)
            {
                ev.key = Key::CTRL_A;
                return ev;
            } // Ctrl+A
            if (c == 3)
            {
                ev.key = Key::CTRL_C;
                return ev;
            }
            if (c == 4)
            {
                ev.key = Key::CTRL_D;
                return ev;
            }
            if (c == 5)
            {
                ev.key = Key::CTRL_E;
                return ev;
            }
            if (c == 11)
            {
                ev.key = Key::CTRL_K;
                return ev;
            }
            if (c == 12)
            {
                ev.key = Key::CTRL_L;
                return ev;
            }
            if (c == 21)
            {
                ev.key = Key::CTRL_U;
                return ev;
            }
            if (c == 23)
            {
                ev.key = Key::CTRL_W;
                return ev;
            }

            // Escape sequences
            if (c == 27)
            {
                char seq[6];
                if (::read(STDIN_FILENO, &seq[0], 1) != 1)
                {
                    ev.key = Key::UNKNOWN;
                    return ev;
                }

                // Alt+Enter: ESC followed by CR or LF
                if (seq[0] == '\r' || seq[0] == '\n')
                {
                    ev.key = Key::SHIFT_ENTER;
                    return ev;
                }

                if (seq[0] == '[')
                {
                    if (::read(STDIN_FILENO, &seq[1], 1) != 1)
                    {
                        ev.key = Key::UNKNOWN;
                        return ev;
                    }

                    // Kitty keyboard protocol: ESC[13;2u = Shift+Enter
                    if (seq[1] == '1')
                    {
                        // Could be ESC[1~ (Home), ESC[13;2u (Shift+Enter), etc.
                        char next;
                        if (::read(STDIN_FILENO, &next, 1) != 1)
                        {
                            ev.key = Key::HOME;
                            return ev;
                        }
                        if (next == '3')
                        {
                            // ESC[13... could be ESC[13;2u (Shift+Enter)
                            char mod;
                            if (::read(STDIN_FILENO, &mod, 1) != 1)
                            {
                                ev.key = Key::UNKNOWN;
                                return ev;
                            }
                            if (mod == ';')
                            {
                                // Read modifier and terminator
                                char modVal, term;
                                if (::read(STDIN_FILENO, &modVal, 1) != 1 ||
                                    ::read(STDIN_FILENO, &term, 1) != 1)
                                {
                                    ev.key = Key::UNKNOWN;
                                    return ev;
                                }
                                if (term == 'u' && (modVal == '2' || modVal == '6'))
                                {
                                    // Shift+Enter (;2u) or Ctrl+Shift+Enter (;6u)
                                    ev.key = Key::SHIFT_ENTER;
                                    return ev;
                                }
                                ev.key = Key::UNKNOWN;
                                return ev;
                            }
                            if (mod == '~')
                            {
                                // ESC[13~ — some terminals send this
                                ev.key = Key::UNKNOWN;
                                return ev;
                            }
                            ev.key = Key::UNKNOWN;
                            return ev;
                        }
                        if (next == '~')
                        {
                            ev.key = Key::HOME;
                            return ev;
                        }
                        if (next == ';')
                        {
                            // Modified key: ESC[1;Xm where X=modifier, m=direction
                            char modVal, dir;
                            if (::read(STDIN_FILENO, &modVal, 1) != 1 ||
                                ::read(STDIN_FILENO, &dir, 1) != 1)
                            {
                                ev.key = Key::UNKNOWN;
                                return ev;
                            }
                            // Just map direction, ignore modifier for now
                            switch (dir)
                            {
                            case 'A':
                                ev.key = Key::UP;
                                return ev;
                            case 'B':
                                ev.key = Key::DOWN;
                                return ev;
                            case 'C':
                                ev.key = Key::RIGHT;
                                return ev;
                            case 'D':
                                ev.key = Key::LEFT;
                                return ev;
                            case 'H':
                                ev.key = Key::HOME;
                                return ev;
                            case 'F':
                                ev.key = Key::END;
                                return ev;
                            }
                            ev.key = Key::UNKNOWN;
                            return ev;
                        }
                        ev.key = Key::HOME;
                        return ev;
                    }

                    // xterm modifyOtherKeys: ESC[27;2;13~ = Shift+Enter
                    if (seq[1] == '2')
                    {
                        char next;
                        if (::read(STDIN_FILENO, &next, 1) != 1)
                        {
                            ev.key = Key::UNKNOWN;
                            return ev;
                        }
                        if (next == '7')
                        {
                            // ESC[27;... — modifyOtherKeys format
                            char buf[10];
                            int idx = 0;
                            while (idx < 9)
                            {
                                if (::read(STDIN_FILENO, &buf[idx], 1) != 1)
                                    break;
                                if (buf[idx] == '~')
                                {
                                    buf[idx + 1] = 0;
                                    break;
                                }
                                idx++;
                            }
                            // Check if it ends with ;13~ (Enter with modifier)
                            std::string rest(buf, idx + 1);
                            if (rest.find(";13~") != std::string::npos)
                            {
                                ev.key = Key::SHIFT_ENTER;
                                return ev;
                            }
                            ev.key = Key::UNKNOWN;
                            return ev;
                        }
                        if (next == '~')
                        {
                            ev.key = Key::UNKNOWN; // ESC[2~ = Insert
                            return ev;
                        }
                        ev.key = Key::UNKNOWN;
                        return ev;
                    }

                    switch (seq[1])
                    {
                    case 'A':
                        ev.key = Key::UP;
                        return ev;
                    case 'B':
                        ev.key = Key::DOWN;
                        return ev;
                    case 'C':
                        ev.key = Key::RIGHT;
                        return ev;
                    case 'D':
                        ev.key = Key::LEFT;
                        return ev;
                    case 'H':
                        ev.key = Key::HOME;
                        return ev;
                    case 'F':
                        ev.key = Key::END;
                        return ev;
                    case '3':
                    {
                        // Delete key: ESC [ 3 ~
                        char tilde;
                        ssize_t r = ::read(STDIN_FILENO, &tilde, 1);
                        (void)r;
                        ev.key = Key::DELETE_KEY;
                        return ev;
                    }
                    case '7':
                    {
                        char tilde;
                        ssize_t r = ::read(STDIN_FILENO, &tilde, 1);
                        (void)r;
                        ev.key = Key::HOME;
                        return ev;
                    }
                    case '4':
                    case '8':
                    {
                        char tilde;
                        ssize_t r = ::read(STDIN_FILENO, &tilde, 1);
                        (void)r;
                        ev.key = Key::END;
                        return ev;
                    }
                    case '5':
                    {
                        // Page Up: ESC[5~ — ignore
                        char tilde;
                        ssize_t r = ::read(STDIN_FILENO, &tilde, 1);
                        (void)r;
                        ev.key = Key::UNKNOWN;
                        return ev;
                    }
                    case '6':
                    {
                        // Page Down: ESC[6~ — ignore
                        char tilde;
                        ssize_t r = ::read(STDIN_FILENO, &tilde, 1);
                        (void)r;
                        ev.key = Key::UNKNOWN;
                        return ev;
                    }
                    }
                }
                else if (seq[0] == 'O')
                {
                    if (::read(STDIN_FILENO, &seq[1], 1) != 1)
                    {
                        ev.key = Key::UNKNOWN;
                        return ev;
                    }
                    switch (seq[1])
                    {
                    case 'H':
                        ev.key = Key::HOME;
                        return ev;
                    case 'F':
                        ev.key = Key::END;
                        return ev;
                    }
                }
                ev.key = Key::UNKNOWN;
                return ev;
            }

            // Regular printable character
            if (c >= 32 && c < 127)
            {
                ev.key = Key::CHAR;
                ev.ch = c;
                return ev;
            }

            // Handle UTF-8 multi-byte sequences (pass through as chars)
            if ((unsigned char)c >= 0xC0)
            {
                ev.key = Key::CHAR;
                ev.ch = c;
                return ev;
            }

            ev.key = Key::UNKNOWN;
            return ev;
#endif
        }

        /// Get terminal width in columns
        int getWidth() const
        {
#ifdef _WIN32
            CONSOLE_SCREEN_BUFFER_INFO csbi;
            GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
            return csbi.srWindow.Right - csbi.srWindow.Left + 1;
#else
            struct winsize ws;
            if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1)
                return 80;
            return ws.ws_col;
#endif
        }

        /// Check if stdin is a TTY
        static bool isInteractive()
        {
#ifdef _WIN32
            return _isatty(_fileno(stdin));
#else
            return isatty(STDIN_FILENO);
#endif
        }

        // ---- ANSI output helpers -------------------------------------------

        static void write(const std::string &s)
        {
            ssize_t r = ::write(STDOUT_FILENO, s.c_str(), s.size());
            (void)r;
        }
        static void clearLine() { write("\r\033[2K"); }
        static void cursorForward(int n)
        {
            if (n > 0)
                write("\033[" + std::to_string(n) + "C");
        }
        static void cursorBackward(int n)
        {
            if (n > 0)
                write("\033[" + std::to_string(n) + "D");
        }
        static void clearScreen() { write("\033[2J\033[H"); }
        static void cursorToCol(int col) { write("\r\033[" + std::to_string(col) + "C"); }

    private:
        bool rawEnabled_ = false;
#ifndef _WIN32
        struct termios origTermios_;
#endif
    };

} // namespace xell
