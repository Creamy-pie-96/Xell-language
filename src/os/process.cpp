// =============================================================================
// process.cpp — Subprocess execution for Xell
// =============================================================================
//
// Two modes:
//   run()         — inherits stdin/stdout/stderr, returns exit code
//   run_capture() — captures stdout + stderr into strings
//
// Unix:    pipe() + fork() + dup2() + execl("/bin/sh", ...)
// Windows: CreateProcess with STARTUPINFO pipe redirection
//
// =============================================================================

#include "os.hpp"
#include "../lib/errors/error.hpp"

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <array>

#ifdef _WIN32
// ======================= WINDOWS IMPLEMENTATION =============================
#include <windows.h>

namespace xell
{
    namespace os
    {

        int run(const std::string &command)
        {
            STARTUPINFOA si;
            PROCESS_INFORMATION pi;
            ZeroMemory(&si, sizeof(si));
            si.cb = sizeof(si);
            ZeroMemory(&pi, sizeof(pi));

            // CreateProcess needs a mutable copy of the command string
            std::vector<char> cmd(command.begin(), command.end());
            cmd.push_back('\0');

            // Use cmd.exe /c to execute the command (like system())
            std::string full = "cmd.exe /c " + command;
            std::vector<char> fullCmd(full.begin(), full.end());
            fullCmd.push_back('\0');

            if (!CreateProcessA(
                    NULL,           // lpApplicationName
                    fullCmd.data(), // lpCommandLine
                    NULL,           // lpProcessAttributes
                    NULL,           // lpThreadAttributes
                    FALSE,          // bInheritHandles
                    0,              // dwCreationFlags
                    NULL,           // lpEnvironment
                    NULL,           // lpCurrentDirectory
                    &si,            // lpStartupInfo
                    &pi))           // lpProcessInformation
            {
                throw ProcessError("failed to start process: " + command, 0);
            }

            WaitForSingleObject(pi.hProcess, INFINITE);

            DWORD exitCode = 0;
            GetExitCodeProcess(pi.hProcess, &exitCode);

            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);

            return static_cast<int>(exitCode);
        }

        ProcessResult run_capture(const std::string &command)
        {
            SECURITY_ATTRIBUTES sa;
            sa.nLength = sizeof(sa);
            sa.bInheritHandle = TRUE;
            sa.lpSecurityDescriptor = NULL;

            // Create pipes for stdout
            HANDLE hStdoutRead = NULL, hStdoutWrite = NULL;
            if (!CreatePipe(&hStdoutRead, &hStdoutWrite, &sa, 0))
                throw ProcessError("failed to create stdout pipe", 0);
            SetHandleInformation(hStdoutRead, HANDLE_FLAG_INHERIT, 0);

            // Create pipes for stderr
            HANDLE hStderrRead = NULL, hStderrWrite = NULL;
            if (!CreatePipe(&hStderrRead, &hStderrWrite, &sa, 0))
            {
                CloseHandle(hStdoutRead);
                CloseHandle(hStdoutWrite);
                throw ProcessError("failed to create stderr pipe", 0);
            }
            SetHandleInformation(hStderrRead, HANDLE_FLAG_INHERIT, 0);

            STARTUPINFOA si;
            ZeroMemory(&si, sizeof(si));
            si.cb = sizeof(si);
            si.hStdOutput = hStdoutWrite;
            si.hStdError = hStderrWrite;
            si.dwFlags |= STARTF_USESTDHANDLES;

            PROCESS_INFORMATION pi;
            ZeroMemory(&pi, sizeof(pi));

            std::string full = "cmd.exe /c " + command;
            std::vector<char> fullCmd(full.begin(), full.end());
            fullCmd.push_back('\0');

            if (!CreateProcessA(NULL, fullCmd.data(), NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi))
            {
                CloseHandle(hStdoutRead);
                CloseHandle(hStdoutWrite);
                CloseHandle(hStderrRead);
                CloseHandle(hStderrWrite);
                throw ProcessError("failed to start process: " + command, 0);
            }

            // Close write ends in parent
            CloseHandle(hStdoutWrite);
            CloseHandle(hStderrWrite);

            // Read stdout
            std::string stdoutStr, stderrStr;
            char buffer[4096];
            DWORD bytesRead;

            while (ReadFile(hStdoutRead, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0)
                stdoutStr.append(buffer, bytesRead);

            while (ReadFile(hStderrRead, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0)
                stderrStr.append(buffer, bytesRead);

            WaitForSingleObject(pi.hProcess, INFINITE);

            DWORD exitCode = 0;
            GetExitCodeProcess(pi.hProcess, &exitCode);

            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            CloseHandle(hStdoutRead);
            CloseHandle(hStderrRead);

            return {static_cast<int>(exitCode), std::move(stdoutStr), std::move(stderrStr)};
        }

        int get_pid()
        {
            return static_cast<int>(GetCurrentProcessId());
        }

    } // namespace os
} // namespace xell

#else
// ======================== UNIX IMPLEMENTATION ===============================
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

namespace xell
{
    namespace os
    {

