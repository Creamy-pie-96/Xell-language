#pragma once

// =============================================================================
// Text Processing builtins — Unix-like text tools implemented in pure C++
// =============================================================================
// head, tail, tail_follow, grep, grep_regex, grep_recursive,
// sed, awk, cut, sort_file, uniq, wc, tee, tr, patch, less, more, xargs
//
// All functions work in both command-style (just prints) and function-style
// (returns a value). The parser desugars `head "file.txt" 10` into
// `head("file.txt", 10)`, so a single BuiltinFn handles both.
// =============================================================================

#include "builtin_registry.hpp"
#include "../lib/errors/error.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <deque>
#include <thread>
#include <chrono>
#include <cctype>
#include <functional>

namespace xell
{

    inline void registerTextProcBuiltins(BuiltinTable &t)
    {
        // =================================================================
        // head(path, n=10) — first N lines of a file → list of strings
        // =================================================================
        t["head"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() < 1 || args.size() > 2)
                throw ArityError("head", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("head() expects a string path", line);
            int n = 10;
            if (args.size() == 2)
            {
                if (!args[1].isInt())
                    throw TypeError("head() expects an integer for line count", line);
                n = (int)args[1].asInt();
            }
            std::ifstream ifs(args[0].asString());
            if (!ifs)
                throw FileNotFoundError(args[0].asString(), line);
            std::vector<XObject> result;
            std::string l;
            for (int i = 0; i < n && std::getline(ifs, l); i++)
                result.push_back(XObject::makeString(l));
            return XObject::makeList(std::move(result));
        };

        // =================================================================
        // tail(path, n=10) — last N lines of a file → list of strings
        // =================================================================
        t["tail"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() < 1 || args.size() > 2)
                throw ArityError("tail", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("tail() expects a string path", line);
            int n = 10;
            if (args.size() == 2)
            {
                if (!args[1].isInt())
                    throw TypeError("tail() expects an integer for line count", line);
                n = (int)args[1].asInt();
            }
            std::ifstream ifs(args[0].asString());
            if (!ifs)
                throw FileNotFoundError(args[0].asString(), line);
            std::deque<std::string> buf;
            std::string l;
            while (std::getline(ifs, l))
            {
                buf.push_back(l);
                if ((int)buf.size() > n)
                    buf.pop_front();
            }
            std::vector<XObject> result;
            for (auto &s : buf)
                result.push_back(XObject::makeString(s));
            return XObject::makeList(std::move(result));
        };

        // =================================================================
        // tail_follow(path, n=10, duration_ms=5000) — follow file growth
        // Returns lines added during the monitoring window
        // =================================================================
        t["tail_follow"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() < 1 || args.size() > 3)
                throw ArityError("tail_follow", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("tail_follow() expects a string path", line);
            int n = 10;
            int durationMs = 5000;
            if (args.size() >= 2 && args[1].isInt())
                n = (int)args[1].asInt();
            if (args.size() >= 3 && args[2].isInt())
                durationMs = (int)args[2].asInt();

            std::string path = args[0].asString();
            // Get initial tail
            {
                std::ifstream ifs(path);
                if (!ifs)
                    throw FileNotFoundError(path, line);
            }
            // Get current file size
            auto startSize = std::filesystem::file_size(path);

            // Wait for the specified duration, polling for new content
            auto start = std::chrono::steady_clock::now();
            std::vector<XObject> newLines;
            while (std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - start)
                       .count() < durationMs)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                try
                {
                    auto curSize = std::filesystem::file_size(path);
                    if (curSize > startSize)
                    {
                        std::ifstream ifs(path);
                        ifs.seekg(startSize);
                        std::string l;
                        while (std::getline(ifs, l))
                            newLines.push_back(XObject::makeString(l));
                        startSize = curSize;
                    }
                }
                catch (...)
                {
                    break;
                }
            }
            return XObject::makeList(std::move(newLines));
        };

        // =================================================================
        // grep(pattern, path) — search file for lines containing pattern
        // Returns list of matching lines
        // =================================================================
        t["grep"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("grep", 2, (int)args.size(), line);
            if (!args[0].isString() || !args[1].isString())
                throw TypeError("grep() expects two strings (pattern, path)", line);
            std::string pattern = args[0].asString();
            std::ifstream ifs(args[1].asString());
            if (!ifs)
                throw FileNotFoundError(args[1].asString(), line);
            std::vector<XObject> result;
            std::string l;
            while (std::getline(ifs, l))
            {
                if (l.find(pattern) != std::string::npos)
                    result.push_back(XObject::makeString(l));
            }
            return XObject::makeList(std::move(result));
        };

        // =================================================================
        // grep_regex(pattern, path) — search using regex
        // Returns list of matching lines
        // =================================================================
        t["grep_regex"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("grep_regex", 2, (int)args.size(), line);
            if (!args[0].isString() || !args[1].isString())
                throw TypeError("grep_regex() expects two strings (pattern, path)", line);
            std::regex re;
            try
            {
                re = std::regex(args[0].asString());
            }
            catch (const std::regex_error &e)
            {
                throw RuntimeError("grep_regex(): invalid regex: " + std::string(e.what()), line);
            }
            std::ifstream ifs(args[1].asString());
            if (!ifs)
                throw FileNotFoundError(args[1].asString(), line);
            std::vector<XObject> result;
            std::string l;
            while (std::getline(ifs, l))
            {
                if (std::regex_search(l, re))
                    result.push_back(XObject::makeString(l));
            }
            return XObject::makeList(std::move(result));
        };

        // =================================================================
        // grep_recursive(pattern, dir) — search recursively in all files
        // Returns list of maps: {file, line_number, text}
        // =================================================================
        t["grep_recursive"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("grep_recursive", 2, (int)args.size(), line);
            if (!args[0].isString() || !args[1].isString())
                throw TypeError("grep_recursive() expects two strings (pattern, dir)", line);
            std::string pattern = args[0].asString();
            std::string dir = args[1].asString();
            if (!std::filesystem::exists(dir))
                throw FileNotFoundError(dir, line);
            if (!std::filesystem::is_directory(dir))
                throw IOError("grep_recursive(): not a directory: " + dir, line);

            std::vector<XObject> results;
            for (auto &entry : std::filesystem::recursive_directory_iterator(dir))
            {
                if (!entry.is_regular_file())
                    continue;
                // Skip binary files (heuristic: check first 512 bytes for null)
                std::ifstream check(entry.path(), std::ios::binary);
                char buf[512];
                check.read(buf, sizeof(buf));
                auto bytesRead = check.gcount();
                bool binary = false;
                for (int i = 0; i < bytesRead; i++)
                {
                    if (buf[i] == '\0')
                    {
                        binary = true;
                        break;
                    }
                }
                if (binary)
                    continue;

                std::ifstream ifs(entry.path());
                std::string l;
                int lineNum = 0;
                while (std::getline(ifs, l))
                {
                    lineNum++;
                    if (l.find(pattern) != std::string::npos)
                    {
                        XMap m;
                        m.set("file", XObject::makeString(entry.path().string()));
                        m.set("line_number", XObject::makeInt(lineNum));
                        m.set("text", XObject::makeString(l));
                        results.push_back(XObject::makeMap(std::move(m)));
                    }
                }
            }
            return XObject::makeList(std::move(results));
        };

        // =================================================================
        // sed(pattern, replacement, path) — search and replace in file
        // Returns the modified content as string (does NOT modify file)
        // =================================================================
        t["sed"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() < 3 || args.size() > 4)
                throw ArityError("sed", 3, (int)args.size(), line);
            if (!args[0].isString() || !args[1].isString() || !args[2].isString())
                throw TypeError("sed() expects strings (pattern, replacement, path)", line);
            bool inPlace = false;
            if (args.size() == 4 && args[3].isBool())
                inPlace = args[3].asBool();

            std::regex re;
            try
            {
                re = std::regex(args[0].asString());
            }
            catch (const std::regex_error &e)
            {
                throw RuntimeError("sed(): invalid regex: " + std::string(e.what()), line);
            }

            std::ifstream ifs(args[2].asString());
            if (!ifs)
                throw FileNotFoundError(args[2].asString(), line);
            std::ostringstream oss;
            std::string l;
            bool first = true;
            while (std::getline(ifs, l))
            {
                if (!first)
                    oss << "\n";
                first = false;
                oss << std::regex_replace(l, re, args[1].asString());
            }
            ifs.close();
            std::string result = oss.str();

            if (inPlace)
            {
                std::ofstream ofs(args[2].asString());
                if (!ofs)
                    throw IOError("sed(): cannot write to " + args[2].asString(), line);
                ofs << result;
            }
            return XObject::makeString(result);
        };

        // =================================================================
        // awk(program, path) — simplified awk: split lines by whitespace,
        // evaluate field references like $1, $2, $0 (whole line), $NF (last)
        // Returns list of result strings
        // =================================================================
        t["awk"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() < 2 || args.size() > 3)
                throw ArityError("awk", 2, (int)args.size(), line);
            if (!args[0].isString() || !args[1].isString())
                throw TypeError("awk() expects strings (program, path)", line);
            std::string program = args[0].asString();
            std::string delim = " ";
            if (args.size() == 3 && args[2].isString())
                delim = args[2].asString();

            std::ifstream ifs(args[1].asString());
            if (!ifs)
                throw FileNotFoundError(args[1].asString(), line);

            // Parse field references from program: $0, $1, $2, ..., $NF
            // Simple format: space-separated field references
            // e.g. "$1" or "$1 $3" or "$0"
            std::vector<int> fields; // -1 = $0 (whole line), -2 = $NF (last)
            std::istringstream pss(program);
            std::string tok;
            while (pss >> tok)
            {
                if (tok[0] == '$')
                {
                    std::string num = tok.substr(1);
                    if (num == "0")
                        fields.push_back(-1);
                    else if (num == "NF")
                        fields.push_back(-2);
                    else
                    {
                        try
                        {
                            fields.push_back(std::stoi(num));
                        }
                        catch (...)
                        {
                            throw RuntimeError("awk(): invalid field reference: " + tok, line);
                        }
                    }
                }
                else
                {
                    throw RuntimeError("awk(): expected field reference ($N), got: " + tok, line);
                }
            }

            if (fields.empty())
                throw RuntimeError("awk(): no field references in program", line);

            // Split helper
            auto splitLine = [&](const std::string &s) -> std::vector<std::string>
            {
                std::vector<std::string> parts;
                if (delim == " ")
                {
                    // Split by whitespace (awk default)
                    std::istringstream iss(s);
                    std::string w;
                    while (iss >> w)
                        parts.push_back(w);
                }
                else
                {
                    size_t start = 0;
                    while (true)
                    {
                        size_t pos = s.find(delim, start);
                        if (pos == std::string::npos)
                        {
                            parts.push_back(s.substr(start));
                            break;
                        }
                        parts.push_back(s.substr(start, pos - start));
                        start = pos + delim.size();
                    }
                }
                return parts;
            };

            std::vector<XObject> results;
            std::string l;
            while (std::getline(ifs, l))
            {
                auto parts = splitLine(l);
                std::ostringstream out;
                for (size_t i = 0; i < fields.size(); i++)
                {
                    if (i > 0)
                        out << " ";
                    int f = fields[i];
                    if (f == -1)
                        out << l; // $0
                    else if (f == -2)
                        out << (parts.empty() ? "" : parts.back()); // $NF
                    else if (f >= 1 && f <= (int)parts.size())
                        out << parts[f - 1];
                    // out of range fields are silently empty (awk behavior)
                }
                results.push_back(XObject::makeString(out.str()));
            }
            return XObject::makeList(std::move(results));
        };

        // =================================================================
        // cut(path, delim, fields) — extract columns from each line
        // fields is a list of 1-based column indices
        // Returns list of extracted strings
        // =================================================================
        t["cut"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 3)
                throw ArityError("cut", 3, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("cut() expects a string path", line);
            if (!args[1].isString())
                throw TypeError("cut() expects a string delimiter", line);
            if (!args[2].isList())
                throw TypeError("cut() expects a list of field indices", line);

            std::string delim = args[1].asString();
            auto &fieldList = args[2].asList();
            std::vector<int> fieldIndices;
            for (auto &f : fieldList)
            {
                if (!f.isInt())
                    throw TypeError("cut() field indices must be integers", line);
                fieldIndices.push_back((int)f.asInt());
            }

            std::ifstream ifs(args[0].asString());
            if (!ifs)
                throw FileNotFoundError(args[0].asString(), line);

            std::vector<XObject> results;
            std::string l;
            while (std::getline(ifs, l))
            {
                // Split by delimiter
                std::vector<std::string> parts;
                size_t start = 0;
                while (true)
                {
                    size_t pos = l.find(delim, start);
                    if (pos == std::string::npos)
                    {
                        parts.push_back(l.substr(start));
                        break;
                    }
                    parts.push_back(l.substr(start, pos - start));
                    start = pos + delim.size();
                }

                std::ostringstream out;
                for (size_t i = 0; i < fieldIndices.size(); i++)
                {
                    if (i > 0)
                        out << delim;
                    int idx = fieldIndices[i];
                    if (idx >= 1 && idx <= (int)parts.size())
                        out << parts[idx - 1];
                }
                results.push_back(XObject::makeString(out.str()));
            }
            return XObject::makeList(std::move(results));
        };

        // =================================================================
        // sort_file(path, reverse=false) — sort lines alphabetically
        // Returns sorted list of strings
        // =================================================================
        t["sort_file"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() < 1 || args.size() > 2)
                throw ArityError("sort_file", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("sort_file() expects a string path", line);
            bool reverse = false;
            if (args.size() == 2 && args[1].isBool())
                reverse = args[1].asBool();

            std::ifstream ifs(args[0].asString());
            if (!ifs)
                throw FileNotFoundError(args[0].asString(), line);
            std::vector<std::string> lines;
            std::string l;
            while (std::getline(ifs, l))
                lines.push_back(l);

            if (reverse)
                std::sort(lines.begin(), lines.end(), std::greater<std::string>());
            else
                std::sort(lines.begin(), lines.end());

            std::vector<XObject> result;
            for (auto &s : lines)
                result.push_back(XObject::makeString(s));
            return XObject::makeList(std::move(result));
        };

        // =================================================================
        // uniq(path) — remove consecutive duplicate lines
        // Returns list of unique lines
        // =================================================================
        t["uniq"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("uniq", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("uniq() expects a string path", line);
            std::ifstream ifs(args[0].asString());
            if (!ifs)
                throw FileNotFoundError(args[0].asString(), line);
            std::vector<XObject> result;
            std::string l, prev;
            bool hasPrev = false;
            while (std::getline(ifs, l))
            {
                if (!hasPrev || l != prev)
                {
                    result.push_back(XObject::makeString(l));
                    prev = l;
                    hasPrev = true;
                }
            }
            return XObject::makeList(std::move(result));
        };

        // =================================================================
        // wc(path) — count lines, words, bytes
        // Returns map {lines, words, bytes}
        // =================================================================
        t["wc"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("wc", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("wc() expects a string path", line);
            std::ifstream ifs(args[0].asString());
            if (!ifs)
                throw FileNotFoundError(args[0].asString(), line);
            int64_t lineCount = 0, wordCount = 0, byteCount = 0;
            std::string l;
            while (std::getline(ifs, l))
            {
                lineCount++;
                byteCount += (int64_t)l.size() + 1; // +1 for newline
                std::istringstream iss(l);
                std::string w;
                while (iss >> w)
                    wordCount++;
            }
            XMap result;
            result.set("lines", XObject::makeInt(lineCount));
            result.set("words", XObject::makeInt(wordCount));
            result.set("bytes", XObject::makeInt(byteCount));
            return XObject::makeMap(std::move(result));
        };

        // =================================================================
        // tee(content, path) — write content to file AND return it
        // =================================================================
        t["tee"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("tee", 2, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("tee() expects a string content", line);
            if (!args[1].isString())
                throw TypeError("tee() expects a string path", line);
            std::string content = args[0].asString();
            std::ofstream ofs(args[1].asString());
            if (!ofs)
                throw IOError("tee(): cannot write to " + args[1].asString(), line);
            ofs << content;
            return XObject::makeString(content);
        };

        // =================================================================
        // tr(from, to, input) — translate characters
        // Maps each char in 'from' to the corresponding char in 'to'
        // Returns translated string
        // =================================================================
        t["tr"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 3)
                throw ArityError("tr", 3, (int)args.size(), line);
            if (!args[0].isString() || !args[1].isString() || !args[2].isString())
                throw TypeError("tr() expects three strings (from, to, input)", line);
            std::string from = args[0].asString();
            std::string to = args[1].asString();
            std::string input = args[2].asString();

            // Expand ranges: a-z → abcde...z
            auto expand = [&](const std::string &s) -> std::string
            {
                std::string result;
                for (size_t i = 0; i < s.size(); i++)
                {
                    if (i + 2 < s.size() && s[i + 1] == '-')
                    {
                        char start = s[i], end = s[i + 2];
                        if (start <= end)
                        {
                            for (char c = start; c <= end; c++)
                                result += c;
                        }
                        else
                        {
                            for (char c = start; c >= end; c--)
                                result += c;
                        }
                        i += 2;
                    }
                    else
                    {
                        result += s[i];
                    }
                }
                return result;
            };

            std::string expandedFrom = expand(from);
            std::string expandedTo = expand(to);

            // Build translation map
            std::string output = input;
            for (size_t i = 0; i < output.size(); i++)
            {
                size_t pos = expandedFrom.find(output[i]);
                if (pos != std::string::npos && pos < expandedTo.size())
                    output[i] = expandedTo[pos];
            }
            return XObject::makeString(output);
        };

        // =================================================================
        // patch(file, patchfile) — apply a simple unified diff patch
        // Supports lines starting with + (add), - (remove), ~ (change)
        // from file_diff() output format
        // =================================================================
        t["patch"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("patch", 2, (int)args.size(), line);
            if (!args[0].isString() || !args[1].isString())
                throw TypeError("patch() expects two string paths (file, patchfile)", line);

            // Read original file
            std::ifstream ifs(args[0].asString());
            if (!ifs)
                throw FileNotFoundError(args[0].asString(), line);
            std::vector<std::string> lines;
            std::string l;
            while (std::getline(ifs, l))
                lines.push_back(l);
            ifs.close();

            // Read patch file (file_diff format: +N: line, -N: line, ~N: -old +new)
            std::ifstream pfs(args[1].asString());
            if (!pfs)
                throw FileNotFoundError(args[1].asString(), line);

            // Collect patch operations
            struct PatchOp
            {
                char type;       // '+', '-', '~'
                int lineNum;     // 1-based
                std::string add; // for + and ~
            };
            std::vector<PatchOp> ops;
            std::string pl;
            while (std::getline(pfs, pl))
            {
                if (pl.empty())
                    continue;
                char type = pl[0];
                if (type != '+' && type != '-' && type != '~')
                    continue;
                // Extract line number
                size_t colonPos = pl.find(": ", 1);
                if (colonPos == std::string::npos)
                    continue;
                int lnum;
                try
                {
                    lnum = std::stoi(pl.substr(1, colonPos - 1));
                }
                catch (...)
                {
                    continue;
                }
                std::string content = pl.substr(colonPos + 2);
                PatchOp op;
                op.type = type;
                op.lineNum = lnum;
                if (type == '~')
                {
                    // Format: ~N: -old +new
                    size_t plusPos = content.find(" +");
                    if (plusPos != std::string::npos)
                        op.add = content.substr(plusPos + 2);
                }
                else if (type == '+')
                {
                    op.add = content;
                }
                ops.push_back(op);
            }

            // Apply operations (process in reverse line order for stability)
            std::sort(ops.begin(), ops.end(), [](const PatchOp &a, const PatchOp &b)
                      { return a.lineNum > b.lineNum; });

            for (auto &op : ops)
            {
                int idx = op.lineNum - 1;
                if (op.type == '-' && idx >= 0 && idx < (int)lines.size())
                {
                    lines.erase(lines.begin() + idx);
                }
                else if (op.type == '+')
                {
                    if (idx <= (int)lines.size())
                        lines.insert(lines.begin() + idx, op.add);
                }
                else if (op.type == '~' && idx >= 0 && idx < (int)lines.size())
                {
                    lines[idx] = op.add;
                }
            }

            // Write back
            std::ofstream ofs(args[0].asString());
            if (!ofs)
                throw IOError("patch(): cannot write to " + args[0].asString(), line);
            for (size_t i = 0; i < lines.size(); i++)
            {
                if (i > 0)
                    ofs << "\n";
                ofs << lines[i];
            }

            return XObject::makeInt((int64_t)ops.size());
        };

        // =================================================================
        // less(path, page_size=20) — paginated file view
        // Returns list of pages (each page is a list of lines)
        // =================================================================
        t["less"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() < 1 || args.size() > 2)
                throw ArityError("less", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("less() expects a string path", line);
            int pageSize = 20;
            if (args.size() == 2 && args[1].isInt())
                pageSize = (int)args[1].asInt();
            if (pageSize < 1)
                pageSize = 1;

            std::ifstream ifs(args[0].asString());
            if (!ifs)
                throw FileNotFoundError(args[0].asString(), line);

            std::vector<XObject> pages;
            std::vector<XObject> currentPage;
            std::string l;
            while (std::getline(ifs, l))
            {
                currentPage.push_back(XObject::makeString(l));
                if ((int)currentPage.size() >= pageSize)
                {
                    pages.push_back(XObject::makeList(std::move(currentPage)));
                    currentPage.clear();
                }
            }
            if (!currentPage.empty())
                pages.push_back(XObject::makeList(std::move(currentPage)));
            return XObject::makeList(std::move(pages));
        };

        // =================================================================
        // more(path, page_size=20) — simpler paginated view (alias for less)
        // Returns same structure as less
        // =================================================================
        t["more"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() < 1 || args.size() > 2)
                throw ArityError("more", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("more() expects a string path", line);
            int pageSize = 20;
            if (args.size() == 2 && args[1].isInt())
                pageSize = (int)args[1].asInt();
            if (pageSize < 1)
                pageSize = 1;

            std::ifstream ifs(args[0].asString());
            if (!ifs)
                throw FileNotFoundError(args[0].asString(), line);

            std::vector<XObject> pages;
            std::vector<XObject> currentPage;
            std::string l;
            while (std::getline(ifs, l))
            {
                currentPage.push_back(XObject::makeString(l));
                if ((int)currentPage.size() >= pageSize)
                {
                    pages.push_back(XObject::makeList(std::move(currentPage)));
                    currentPage.clear();
                }
            }
            if (!currentPage.empty())
                pages.push_back(XObject::makeList(std::move(currentPage)));
            return XObject::makeList(std::move(pages));
        };

        // =================================================================
        // xargs(cmd_template, input_list) — build commands from list items
        // cmd_template is a string with {} placeholder for each item
        // Returns list of {command, exit_code, stdout} maps
        // =================================================================
        t["xargs"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("xargs", 2, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("xargs() expects a string command template", line);
            if (!args[1].isList())
                throw TypeError("xargs() expects a list of items", line);
            std::string tmpl = args[0].asString();
            auto &items = args[1].asList();
            std::vector<XObject> results;
            for (auto &item : items)
            {
                std::string val = item.isString() ? item.asString() : item.toString();
                // Replace {} with the item value
                std::string cmd = tmpl;
                size_t pos = cmd.find("{}");
                if (pos != std::string::npos)
                    cmd.replace(pos, 2, val);
                else
                    cmd += " " + val;

                // Execute using popen
                FILE *fp = popen(cmd.c_str(), "r");
                std::string output;
                if (fp)
                {
                    char buf[1024];
                    while (fgets(buf, sizeof(buf), fp))
                        output += buf;
                    int code = pclose(fp);
                    XMap m;
                    m.set("command", XObject::makeString(cmd));
                    m.set("exit_code", XObject::makeInt(WEXITSTATUS(code)));
                    m.set("stdout", XObject::makeString(output));
                    results.push_back(XObject::makeMap(std::move(m)));
                }
            }
            return XObject::makeList(std::move(results));
        };
    }

} // namespace xell
