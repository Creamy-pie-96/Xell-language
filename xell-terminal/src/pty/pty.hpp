#pragma once

// =============================================================================
// pty.hpp — Unified PTY (Pseudo Terminal) interface
// =============================================================================
// Platform-specific implementations in pty_unix.cpp and pty_win.cpp.
// The PTY spawns a child process (Xell shell or any program) and provides
// read/write access to its terminal I/O.
// =============================================================================

#include <string>
#include <functional>

namespace xterm
{

    class PTY
    {
    public:
        PTY() = default;
        ~PTY();

        // Non-copyable, non-movable
        PTY(const PTY &) = delete;
        PTY &operator=(const PTY &) = delete;

        /// Spawn a child process in the PTY.
        /// @param shell_path  Path to the shell executable (e.g., "/usr/local/bin/xell")
        /// @param rows        Initial terminal rows
        /// @param cols        Initial terminal columns
        /// @param args        Optional arguments passed to the shell
        /// @return true on success
        bool spawn(const std::string &shell_path, int rows, int cols,
                   const std::vector<std::string> &args = {});

        /// Write data to the PTY (sends keystrokes to the child process).
        void write(const std::string &data);

        /// Non-blocking read from PTY. Returns empty string if nothing available.
        std::string read();

        /// Notify the PTY of a terminal resize.
        void resize(int rows, int cols);

        /// Check if the child process is still alive.
        bool is_alive() const;

        /// Kill the child process.
        void kill();

    private:
        // Platform-specific handles — defined in the .cpp files
#ifdef _WIN32
        void *hPC_ = nullptr;      // HPCON
        void *hPipeIn_ = nullptr;  // HANDLE — write end
        void *hPipeOut_ = nullptr; // HANDLE — read end
        void *hProcess_ = nullptr; // HANDLE — child process
#else
        int master_fd_ = -1;
        int child_pid_ = -1;
#endif
    };

} // namespace xterm
