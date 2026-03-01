// =============================================================================
// pty_unix.cpp — Unix PTY implementation using forkpty()
// =============================================================================
// Uses POSIX forkpty() to create a pseudo-terminal pair, fork the child
// process, and exec the shell. The parent reads/writes via master_fd.
// =============================================================================

#ifndef _WIN32

#include "pty.hpp"
#include <unistd.h>
#ifdef __APPLE__
#include <util.h> // macOS forkpty()
#else
#include <pty.h> // Linux forkpty()
#endif
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <fcntl.h>
#include <cstring>
#include <cerrno>
#include <vector>

namespace xterm
{

    PTY::~PTY()
    {
        kill();
    }

    bool PTY::spawn(const std::string &shell_path, int rows, int cols,
                    const std::vector<std::string> &args)
    {
        // Set up the initial window size
        struct winsize ws;
        std::memset(&ws, 0, sizeof(ws));
        ws.ws_row = static_cast<unsigned short>(rows);
        ws.ws_col = static_cast<unsigned short>(cols);

        // forkpty creates a PTY pair and forks:
        //   - In the child: stdin/stdout/stderr are wired to the slave PTY
        //   - In the parent: master_fd_ can be used to read/write to the child
        pid_t pid = forkpty(&master_fd_, nullptr, nullptr, &ws);

        if (pid < 0)
        {
            // fork failed
            return false;
        }

        if (pid == 0)
        {
            // ---- CHILD PROCESS ----

            // Build argv array for execvp
            std::vector<const char *> argv;
            argv.push_back(shell_path.c_str());
            for (const auto &arg : args)
                argv.push_back(arg.c_str());
            argv.push_back(nullptr);

            // Set TERM so programs know we support basic VT100
            setenv("TERM", "xterm-256color", 1);

            // Tell the Xell REPL it's running inside the Xell Terminal.
            // This enables shell-like behavior: user@host:cwd prompt,
            // single-Enter execution, etc.
            setenv("XELL_TERMINAL", "1", 1);

            // Provide COLORTERM for true-color support detection
            setenv("COLORTERM", "truecolor", 1);

            // Replace child process with the shell
            execvp(shell_path.c_str(), const_cast<char *const *>(argv.data()));

            // If execvp returns, something went wrong
            _exit(127);
        }

        // ---- PARENT PROCESS ----
        child_pid_ = pid;

        // Set master_fd to non-blocking so read() doesn't hang the main loop
        int flags = fcntl(master_fd_, F_GETFL);
        if (flags != -1)
            fcntl(master_fd_, F_SETFL, flags | O_NONBLOCK);

        return true;
    }

    void PTY::write(const std::string &data)
    {
        if (master_fd_ < 0)
            return;

        // Write all bytes (handle partial writes)
        size_t written = 0;
        while (written < data.size())
        {
            ssize_t n = ::write(master_fd_, data.data() + written, data.size() - written);
            if (n < 0)
            {
                if (errno == EAGAIN || errno == EINTR)
                    continue;
                break; // pipe broken or error
            }
            written += static_cast<size_t>(n);
        }
    }

    std::string PTY::read()
    {
        if (master_fd_ < 0)
            return {};

        char buf[4096];
        std::string result;

        // Non-blocking: read as much as available
        while (true)
        {
            ssize_t n = ::read(master_fd_, buf, sizeof(buf));
            if (n > 0)
            {
                result.append(buf, static_cast<size_t>(n));
            }
            else if (n == 0)
            {
                // EOF — child closed its side
                break;
            }
            else
            {
                // n < 0
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break; // nothing available right now
                if (errno == EINTR)
                    continue;
                break; // real error
            }
        }

        return result;
    }

    void PTY::resize(int rows, int cols)
    {
        if (master_fd_ < 0)
            return;

        struct winsize ws;
        std::memset(&ws, 0, sizeof(ws));
        ws.ws_row = static_cast<unsigned short>(rows);
        ws.ws_col = static_cast<unsigned short>(cols);

        // TIOCSWINSZ tells the kernel to update the PTY size
        // This also sends SIGWINCH to the child process
        ioctl(master_fd_, TIOCSWINSZ, &ws);
    }

    bool PTY::is_alive() const
    {
        if (child_pid_ <= 0)
            return false;

        // waitpid with WNOHANG: check without blocking
        int status;
        pid_t result = waitpid(child_pid_, &status, WNOHANG);
        if (result == 0)
            return true; // still running
        return false;    // exited or error
    }

    void PTY::kill()
    {
        if (child_pid_ > 0)
        {
            ::kill(child_pid_, SIGTERM);

            // Give it a moment, then force kill
            usleep(50000); // 50ms
            int status;
            if (waitpid(child_pid_, &status, WNOHANG) == 0)
            {
                ::kill(child_pid_, SIGKILL);
                waitpid(child_pid_, &status, 0);
            }
            child_pid_ = -1;
        }

        if (master_fd_ >= 0)
        {
            close(master_fd_);
            master_fd_ = -1;
        }
    }

} // namespace xterm

#endif // !_WIN32
