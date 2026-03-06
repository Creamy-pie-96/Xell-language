// =============================================================================
// Xell — main entry point
// =============================================================================
//
// Usage:
//   xell                  Start the interactive REPL
//   xell <file.xel>       Execute a Xell script
//   xell --check [file]   Lint: shallow parse-only check (file or stdin)
//   xell --lint [file]    Alias for --check
//   xell --check-string <code>  Lint raw source string (no file I/O)
//   xell --convert <file> [map.xesy]   Convert dialect → canonical in-place
//   xell --revert  <file> [map.xesy]   Restore dialect from canonical
//   xell --gen_xesy [output.xesy]      Generate template mapping file
//   xell --terminal       Launch the Xell Terminal (SDL2 GUI)
//   xell --ide             Alias for --terminal
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
#include <map>
#include <set>
#include <regex>
#include <algorithm>
#include <filesystem>

#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "interpreter/interpreter.hpp"
#include "analyzer/static_analyzer.hpp"
#include "analyzer/symbol_collector.hpp"
#include "lib/errors/error.hpp"
#include "repl/repl.hpp"
#include "hash/hash_algorithm.hpp"
#include "common/dialect_convert.hpp"

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
    std::cout << "  xell --check-string <code>  Lint raw source string (no file I/O)\n";
    std::cout << "  xell --convert <file> [map.xesy]\n";
    std::cout << "                        Convert dialect file to canonical Xell in-place\n";
    std::cout << "  xell --revert <file> [map.xesy]\n";
    std::cout << "                        Restore dialect from canonical, re-add @convert\n";
    std::cout << "  xell --gen_xesy [output.xesy]\n";
    std::cout << "                        Generate a template .xesy mapping file\n";
    std::cout << "  xell --make_module <path> ...\n";
    std::cout << "                        Register modules & build .xell_meta + cache\n";
    std::cout << "  xell --make_module --update <path> ...\n";
    std::cout << "                        Update changed modules from hash & build .xell_meta + cache\n";
    std::cout << "  xell --terminal       Launch Xell Terminal (SDL2 GUI)\n";
    std::cout << "  xell --ide            Alias for --terminal\n";
    std::cout << "  xell --customize      Launch color customizer\n";
    std::cout << "  xell --kernel         Run as notebook kernel\n";
    std::cout << "  xell --version        Show version\n";
    std::cout << "  xell --help           Show this help\n";
}

// ---- @convert dialect system ------------------------------------------------
// Core utilities live in common/dialect_convert.hpp (shared with interpreter).
// The functions below are main.cpp-specific wrappers that call std::exit(1)
// on errors (appropriate for CLI commands but not for the interpreter).

/// CLI version of parseXesyFile that exits on error.
static std::map<std::string, std::string> parseXesyFile(const std::string &path)
{
    std::map<std::string, std::string> mapping;
    std::ifstream f(path);
    if (!f.is_open())
    {
        std::cerr << "Error: Cannot open mapping file '" << path << "'\n";
        std::exit(1);
    }

    std::string content((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());

    // Simple state-machine JSON parser for flat objects
    // Handles nested _meta object by brace-counting to skip it
    size_t pos = content.find('{');
    if (pos == std::string::npos)
    {
        std::cerr << "Error: Invalid .xesy file (no opening '{') in '" << path << "'\n";
        std::exit(1);
    }
    pos++; // skip opening brace

    auto skipWhitespace = [&]()
    {
        while (pos < content.size() && std::isspace(content[pos]))
            pos++;
    };

    auto readString = [&]() -> std::string
    {
        if (pos >= content.size() || content[pos] != '"')
            return "";
        pos++; // skip opening quote
        std::string result;
        while (pos < content.size() && content[pos] != '"')
        {
            if (content[pos] == '\\' && pos + 1 < content.size())
            {
                pos++;
                switch (content[pos])
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
                    result += content[pos];
                    break;
                }
            }
            else
            {
                result += content[pos];
            }
            pos++;
        }
        if (pos < content.size())
            pos++; // skip closing quote
        return result;
    };

    while (pos < content.size())
    {
        skipWhitespace();
        if (pos >= content.size() || content[pos] == '}')
            break;
        if (content[pos] == ',')
        {
            pos++;
            continue;
        }

        // Read key
        std::string key = readString();
        skipWhitespace();
        if (pos < content.size() && content[pos] == ':')
            pos++; // skip colon
        skipWhitespace();

        // If value is an object (like _meta), skip it by brace-matching
        if (pos < content.size() && content[pos] == '{')
        {
            int depth = 1;
            pos++; // skip opening brace
            while (pos < content.size() && depth > 0)
            {
                if (content[pos] == '{')
                    depth++;
                else if (content[pos] == '}')
                    depth--;
                pos++;
            }
            continue;
        }

        // Read value
        std::string value = readString();

        // Only add non-empty mappings, skip _meta
        if (!key.empty() && !value.empty() && key != "_meta")
        {
            mapping[key] = value;
        }
    }

    return mapping;
}

