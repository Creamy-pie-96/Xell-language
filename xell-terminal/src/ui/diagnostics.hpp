#pragma once

// =============================================================================
// diagnostics.hpp — Inline diagnostics for the Xell Terminal IDE
// =============================================================================
// Runs the Xell static analyzer (--check) on files and renders:
//   - Inline squiggly underlines (red for errors, yellow for warnings)
//   - Gutter markers (● for errors, ▲ for warnings)
//   - Diagnostic list for the bottom panel
// =============================================================================

#include <string>
#include <vector>
#include <regex>
#include <algorithm>
#include <sstream>
#include <cstdio>
#include <array>
#include <memory>
#include "../terminal/types.hpp"
#include "../theme/theme_loader.hpp"

namespace xterm
{

    // ─── Diagnostic types ────────────────────────────────────────────────

    enum class DiagnosticSeverity
    {
        Error,
        Warning,
        Info,
        Hint,
    };

    struct Diagnostic
    {
        int line = 0;           // 0-based line number
        int col = 0;            // 0-based column
        int endCol = -1;        // end column (-1 = whole line)
        std::string message;
        std::string source;     // "analyzer", "parser", "runtime"
        DiagnosticSeverity severity = DiagnosticSeverity::Error;
    };

    // ─── Diagnostic Engine ───────────────────────────────────────────────

    class DiagnosticEngine
    {
    public:
        DiagnosticEngine(const ThemeData &theme) : theme_(theme)
        {
            loadColors();
        }

        // Set the xell binary path
        void setXellBinary(const std::string &path) { xellPath_ = path; }

        // Run --check on a file and collect diagnostics
        void check(const std::string &filePath)
        {
            diagnostics_.clear();

            if (xellPath_.empty())
            {
                // Try common locations
                const char *paths[] = {
                    "./xell", "../build/xell", "/usr/local/bin/xell",
                    "/usr/bin/xell"};
                for (auto p : paths)
                {
                    if (std::filesystem::exists(p))
                    {
                        xellPath_ = p;
                        break;
                    }
                }
                if (xellPath_.empty())
                    return;
            }

            // Run: xell --check <file> 2>&1
            std::string cmd = xellPath_ + " --check " + filePath + " 2>&1";
            std::string output = execCommand(cmd);

            parseDiagnostics(output);
        }

        // Manually add a diagnostic (for live error detection)
        void addDiagnostic(Diagnostic d)
        {
            diagnostics_.push_back(std::move(d));
        }

        void clear() { diagnostics_.clear(); }

        // Get all diagnostics
        const std::vector<Diagnostic> &diagnostics() const { return diagnostics_; }

        // Get diagnostics for a specific line
        std::vector<const Diagnostic *> diagnosticsForLine(int line) const
        {
            std::vector<const Diagnostic *> result;
            for (auto &d : diagnostics_)
                if (d.line == line)
                    result.push_back(&d);
            return result;
        }

        // Counts
        int errorCount() const
        {
            int n = 0;
            for (auto &d : diagnostics_)
                if (d.severity == DiagnosticSeverity::Error)
                    n++;
            return n;
        }

        int warningCount() const
        {
            int n = 0;
            for (auto &d : diagnostics_)
                if (d.severity == DiagnosticSeverity::Warning)
                    n++;
            return n;
        }

        // Gutter marker for a line ('E', 'W', ' ')
        char gutterMarker(int line) const
        {
            bool hasError = false, hasWarning = false;
            for (auto &d : diagnostics_)
            {
                if (d.line == line)
                {
                    if (d.severity == DiagnosticSeverity::Error)
                        hasError = true;
                    else if (d.severity == DiagnosticSeverity::Warning)
                        hasWarning = true;
                }
            }
            if (hasError)
                return 'E';
            if (hasWarning)
                return 'W';
            return ' ';
        }

        Color gutterMarkerColor(int line) const
        {
            char marker = gutterMarker(line);
            if (marker == 'E')
                return errorColor_;
            if (marker == 'W')
                return warningColor_;
            return {51, 51, 51};
        }

        // Render the diagnostic list (for the bottom panel)
        struct DiagListRender
        {
            std::vector<std::vector<Cell>> cells;
        };

