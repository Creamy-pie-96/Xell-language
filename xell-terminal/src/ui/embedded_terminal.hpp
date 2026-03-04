#pragma once

// =============================================================================
// embedded_terminal.hpp — Embeddable PTY terminal for the IDE bottom panel
// =============================================================================
// Wraps PTY + VTParser + ScreenBuffer into a self-contained widget that can
// be embedded as a sub-panel. Runs its own reader thread to feed PTY output
// into the VT parser.
// =============================================================================

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include "../terminal/types.hpp"
#include "../terminal/screen_buffer.hpp"
#include "../terminal/vt_parser.hpp"
#include "../terminal/input_handler.hpp"
#include "../pty/pty.hpp"

#ifdef _WIN32
#include <cstdlib> // _dupenv_s
#else
#include <unistd.h>
#include <pwd.h>
#endif

namespace xterm
{

    class EmbeddedTerminal
    {
    public:
        EmbeddedTerminal(int rows, int cols)
            : rows_(rows), cols_(cols),
              screenBuffer_(rows, cols),
              vtParser_(screenBuffer_)
        {
        }

        ~EmbeddedTerminal()
        {
            stop();
        }

        // Non-copyable
        EmbeddedTerminal(const EmbeddedTerminal &) = delete;
        EmbeddedTerminal &operator=(const EmbeddedTerminal &) = delete;

        /// Start the embedded terminal with the Xell shell
        bool start()
        {
            if (running_.load())
                return true; // already running

            std::string xellBin = findXellBinary();
            if (xellBin.empty())
                return false;

            std::vector<std::string> args = {"--terminal"};
            if (!pty_.spawn(xellBin, rows_, cols_, args))
                return false;

            running_.store(true);
            readerThread_ = std::thread([this]()
                                        { readerLoop(); });
            return true;
        }

        /// Stop the terminal
        void stop()
        {
            running_.store(false);
            if (readerThread_.joinable())
                readerThread_.join();
            pty_.kill();
        }

        bool isRunning() const { return running_.load(); }

        /// Resize the embedded terminal
        void resize(int rows, int cols)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            rows_ = rows;
            cols_ = cols;
            screenBuffer_.resize(rows, cols);
            if (running_.load())
                pty_.resize(rows, cols);
        }

        /// Send keyboard input to the PTY
        void write(const std::string &data)
        {
            if (running_.load())
                pty_.write(data);
        }

        /// Handle an SDL keyboard event — translate to PTY bytes
        void handleKeyEvent(const SDL_Event &event)
        {
            if (!running_.load())
                return;
            auto result = InputHandler::translate(event, false);
            if (!result.data.empty())
                pty_.write(result.data);
        }

        /// Handle text input
        void handleTextInput(const std::string &text)
        {
            if (running_.load())
                pty_.write(text);
        }

        /// Check if there's new data to render
        bool hasNewData()
        {
            bool expected = true;
            return hasNewData_.compare_exchange_strong(expected, false);
        }

        /// Get the current cell grid for rendering
        /// Returns a snapshot of the screen buffer
        std::vector<std::vector<Cell>> getCells() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            std::vector<std::vector<Cell>> cells(rows_);
            for (int r = 0; r < rows_; r++)
            {
                cells[r].resize(cols_);
                for (int c = 0; c < cols_; c++)
                {
                    cells[r][c] = screenBuffer_.get_cell(r, c);
                }
            }
            return cells;
        }

        /// Copy the screen buffer cells directly into an output grid
        /// starting at the given row offset
        void renderInto(std::vector<std::vector<Cell>> &grid, int startRow) const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (int r = 0; r < rows_ && startRow + r < (int)grid.size(); r++)
            {
                for (int c = 0; c < cols_ && c < (int)grid[startRow + r].size(); c++)
                {
                    grid[startRow + r][c] = screenBuffer_.get_cell(r, c);
                    grid[startRow + r][c].dirty = true;
                }
            }
        }

        /// Get cursor position
        std::pair<int, int> cursorPos() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            // ScreenBuffer stores cursor internally; we can get it from the cells
            // For now, return -1,-1 — cursor rendering is handled by the screen buffer
            return {-1, -1};
        }

        int rows() const { return rows_; }
        int cols() const { return cols_; }

    private:
        int rows_;
        int cols_;
        mutable ScreenBuffer screenBuffer_;
        VTParser vtParser_;
        PTY pty_;

        std::thread readerThread_;
        mutable std::mutex mutex_;
        std::atomic<bool> running_{false};
        std::atomic<bool> hasNewData_{false};

        void readerLoop()
        {
            while (running_.load())
            {
                std::string data = pty_.read();
                if (!data.empty())
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    vtParser_.feed(data);
                    hasNewData_.store(true);
                }
                else
                {
                    // Small sleep to avoid busy-waiting
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }

                if (!pty_.is_alive())
                {
                    // Respawn or mark as stopped
                    running_.store(false);
                    break;
                }
            }
        }

        static std::string findXellBinary()
        {
            // Try common locations for the xell binary
            std::vector<std::string> candidates = {
                "/usr/local/bin/xell",
                "/usr/bin/xell",
                "./xell",
                "../build/xell",
                "xell" // rely on PATH
            };

            // Also try relative to /proc/self/exe (Linux)
#ifndef _WIN32
            char selfPath[1024] = {0};
            ssize_t len = readlink("/proc/self/exe", selfPath, sizeof(selfPath) - 1);
            if (len > 0)
            {
                selfPath[len] = '\0';
                std::string dir(selfPath);
                auto pos = dir.rfind('/');
                if (pos != std::string::npos)
                {
                    dir = dir.substr(0, pos);
                    candidates.insert(candidates.begin(), dir + "/xell");
                }
            }
#endif

            for (const auto &c : candidates)
            {
                // For "xell" (no path), check if it's in PATH
                if (c.find('/') == std::string::npos && c.find('\\') == std::string::npos)
                {
                    // Use `which` to find it
                    std::string cmd = "which " + c + " 2>/dev/null";
                    FILE *fp = popen(cmd.c_str(), "r");
                    if (fp)
                    {
                        char buf[512] = {0};
                        if (fgets(buf, sizeof(buf), fp))
                        {
                            std::string result(buf);
                            while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
                                result.pop_back();
                            pclose(fp);
                            if (!result.empty())
                                return result;
                        }
                        else
                        {
                            pclose(fp);
                        }
                    }
                    continue;
                }
                // Check if the file exists
                if (access(c.c_str(), X_OK) == 0)
                    return c;
            }

            // Fallback: return "xell" and hope it's in PATH
            return "xell";
        }

        static std::string getDefaultShell()
        {
#ifdef _WIN32
            char *shell = nullptr;
            size_t len = 0;
            if (_dupenv_s(&shell, &len, "COMSPEC") == 0 && shell)
            {
                std::string s(shell);
                free(shell);
                return s;
            }
            return "cmd.exe";
#else
            const char *shell = std::getenv("SHELL");
            if (shell && shell[0])
                return std::string(shell);
            struct passwd *pw = getpwuid(getuid());
            if (pw && pw->pw_shell && pw->pw_shell[0])
                return std::string(pw->pw_shell);
            return "/bin/sh";
#endif
        }
    };

} // namespace xterm
