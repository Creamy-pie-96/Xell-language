#pragma once

// ─── Debug IPC Client — IDE-side connection to the Xell debug server ─────
//
// Lightweight client for the xell-terminal to connect to xell --debug.
// Communicates via Unix domain socket with newline-delimited JSON.
//
// Protocol:
//   IDE → Interpreter: {"cmd":"step_over"}\n  {"cmd":"continue"}\n  etc.
//   Interpreter → IDE: {"state":"paused","line":25,...}\n  etc.
//

#include <string>
#include <atomic>
#include <sstream>
#include <cstring>

#ifndef _WIN32
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <poll.h>
#endif

namespace xterm
{

    // ─── Debug Session State ─────────────────────────────────────────────

    struct DebugState
    {
        bool active = false;          // Is a debug session running?
        bool paused = false;          // Is execution paused (stepping)?
        int currentLine = -1;         // Line number where paused
        int sequence = -1;            // Trace sequence number
        int depth = -1;               // Call stack depth
        std::string varsJSON = "{}";  // Variables at current pause
        std::string stackJSON = "[]"; // Call stack at current pause
    };

    // ─── Debug IPC Client ────────────────────────────────────────────────

    class DebugClient
    {
    public:
        DebugClient() = default;
        ~DebugClient() { disconnect(); }

        // Connect to a debug server socket path.
        bool connect(const std::string &socketPath, int timeoutMs = 3000)
        {
#ifndef _WIN32
            int elapsed = 0;
            const int interval = 50;

            while (elapsed < timeoutMs)
            {
                fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
                if (fd_ < 0)
                    return false;

                struct sockaddr_un addr{};
                addr.sun_family = AF_UNIX;
                std::strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);

                if (::connect(fd_, (struct sockaddr *)&addr, sizeof(addr)) == 0)
                {
                    connected_ = true;
                    return true;
                }

                ::close(fd_);
                fd_ = -1;
                usleep(interval * 1000);
                elapsed += interval;
            }
#endif
            return false;
        }

        void disconnect()
        {
#ifndef _WIN32
            if (fd_ >= 0)
            {
                ::close(fd_);
                fd_ = -1;
            }
#endif
            connected_ = false;
        }

        bool isConnected() const { return connected_; }

        // ── Send commands ────────────────────────────────────────────

        bool sendContinue() { return sendCmd("{\"cmd\":\"continue\"}"); }
        bool sendStepOver() { return sendCmd("{\"cmd\":\"step_over\"}"); }
        bool sendStepInto() { return sendCmd("{\"cmd\":\"step_into\"}"); }
        bool sendStepOut() { return sendCmd("{\"cmd\":\"step_out\"}"); }
        bool sendStop() { return sendCmd("{\"cmd\":\"stop\"}"); }

        bool sendAddBreakpoint(int line, const std::string &type = "pause")
        {
            std::ostringstream ss;
            ss << "{\"cmd\":\"add_breakpoint\",\"line\":" << line
               << ",\"type\":\"" << type << "\"}";
            return sendCmd(ss.str());
        }

        bool sendRemoveBreakpoint(int line)
        {
            return sendCmd("{\"cmd\":\"remove_breakpoint\",\"line\":" + std::to_string(line) + "}");
        }

        // ── Receive state updates ────────────────────────────────────

        // Non-blocking poll: check if data available.
        bool hasMessage(int timeoutMs = 0)
        {
#ifndef _WIN32
            if (fd_ < 0)
                return false;
            if (recvBuffer_.find('\n') != std::string::npos)
                return true;

            struct pollfd pfd{};
            pfd.fd = fd_;
            pfd.events = POLLIN;
            return ::poll(&pfd, 1, timeoutMs) > 0 && (pfd.revents & POLLIN);
#else
            return false;
#endif
        }

