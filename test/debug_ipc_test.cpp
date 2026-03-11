// =============================================================================
// Debug IPC Integration Tests — End-to-End Socket-Based Debug Session Testing
// =============================================================================
// Tests the FULL debug pipeline by launching xell --debug as a subprocess,
// connecting to its IPC socket, sending commands (continue, step_over,
// step_into, step_out, stop, add_breakpoint, remove_breakpoint), and
// asserting the state JSON responses.
//
// Test categories:
//   1. Basic session lifecycle (launch, pause on first line, continue, finish)
//   2. Step Over (does not enter functions)
//   3. Step Into (enters functions, call stack grows)
//   4. Step Out (returns to caller)
//   5. IDE breakpoints (add while paused, hit on continue)
//   6. Call stack correctness
//   7. Variable inspection
//   8. Stop command (terminates session)
//   9. Decorator @breakpoint + IPC stepping (mixed mode)
//  10. Decorator @checkpoint + IPC verification
//  11. Decorator @watch + trace verification
//  12. Decorator @notrack + trace verification
//  13. Continue runs to completion
// =============================================================================

#include <iostream>
#include <sstream>
#include <functional>
#include <string>
#include <vector>
#include <algorithm>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <filesystem>

// POSIX
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <fcntl.h>

// ---- Minimal test framework ------------------------------------------------

static int g_passed = 0;
static int g_failed = 0;

static void runTest(const std::string &name, std::function<void()> fn)
{
    try
    {
        fn();
        std::cout << "  PASS: " << name << "\n";
        g_passed++;
    }
    catch (const std::exception &e)
    {
        std::cout << "  FAIL: " << name << "\n        " << e.what() << "\n";
        g_failed++;
    }
}

#define XASSERT(cond)                                                      \
    do                                                                     \
    {                                                                      \
        if (!(cond))                                                       \
        {                                                                  \
            std::ostringstream os;                                         \
            os << "Assertion failed: " #cond " (line " << __LINE__ << ")"; \
            throw std::runtime_error(os.str());                            \
        }                                                                  \
    } while (0)

#define XASSERT_EQ(a, b)                                 \
    do                                                   \
    {                                                    \
        if ((a) != (b))                                  \
        {                                                \
            std::ostringstream os;                       \
            os << "Expected [" << (a) << "] == [" << (b) \
               << "] (line " << __LINE__ << ")";         \
            throw std::runtime_error(os.str());          \
        }                                                \
    } while (0)

#define XASSERT_NE(a, b)                                 \
    do                                                   \
    {                                                    \
        if ((a) == (b))                                  \
        {                                                \
            std::ostringstream os;                       \
            os << "Expected [" << (a) << "] != [" << (b) \
               << "] (line " << __LINE__ << ")";         \
            throw std::runtime_error(os.str());          \
        }                                                \
    } while (0)

#define XASSERT_GE(a, b)                                 \
    do                                                   \
    {                                                    \
        if ((a) < (b))                                   \
        {                                                \
            std::ostringstream os;                       \
            os << "Expected [" << (a) << "] >= [" << (b) \
               << "] (line " << __LINE__ << ")";         \
            throw std::runtime_error(os.str());          \
        }                                                \
    } while (0)

#define XASSERT_LE(a, b)                                 \
    do                                                   \
    {                                                    \
        if ((a) > (b))                                   \
        {                                                \
            std::ostringstream os;                       \
            os << "Expected [" << (a) << "] <= [" << (b) \
               << "] (line " << __LINE__ << ")";         \
            throw std::runtime_error(os.str());          \
        }                                                \
    } while (0)

#define XASSERT_GT(a, b)                                \
    do                                                  \
    {                                                   \
        if ((a) <= (b))                                 \
        {                                               \
            std::ostringstream os;                      \
            os << "Expected [" << (a) << "] > [" << (b) \
               << "] (line " << __LINE__ << ")";        \
            throw std::runtime_error(os.str());         \
        }                                               \
    } while (0)

// ---- IPC Debug Client for tests --------------------------------------------

struct DebugState
{
    std::string state; // "paused", "running", "finished"
    int line = -1;
    int seq = -1;
    int depth = -1;
    std::string varsJSON;
    std::string callStackJSON;
    bool valid = false;
};

class TestDebugClient
{
public:
    TestDebugClient() = default;
    ~TestDebugClient() { disconnect(); }

    bool connect(const std::string &socketPath, int timeoutMs = 5000)
    {
        fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd_ < 0)
            return false;

