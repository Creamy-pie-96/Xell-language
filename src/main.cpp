// =============================================================================
// Xell — main entry point
// =============================================================================
//
// Usage:
//   xell                  Start the interactive REPL
//   xell <file.xel>       Execute a Xell script
//   xell --check [file]   Lint: shallow parse-only check (file or stdin)
//   xell --lint [file]    Alias for --check
//   xell --customize      Launch the color customizer web app
//   xell --kernel          Run as a JSON kernel for notebook integration
//   xell --version        Print version information
//   xell --help           Print usage help
//
// =============================================================================

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <cstdlib>
#include <vector>

#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "interpreter/interpreter.hpp"
#include "analyzer/static_analyzer.hpp"
#include "lib/errors/error.hpp"
#include "repl/repl.hpp"

// ---- Helpers ----------------------------------------------------------------

static std::string readFile(const std::string &path)
{
    std::ifstream f(path);
    if (!f.is_open())
    {
        std::cerr << "Error: Cannot open file '" << path << "'\n";
        std::exit(1);
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static void printVersion()
{
    std::cout << "Xell Language v0.1.0\n";
    std::cout << "A modern scripting language with crystal-clear syntax.\n";
}

static void printHelp()
{
    std::cout << "Usage:\n";
    std::cout << "  xell                  Start interactive REPL\n";
    std::cout << "  xell <file.xel>       Execute a Xell script\n";
    std::cout << "  xell --check [file]   Lint: parse-only check (file or stdin)\n";
    std::cout << "  xell --lint [file]    Alias for --check\n";
    std::cout << "  xell --customize      Launch color customizer\n";
    std::cout << "  xell --kernel         Run as notebook kernel\n";
    std::cout << "  xell --version        Show version\n";
    std::cout << "  xell --help           Show this help\n";
}

// ---- Execute a file ---------------------------------------------------------

static int executeFile(const std::string &path)
{
    std::string source = readFile(path);

    try
    {
        xell::Lexer lexer(source);
        auto tokens = lexer.tokenize();
        xell::Parser parser(tokens);
        auto program = parser.parse();
        xell::Interpreter interpreter;
        interpreter.setSourceFile(path);
        interpreter.run(program);

        // Print captured output (from print() calls)
        for (auto &line : interpreter.output())
            std::cout << line << "\n";

        return 0;
    }
    catch (const xell::XellError &e)
    {
        std::cerr << e.what() << "\n";
        return 1;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 2;
    }
}

// ---- Lint / check a file for errors (shallow: lex + parse only) -----------

static std::string readStdin()
{
    std::ostringstream ss;
    ss << std::cin.rdbuf();
    return ss.str();
}

static int lintSource(const std::string &source)
{
    // Shallow check: lex + parse + static analysis — no execution.
    // This is fast and safe for the LSP to call on every keystroke.
    try
    {
        xell::Lexer lexer(source);
        auto tokens = lexer.tokenize();
        xell::Parser parser(tokens);
        auto program = parser.parse();

        // Static analysis: check for undefined names, typos, etc.
        xell::StaticAnalyzer analyzer;
        auto diagnostics = analyzer.analyze(program);

        int exitCode = 0;
        for (auto &d : diagnostics)
        {
            std::string prefix;
            if (d.severity == "error")
            {
                prefix = "[XELL ERROR]";
                exitCode = 1;
            }
            else if (d.severity == "warning")
                prefix = "[XELL WARNING]";
            else
                prefix = "[XELL HINT]";

            std::cerr << prefix << " Line " << d.line
                      << " \xe2\x80\x94 " << d.message << "\n";
        }

        return exitCode;
    }
    catch (const xell::XellError &e)
    {
        std::cerr << e.what() << "\n";
        return 1;
    }
    catch (const std::exception &e)
    {
        std::cerr << "[XELL ERROR] Line 1 — Fatal: " << e.what() << "\n";
        return 1;
    }
}

static int checkFile(const std::string &path)
{
    return lintSource(readFile(path));
}

static int checkStdin()
{
    return lintSource(readStdin());
}

// ---- Launch the color customizer -------------------------------------------

static int launchCustomizer()
{
    std::vector<std::string> searchPaths = {
        // Relative to CWD (dev mode)
        "Extensions/xell-vscode/color_customizer/customizer_server.py",
        "../Extensions/xell-vscode/color_customizer/customizer_server.py",
        "../../Extensions/xell-vscode/color_customizer/customizer_server.py",
        // System-wide install locations
        "/usr/local/share/xell/color_customizer/customizer_server.py",
        "/usr/share/xell/color_customizer/customizer_server.py",
    };

    // Also check ~/.local/share/xell and XELL_HOME
    const char *home = std::getenv("HOME");
    if (home)
    {
        searchPaths.insert(searchPaths.begin(),
                           std::string(home) + "/.local/share/xell/color_customizer/customizer_server.py");
    }

    // Also check XELL_HOME environment variable
    const char *xellHome = std::getenv("XELL_HOME");
    if (xellHome)
    {
        searchPaths.insert(searchPaths.begin(),
                           std::string(xellHome) + "/Extensions/xell-vscode/color_customizer/customizer_server.py");
    }

    std::string serverPath;
    for (auto &p : searchPaths)
    {
        std::ifstream f(p);
        if (f.good())
        {
            serverPath = p;
            break;
        }
    }

    if (serverPath.empty())
    {
        std::cerr << "Error: Cannot find customizer_server.py\n";
        std::cerr << "Set XELL_HOME to your Xell project root, or run from the project directory.\n";
        return 1;
    }

    std::cout << "🎨 Launching Xell Color Customizer...\n";
    std::cout << "   Server: " << serverPath << "\n";

    // Launch Python server
    std::string cmd = "python3 \"" + serverPath + "\"";
    return std::system(cmd.c_str());
}

// ---- JSON Kernel mode (for notebook integration) ---------------------------

static int runKernel()
{
    // Disable stdout buffering so each line is flushed immediately
    std::cout << std::unitbuf;

    // Signal that the kernel is ready
    std::cout << "{\"status\":\"kernel_ready\",\"version\":\"0.1.0\"}" << std::endl;

    xell::Interpreter interpreter;
    int executionCount = 0;

    std::string line;
    while (std::getline(std::cin, line))
    {
        if (line.empty())
            continue;

        // Minimal JSON parsing (no external lib dependency)
        // Expected input: {"action":"execute","cell_id":"...","code":"..."}
        //                 {"action":"reset"}
        //                 {"action":"shutdown"}

        // Extract action
        auto extractField = [&](const std::string &json, const std::string &key) -> std::string
        {
            std::string search = "\"" + key + "\"";
            auto pos = json.find(search);
            if (pos == std::string::npos)
                return "";
            // Find the colon after the key
            pos = json.find(':', pos + search.size());
            if (pos == std::string::npos)
                return "";
            pos++; // skip colon
            // Skip whitespace
            while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t'))
                pos++;
            if (pos >= json.size())
                return "";
            if (json[pos] == '"')
            {
                // String value — parse with escape handling
                pos++; // skip opening quote
                std::string result;
                while (pos < json.size() && json[pos] != '"')
                {
                    if (json[pos] == '\\' && pos + 1 < json.size())
                    {
                        pos++;
                        switch (json[pos])
                        {
                        case 'n':
                            result += '\n';
                            break;
                        case 't':
                            result += '\t';
                            break;
                        case '\\':
                            result += '\\';
                            break;
                        case '"':
                            result += '"';
                            break;
                        default:
                            result += '\\';
                            result += json[pos];
                            break;
                        }
                    }
                    else
                    {
                        result += json[pos];
                    }
                    pos++;
                }
                return result;
            }
            // Number or other
            auto end = json.find_first_of(",}", pos);
            return json.substr(pos, end - pos);
        };

        // JSON escape helper
        auto jsonEscape = [](const std::string &s) -> std::string
        {
            std::string out;
            out.reserve(s.size() + 16);
            for (char c : s)
            {
                switch (c)
                {
                case '"':
                    out += "\\\"";
                    break;
                case '\\':
                    out += "\\\\";
                    break;
                case '\n':
                    out += "\\n";
                    break;
                case '\r':
                    out += "\\r";
                    break;
                case '\t':
                    out += "\\t";
                    break;
                default:
                    out += c;
                }
            }
            return out;
        };

        std::string action = extractField(line, "action");

        if (action == "shutdown")
        {
            std::cout << "{\"status\":\"shutdown\"}" << std::endl;
            break;
        }

        if (action == "reset")
        {
            interpreter.reset();
            executionCount = 0;
            std::cout << "{\"status\":\"ok\",\"message\":\"Kernel reset\"}" << std::endl;
            continue;
        }

        if (action == "execute")
        {
            std::string cellId = extractField(line, "cell_id");
            std::string code = extractField(line, "code");
            executionCount++;

            std::string stdoutStr;
            std::string stderrStr;
            std::string resultStr;
            std::string status = "ok";

            try
            {
                xell::Lexer lexer(code);
                auto tokens = lexer.tokenize();
                xell::Parser parser(tokens);
                auto program = parser.parse();
                interpreter.run(program);

                // Collect output from print() calls
                const auto &output = interpreter.output();
                for (const auto &ln : output)
                {
                    if (!stdoutStr.empty())
                        stdoutStr += "\n";
                    stdoutStr += ln;
                }
                interpreter.clearOutput();
            }
            catch (const xell::XellError &e)
            {
                stderrStr = e.what();
                status = "error";
            }
            catch (const std::exception &e)
            {
                stderrStr = std::string("Fatal: ") + e.what();
                status = "error";
            }

            std::cout << "{\"cell_id\":\"" << jsonEscape(cellId) << "\""
                      << ",\"status\":\"" << status << "\""
                      << ",\"stdout\":\"" << jsonEscape(stdoutStr) << "\""
                      << ",\"stderr\":\"" << jsonEscape(stderrStr) << "\""
                      << ",\"result\":\"" << jsonEscape(resultStr) << "\""
                      << ",\"execution_count\":" << executionCount
                      << "}" << std::endl;
            continue;
        }

        // Unknown action
        std::cout << "{\"status\":\"error\",\"stderr\":\"Unknown action: " << action << "\"}" << std::endl;
    }

    return 0;
}

// ---- Main -------------------------------------------------------------------

int main(int argc, char *argv[])
{
    if (argc == 1)
    {
        // No arguments — start REPL
        xell::Repl repl;
        repl.run();
        return 0;
    }

    std::string arg1 = argv[1];

    if (arg1 == "--version" || arg1 == "-v")
    {
        printVersion();
        return 0;
    }

    if (arg1 == "--help" || arg1 == "-h")
    {
        printHelp();
        return 0;
    }

    if (arg1 == "--check" || arg1 == "--lint")
    {
        if (argc >= 3)
            return checkFile(argv[2]);
        // No file argument → read from stdin
        return checkStdin();
    }

    if (arg1 == "--customize")
    {
        return launchCustomizer();
    }

    if (arg1 == "--kernel")
    {
        return runKernel();
    }

    // Default: treat as file to execute
    return executeFile(arg1);
}
