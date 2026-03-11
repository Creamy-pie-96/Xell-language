#pragma once

// ─── Debug IPC — Cross-Platform Inter-Process Communication ──────────────
//
// A dedicated channel between the IDE (xell-terminal) and the interpreter
// for debug control (step, continue, breakpoint add/remove, state push).
//
// Protocol: newline-delimited JSON messages.
//
// Linux/macOS:  Unix domain socket  /tmp/xell-debug-{pid}.sock
// Windows:      Named pipe          \\.\pipe\xell-debug-{pid}
//
// IDE → Interpreter:
//   {"cmd":"step_over"}
//   {"cmd":"step_into"}
//   {"cmd":"step_out"}
//   {"cmd":"continue"}
//   {"cmd":"stop"}
//   {"cmd":"add_breakpoint","line":25,"type":"snapshot"}
//   {"cmd":"add_breakpoint","line":30,"type":"pause"}
//   {"cmd":"remove_breakpoint","line":25}
//   {"cmd":"add_watch","expr":"x > 100"}
//   {"cmd":"remove_watch","expr":"x > 100"}
//   {"cmd":"jump_to","sequence":42}
//   {"cmd":"eval","expr":"x + y"}
//
// Interpreter → IDE:
//   {"state":"paused","line":25,"seq":42,"depth":3,"vars":{...},"callStack":[...]}
//   {"state":"running"}
//   {"state":"finished","exitCode":0}
//   {"event":"breakpoint_hit","name":"epoch_start","line":10,"seq":42}
//   {"event":"watch_triggered","expr":"x > 100","line":15,"seq":55}
//   {"event":"var_changed","name":"x","type":"int","value":"42","line":5}
//   {"event":"error","message":"...","line":10}
//
// Usage:
//   Interpreter side: DebugIPC server;
//                     server.listen(getpid());
//                     // blocks until IDE connects, then recv/send
//
//   IDE side:         DebugIPC client;
//                     client.connect(childPid);
//                     client.send("{\"cmd\":\"continue\"}\n");
//

#include <string>
#include <functional>
#include <atomic>
#include <sstream>
#include <cstring>
#include <cerrno>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#endif

namespace xell
{

    // ─── Debug Commands (IDE → Interpreter) ──────────────────────────────

    enum class DebugCmd
    {
        Continue, // Run until next breakpoint or end
        StepOver, // Execute one statement, skip function internals
        StepInto, // Pause at first line of called function
        StepOut,  // Continue until current function returns
        Stop,     // Abort execution
        AddBreakpoint,
        RemoveBreakpoint,
        AddWatch,
        RemoveWatch,
        JumpTo, // Time travel: jump to sequence number
        Eval,   // Evaluate expression in current scope
        Unknown
    };

    struct DebugMessage
    {
        DebugCmd cmd = DebugCmd::Unknown;
        int line = -1;
        int sequence = -1;
        int depth = -1;
        std::string type; // "snapshot", "pause"
        std::string expr; // watch/eval expression
        std::string name; // breakpoint name
        std::string raw;  // raw JSON string
    };

    // ─── IPC Channel ─────────────────────────────────────────────────────

    class DebugIPC
    {
    public:
        DebugIPC() = default;
        ~DebugIPC() { close(); }

        // Non-copyable, movable
        DebugIPC(const DebugIPC &) = delete;
        DebugIPC &operator=(const DebugIPC &) = delete;
        DebugIPC(DebugIPC &&other) noexcept { swap(other); }
        DebugIPC &operator=(DebugIPC &&other) noexcept
        {
            close();
            swap(other);
            return *this;
        }

        // ── Server side (Interpreter) ────────────────────────────────

        // Create the socket/pipe and wait for the IDE to connect.
        // Returns true on success.
        bool listen(int pid)
        {
            pid_ = pid;
            isServer_ = true;
            path_ = socketPath(pid);

#ifdef _WIN32
            return listenWindows();
#else
            return listenUnix();
#endif
        }

        // ── Client side (IDE) ────────────────────────────────────────

        // Connect to the interpreter's debug channel.
        // Returns true on success.
        bool connect(int pid, int timeoutMs = 5000)
        {
            pid_ = pid;
            isServer_ = false;
            path_ = socketPath(pid);

#ifdef _WIN32
            return connectWindows(timeoutMs);
#else
            return connectUnix(timeoutMs);
#endif
        }

        // ── Send/Receive ─────────────────────────────────────────────