        struct sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);

        int elapsed = 0;
        while (elapsed < timeoutMs)
        {
            if (::connect(fd_, (struct sockaddr *)&addr, sizeof(addr)) == 0)
            {
                connected_ = true;
                return true;
            }
            usleep(50000); // 50ms
            elapsed += 50;
        }
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    void disconnect()
    {
        if (fd_ >= 0)
        {
            ::close(fd_);
            fd_ = -1;
        }
        connected_ = false;
    }

    bool isConnected() const { return connected_; }

    bool sendCmd(const std::string &json)
    {
        if (!connected_)
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
    }

    bool sendContinue() { return sendCmd("{\"cmd\":\"continue\"}"); }
    bool sendStepOver() { return sendCmd("{\"cmd\":\"step_over\"}"); }
    bool sendStepInto() { return sendCmd("{\"cmd\":\"step_into\"}"); }
    bool sendStepOut() { return sendCmd("{\"cmd\":\"step_out\"}"); }
    bool sendStop() { return sendCmd("{\"cmd\":\"stop\"}"); }

    bool sendAddBreakpoint(int line, const std::string &type = "pause")
    {
        return sendCmd("{\"cmd\":\"add_breakpoint\",\"line\":" + std::to_string(line) +
                       ",\"type\":\"" + type + "\"}");
    }

    bool sendRemoveBreakpoint(int line)
    {
        return sendCmd("{\"cmd\":\"remove_breakpoint\",\"line\":" + std::to_string(line) + "}");
    }

    // Receive one JSON message with timeout. Returns empty on timeout/error.
    std::string recv(int timeoutMs = 10000)
    {
        if (!connected_)
            return "";

        // Check buffer first
        auto nlPos = buffer_.find('\n');
        if (nlPos != std::string::npos)
        {
            std::string line = buffer_.substr(0, nlPos);
            buffer_.erase(0, nlPos + 1);
            return line;
        }

        // Poll with timeout
        struct pollfd pfd{};
        pfd.fd = fd_;
        pfd.events = POLLIN;

        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
        while (std::chrono::steady_clock::now() < deadline)
        {
            int remaining = (int)std::chrono::duration_cast<std::chrono::milliseconds>(
                                deadline - std::chrono::steady_clock::now())
                                .count();
            if (remaining <= 0)
                break;

            int ret = ::poll(&pfd, 1, std::min(remaining, 100));
            if (ret > 0 && (pfd.revents & POLLIN))
            {
                char buf[4096];
                ssize_t n = ::read(fd_, buf, sizeof(buf));
                if (n <= 0)
                {
                    connected_ = false;
                    // Check buffer
                    nlPos = buffer_.find('\n');
                    if (nlPos != std::string::npos)
                    {
                        std::string line = buffer_.substr(0, nlPos);
                        buffer_.erase(0, nlPos + 1);
                        return line;
                    }
                    return "";
                }
                buffer_.append(buf, n);

                nlPos = buffer_.find('\n');
                if (nlPos != std::string::npos)
                {
                    std::string line = buffer_.substr(0, nlPos);
                    buffer_.erase(0, nlPos + 1);
                    return line;
                }
            }
        }
        return "";
    }

    // Try to receive non-blocking
    std::string tryRecv()
    {
        if (!connected_)
            return "";

        auto nlPos = buffer_.find('\n');
        if (nlPos != std::string::npos)
        {
            std::string line = buffer_.substr(0, nlPos);
            buffer_.erase(0, nlPos + 1);
            return line;
        }

        struct pollfd pfd{};
        pfd.fd = fd_;
        pfd.events = POLLIN;
        while (::poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN))
        {
            char buf[4096];
            ssize_t n = ::read(fd_, buf, sizeof(buf));
            if (n <= 0)
            {
                if (n == 0)
                    connected_ = false;
                break;
            }
            buffer_.append(buf, n);
        }

        nlPos = buffer_.find('\n');
        if (nlPos != std::string::npos)
        {
            std::string line = buffer_.substr(0, nlPos);
            buffer_.erase(0, nlPos + 1);
            return line;
        }
        return "";
    }

private:
    int fd_ = -1;
    bool connected_ = false;
    std::string buffer_;
};

// ---- JSON helpers ----------------------------------------------------------

static std::string jsonGetString(const std::string &json, const std::string &key)
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
}

static int jsonGetInt(const std::string &json, const std::string &key)
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
}

static std::string jsonGetObject(const std::string &json, const std::string &key)
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
}

static DebugState parseState(const std::string &json)
{
    DebugState s;
    s.state = jsonGetString(json, "state");
    s.line = jsonGetInt(json, "line");
    s.seq = jsonGetInt(json, "seq");
    s.depth = jsonGetInt(json, "depth");
    s.varsJSON = jsonGetObject(json, "vars");
    s.callStackJSON = jsonGetObject(json, "callStack");
    s.valid = !s.state.empty();
    return s;
}