/// Invert a mapping: canonical→custom becomes custom→canonical
static std::map<std::string, std::string> invertMapping(const std::map<std::string, std::string> &mapping)
{
    std::map<std::string, std::string> inverted;
    for (auto &[canonical, custom] : mapping)
    {
        if (!custom.empty())
            inverted[custom] = canonical;
    }
    return inverted;
}

/// Replace all mapped words in source (whole-word boundary matching).
/// `wordMap` maps from_word → to_word.
static std::string replaceWords(const std::string &source,
                                const std::map<std::string, std::string> &wordMap)
{
    if (wordMap.empty())
        return source;

    // Build a regex alternation of all source words, longest first for greedy match
    std::vector<std::string> words;
    words.reserve(wordMap.size());
    for (auto &[from, to] : wordMap)
        words.push_back(from);
    // Sort by length descending so longer words match first
    std::sort(words.begin(), words.end(),
              [](const std::string &a, const std::string &b)
              { return a.size() > b.size(); });

    std::string pattern = "\\b(";
    for (size_t i = 0; i < words.size(); i++)
    {
        if (i > 0)
            pattern += "|";
        pattern += words[i];
    }
    pattern += ")\\b";

    std::regex re(pattern);
    std::string result;
    result.reserve(source.size());

    auto begin = std::sregex_iterator(source.begin(), source.end(), re);
    auto end = std::sregex_iterator();

    size_t lastPos = 0;
    bool inString = false;

    // We need string-awareness: don't replace inside string literals.
    // Use a simple character-by-character approach instead.
    result.clear();

    // Character-by-character approach with word boundary detection
    size_t i = 0;
    while (i < source.size())
    {
        // Track strings
        if (source[i] == '"' && (i == 0 || source[i - 1] != '\\'))
        {
            inString = !inString;
            result += source[i];
            i++;
            continue;
        }
        if (inString)
        {
            result += source[i];
            i++;
            continue;
        }

        // Skip comments (# to end of line)
        if (source[i] == '#')
        {
            while (i < source.size() && source[i] != '\n')
            {
                result += source[i];
                i++;
            }
            continue;
        }

        // Skip arrow comments (-> ... <-)
        if (source[i] == '-' && i + 1 < source.size() && source[i + 1] == '>')
        {
            result += source[i];
            result += source[i + 1];
            i += 2;
            while (i < source.size())
            {
                if (source[i] == '<' && i + 1 < source.size() && source[i + 1] == '-')
                {
                    result += source[i];
                    result += source[i + 1];
                    i += 2;
                    break;
                }
                result += source[i];
                i++;
            }
            continue;
        }

        // Check for word boundary — start of an identifier
        if (std::isalpha(source[i]) || source[i] == '_')
        {
            size_t wordStart = i;
            std::string word;
            while (i < source.size() && (std::isalnum(source[i]) || source[i] == '_'))
            {
                word += source[i];
                i++;
            }

            auto it = wordMap.find(word);
            if (it != wordMap.end())
            {
                result += it->second;
            }
            else
            {
                result += word;
            }
            continue;
        }

        result += source[i];
        i++;
    }

    return result;
}

/// Parse @convert directive from first 5 non-blank lines of source.
/// Returns: path string if found (may be empty if no path given), or empty optional if not found.
struct ConvertDirective
{
    bool found = false;
    std::string mappingPath; // empty if @convert with no path
    int lineIndex = -1;      // 0-based line index of the @convert line
};

static ConvertDirective parseConvertDirective(const std::string &source)
{
    ConvertDirective result;
    std::istringstream stream(source);
    std::string line;
    int lineNum = 0;
    int nonBlankCount = 0;

    while (std::getline(stream, line) && nonBlankCount < 5)
    {
        // Trim leading whitespace
        size_t start = line.find_first_not_of(" \t\r");
        if (start == std::string::npos)
        {
            lineNum++;
            continue; // blank line
        }
        nonBlankCount++;

        std::string trimmed = line.substr(start);

        // Match: @convert "path" or @convert (no arg)
        if (trimmed.rfind("@convert", 0) == 0)
        {
            result.found = true;
            result.lineIndex = lineNum;

            // Extract path from @convert "..."
            auto quoteStart = trimmed.find('"', 8);
            if (quoteStart != std::string::npos)
            {
                auto quoteEnd = trimmed.find('"', quoteStart + 1);
                if (quoteEnd != std::string::npos)
                {
                    result.mappingPath = trimmed.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                }
            }
            return result;
        }

        lineNum++;
    }
    return result;
}

/// Remove the @convert line from source text (by line index)
static std::string stripConvertLine(const std::string &source, int lineIndex)
{
    std::istringstream stream(source);
    std::string line;
    std::ostringstream result;
    int lineNum = 0;
    bool first = true;

    while (std::getline(stream, line))
    {
        if (lineNum != lineIndex)
        {
            if (!first)
                result << "\n";
            result << line;
            first = false;
        }
        lineNum++;
    }
    return result.str();
}

/// Find a .xesy file in the same directory as the given file
static std::string findXesyInDirectory(const std::string &filePath)
{
    namespace fs = std::filesystem;
    fs::path dir = fs::path(filePath).parent_path();
    if (dir.empty())
        dir = ".";

    for (auto &entry : fs::directory_iterator(dir))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".xesy")
        {
            return entry.path().string();
        }
    }
    return "";
}

