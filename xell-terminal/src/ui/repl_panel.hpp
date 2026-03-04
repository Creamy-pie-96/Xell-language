#pragma once

// =============================================================================
// repl_panel.hpp — REPL Integration Panel for the Xell Terminal IDE
// =============================================================================
// Phase 4: Run selection in REPL, run file, variable inspector, inline eval.
//
// Three sub-panels:
//   1. TERMINAL — interactive REPL with command input + scrollback output
//   2. OUTPUT   — file execution output (Ctrl+Shift+B)
//   3. VARIABLES — live variable inspector from REPL session
//
// Integration with editor:
//   - Ctrl+Enter      → run selected code in REPL
//   - Ctrl+Shift+B    → run current file
//   - Ctrl+Shift+E    → inline evaluate line → ghost text
// =============================================================================

#include <string>
#include <vector>
#include <deque>
#include <functional>
#include <filesystem>
#include <cstdio>
#include <array>
#include <memory>
#include <sstream>
#include <algorithm>
#include <fstream>
#include "../terminal/types.hpp"
#include "../theme/theme_loader.hpp"
#include "panel.hpp"
#include "embedded_terminal.hpp"

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#include <sys/types.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace xterm
{

    // ─── REPL Output line ────────────────────────────────────────────────

    struct REPLLine
    {
        std::string text;
        enum Kind
        {
            INPUT,
            OUTPUT,
            ERROR,
            INFO,
            PROMPT
        } kind = OUTPUT;
    };

    // ─── Variable entry (for inspector) ──────────────────────────────────

    struct VarEntry
    {
        std::string name;
        std::string type;
        std::string value; // preview string
        bool expanded = false;
    };

    // ─── Active bottom tab ───────────────────────────────────────────────

    enum class BottomTab
    {
        TERMINAL,
        OUTPUT,
        DIAGNOSTICS,
        VARIABLES,
    };

    // ─── Shell output capture utility ────────────────────────────────────

    static inline std::string captureCommand(const std::string &cmd, int &exitCode)
    {
        std::string result;
        std::string fullCmd = cmd + " 2>&1";
        FILE *fp = popen(fullCmd.c_str(), "r");
        if (!fp)
        {
            exitCode = -1;
            return "[failed to execute: " + cmd + "]";
        }
        char buf[4096];
        while (fgets(buf, sizeof(buf), fp))
            result += buf;
        exitCode = pclose(fp);
#ifndef _WIN32
        if (WIFEXITED(exitCode))
            exitCode = WEXITSTATUS(exitCode);
#endif
        // Strip trailing newline
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
            result.pop_back();
        return result;
    }

    // Pipe a string to a command's stdin and capture its stdout+stderr.
    // No temp files, no shell escaping — pure pipe I/O.
    static inline std::string captureCommandWithStdin(
        const std::string &cmd, const std::string &input, int &exitCode)
    {
        std::string result;
        // Use pipe + fork to write to stdin and read from stdout+stderr
        int pipeIn[2], pipeOut[2];
        if (pipe(pipeIn) != 0 || pipe(pipeOut) != 0)
        {
            exitCode = -1;
            return "[failed to create pipes]";
        }
        pid_t pid = fork();
        if (pid < 0)
        {
            exitCode = -1;
            return "[failed to fork]";
        }
        if (pid == 0)
        {
            // Child: wire up stdin/stdout/stderr
            close(pipeIn[1]);
            close(pipeOut[0]);
            dup2(pipeIn[0], STDIN_FILENO);
            dup2(pipeOut[1], STDOUT_FILENO);
            dup2(pipeOut[1], STDERR_FILENO);
            close(pipeIn[0]);
            close(pipeOut[1]);
            execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
            _exit(127);
        }
        // Parent: write input to child's stdin, then read output
        close(pipeIn[0]);
        close(pipeOut[1]);

        // Write all input
        const char *data = input.data();
        size_t remaining = input.size();
        while (remaining > 0)
        {
            ssize_t n = write(pipeIn[1], data, remaining);
            if (n <= 0)
                break;
            data += n;
            remaining -= n;
        }
        close(pipeIn[1]); // signal EOF

        // Read all output
        char buf[4096];
        ssize_t n;
        while ((n = read(pipeOut[0], buf, sizeof(buf))) > 0)
            result.append(buf, n);
        close(pipeOut[0]);

        // Wait for child
        int status = 0;
        waitpid(pid, &status, 0);
        exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

        // Strip trailing newline
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
            result.pop_back();
        return result;
    }

    // ─── Find xell binary ───────────────────────────────────────────────

    static inline std::string findXellBinary()
    {
        std::vector<std::string> candidates = {
            "./build/xell",
            "../build/xell",
            "./xell",
            "./build_release/xell",
            "../build_release/xell",
            "/usr/local/bin/xell",
#ifdef _WIN32
            ".\\build\\xell.exe",
            "..\\build\\xell.exe",
            ".\\xell.exe",
            ".\\build_release\\xell.exe",
            "..\\build_release\\xell.exe",
#endif
        };
        for (auto &c : candidates)
            if (std::filesystem::exists(c))
                return c;
#ifdef _WIN32
        return "xell.exe"; // fallback to PATH
#else
        return "xell"; // fallback to PATH
#endif
    }

    // ─── Platform temp directory ─────────────────────────────────────────

    static inline std::string xellTempDir()
    {
        static std::string dir;
        if (dir.empty())
        {
            try
            {
                dir = std::filesystem::temp_directory_path().string();
            }
            catch (...)
            {
#ifdef _WIN32
                dir = ".";
#else
                dir = "/tmp";
#endif
            }
        }
        return dir;
    }

    // =====================================================================
    // REPLSession — manages a REPL-like session via the xell binary
    // =====================================================================

    class REPLSession
    {
    public:
        REPLSession()
        {
            xellBin_ = findXellBinary();
        }

        // Execute a code snippet and return the output
        struct ExecResult
        {
            std::string output;
            std::string errors;
            int exitCode = 0;
            std::vector<VarEntry> variables; // updated after each run
        };

        ExecResult executeCode(const std::string &code)
        {
            ExecResult result;

            // Write code to a temp file
            std::string tmpFile = xellTempDir() + "/xell_repl_" + std::to_string(sessionId_) + ".xel";
            {
                FILE *f = fopen(tmpFile.c_str(), "w");
                if (!f)
                {
                    result.errors = "Failed to create temp file";
                    result.exitCode = -1;
                    return result;
                }
                // Prepend session history for continuity
                for (auto &h : history_)
                    fprintf(f, "%s\n", h.c_str());
                fprintf(f, "%s\n", code.c_str());
                fclose(f);
            }

            // Execute
            std::string cmd = xellBin_ + " " + tmpFile;
            result.output = captureCommand(cmd, result.exitCode);

            // Parse for errors vs output
            if (result.exitCode != 0)
            {
                result.errors = result.output;
                result.output.clear();
            }

            // Add to history
            history_.push_back(code);

            // Parse variables (ask xell to dump vars)
            result.variables = inspectVariables();

            return result;
        }

        // Run a file and return output
        ExecResult runFile(const std::string &filePath)
        {
            ExecResult result;
            std::string cmd = xellBin_ + " \"" + filePath + "\"";
            result.output = captureCommand(cmd, result.exitCode);

            if (result.exitCode != 0)
            {
                result.errors = result.output;
                result.output.clear();
            }

            // Read the file into history so inspectVariables can extract vars
            history_.clear();
            {
                std::ifstream fin(filePath);
                if (fin.is_open())
                {
                    std::string line;
                    while (std::getline(fin, line))
                        history_.push_back(line);
                }
            }

            result.variables = inspectVariables();

            return result;
        }

        // Evaluate a single expression (for inline eval)
        std::string evalExpression(const std::string &expr)
        {
            std::string tmpFile = xellTempDir() + "/xell_eval_" + std::to_string(sessionId_) + ".xel";
            {
                FILE *f = fopen(tmpFile.c_str(), "w");
                if (!f)
                    return "?";
                // Wrap expression in print for capture
                fprintf(f, "print(%s)\n", expr.c_str());
                fclose(f);
            }

            int exitCode;
            std::string out = captureCommand(xellBin_ + " " + tmpFile, exitCode);
            if (exitCode != 0)
                return ""; // evaluation failed silently
            return out;
        }

        // Inspect variables in the current session
        std::vector<VarEntry> inspectVariables()
        {
            std::vector<VarEntry> vars;

            if (history_.empty())
                return vars;

            // First pass: extract variable names from assignment history
            std::vector<std::string> varNames;
            for (auto &h : history_)
            {
                std::string line = h;
                size_t eqPos = line.find('=');
                if (eqPos == std::string::npos || eqPos == 0)
                    continue;
                if (eqPos > 0 && (line[eqPos - 1] == '!' || line[eqPos - 1] == '<' || line[eqPos - 1] == '>'))
                    continue;
                if (eqPos + 1 < line.size() && line[eqPos + 1] == '=')
                    continue; // ==

                std::string lhs = line.substr(0, eqPos);
                while (!lhs.empty() && lhs.back() == ' ')
                    lhs.pop_back();

                if (lhs.substr(0, 4) == "let ")
                    lhs = lhs.substr(4);
                if (lhs.substr(0, 3) == "be ")
                    lhs = lhs.substr(3);
                while (!lhs.empty() && lhs.front() == ' ')
                    lhs.erase(lhs.begin());

                bool valid = !lhs.empty();
                for (char c : lhs)
                    if (!isalnum(c) && c != '_')
                        valid = false;

                if (valid)
                {
                    // Avoid duplicates — keep latest
                    auto it = std::find(varNames.begin(), varNames.end(), lhs);
                    if (it != varNames.end())
                        varNames.erase(it);
                    varNames.push_back(lhs);
                }
            }

            if (varNames.empty())
                return vars;

            // Build a script that prints type and value of each variable
            std::string tmpFile = xellTempDir() + "/xell_vars_" + std::to_string(sessionId_) + ".xel";
            {
                FILE *f = fopen(tmpFile.c_str(), "w");
                if (!f)
                    return vars;
                for (auto &h : history_)
                    fprintf(f, "%s\n", h.c_str());
                // Print type and value for each variable
                for (auto &name : varNames)
                {
                    fprintf(f, "print(\"__VAR__:%s:\" + str(type(%s)) + \":\" + str(%s))\n",
                            name.c_str(), name.c_str(), name.c_str());
                }
                fclose(f);
            }

            int exitCode = 0;
            std::string output = captureCommand(xellBin_ + " " + tmpFile, exitCode);

            // Parse __VAR__ lines
            std::istringstream ss(output);
            std::string line;
            while (std::getline(ss, line))
            {
                if (line.substr(0, 8) == "__VAR__:")
                {
                    // Format: __VAR__:name:type:value
                    std::string rest = line.substr(8);
                    size_t p1 = rest.find(':');
                    if (p1 == std::string::npos)
                        continue;
                    std::string name = rest.substr(0, p1);
                    std::string rest2 = rest.substr(p1 + 1);
                    size_t p2 = rest2.find(':');
                    if (p2 == std::string::npos)
                        continue;
                    std::string type = rest2.substr(0, p2);
                    std::string value = rest2.substr(p2 + 1);

                    VarEntry ve;
                    ve.name = name;
                    ve.type = type;
                    ve.value = value;
                    vars.push_back(ve);
                }
            }

            // If parsing failed, fall back to simple extraction
            if (vars.empty())
            {
                for (auto &name : varNames)
                {
                    VarEntry ve;
                    ve.name = name;
                    ve.type = "?";
                    // Get value from last assignment
                    for (auto it = history_.rbegin(); it != history_.rend(); ++it)
                    {
                        size_t eq = it->find('=');
                        if (eq == std::string::npos)
                            continue;
                        std::string lhsTmp = it->substr(0, eq);
                        while (!lhsTmp.empty() && lhsTmp.back() == ' ')
                            lhsTmp.pop_back();
                        if (lhsTmp.substr(0, 4) == "let ")
                            lhsTmp = lhsTmp.substr(4);
                        if (lhsTmp.substr(0, 3) == "be ")
                            lhsTmp = lhsTmp.substr(3);
                        while (!lhsTmp.empty() && lhsTmp.front() == ' ')
                            lhsTmp.erase(lhsTmp.begin());
                        if (lhsTmp == name)
                        {
                            ve.value = it->substr(eq + 1);
                            while (!ve.value.empty() && ve.value.front() == ' ')
                                ve.value.erase(ve.value.begin());
                            break;
                        }
                    }
                    vars.push_back(ve);
                }
            }

            return vars;
        }

        void clearHistory()
        {
            history_.clear();
        }

        const std::vector<std::string> &history() const { return history_; }

    private:
        std::string xellBin_;
        std::vector<std::string> history_;
        int sessionId_ = 1;
    };

    // =====================================================================
    // REPLPanel — Bottom panel for REPL / Output / Variables
    // =====================================================================

    class REPLPanel : public Panel
    {
    public:
        explicit REPLPanel(const ThemeData &theme)
            : theme_(theme)
        {
            loadColors();
        }

        PanelType type() const override { return PanelType::REPL; }
        std::string title() const override { return "REPL"; }

        // ── Tab management ──────────────────────────────────────────

        BottomTab activeTab() const { return activeTab_; }
        void setActiveTab(BottomTab tab) { activeTab_ = tab; }
        void setHoverCol(int col) { hoverCol_ = col; }
        void cycleTab()
        {
            int t = static_cast<int>(activeTab_);
            t = (t + 1) % 4;
            activeTab_ = static_cast<BottomTab>(t);
        }

        // ── Run selection in REPL (Ctrl+Enter) ──────────────────────

        void runCode(const std::string &code)
        {
            // Clear previous output for a fresh run
            fileOutputLog_.clear();

            // Show what we're running (first line as header)
            REPLLine header;
            std::string firstLine = code;
            auto nlPos = firstLine.find('\n');
            if (nlPos != std::string::npos)
            {
                int lineCount = 1;
                for (char c : code)
                    if (c == '\n')
                        lineCount++;
                header.text = "[Running " + std::to_string(lineCount) + " lines to cursor]";
            }
            else
            {
                header.text = "[Running: " + firstLine + "]";
            }
            header.kind = REPLLine::INFO;
            fileOutputLog_.push_back(header);

            // Execute — clear session history since we run the full code from top
            session_.clearHistory();
            auto result = session_.executeCode(code);

            if (!result.output.empty())
            {
                // Split multi-line output
                std::string line;
                for (char c : result.output)
                {
                    if (c == '\n')
                    {
                        REPLLine out;
                        out.text = line;
                        out.kind = REPLLine::OUTPUT;
                        fileOutputLog_.push_back(out);
                        line.clear();
                    }
                    else
                    {
                        line += c;
                    }
                }
                if (!line.empty())
                {
                    REPLLine out;
                    out.text = line;
                    out.kind = REPLLine::OUTPUT;
                    fileOutputLog_.push_back(out);
                }
            }

            if (!result.errors.empty())
            {
                REPLLine err;
                err.text = result.errors;
                err.kind = REPLLine::ERROR;
                fileOutputLog_.push_back(err);
            }

            variables_ = result.variables;
        }

        // ── Run file (Ctrl+Shift+B) ─────────────────────────────────

        void runFile(const std::string &filePath)
        {
            fileOutputLog_.clear();

            REPLLine header;
            header.text = "[Running: " + std::filesystem::path(filePath).filename().string() + "]";
            header.kind = REPLLine::INFO;
            fileOutputLog_.push_back(header);

            auto result = session_.runFile(filePath);

            if (!result.output.empty())
            {
                // Split multi-line output
                std::string line;
                for (char c : result.output)
                {
                    if (c == '\n')
                    {
                        REPLLine out;
                        out.text = line;
                        out.kind = REPLLine::OUTPUT;
                        fileOutputLog_.push_back(out);
                        line.clear();
                    }
                    else
                    {
                        line += c;
                    }
                }
                if (!line.empty())
                {
                    REPLLine out;
                    out.text = line;
                    out.kind = REPLLine::OUTPUT;
                    fileOutputLog_.push_back(out);
                }
            }

            if (!result.errors.empty())
            {
                std::string line;
                for (char c : result.errors)
                {
                    if (c == '\n')
                    {
                        REPLLine err;
                        err.text = line;
                        err.kind = REPLLine::ERROR;
                        fileOutputLog_.push_back(err);
                        line.clear();
                    }
                    else
                    {
                        line += c;
                    }
                }
                if (!line.empty())
                {
                    REPLLine err;
                    err.text = line;
                    err.kind = REPLLine::ERROR;
                    fileOutputLog_.push_back(err);
                }
            }

            REPLLine footer;
            footer.text = "[Exit code: " + std::to_string(result.exitCode) + "]";
            footer.kind = (result.exitCode == 0) ? REPLLine::INFO : REPLLine::ERROR;
            fileOutputLog_.push_back(footer);

            // Populate variables from file run
            variables_ = result.variables;

            // Switch to output tab
            activeTab_ = BottomTab::OUTPUT;
            fileScrollOffset_ = std::max(0, (int)fileOutputLog_.size() - contentHeight());
        }

        // ── Inline evaluation (Ctrl+Shift+E) ────────────────────────

        std::string evalInline(const std::string &expression)
        {
            return session_.evalExpression(expression);
        }

        // ── Ghost text for inline eval result ────────────────────────

        struct GhostText
        {
            int line = -1;
            std::string text;
        };

        GhostText ghostText() const { return ghostText_; }

        void setGhostText(int line, const std::string &text)
        {
            ghostText_.line = line;
            ghostText_.text = text;
        }

        void clearGhostText()
        {
            ghostText_.line = -1;
            ghostText_.text.clear();
        }

        // ── Diagnostics integration ─────────────────────────────────

        void setDiagnosticLines(const std::vector<std::string> &lines)
        {
            diagnosticLines_ = lines;
        }

        // ── Command input (terminal tab) ─────────────────────────────

        void handleChar(char ch)
        {
            if (activeTab_ != BottomTab::TERMINAL)
                return;

            if (ch == '\n' || ch == '\r')
            {
                // Submit command
                if (!inputLine_.empty())
                {
                    runCode(inputLine_);
                    inputLine_.clear();
                    inputCursor_ = 0;
                }
            }
            else if (ch == '\b' || ch == 127)
            {
                if (inputCursor_ > 0)
                {
                    inputLine_.erase(inputCursor_ - 1, 1);
                    inputCursor_--;
                }
            }
            else
            {
                inputLine_.insert(inputCursor_, 1, ch);
                inputCursor_++;
            }
        }

        void handleTextInput(const std::string &text)
        {
            if (activeTab_ == BottomTab::TERMINAL)
            {
                ensureTerminalStarted();
                if (embeddedTerm_ && embeddedTerm_->isRunning())
                {
                    embeddedTerm_->handleTextInput(text);
                    return;
                }
            }
            // Fallback for non-terminal tabs (OUTPUT, DIAGNOSTICS, VARIABLES) — do nothing
        }

        // ── Mouse handling ───────────────────────────────────────────

        bool handleMouseClick(int row, int /*col*/, bool /*shift*/) override
        {
            // Row 0 = tab bar — handled by LayoutManager's handleBottomTabClick
            // Other rows = content area (click to focus, future: select text)
            if (row == 0)
                return false; // let parent handle tab bar
            // Click in content area — just consume the event
            return true;
        }

        bool handleMouseWheel(int delta) override
        {
            if (activeTab_ == BottomTab::TERMINAL)
            {
                scrollOffset_ -= delta * 3;
                scrollOffset_ = std::max(0, scrollOffset_);
                int maxScroll = std::max(0, (int)outputLog_.size() - (rect_.h - 2));
                scrollOffset_ = std::min(scrollOffset_, maxScroll);
            }
            else if (activeTab_ == BottomTab::OUTPUT)
            {
                fileScrollOffset_ -= delta * 3;
                fileScrollOffset_ = std::max(0, fileScrollOffset_);
                int maxScroll = std::max(0, (int)fileOutputLog_.size() - (rect_.h - 2));
                fileScrollOffset_ = std::min(fileScrollOffset_, maxScroll);
            }
            return true;
        }

        // ── Keyboard handling ────────────────────────────────────────

        bool handleKeyDown(const SDL_Event &event) override
        {
            if (event.type != SDL_KEYDOWN)
                return false;

            auto key = event.key.keysym;
            bool ctrl = (key.mod & KMOD_CTRL) != 0;
            (void)ctrl;

            // Tab cycling works for all tabs
            if (key.sym == SDLK_TAB && ctrl)
            {
                cycleTab();
                return true;
            }

            // If TERMINAL tab is active, send keys to embedded terminal
            if (activeTab_ == BottomTab::TERMINAL)
            {
                ensureTerminalStarted();
                if (embeddedTerm_ && embeddedTerm_->isRunning())
                {
                    embeddedTerm_->handleKeyEvent(event);
                    return true;
                }
            }

            switch (key.sym)
            {
            case SDLK_TAB:
                cycleTab();
                return true;

            case SDLK_UP:
                scrollUp();
                return true;

            case SDLK_DOWN:
                scrollDown();
                return true;

            case SDLK_PAGEUP:
                for (int i = 0; i < contentHeight(); i++)
                    scrollUp();
                return true;

            case SDLK_PAGEDOWN:
                for (int i = 0; i < contentHeight(); i++)
                    scrollDown();
                return true;

            default:
                break;
            }

            return false;
        }

        // ── Rendering ────────────────────────────────────────────────

        std::vector<std::vector<Cell>> render() const override
        {
            int w = rect_.w;
            int h = rect_.h;
            std::vector<std::vector<Cell>> cells(h, std::vector<Cell>(w));

            // Fill background
            for (int r = 0; r < h; r++)
                for (int c = 0; c < w; c++)
                {
                    cells[r][c].ch = U' ';
                    cells[r][c].bg = contentBg_;
                    cells[r][c].fg = contentFg_;
                    cells[r][c].dirty = true;
                }

            // Tab bar (row 0)
            renderTabBar(cells[0], w);

            // Content area
            switch (activeTab_)
            {
            case BottomTab::TERMINAL:
                renderREPL(cells, w, h);
                break;
            case BottomTab::OUTPUT:
                renderOutput(cells, w, h);
                break;
            case BottomTab::DIAGNOSTICS:
                renderDiagnostics(cells, w, h);
                break;
            case BottomTab::VARIABLES:
                renderVariables(cells, w, h);
                break;
            }

            return cells;
        }

        // ── Accessors ────────────────────────────────────────────────

        const std::vector<VarEntry> &variables() const { return variables_; }
        const std::deque<REPLLine> &outputLog() const { return outputLog_; }
        const std::vector<REPLLine> &fileOutputLog() const { return fileOutputLog_; }

        REPLSession &session() { return session_; }

    private:
        const ThemeData &theme_;
        mutable REPLSession session_;

        mutable BottomTab activeTab_ = BottomTab::TERMINAL;

        // Embedded PTY terminal for the TERMINAL tab
        mutable std::unique_ptr<EmbeddedTerminal> embeddedTerm_;
        mutable int hoverCol_ = -1;

        void ensureTerminalStarted() const
        {
            if (!embeddedTerm_)
            {
                int rows = std::max(1, rect_.h - 1); // minus tab bar row
                int cols = std::max(1, rect_.w);
                embeddedTerm_ = std::make_unique<EmbeddedTerminal>(rows, cols);
                embeddedTerm_->start();
            }
        }

        // REPL terminal state
        mutable std::deque<REPLLine> outputLog_;
        mutable std::string inputLine_;
        mutable int inputCursor_ = 0;
        mutable int scrollOffset_ = 0;
        static constexpr int MAX_OUTPUT_LINES = 5000;

        // File output state
        mutable std::vector<REPLLine> fileOutputLog_;
        mutable int fileScrollOffset_ = 0;

        // Variable inspector
        mutable std::vector<VarEntry> variables_;

        // Diagnostics
        mutable std::vector<std::string> diagnosticLines_;

        // Ghost text
        mutable GhostText ghostText_;

        // Theme colors
        Color tabBarBg_ = {30, 30, 30};
        Color tabActiveFg_ = {255, 255, 255};
        Color tabInactiveFg_ = {128, 128, 128};
        Color contentBg_ = {24, 24, 24};
        Color contentFg_ = {204, 204, 204};
        Color inputBg_ = {30, 30, 30};
        Color promptColor_ = {86, 156, 214};    // blue
        Color outputColor_ = {204, 204, 204};   // gray
        Color errorColor_ = {244, 71, 71};      // red
        Color infoColor_ = {78, 201, 176};      // teal
        Color inputColor_ = {206, 145, 120};    // orange
        Color varNameColor_ = {156, 220, 254};  // light blue
        Color varTypeColor_ = {78, 201, 176};   // teal
        Color varValueColor_ = {206, 145, 120}; // orange
        Color borderColor_ = {51, 51, 51};

        void loadColors()
        {
            tabBarBg_ = getUIColor(theme_, "tab_bar_bg", tabBarBg_);
            contentBg_ = getUIColor(theme_, "terminal_bg", contentBg_);
            contentFg_ = getUIColor(theme_, "terminal_fg", contentFg_);
            borderColor_ = getUIColor(theme_, "panel_border", borderColor_);
        }

        int contentHeight() const { return std::max(1, rect_.h - 2); } // -1 tab, -1 input

        void scrollUp()
        {
            if (activeTab_ == BottomTab::TERMINAL && scrollOffset_ > 0)
                scrollOffset_--;
            else if (activeTab_ == BottomTab::OUTPUT && fileScrollOffset_ > 0)
                fileScrollOffset_--;
        }

        void scrollDown()
        {
            if (activeTab_ == BottomTab::TERMINAL)
            {
                int maxScroll = std::max(0, (int)outputLog_.size() - contentHeight());
                if (scrollOffset_ < maxScroll)
                    scrollOffset_++;
            }
            else if (activeTab_ == BottomTab::OUTPUT)
            {
                int maxScroll = std::max(0, (int)fileOutputLog_.size() - contentHeight());
                if (fileScrollOffset_ < maxScroll)
                    fileScrollOffset_++;
            }
        }

        void scrollToBottom()
        {
            scrollOffset_ = std::max(0, (int)outputLog_.size() - contentHeight());
            // Trim if too long
            while ((int)outputLog_.size() > MAX_OUTPUT_LINES)
                outputLog_.pop_front();
        }

        // ── Tab bar ──────────────────────────────────────────────────

        void renderTabBar(std::vector<Cell> &row, int w) const
        {
            for (int c = 0; c < w; c++)
            {
                row[c].ch = U' ';
                row[c].bg = tabBarBg_;
                row[c].fg = tabInactiveFg_;
                row[c].dirty = true;
            }

            std::vector<std::pair<std::string, BottomTab>> tabs = {
                {"TERMINAL", BottomTab::TERMINAL},
                {"OUTPUT", BottomTab::OUTPUT},
                {"DIAGNOSTICS", BottomTab::DIAGNOSTICS},
                {"VARIABLES", BottomTab::VARIABLES},
            };

            int col = 0;
            for (auto &[label, tab] : tabs)
            {
                bool active = (tab == activeTab_);
                Color fg = active ? tabActiveFg_ : tabInactiveFg_;
                Color bg = active ? contentBg_ : tabBarBg_;

                std::string text = " " + label + " ";
                int textLen = utf8Len(text);

                // Hover effect: highlight tab background if mouse is over it
                if (!active && hoverCol_ >= col && hoverCol_ < col + textLen)
                    bg = {40, 40, 40}; // hover bg

                // UTF-8 aware writing (labels are ASCII but future-proof)
                col = utf8Write(row, col, text, fg, bg, active);

                if (col < w)
                {
                    row[col].ch = U'│';
                    row[col].fg = borderColor_;
                    row[col].bg = tabBarBg_;
                    row[col].dirty = true;
                    col++;
                }
            }
        }

        // ── REPL content ─────────────────────────────────────────────

        void renderREPL(std::vector<std::vector<Cell>> &cells, int w, int h) const
        {
            // Use the embedded terminal
            ensureTerminalStarted();
            if (embeddedTerm_)
            {
                // Resize if needed
                int termH = h - 1; // row 0 is the tab bar
                int termW = w;
                if (termH != embeddedTerm_->rows() || termW != embeddedTerm_->cols())
                {
                    embeddedTerm_->resize(termH, termW);
                }

                // Render embedded terminal cells into our grid (starting after tab bar)
                embeddedTerm_->renderInto(cells, 1);
                return;
            }

            // Fallback: show a message
            std::string msg = "Terminal not available. Press any key to start.";
            {
                size_t si = 0;
                int c = 0;
                while (si < msg.size() && c < w)
                {
                    cells[1][c].ch = utf8Decode(msg, si);
                    cells[1][c].fg = infoColor_;
                    cells[1][c].dirty = true;
                    c++;
                }
            }
        }

        // ── Output content ───────────────────────────────────────────

        void renderOutput(std::vector<std::vector<Cell>> &cells, int w, int h) const
        {
            int contentH = h - 1; // row 0 = tabs
            int startLine = fileScrollOffset_;

            for (int r = 0; r < contentH && startLine + r < (int)fileOutputLog_.size(); r++)
            {
                const auto &line = fileOutputLog_[startLine + r];
                Color fg = outputColor_;
                if (line.kind == REPLLine::ERROR)
                    fg = errorColor_;
                else if (line.kind == REPLLine::INFO)
                    fg = infoColor_;

                // UTF-8 aware rendering
                {
                    size_t si = 0;
                    int c = 0;
                    while (si < line.text.size() && c < w)
                    {
                        char32_t cp = utf8Decode(line.text, si);
                        cells[r + 1][c].ch = cp;
                        cells[r + 1][c].fg = fg;
                        cells[r + 1][c].dirty = true;
                        c++;
                    }
                }
            }
        }

        // ── Diagnostics content ──────────────────────────────────────

        void renderDiagnostics(std::vector<std::vector<Cell>> &cells, int w, int h) const
        {
            int contentH = h - 1;

            if (diagnosticLines_.empty())
            {
                std::string msg = "No diagnostics.";
                {
                    size_t si = 0;
                    int c = 0;
                    while (si < msg.size() && c < w)
                    {
                        cells[1][c].ch = utf8Decode(msg, si);
                        cells[1][c].fg = infoColor_;
                        cells[1][c].dirty = true;
                        c++;
                    }
                }
                return;
            }

            for (int r = 0; r < contentH && r < (int)diagnosticLines_.size(); r++)
            {
                const auto &line = diagnosticLines_[r];
                Color fg = contentFg_;
                // Color success green, errors red, warnings yellow
                if (line.find("No errors found") != std::string::npos || line.find("\xE2\x9C\x93") != std::string::npos)
                    fg = {80, 200, 120}; // green
                else if (line.find("error") != std::string::npos || line.find("Error") != std::string::npos)
                    fg = errorColor_;
                else if (line.find("warning") != std::string::npos)
                    fg = {229, 192, 123}; // yellow

                {
                    size_t si = 0;
                    int c = 0;
                    while (si < line.size() && c < w)
                    {
                        cells[r + 1][c].ch = utf8Decode(line, si);
                        cells[r + 1][c].fg = fg;
                        cells[r + 1][c].dirty = true;
                        c++;
                    }
                }
            }
        }

        // ── Variables content ────────────────────────────────────────

        void renderVariables(std::vector<std::vector<Cell>> &cells, int w, int h) const
        {
            int contentH = h - 1;

            if (variables_.empty())
            {
                std::string msg1 = "No variables in scope.";
                std::string msg2 = "Use Ctrl+R to run a file and inspect variables.";
                {
                    size_t si = 0;
                    int c = 0;
                    while (si < msg1.size() && c < w)
                    {
                        cells[1][c].ch = utf8Decode(msg1, si);
                        cells[1][c].fg = infoColor_;
                        cells[1][c].dirty = true;
                        c++;
                    }
                }
                if (contentH > 1)
                {
                    size_t si = 0;
                    int c = 0;
                    while (si < msg2.size() && c < w)
                    {
                        cells[2][c].ch = utf8Decode(msg2, si);
                        cells[2][c].fg = tabInactiveFg_;
                        cells[2][c].dirty = true;
                        c++;
                    }
                }
                return;
            }

            // Header
            {
                std::string hdr = " NAME              TYPE       VALUE";
                {
                    size_t si = 0;
                    int c = 0;
                    while (si < hdr.size() && c < w)
                    {
                        cells[1][c].ch = utf8Decode(hdr, si);
                        cells[1][c].fg = tabInactiveFg_;
                        cells[1][c].bold = true;
                        cells[1][c].dirty = true;
                        c++;
                    }
                }
            }

            // Variables
            for (int r = 0; r < (int)variables_.size() && r + 2 < contentH; r++)
            {
                const auto &v = variables_[r];
                int col = 1;

                // Name (18 chars)
                {
                    size_t si = 0;
                    int count = 0;
                    while (si < v.name.size() && col < w && count < 18)
                    {
                        cells[r + 2][col].ch = utf8Decode(v.name, si);
                        cells[r + 2][col].fg = varNameColor_;
                        cells[r + 2][col].dirty = true;
                        col++;
                        count++;
                    }
                }
                while (col < 19 && col < w)
                {
                    cells[r + 2][col].dirty = true;
                    col++;
                }

                // Type (10 chars)
                {
                    size_t si = 0;
                    int count = 0;
                    while (si < v.type.size() && col < w && count < 10)
                    {
                        cells[r + 2][col].ch = utf8Decode(v.type, si);
                        cells[r + 2][col].fg = varTypeColor_;
                        cells[r + 2][col].dirty = true;
                        col++;
                        count++;
                    }
                }
                while (col < 30 && col < w)
                {
                    cells[r + 2][col].dirty = true;
                    col++;
                }

                // Value
                {
                    size_t si = 0;
                    while (si < v.value.size() && col < w)
                    {
                        cells[r + 2][col].ch = utf8Decode(v.value, si);
                        cells[r + 2][col].fg = varValueColor_;
                        cells[r + 2][col].dirty = true;
                        col++;
                    }
                }
            }
        }
    };

} // namespace xterm