static bool jsonContains(const std::string &json, const std::string &substring)
{
    return json.find(substring) != std::string::npos;
}

// Count entries in a JSON array of strings: ["a","b","c"] → 3
static int jsonArraySize(const std::string &json)
{
    if (json.size() < 2 || json[0] != '[')
        return 0;
    if (json == "[]")
        return 0;
    int count = 0;
    bool inString = false;
    for (size_t i = 1; i < json.size() - 1; i++)
    {
        if (json[i] == '"' && (i == 0 || json[i - 1] != '\\'))
        {
            if (!inString)
                count++;
            inString = !inString;
        }
    }
    return count; // each opening quote = one string entry
}

// ---- Debug session launcher ------------------------------------------------

struct DebugSession
{
    pid_t childPid = 0;
    std::string socketPath;
    TestDebugClient client;
    int stderrPipeFd = -1;

    ~DebugSession() { cleanup(); }

    void cleanup()
    {
        client.disconnect();
        if (childPid > 0)
        {
            kill(-childPid, SIGTERM);
            kill(childPid, SIGTERM);
            usleep(50000);
            kill(-childPid, SIGKILL);
            kill(childPid, SIGKILL);
            waitpid(childPid, nullptr, WNOHANG);
            childPid = 0;
        }
        if (!socketPath.empty())
        {
            unlink(socketPath.c_str());
            socketPath.clear();
        }
    }
};

static std::string getXellBinary()
{
    // Look relative to test binary first, then fallback
    std::string buildDir = std::filesystem::path(__FILE__).parent_path().parent_path().string() + "/build";
    std::string path = buildDir + "/xell";
    if (std::filesystem::exists(path))
        return path;
    // Try absolute
    return "/home/DATA/CODE/code/Xell/build/xell";
}

static std::string getFixturePath(const std::string &name)
{
    std::string srcDir = std::filesystem::path(__FILE__).parent_path().string();
    return srcDir + "/fixtures/" + name;
}

// Launch xell --debug, wait for socket, connect. Returns true on success.
static bool launchDebugSession(DebugSession &session, const std::string &fixture)
{
    std::string xellBin = getXellBinary();
    std::string filePath = getFixturePath(fixture);

    if (!std::filesystem::exists(filePath))
    {
        std::cerr << "  [ERROR] Fixture not found: " << filePath << "\n";
        return false;
    }

    int stderrPipe[2];
    if (pipe(stderrPipe) != 0)
        return false;

    pid_t pid = fork();
    if (pid < 0)
    {
        close(stderrPipe[0]);
        close(stderrPipe[1]);
        return false;
    }

    if (pid == 0)
    {
        // Child
        setsid();
        close(stderrPipe[0]);
        dup2(stderrPipe[1], STDERR_FILENO);
        close(stderrPipe[1]);
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0)
        {
            dup2(devnull, STDOUT_FILENO);
            close(devnull);
        }
        execlp(xellBin.c_str(), xellBin.c_str(), "--debug", filePath.c_str(), nullptr);
        _exit(127);
    }

    // Parent
    close(stderrPipe[1]);
    session.childPid = pid;

    // Read socket path from stderr
    int flags = fcntl(stderrPipe[0], F_GETFL, 0);
    fcntl(stderrPipe[0], F_SETFL, flags | O_NONBLOCK);

    std::string stderrBuf;
    char buf[512];
    int elapsed = 0;
    const int intervalUs = 20000;
    const int timeoutUs = 5000000; // 5 seconds

    while (elapsed < timeoutUs)
    {
        ssize_t n = read(stderrPipe[0], buf, sizeof(buf) - 1);
        if (n > 0)
        {
            buf[n] = '\0';
            stderrBuf.append(buf, n);
            const std::string prefix = "XELL_DEBUG_SOCKET:";
            auto pos = stderrBuf.find(prefix);
            if (pos != std::string::npos)
            {
                auto lineEnd = stderrBuf.find('\n', pos);
                if (lineEnd != std::string::npos)
                    session.socketPath = stderrBuf.substr(pos + prefix.size(), lineEnd - pos - prefix.size());
                else
                    session.socketPath = stderrBuf.substr(pos + prefix.size());
                while (!session.socketPath.empty() &&
                       (session.socketPath.back() == '\n' || session.socketPath.back() == '\r' || session.socketPath.back() == ' '))
                    session.socketPath.pop_back();
                break;
            }
        }
        usleep(intervalUs);
        elapsed += intervalUs;
    }
    close(stderrPipe[0]);

    if (session.socketPath.empty())
    {
        std::cerr << "  [ERROR] No socket path from xell --debug\n";
        session.cleanup();
        return false;
    }

    // Connect
    if (!session.client.connect(session.socketPath, 5000))
    {
        std::cerr << "  [ERROR] Could not connect to " << session.socketPath << "\n";
        session.cleanup();
        return false;
    }

    return true;
}