/// Resolve mapping path relative to the code file's directory
static std::string resolveXesyPath(const std::string &codeFilePath, const std::string &mappingPath)
{
    if (mappingPath.empty())
    {
        // Auto-detect: find *.xesy in same directory
        return findXesyInDirectory(codeFilePath);
    }

    namespace fs = std::filesystem;
    fs::path mp(mappingPath);
    if (mp.is_absolute())
        return mp.string();

    // Resolve relative to code file's directory
    fs::path codeDir = fs::path(codeFilePath).parent_path();
    if (codeDir.empty())
        codeDir = ".";
    return (codeDir / mp).string();
}

/// Apply @convert: CLI version that exits on missing .xesy.
/// For interpreter-side use, see dialect::applyConvertIfNeeded().
static std::string applyConvertIfNeeded(const std::string &source, const std::string &filePath)
{
    auto directive = parseConvertDirective(source);
    if (!directive.found)
        return source;

    std::string xesyPath = resolveXesyPath(filePath, directive.mappingPath);
    if (xesyPath.empty() || !std::filesystem::exists(xesyPath))
    {
        if (directive.mappingPath.empty())
        {
            std::cerr << "[XELL ERROR] Line " << (directive.lineIndex + 1)
                      << " — @convert: No .xesy mapping file found in '"
                      << std::filesystem::path(filePath).parent_path().string()
                      << "'. Create one with: xell --gen_xesy\n";
        }
        else
        {
            std::cerr << "[XELL ERROR] Line " << (directive.lineIndex + 1)
                      << " — @convert: Cannot find mapping file '"
                      << directive.mappingPath << "'\n";
        }
        std::exit(1);
    }

    // Load mapping and invert (canonical→custom → custom→canonical)
    auto canonicalToCustom = parseXesyFile(xesyPath);
    auto customToCanonical = invertMapping(canonicalToCustom);

    // Strip @convert line, then replace words
    std::string stripped = stripConvertLine(source, directive.lineIndex);
    return replaceWords(stripped, customToCanonical);
}

// ---- xell --convert: convert dialect file to canonical in-place -------------

static int convertFile(const std::string &filePath, const std::string &explicitXesy = "")
{
    namespace fs = std::filesystem;

    std::string source = readFile(filePath);
    auto directive = parseConvertDirective(source);

    if (!directive.found)
    {
        std::cerr << "Error: No @convert directive found in '" << filePath << "'\n";
        return 1;
    }

    // Determine the mapping path
    std::string xesyPathArg = explicitXesy.empty() ? directive.mappingPath : explicitXesy;
    std::string xesyPath = resolveXesyPath(filePath, xesyPathArg);
    if (xesyPath.empty() || !fs::exists(xesyPath))
    {
        std::cerr << "Error: Cannot find mapping file. ";
        if (xesyPathArg.empty())
            std::cerr << "No .xesy file in directory.\n";
        else
            std::cerr << "'" << xesyPathArg << "' not found.\n";
        return 1;
    }

    // Load and invert
    auto canonicalToCustom = parseXesyFile(xesyPath);
    auto customToCanonical = invertMapping(canonicalToCustom);

    // Strip @convert line, then replace
    std::string stripped = stripConvertLine(source, directive.lineIndex);
    std::string converted = replaceWords(stripped, customToCanonical);

    // Write converted file
    std::ofstream out(filePath, std::ios::trunc);
    if (!out.is_open())
    {
        std::cerr << "Error: Cannot write to '" << filePath << "'\n";
        return 1;
    }
    out << converted;
    out.close();

    // Save revert info (so --revert knows which .xesy to use)
    std::string revertPath = filePath + ".xesy.revert";
    std::ofstream revertFile(revertPath);
    revertFile << fs::absolute(xesyPath).string() << "\n";
    revertFile.close();

    std::cout << "✓ Converted '" << filePath << "' to canonical Xell\n";
    std::cout << "  Mapping: " << xesyPath << "\n";
    std::cout << "  Revert info saved to: " << revertPath << "\n";
    return 0;
}

// ---- xell --revert: restore dialect from canonical file ---------------------