        DiagListRender renderList(int width, int height) const
        {
            DiagListRender out;
            out.cells.resize(height);
            for (auto &row : out.cells)
            {
                row.resize(width);
                for (auto &c : row)
                {
                    c.ch = U' ';
                    c.bg = listBg_;
                    c.fg = listFg_;
                    c.dirty = true;
                }
            }

            if (diagnostics_.empty())
            {
                std::string msg = "No problems detected ✓";
                for (int i = 0; i < (int)msg.size() && i + 2 < width; i++)
                {
                    out.cells[0][i + 2].ch = (char32_t)msg[i];
                    out.cells[0][i + 2].fg = {98, 195, 121}; // green
                }
                return out;
            }

            // Summary line
            std::string summary = "⊘ " + std::to_string(errorCount()) + " errors, " +
                                  std::to_string(warningCount()) + " warnings";
            for (int i = 0; i < (int)summary.size() && i + 1 < width; i++)
            {
                out.cells[0][i + 1].ch = (char32_t)summary[i];
                out.cells[0][i + 1].fg = errorCount() > 0 ? errorColor_ : warningColor_;
                out.cells[0][i + 1].bold = true;
            }

            // Diagnostic entries
            for (int d = 0; d < (int)diagnostics_.size() && d + 1 < height; d++)
            {
                auto &diag = diagnostics_[d];
                int row = d + 1;

                Color iconColor = diag.severity == DiagnosticSeverity::Error
                                      ? errorColor_
                                      : warningColor_;
                std::string icon = diag.severity == DiagnosticSeverity::Error ? "✗" : "▲";

                // Icon
                for (int i = 0; i < (int)icon.size() && 2 + i < width; i++)
                {
                    out.cells[row][2 + i].ch = (char32_t)icon[i];
                    out.cells[row][2 + i].fg = iconColor;
                }

                // Line number
                std::string lineStr = " L" + std::to_string(diag.line + 1) + ": ";
                int col = 4;
                for (int i = 0; i < (int)lineStr.size() && col + i < width; i++)
                {
                    out.cells[row][col + i].ch = (char32_t)lineStr[i];
                    out.cells[row][col + i].fg = {128, 128, 128};
                }

                // Message
                col += (int)lineStr.size();
                for (int i = 0; i < (int)diag.message.size() && col + i < width; i++)
                {
                    out.cells[row][col + i].ch = (char32_t)diag.message[i];
                    out.cells[row][col + i].fg = listFg_;
                }
            }

            return out;
        }

        // Colors
        Color errorColor() const { return errorColor_; }
        Color warningColor() const { return warningColor_; }

    private:
        const ThemeData &theme_;
        std::vector<Diagnostic> diagnostics_;
        std::string xellPath_;

        Color errorColor_ = {224, 108, 117};
        Color warningColor_ = {229, 192, 123};
        Color listBg_ = {24, 24, 24};
        Color listFg_ = {204, 204, 204};

        void loadColors()
        {
            errorColor_ = getUIColor(theme_, "error", errorColor_);
            warningColor_ = getUIColor(theme_, "warning", warningColor_);
        }

        // Execute a command and capture output
        static std::string execCommand(const std::string &cmd)
        {
            std::string result;
            std::array<char, 256> buffer;
            FILE *pipe = popen(cmd.c_str(), "r");
            if (!pipe)
                return "";
            while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
                result += buffer.data();
            pclose(pipe);
            return result;
        }

        // Parse --check output into diagnostics
        // Xell --check output format:
        //   ⚠ Error at line 5: undefined variable 'x'
        //   ⚠ Warning at line 12: unused variable 'y'
        void parseDiagnostics(const std::string &output)
        {
            std::istringstream stream(output);
            std::string line;

            while (std::getline(stream, line))
            {
                Diagnostic diag;

                // Try to match: "Error at line N: message"
                // or: "Warning at line N: message"
                // or: "[ERROR] line N: message"
                // or: "Line N: error: message"

                // Generic pattern: find "line" + number + message
                size_t lineIdx = std::string::npos;
                std::string lowerLine = line;
                for (auto &c : lowerLine)
                    c = std::tolower(c);

                // Determine severity
                if (lowerLine.find("error") != std::string::npos)
                    diag.severity = DiagnosticSeverity::Error;
                else if (lowerLine.find("warning") != std::string::npos)
                    diag.severity = DiagnosticSeverity::Warning;
                else
                    continue; // skip non-diagnostic lines

                // Find "line N"
                lineIdx = lowerLine.find("line ");
                if (lineIdx != std::string::npos)
                {
                    size_t numStart = lineIdx + 5;
                    size_t numEnd = numStart;
                    while (numEnd < line.size() && std::isdigit(line[numEnd]))
                        numEnd++;
                    if (numEnd > numStart)
                    {
                        diag.line = std::stoi(line.substr(numStart, numEnd - numStart)) - 1; // to 0-based
                    }
                }

                // Extract message: everything after the last ': '
                size_t msgStart = line.rfind(": ");
                if (msgStart != std::string::npos && msgStart + 2 < line.size())
                    diag.message = line.substr(msgStart + 2);
                else
                    diag.message = line;

                diag.source = "analyzer";
                diagnostics_.push_back(diag);
            }

            // Sort by line
            std::sort(diagnostics_.begin(), diagnostics_.end(),
                      [](const Diagnostic &a, const Diagnostic &b)
                      { return a.line < b.line; });
        }
    };

} // namespace xterm