        // Send a newline-delimited JSON message.
        bool send(const std::string &json)
        {
            std::string msg = json;
            if (msg.empty() || msg.back() != '\n')
                msg += '\n';

#ifdef _WIN32
            return sendWindows(msg);
#else
            return sendUnix(msg);
#endif
        }

        // Receive one newline-delimited JSON message (blocking).
        // Returns empty string on error/disconnect.
        std::string recv()
        {
            // Check buffer first for a complete line
            auto nlPos = recvBuffer_.find('\n');
            if (nlPos != std::string::npos)
            {
                std::string line = recvBuffer_.substr(0, nlPos);
                recvBuffer_.erase(0, nlPos + 1);
                return line;
            }

#ifdef _WIN32
            return recvWindows();
#else
            return recvUnix();
#endif
        }

        // Non-blocking check if data is available (within timeoutMs).
        bool poll(int timeoutMs = 0)
        {
#ifdef _WIN32
            return pollWindows(timeoutMs);
#else
            return pollUnix(timeoutMs);
#endif
        }

        // Check if connected.
        bool isConnected() const { return connected_; }

        // Get the socket path.
        const std::string &socketPathStr() const { return path_; }

        // Close the channel and clean up.
        void close()
        {
#ifdef _WIN32
            closeWindows();
#else
            closeUnix();
#endif
            connected_ = false;
        }

        // ── Helpers ──────────────────────────────────────────────────

        // Parse a received JSON string into a DebugMessage.
        static DebugMessage parseCommand(const std::string &json)
        {
            DebugMessage msg;
            msg.raw = json;

            auto getString = [&](const std::string &key) -> std::string
            {
                std::string search = "\"" + key + "\"";
                auto pos = json.find(search);
                if (pos == std::string::npos)
                    return "";
                pos = json.find(':', pos + search.size());
                if (pos == std::string::npos)
                    return "";
                pos = json.find('"', pos + 1);
                if (pos == std::string::npos)
                    return "";
                pos++;
                auto end = json.find('"', pos);
                if (end == std::string::npos)
                    return "";
                return json.substr(pos, end - pos);
            };

            auto getInt = [&](const std::string &key) -> int
            {
                std::string search = "\"" + key + "\"";
                auto pos = json.find(search);
                if (pos == std::string::npos)
                    return -1;
                pos = json.find(':', pos + search.size());
                if (pos == std::string::npos)
                    return -1;
                pos++;
                while (pos < json.size() && json[pos] == ' ')
                    pos++;
                if (pos >= json.size())
                    return -1;
                int val = 0;
                bool neg = false;
                if (json[pos] == '-')
                {
                    neg = true;
                    pos++;
                }
                while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9')
                    val = val * 10 + (json[pos++] - '0');
                return neg ? -val : val;
            };

            std::string cmd = getString("cmd");
            if (cmd == "continue")
                msg.cmd = DebugCmd::Continue;
            else if (cmd == "step_over")
                msg.cmd = DebugCmd::StepOver;
            else if (cmd == "step_into")
                msg.cmd = DebugCmd::StepInto;
            else if (cmd == "step_out")
                msg.cmd = DebugCmd::StepOut;
            else if (cmd == "stop")
                msg.cmd = DebugCmd::Stop;
            else if (cmd == "add_breakpoint")
            {
                msg.cmd = DebugCmd::AddBreakpoint;
                msg.line = getInt("line");
                msg.type = getString("type");
                msg.name = getString("name");
            }
            else if (cmd == "remove_breakpoint")
            {
                msg.cmd = DebugCmd::RemoveBreakpoint;
                msg.line = getInt("line");
            }
            else if (cmd == "add_watch")
            {
                msg.cmd = DebugCmd::AddWatch;
                msg.expr = getString("expr");
            }
            else if (cmd == "remove_watch")
            {
                msg.cmd = DebugCmd::RemoveWatch;
                msg.expr = getString("expr");
            }
            else if (cmd == "jump_to")
            {
                msg.cmd = DebugCmd::JumpTo;
                msg.sequence = getInt("sequence");
            }
            else if (cmd == "eval")
            {
                msg.cmd = DebugCmd::Eval;
                msg.expr = getString("expr");
            }

            return msg;
        }

        // Build a JSON state message (Interpreter → IDE).
        static std::string buildStateJSON(const std::string &state, int line, int seq,
                                          int depth, const std::string &varsJSON = "{}",
                                          const std::string &callStackJSON = "[]")
        {
            std::ostringstream ss;
            ss << "{\"state\":\"" << state << "\"";
            if (line >= 0)
                ss << ",\"line\":" << line;
            if (seq >= 0)
                ss << ",\"seq\":" << seq;
            if (depth >= 0)
                ss << ",\"depth\":" << depth;
            if (!varsJSON.empty() && varsJSON != "{}")
                ss << ",\"vars\":" << varsJSON;
            ss << ",\"callStack\":" << callStackJSON;
            ss << "}";
            return ss.str();
        }

