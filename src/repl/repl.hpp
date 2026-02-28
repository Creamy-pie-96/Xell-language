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
#include <iostream>
#include <string>

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

            std::string accumulated;
            int depth = 0;

            while (true)
            {
                std::string prompt = makePrompt(depth);
                std::string line;

                bool ok = editor_.readLine(prompt, line);
                if (!ok)
                {
                    // Ctrl+D — exit
                    if (accumulated.empty())
                    {
                        terminal_.disableRawMode();
                        std::cout << color::DIM << "Goodbye!" << color::RESET << std::endl;
                        history_.save(historyPath_);
                        return;
                    }
                    // Cancel multi-line
                    accumulated.clear();
                    depth = 0;
                    continue;
                }

                if (line.empty() && accumulated.empty())
                    continue;

                // Handle special REPL commands
                if (accumulated.empty() && handleCommand(line))
                    continue;

                // Accumulate input
                if (!accumulated.empty())
                    accumulated += "\n";
                accumulated += line;

                // Count scope depth
                depth = computeDepth(accumulated);

                if (depth > 0)
                    continue; // Need more input

                // Execute the accumulated input
                history_.add(accumulated);
                execute(accumulated);
                accumulated.clear();
                depth = 0;
            }
        }

    private:
        Terminal terminal_;
        History history_;
        Completer completer_;
        LineEditor editor_;
        Interpreter interpreter_;
        std::string historyPath_;

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
            return std::string(color::CYAN) + color::BOLD + "xell" +
                   color::RESET + std::string(color::DIM) + " ▸ " + color::RESET;
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
            Terminal::write(std::string(color::DIM) + "    Tab       " + color::RESET + "Auto-complete\r\n");
            Terminal::write(std::string(color::DIM) + "    ↑/↓       " + color::RESET + "History navigation\r\n");
            Terminal::write(std::string(color::DIM) + "    Ctrl+C    " + color::RESET + "Cancel current input\r\n");
            Terminal::write(std::string(color::DIM) + "    Ctrl+D    " + color::RESET + "Exit (on empty line)\r\n");
            Terminal::write(std::string(color::DIM) + "    Ctrl+L    " + color::RESET + "Clear screen\r\n");
            Terminal::write(std::string(color::DIM) + "    Ctrl+W    " + color::RESET + "Delete word backward\r\n");
            Terminal::write(std::string(color::DIM) + "    Ctrl+U    " + color::RESET + "Delete to line start\r\n");
            Terminal::write(std::string(color::DIM) + "    Ctrl+K    " + color::RESET + "Delete to line end\r\n");
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
