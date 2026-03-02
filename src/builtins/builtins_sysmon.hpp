#pragma once

// =============================================================================
// System Monitoring builtins — cross-platform system stats & hardware info
// =============================================================================
// free, vmstat, iostat, mpstat, sar, cpu_count, cpu_usage,
// mem_total, mem_free, mem_used, lscpu, lsmem, lspci, lsusb, lsblk,
// fdisk, mount_fs, umount_fs, dmesg, journalctl, w_cmd, last_logins,
// ulimit_info, cal, date_str
//
// Most functions return structured data (maps/lists) when called as
// functions, and the same data can be used in command style.
// =============================================================================

#include "builtin_registry.hpp"
#include "../lib/errors/error.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cmath>
#include <sstream>
#include <string>
#include <vector>
#include <fstream>
#include <chrono>
#include <thread>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#endif

namespace xell
{

    // Helper: run a shell command and capture stdout
    static inline std::string captureSysCmd(const std::string &cmd)
    {
        std::string result;
        FILE *fp = popen(cmd.c_str(), "r");
        if (!fp)
            return "";
        char buf[4096];
        while (fgets(buf, sizeof(buf), fp))
            result += buf;
        pclose(fp);
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
            result.pop_back();
        return result;
    }

    inline void registerSysMonBuiltins(BuiltinTable &t)
    {
        // =================================================================
        // free() — RAM info → map {total, used, free, swap_total, swap_free}
        // =================================================================
        t["free"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("free", 0, (int)args.size(), line);
            XMap m;
#ifdef _WIN32
            MEMORYSTATUSEX ms;
            ms.dwLength = sizeof(ms);
            if (GlobalMemoryStatusEx(&ms))
            {
                m.set("total", XObject::makeInt((int64_t)ms.ullTotalPhys));
                m.set("free", XObject::makeInt((int64_t)ms.ullAvailPhys));
                m.set("used", XObject::makeInt((int64_t)(ms.ullTotalPhys - ms.ullAvailPhys)));
                m.set("swap_total", XObject::makeInt((int64_t)ms.ullTotalPageFile));
                m.set("swap_free", XObject::makeInt((int64_t)ms.ullAvailPageFile));
            }
#else
            std::ifstream ifs("/proc/meminfo");
            if (ifs)
            {
                std::string key;
                int64_t val;
                std::string unit;
                int64_t memTotal = 0, memFree = 0, memAvail = 0, buffers = 0, cached = 0;
                int64_t swapTotal = 0, swapFree = 0;
                std::string sl;
                while (std::getline(ifs, sl))
                {
                    std::istringstream iss(sl);
                    iss >> key >> val >> unit;
                    if (key == "MemTotal:")
                        memTotal = val * 1024;
                    else if (key == "MemFree:")
                        memFree = val * 1024;
                    else if (key == "MemAvailable:")
                        memAvail = val * 1024;
                    else if (key == "Buffers:")
                        buffers = val * 1024;
                    else if (key == "Cached:")
                        cached = val * 1024;
                    else if (key == "SwapTotal:")
                        swapTotal = val * 1024;
                    else if (key == "SwapFree:")
                        swapFree = val * 1024;
                }
                m.set("total", XObject::makeInt(memTotal));
                m.set("free", XObject::makeInt(memAvail > 0 ? memAvail : memFree));
                m.set("used", XObject::makeInt(memTotal - (memAvail > 0 ? memAvail : memFree)));
                m.set("buffers", XObject::makeInt(buffers));
                m.set("cached", XObject::makeInt(cached));
                m.set("swap_total", XObject::makeInt(swapTotal));
                m.set("swap_free", XObject::makeInt(swapFree));
            }
#endif
            return XObject::makeMap(std::move(m));
        };

        // =================================================================
        // vmstat() — virtual memory statistics as string
        // =================================================================
        t["vmstat"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("vmstat", 0, (int)args.size(), line);
#ifdef _WIN32
            return XObject::makeString("vmstat not available on Windows");
#else
            return XObject::makeString(captureSysCmd("vmstat 2>/dev/null"));
#endif
        };

        // =================================================================
        // iostat() — CPU and disk I/O statistics as string
        // =================================================================
        t["iostat"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("iostat", 0, (int)args.size(), line);
#ifdef _WIN32
            return XObject::makeString("iostat not available on Windows");
#else
            return XObject::makeString(captureSysCmd("iostat 2>/dev/null || cat /proc/diskstats 2>/dev/null"));
#endif
        };

        // =================================================================
        // mpstat() — per-CPU statistics as string
        // =================================================================
        t["mpstat"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("mpstat", 0, (int)args.size(), line);
#ifdef _WIN32
            return XObject::makeString("mpstat not available on Windows");
#else
            return XObject::makeString(captureSysCmd("mpstat 2>/dev/null || cat /proc/stat 2>/dev/null"));
#endif
        };

        // =================================================================
        // sar() — system activity report as string
        // =================================================================
        t["sar"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("sar", 0, (int)args.size(), line);
#ifdef _WIN32
            return XObject::makeString("sar not available on Windows");
#else
            return XObject::makeString(captureSysCmd("sar 1 1 2>/dev/null"));
#endif
        };

        // =================================================================
        // cpu_count() — number of logical CPU cores
        // =================================================================
        t["cpu_count"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("cpu_count", 0, (int)args.size(), line);
#ifdef _WIN32
            SYSTEM_INFO si;
            GetNativeSystemInfo(&si);
            return XObject::makeInt((int64_t)si.dwNumberOfProcessors);
#else
            long n = sysconf(_SC_NPROCESSORS_ONLN);
            return XObject::makeInt(n > 0 ? (int64_t)n : 1);
#endif
        };

        // =================================================================
        // cpu_usage() — current CPU usage percentage (snapshot)
        // Reads /proc/stat twice with 100ms delay
        // =================================================================
        t["cpu_usage"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("cpu_usage", 0, (int)args.size(), line);
#ifdef _WIN32
            return XObject::makeFloat(0.0);
#else
            auto readCpuStats = []() -> std::vector<int64_t>
            {
                std::ifstream ifs("/proc/stat");
                std::string l;
                std::getline(ifs, l);
                std::istringstream iss(l);
                std::string cpu;
                iss >> cpu;
                std::vector<int64_t> vals;
                int64_t v;
                while (iss >> v)
                    vals.push_back(v);
                return vals;
            };

            auto s1 = readCpuStats();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            auto s2 = readCpuStats();

            if (s1.size() < 4 || s2.size() < 4)
                return XObject::makeFloat(0.0);

            int64_t total1 = 0, total2 = 0;
            for (auto v : s1)
                total1 += v;
            for (auto v : s2)
                total2 += v;
            int64_t idle1 = s1[3], idle2 = s2[3]; // 4th field is idle

            int64_t totalDiff = total2 - total1;
            int64_t idleDiff = idle2 - idle1;

            if (totalDiff == 0)
                return XObject::makeFloat(0.0);
            double usage = 100.0 * (1.0 - (double)idleDiff / (double)totalDiff);
            return XObject::makeFloat(usage);
#endif
        };

        // =================================================================
        // mem_total() — total RAM in bytes
        // =================================================================
        t["mem_total"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("mem_total", 0, (int)args.size(), line);
#ifdef _WIN32
            MEMORYSTATUSEX ms;
            ms.dwLength = sizeof(ms);
            if (GlobalMemoryStatusEx(&ms))
                return XObject::makeInt((int64_t)ms.ullTotalPhys);
            return XObject::makeInt(0);
#else
            struct sysinfo si;
            if (::sysinfo(&si) == 0)
                return XObject::makeInt((int64_t)si.totalram * si.mem_unit);
            return XObject::makeInt(0);
#endif
        };

        // =================================================================
        // mem_free() — free RAM in bytes
        // =================================================================
        t["mem_free"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("mem_free", 0, (int)args.size(), line);
#ifdef _WIN32
            MEMORYSTATUSEX ms;
            ms.dwLength = sizeof(ms);
            if (GlobalMemoryStatusEx(&ms))
                return XObject::makeInt((int64_t)ms.ullAvailPhys);
            return XObject::makeInt(0);
#else
            // Use MemAvailable from /proc/meminfo (more accurate than sysinfo)
            std::ifstream ifs("/proc/meminfo");
            std::string sl;
            while (std::getline(ifs, sl))
            {
                if (sl.substr(0, 14) == "MemAvailable:")
                {
                    std::istringstream iss(sl.substr(14));
                    int64_t val;
                    iss >> val;
                    return XObject::makeInt(val * 1024);
                }
            }
            struct sysinfo si;
            if (::sysinfo(&si) == 0)
                return XObject::makeInt((int64_t)si.freeram * si.mem_unit);
            return XObject::makeInt(0);
#endif
        };

        // =================================================================
        // mem_used() — used RAM in bytes
        // =================================================================
        t["mem_used"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("mem_used", 0, (int)args.size(), line);
#ifdef _WIN32
            MEMORYSTATUSEX ms;
            ms.dwLength = sizeof(ms);
            if (GlobalMemoryStatusEx(&ms))
                return XObject::makeInt((int64_t)(ms.ullTotalPhys - ms.ullAvailPhys));
            return XObject::makeInt(0);
#else
            int64_t total = 0, avail = 0;
            std::ifstream ifs("/proc/meminfo");
            std::string sl;
            while (std::getline(ifs, sl))
            {
                if (sl.substr(0, 9) == "MemTotal:")
                {
                    std::istringstream iss(sl.substr(9));
                    iss >> total;
                    total *= 1024;
                }
                else if (sl.substr(0, 14) == "MemAvailable:")
                {
                    std::istringstream iss(sl.substr(14));
                    iss >> avail;
                    avail *= 1024;
                }
            }
            return XObject::makeInt(total - avail);
#endif
        };

        // =================================================================
        // lscpu() — detailed CPU info (wraps command or reads /proc/cpuinfo)
        // =================================================================
        t["lscpu"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("lscpu", 0, (int)args.size(), line);
#ifdef _WIN32
            return XObject::makeString(captureSysCmd("wmic cpu get /format:list"));
#else
            std::string result = captureSysCmd("lscpu 2>/dev/null");
            if (result.empty())
            {
                // Fallback to /proc/cpuinfo
                std::ifstream ifs("/proc/cpuinfo");
                if (ifs)
                {
                    std::ostringstream oss;
                    oss << ifs.rdbuf();
                    result = oss.str();
                }
            }
            return XObject::makeString(result);
#endif
        };

        // =================================================================
        // lsmem() — memory info
        // =================================================================
        t["lsmem"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("lsmem", 0, (int)args.size(), line);
#ifdef _WIN32
            return XObject::makeString(captureSysCmd("wmic memorychip get /format:list"));
#else
            std::string result = captureSysCmd("lsmem 2>/dev/null");
            if (result.empty())
            {
                std::ifstream ifs("/proc/meminfo");
                if (ifs)
                {
                    std::ostringstream oss;
                    oss << ifs.rdbuf();
                    result = oss.str();
                }
            }
            return XObject::makeString(result);
#endif
        };

        // =================================================================
        // lspci() — list PCI devices
        // =================================================================
        t["lspci"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("lspci", 0, (int)args.size(), line);
#ifdef _WIN32
            return XObject::makeString(captureSysCmd("wmic path Win32_PnPEntity get Name"));
#else
            return XObject::makeString(captureSysCmd("lspci 2>/dev/null"));
#endif
        };

        // =================================================================
        // lsusb() — list USB devices
        // =================================================================
        t["lsusb"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("lsusb", 0, (int)args.size(), line);
#ifdef _WIN32
            return XObject::makeString(captureSysCmd("wmic path Win32_USBHub get Name"));
#else
            return XObject::makeString(captureSysCmd("lsusb 2>/dev/null"));
#endif
        };

        // =================================================================
        // lsblk() — list block storage devices
        // =================================================================
        t["lsblk"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("lsblk", 0, (int)args.size(), line);
#ifdef _WIN32
            return XObject::makeString(captureSysCmd("wmic diskdrive get /format:list"));
#else
            return XObject::makeString(captureSysCmd("lsblk 2>/dev/null"));
#endif
        };

        // =================================================================
        // fdisk(device) — partition info
        // =================================================================
        t["fdisk"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("fdisk", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("fdisk() expects a string device path", line);
#ifdef _WIN32
            return XObject::makeString("fdisk not available on Windows");
#else
            return XObject::makeString(captureSysCmd("fdisk -l " + args[0].asString() + " 2>/dev/null"));
#endif
        };

        // =================================================================
        // mount_fs(device, mount_point) — mount filesystem
        // =================================================================
        t["mount_fs"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("mount_fs", 2, (int)args.size(), line);
            if (!args[0].isString() || !args[1].isString())
                throw TypeError("mount_fs() expects two strings", line);
#ifdef _WIN32
            throw RuntimeError("mount_fs() is not available on Windows", line);
#else
            std::string cmd = "mount " + args[0].asString() + " " + args[1].asString() + " 2>&1";
            std::string result = captureSysCmd(cmd);
            return XObject::makeString(result);
#endif
        };

        // =================================================================
        // umount_fs(mount_point) — unmount filesystem
        // =================================================================
        t["umount_fs"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("umount_fs", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("umount_fs() expects a string mount point", line);
#ifdef _WIN32
            throw RuntimeError("umount_fs() is not available on Windows", line);
#else
            std::string cmd = "umount " + args[0].asString() + " 2>&1";
            return XObject::makeString(captureSysCmd(cmd));
#endif
        };

        // =================================================================
        // dmesg() — kernel ring buffer messages
        // =================================================================
        t["dmesg"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("dmesg", 0, (int)args.size(), line);
#ifdef _WIN32
            return XObject::makeString("dmesg not available on Windows");
#else
            return XObject::makeString(captureSysCmd("dmesg 2>/dev/null | tail -50"));
#endif
        };

        // =================================================================
        // journalctl(n=50) — query systemd journal, last n entries
        // =================================================================
        t["journalctl"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() > 1)
                throw ArityError("journalctl", 0, (int)args.size(), line);
            int n = 50;
            if (args.size() == 1 && args[0].isInt())
                n = (int)args[0].asInt();
#ifdef _WIN32
            return XObject::makeString("journalctl not available on Windows");
#else
            return XObject::makeString(captureSysCmd("journalctl -n " + std::to_string(n) + " --no-pager 2>/dev/null"));
#endif
        };

        // =================================================================
        // w_cmd() — show logged in users and activity
        // =================================================================
        t["w_cmd"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("w_cmd", 0, (int)args.size(), line);
#ifdef _WIN32
            return XObject::makeString(captureSysCmd("query user"));
#else
            return XObject::makeString(captureSysCmd("w 2>/dev/null"));
#endif
        };

        // =================================================================
        // last_logins(n=10) — show login history
        // =================================================================
        t["last_logins"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() > 1)
                throw ArityError("last_logins", 0, (int)args.size(), line);
            int n = 10;
            if (args.size() == 1 && args[0].isInt())
                n = (int)args[0].asInt();
#ifdef _WIN32
            return XObject::makeString("last_logins not available on Windows");
#else
            return XObject::makeString(captureSysCmd("last -n " + std::to_string(n) + " 2>/dev/null"));
#endif
        };

        // =================================================================
        // ulimit_info() — show resource limits
        // =================================================================
        t["ulimit_info"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("ulimit_info", 0, (int)args.size(), line);
#ifdef _WIN32
            return XObject::makeString("ulimit not available on Windows");
#else
            return XObject::makeString(captureSysCmd("ulimit -a 2>/dev/null"));
#endif
        };

        // =================================================================
        // cal(month=0, year=0) — print calendar
        // No args → current month; (year) → full year; (month, year) → specific
        // =================================================================
        t["cal"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() > 2)
                throw ArityError("cal", 0, (int)args.size(), line);

            if (args.empty())
            {
                // Current month calendar — pure C++ implementation
                auto now = std::chrono::system_clock::now();
                time_t t = std::chrono::system_clock::to_time_t(now);
                struct tm *tm = localtime(&t);
                int month = tm->tm_mon + 1;
                int year = tm->tm_year + 1900;

                // Build calendar
                std::ostringstream oss;
                static const char *monthNames[] = {"", "January", "February", "March",
                                                   "April", "May", "June", "July", "August",
                                                   "September", "October", "November", "December"};
                static const int daysInMonth[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

                std::string header = std::string(monthNames[month]) + " " + std::to_string(year);
                int pad = (20 - (int)header.size()) / 2;
                oss << std::string(std::max(0, pad), ' ') << header << "\n";
                oss << "Su Mo Tu We Th Fr Sa\n";

                // Days in this month (handle leap year)
                int days = daysInMonth[month];
                if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0))
                    days = 29;

                // Find day of week for the 1st (Zeller's or mktime)
                struct tm first = {};
                first.tm_year = year - 1900;
                first.tm_mon = month - 1;
                first.tm_mday = 1;
                mktime(&first);
                int dow = first.tm_wday; // 0=Sun

                // Print leading spaces
                for (int i = 0; i < dow; i++)
                    oss << "   ";

                // Print days
                for (int d = 1; d <= days; d++)
                {
                    oss << std::setw(2) << d;
                    if ((dow + d) % 7 == 0)
                        oss << "\n";
                    else
                        oss << " ";
                }
                return XObject::makeString(oss.str());
            }
            else
            {
                // Use cal command for specific month/year
                std::string cmd = "cal";
                if (args.size() == 1 && args[0].isInt())
                    cmd += " " + std::to_string(args[0].asInt());
                else if (args.size() == 2 && args[0].isInt() && args[1].isInt())
                    cmd += " " + std::to_string(args[0].asInt()) + " " + std::to_string(args[1].asInt());
#ifdef _WIN32
                // No cal on Windows, use our own for current month
                return XObject::makeString("cal not available with arguments on Windows");
#else
                return XObject::makeString(captureSysCmd(cmd + " 2>/dev/null"));
#endif
            }
        };

        // =================================================================
        // date_str(format="") — current date/time as formatted string
        // No format → default "YYYY-MM-DD HH:MM:SS"
        // With format → strftime format
        // =================================================================
        t["date_str"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() > 1)
                throw ArityError("date_str", 0, (int)args.size(), line);
            std::string fmt = "%Y-%m-%d %H:%M:%S";
            if (args.size() == 1)
            {
                if (!args[0].isString())
                    throw TypeError("date_str() format must be a string", line);
                fmt = args[0].asString();
            }
            auto now = std::chrono::system_clock::now();
            time_t t = std::chrono::system_clock::to_time_t(now);
            struct tm *tm = localtime(&t);
            char buf[256];
            strftime(buf, sizeof(buf), fmt.c_str(), tm);
            return XObject::makeString(std::string(buf));
        };
    }

} // namespace xell
