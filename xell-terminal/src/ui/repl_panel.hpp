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
#include <atomic>
#include <thread>
#include <mutex>
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
#include <sys/select.h>
#include <unistd.h>
#include <signal.h>
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
        HELP,
    };

    // ─── Shell output capture utility ────────────────────────────────────

    // Atomic PID of the currently running child process (0 if none).
    // Used by killRunningProcess() to send SIGKILL for emergency stop.
    static inline std::atomic<pid_t> runningChildPid_{0};

    static inline std::string captureCommand(const std::string &cmd, int &exitCode)
    {
        std::string result;
        // Use fork/exec so we can track PID for emergency kill
        int pipeOut[2];
        if (pipe(pipeOut) != 0)
        {
            exitCode = -1;
            return "[failed to create pipe]";
        }
        pid_t pid = fork();
        if (pid < 0)
        {
            exitCode = -1;
            return "[failed to fork]";
        }
        if (pid == 0)
        {
            // Child — create new process group so we can kill all descendants
            setsid();
            close(pipeOut[0]);
            dup2(pipeOut[1], STDOUT_FILENO);
            dup2(pipeOut[1], STDERR_FILENO);
            close(pipeOut[1]);
            execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
            _exit(127);
        }
        // Parent
        close(pipeOut[1]);
        runningChildPid_.store(pid);

        char buf[4096];
        ssize_t n;
        while ((n = read(pipeOut[0], buf, sizeof(buf))) > 0)
            result.append(buf, n);
        close(pipeOut[0]);

        int status = 0;
        waitpid(pid, &status, 0);
        runningChildPid_.store(0);
        exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

        // Strip trailing newline
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
            result.pop_back();
        return result;
    }

    /// Kill the running child process (emergency stop)
    /// Returns true if a process was actually killed.
    static inline bool killRunningProcess()
    {
        pid_t pid = runningChildPid_.load();
        if (pid > 0)
        {
            // Kill the entire session group (setsid was called in child)
            // -pid kills the process group led by pid
            kill(-pid, SIGTERM); // Try graceful first
            kill(pid, SIGTERM);
            // Give a tiny moment, then SIGKILL
            usleep(50000); // 50ms
            kill(-pid, SIGKILL);
            int ret = kill(pid, SIGKILL);
            // Reap to prevent zombie
            int status = 0;
            waitpid(pid, &status, WNOHANG);
            runningChildPid_.store(0);
            return true; // we attempted kill on a real PID
        }
        return false; // nothing was running
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

    // ─── captureCommandSplitOutput ──────────────────────────────────────
    // Like captureCommandWithStdin, but captures stdout and stderr separately.
    // Used by --check-symbols: symbols on stdout, diagnostics on stderr.
    static inline std::string captureCommandSplitOutput(
        const std::string &cmd, const std::string &input, int &exitCode,
        std::string &stdoutStr, std::string &stderrStr)
    {
        stdoutStr.clear();
        stderrStr.clear();
        int pipeIn[2], pipeOut[2], pipeErr[2];
        if (pipe(pipeIn) != 0 || pipe(pipeOut) != 0 || pipe(pipeErr) != 0)
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
            // Child
            close(pipeIn[1]);
            close(pipeOut[0]);
            close(pipeErr[0]);
            dup2(pipeIn[0], STDIN_FILENO);
            dup2(pipeOut[1], STDOUT_FILENO);
            dup2(pipeErr[1], STDERR_FILENO);
            close(pipeIn[0]);
            close(pipeOut[1]);
            close(pipeErr[1]);
            execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
            _exit(127);
        }
        // Parent
        close(pipeIn[0]);
        close(pipeOut[1]);
        close(pipeErr[1]);

        // Write input
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
        close(pipeIn[1]);

        // Read stdout and stderr (use select to avoid blocking)
        char buf[4096];
        bool outOpen = true, errOpen = true;
        while (outOpen || errOpen)
        {
            fd_set fds;
            FD_ZERO(&fds);
            int maxFd = 0;
            if (outOpen)
            {
                FD_SET(pipeOut[0], &fds);
                maxFd = std::max(maxFd, pipeOut[0]);
            }
            if (errOpen)
            {
                FD_SET(pipeErr[0], &fds);
                maxFd = std::max(maxFd, pipeErr[0]);
            }
            if (select(maxFd + 1, &fds, nullptr, nullptr, nullptr) <= 0)
                break;
            if (outOpen && FD_ISSET(pipeOut[0], &fds))
            {
                ssize_t n = read(pipeOut[0], buf, sizeof(buf));
                if (n > 0)
                    stdoutStr.append(buf, n);
                else
                    outOpen = false;
            }
            if (errOpen && FD_ISSET(pipeErr[0], &fds))
            {
                ssize_t n = read(pipeErr[0], buf, sizeof(buf));
                if (n > 0)
                    stderrStr.append(buf, n);
                else
                    errOpen = false;
            }
        }
        close(pipeOut[0]);
        close(pipeErr[0]);

        int status = 0;
        waitpid(pid, &status, 0);
        exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;

        // Strip trailing newlines
        while (!stdoutStr.empty() && (stdoutStr.back() == '\n' || stdoutStr.back() == '\r'))
            stdoutStr.pop_back();
        while (!stderrStr.empty() && (stderrStr.back() == '\n' || stderrStr.back() == '\r'))
            stderrStr.pop_back();

        return stderrStr; // return stderr for backward compat (diagnostics)
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

            // Add to history (split multiline code into individual lines)
            {
                std::istringstream lineStream(code);
                std::string singleLine;
                while (std::getline(lineStream, singleLine))
                    history_.push_back(singleLine);
            }

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
            // Each __VAR__ print is wrapped in try/catch so one failure doesn't
            // prevent others from being collected.
            std::string tmpFile = xellTempDir() + "/xell_vars_" + std::to_string(sessionId_) + ".xel";
            {
                FILE *f = fopen(tmpFile.c_str(), "w");
                if (!f)
                    return vars;
                for (auto &h : history_)
                    fprintf(f, "%s\n", h.c_str());
                // Print type and value for each variable, wrapped in try/catch
                for (auto &name : varNames)
                {
                    fprintf(f, "try:\n");
                    fprintf(f, "    print(\"__VAR__:%s:\" + str(type(%s)) + \":\" + str(%s))\n",
                            name.c_str(), name.c_str(), name.c_str());
                    fprintf(f, "catch e:\n");
                    fprintf(f, "    print(\"__VAR__:%s:?:unknown\")\n", name.c_str());
                    fprintf(f, ";\n");
                }
                fclose(f);
            }

            int exitCode = 0;
            // Suppress stderr so error messages don't pollute __VAR__ output
            std::string output = captureCommand(xellBin_ + " " + tmpFile + " 2>/dev/null", exitCode);

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

        ~REPLPanel()
        {
            if (execThread_.joinable())
                execThread_.join();
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
            t = (t + 1) % 5;
            activeTab_ = static_cast<BottomTab>(t);
        }

        // Clear output log
        void clearOutput()
        {
            fileOutputLog_.clear();
            fileScrollOffset_ = 0;
        }

        // ── Run selection in REPL (Ctrl+Enter) — async ────────────────

        void runCode(const std::string &code)
        {
            if (isExecuting_.load())
                return; // Already running

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
                header.text = "⏳ [Running " + std::to_string(lineCount) + " lines to cursor]";
            }
            else
            {
                header.text = "⏳ [Running: " + firstLine + "]";
            }
            header.kind = REPLLine::INFO;
            fileOutputLog_.push_back(header);

            // Switch to output tab so user sees the running indicator
            activeTab_ = BottomTab::OUTPUT;
            fileScrollOffset_ = 0;

            isExecuting_.store(true);

            // Join any previous thread
            if (execThread_.joinable())
                execThread_.join();

            execThread_ = std::thread([this, code]()
                                      {
                session_.clearHistory();
                auto result = session_.executeCode(code);
                {
                    std::lock_guard<std::mutex> lock(execMutex_);
                    pendingResult_ = result;
                    resultReady_.store(true);
                }
                isExecuting_.store(false); });
        }

        // ── Run file (Ctrl+Shift+B) — async ─────────────────────────

        void runFile(const std::string &filePath)
        {
            if (isExecuting_.load())
                return;

            fileOutputLog_.clear();

            REPLLine header;
            header.text = "⏳ [Running: " + std::filesystem::path(filePath).filename().string() + "]";
            header.kind = REPLLine::INFO;
            fileOutputLog_.push_back(header);

            // Switch to output tab so user sees the running indicator
            activeTab_ = BottomTab::OUTPUT;
            fileScrollOffset_ = 0;

            isExecuting_.store(true);

            if (execThread_.joinable())
                execThread_.join();

            execThread_ = std::thread([this, filePath]()
                                      {
                auto result = session_.runFile(filePath);
                {
                    std::lock_guard<std::mutex> lock(execMutex_);
                    pendingResult_ = result;
                    resultReady_.store(true);
                }
                isExecuting_.store(false); });
        }

        // Check if code is currently executing
        bool isExecuting() const { return isExecuting_.load(); }

        // Poll for async execution results (called from tick or render)
        void pollAsyncResult()
        {
            if (!resultReady_.load())
                return;

            REPLSession::ExecResult result;
            {
                std::lock_guard<std::mutex> lock(execMutex_);
                result = std::move(pendingResult_);
                resultReady_.store(false);
            }

            // Update header to remove spinner
            if (!fileOutputLog_.empty() && fileOutputLog_[0].kind == REPLLine::INFO)
            {
                auto &hdr = fileOutputLog_[0].text;
                // ⏳ is 3 bytes in UTF-8: e2 8f b3
                if (hdr.size() > 3 && hdr.substr(0, 3) == "\xe2\x8f\xb3")
                {
                    std::string rest = hdr.substr(3); // " [Running ...]"
                    if (result.exitCode == 0)
                        hdr = "\xe2\x9c\x93" + rest; // ✓
                    else
                        hdr = "\xe2\x9c\x97" + rest; // ✗
                }
            }

            if (!result.output.empty())
            {
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

            // Exit code footer
            REPLLine footer;
            footer.text = "[Exit code: " + std::to_string(result.exitCode) + "]";
            footer.kind = (result.exitCode == 0) ? REPLLine::INFO : REPLLine::ERROR;
            fileOutputLog_.push_back(footer);

            variables_ = result.variables;

            // Auto-scroll to bottom
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

        // Append a single line to the output log (for emergency stop messages, etc.)
        void appendOutputLine(const std::string &text, REPLLine::Kind kind)
        {
            REPLLine line;
            line.text = text;
            line.kind = kind;
            fileOutputLog_.push_back(line);
            fileScrollOffset_ = std::max(0, (int)fileOutputLog_.size() - contentHeight());
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

        bool handleMouseClick(int row, int col, bool /*shift*/) override
        {
            // Row 0 = tab bar — handled by LayoutManager's handleBottomTabClick
            if (row == 0)
                return false; // let parent handle tab bar

            // Terminal tab scrollbar click (last column)
            if (activeTab_ == BottomTab::TERMINAL && col == rect_.w - 1 && row > 0 && embeddedTerm_)
            {
                int totalScrollback = embeddedTerm_->scrollbackSize();
                if (totalScrollback > 0)
                {
                    int termH = rect_.h - 1;
                    int clickPos = row - 1; // relative to track
                    // Map click position to scroll offset
                    // clickPos=0 → max scroll (top of scrollback), clickPos=termH-1 → scroll 0 (bottom)
                    int scrollOff = totalScrollback * (termH - 1 - clickPos) / std::max(1, termH - 1);
                    scrollOff = std::max(0, std::min(scrollOff, totalScrollback));
                    // Need to set it directly — expose via a method
                    // For now, use scrollUp/scrollDown to approximate
                    int currentOff = embeddedTerm_->scrollOffset();
                    int diff = scrollOff - currentOff;
                    if (diff > 0)
                        embeddedTerm_->scrollUp(diff);
                    else if (diff < 0)
                        embeddedTerm_->scrollDown(-diff);
                }
                return true;
            }

            // Non-terminal scrollbar click (last column)
            if (activeTab_ != BottomTab::TERMINAL && col == rect_.w - 1 && row > 0)
            {
                int totalLines = activeContentLines();
                int visibleH = contentHeight();
                if (totalLines > visibleH)
                {
                    int trackH = rect_.h - 1;
                    int clickPos = row - 1; // relative to track
                    int maxScroll = std::max(1, totalLines - visibleH);
                    int &offset = activeScrollOffset();
                    offset = clickPos * maxScroll / std::max(1, trackH);
                    offset = std::max(0, std::min(offset, maxScroll));
                }
                return true;
            }

            // Click in content area — just consume the event
            return true;
        }

        bool handleMouseWheel(int delta) override
        {
            if (activeTab_ == BottomTab::TERMINAL)
            {
                // Forward scroll to embedded terminal scrollback
                if (embeddedTerm_ && embeddedTerm_->isRunning())
                {
                    if (delta > 0)
                        embeddedTerm_->scrollUp(delta * 3);
                    else
                        embeddedTerm_->scrollDown(-delta * 3);
                }
                return true;
            }
            // All other tabs use the generic scroll system
            int &offset = activeScrollOffset();
            offset -= delta * 3;
            offset = std::max(0, offset);
            int maxScroll = std::max(0, activeContentLines() - contentHeight());
            offset = std::min(offset, maxScroll);
            return true;
        }

        // Handle scrollbar drag (called from layout manager on mouse motion with button held)
        void handleScrollbarDrag(int localRow)
        {
            if (localRow <= 0)
                return;

            if (activeTab_ == BottomTab::TERMINAL)
            {
                if (embeddedTerm_)
                {
                    int totalScrollback = embeddedTerm_->scrollbackSize();
                    if (totalScrollback > 0)
                    {
                        int termH = rect_.h - 1;
                        int clickPos = localRow - 1;
                        int scrollOff = totalScrollback * (termH - 1 - clickPos) / std::max(1, termH - 1);
                        scrollOff = std::max(0, std::min(scrollOff, totalScrollback));
                        int currentOff = embeddedTerm_->scrollOffset();
                        int diff = scrollOff - currentOff;
                        if (diff > 0)
                            embeddedTerm_->scrollUp(diff);
                        else if (diff < 0)
                            embeddedTerm_->scrollDown(-diff);
                    }
                }
            }
            else
            {
                int totalLines = activeContentLines();
                int visibleH = contentHeight();
                if (totalLines > visibleH)
                {
                    int trackH = rect_.h - 1;
                    int clickPos = localRow - 1;
                    int maxScroll = std::max(1, totalLines - visibleH);
                    int &offset = activeScrollOffset();
                    offset = clickPos * maxScroll / std::max(1, trackH);
                    offset = std::max(0, std::min(offset, maxScroll));
                }
            }
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

            // Ctrl+L — clear output when on OUTPUT tab
            if (ctrl && key.sym == SDLK_l && activeTab_ == BottomTab::OUTPUT)
            {
                clearOutput();
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
            case BottomTab::HELP:
                renderHelp(cells, w, h);
                break;
            }

            // Scrollbar for non-terminal tabs
            if (activeTab_ != BottomTab::TERMINAL)
                renderScrollbar(cells, w, h);

            // Scrollbar for terminal (based on scrollback)
            if (activeTab_ == BottomTab::TERMINAL && embeddedTerm_)
                renderTerminalScrollbar(cells, w, h);

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
        mutable int varScrollOffset_ = 0;

        // Async execution state
        mutable std::thread execThread_;
        mutable std::mutex execMutex_;
        mutable std::atomic<bool> isExecuting_{false};
        mutable std::atomic<bool> resultReady_{false};
        mutable REPLSession::ExecResult pendingResult_;

        // Diagnostics
        mutable std::vector<std::string> diagnosticLines_;
        mutable int diagScrollOffset_ = 0;

        // Help tab scroll
        mutable int helpScrollOffset_ = 0;

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

        int contentHeight() const { return std::max(1, rect_.h - 1); } // -1 for tab bar

        // Get the current scroll offset and max scroll for the active tab
        int &activeScrollOffset() const
        {
            switch (activeTab_)
            {
            case BottomTab::TERMINAL:
                return scrollOffset_;
            case BottomTab::OUTPUT:
                return fileScrollOffset_;
            case BottomTab::DIAGNOSTICS:
                return diagScrollOffset_;
            case BottomTab::VARIABLES:
                return varScrollOffset_;
            case BottomTab::HELP:
                return helpScrollOffset_;
            default:
                return scrollOffset_;
            }
        }

        int activeContentLines() const
        {
            switch (activeTab_)
            {
            case BottomTab::OUTPUT:
                return (int)fileOutputLog_.size();
            case BottomTab::DIAGNOSTICS:
                return (int)diagnosticLines_.size();
            case BottomTab::VARIABLES:
                return variables_.empty() ? 2 : (int)variables_.size() + 1; // +1 for header
            case BottomTab::HELP:
                return 30; // approximate help lines
            default:
                return 0; // TERMINAL uses embedded terminal scrollback
            }
        }

        void scrollUp()
        {
            int &offset = activeScrollOffset();
            if (offset > 0)
                offset--;
        }

        void scrollDown()
        {
            int &offset = activeScrollOffset();
            int maxScroll = std::max(0, activeContentLines() - contentHeight());
            if (offset < maxScroll)
                offset++;
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
                {"HELP", BottomTab::HELP},
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

            for (int r = 0; r < contentH && diagScrollOffset_ + r < (int)diagnosticLines_.size(); r++)
            {
                const auto &line = diagnosticLines_[diagScrollOffset_ + r];
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
            int varStart = varScrollOffset_;
            for (int r = 0; r < (int)variables_.size() - varStart && r + 2 < contentH; r++)
            {
                const auto &v = variables_[varStart + r];
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

        // ── Help content ─────────────────────────────────────────────

        void renderHelp(std::vector<std::vector<Cell>> &cells, int w, int h) const
        {
            static const std::vector<std::pair<std::string, std::string>> helpItems = {
                {"Ctrl+Enter", "Run selection, or top-to-cursor if none"},
                {"Ctrl+R", "Run the current file"},
                {"Ctrl+Shift+K/Q", "Emergency stop running program"},
                {"Ctrl+S", "Save current file"},
                {"Ctrl+N", "New file"},
                {"Ctrl+W", "Close current tab"},
                {"Ctrl+B", "Toggle sidebar"},
                {"Ctrl+T", "Switch to terminal mode"},
                {"Ctrl+F", "Find (regex)"},
                {"Ctrl+H", "Find & Replace (regex)"},
                {"Ctrl+G", "Go to line number"},
                {"Ctrl+Tab", "Next editor tab"},
                {"Ctrl+Shift+Tab", "Previous editor tab"},
                {"Ctrl+L", "Clear output (in OUTPUT tab)"},
                {"Tab", "Cycle bottom panel tabs"},
                {"F3 / Shift+F3", "Next / Previous match (in Find)"},
                {"Escape", "Close dialog / cancel"},
                {"↑/↓", "Scroll content"},
                {"Mouse wheel", "Scroll content"},
                {"Click scrollbar", "Jump to position"},
                {"Double-click border", "Toggle panel size"},
                {"Right-click", "Context menu"},
                {"", ""},
                {":ide", "Switch to IDE mode"},
                {":terminal", "Switch to terminal mode"},
                {":help", "Show help in terminal mode"},
                {":quit", "Exit Xell"},
            };

            int contentH = h - 1;
            Color headerFg = {86, 156, 214}; // blue
            Color keyFg = {206, 145, 120};   // orange
            Color descFg = {204, 204, 204};  // gray

            // Title
            {
                std::string title = " Xell IDE - Keyboard Shortcuts";
                size_t si = 0;
                int c = 0;
                while (si < title.size() && c < w)
                {
                    cells[1][c].ch = utf8Decode(title, si);
                    cells[1][c].fg = headerFg;
                    cells[1][c].bold = true;
                    cells[1][c].dirty = true;
                    c++;
                }
            }

            // Separator
            if (contentH > 1)
            {
                for (int c = 1; c < w - 1 && c < 50; c++)
                {
                    cells[2][c].ch = U'─';
                    cells[2][c].fg = {51, 51, 51};
                    cells[2][c].dirty = true;
                }
            }

            // Help entries
            int startIdx = helpScrollOffset_;
            for (int i = 0; i < contentH - 2 && startIdx + i < (int)helpItems.size(); i++)
            {
                const auto &[key, desc] = helpItems[startIdx + i];
                int row = i + 3; // skip title + separator + tab bar
                if (row >= h)
                    break;

                int col = 1;
                // Key (padded to 20 chars)
                {
                    size_t si = 0;
                    int count = 0;
                    while (si < key.size() && col < w && count < 20)
                    {
                        cells[row][col].ch = utf8Decode(key, si);
                        cells[row][col].fg = keyFg;
                        cells[row][col].dirty = true;
                        col++;
                        count++;
                    }
                }
                while (col < 22 && col < w)
                {
                    cells[row][col].dirty = true;
                    col++;
                }

                // Description
                {
                    size_t si = 0;
                    while (si < desc.size() && col < w)
                    {
                        cells[row][col].ch = utf8Decode(desc, si);
                        cells[row][col].fg = descFg;
                        cells[row][col].dirty = true;
                        col++;
                    }
                }
            }
        }

        // ── Scrollbar for bottom panel ───────────────────────────────

        void renderTerminalScrollbar(std::vector<std::vector<Cell>> &cells, int w, int h) const
        {
            if (!embeddedTerm_)
                return;

            int scrollOff = embeddedTerm_->scrollOffset();
            int totalScrollback = embeddedTerm_->scrollbackSize();
            int termH = h - 1; // exclude tab bar row

            if (termH < 2 || w < 2)
                return;

            // Only show scrollbar if there's scrollback content
            if (totalScrollback <= 0)
                return;

            int scrollCol = w - 1;
            int totalLines = totalScrollback + termH; // total virtual height
            int maxScroll = std::max(1, totalScrollback);

            // Proportional thumb
            int thumbH = std::max(1, termH * termH / totalLines);
            // Position: when scrollOff=0 (bottom), thumb is at bottom; when scrollOff=max, at top
            int thumbTop = 0;
            if (maxScroll > 0)
                thumbTop = (maxScroll - scrollOff) * (termH - thumbH) / maxScroll;
            thumbTop = std::clamp(thumbTop, 0, termH - thumbH);

            for (int r = 0; r < termH; r++)
            {
                int row = r + 1;
                if (row >= h || scrollCol >= (int)cells[row].size())
                    break;
                bool isThumb = (r >= thumbTop && r < thumbTop + thumbH);
                cells[row][scrollCol].ch = isThumb ? U'█' : U'░';
                cells[row][scrollCol].fg = isThumb ? Color{120, 120, 120} : Color{50, 50, 50};
                cells[row][scrollCol].bg = contentBg_;
                cells[row][scrollCol].dirty = true;
            }
        }

        void renderScrollbar(std::vector<std::vector<Cell>> &cells, int w, int h) const
        {
            int totalLines = activeContentLines();
            int visibleH = contentHeight();
            if (totalLines <= visibleH || visibleH < 1 || w < 2)
                return; // no scrollbar needed

            int scrollCol = w - 1;
            int trackH = h - 1; // exclude tab bar
            if (trackH < 2)
                return;

            int offset = activeScrollOffset();
            int maxScroll = std::max(1, totalLines - visibleH);

            // Thumb size and position
            int thumbH = std::max(1, trackH * visibleH / totalLines);
            int thumbStart = (trackH - thumbH) * offset / maxScroll;

            for (int r = 0; r < trackH; r++)
            {
                int row = r + 1; // skip tab bar
                if (row >= h)
                    break;
                bool isThumb = (r >= thumbStart && r < thumbStart + thumbH);
                cells[row][scrollCol].ch = isThumb ? U'█' : U'░';
                cells[row][scrollCol].fg = isThumb ? Color{120, 120, 120} : Color{50, 50, 50};
                cells[row][scrollCol].bg = contentBg_;
                cells[row][scrollCol].dirty = true;
            }
        }
    };

} // namespace xterm