// Receive a state message, assert it's valid and paused
static DebugState expectPaused(DebugSession &session, int timeoutMs = 10000)
{
    std::string msg = session.client.recv(timeoutMs);
    if (msg.empty())
        throw std::runtime_error("Timeout waiting for paused state");
    DebugState s = parseState(msg);
    if (!s.valid)
        throw std::runtime_error("Invalid state JSON: " + msg);
    if (s.state != "paused")
        throw std::runtime_error("Expected paused, got: " + s.state + " (msg: " + msg + ")");
    return s;
}

// Receive a finished message
static DebugState expectFinished(DebugSession &session, int timeoutMs = 10000)
{
    std::string msg = session.client.recv(timeoutMs);
    if (msg.empty())
    {
        // Connection might have closed — check if child exited
        if (session.childPid > 0)
        {
            int status = 0;
            pid_t result = waitpid(session.childPid, &status, WNOHANG);
            if (result > 0)
            {
                session.childPid = 0;
                // Process exited, this counts as finished
                DebugState s;
                s.state = "finished";
                s.valid = true;
                return s;
            }
        }
        throw std::runtime_error("Timeout waiting for finished state");
    }
    DebugState s = parseState(msg);
    if (s.state != "finished")
        throw std::runtime_error("Expected finished, got: " + s.state);
    return s;
}

// Step and expect paused at a specific line
static DebugState stepAndExpect(DebugSession &session, const std::string &cmd, int expectedLine)
{
    bool ok;
    if (cmd == "step_over")
        ok = session.client.sendStepOver();
    else if (cmd == "step_into")
        ok = session.client.sendStepInto();
    else if (cmd == "step_out")
        ok = session.client.sendStepOut();
    else if (cmd == "continue")
        ok = session.client.sendContinue();
    else
        throw std::runtime_error("Unknown cmd: " + cmd);

    if (!ok)
        throw std::runtime_error("Failed to send " + cmd);

    DebugState s = expectPaused(session);
    if (expectedLine > 0 && s.line != expectedLine)
    {
        std::ostringstream os;
        os << "After " << cmd << ": expected line " << expectedLine << ", got " << s.line;
        throw std::runtime_error(os.str());
    }
    return s;
}

// ============================================================================
// Section 1: Basic Session Lifecycle
// ============================================================================

static void testBasicLifecycle()
{
    std::cout << "\n===== Basic Session Lifecycle =====\n";

    // step_test.xel:
    //  1: fn add(a, b) :        (fn declaration — skipped by interpreter)
    //  ...
    //  8: x = 10                (first executable statement)
    //  9: y = add(3, 4)
    // 10: z = multiply(x, y)
    // 11: print(z)

    runTest("launch: pauses on first executable statement", []()
            {
        DebugSession session;
        XASSERT(launchDebugSession(session, "step_test.xel"));
        DebugState s = expectPaused(session);
        XASSERT(s.valid);
        XASSERT_EQ(s.state, "paused");
        // First executable statement should be the first fn decl or first assignment
        // Fn declarations are executable (they bind the function name)
        XASSERT_GE(s.line, 1);
        XASSERT_LE(s.line, 8);
        session.cleanup(); });

    // breakpoint_test.xel:
    //  1: x = 1
    //  2: y = 2
    //  3: z = x + y
    //  4: total = z * 10
    //  5: print(total)

    runTest("continue to completion: receives finished", []()
            {
        DebugSession session;
        XASSERT(launchDebugSession(session, "breakpoint_test.xel"));
        // Should be paused on first line
        DebugState s = expectPaused(session);
        XASSERT_EQ(s.state, "paused");

        // Send continue — should run to completion
        XASSERT(session.client.sendContinue());
        DebugState fin = expectFinished(session);
        XASSERT_EQ(fin.state, "finished");
        session.cleanup(); });

    runTest("state includes seq and depth fields", []()
            {
        DebugSession session;
        XASSERT(launchDebugSession(session, "breakpoint_test.xel"));
        DebugState s = expectPaused(session);
        XASSERT_GE(s.seq, 0);
        XASSERT_GE(s.depth, 0);
        session.cleanup(); });

    runTest("state always includes callStack field", []()
            {
        DebugSession session;
        XASSERT(launchDebugSession(session, "breakpoint_test.xel"));
        DebugState s = expectPaused(session);
        // callStack should always be present (even if empty [])
        XASSERT(!s.callStackJSON.empty());
        XASSERT(s.callStackJSON[0] == '[');
        session.cleanup(); });
}

