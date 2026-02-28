#pragma once

// ═══════════════════════════════════════════════════════════
// Xell Kernel Mode — JSON protocol for notebook integration
// ═══════════════════════════════════════════════════════════
//
// Provides:
//   • Minimal JSON parser/builder (no external deps)
//   • Kernel loop: reads JSON commands from stdin, writes JSON responses to stdout
//   • Integrates with Xell's Interpreter, Lexer, and Parser
//
// Protocol: see Extensions/notebook/NXEL_FORMAT.md
// ═══════════════════════════════════════════════════════════

#include "../src/lexer/lexer.hpp"
#include "../src/parser/parser.hpp"
#include "../src/interpreter/interpreter.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

// ── Minimal JSON Helpers ─────────────────────────────────

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
        if (i < json.size() && json[i] == '"')
        {
            result[key] = readString();
        }
        else
        {
            std::string val;
            while (i < json.size() && json[i] != ',' && json[i] != '}')
                val += json[i++];
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

// ── Kernel Mode ──────────────────────────────────────────

// Global kernel state
bool _xell_kernel_mode = false;
std::streambuf *_xell_kernel_real_stdout = nullptr;
std::string _xell_kernel_cell_id;

namespace kernel_io
{
    static std::streambuf *realStdoutBuf = nullptr;
    static std::string currentCellId;
    static bool inKernelMode = false;
}

inline void runKernel()
{
    using namespace xell;

    Interpreter interp;
    int executionCount = 0;

    kernel_io::inKernelMode = true;
    kernel_io::realStdoutBuf = std::cout.rdbuf();

    _xell_kernel_mode = true;
    _xell_kernel_real_stdout = std::cout.rdbuf();

    // Signal ready
    std::cout << "{\"status\":\"kernel_ready\",\"version\":\"0.1\"}" << std::endl;
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
            interp.reset();
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
            _xell_kernel_cell_id = cellId;

            // Capture stdout
            std::ostringstream capturedOut;
            std::streambuf *oldBuf = std::cout.rdbuf(capturedOut.rdbuf());

            std::string errorStr;
            std::string resultStr;

            try
            {
                // Lex → Parse → Execute using Xell's pipeline
                Lexer lexer(code);
                auto tokens = lexer.tokenize();

                Parser parser(tokens);
                auto program = parser.parse();

                interp.run(*program);
            }
            catch (GiveSignal &g)
            {
                // Top-level give — format the value as result
                resultStr = g.value.toString();
            }
            catch (std::exception &e)
            {
                errorStr = e.what();
            }

            // Restore stdout
            std::cout.rdbuf(oldBuf);
            std::string stdoutStr = capturedOut.str();

            // Also grab any output from the interpreter's captured output
            const auto &interpOutput = interp.output();
            if (!interpOutput.empty())
            {
                for (const auto &line : interpOutput)
                {
                    if (!stdoutStr.empty() && stdoutStr.back() != '\n')
                        stdoutStr += '\n';
                    stdoutStr += line;
                }
            }

            std::string status = errorStr.empty() ? "ok" : "error";
            std::cout << make_json_response(cellId, status, stdoutStr, errorStr, resultStr, executionCount) << std::endl;
            std::cout.flush();
            continue;
        }

        if (action == "complete")
        {
            std::string code = cmd["code"];
            // Simple completion: list scope variables that match prefix
            // TODO: expand when Interpreter exposes scope enumeration
            std::cout << "{\"status\":\"ok\",\"completions\":[]}" << std::endl;
            std::cout.flush();
            continue;
        }

        // Unknown action
        std::cout << "{\"status\":\"error\",\"stderr\":\"Unknown action: " + json_escape(action) + "\"}" << std::endl;
        std::cout.flush();
    }
}
