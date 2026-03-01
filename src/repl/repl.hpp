#pragma once

// =============================================================================
// Repl — interactive read-eval-print loop for Xell
// =============================================================================
// Features:
//   - Line editing (cursor movement, word delete, etc.)
//   - Command history (persisted to ~/.xell_history)
//   - Tab completion (keywords, builtins, user variables)
//   - Multi-line input (auto-detects unclosed : blocks)
//   - Colored prompt and output
//   - Ctrl+C cancels current input, Ctrl+D exits
// =============================================================================

#include "terminal.hpp"
#include "line_editor.hpp"
#include "history.hpp"
#include "completer.hpp"
#include "../lexer/lexer.hpp"
#include "../parser/parser.hpp"
#include "../interpreter/interpreter.hpp"
#include "../os/os.hpp"
#include <iostream>
#include <string>
#include <cstdlib>

#ifndef _WIN32
#include <unistd.h>
#include <pwd.h>
#include <climits>
#else
#include <windows.h>
#include <direct.h>
#ifndef PATH_MAX
#define PATH_MAX MAX_PATH
#endif
#endif

namespace xell
{

    // ANSI color codes
    namespace color
    {
        const char *RESET = "\033[0m";
        const char *BOLD = "\033[1m";
        const char *DIM = "\033[2m";
        const char *CYAN = "\033[36m";
        const char *GREEN = "\033[32m";
        const char *YELLOW = "\033[33m";
        const char *RED = "\033[31m";
        const char *BLUE = "\033[34m";
        const char *MAGENTA = "\033[35m";
    } // namespace color

    class Repl
    {
    public:
        Repl() : editor_(terminal_, history_, completer_)
        {
            completer_.setEnvironment(&interpreter_.globals());

            // Detect if we're running inside the Xell Terminal emulator.
            // When XELL_TERMINAL=1, we use a shell-like prompt (user@host:cwd)
            // and single-Enter execution instead of multiline-first editing.
            const char *xt = std::getenv("XELL_TERMINAL");
            isTerminalMode_ = (xt && std::string(xt) == "1");
            editor_.setTerminalMode(isTerminalMode_);
        }

        void run()
        {
            // Load history
            historyPath_ = History::defaultPath();
            history_.load(historyPath_);

            if (!terminal_.enableRawMode())
            {
                std::cerr << "Warning: Could not enable raw terminal mode.\n";
                runFallback();
                return;
            }

            printBanner();

            while (true)
            {
                std::string prompt = makePrompt(0);
                std::string contPrompt = makeContPrompt();
                std::string input;

                bool ok = editor_.readLine(prompt, contPrompt, input);
                if (!ok)
                {
                    // Ctrl+D — exit
                    terminal_.disableRawMode();
                    std::cout << color::DIM << "Goodbye!" << color::RESET << std::endl;
                    history_.save(historyPath_);
                    return;
                }

                if (input.empty())
                    continue;

                // Handle special REPL commands
                if (handleCommand(input))
                    continue;

                // Execute the input (can be multiline)
                history_.add(input);
                execute(input);
            }
        }

    private:
        Terminal terminal_;
        History history_;
        Completer completer_;
        LineEditor editor_;
        Interpreter interpreter_;
        bool isTerminalMode_ = false;
        std::string historyPath_;

        // ---- Shell-style prompt helpers (for terminal mode) -----------------

        static std::string getUsername()
        {
#ifdef _WIN32
            char buf[256];
            DWORD len = sizeof(buf);
            if (GetUserNameA(buf, &len))
                return buf;
            const char *user = std::getenv("USERNAME");
            return user ? user : "user";
#else
            const char *user = std::getenv("USER");
            if (user)
                return user;
            struct passwd *pw = getpwuid(getuid());
            return pw ? pw->pw_name : "user";
#endif
        }