        // Build an event message (Interpreter → IDE).
        static std::string buildEventJSON(const std::string &event,
                                          const std::string &name = "",
                                          int line = -1, int seq = -1,
                                          const std::string &extra = "")
        {
            std::ostringstream ss;
            ss << "{\"event\":\"" << event << "\"";
            if (!name.empty())
                ss << ",\"name\":\"" << name << "\"";
            if (line >= 0)
                ss << ",\"line\":" << line;
            if (seq >= 0)
                ss << ",\"seq\":" << seq;
            if (!extra.empty())
                ss << "," << extra;
            ss << "}";
            return ss.str();
        }

    private:
        int pid_ = 0;
        bool isServer_ = false;
        std::atomic<bool> connected_{false};
        std::string path_;
        std::string recvBuffer_;

        static std::string socketPath(int pid)
        {
#ifdef _WIN32
            return "\\\\.\\pipe\\xell-debug-" + std::to_string(pid);
#else
            return "/tmp/xell-debug-" + std::to_string(pid) + ".sock";
#endif
        }

        void swap(DebugIPC &other)
        {
            std::swap(pid_, other.pid_);
            std::swap(isServer_, other.isServer_);
            bool c = connected_.load();
            connected_.store(other.connected_.load());
            other.connected_.store(c);
            std::swap(path_, other.path_);
            std::swap(recvBuffer_, other.recvBuffer_);
#ifdef _WIN32
            std::swap(pipeHandle_, other.pipeHandle_);
#else
            std::swap(serverFd_, other.serverFd_);
            std::swap(clientFd_, other.clientFd_);
#endif
        }

        // ── Unix implementation ──────────────────────────────────────

#ifndef _WIN32
        int serverFd_ = -1;
        int clientFd_ = -1;

        bool listenUnix()
        {
            // Remove stale socket
            ::unlink(path_.c_str());

            serverFd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
            if (serverFd_ < 0)
                return false;

            struct sockaddr_un addr{};
            addr.sun_family = AF_UNIX;
            std::strncpy(addr.sun_path, path_.c_str(), sizeof(addr.sun_path) - 1);

            if (::bind(serverFd_, (struct sockaddr *)&addr, sizeof(addr)) < 0)
            {
                ::close(serverFd_);
                serverFd_ = -1;
                return false;
            }

            if (::listen(serverFd_, 1) < 0)
            {
                ::close(serverFd_);
                ::unlink(path_.c_str());
                serverFd_ = -1;
                return false;
            }

            // Accept one connection (blocking)
            clientFd_ = ::accept(serverFd_, nullptr, nullptr);
            if (clientFd_ < 0)
            {
                ::close(serverFd_);
                ::unlink(path_.c_str());
                serverFd_ = -1;
                return false;
            }

            connected_ = true;
            return true;
        }

        bool connectUnix(int timeoutMs)
        {
            int elapsed = 0;
            const int interval = 50; // ms

            while (elapsed < timeoutMs)
            {
                clientFd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
                if (clientFd_ < 0)
                    return false;

                struct sockaddr_un addr{};
                addr.sun_family = AF_UNIX;
                std::strncpy(addr.sun_path, path_.c_str(), sizeof(addr.sun_path) - 1);

                if (::connect(clientFd_, (struct sockaddr *)&addr, sizeof(addr)) == 0)
                {
                    connected_ = true;
                    return true;
                }

                ::close(clientFd_);
                clientFd_ = -1;

                // Wait and retry
                usleep(interval * 1000);
                elapsed += interval;
            }

            return false;
        }

        bool sendUnix(const std::string &msg)
        {
            if (clientFd_ < 0)
                return false;
            size_t total = 0;
            while (total < msg.size())
            {
                ssize_t n = ::write(clientFd_, msg.data() + total, msg.size() - total);
                if (n <= 0)
                {
                    connected_ = false;
                    return false;
                }
                total += n;
            }
            return true;
        }