static int revertFile(const std::string &filePath, const std::string &explicitXesy = "")
{
    namespace fs = std::filesystem;

    std::string source = readFile(filePath);

    // Determine mapping path: explicit arg > .xesy.revert sidecar > auto-detect
    std::string xesyPath;
    if (!explicitXesy.empty())
    {
        xesyPath = resolveXesyPath(filePath, explicitXesy);
    }
    else
    {
        // Try .xesy.revert sidecar
        std::string revertInfoPath = filePath + ".xesy.revert";
        if (fs::exists(revertInfoPath))
        {
            std::ifstream rf(revertInfoPath);
            std::getline(rf, xesyPath);
        }
        else
        {
            xesyPath = findXesyInDirectory(filePath);
        }
    }

    if (xesyPath.empty() || !fs::exists(xesyPath))
    {
        std::cerr << "Error: Cannot find mapping file for revert.\n";
        std::cerr << "Specify explicitly: xell --revert " << filePath << " <map.xesy>\n";
        return 1;
    }

    // Load forward map (canonical→custom)
    auto canonicalToCustom = parseXesyFile(xesyPath);

    // Replace canonical → custom
    std::string reverted = replaceWords(source, canonicalToCustom);

    // Compute relative path from code file dir to xesy file
    fs::path codeDir = fs::path(filePath).parent_path();
    if (codeDir.empty())
        codeDir = ".";
    fs::path relXesy = fs::relative(fs::absolute(xesyPath), fs::absolute(codeDir));
    std::string xesyRef = relXesy.string();

    // Prepend @convert directive
    std::string output = "@convert \"" + xesyRef + "\"\n" + reverted;

    // Write back
    std::ofstream out(filePath, std::ios::trunc);
    if (!out.is_open())
    {
        std::cerr << "Error: Cannot write to '" << filePath << "'\n";
        return 1;
    }
    out << output;
    out.close();

    // Remove revert sidecar if it exists
    std::string revertInfoPath = filePath + ".xesy.revert";
    if (fs::exists(revertInfoPath))
        fs::remove(revertInfoPath);

    std::cout << "✓ Reverted '" << filePath << "' to dialect syntax\n";
    std::cout << "  Mapping: " << xesyPath << "\n";
    std::cout << "  @convert directive re-added\n";
    return 0;
}

// ---- xell --gen_xesy: generate template mapping file ------------------------

static int genXesy(const std::string &outputPath = "")
{
    namespace fs = std::filesystem;

    std::string outFile = outputPath.empty() ? "dialect.xesy" : outputPath;

    // Add .xesy extension if not present
    if (fs::path(outFile).extension() != ".xesy")
        outFile += ".xesy";

    // Collect all canonical keyword names from the lexer's keywordMap
    // We replicate the list here since we can't call the static function directly.
    // This stays in sync because gen_xell_grammar.py can also generate .xesy templates.
    std::vector<std::string> keywords = {
        "fn", "give", "if", "elif", "else", "for", "while", "in",
        "break", "continue", "try", "catch", "finally", "incase", "let", "be", "loop",
        "bring", "from", "as", "module", "export", "requires",
        "enum", "struct", "class", "inherits", "immutable",
        "private", "protected", "public", "static",
        "interface", "implements", "abstract", "mixin", "with",
        "yield", "async", "await",
        "true", "false", "none",
        "and", "or", "not",
        "is", "eq", "ne", "gt", "lt", "ge", "le",
        "of"};

    // Common builtins
    std::vector<std::string> builtins = {
        "print", "input", "exit", "len", "type", "str", "num",
        "typeof", "to_int", "to_float", "to_str",
        "push", "pop", "keys", "values", "range", "set", "has",
        "assert", "sort", "reverse", "slice", "join", "split",
        "map", "filter", "reduce", "contains", "index_of",
        "mkdir", "rm", "cp", "mv", "exists", "is_file", "is_dir",
        "ls", "read", "write", "append", "file_size",
        "cwd", "cd", "abspath", "basename", "dirname", "ext",
        "env_get", "env_set", "env_unset", "env_has",
        "run", "run_capture", "pid", "sleep",
        "floor", "ceil", "round", "abs", "mod", "sqrt", "pow",
        "sin", "cos", "tan", "log", "log10", "max", "min",
        "random", "random_int",
        "upper", "lower", "trim", "starts_with", "ends_with",
        "replace", "substr", "char_at"};

    std::ofstream out(outFile);
    if (!out.is_open())
    {
        std::cerr << "Error: Cannot create '" << outFile << "'\n";
        return 1;
    }

    out << "{\n";
    out << "  \"_meta\": {\n";
    out << "    \"dialect_name\": \"My Dialect\",\n";
    out << "    \"author\": \"\",\n";
    out << "    \"xell_version\": \"0.1.0\",\n";
    out << "    \"description\": \"Custom keyword mapping for Xell. Fill in values to map canonical keywords to your dialect.\"\n";
    out << "  },\n\n";

    out << "  \"_comment_keywords\": \"=== Language Keywords ===\",\n";
    for (size_t i = 0; i < keywords.size(); i++)
    {
        out << "  \"" << keywords[i] << "\": \"\"";
        if (i + 1 < keywords.size() || !builtins.empty())
            out << ",";
        out << "\n";
    }

    out << "\n  \"_comment_builtins\": \"=== Built-in Functions ===\",\n";
    for (size_t i = 0; i < builtins.size(); i++)
    {
        out << "  \"" << builtins[i] << "\": \"\"";
        if (i + 1 < builtins.size())
            out << ",";
        out << "\n";
    }

    out << "}\n";
    out.close();

    std::cout << "✓ Generated template mapping file: " << outFile << "\n";
    std::cout << "  Fill in the values with your dialect words, then use:\n";
    std::cout << "    @convert \"" << outFile << "\"    (at the top of your .xel file)\n";
    return 0;
}

// ---- Execute a file ---------------------------------------------------------