        int run(const std::string &command)
        {
            pid_t pid = fork();
            if (pid < 0)
                throw ProcessError("fork() failed for command: " + command, 0);

            if (pid == 0)
            {
                // Child process — execute via /bin/sh
                execl("/bin/sh", "sh", "-c", command.c_str(), nullptr);
                // If execl returns, it failed
                _exit(127);
            }

            // Parent — wait for child
            int status = 0;
            if (waitpid(pid, &status, 0) < 0)
                throw ProcessError("waitpid() failed for command: " + command, 0);

            if (WIFEXITED(status))
                return WEXITSTATUS(status);
            if (WIFSIGNALED(status))
                return -WTERMSIG(status); // negative = killed by signal

            return -1;
        }

        ProcessResult run_capture(const std::string &command)
        {
            // Create pipes: stdout and stderr
            int stdoutPipe[2], stderrPipe[2];

            if (pipe(stdoutPipe) != 0)
                throw ProcessError("pipe() failed for stdout", 0);
            if (pipe(stderrPipe) != 0)
            {
                close(stdoutPipe[0]);
                close(stdoutPipe[1]);
                throw ProcessError("pipe() failed for stderr", 0);
            }

            pid_t pid = fork();
            if (pid < 0)
            {
                close(stdoutPipe[0]);
                close(stdoutPipe[1]);
                close(stderrPipe[0]);
                close(stderrPipe[1]);
                throw ProcessError("fork() failed for command: " + command, 0);
            }

            if (pid == 0)
            {
                // ---- Child process ----
                // Redirect stdout to pipe
                close(stdoutPipe[0]); // close read end
                dup2(stdoutPipe[1], STDOUT_FILENO);
                close(stdoutPipe[1]);

                // Redirect stderr to pipe
                close(stderrPipe[0]); // close read end
                dup2(stderrPipe[1], STDERR_FILENO);
                close(stderrPipe[1]);

                execl("/bin/sh", "sh", "-c", command.c_str(), nullptr);
                _exit(127);
            }

            // ---- Parent process ----
            // Close write ends
            close(stdoutPipe[1]);
            close(stderrPipe[1]);

            // Read stdout
            std::string stdoutStr, stderrStr;
            char buffer[4096];
            ssize_t n;

            while ((n = read(stdoutPipe[0], buffer, sizeof(buffer))) > 0)
                stdoutStr.append(buffer, static_cast<size_t>(n));
            close(stdoutPipe[0]);

            while ((n = read(stderrPipe[0], buffer, sizeof(buffer))) > 0)
                stderrStr.append(buffer, static_cast<size_t>(n));
            close(stderrPipe[0]);

            // Wait for child
            int status = 0;
            waitpid(pid, &status, 0);

            int exitCode = 0;
            if (WIFEXITED(status))
                exitCode = WEXITSTATUS(status);
            else if (WIFSIGNALED(status))
                exitCode = -WTERMSIG(status);
            else
                exitCode = -1;

            return {exitCode, std::move(stdoutStr), std::move(stderrStr)};
        }

        int get_pid()
        {
            return static_cast<int>(getpid());
        }

    } // namespace os
} // namespace xell

#endif // _WIN32