        // Receive one JSON message (blocking).
        std::string recv()
        {
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
                ssize_t n = ::read(fd_, buf, sizeof(buf));
                if (n <= 0)
                {
                    connected_ = false;
                    // Check buffer for any remaining complete message before returning empty
                    nlPos = recvBuffer_.find('\n');
                    if (nlPos != std::string::npos)
                    {
                        std::string line = recvBuffer_.substr(0, nlPos);
                        recvBuffer_.erase(0, nlPos + 1);
                        return line;
                    }
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

        // Receive one message non-blocking. Returns empty if nothing available.
        std::string tryRecv()
        {
            // First check if we already have a complete message buffered
            auto nlPos = recvBuffer_.find('\n');
            if (nlPos != std::string::npos)
            {
                std::string line = recvBuffer_.substr(0, nlPos);
                recvBuffer_.erase(0, nlPos + 1);
                return line;
            }

            if (!hasMessage(0))
                return "";
            // Read available data
#ifndef _WIN32
            char buf[4096];
            struct pollfd pfd{};
            pfd.fd = fd_;
            pfd.events = POLLIN;
            while (::poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN))
            {
                ssize_t n = ::read(fd_, buf, sizeof(buf));
                if (n <= 0)
                {
                    // EOF or error — connection closing.
                    // Don't return yet; check buffer first for any complete message.
                    if (n == 0)
                        connected_ = false;
                    break;
                }
                recvBuffer_.append(buf, n);
            }
#endif
            nlPos = recvBuffer_.find('\n');
            if (nlPos != std::string::npos)
            {
                std::string line = recvBuffer_.substr(0, nlPos);
                recvBuffer_.erase(0, nlPos + 1);
                return line;
            }
            return "";
        }

        // ── Parse state from received JSON ───────────────────────────

        static DebugState parseState(const std::string &json)
        {
            DebugState state;

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
                int val = 0;
                bool neg = false;
                if (pos < json.size() && json[pos] == '-')
                {
                    neg = true;
                    pos++;
                }
                while (pos < json.size() && json[pos] >= '0' && json[pos] <= '9')
                    val = val * 10 + (json[pos++] - '0');
                return neg ? -val : val;
            };

            auto getObject = [&](const std::string &key) -> std::string
            {
                std::string search = "\"" + key + "\"";
                auto pos = json.find(search);
                if (pos == std::string::npos)
                    return "";
                pos = json.find(':', pos + search.size());
                if (pos == std::string::npos)
                    return "";
                pos++;
                while (pos < json.size() && json[pos] == ' ')
                    pos++;
                if (pos >= json.size())
                    return "";
                char open = json[pos];
                char close = (open == '{') ? '}' : (open == '[') ? ']'
                                                                 : '\0';
                if (close == '\0')
                    return "";
                int depth = 1;
                size_t start = pos;
                pos++;
                while (pos < json.size() && depth > 0)
                {
                    if (json[pos] == open)
                        depth++;
                    else if (json[pos] == close)
                        depth--;
                    pos++;
                }
                return json.substr(start, pos - start);
            };

            std::string stateStr = getString("state");
            if (stateStr == "paused")
            {
                state.active = true;
                state.paused = true;
            }
            else if (stateStr == "running")
            {
                state.active = true;
                state.paused = false;
            }
            else if (stateStr == "finished")
            {
                state.active = false;
                state.paused = false;
            }

            state.currentLine = getInt("line");
            state.sequence = getInt("seq");
            state.depth = getInt("depth");

            std::string vars = getObject("vars");
            if (!vars.empty())
                state.varsJSON = vars;

            std::string stack = getObject("callStack");
            if (!stack.empty())
                state.stackJSON = stack;

            return state;
        }

    private:
        int fd_ = -1;
        bool connected_ = false;
        std::string recvBuffer_;

        bool sendCmd(const std::string &json)
        {
#ifndef _WIN32
            if (fd_ < 0)
                return false;
            std::string msg = json + "\n";
            size_t total = 0;
            while (total < msg.size())
            {
                ssize_t n = ::write(fd_, msg.data() + total, msg.size() - total);
                if (n <= 0)
                {
                    connected_ = false;
                    return false;
                }
                total += n;
            }
            return true;
#else
            return false;
#endif
        }
    };

} // namespace xterm
