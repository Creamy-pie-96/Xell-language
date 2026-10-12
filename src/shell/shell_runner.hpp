#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <cstring>
#include <cerrno>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <time.h>
#include "../lib/errors/error.hpp"

extern char **environ;

namespace xell
{
    // UTF-8 lossy decoder replacing invalid byte sequences with U+FFFD
    inline std::string decodeUtf8Lossy(const std::string &bytes)
    {
        std::string out;
        out.reserve(bytes.size());
        size_t i = 0;
        const size_t n = bytes.size();

        while (i < n)
        {
            unsigned char c = static_cast<unsigned char>(bytes[i]);
            if (c < 0x80)
            {
                out += static_cast<char>(c);
                i++;
            }
            else if ((c >> 5) == 0x06) // 110xxxxx: 2 bytes
            {
                if (i + 1 < n &&
                    (static_cast<unsigned char>(bytes[i + 1]) >> 6) == 0x02 &&
                    c >= 0xC2)
                {
                    out += bytes[i];
                    out += bytes[i + 1];
                    i += 2;
                }
                else
                {
                    out += "\xEF\xBF\xBD";
                    i++;
                }
            }
            else if ((c >> 4) == 0x0E) // 1110xxxx: 3 bytes
            {
                if (i + 2 < n &&
                    (static_cast<unsigned char>(bytes[i + 1]) >> 6) == 0x02 &&
                    (static_cast<unsigned char>(bytes[i + 2]) >> 6) == 0x02)
                {
                    unsigned char c2 = static_cast<unsigned char>(bytes[i + 1]);
                    bool valid = true;
                    if (c == 0xE0 && c2 < 0xA0) valid = false; // overlong
                    if (c == 0xED && c2 >= 0xA0) valid = false; // surrogate (0xD800..0xDFFF)
                    if (valid)
                    {
                        out += bytes[i];
                        out += bytes[i + 1];
                        out += bytes[i + 2];
                        i += 3;
                    }
                    else
                    {
                        out += "\xEF\xBF\xBD";
                        i++;
                    }
                }
                else
                {
                    out += "\xEF\xBF\xBD";
                    i++;
                }
            }
            else if ((c >> 3) == 0x1E) // 11110xxx: 4 bytes
            {
                if (i + 3 < n &&
                    (static_cast<unsigned char>(bytes[i + 1]) >> 6) == 0x02 &&
                    (static_cast<unsigned char>(bytes[i + 2]) >> 6) == 0x02 &&
                    (static_cast<unsigned char>(bytes[i + 3]) >> 6) == 0x02)
                {
                    unsigned char c2 = static_cast<unsigned char>(bytes[i + 1]);
                    bool valid = true;
                    if (c == 0xF0 && c2 < 0x90) valid = false; // overlong
                    if (c == 0xF4 && c2 > 0x8F) valid = false; // > 0x10FFFF
                    if (c > 0xF4) valid = false;
                    if (valid)
                    {
                        out += bytes[i];
                        out += bytes[i + 1];
                        out += bytes[i + 2];
                        out += bytes[i + 3];
                        i += 4;
                    }
                    else
                    {
                        out += "\xEF\xBF\xBD";
                        i++;
                    }
                }
                else
                {
                    out += "\xEF\xBF\xBD";
                    i++;
                }
            }
            else
            {
                out += "\xEF\xBF\xBD";
                i++;
            }
        }
        return out;
    }

    struct ProcessOptions
    {
        std::string stdinMode = "closed"; // "inherit" or "closed"
        bool captureStdout = true;
        bool captureStderr = true;
        double timeoutSeconds = 0.0;     // 0 = no timeout
        size_t maxOutputBytes = 16 * 1024 * 1024; // 0 = unlimited, default 16 MiB
        std::string cwd = "";
        std::unordered_map<std::string, std::string> env;
    };

    struct ProcessExecutionResult
    {
        std::string stdoutStr;
        std::string stderrStr;
        int exitCode = 0;
        int signalNum = 0;
        bool timedOut = false;
        bool outputLimitExceeded = false;
        bool success = false;
    };

