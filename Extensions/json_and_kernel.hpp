#pragma once

#include "perser.hpp"

// ═══════════════════════════════════════════════════════════
// ──── Minimal JSON Helpers (for kernel mode) ───────────────
// ═══════════════════════════════════════════════════════════

// Minimal JSON value: string→string map parser (for kernel protocol)
inline std::unordered_map<std::string, std::string> parse_json_object(const std::string &json)
{
    std::unordered_map<std::string, std::string> result;
    size_t i = json.find('{');
    if (i == std::string::npos)
        return result;
    i++;

    auto skipWS = [&]()
    { while (i < json.size() && std::isspace(json[i])) i++; };
    auto readString = [&]() -> std::string
    {
        skipWS();
        if (i >= json.size() || json[i] != '"')
            return "";
        i++; // skip opening "
        std::string s;
        while (i < json.size() && json[i] != '"')
        {
            if (json[i] == '\\' && i + 1 < json.size())
            {
                i++;
                if (json[i] == 'n')
                    s += '\n';
                else if (json[i] == 't')
                    s += '\t';
                else if (json[i] == '\\')
                    s += '\\';
                else if (json[i] == '"')
                    s += '"';
                else if (json[i] == '/')
                    s += '/';
                else
                    s += json[i];
            }
            else
                s += json[i];
            i++;
        }
        if (i < json.size())
            i++; // skip closing "
        return s;
    };

    while (i < json.size())
    {
        skipWS();
        if (json[i] == '}')
            break;
        if (json[i] == ',')
        {
            i++;
            continue;
        }
        std::string key = readString();
        skipWS();
        if (i < json.size() && json[i] == ':')
            i++;
        skipWS();
        // Value can be string, number, or null
        if (i < json.size() && json[i] == '"')
        {
            result[key] = readString();
        }
        else
        {
            // Read non-string value as raw text until , or }
            std::string val;
            while (i < json.size() && json[i] != ',' && json[i] != '}')
                val += json[i++];
            // trim
            while (!val.empty() && std::isspace(val.back()))
                val.pop_back();
            result[key] = val;
        }
    }
    return result;
}

inline std::string json_escape(const std::string &s)
{
    std::string out;
    out.reserve(s.size() + 10);
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
}

inline std::string make_json_response(const std::string &cellId, const std::string &status,
                                      const std::string &stdoutStr, const std::string &stderrStr,
                                      const std::string &result, int execCount)
{
    return "{\"cell_id\":\"" + json_escape(cellId) + "\","
                                                     "\"status\":\"" +
           json_escape(status) + "\","
                                 "\"stdout\":\"" +
           json_escape(stdoutStr) + "\","
                                    "\"stderr\":\"" +
           json_escape(stderrStr) + "\","
                                    "\"result\":\"" +
           json_escape(result) + "\","
                                 "\"execution_count\":" +
           std::to_string(execCount) + "}";
}

// ═══════════════════════════════════════════════════════════
// ──── Kernel Mode ──────────────────────────────────────────
// ═══════════════════════════════════════════════════════════

// Global kernel state for the input() override
// These are `extern`-declared in scriptit_builtins.hpp so input() can access them
bool _scriptit_kernel_mode = false;
std::streambuf *_scriptit_kernel_real_stdout = nullptr;
std::string _scriptit_kernel_cell_id;

namespace kernel_io
{
    static std::streambuf *realStdoutBuf = nullptr; // original cout buffer
    static std::string currentCellId;               // currently executing cell
    static bool inKernelMode = false;
}

void runKernel()
{
    Scope globalScope;
    globalScope.define("PI", var(3.14159265));
    globalScope.define("e", var(2.7182818));
    int executionCount = 0;

    kernel_io::inKernelMode = true;
    kernel_io::realStdoutBuf = std::cout.rdbuf(); // save the real stdout

    // Set the extern globals so input() builtin can access them
    _scriptit_kernel_mode = true;
    _scriptit_kernel_real_stdout = std::cout.rdbuf();

    // Signal ready
    std::cout << "{\"status\":\"kernel_ready\",\"version\":\"2.0\"}" << std::endl;
    std::cout.flush();

    std::string line;
    while (std::getline(std::cin, line))
    {
        if (line.empty())
            continue;

        auto cmd = parse_json_object(line);
        std::string action = cmd["action"];

        if (action == "shutdown")
        {
            std::cout << "{\"status\":\"shutdown_ok\"}" << std::endl;
            std::cout.flush();
            break;
        }

        if (action == "reset")
        {
            globalScope.clear();
            globalScope.define("PI", var(3.14159265));
            globalScope.define("e", var(2.7182818));
            executionCount = 0;
            std::cout << "{\"status\":\"reset_ok\"}" << std::endl;
            std::cout.flush();
            continue;
        }

        if (action == "execute")
        {
            std::string cellId = cmd["cell_id"];
            std::string code = cmd["code"];
            executionCount++;

            kernel_io::currentCellId = cellId;
            _scriptit_kernel_cell_id = cellId;

            // Capture stdout
            std::ostringstream capturedOut;
            std::streambuf *oldBuf = std::cout.rdbuf(capturedOut.rdbuf());

            std::string errorStr;
            std::string resultStr;

            try
            {
                Tokenizer tokenizer;
                auto tokens = tokenizer.tokenize(code);
                Parser parser(tokens);
                auto program = parser.parseProgram();

                for (auto &stmt : program->statements)
                    stmt->execute(globalScope);
            }
            catch (ReturnException &e)
            {
                resultStr = format_output(e.value);
            }
            catch (std::exception &e)
            {
                errorStr = e.what();
            }

            // Restore stdout
            std::cout.rdbuf(oldBuf);
            std::string stdoutStr = capturedOut.str();

            std::string status = errorStr.empty() ? "ok" : "error";
            std::cout << make_json_response(cellId, status, stdoutStr, errorStr, resultStr, executionCount) << std::endl;
            std::cout.flush();
            continue;
        }

        if (action == "input_reply")
        {
            // This arrives as a response to an input_request we sent.
            // It's handled inline in the input() override via readLine,
            // so we shouldn't normally reach here. Ignore it.
            continue;
        }

        if (action == "complete")
        {
            std::string code = cmd["code"];
            // Simple completion: list scope variables and builtins that match prefix
            std::vector<std::string> matches;
            for (auto &[name, _] : globalScope.getAll())
            {
                if (name.find(code) == 0)
                    matches.push_back(name);
            }
            // Return as JSON array
            std::string out = "{\"status\":\"ok\",\"completions\":[";
            for (size_t i = 0; i < matches.size(); i++)
            {
                if (i > 0)
                    out += ",";
                out += "\"" + json_escape(matches[i]) + "\"";
            }
            out += "]}";
            std::cout << out << std::endl;
            std::cout.flush();
            continue;
        }

        // Unknown action
        std::cout << "{\"status\":\"error\",\"stderr\":\"Unknown action: " + json_escape(action) + "\"}" << std::endl;
        std::cout.flush();
    }
}