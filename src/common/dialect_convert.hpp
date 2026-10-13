#pragma once

// =============================================================================
// Xell @convert Dialect System — shared preprocessing utilities
// =============================================================================
//
// Used by:
//   - main.cpp (executeFile, checkFile, convertFile, revertFile, makeModule)
//   - interpreter.cpp (executeModuleFile, bringFromFile)
//
// All functions live in the `dialect` namespace to avoid collisions.
//
// =============================================================================

#include <string>
#include <map>
#include <vector>
#include <sstream>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <filesystem>
#include "json.hpp"

namespace dialect
{

    // ── Structs ──────────────────────────────────────────────

    struct ConvertDirective
    {
        bool found = false;
        std::string mappingPath; // empty if @convert with no path
        int lineIndex = -1;      // 0-based line index of the @convert line
    };

    // ── .xesy File Parsing ───────────────────────────────────

    /// Parse a .xesy mapping file using standalone xell::json::parse.
    /// Skips the special "_meta" key and non-string values. Returns canonical→custom map.
    inline std::map<std::string, std::string> parseXesyFile(const std::string &path)
    {
        std::map<std::string, std::string> mapping;
        std::ifstream f(path);
        if (!f.is_open())
        {
            std::cerr << "Error: Cannot open mapping file '" << path << "'\n";
            return mapping; // return empty instead of exit — caller decides
        }

        std::string content((std::istreambuf_iterator<char>(f)),
                            std::istreambuf_iterator<char>());

        try
        {
            xell::json::Value val = xell::json::parse(content);
            if (val.isObject())
            {
                for (const auto &[key, v] : val.asObject())
                {
                    if (key != "_meta" && v.isString() && !key.empty() && !v.asString().empty())
                    {
                        mapping[key] = v.asString();
                    }
                }
            }
        }
        catch (const xell::json::ParseError &e)
        {
            std::cerr << "Error: Failed to parse mapping file '" << path << "': " << e.what() << "\n";
        }

        return mapping;
    }

    // ── Mapping Utilities ────────────────────────────────────

    /// Invert: canonical→custom becomes custom→canonical
    inline std::map<std::string, std::string> invertMapping(
        const std::map<std::string, std::string> &mapping)
    {
        std::map<std::string, std::string> inverted;
        for (auto &[canonical, custom] : mapping)
        {
            if (!custom.empty())
                inverted[custom] = canonical;
        }
        return inverted;
    }

    /// Replace all mapped words in source (whole-word, skips strings & comments).
    inline std::string replaceWords(const std::string &source,
                                    const std::map<std::string, std::string> &wordMap)
    {
        if (wordMap.empty())
            return source;

        std::string result;
        result.reserve(source.size());

        bool inString = false;
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

            // Skip line comments (# to end of line)
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

            // Word boundary — identifier
            if (std::isalpha(static_cast<unsigned char>(source[i])) || source[i] == '_')
            {
                std::string word;
                while (i < source.size() &&
                       (std::isalnum(static_cast<unsigned char>(source[i])) || source[i] == '_'))
                {
                    word += source[i];
                    i++;
                }

                auto it = wordMap.find(word);
                if (it != wordMap.end())
                    result += it->second;
                else
                    result += word;
                continue;
            }

            result += source[i];
            i++;
        }

        return result;
    }

    // ── @convert Directive Detection ─────────────────────────

    /// Parse @convert from first 5 non-blank lines.
    inline ConvertDirective parseConvertDirective(const std::string &source)
    {
        ConvertDirective result;
        std::istringstream stream(source);
        std::string line;
        int lineNum = 0;
        int nonBlankCount = 0;

        while (std::getline(stream, line) && nonBlankCount < 5)
        {
            size_t start = line.find_first_not_of(" \t\r");
            if (start == std::string::npos)
            {
                lineNum++;
                continue;
            }
            nonBlankCount++;

            std::string trimmed = line.substr(start);
            if (trimmed.rfind("@convert", 0) == 0)
            {
                result.found = true;
                result.lineIndex = lineNum;

                auto quoteStart = trimmed.find('"', 8);
                if (quoteStart != std::string::npos)
                {
                    auto quoteEnd = trimmed.find('"', quoteStart + 1);
                    if (quoteEnd != std::string::npos)
                    {
                        result.mappingPath =
                            trimmed.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
                    }
                }
                return result;
            }

            lineNum++;
        }
        return result;
    }

    /// Remove the @convert line from source text.
    inline std::string stripConvertLine(const std::string &source, int lineIndex)
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

    // ── .xesy Path Resolution ────────────────────────────────

    /// Find a .xesy file in the same directory as the given file.
    inline std::string findXesyInDirectory(const std::string &filePath)
    {
        namespace fs = std::filesystem;
        fs::path dir = fs::path(filePath).parent_path();
        if (dir.empty())
            dir = ".";

        try
        {
            for (auto &entry : fs::directory_iterator(dir))
            {
                if (entry.is_regular_file() && entry.path().extension() == ".xesy")
                    return entry.path().string();
            }
        }
        catch (...)
        {
        }
        return "";
    }

    /// Resolve mapping path relative to the code file's directory.
    inline std::string resolveXesyPath(const std::string &codeFilePath,
                                       const std::string &mappingPath)
    {
        if (mappingPath.empty())
            return findXesyInDirectory(codeFilePath);

        namespace fs = std::filesystem;
        fs::path mp(mappingPath);
        if (mp.is_absolute())
            return mp.string();

        fs::path codeDir = fs::path(codeFilePath).parent_path();
        if (codeDir.empty())
            codeDir = ".";
        return (codeDir / mp).string();
    }

    // ── Top-Level API ────────────────────────────────────────

    /// Apply @convert: convert dialect source to canonical in-memory.
    /// Returns the canonical source (with @convert line stripped).
    /// If no @convert found, returns source unchanged.
    /// If the .xesy file is not found, prints an error and returns source unchanged
    /// (the caller — lexer/parser — will emit its own errors on unknown tokens).
    inline std::string applyConvertIfNeeded(const std::string &source,
                                            const std::string &filePath)
    {
        auto directive = parseConvertDirective(source);
        if (!directive.found)
            return source;

        std::string xesyPath = resolveXesyPath(filePath, directive.mappingPath);
        if (xesyPath.empty() || !std::filesystem::exists(xesyPath))
        {
            if (directive.mappingPath.empty())
            {
                std::cerr << "[XELL WARNING] Line " << (directive.lineIndex + 1)
                          << " — @convert: No .xesy mapping file found in '"
                          << std::filesystem::path(filePath).parent_path().string()
                          << "'. Create one with: xell --gen_xesy\n";
            }
            else
            {
                std::cerr << "[XELL WARNING] Line " << (directive.lineIndex + 1)
                          << " — @convert: Cannot find mapping file '"
                          << directive.mappingPath << "'\n";
            }
            // Return source unchanged — let downstream produce parse errors
            return source;
        }

        auto canonicalToCustom = parseXesyFile(xesyPath);
        auto customToCanonical = invertMapping(canonicalToCustom);

        std::string stripped = stripConvertLine(source, directive.lineIndex);
        return replaceWords(stripped, customToCanonical);
    }

} // namespace dialect