        std::string recvUnix()
        {
            if (clientFd_ < 0)
                return "";

            char buf[4096];
            while (true)
            {
                ssize_t n = ::read(clientFd_, buf, sizeof(buf));
                if (n <= 0)
                {
                    connected_ = false;
                    return "";
                }
                recvBuffer_.append(buf, n);

                auto nlPos = recvBuffer_.find('\n');
                if (nlPos != std::string::npos)
                {
                    std::string line = recvBuffer_.substr(0, nlPos);
                    recvBuffer_.erase(0, nlPos + 1);
                    return line;
                }
            }
        }

        bool pollUnix(int timeoutMs)
        {
            if (clientFd_ < 0)
                return false;

            // Check buffer first
            if (recvBuffer_.find('\n') != std::string::npos)
                return true;

            struct pollfd pfd{};
            pfd.fd = clientFd_;
            pfd.events = POLLIN;

            int ret = ::poll(&pfd, 1, timeoutMs);
            return ret > 0 && (pfd.revents & POLLIN);
        }

        void closeUnix()
        {
            if (clientFd_ >= 0)
            {
                ::close(clientFd_);
                clientFd_ = -1;
            }
            if (serverFd_ >= 0)
            {
                ::close(serverFd_);
                serverFd_ = -1;
                if (isServer_ && !path_.empty())
                    ::unlink(path_.c_str());
            }
        }
#endif

        // ── Windows implementation ───────────────────────────────────

#ifdef _WIN32
        HANDLE pipeHandle_ = INVALID_HANDLE_VALUE;

        bool listenWindows()
        {
            pipeHandle_ = CreateNamedPipeA(
                path_.c_str(),
                PIPE_ACCESS_DUPLEX,
                PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                1, 4096, 4096, 0, nullptr);

            if (pipeHandle_ == INVALID_HANDLE_VALUE)
                return false;

            // Block until a client connects
            if (!ConnectNamedPipe(pipeHandle_, nullptr))
            {
                if (GetLastError() != ERROR_PIPE_CONNECTED)
                {
                    CloseHandle(pipeHandle_);
                    pipeHandle_ = INVALID_HANDLE_VALUE;
                    return false;
                }
            }

            connected_ = true;
            return true;
        }

        bool connectWindows(int timeoutMs)
        {
            int elapsed = 0;
            const int interval = 50;

            while (elapsed < timeoutMs)
            {
                pipeHandle_ = CreateFileA(
                    path_.c_str(), GENERIC_READ | GENERIC_WRITE,
                    0, nullptr, OPEN_EXISTING, 0, nullptr);

                if (pipeHandle_ != INVALID_HANDLE_VALUE)
                {
                    connected_ = true;
                    return true;
                }

                Sleep(interval);
                elapsed += interval;
            }
            return false;
        }

        bool sendWindows(const std::string &msg)
        {
            if (pipeHandle_ == INVALID_HANDLE_VALUE)
                return false;
            DWORD written;
            if (!WriteFile(pipeHandle_, msg.data(), (DWORD)msg.size(), &written, nullptr))
            {
                connected_ = false;
                return false;
            }
            return written == (DWORD)msg.size();
        }

        std::string recvWindows()
        {
            if (pipeHandle_ == INVALID_HANDLE_VALUE)
                return "";

            char buf[4096];
            while (true)
            {
                DWORD bytesRead;
                if (!ReadFile(pipeHandle_, buf, sizeof(buf), &bytesRead, nullptr) || bytesRead == 0)
                {
                    connected_ = false;
                    return "";
                }
                recvBuffer_.append(buf, bytesRead);

                auto nlPos = recvBuffer_.find('\n');
                if (nlPos != std::string::npos)
                {
                    std::string line = recvBuffer_.substr(0, nlPos);
                    recvBuffer_.erase(0, nlPos + 1);
                    return line;
                }
            }
        }

        bool pollWindows(int timeoutMs)
        {
            if (pipeHandle_ == INVALID_HANDLE_VALUE)
                return false;
            if (recvBuffer_.find('\n') != std::string::npos)
                return true;

            DWORD available;
            if (PeekNamedPipe(pipeHandle_, nullptr, 0, nullptr, &available, nullptr))
                return available > 0;
            return false;
        }

        void closeWindows()
        {
            if (pipeHandle_ != INVALID_HANDLE_VALUE)
            {
                if (isServer_)
                    DisconnectNamedPipe(pipeHandle_);
                CloseHandle(pipeHandle_);
                pipeHandle_ = INVALID_HANDLE_VALUE;
            }
        }
#endif
    };