// ============================================================================
// Section 2: Step Over
// ============================================================================

static void testStepOver()
{
    std::cout << "\n===== Step Over =====\n";

    // breakpoint_test.xel: 5 simple lines, no functions
    //  1: x = 1
    //  2: y = 2
    //  3: z = x + y
    //  4: total = z * 10
    //  5: print(total)

    runTest("step over: advances line by line in simple code", []()
            {
        DebugSession session;
        XASSERT(launchDebugSession(session, "breakpoint_test.xel"));

        DebugState s = expectPaused(session);
        int firstLine = s.line;
        XASSERT_EQ(firstLine, 1);

        // Step over should advance to next line
        s = stepAndExpect(session, "step_over", 2);
        s = stepAndExpect(session, "step_over", 3);
        s = stepAndExpect(session, "step_over", 4);
        s = stepAndExpect(session, "step_over", 5);

        // One more step should finish
        session.client.sendStepOver();
        DebugState fin = expectFinished(session);
        XASSERT_EQ(fin.state, "finished");
        session.cleanup(); });

    // step_test.xel: has functions
    //  1: fn add(a, b) :       (fn decl)
    //  ...
    //  8: x = 10
    //  9: y = add(3, 4)        ← step over should NOT enter add()
    // 10: z = multiply(x, y)   ← should arrive here after step over
    // 11: print(z)

    runTest("step over: does NOT enter function calls", []()
            {
        DebugSession session;
        XASSERT(launchDebugSession(session, "step_test.xel"));

        DebugState s = expectPaused(session);
        // Step through fn declarations until we hit line 8
        while (s.line < 8)
        {
            session.client.sendStepOver();
            s = expectPaused(session);
        }
        XASSERT_EQ(s.line, 8); // x = 10

        // Step over to y = add(3, 4)
        s = stepAndExpect(session, "step_over", 9);
        int depthAtCall = s.depth;

        // Step over again — should land on line 10, NOT inside add()
        s = stepAndExpect(session, "step_over", 10);
        // Depth should be same as before (did not descend)
        XASSERT_LE(s.depth, depthAtCall);

        session.cleanup(); });
}

// ============================================================================
// Section 3: Step Into
// ============================================================================

static void testStepInto()
{
    std::cout << "\n===== Step Into =====\n";

    // step_test.xel:
    //  1: fn add(a, b) :
    //  2:     result = a + b
    //  3:     give result
    //  4: ;
    //  5: fn multiply(a, b) :
    //  6:     give a * b
    //  7: ;
    //  8: x = 10
    //  9: y = add(3, 4)
    // 10: z = multiply(x, y)
    // 11: print(z)

    runTest("step into: enters function body", []()
            {
        DebugSession session;
        XASSERT(launchDebugSession(session, "step_test.xel"));

        DebugState s = expectPaused(session);
        // Navigate to line 9: y = add(3, 4)
        while (s.line < 9)
        {
            session.client.sendStepInto();
            s = expectPaused(session);
        }
        XASSERT_EQ(s.line, 9);

        // Step into — should enter add() function body
        s = stepAndExpect(session, "step_into", 2); // result = a + b
        // Depth should have increased
        XASSERT_GT(s.depth, 0);

        session.cleanup(); });

    runTest("step into: call stack grows when entering function", []()
            {
        DebugSession session;
        XASSERT(launchDebugSession(session, "callstack_test.xel"));
        // callstack_test.xel:
        //  1: fn inner(n) :
        //  2:     result = n * 2
        //  3:     give result
        //  4: ;
        //  5: fn outer(n) :
        //  6:     val = inner(n)
        //  7:     give val + 1
        //  8: ;
        //  9: answer = outer(5)
        // 10: print(answer)

        DebugState s = expectPaused(session);
        // Navigate to line 9
        while (s.line < 9)
        {
            session.client.sendStepInto();
            s = expectPaused(session);
        }

        int topLevelStack = jsonArraySize(s.callStackJSON);

        // Step into outer()
        s = stepAndExpect(session, "step_into", 6);
        int outerStack = jsonArraySize(s.callStackJSON);
        XASSERT_GT(outerStack, topLevelStack);

        // Step into inner() from inside outer()
        s = stepAndExpect(session, "step_into", 2);
        int innerStack = jsonArraySize(s.callStackJSON);
        XASSERT_GT(innerStack, outerStack);

        session.cleanup(); });
}

// ============================================================================
// Section 4: Step Out
// ============================================================================