        static std::string getHostname()
        {
#ifdef _WIN32
            char buf[256];
            DWORD len = sizeof(buf);
            if (GetComputerNameA(buf, &len))
                return buf;
            return "pc";
#else
            char buf[256];
            if (gethostname(buf, sizeof(buf)) == 0)
            {
                // Trim domain part (e.g., "macbook.local" → "macbook")
                std::string h(buf);
                auto dot = h.find('.');
                if (dot != std::string::npos)
                    h = h.substr(0, dot);
                return h;
            }
            return "localhost";
#endif
        }

        static std::string getShortCwd()
        {
            char buf[PATH_MAX];
#ifdef _WIN32
            if (_getcwd(buf, sizeof(buf)))
            {
#else
            if (getcwd(buf, sizeof(buf)))
            {
#endif
                std::string cwd(buf);
                // Replace $HOME with ~
                const char *home = std::getenv("HOME");
#ifdef _WIN32
                if (!home)
                    home = std::getenv("USERPROFILE");
#endif
                if (home)
                {
                    std::string h(home);
                    if (cwd == h)
                        return "~";
                    if (cwd.size() > h.size() && cwd.substr(0, h.size()) == h &&
                        (cwd[h.size()] == '/' || cwd[h.size()] == '\\'))
                    {
                        return "~" + cwd.substr(h.size());
                    }
                }
                return cwd;
            }
            return "?";
        }

        // -----------------------------------------------------------------

        void printBanner()
        {
            std::string banner;
            banner += "\r\n";
            banner += std::string(color::CYAN) + color::BOLD + "  ╔═══════════════════════════════════════╗\r\n";
            banner += std::string("  ║") + color::RESET + "   ";
            banner += std::string(color::BLUE) + color::BOLD + "✦ Xell" + color::RESET;
            banner += std::string(color::DIM) + " Interactive Shell v0.1.0" + color::RESET;
            banner += std::string(color::CYAN) + color::BOLD + "     ║\r\n";
            banner += std::string("  ╚═══════════════════════════════════════╝") + color::RESET + "\r\n";
            banner += std::string(color::DIM) + "  Type :help for commands, Ctrl+D to exit\r\n" + color::RESET;

            if (isTerminalMode_)
            {
                banner += std::string(color::DIM) + "  Enter=run  Shift+Enter=newline  (Terminal Mode)\r\n" + color::RESET;
            }
            else
            {
                banner += std::string(color::DIM) + "  Enter=newline  Shift+Enter/empty line=run\r\n" + color::RESET;
            }

            banner += "\r\n";
            Terminal::write(banner);
        }

        std::string makePrompt(int depth)
        {
            if (depth > 0)
            {
                // Continuation prompt — show depth
                std::string dots(depth * 2, '.');
                return std::string(color::DIM) + "  " + dots + " " + color::RESET;
            }

            if (isTerminalMode_)
            {
                // Shell-like prompt:  user@host:~/path $
                std::string user = getUsername();
                std::string host = getHostname();
                std::string cwd = getShortCwd();

                return std::string(color::BOLD) + color::GREEN + user + "@" + host + color::RESET + ":" + std::string(color::BOLD) + color::BLUE + cwd + color::RESET + " $ ";
            }

            // Default REPL prompt
            return std::string(color::CYAN) + color::BOLD + "xell" +
                   color::RESET + std::string(color::DIM) + " ▸ " + color::RESET;
        }

        std::string makeContPrompt()
        {
            if (isTerminalMode_)
            {
                return std::string(color::DIM) + "... " + color::RESET;
            }
            return std::string(color::DIM) + "  ·· " + color::RESET;
        }

        /// Returns how many more `;` we need to close open `:` blocks
        int computeDepth(const std::string &code)
        {
            int depth = 0;
            // Simple heuristic: count unmatched : vs ;
            // We need to be careful about : inside strings and maps
            bool inString = false;
            bool escaped = false;
            int parenDepth = 0;   // ()
            int braceDepth = 0;   // {}
            int bracketDepth = 0; // []

            for (size_t i = 0; i < code.size(); i++)
            {
                char c = code[i];

                if (escaped)
                {
                    escaped = false;
                    continue;
                }
                if (c == '\\' && inString)
                {
                    escaped = true;
                    continue;
                }
                if (c == '"')
                {
                    inString = !inString;
                    continue;
                }
                if (inString)
                    continue;

                // Skip comments
                if (c == '#')
                {
                    while (i < code.size() && code[i] != '\n')
                        i++;
                    continue;
                }
                if (c == '-' && i + 2 < code.size() && code[i + 1] == '-' && code[i + 2] == '>')
                {
                    i += 3;
                    while (i + 2 < code.size())
                    {
                        if (code[i] == '<' && code[i + 1] == '-' && code[i + 2] == '-')
                        {
                            i += 2;
                            break;
                        }
                        i++;
                    }
                    continue;
                }

                if (c == '(')
                    parenDepth++;
                else if (c == ')')
                    parenDepth--;
                else if (c == '{')
                    braceDepth++;
                else if (c == '}')
                    braceDepth--;
                else if (c == '[')
                    bracketDepth++;
                else if (c == ']')
                    bracketDepth--;
                else if (c == ':' && parenDepth == 0 && braceDepth == 0 && bracketDepth == 0)
                {
                    // Check if this looks like a block opener (after keyword, before newline/EOF)
                    // Simple: it's a scope colon if not inside brackets and followed by space/newline/EOF
                    depth++;
                }
                else if (c == ';' && parenDepth == 0 && braceDepth == 0 && bracketDepth == 0)
                {
                    depth--;
                }
            }
            return depth > 0 ? depth : 0;
        }

        /// Handle special REPL commands (lines starting with :)
        bool handleCommand(const std::string &line)
        {
            if (line == ":help" || line == ":h")
            {
                printHelp();
                return true;
            }
            if (line == ":quit" || line == ":q")
            {
                terminal_.disableRawMode();
                std::cout << color::DIM << "Goodbye!" << color::RESET << std::endl;
                history_.save(historyPath_);
                std::exit(0);
            }
            if (line == ":clear" || line == ":cls")
            {
                Terminal::clearScreen();
                return true;
            }
            if (line == ":reset")
            {
                interpreter_.reset();
                completer_.setEnvironment(&interpreter_.globals());
                Terminal::write(std::string(color::YELLOW) + "Environment reset.\r\n" + color::RESET);
                return true;
            }
            if (line == ":history" || line == ":hist")
            {
                auto &entries = history_.entries();
                for (size_t i = 0; i < entries.size(); i++)
                {
                    Terminal::write(std::string(color::DIM) + std::to_string(i + 1) +
                                    ": " + color::RESET + entries[i] + "\r\n");
                }
                return true;
            }
            if (line == ":vars" || line == ":env")
            {
                auto names = interpreter_.globals().allNames();
                if (names.empty())
                {
                    Terminal::write(std::string(color::DIM) + "(no variables defined)\r\n" + color::RESET);
                }
                else
                {
                    std::sort(names.begin(), names.end());
                    for (auto &n : names)
                    {
                        try
                        {
                            auto val = interpreter_.globals().get(n, 0);
                            Terminal::write(std::string(color::CYAN) + n + color::RESET +
                                            " = " + val.toString() + "\r\n");
                        }
                        catch (...)
                        {
                        }
                    }
                }
                return true;
            }

            // ---- Shell-like built-in commands (for terminal mode) ----

            // cd <path> — change working directory
            if (line == "cd" || line.substr(0, 3) == "cd ")
            {
                std::string target;
                if (line.size() > 3)
                    target = line.substr(3);
                // Trim whitespace
                while (!target.empty() && (target.front() == ' ' || target.front() == '\t'))
                    target.erase(target.begin());
                while (!target.empty() && (target.back() == ' ' || target.back() == '\t'))
                    target.pop_back();
                // Remove surrounding quotes if present
                if (target.size() >= 2 &&
                    ((target.front() == '"' && target.back() == '"') ||
                     (target.front() == '\'' && target.back() == '\'')))
                {
                    target = target.substr(1, target.size() - 2);
                }
                // Default to home directory
                if (target.empty() || target == "~")
                {
                    const char *home = std::getenv("HOME");
#ifdef _WIN32
                    if (!home) home = std::getenv("USERPROFILE");
#endif
                    if (home)
                        target = home;
                    else
                    {
                        Terminal::write(std::string(color::RED) + "cd: HOME not set\r\n" + color::RESET);
                        return true;
                    }
                }
                // Handle ~ prefix
                if (target.size() >= 2 && target[0] == '~' && (target[1] == '/' || target[1] == '\\'))
                {
                    const char *home = std::getenv("HOME");
#ifdef _WIN32
                    if (!home) home = std::getenv("USERPROFILE");
#endif
                    if (home)
                        target = std::string(home) + target.substr(1);
                }

#ifdef _WIN32
                int rc = _chdir(target.c_str());
#else
                int rc = chdir(target.c_str());
#endif
                if (rc != 0)
                {
                    Terminal::write(std::string(color::RED) + "cd: " + target +
                                    ": No such file or directory\r\n" + color::RESET);
                }
                return true;
            }

            // pwd — print working directory
            if (line == "pwd")
            {
                char buf[PATH_MAX];
#ifdef _WIN32
                if (_getcwd(buf, sizeof(buf)))
#else
                if (getcwd(buf, sizeof(buf)))
#endif
                {
                    Terminal::write(std::string(buf) + "\r\n");
                }
                else
                {
                    Terminal::write(std::string(color::RED) + "pwd: error\r\n" + color::RESET);
                }
                return true;
            }

            // ls — list directory (delegate to system)
            if (line == "ls" || line.substr(0, 3) == "ls ")
            {
                // Run ls directly as a shell command
                os::run(line);
                return true;
            }

            // run <command> — execute a shell command
            // Supports:
            //   run echo "hello" && ls     (paren-less, raw text after "run ")
            //   run(echo "hello" && ls)    (parens act as delimiters, content is raw)
            //   run("echo hello")          (backward compat: strips surrounding quotes)
            if (line == "run" || line.substr(0, 4) == "run " || line.substr(0, 4) == "run(")
            {
                std::string cmd;
                if (line.size() >= 4 && line[3] == '(')
                {
                    // run(command) syntax — extract content between parens
                    size_t end_paren = line.rfind(')');
                    if (end_paren != std::string::npos && end_paren > 4)
                        cmd = line.substr(4, end_paren - 4);
                    else
                        cmd = line.substr(4); // no closing paren, use rest

                    // Trim whitespace
                    while (!cmd.empty() && (cmd.front() == ' ' || cmd.front() == '\t'))
                        cmd.erase(cmd.begin());
                    while (!cmd.empty() && (cmd.back() == ' ' || cmd.back() == '\t'))
                        cmd.pop_back();

                    // If it's a single quoted string like run("cmd"), strip quotes
                    // to maintain backward compatibility with run("echo hello")
                    if (cmd.size() >= 2 && cmd[0] == '"' && cmd.back() == '"' &&
                        cmd.find('"', 1) == cmd.size() - 1)
                    {
                        cmd = cmd.substr(1, cmd.size() - 2);
                    }
                }
                else if (line.size() > 4)
                {
                    // run command args... — paren-less, everything after "run " is the command
                    cmd = line.substr(4);
                    while (!cmd.empty() && (cmd.front() == ' ' || cmd.front() == '\t'))
                        cmd.erase(cmd.begin());
                }

                if (cmd.empty())
                {
                    Terminal::write(std::string(color::RED) + "run: no command specified\r\n" + color::RESET);
                    return true;
                }

                // If the command is "cd <path>", handle with chdir() instead
                // of spawning a subprocess (which can't change our cwd)
                if (cmd == "cd" || (cmd.size() >= 3 && cmd.substr(0, 3) == "cd "))
                {
                    return handleCommand(cmd); // delegate to the cd handler above
                }

                int code = os::run(cmd);
                if (code != 0)
                {
                    Terminal::write(std::string(color::RED) + "[exit " +
                                    std::to_string(code) + "]" + color::RESET + "\r\n");
                }
                return true;
            }

            return false;
        }

        void printHelp()
        {
            Terminal::write("\r\n");
            Terminal::write(std::string(color::BOLD) + "  REPL Commands:\r\n" + color::RESET);
            Terminal::write(std::string(color::CYAN) + "    :help     " + color::RESET + "Show this help\r\n");
            Terminal::write(std::string(color::CYAN) + "    :quit     " + color::RESET + "Exit the REPL\r\n");
            Terminal::write(std::string(color::CYAN) + "    :clear    " + color::RESET + "Clear the screen\r\n");
            Terminal::write(std::string(color::CYAN) + "    :reset    " + color::RESET + "Reset the environment\r\n");
            Terminal::write(std::string(color::CYAN) + "    :history  " + color::RESET + "Show command history\r\n");
            Terminal::write(std::string(color::CYAN) + "    :vars     " + color::RESET + "Show defined variables\r\n");
            Terminal::write("\r\n");
            Terminal::write(std::string(color::BOLD) + "  Keyboard:\r\n" + color::RESET);
            Terminal::write(std::string(color::DIM) + "    Enter         " + color::RESET + "New line (multiline editing)\r\n");
            Terminal::write(std::string(color::DIM) + "    Shift+Enter   " + color::RESET + "Execute code (or Alt+Enter)\r\n");
            Terminal::write(std::string(color::DIM) + "    Tab           " + color::RESET + "Auto-complete\r\n");
            Terminal::write(std::string(color::DIM) + "    ↑/↓           " + color::RESET + "Navigate lines or history\r\n");
            Terminal::write(std::string(color::DIM) + "    ←/→           " + color::RESET + "Move cursor\r\n");
            Terminal::write(std::string(color::DIM) + "    Ctrl+C        " + color::RESET + "Cancel current input\r\n");
            Terminal::write(std::string(color::DIM) + "    Ctrl+D        " + color::RESET + "Exit (on empty buffer)\r\n");
            Terminal::write(std::string(color::DIM) + "    Ctrl+L        " + color::RESET + "Clear screen\r\n");
            Terminal::write(std::string(color::DIM) + "    Ctrl+W        " + color::RESET + "Delete word backward\r\n");
            Terminal::write(std::string(color::DIM) + "    Ctrl+U/K      " + color::RESET + "Delete to line start/end\r\n");
            Terminal::write("\r\n");
        }

        void execute(const std::string &code)
        {
            try
            {
                // Clear output buffer before execution
                interpreter_.clearOutput();

                Lexer lexer(code);
                auto tokens = lexer.tokenize();
                Parser parser(tokens);
                auto program = parser.parse();
                interpreter_.run(program);

                // Print any new output
                auto &out = interpreter_.output();
                for (auto &line : out)
                {
                    Terminal::write(line + "\r\n");
                }
            }
            catch (const XellError &e)
            {
                Terminal::write(std::string(color::RED) + e.what() + color::RESET + "\r\n");
            }
            catch (const std::exception &e)
            {
                Terminal::write(std::string(color::RED) + "Error: " + e.what() + color::RESET + "\r\n");
            }
        }

        /// Fallback for non-interactive terminals
        void runFallback()
        {
            std::cout << "Xell Interactive Shell v0.1.0 (basic mode)\n";
            std::cout << "Type :help for commands, Ctrl+D to exit\n\n";

            std::string accumulated;
            int depth = 0;

            while (true)
            {
                if (depth > 0)
                    std::cout << ".. ";
                else
                    std::cout << "xell> ";
                std::cout.flush();

                std::string line;
                if (!std::getline(std::cin, line))
                    break;

                if (line.empty() && accumulated.empty())
                    continue;

                if (accumulated.empty() && handleCommand(line))
                    continue;

                if (!accumulated.empty())
                    accumulated += "\n";
                accumulated += line;

                depth = computeDepth(accumulated);
                if (depth > 0)
                    continue;

                history_.add(accumulated);
                execute(accumulated);
                accumulated.clear();
                depth = 0;
            }
        }
    };

} // namespace xell