    // ─── Non-blocking Server Variant ─────────────────────────────────────
    //
    // The regular DebugIPC::listen() blocks until the IDE connects.
    // For the interpreter, we need a non-blocking variant that:
    //   1. Creates the socket
    //   2. Starts listening in a background thread
    //   3. Returns immediately so execution can begin
    //   4. Once IDE connects, debug session starts
    //
    // This is the "server" wrapper used by the interpreter.

    class DebugServer
    {
    public:
        DebugServer() = default;
        ~DebugServer() { shutdown(); }

        // Start listening on a background thread. Returns the socket path.
        std::string start(int pid)
        {
            pid_ = pid;
            path_ = "/tmp/xell-debug-" + std::to_string(pid) + ".sock";

            // Remove stale socket
            ::unlink(path_.c_str());

#ifndef _WIN32
            serverFd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
            if (serverFd_ < 0)
                return "";

            struct sockaddr_un addr{};
            addr.sun_family = AF_UNIX;
            std::strncpy(addr.sun_path, path_.c_str(), sizeof(addr.sun_path) - 1);

            if (::bind(serverFd_, (struct sockaddr *)&addr, sizeof(addr)) < 0)
            {
                ::close(serverFd_);
                serverFd_ = -1;
                return "";
            }

            if (::listen(serverFd_, 1) < 0)
            {
                ::close(serverFd_);
                ::unlink(path_.c_str());
                serverFd_ = -1;
                return "";
            }

            // Set server socket non-blocking so we can poll for connections
            int flags = ::fcntl(serverFd_, F_GETFL, 0);
            ::fcntl(serverFd_, F_SETFL, flags | O_NONBLOCK);
#endif

            ready_ = true;
            return path_;
        }

        // Non-blocking: check if IDE has connected. Call this periodically.
        bool acceptIfReady()
        {
            if (!ready_ || connected_)
                return connected_;

#ifndef _WIN32
            clientFd_ = ::accept(serverFd_, nullptr, nullptr);
            if (clientFd_ >= 0)
            {
                connected_ = true;
                return true;
            }
#endif
            return false;
        }

        // Once connected, send/recv via the IPC channel.
        bool send(const std::string &json)
        {
            if (!connected_)
                return false;
            std::string msg = json;
            if (msg.empty() || msg.back() != '\n')
                msg += '\n';

#ifndef _WIN32
            size_t total = 0;
            while (total < msg.size())
            {
                ssize_t n = ::write(clientFd_, msg.data() + total, msg.size() - total);
                if (n <= 0)
                {
                    connected_ = false;
                    return false;
                }
                total += n;
            }
#endif
            return true;
        }

        std::string recv()
        {
            if (!connected_)
                return "";

            auto nlPos = recvBuffer_.find('\n');
            if (nlPos != std::string::npos)
            {
                std::string line = recvBuffer_.substr(0, nlPos);
                recvBuffer_.erase(0, nlPos + 1);
                return line;
            }

#ifndef _WIN32
            char buf[4096];
            while (true)
            {
                ssize_t n = ::read(clientFd_, buf, sizeof(buf));
                if (n <= 0)
                {
                    connected_ = false;
                    return "";
                }
                recvBuffer_.append(buf, n);

                nlPos = recvBuffer_.find('\n');
                if (nlPos != std::string::npos)
                {
                    std::string line = recvBuffer_.substr(0, nlPos);
                    recvBuffer_.erase(0, nlPos + 1);
                    return line;
                }
            }
#endif
            return "";
        }

        bool poll(int timeoutMs = 0)
        {
            if (!connected_)
                return false;
            if (recvBuffer_.find('\n') != std::string::npos)
                return true;

#ifndef _WIN32
            struct pollfd pfd{};
            pfd.fd = clientFd_;
            pfd.events = POLLIN;
            return ::poll(&pfd, 1, timeoutMs) > 0 && (pfd.revents & POLLIN);
#else
            return false;
#endif
        }

        bool isConnected() const { return connected_; }
        bool isReady() const { return ready_; }
        const std::string &path() const { return path_; }

        void shutdown()
        {
#ifndef _WIN32
            if (clientFd_ >= 0)
            {
                ::close(clientFd_);
                clientFd_ = -1;
            }
            if (serverFd_ >= 0)
            {
                ::close(serverFd_);
                serverFd_ = -1;
                ::unlink(path_.c_str());
            }
#endif
            connected_ = false;
            ready_ = false;
        }

    private:
        int pid_ = 0;
        std::string path_;
        std::string recvBuffer_;
        bool ready_ = false;
        bool connected_ = false;

#ifndef _WIN32
        int serverFd_ = -1;
        int clientFd_ = -1;
#endif
    };

} // namespace xell