static int executeFile(const std::string &path, const std::vector<std::string> &cliArgs = {})
{
    std::string source = readFile(path);

    // Auto-detect @convert and convert dialect → canonical in-memory
    source = applyConvertIfNeeded(source, path);

    xell::Interpreter interpreter;

    try
    {
        xell::Lexer lexer(source);
        auto tokens = lexer.tokenize();
        xell::Parser parser(tokens);
        auto program = parser.parse();
        interpreter.setSourceFile(path);
        interpreter.setIsMainFile(true);
        interpreter.setCliArgs(cliArgs);
        interpreter.run(program);

        // Print captured output (from print() calls)
        for (auto &line : interpreter.output())
            std::cout << line << "\n";

        return 0;
    }
    catch (const xell::XellError &e)
    {
        // Flush any output that was generated before the error
        for (auto &line : interpreter.output())
            std::cout << line << "\n";
        std::cerr << e.what() << "\n";
        return 1;
    }
    catch (const std::exception &e)
    {
        for (auto &line : interpreter.output())
            std::cout << line << "\n";
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

static int lintSource(const std::string &source, const std::string &sourceDir = "",
                      bool emitSymbols = false)
{
    // Shallow check: lex + parse (with recovery) + static analysis — no execution.
    // This is fast and safe for the LSP to call on every keystroke.
    int exitCode = 0;

    // --- Step 1: Lex ---
    std::vector<xell::Token> tokens;
    try
    {
        xell::Lexer lexer(source);
        tokens = lexer.tokenize();
    }
    catch (const xell::XellError &e)
    {
        std::cerr << e.what() << "\n";
        return 1;
    }

    // --- Step 2: Parse with error recovery ---
    std::vector<xell::CollectedParseError> parseErrors;
    xell::Parser parser(tokens);
    auto program = parser.parseLint(parseErrors);

    // Emit parse errors
    for (auto &pe : parseErrors)
    {
        std::cerr << "[XELL ERROR] Line " << pe.line
                  << " \xe2\x80\x94 ParseError: " << pe.message << "\n";
        exitCode = 1;
    }

    // --- Step 3: Static analysis on the partial AST ---
    xell::StaticAnalyzer analyzer;
    if (!sourceDir.empty())
        analyzer.setSourceDir(sourceDir);
    auto diagnostics = analyzer.analyze(program);

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

    // --- Step 4 (optional): Emit symbols as JSON on stdout ---
    if (emitSymbols)
    {
        xell::SymbolCollector collector;
        auto symbols = collector.collect(program);

        // Inject module exports discovered by the static analyzer
        // (these come from resolved imports, not the current file's AST)
        auto &modExports = analyzer.getModuleExports();
        for (auto &[modName, members] : modExports)
        {
            for (auto &member : members)
            {
                // Only add if not already present from the local AST
                bool found = false;
                for (auto &s : symbols)
                    if (s.name == member && s.parentScope == modName)
                    {
                        found = true;
                        break;
                    }
                if (!found)
                {
                    xell::SymbolInfo sym;
                    sym.name = member;
                    sym.kind = xell::SymbolKind::Method; // generic — could be fn or var
                    sym.line = 0;
                    sym.detail = modName + "->" + member;
                    sym.parentScope = modName;
                    symbols.push_back(sym);
                }
            }
        }

        std::cout << xell::SymbolCollector::toJSON(symbols) << "\n";
    }

    return exitCode;
}

static int checkFile(const std::string &path, bool emitSymbols = false)
{
    std::string source = readFile(path);
    // Auto-detect @convert and convert dialect → canonical in-memory
    source = applyConvertIfNeeded(source, path);
    // Always resolve to absolute path first so parent_path is never empty
    std::string absPath = std::filesystem::absolute(path).string();
    std::string dir = std::filesystem::path(absPath).parent_path().string();
    return lintSource(source, dir, emitSymbols);
}

static int checkStdin(bool emitSymbols = false)
{
    // For stdin, try to use CWD as source dir for import resolution
    char *cwd = getcwd(nullptr, 0);
    std::string dir = cwd ? std::string(cwd) : "";
    if (cwd)
        free(cwd);
    return lintSource(readStdin(), dir, emitSymbols);
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

// ---- Launch the Xell Terminal (SDL2 GUI) -----------------------------------

static int launchTerminal()
{
    // Search for the xell-terminal binary in known locations
    std::vector<std::string> searchPaths;

    // Relative to the running xell binary (common in installed setups)
    // When installed, both xell and xell-terminal are in the same dir
    const char *xellHome = std::getenv("XELL_HOME");
    if (xellHome)
    {
        searchPaths.push_back(std::string(xellHome) + "/xell-terminal/build/xell-terminal");
        searchPaths.push_back(std::string(xellHome) + "/xell-terminal/build_release/xell-terminal");
    }

    const char *home = std::getenv("HOME");

    // Check $HOME/.local/bin (local install)
    if (home)
        searchPaths.push_back(std::string(home) + "/.local/bin/xell-terminal");

    // System install locations
    searchPaths.push_back("/usr/local/bin/xell-terminal");
    searchPaths.push_back("/usr/bin/xell-terminal");

    // Development mode — relative to CWD
    searchPaths.push_back("xell-terminal/build/xell-terminal");
    searchPaths.push_back("xell-terminal/build_release/xell-terminal");
    searchPaths.push_back("../xell-terminal/build/xell-terminal");

    std::string terminalPath;
    for (auto &p : searchPaths)
    {
        std::ifstream f(p);
        if (f.good())
        {
            terminalPath = p;
            break;
        }
    }

    if (terminalPath.empty())
    {
        std::cerr << "Error: Cannot find xell-terminal binary.\n";
        std::cerr << "Install it with: ./install.sh\n";
        std::cerr << "Or build it manually: cd xell-terminal && mkdir -p build && cd build && cmake .. && make\n";
        std::cerr << "Requires SDL2: sudo apt install libsdl2-dev libsdl2-ttf-dev\n";
        return 1;
    }

    std::cout << "🖥️  Launching Xell Terminal...\n";
    std::cout << "   Binary: " << terminalPath << "\n";

    // Launch the terminal (replace current process or run as child)
    return std::system(("\"" + terminalPath + "\"").c_str());
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

// ---- xell --make_module: register modules and build metadata ----------------

// Collect all module definitions from a parsed AST
// Module info with full hierarchy for .xell_meta
struct ModuleInfo
{
    std::string name;
    std::vector<std::string> exports;
    std::vector<ModuleInfo> submodules;
};

static void collectModuleDefs(const xell::Stmt *stmt,
                              std::vector<ModuleInfo> &modules)
{
    if (auto *mod = dynamic_cast<const xell::ModuleDef *>(stmt))
    {
        ModuleInfo info;
        info.name = mod->name;

        for (const auto &s : mod->body)
        {
            if (auto *ed = dynamic_cast<const xell::ExportDecl *>(s.get()))
            {
                if (auto *fn = dynamic_cast<const xell::FnDef *>(ed->declaration.get()))
                    info.exports.push_back(fn->name);
                else if (auto *st = dynamic_cast<const xell::StructDef *>(ed->declaration.get()))
                    info.exports.push_back(st->name);
                else if (auto *cls = dynamic_cast<const xell::ClassDef *>(ed->declaration.get()))
                    info.exports.push_back(cls->name);
                else if (auto *a = dynamic_cast<const xell::Assignment *>(ed->declaration.get()))
                    info.exports.push_back(a->name);
                else if (auto *im = dynamic_cast<const xell::ImmutableBinding *>(ed->declaration.get()))
                    info.exports.push_back(im->name);
                else if (auto *nm = dynamic_cast<const xell::ModuleDef *>(ed->declaration.get()))
                {
                    info.exports.push_back(nm->name);
                    // Also collect as submodule
                    std::vector<ModuleInfo> nested;
                    collectModuleDefs(nm, nested);
                    if (!nested.empty())
                        info.submodules.push_back(nested[0]);
                }
            }
            else
            {
                // Check for non-exported nested modules too
                std::vector<ModuleInfo> nested;
                collectModuleDefs(s.get(), nested);
                for (auto &n : nested)
                    info.submodules.push_back(std::move(n));
            }
        }
        modules.push_back(std::move(info));
    }
}

// Helper to write ModuleInfo as JSON with proper submodule hierarchy
static void writeModuleJson(std::ostream &out, const ModuleInfo &mod,
                            const std::string &fileName, const std::string &hash,
                            const std::string &convertXesy,
                            int indent)
{
    std::string pad(indent, ' ');
    out << pad << "\"" << mod.name << "\": {\n";
    out << pad << "  \"file\": \"" << fileName << "\",\n";
    out << pad << "  \"hash\": \"" << hash << "\",\n";
    if (!convertXesy.empty())
        out << pad << "  \"convert\": \"" << convertXesy << "\",\n";
    out << pad << "  \"exports\": [";
    for (size_t i = 0; i < mod.exports.size(); i++)
    {
        if (i)
            out << ", ";
        out << "\"" << mod.exports[i] << "\"";
    }
    out << "],\n";
    out << pad << "  \"submodules\": {";
    if (mod.submodules.empty())
    {
        out << "}";
    }
    else
    {
        out << "\n";
        for (size_t i = 0; i < mod.submodules.size(); i++)
        {
            if (i)
                out << ",\n";
            // Submodules share the same file and hash
            writeModuleJson(out, mod.submodules[i], fileName, hash, convertXesy, indent + 4);
        }
        out << "\n"
            << pad << "  }";
    }
    out << "\n"
        << pad << "}";
}

static int makeModule(int argc, char *argv[])
{
    namespace fs = std::filesystem;

    bool updateMode = false;
    std::vector<std::string> targets;

    for (int i = 2; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "--update")
            updateMode = true;
        else
            targets.push_back(arg);
    }

    if (targets.empty())
    {
        std::cerr << "Usage: xell --make_module [--update] <file.xel|directory> ...\n";
        return 1;
    }

    // Expand targets: directories → all .xel/.xell files in them
    std::vector<std::string> files;
    for (const auto &t : targets)
    {
        fs::path p(t);
        if (fs::is_directory(p))
        {
            for (auto &entry : fs::recursive_directory_iterator(p))
            {
                if (entry.is_regular_file())
                {
                    auto ext = entry.path().extension().string();
                    if (ext == ".xel" || ext == ".xell")
                        files.push_back(entry.path().string());
                }
            }
        }
        else if (fs::exists(p))
        {
            files.push_back(p.string());
        }
        else
        {
            std::cerr << "Warning: '" << t << "' not found, skipping.\n";
        }
    }

    if (files.empty())
    {
        std::cerr << "No .xel or .xell files found.\n";
        return 1;
    }

    // Group files by directory for .xell_meta generation
    std::map<std::string, std::vector<std::string>> dirFiles;
    for (const auto &f : files)
    {
        fs::path dir = fs::path(f).parent_path();
        if (dir.empty())
            dir = ".";
        dirFiles[dir.string()].push_back(f);
    }

    int totalModules = 0;

    for (auto &[dir, dirFileList] : dirFiles)
    {
        // In update mode, load existing .xell_meta to preserve unchanged entries
        // Key: module_name → raw JSON fragment (the whole "name": { ... } block)
        std::map<std::string, std::string> preservedModules;
        std::set<std::string> skippedFiles;
        std::set<std::string> changedFiles;

        if (updateMode)
        {
            fs::path metaPath = fs::path(dir) / ".xell_meta";
            if (fs::exists(metaPath))
            {
                // Read existing .xell_meta and extract module entries
                std::ifstream mf(metaPath);
                std::string existingMeta((std::istreambuf_iterator<char>(mf)),
                                         std::istreambuf_iterator<char>());

                // Extract each module's JSON entry from the existing metadata.
                // Format: "module_name": { "file": "...", ... }
                // We find the "modules" section and extract entries with brace-matching.
                auto modulesPos = existingMeta.find("\"modules\"");
                if (modulesPos != std::string::npos)
                {
                    auto braceStart = existingMeta.find('{', modulesPos + 9);
                    if (braceStart != std::string::npos)
                    {
                        size_t pos = braceStart + 1;
                        while (pos < existingMeta.size())
                        {
                            // Find next quoted module name
                            auto nameStart = existingMeta.find('"', pos);
                            if (nameStart == std::string::npos || existingMeta[nameStart - 1] == '}')
                                break;
                            auto nameEnd = existingMeta.find('"', nameStart + 1);
                            if (nameEnd == std::string::npos)
                                break;
                            std::string modName = existingMeta.substr(nameStart + 1, nameEnd - nameStart - 1);

                            // Skip past the colon to the opening brace
                            auto objStart = existingMeta.find('{', nameEnd);
                            if (objStart == std::string::npos)
                                break;

                            // Brace-match to find the end of this module's JSON object
                            int depth = 1;
                            size_t objEnd = objStart + 1;
                            while (objEnd < existingMeta.size() && depth > 0)
                            {
                                if (existingMeta[objEnd] == '{')
                                    depth++;
                                else if (existingMeta[objEnd] == '}')
                                    depth--;
                                objEnd++;
                            }

                            // Extract the full fragment: "module_name": { ... }
                            std::string fragment = existingMeta.substr(nameStart - 4, objEnd - (nameStart - 4));
                            // Clean up — ensure it starts with proper indentation
                            auto firstQuote = fragment.find('"');
                            if (firstQuote != std::string::npos)
                                fragment = "    " + fragment.substr(firstQuote);
                            preservedModules[modName] = fragment;

                            pos = objEnd;
                        }
                    }
                }
            }
        }

        // Track newly processed modules
        struct ProcessedModule
        {
            std::string jsonFragment;
            std::string name;
        };
        std::vector<ProcessedModule> newModules;

        for (const auto &filePath : dirFileList)
        {
            // Read source
            std::string source = readFile(filePath);
            std::string hash = xell::hash::sha256_string(source);
            std::string relFile = fs::path(filePath).filename().string();

            // Detect & record @convert dialect info before preprocessing
            auto convertDir = dialect::parseConvertDirective(source);
            std::string convertXesy;
            if (convertDir.found)
            {
                std::string resolvedXesy = dialect::resolveXesyPath(filePath, convertDir.mappingPath);
                if (!resolvedXesy.empty() && fs::exists(resolvedXesy))
                {
                    // Store relative path for .xell_meta
                    convertXesy = fs::path(resolvedXesy).filename().string();
                }
                // Preprocess: convert dialect → canonical for parsing
                source = dialect::applyConvertIfNeeded(source, filePath);
            }

            // Check update mode — skip if hash matches
            if (updateMode)
            {
                fs::path cacheDir = fs::path(dir) / "__xelcache__";
                fs::path hashFile = cacheDir / (fs::path(filePath).filename().string() + ".hash");
                if (fs::exists(hashFile))
                {
                    std::ifstream hf(hashFile);
                    std::string storedHash;
                    std::getline(hf, storedHash);
                    if (storedHash == hash)
                    {
                        std::cout << "  ⏭  " << filePath << " (unchanged)\n";
                        skippedFiles.insert(relFile);
                        continue;
                    }
                }
            }

            changedFiles.insert(relFile);

            // Parse
            try
            {
                xell::Lexer lexer(source);
                auto tokens = lexer.tokenize();
                xell::Parser parser(tokens);
                auto program = parser.parse();

                // Collect module definitions with hierarchy
                std::vector<ModuleInfo> modules;
                for (const auto &stmt : program.statements)
                    collectModuleDefs(stmt.get(), modules);

                for (const auto &mod : modules)
                {
                    std::ostringstream fragment;
                    writeModuleJson(fragment, mod, relFile, hash, convertXesy, 4);
                    newModules.push_back({fragment.str(), mod.name});

                    totalModules++;
                    std::cout << "  \xe2\x9c\x93 " << mod.name << " (from " << relFile << ")\n";
                }

                // Write __xelcache__/ hash file
                fs::path cacheDir = fs::path(dir) / "__xelcache__";
                fs::create_directories(cacheDir);

                std::ofstream hashOut(cacheDir / (relFile + ".hash"));
                hashOut << hash;

                std::ofstream xelcOut(cacheDir / (relFile + "c"));
                xelcOut << "# xelc bytecode placeholder — will be replaced when bytecode VM is implemented\n";
                xelcOut << "# source_hash: " << hash << "\n";
            }
            catch (const xell::XellError &e)
            {
                std::cerr << "  ✗ Error in " << filePath << ": " << e.what() << "\n";
            }
        }

        // Build .xell_meta — merge preserved entries with new entries
        std::ostringstream meta;
        meta << "{\n";
        meta << "  \"xell_meta_version\": 1,\n";
        meta << "  \"directory\": \"" << dir << "\",\n";
        meta << "  \"modules\": {\n";

        bool firstModule = true;

        // First: add preserved entries from unchanged files
        // (skip any module whose file was changed — those get fresh entries)
        if (updateMode && !skippedFiles.empty())
        {
            for (auto &[modName, fragment] : preservedModules)
            {
                // Check if this module came from a changed file
                // by looking for "file": "xxx" in the fragment
                bool fromChangedFile = false;
                for (auto &cf : changedFiles)
                {
                    if (fragment.find("\"" + cf + "\"") != std::string::npos)
                    {
                        fromChangedFile = true;
                        break;
                    }
                }
                if (fromChangedFile)
                    continue;

                if (!firstModule)
                    meta << ",\n";
                firstModule = false;
                meta << fragment;
            }
        }

        // Then: add newly processed modules
        for (auto &mod : newModules)
        {
            if (!firstModule)
                meta << ",\n";
            firstModule = false;
            meta << mod.jsonFragment;
        }

        meta << "\n  }\n}\n";

        // Write .xell_meta
        std::string metaPath = (fs::path(dir) / ".xell_meta").string();
        std::ofstream metaFile(metaPath);
        metaFile << meta.str();
        std::cout << "  📝 Wrote " << metaPath << "\n";
    }

    std::cout << "\n✅ Registered " << totalModules << " module(s) from " << files.size() << " file(s).\n";
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

    // --check-symbols — lint + output symbol map as JSON on stdout
    // Used by the IDE to get diagnostics AND autocomplete symbols in one call
    if (arg1 == "--check-symbols")
    {
        if (argc >= 3)
            return checkFile(argv[2], true);
        // No file argument → read from stdin
        return checkStdin(true);
    }

    // --check-string "raw code" — lint code passed directly as a string
    // (no file I/O, used by the IDE for live linting on buffer changes)
    if (arg1 == "--check-string")
    {
        if (argc >= 3)
            return lintSource(std::string(argv[2]));
        std::cerr << "[XELL ERROR] --check-string requires a code string argument\n";
        return 1;
    }

    if (arg1 == "--customize")
    {
        return launchCustomizer();
    }

    if (arg1 == "--terminal" || arg1 == "-t" || arg1 == "--ide")
    {
        return launchTerminal();
    }

    if (arg1 == "--kernel")
    {
        return runKernel();
    }

    if (arg1 == "--make_module")
    {
        return makeModule(argc, argv);
    }

    if (arg1 == "--convert")
    {
        if (argc < 3)
        {
            std::cerr << "Usage: xell --convert <file.xel> [map.xesy]\n";
            return 1;
        }
        std::string xesy = (argc >= 4) ? argv[3] : "";
        return convertFile(argv[2], xesy);
    }

    if (arg1 == "--revert")
    {
        if (argc < 3)
        {
            std::cerr << "Usage: xell --revert <file.xel> [map.xesy]\n";
            return 1;
        }
        std::string xesy = (argc >= 4) ? argv[3] : "";
        return revertFile(argv[2], xesy);
    }

    if (arg1 == "--gen_xesy")
    {
        std::string output = (argc >= 3) ? argv[2] : "";
        return genXesy(output);
    }

    // Default: treat as file to execute
    // Any remaining args after the filename are passed as __args__
    std::vector<std::string> cliArgs;
    for (int i = 2; i < argc; ++i)
        cliArgs.push_back(argv[i]);
    return executeFile(arg1, cliArgs);
}