    class ShellRunner
    {
    private:
        static void killProcessGroup(pid_t pid, int &status)
        {
            // Send SIGTERM to entire process group (-pid)
            kill(-pid, SIGTERM);

            // 2.0s grace period
            auto graceStart = std::chrono::steady_clock::now();
            bool reaped = false;
            while (std::chrono::duration<double>(std::chrono::steady_clock::now() - graceStart).count() < 2.0)
            {
                pid_t r = waitpid(pid, &status, WNOHANG);
                if (r > 0)
                {
                    reaped = true;
                    break;
                }
                struct timespec ts = {0, 10000000}; // 10ms
                nanosleep(&ts, nullptr);
            }

            if (!reaped)
            {
                kill(-pid, SIGKILL);
                waitpid(pid, &status, 0);
            }
        }

    public:
        static ProcessExecutionResult run(const std::vector<std::string> &argv,
                                          const ProcessOptions &opts,
                                          int line = 0)
        {
            if (argv.empty())
            {
                throw ProcessError("Cannot execute empty command", line);
            }

            if (!opts.cwd.empty())
            {
                if (access(opts.cwd.c_str(), F_OK) != 0)
                {
                    throw ProcessError("cwd directory does not exist: " + opts.cwd, line);
                }
            }

            int stdoutPipe[2] = {-1, -1};
            int stderrPipe[2] = {-1, -1};
            int errPipe[2] = {-1, -1};

            if (opts.captureStdout)
            {
                if (pipe(stdoutPipe) != 0)
                    throw ProcessError("Failed to create stdout pipe: " + std::string(strerror(errno)), line);
                fcntl(stdoutPipe[0], F_SETFL, O_NONBLOCK);
            }

            if (opts.captureStderr)
            {
                if (pipe(stderrPipe) != 0)
                {
                    if (opts.captureStdout) { close(stdoutPipe[0]); close(stdoutPipe[1]); }
                    throw ProcessError("Failed to create stderr pipe: " + std::string(strerror(errno)), line);
                }
                fcntl(stderrPipe[0], F_SETFL, O_NONBLOCK);
            }

            if (pipe(errPipe) != 0)
            {
                if (opts.captureStdout) { close(stdoutPipe[0]); close(stdoutPipe[1]); }
                if (opts.captureStderr) { close(stderrPipe[0]); close(stderrPipe[1]); }
                throw ProcessError("Failed to create error pipe: " + std::string(strerror(errno)), line);
            }
            fcntl(errPipe[1], F_SETFD, FD_CLOEXEC);

            // Prepare environment
            std::unordered_map<std::string, std::string> mergedEnv;
            if (environ)
            {
                for (char **e = environ; *e != nullptr; ++e)
                {
                    std::string entry(*e);
                    size_t eq = entry.find('=');
                    if (eq != std::string::npos)
                    {
                        mergedEnv[entry.substr(0, eq)] = entry.substr(eq + 1);
                    }
                }
            }
            for (const auto &[k, v] : opts.env)
            {
                mergedEnv[k] = v;
            }

            std::vector<std::string> envStrings;
            std::vector<char *> envp;
            envStrings.reserve(mergedEnv.size());
            envp.reserve(mergedEnv.size() + 1);
            for (const auto &[k, v] : mergedEnv)
            {
                envStrings.push_back(k + "=" + v);
                envp.push_back(const_cast<char *>(envStrings.back().c_str()));
            }
            envp.push_back(nullptr);

            // Resolve executable path via PATH if it doesn't contain a slash
            std::string executablePath = argv[0];
            if (executablePath.find('/') == std::string::npos)
            {
                const char *pathEnv = std::getenv("PATH");
                if (pathEnv)
                {
                    std::string paths(pathEnv);
                    size_t pos = 0;
                    while (pos < paths.size())
                    {
                        size_t sep = paths.find(':', pos);
                        if (sep == std::string::npos)
                            sep = paths.size();
                        std::string dir = paths.substr(pos, sep - pos);
                        std::string full = dir + "/" + executablePath;
                        if (access(full.c_str(), X_OK) == 0)
                        {
                            executablePath = full;
                            break;
                        }
                        pos = sep + 1;
                    }
                }
            }

            // Prepare argv
            std::vector<char *> childArgv;
            childArgv.reserve(argv.size() + 1);
            for (const auto &arg : argv)
            {
                childArgv.push_back(const_cast<char *>(arg.c_str()));
            }
            childArgv.push_back(nullptr);

            pid_t pid = fork();
            if (pid < 0)
            {
                if (opts.captureStdout) { close(stdoutPipe[0]); close(stdoutPipe[1]); }
                if (opts.captureStderr) { close(stderrPipe[0]); close(stderrPipe[1]); }
                close(errPipe[0]); close(errPipe[1]);
                throw ProcessError("fork() failed: " + std::string(strerror(errno)), line);
            }

            if (pid == 0)
            {
                // Child process
                setsid();
                close(errPipe[0]);

                // Configure stdin
                if (opts.stdinMode == "closed")
                {
                    int nullFd = open("/dev/null", O_RDONLY);
                    if (nullFd >= 0)
                    {
                        dup2(nullFd, STDIN_FILENO);
                        close(nullFd);
                    }
                }

                // Configure stdout
                if (opts.captureStdout)
                {
                    close(stdoutPipe[0]);
                    dup2(stdoutPipe[1], STDOUT_FILENO);
                    close(stdoutPipe[1]);
                }

                // Configure stderr
                if (opts.captureStderr)
                {
                    close(stderrPipe[0]);
                    dup2(stderrPipe[1], STDERR_FILENO);
                    close(stderrPipe[1]);
                }

                // Configure cwd
                if (!opts.cwd.empty())
                {
                    if (chdir(opts.cwd.c_str()) != 0)
                    {
                        int err = errno;
                        ssize_t w = write(errPipe[1], &err, sizeof(err));
                        (void)w;
                        _exit(127);
                    }
                }

                // Execute
                execve(executablePath.c_str(), childArgv.data(), envp.data());

                int err = errno;
                ssize_t w = write(errPipe[1], &err, sizeof(err));
                (void)w;
                _exit(127);
            }

            // Parent process
            close(errPipe[1]);
            if (opts.captureStdout) close(stdoutPipe[1]);
            if (opts.captureStderr) close(stderrPipe[1]);

            // Check if execve failed
            int childErr = 0;
            ssize_t nErr = read(errPipe[0], &childErr, sizeof(childErr));
            close(errPipe[0]);
            if (nErr == sizeof(childErr))
            {
                int st = 0;
                waitpid(pid, &st, 0);
                if (opts.captureStdout) close(stdoutPipe[0]);
                if (opts.captureStderr) close(stderrPipe[0]);
                throw ProcessError("Failed to execute '" + argv[0] + "': " + std::string(strerror(childErr)), line);
            }

            std::string rawStdout;
            std::string rawStderr;
            bool timedOut = false;
            bool outputLimitExceeded = false;
            int status = 0;

            bool hasTimeout = opts.timeoutSeconds > 0.0;
            auto startTime = std::chrono::steady_clock::now();
            auto deadline = startTime + std::chrono::duration<double>(opts.timeoutSeconds);

            if (opts.captureStdout || opts.captureStderr)
            {
                pollfd pfds[2];
                int nfds = 0;
                int stdoutIdx = -1;
                int stderrIdx = -1;

                if (opts.captureStdout)
                {
                    stdoutIdx = nfds;
                    pfds[nfds].fd = stdoutPipe[0];
                    pfds[nfds].events = POLLIN | POLLHUP | POLLERR;
                    pfds[nfds].revents = 0;
                    nfds++;
                }

                if (opts.captureStderr)
                {
                    stderrIdx = nfds;
                    pfds[nfds].fd = stderrPipe[0];
                    pfds[nfds].events = POLLIN | POLLHUP | POLLERR;
                    pfds[nfds].revents = 0;
                    nfds++;
                }

                size_t totalBytes = 0;
                bool stdoutClosed = !opts.captureStdout;
                bool stderrClosed = !opts.captureStderr;
                char buf[4096];

                while (!stdoutClosed || !stderrClosed)
                {
                    int timeoutMs = -1;
                    if (hasTimeout)
                    {
                        auto now = std::chrono::steady_clock::now();
                        if (now >= deadline)
                        {
                            timedOut = true;
                            break;
                        }
                        auto remMs = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
                        timeoutMs = (remMs < 0) ? 0 : static_cast<int>(remMs);
                    }

                    int ret = poll(pfds, nfds, timeoutMs);
                    if (ret < 0)
                    {
                        if (errno == EINTR) continue;
                        break;
                    }
                    if (ret == 0)
                    {
                        timedOut = true;
                        break;
                    }

                    // Read stdout
                    if (stdoutIdx >= 0 && !stdoutClosed && (pfds[stdoutIdx].revents & (POLLIN | POLLHUP | POLLERR)))
                    {
                        while (true)
                        {
                            ssize_t n = read(pfds[stdoutIdx].fd, buf, sizeof(buf));
                            if (n > 0)
                            {
                                totalBytes += static_cast<size_t>(n);
                                rawStdout.append(buf, static_cast<size_t>(n));
                                if (opts.maxOutputBytes > 0 && totalBytes > opts.maxOutputBytes)
                                {
                                    outputLimitExceeded = true;
                                    break;
                                }
                            }
                            else if (n == 0)
                            {
                                close(pfds[stdoutIdx].fd);
                                pfds[stdoutIdx].fd = -1;
                                stdoutClosed = true;
                                break;
                            }
                            else
                            {
                                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                                close(pfds[stdoutIdx].fd);
                                pfds[stdoutIdx].fd = -1;
                                stdoutClosed = true;
                                break;
                            }
                        }
                        if (outputLimitExceeded) break;
                    }

                    // Read stderr
                    if (stderrIdx >= 0 && !stderrClosed && (pfds[stderrIdx].revents & (POLLIN | POLLHUP | POLLERR)))
                    {
                        while (true)
                        {
                            ssize_t n = read(pfds[stderrIdx].fd, buf, sizeof(buf));
                            if (n > 0)
                            {
                                totalBytes += static_cast<size_t>(n);
                                rawStderr.append(buf, static_cast<size_t>(n));
                                if (opts.maxOutputBytes > 0 && totalBytes > opts.maxOutputBytes)
                                {
                                    outputLimitExceeded = true;
                                    break;
                                }
                            }
                            else if (n == 0)
                            {
                                close(pfds[stderrIdx].fd);
                                pfds[stderrIdx].fd = -1;
                                stderrClosed = true;
                                break;
                            }
                            else
                            {
                                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                                close(pfds[stderrIdx].fd);
                                pfds[stderrIdx].fd = -1;
                                stderrClosed = true;
                                break;
                            }
                        }
                        if (outputLimitExceeded) break;
                    }
                }

                if (stdoutIdx >= 0 && !stdoutClosed)
                {
                    close(pfds[stdoutIdx].fd);
                    stdoutClosed = true;
                }
                if (stderrIdx >= 0 && !stderrClosed)
                {
                    close(pfds[stderrIdx].fd);
                    stderrClosed = true;
                }

                if (timedOut || outputLimitExceeded)
                {
                    killProcessGroup(pid, status);
                }
                else
                {
                    waitpid(pid, &status, 0);
                }
            }
            else
            {
                // Plain mode (inherited stdout/stderr)
                if (hasTimeout)
                {
                    while (true)
                    {
                        pid_t r = waitpid(pid, &status, WNOHANG);
                        if (r > 0) break;
                        if (r < 0) { if (errno == EINTR) continue; break; }

                        auto now = std::chrono::steady_clock::now();
                        if (now >= deadline)
                        {
                            timedOut = true;
                            killProcessGroup(pid, status);
                            break;
                        }
                        struct timespec ts = {0, 10000000}; // 10ms
                        nanosleep(&ts, nullptr);
                    }
                }
                else
                {
                    waitpid(pid, &status, 0);
                }
            }

            ProcessExecutionResult result;
            result.stdoutStr = decodeUtf8Lossy(rawStdout);
            result.stderrStr = decodeUtf8Lossy(rawStderr);
            result.timedOut = timedOut;
            result.outputLimitExceeded = outputLimitExceeded;

            if (WIFEXITED(status))
            {
                result.exitCode = WEXITSTATUS(status);
                result.signalNum = 0;
                result.success = (result.exitCode == 0) && !timedOut && !outputLimitExceeded;
            }
            else if (WIFSIGNALED(status))
            {
                result.signalNum = WTERMSIG(status);
                result.exitCode = 128 + result.signalNum;
                result.success = false;
            }
            else
            {
                result.exitCode = -1;
                result.signalNum = 0;
                result.success = false;
            }

            return result;
        }
    };
} // namespace xell