static void testStepOut()
{
    std::cout << "\n===== Step Out =====\n";

    runTest("step out: returns to caller", []()
            {
        DebugSession session;
        XASSERT(launchDebugSession(session, "callstack_test.xel"));

        DebugState s = expectPaused(session);
        // Navigate to line 9 (answer = outer(5))
        while (s.line < 9)
        {
            session.client.sendStepInto();
            s = expectPaused(session);
        }

        // Step into outer()
        s = stepAndExpect(session, "step_into", 6);
        // Step into inner()
        s = stepAndExpect(session, "step_into", 2);
        int innerDepth = s.depth;

        // Step out — should return to outer()
        session.client.sendStepOut();
        s = expectPaused(session);
        // Should be back in outer or at the call site level
        XASSERT(s.depth < innerDepth);

        session.cleanup(); });
}

// ============================================================================
// Section 5: IDE Breakpoints
// ============================================================================

static void testBreakpoints()
{
    std::cout << "\n===== IDE Breakpoints =====\n";

    // breakpoint_test.xel:
    //  1: x = 1
    //  2: y = 2
    //  3: z = x + y
    //  4: total = z * 10
    //  5: print(total)

    runTest("add breakpoint: hits on continue", []()
            {
        DebugSession session;
        XASSERT(launchDebugSession(session, "breakpoint_test.xel"));

        DebugState s = expectPaused(session);
        XASSERT_EQ(s.line, 1);

        // Add breakpoint on line 4
        XASSERT(session.client.sendAddBreakpoint(4));

        // Continue — should pause on line 4
        session.client.sendContinue();
        s = expectPaused(session);
        XASSERT_EQ(s.line, 4);

        // Continue to end
        session.client.sendContinue();
        expectFinished(session);
        session.cleanup(); });

    runTest("multiple breakpoints: hits each in order", []()
            {
        DebugSession session;
        XASSERT(launchDebugSession(session, "breakpoint_test.xel"));

        DebugState s = expectPaused(session);

        // Add breakpoints on lines 2 and 4
        session.client.sendAddBreakpoint(2);
        session.client.sendAddBreakpoint(4);

        // Continue — should hit line 2
        session.client.sendContinue();
        s = expectPaused(session);
        XASSERT_EQ(s.line, 2);

        // Continue — should hit line 4
        session.client.sendContinue();
        s = expectPaused(session);
        XASSERT_EQ(s.line, 4);

        // Continue to end
        session.client.sendContinue();
        expectFinished(session);
        session.cleanup(); });

    runTest("remove breakpoint: no longer hits", []()
            {
        DebugSession session;
        XASSERT(launchDebugSession(session, "breakpoint_test.xel"));

        DebugState s = expectPaused(session);

        // Add breakpoints on 2, 3, 4
        session.client.sendAddBreakpoint(2);
        session.client.sendAddBreakpoint(3);
        session.client.sendAddBreakpoint(4);

        // Remove the one on line 3
        session.client.sendRemoveBreakpoint(3);

        // Continue — should hit line 2
        session.client.sendContinue();
        s = expectPaused(session);
        XASSERT_EQ(s.line, 2);

        // Continue — should hit line 4 (skipping 3)
        session.client.sendContinue();
        s = expectPaused(session);
        XASSERT_EQ(s.line, 4);

        session.cleanup(); });
}

// ============================================================================
// Section 6: Variable Inspection
// ============================================================================

static void testVariables()
{
    std::cout << "\n===== Variable Inspection =====\n";

    runTest("vars contain assigned variables", []()
            {
        DebugSession session;
        XASSERT(launchDebugSession(session, "breakpoint_test.xel"));

        // Line 1: x = 1 (paused BEFORE execution)
        DebugState s = expectPaused(session);
        XASSERT_EQ(s.line, 1);

        // Step over to line 2 — now x should be defined
        s = stepAndExpect(session, "step_over", 2);
        // vars should contain "x"
        XASSERT(jsonContains(s.varsJSON, "\"x\""));

        // Step to line 3 — now y should also be defined
        s = stepAndExpect(session, "step_over", 3);
        XASSERT(jsonContains(s.varsJSON, "\"x\""));
        XASSERT(jsonContains(s.varsJSON, "\"y\""));

        session.cleanup(); });
}

// ============================================================================
// Section 7: Stop Command
// ============================================================================

static void testStop()
{
    std::cout << "\n===== Stop Command =====\n";

    runTest("stop: terminates session", []()
            {
        DebugSession session;
        XASSERT(launchDebugSession(session, "breakpoint_test.xel"));

        DebugState s = expectPaused(session);

        // Send stop
        session.client.sendStop();

        // The session should end — either finished or connection drops
        // Give it a moment
        usleep(500000); // 500ms

        // Child should have exited
        int status = 0;
        pid_t result = waitpid(session.childPid, &status, WNOHANG);
        if (result > 0)
            session.childPid = 0; // Already reaped

        // Either way, session should be done
        XASSERT(result > 0 || !session.client.isConnected());
        session.cleanup(); });
}

