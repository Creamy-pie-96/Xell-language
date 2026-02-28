// =============================================================================
// Xell — main entry point
// =============================================================================
//
// Usage:
//   xell                  Start the interactive REPL
//   xell <file.xel>       Execute a Xell script
//   xell --check <file>   Check a file for errors (used by LSP)
//   xell --customize      Launch the color customizer web app
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
    std::cout << "  xell --check <file>   Check file for errors (LSP mode)\n";
    std::cout << "  xell --customize      Launch color customizer\n";
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

// ---- Check a file for errors (LSP diagnostics) ----------------------------

static int checkFile(const std::string &path)
{
    std::string source = readFile(path);

    try
    {
        xell::Lexer lexer(source);
        auto tokens = lexer.tokenize();
        xell::Parser parser(tokens);
        auto program = parser.parse();

        // Also try running to catch runtime errors
        xell::Interpreter interpreter;
        interpreter.run(program);

        // No errors
        return 0;
    }
    catch (const xell::XellError &e)
    {
        // Output in the format the LSP server expects
        std::cerr << e.what() << "\n";
        return 1;
    }
    catch (const std::exception &e)
    {
        std::cerr << "[XELL ERROR] Line 1 — Fatal: " << e.what() << "\n";
        return 1;
    }
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

    if (arg1 == "--check")
    {
        if (argc < 3)
        {
            std::cerr << "Usage: xell --check <file>\n";
            return 1;
        }
        return checkFile(argv[2]);
    }

    if (arg1 == "--customize")
    {
        return launchCustomizer();
    }

    // Default: treat as file to execute
    return executeFile(arg1);
}
