#pragma once

// =============================================================================
// Process & System builtins — cross-platform process management & system info
// =============================================================================
// ps, kill, kill_name, pkill, pgrep, pidof, pstree, jobs, bg, fg,
// nohup, nice, wait, ppid, exec, spawn, run_timeout, signal,
// exit_proc, getuid, is_root, id, whoami, hostname, uname, uptime,
// time_cmd, watch, strace, lsof, sys_info, os_name, arch
//
// NOTE: Some functions (pid, exit, run, run_capture) already exist in
// builtins_os.hpp and are NOT duplicated here.
// =============================================================================

#include "builtin_registry.hpp"
#include "../lib/errors/error.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <regex>
#include <algorithm>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#include <process.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <pwd.h>
#include <grp.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <dirent.h>
#endif

namespace xell
{

    // Helper: run a shell command and capture stdout
    static inline std::string captureCmd(const std::string &cmd)
    {
        std::string result;
        FILE *fp = popen(cmd.c_str(), "r");
        if (!fp)
            return "";
        char buf[4096];
        while (fgets(buf, sizeof(buf), fp))
            result += buf;
        pclose(fp);
        // Trim trailing newline
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
            result.pop_back();
        return result;
    }

    inline void registerProcessBuiltins(BuiltinTable &t)
    {
        // =================================================================
        // ps() — list running processes → list of maps
        // Each map: {pid, name, cpu, mem} (cross-platform)
        // =================================================================
        t["ps"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("ps", 0, (int)args.size(), line);
#ifdef _WIN32
            std::vector<XObject> procs;
            HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (snap != INVALID_HANDLE_VALUE)
            {
                PROCESSENTRY32 pe;
                pe.dwSize = sizeof(pe);
                if (Process32First(snap, &pe))
                {
                    do
                    {
                        XMap m;
                        m.set("pid", XObject::makeInt((int64_t)pe.th32ProcessID));
                        m.set("name", XObject::makeString(pe.szExeFile));
                        m.set("ppid", XObject::makeInt((int64_t)pe.th32ParentProcessID));
                        procs.push_back(XObject::makeMap(std::move(m)));
                    } while (Process32Next(snap, &pe));
                }
                CloseHandle(snap);
            }
            return XObject::makeList(std::move(procs));
#else
            std::vector<XObject> procs;
            // Parse /proc for process info
            for (auto &entry : std::filesystem::directory_iterator("/proc"))
            {
                std::string name = entry.path().filename().string();
                // Check if directory name is a PID (all digits)
                bool isPid = !name.empty() && std::all_of(name.begin(), name.end(), ::isdigit);
                if (!isPid || !entry.is_directory())
                    continue;
                int64_t pid = std::stoll(name);
                // Read comm (process name)
                std::string commPath = "/proc/" + name + "/comm";
                std::string comm;
                std::ifstream cfs(commPath);
                if (cfs)
                    std::getline(cfs, comm);
                // Read status for PPid and memory
                std::string statusPath = "/proc/" + name + "/status";
                int64_t ppid = 0;
                int64_t vmrss = 0;
                std::ifstream sfs(statusPath);
                if (sfs)
                {
                    std::string sl;
                    while (std::getline(sfs, sl))
                    {
                        if (sl.substr(0, 5) == "PPid:")
                            ppid = std::stoll(sl.substr(5));
                        else if (sl.substr(0, 6) == "VmRSS:")
                        {
                            std::istringstream iss(sl.substr(6));
                            iss >> vmrss;
                            vmrss *= 1024; // kB to bytes
                        }
                    }
                }
                XMap m;
                m.set("pid", XObject::makeInt(pid));
                m.set("name", XObject::makeString(comm));
                m.set("ppid", XObject::makeInt(ppid));
                m.set("mem", XObject::makeInt(vmrss));
                procs.push_back(XObject::makeMap(std::move(m)));
            }
            return XObject::makeList(std::move(procs));
#endif
        };

        // =================================================================
        // kill(pid, signal=15) — send signal to process
        // =================================================================
        t["kill"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() < 1 || args.size() > 2)
                throw ArityError("kill", 1, (int)args.size(), line);
            if (!args[0].isInt())
                throw TypeError("kill() expects an integer PID", line);
            int sig = 15; // SIGTERM
            if (args.size() == 2)
            {
                if (!args[1].isInt())
                    throw TypeError("kill() signal must be an integer", line);
                sig = (int)args[1].asInt();
            }
#ifdef _WIN32
            HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)args[0].asInt());
            if (hProc)
            {
                TerminateProcess(hProc, 1);
                CloseHandle(hProc);
                return XObject::makeBool(true);
            }
            return XObject::makeBool(false);