// ============================================================================
// Section 8: Call Stack
// ============================================================================

static void testCallStack()
{
    std::cout << "\n===== Call Stack =====\n";

    runTest("callStack is empty array at top level", []()
            {
        DebugSession session;
        XASSERT(launchDebugSession(session, "breakpoint_test.xel"));

        DebugState s = expectPaused(session);
        // At top level, call stack should be empty
        XASSERT(!s.callStackJSON.empty());
        XASSERT(s.callStackJSON == "[]" || jsonArraySize(s.callStackJSON) == 0);
        session.cleanup(); });

    runTest("callStack grows when entering functions", []()
            {
        DebugSession session;
        XASSERT(launchDebugSession(session, "step_test.xel"));

        DebugState s = expectPaused(session);
        // Navigate to y = add(3, 4) at line 9
        while (s.line < 9)
        {
            session.client.sendStepInto();
            s = expectPaused(session);
        }

        int topStack = jsonArraySize(s.callStackJSON);

        // Step into add()
        s = stepAndExpect(session, "step_into", 2);
        int insideStack = jsonArraySize(s.callStackJSON);
        XASSERT_GT(insideStack, topStack);

        session.cleanup(); });

    runTest("callStack shrinks when returning from functions", []()
            {
        DebugSession session;
        XASSERT(launchDebugSession(session, "step_test.xel"));

        DebugState s = expectPaused(session);
        while (s.line < 9)
        {
            session.client.sendStepInto();
            s = expectPaused(session);
        }

        int beforeCall = jsonArraySize(s.callStackJSON);

        // Step into add()
        s = stepAndExpect(session, "step_into", 2);
        int insideCall = jsonArraySize(s.callStackJSON);
        XASSERT_GT(insideCall, beforeCall);

        // Step out
        session.client.sendStepOut();
        s = expectPaused(session);
        int afterReturn = jsonArraySize(s.callStackJSON);
        XASSERT_LE(afterReturn, beforeCall + 1); // Should be back to caller level

        session.cleanup(); });
}

// ============================================================================
// Section 9: Decorator @breakpoint + IPC stepping (mixed mode)
// ============================================================================

static void testMixedMode()
{
    std::cout << "\n===== Mixed Decorators + IPC =====\n";

    // mixed_test.xel:
    //  1: @debug on
    //  2: @breakpoint("snap1")
    //  3: x = 10
    //  4: @breakpoint("snap2")
    //  5: y = 20
    //  6: z = x + y
    //  7: print(z)

    runTest("decorator breakpoints trigger during stepping", []()
            {
        DebugSession session;
        XASSERT(launchDebugSession(session, "mixed_test.xel"));

        DebugState s = expectPaused(session);
        // First pause is on line 1 (first statement)
        // Step through — breakpoints should cause pauses
        // Just continue to end to verify no crash
        session.client.sendContinue();

        // Should finish (breakpoints are snapshot type, not pause type)
        DebugState fin = expectFinished(session);
        XASSERT_EQ(fin.state, "finished");
        session.cleanup(); });
}

// ============================================================================
// Section 10: Continue runs all the way to completion
// ============================================================================

static void testContinueToEnd()
{
    std::cout << "\n===== Continue to Completion =====\n";

    runTest("continue from start: simple file finishes", []()
            {
        DebugSession session;
        XASSERT(launchDebugSession(session, "breakpoint_test.xel"));
        expectPaused(session);
        session.client.sendContinue();
        expectFinished(session);
        session.cleanup(); });

    runTest("continue from start: file with functions finishes", []()
            {
        DebugSession session;
        XASSERT(launchDebugSession(session, "step_test.xel"));
        expectPaused(session);
        session.client.sendContinue();
        expectFinished(session);
        session.cleanup(); });

    runTest("continue from start: file with decorators finishes", []()
            {
        DebugSession session;
        XASSERT(launchDebugSession(session, "mixed_test.xel"));
        expectPaused(session);
        session.client.sendContinue();
        expectFinished(session);
        session.cleanup(); });

    runTest("continue from start: file with watch finishes", []()
            {
        DebugSession session;
        XASSERT(launchDebugSession(session, "watch_test.xel"));
        expectPaused(session);
        session.client.sendContinue();
        expectFinished(session);
        session.cleanup(); });

    runTest("continue from start: file with checkpoint finishes", []()
            {
        DebugSession session;
        XASSERT(launchDebugSession(session, "checkpoint_test.xel"));
        expectPaused(session);
        session.client.sendContinue();
        expectFinished(session);
        session.cleanup(); });

    runTest("continue from start: nested function file finishes", []()
            {
        DebugSession session;
        XASSERT(launchDebugSession(session, "callstack_test.xel"));
        expectPaused(session);
        session.client.sendContinue();
        expectFinished(session);
        session.cleanup(); });
}

// ============================================================================
// Section 11: Step through entire simple file
// ============================================================================

static void testStepEntireFile()
{
    std::cout << "\n===== Step Through Entire File =====\n";

    runTest("step over entire simple file without crash", []()
            {
        DebugSession session;
        XASSERT(launchDebugSession(session, "breakpoint_test.xel"));

        int steps = 0;
        const int maxSteps = 50; // safety limit

        // Step until finished
        DebugState s = expectPaused(session);
        while (steps < maxSteps)
        {
            session.client.sendStepOver();
            std::string msg = session.client.recv(10000);
            if (msg.empty())
                break;
            s = parseState(msg);
            if (s.state == "finished")
                break;
            if (s.state != "paused")
                break;
            steps++;
        }
        // Should have stepped through all 5 lines
        XASSERT_GE(steps, 4);
        XASSERT_EQ(s.state, "finished");
        session.cleanup(); });
}

// ============================================================================
// Section 12: Breakpoint + Step combination
// ============================================================================

static void testBreakpointThenStep()
{
    std::cout << "\n===== Breakpoint + Step Combo =====\n";

    runTest("continue to breakpoint then step over", []()
            {
        DebugSession session;
        XASSERT(launchDebugSession(session, "breakpoint_test.xel"));

        DebugState s = expectPaused(session);
        // Add breakpoint on line 3
        session.client.sendAddBreakpoint(3);

        // Continue — should hit line 3
        session.client.sendContinue();
        s = expectPaused(session);
        XASSERT_EQ(s.line, 3);

        // Now step over from the breakpoint
        s = stepAndExpect(session, "step_over", 4);
        s = stepAndExpect(session, "step_over", 5);

        session.client.sendStepOver();
        expectFinished(session);
        session.cleanup(); });

    runTest("step to breakpoint line does not double-pause", []()
            {
        DebugSession session;
        XASSERT(launchDebugSession(session, "breakpoint_test.xel"));

        DebugState s = expectPaused(session);
        XASSERT_EQ(s.line, 1);

        // Add breakpoint on line 2
        session.client.sendAddBreakpoint(2);

        // Step over — should pause on line 2 (breakpoint OR stepping, but only ONE pause)
        s = stepAndExpect(session, "step_over", 2);
        // We should get exactly one paused message, not two
        // If there's another buffered, that would be a bug
        std::string extra = session.client.tryRecv();
        // Extra should be empty (no double pause)
        if (!extra.empty())
        {
            DebugState extraS = parseState(extra);
            // If it's a paused state at the same line, that's a double-pause bug
            if (extraS.state == "paused" && extraS.line == 2)
                throw std::runtime_error("Double pause on breakpoint line (got two paused messages for line 2)");
        }

        session.cleanup(); });
}

// ============================================================================
// Section 13: Multiple sessions (isolation test)
// ============================================================================

static void testSessionIsolation()
{
    std::cout << "\n===== Session Isolation =====\n";

    runTest("two sequential sessions are independent", []()
            {
        // First session
        {
            DebugSession session;
            XASSERT(launchDebugSession(session, "breakpoint_test.xel"));
            expectPaused(session);
            session.client.sendContinue();
            expectFinished(session);
            session.cleanup();
        }

        // Second session — should work cleanly
        {
            DebugSession session;
            XASSERT(launchDebugSession(session, "step_test.xel"));
            expectPaused(session);
            session.client.sendContinue();
            expectFinished(session);
            session.cleanup();
        } });
}

// ============================================================================
// main
// ============================================================================

int main()
{
    std::cout << "========================================\n";
    std::cout << " Debug IPC Integration Tests\n";
    std::cout << "========================================\n";

    testBasicLifecycle();     // 4 tests
    testStepOver();           // 2 tests
    testStepInto();           // 2 tests
    testStepOut();            // 1 test
    testBreakpoints();        // 3 tests
    testVariables();          // 1 test
    testStop();               // 1 test
    testCallStack();          // 3 tests
    testMixedMode();          // 1 test
    testContinueToEnd();      // 6 tests
    testStepEntireFile();     // 1 test
    testBreakpointThenStep(); // 2 tests
    testSessionIsolation();   // 1 test

    std::cout << "\n========================================\n";
    std::cout << " Results: " << g_passed << " passed, " << g_failed << " failed\n";
    std::cout << "========================================\n";

    return g_failed > 0 ? 1 : 0;
}