#else
            int ret = ::kill((pid_t)args[0].asInt(), sig);
            return XObject::makeBool(ret == 0);
#endif
        };

        // =================================================================
        // kill_name(name) — kill all processes by name
        // =================================================================
        t["kill_name"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("kill_name", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("kill_name() expects a string process name", line);
#ifdef _WIN32
            std::string cmd = "taskkill /IM " + args[0].asString() + " /F";
            int ret = system(cmd.c_str());
            return XObject::makeInt((int64_t)ret);
#else
            std::string cmd = "killall " + args[0].asString() + " 2>/dev/null";
            int ret = system(cmd.c_str());
            return XObject::makeInt((int64_t)WEXITSTATUS(ret));
#endif
        };

        // =================================================================
        // pkill(pattern) — kill processes matching pattern
        // =================================================================
        t["pkill"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("pkill", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("pkill() expects a string pattern", line);
#ifdef _WIN32
            // No direct pkill on Windows
            return XObject::makeInt(0);
#else
            std::string cmd = "pkill " + args[0].asString() + " 2>/dev/null";
            int ret = system(cmd.c_str());
            return XObject::makeInt((int64_t)WEXITSTATUS(ret));
#endif
        };

        // =================================================================
        // pgrep(pattern) — find PIDs matching pattern → list of ints
        // =================================================================
        t["pgrep"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("pgrep", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("pgrep() expects a string pattern", line);
            std::string pattern = args[0].asString();
            std::vector<XObject> pids;
#ifdef _WIN32
            // Iterate processes on Windows
            HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (snap != INVALID_HANDLE_VALUE)
            {
                PROCESSENTRY32 pe;
                pe.dwSize = sizeof(pe);
                if (Process32First(snap, &pe))
                {
                    do
                    {
                        if (std::string(pe.szExeFile).find(pattern) != std::string::npos)
                            pids.push_back(XObject::makeInt((int64_t)pe.th32ProcessID));
                    } while (Process32Next(snap, &pe));
                }
                CloseHandle(snap);
            }
#else
            // Parse /proc
            for (auto &entry : std::filesystem::directory_iterator("/proc"))
            {
                std::string name = entry.path().filename().string();
                bool isPid = !name.empty() && std::all_of(name.begin(), name.end(), ::isdigit);
                if (!isPid)
                    continue;
                std::ifstream cfs("/proc/" + name + "/comm");
                std::string comm;
                if (cfs && std::getline(cfs, comm))
                {
                    if (comm.find(pattern) != std::string::npos)
                        pids.push_back(XObject::makeInt(std::stoll(name)));
                }
            }
#endif
            return XObject::makeList(std::move(pids));
        };

        // =================================================================
        // pidof(name) — get PID by exact process name
        // =================================================================
        t["pidof"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("pidof", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("pidof() expects a string name", line);
            std::string target = args[0].asString();
            std::vector<XObject> pids;
#ifdef _WIN32
            HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (snap != INVALID_HANDLE_VALUE)
            {
                PROCESSENTRY32 pe;
                pe.dwSize = sizeof(pe);
                if (Process32First(snap, &pe))
                {
                    do
                    {
                        if (std::string(pe.szExeFile) == target)
                            pids.push_back(XObject::makeInt((int64_t)pe.th32ProcessID));
                    } while (Process32Next(snap, &pe));
                }
                CloseHandle(snap);
            }
#else
            for (auto &entry : std::filesystem::directory_iterator("/proc"))
            {
                std::string name = entry.path().filename().string();
                bool isPid = !name.empty() && std::all_of(name.begin(), name.end(), ::isdigit);
                if (!isPid)
                    continue;
                std::ifstream cfs("/proc/" + name + "/comm");
                std::string comm;
                if (cfs && std::getline(cfs, comm))
                {
                    if (comm == target)
                        pids.push_back(XObject::makeInt(std::stoll(name)));
                }
            }
#endif
            return XObject::makeList(std::move(pids));
        };

        // =================================================================
        // pstree(pid=0) — process tree as string (0 = current process)
        // =================================================================
        t["pstree"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() > 1)
                throw ArityError("pstree", 0, (int)args.size(), line);
#ifdef _WIN32
            return XObject::makeString(captureCmd("tasklist /v"));
#else
            std::string cmd = "pstree";
            if (args.size() == 1 && args[0].isInt())
                cmd += " " + std::to_string(args[0].asInt());
            return XObject::makeString(captureCmd(cmd + " 2>/dev/null"));
#endif
        };

        // =================================================================
        // jobs() — list background jobs (shell integration)
        // =================================================================
        t["jobs"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("jobs", 0, (int)args.size(), line);
            return XObject::makeString(captureCmd("jobs 2>/dev/null"));
        };

        // =================================================================
        // bg(pid) — resume process in background
        // =================================================================
        t["bg"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("bg", 1, (int)args.size(), line);
            if (!args[0].isInt())
                throw TypeError("bg() expects an integer PID", line);
#ifdef _WIN32
            return XObject::makeBool(false);
#else
            int ret = ::kill((pid_t)args[0].asInt(), SIGCONT);
            return XObject::makeBool(ret == 0);
#endif
        };

        // =================================================================
        // fg(pid) — send SIGCONT (bring to foreground conceptually)
        // =================================================================
        t["fg"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("fg", 1, (int)args.size(), line);
            if (!args[0].isInt())
                throw TypeError("fg() expects an integer PID", line);
#ifdef _WIN32
            return XObject::makeBool(false);
#else
            int ret = ::kill((pid_t)args[0].asInt(), SIGCONT);
            return XObject::makeBool(ret == 0);
#endif
        };

        // =================================================================
        // nohup(cmd) — run command immune to hangup signal
        // Returns map {pid, command}
        // =================================================================
        t["nohup"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("nohup", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("nohup() expects a string command", line);
#ifdef _WIN32
            std::string cmd = "start /B " + args[0].asString();
            system(cmd.c_str());
            XMap m;
            m.set("command", args[0]);
            return XObject::makeMap(std::move(m));
#else
            std::string cmd = "nohup " + args[0].asString() + " > /dev/null 2>&1 & echo $!";
            std::string pidStr = captureCmd(cmd);
            XMap m;
            m.set("command", args[0]);
            try
            {
                m.set("pid", XObject::makeInt(std::stoll(pidStr)));
            }
            catch (...)
            {
                m.set("pid", XObject::makeInt(0));
            }
            return XObject::makeMap(std::move(m));
#endif
        };

        // =================================================================
        // nice(cmd, level=10) — run with adjusted priority
        // =================================================================
        t["nice"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() < 1 || args.size() > 2)
                throw ArityError("nice", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("nice() expects a string command", line);
            int level = 10;
            if (args.size() == 2)
            {
                if (!args[1].isInt())
                    throw TypeError("nice() level must be an integer", line);
                level = (int)args[1].asInt();
            }
#ifdef _WIN32
            int ret = system(args[0].asString().c_str());
            return XObject::makeInt((int64_t)ret);
#else
            std::string cmd = "nice -n " + std::to_string(level) + " " + args[0].asString();
            int ret = system(cmd.c_str());
            return XObject::makeInt((int64_t)WEXITSTATUS(ret));
#endif
        };

        // =================================================================
        // wait_pid(pid) — wait for process to finish, return exit status
        // =================================================================
        t["wait_pid"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("wait_pid", 1, (int)args.size(), line);
            if (!args[0].isInt())
                throw TypeError("wait_pid() expects an integer PID", line);
#ifdef _WIN32
            HANDLE hProc = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION,
                                       FALSE, (DWORD)args[0].asInt());
            if (!hProc)
                throw ProcessError("wait_pid(): cannot open process", line);
            WaitForSingleObject(hProc, INFINITE);
            DWORD code = 0;
            GetExitCodeProcess(hProc, &code);
            CloseHandle(hProc);
            return XObject::makeInt((int64_t)code);
#else
            int status = 0;
            pid_t result = waitpid((pid_t)args[0].asInt(), &status, 0);
            if (result == -1)
            {
                // Process may not be a child — poll with kill(0)
                while (::kill((pid_t)args[0].asInt(), 0) == 0)
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                return XObject::makeInt(0);
            }
            return XObject::makeInt((int64_t)(WIFEXITED(status) ? WEXITSTATUS(status) : -1));
#endif
        };

        // =================================================================
        // ppid() — parent process ID
        // =================================================================
        t["ppid"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("ppid", 0, (int)args.size(), line);
#ifdef _WIN32
            return XObject::makeInt(0); // No direct API
#else
            return XObject::makeInt((int64_t)getppid());
#endif
        };

        // =================================================================
        // spawn(cmd) — spawn subprocess, return PID immediately
        // =================================================================
        t["spawn"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("spawn", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("spawn() expects a string command", line);
#ifdef _WIN32
            STARTUPINFOA si;
            PROCESS_INFORMATION pi;
            ZeroMemory(&si, sizeof(si));
            si.cb = sizeof(si);
            ZeroMemory(&pi, sizeof(pi));
            std::string cmd = args[0].asString();
            std::vector<char> cmdBuf(cmd.begin(), cmd.end());
            cmdBuf.push_back('\0');
            if (!CreateProcessA(NULL, cmdBuf.data(), NULL, NULL, FALSE,
                                CREATE_NEW_PROCESS_GROUP, NULL, NULL, &si, &pi))
                throw ProcessError("spawn(): failed to create process", line);
            int64_t pid = (int64_t)pi.dwProcessId;
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            return XObject::makeInt(pid);
#else
            pid_t child = fork();
            if (child < 0)
                throw ProcessError("spawn(): fork failed", line);
            if (child == 0)
            {
                // Child process
                setsid(); // detach from parent
                execl("/bin/sh", "sh", "-c", args[0].asString().c_str(), nullptr);
                _exit(127);
            }
            return XObject::makeInt((int64_t)child);
#endif
        };

        // =================================================================
        // run_timeout(cmd, timeout_ms) — run command with timeout
        // Returns map {exit_code, timed_out, stdout}
        // =================================================================
        t["run_timeout"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("run_timeout", 2, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("run_timeout() expects a string command", line);
            if (!args[1].isInt())
                throw TypeError("run_timeout() timeout must be integer (ms)", line);
            int timeoutMs = (int)args[1].asInt();

#ifdef _WIN32
            STARTUPINFOA si;
            PROCESS_INFORMATION pi;
            ZeroMemory(&si, sizeof(si));
            si.cb = sizeof(si);
            ZeroMemory(&pi, sizeof(pi));
            std::string cmd = "cmd.exe /c " + args[0].asString();
            std::vector<char> cmdBuf(cmd.begin(), cmd.end());
            cmdBuf.push_back('\0');
            if (!CreateProcessA(NULL, cmdBuf.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
                throw ProcessError("run_timeout(): failed to create process", line);
            DWORD waitResult = WaitForSingleObject(pi.hProcess, timeoutMs);
            XMap m;
            if (waitResult == WAIT_TIMEOUT)
            {
                TerminateProcess(pi.hProcess, 1);
                m.set("timed_out", XObject::makeBool(true));
                m.set("exit_code", XObject::makeInt(-1));
            }
            else
            {
                DWORD code = 0;
                GetExitCodeProcess(pi.hProcess, &code);
                m.set("timed_out", XObject::makeBool(false));
                m.set("exit_code", XObject::makeInt((int64_t)code));
            }
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            return XObject::makeMap(std::move(m));
#else
            pid_t child = fork();
            if (child < 0)
                throw ProcessError("run_timeout(): fork failed", line);
            if (child == 0)
            {
                execl("/bin/sh", "sh", "-c", args[0].asString().c_str(), nullptr);
                _exit(127);
            }
            // Parent: wait with timeout
            auto start = std::chrono::steady_clock::now();
            int status = 0;
            bool timedOut = false;
            while (true)
            {
                pid_t w = waitpid(child, &status, WNOHANG);
                if (w > 0)
                    break;
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now() - start)
                                   .count();
                if (elapsed >= timeoutMs)
                {
                    ::kill(child, SIGKILL);
                    waitpid(child, &status, 0);
                    timedOut = true;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            XMap m;
            m.set("timed_out", XObject::makeBool(timedOut));
            m.set("exit_code", XObject::makeInt(
                                   timedOut ? -1 : (int64_t)(WIFEXITED(status) ? WEXITSTATUS(status) : -1)));
            return XObject::makeMap(std::move(m));
#endif
        };

        // =================================================================
        // getuid() — get user ID (Unix only, returns 0 on Windows)
        // =================================================================
        t["getuid"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("getuid", 0, (int)args.size(), line);
#ifdef _WIN32
            return XObject::makeInt(0);
#else
            return XObject::makeInt((int64_t)::getuid());
#endif
        };

        // =================================================================
        // is_root() — check if running as root/admin
        // =================================================================
        t["is_root"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("is_root", 0, (int)args.size(), line);
#ifdef _WIN32
            // Check admin on Windows (simplified)
            return XObject::makeBool(false);
#else
            return XObject::makeBool(::getuid() == 0);
#endif
        };

        // =================================================================
        // id() — return map with uid, gid, groups
        // =================================================================
        t["id"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("id", 0, (int)args.size(), line);
#ifdef _WIN32
            XMap m;
            m.set("uid", XObject::makeInt(0));
            m.set("gid", XObject::makeInt(0));
            m.set("user", XObject::makeString("unknown"));
            m.set("groups", XObject::makeList({}));
            return XObject::makeMap(std::move(m));
#else
            XMap m;
            m.set("uid", XObject::makeInt((int64_t)::getuid()));
            m.set("gid", XObject::makeInt((int64_t)::getgid()));
            struct passwd *pw = getpwuid(::getuid());
            m.set("user", XObject::makeString(pw ? pw->pw_name : "unknown"));

            // Get supplementary groups
            int ngroups = 64;
            std::vector<gid_t> groups(ngroups);
            if (getgroups(ngroups, groups.data()) >= 0)
            {
                std::vector<XObject> grpList;
                for (int i = 0; i < ngroups; i++)
                {
                    struct group *gr = getgrgid(groups[i]);
                    if (gr)
                        grpList.push_back(XObject::makeString(gr->gr_name));
                }
                m.set("groups", XObject::makeList(std::move(grpList)));
            }
            else
            {
                m.set("groups", XObject::makeList({}));
            }
            return XObject::makeMap(std::move(m));
#endif
        };

        // =================================================================
        // whoami() — current username
        // =================================================================
        t["whoami"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("whoami", 0, (int)args.size(), line);
#ifdef _WIN32
            char buf[256];
            DWORD size = sizeof(buf);
            if (GetUserNameA(buf, &size))
                return XObject::makeString(std::string(buf));
            return XObject::makeString("unknown");
#else
            struct passwd *pw = getpwuid(::getuid());
            return XObject::makeString(pw ? pw->pw_name : "unknown");
#endif
        };

        // =================================================================
        // hostname() — machine hostname
        // =================================================================
        t["hostname"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("hostname", 0, (int)args.size(), line);
            char buf[256];
#ifdef _WIN32
            DWORD size = sizeof(buf);
            if (GetComputerNameA(buf, &size))
                return XObject::makeString(std::string(buf));
            return XObject::makeString("unknown");
#else
            if (gethostname(buf, sizeof(buf)) == 0)
                return XObject::makeString(std::string(buf));
            return XObject::makeString("unknown");
#endif
        };

        // =================================================================
        // uname() — OS and kernel info → map
        // =================================================================
        t["uname"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("uname", 0, (int)args.size(), line);
#ifdef _WIN32
            XMap m;
            m.set("sysname", XObject::makeString("Windows"));
            m.set("nodename", XObject::makeString(captureCmd("hostname")));
            m.set("release", XObject::makeString(captureCmd("ver")));
            m.set("machine", XObject::makeString("x86_64"));
            return XObject::makeMap(std::move(m));
#else
            struct utsname u;
            XMap m;
            if (::uname(&u) == 0)
            {
                m.set("sysname", XObject::makeString(u.sysname));
                m.set("nodename", XObject::makeString(u.nodename));
                m.set("release", XObject::makeString(u.release));
                m.set("version", XObject::makeString(u.version));
                m.set("machine", XObject::makeString(u.machine));
            }
            return XObject::makeMap(std::move(m));
#endif
        };

        // =================================================================
        // uptime() — system uptime in seconds
        // =================================================================
        t["uptime"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("uptime", 0, (int)args.size(), line);
#ifdef _WIN32
            return XObject::makeInt((int64_t)(GetTickCount64() / 1000));
#else
            struct sysinfo si;
            if (::sysinfo(&si) == 0)
                return XObject::makeInt((int64_t)si.uptime);
            return XObject::makeInt(0);
#endif
        };

        // =================================================================
        // time_cmd(cmd) — measure command execution time
        // Returns map {elapsed_ms, exit_code}
        // =================================================================
        t["time_cmd"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("time_cmd", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("time_cmd() expects a string command", line);
            auto start = std::chrono::steady_clock::now();
            int ret = system(args[0].asString().c_str());
            auto end = std::chrono::steady_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
            XMap m;
            m.set("elapsed_ms", XObject::makeInt((int64_t)ms));
#ifdef _WIN32
            m.set("exit_code", XObject::makeInt((int64_t)ret));
#else
            m.set("exit_code", XObject::makeInt((int64_t)WEXITSTATUS(ret)));
#endif
            return XObject::makeMap(std::move(m));
        };

        // =================================================================
        // watch(cmd, interval_secs=2, count=1) — repeat command
        // Returns list of outputs
        // =================================================================
        t["watch"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() < 1 || args.size() > 3)
                throw ArityError("watch", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("watch() expects a string command", line);
            int intervalSecs = 2;
            int count = 1;
            if (args.size() >= 2 && args[1].isInt())
                intervalSecs = (int)args[1].asInt();
            if (args.size() >= 3 && args[2].isInt())
                count = (int)args[2].asInt();

            std::vector<XObject> results;
            for (int i = 0; i < count; i++)
            {
                if (i > 0)
                    std::this_thread::sleep_for(std::chrono::seconds(intervalSecs));
                results.push_back(XObject::makeString(captureCmd(args[0].asString())));
            }
            return XObject::makeList(std::move(results));
        };

        // =================================================================
        // strace(cmd) — trace system calls (Unix only, wraps strace)
        // Returns the strace output as string
        // =================================================================
        t["strace"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("strace", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("strace() expects a string command", line);
#ifdef _WIN32
            throw RuntimeError("strace() is not available on Windows", line);
#else
            std::string cmd = "strace -c " + args[0].asString() + " 2>&1";
            return XObject::makeString(captureCmd(cmd));
#endif
        };

        // =================================================================
        // lsof() — list open files
        // =================================================================
        t["lsof"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("lsof", 0, (int)args.size(), line);
#ifdef _WIN32
            return XObject::makeString(captureCmd("handle"));
#else
            return XObject::makeString(captureCmd("lsof -p " + std::to_string(getpid()) + " 2>/dev/null"));
#endif
        };

        // =================================================================
        // sys_info() — map of OS name, version, architecture
        // =================================================================
        t["sys_info"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("sys_info", 0, (int)args.size(), line);
            XMap m;
#ifdef _WIN32
            m.set("os", XObject::makeString("windows"));
            SYSTEM_INFO si;
            GetNativeSystemInfo(&si);
            switch (si.wProcessorArchitecture)
            {
            case PROCESSOR_ARCHITECTURE_AMD64:
                m.set("arch", XObject::makeString("x86_64"));
                break;
            case PROCESSOR_ARCHITECTURE_ARM64:
                m.set("arch", XObject::makeString("arm64"));
                break;
            default:
                m.set("arch", XObject::makeString("unknown"));
            }
            m.set("cpu_count", XObject::makeInt((int64_t)si.dwNumberOfProcessors));
#else
            struct utsname u;
            if (::uname(&u) == 0)
            {
                std::string sysname = u.sysname;
                std::transform(sysname.begin(), sysname.end(), sysname.begin(), ::tolower);
                m.set("os", XObject::makeString(sysname));
                m.set("arch", XObject::makeString(u.machine));
                m.set("kernel", XObject::makeString(u.release));
                m.set("hostname", XObject::makeString(u.nodename));
            }
            // CPU count
            long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
            m.set("cpu_count", XObject::makeInt(nprocs > 0 ? (int64_t)nprocs : 1));
#endif
            return XObject::makeMap(std::move(m));
        };

        // =================================================================
        // os_name() — "linux", "windows", or "macos"
        // =================================================================
        t["os_name"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("os_name", 0, (int)args.size(), line);
#ifdef _WIN32
            return XObject::makeString("windows");
#elif __APPLE__
            return XObject::makeString("macos");
#else
            return XObject::makeString("linux");
#endif
        };

        // =================================================================
        // arch() — "x86_64", "arm64", etc.
        // =================================================================
        t["arch"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("arch", 0, (int)args.size(), line);
#ifdef _WIN32
            SYSTEM_INFO si;
            GetNativeSystemInfo(&si);
            switch (si.wProcessorArchitecture)
            {
            case PROCESSOR_ARCHITECTURE_AMD64:
                return XObject::makeString("x86_64");
            case PROCESSOR_ARCHITECTURE_ARM64:
                return XObject::makeString("arm64");
            case PROCESSOR_ARCHITECTURE_INTEL:
                return XObject::makeString("x86");
            default:
                return XObject::makeString("unknown");
            }
#else
            struct utsname u;
            if (::uname(&u) == 0)
                return XObject::makeString(u.machine);
            return XObject::makeString("unknown");
#endif
        };

        // =================================================================
        // exec_proc(path, args_list) — replace process with exec (Unix)
        // =================================================================
        t["exec_proc"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() < 1 || args.size() > 2)
                throw ArityError("exec_proc", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("exec_proc() expects a string path", line);
#ifdef _WIN32
            throw RuntimeError("exec_proc() is not available on Windows", line);
#else
            std::string path = args[0].asString();
            std::vector<std::string> argStrs;
            argStrs.push_back(path);
            if (args.size() == 2 && args[1].isList())
            {
                for (auto &a : args[1].asList())
                    argStrs.push_back(a.isString() ? a.asString() : a.toString());
            }
            std::vector<char *> argv;
            for (auto &s : argStrs)
                argv.push_back(const_cast<char *>(s.c_str()));
            argv.push_back(nullptr);
            execv(path.c_str(), argv.data());
            // If we get here, exec failed
            throw ProcessError("exec_proc(): exec failed for " + path, line);
#endif
        };

        // =================================================================
        // signal_send(pid, signal) — send a specific signal
        // =================================================================
        t["signal_send"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("signal_send", 2, (int)args.size(), line);
            if (!args[0].isInt() || !args[1].isInt())
                throw TypeError("signal_send() expects (pid, signal) as integers", line);
#ifdef _WIN32
            return XObject::makeBool(false);
#else
            int ret = ::kill((pid_t)args[0].asInt(), (int)args[1].asInt());
            return XObject::makeBool(ret == 0);
#endif
        };
    }

} // namespace xell
