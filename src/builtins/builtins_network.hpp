#pragma once

// =============================================================================
// Network builtins — connectivity, HTTP, DNS, and network utilities
// =============================================================================
// ping, http_get, http_post, http_put, http_delete, download,
// dns_lookup, nslookup, host_lookup, whois, traceroute,
// netstat, ss, ifconfig, ip_cmd, route, iptables, ufw,
// nc, telnet_connect, rsync, local_ip, public_ip
//
// Most HTTP functions use system `curl` under the hood.
// DNS uses native getaddrinfo() where possible.
// Network utility commands wrap their system equivalents.
// =============================================================================

#include "builtin_registry.hpp"
#include "../lib/errors/error.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include <regex>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#endif

namespace xell
{

    // Helper: capture output of a shell command
    static inline std::string captureNetCmd(const std::string &cmd)
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

    // Helper: check if an external command exists
    static inline bool hasCmd(const std::string &name)
    {
#ifdef _WIN32
        std::string check = "where " + name + " >nul 2>&1";
#else
        std::string check = "command -v " + name + " >/dev/null 2>&1";
#endif
        return system(check.c_str()) == 0;
    }

    static inline void registerNetworkBuiltins(BuiltinTable &t)
    {
        // =================================================================
        // ping(host, count=4) — test connectivity to host
        // =================================================================
        t["ping"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.empty() || args.size() > 2)
                throw ArityError("ping", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("ping() expects a string hostname", line);
            int count = 4;
            if (args.size() == 2)
            {
                if (!args[1].isInt())
                    throw TypeError("ping() count must be an integer", line);
                count = (int)args[1].asInt();
            }
            std::string host = args[0].asString();
#ifdef _WIN32
            std::string cmd = "ping -n " + std::to_string(count) + " " + host + " 2>&1";
#else
            std::string cmd = "ping -c " + std::to_string(count) + " " + host + " 2>&1";
#endif
            return XObject::makeString(captureNetCmd(cmd));
        };

        // =================================================================
        // http_get(url, headers={}) — HTTP GET, returns {status, body, headers}
        // =================================================================
        t["http_get"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.empty() || args.size() > 2)
                throw ArityError("http_get", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("http_get() expects a string URL", line);
            std::string url = args[0].asString();

            // Build curl command
            std::string cmd = "curl -s -w '\\n%{http_code}' -D -";
            // Add custom headers
            if (args.size() == 2 && args[1].isMap())
            {
                for (auto it = args[1].asMap().begin(); it.valid(); it.next())
                    cmd += " -H '" + it.key().asString() + ": " + it.value().asString() + "'";
            }
            cmd += " '" + url + "' 2>/dev/null";

            std::string raw = captureNetCmd(cmd);

            // Parse response: headers\r\n\r\nbody\nstatus_code
            XMap result;

            // Find header/body boundary
            auto hdrEnd = raw.find("\r\n\r\n");
            if (hdrEnd == std::string::npos)
                hdrEnd = raw.find("\n\n");

            if (hdrEnd != std::string::npos)
            {
                std::string hdrs = raw.substr(0, hdrEnd);
                std::string rest = raw.substr(hdrEnd + (raw[hdrEnd + 1] == '\n' ? 2 : 4));

                // Last line of rest is the status code
                auto lastNl = rest.rfind('\n');
                std::string body, statusStr;
                if (lastNl != std::string::npos)
                {
                    body = rest.substr(0, lastNl);
                    statusStr = rest.substr(lastNl + 1);
                }
                else
                {
                    statusStr = rest;
                }

                int statusCode = 0;
                try
                {
                    statusCode = std::stoi(statusStr);
                }
                catch (...)
                {
                    body = rest;
                }

                result.set("status", XObject::makeInt(statusCode));
                result.set("body", XObject::makeString(body));
                result.set("headers", XObject::makeString(hdrs));
            }
            else
            {
                result.set("status", XObject::makeInt(0));
                result.set("body", XObject::makeString(raw));
                result.set("headers", XObject::makeString(""));
            }

            return XObject::makeMap(std::move(result));
        };

        // =================================================================
        // http_post(url, body, headers={}) — HTTP POST
        // =================================================================
        t["http_post"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() < 2 || args.size() > 3)
                throw ArityError("http_post", 2, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("http_post() URL must be a string", line);
            if (!args[1].isString())
                throw TypeError("http_post() body must be a string", line);

            std::string url = args[0].asString();
            std::string body = args[1].asString();

            std::string cmd = "curl -s -w '\\n%{http_code}' -X POST";

            // Content-Type default
            bool hasContentType = false;
            if (args.size() == 3 && args[2].isMap())
            {
                for (auto it = args[2].asMap().begin(); it.valid(); it.next())
                {
                    cmd += " -H '" + it.key().asString() + ": " + it.value().asString() + "'";
                    std::string lower = it.key().asString();
                    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                    if (lower == "content-type")
                        hasContentType = true;
                }
            }
            if (!hasContentType)
                cmd += " -H 'Content-Type: application/json'";

            cmd += " -d '" + body + "' '" + url + "' 2>/dev/null";

            std::string raw = captureNetCmd(cmd);

            // Parse: body\nstatus_code
            XMap result;
            auto lastNl = raw.rfind('\n');
            if (lastNl != std::string::npos)
            {
                std::string respBody = raw.substr(0, lastNl);
                std::string statusStr = raw.substr(lastNl + 1);
                int statusCode = 0;
                try
                {
                    statusCode = std::stoi(statusStr);
                }
                catch (...)
                {
                }
                result.set("status", XObject::makeInt(statusCode));
                result.set("body", XObject::makeString(respBody));
            }
            else
            {
                result.set("status", XObject::makeInt(0));
                result.set("body", XObject::makeString(raw));
            }
            return XObject::makeMap(std::move(result));
        };

        // =================================================================
        // http_put(url, body, headers={}) — HTTP PUT
        // =================================================================
        t["http_put"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() < 2 || args.size() > 3)
                throw ArityError("http_put", 2, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("http_put() URL must be a string", line);
            if (!args[1].isString())
                throw TypeError("http_put() body must be a string", line);

            std::string url = args[0].asString();
            std::string body = args[1].asString();
            std::string cmd = "curl -s -w '\\n%{http_code}' -X PUT";

            if (args.size() == 3 && args[2].isMap())
            {
                for (auto it = args[2].asMap().begin(); it.valid(); it.next())
                    cmd += " -H '" + it.key().asString() + ": " + it.value().asString() + "'";
            }
            cmd += " -d '" + body + "' '" + url + "' 2>/dev/null";

            std::string raw = captureNetCmd(cmd);
            XMap result;
            auto lastNl = raw.rfind('\n');
            if (lastNl != std::string::npos)
            {
                result.set("status", XObject::makeInt(0));
                try
                {
                    result.set("status", XObject::makeInt(std::stoi(raw.substr(lastNl + 1))));
                }
                catch (...)
                {
                }
                result.set("body", XObject::makeString(raw.substr(0, lastNl)));
            }
            else
            {
                result.set("status", XObject::makeInt(0));
                result.set("body", XObject::makeString(raw));
            }
            return XObject::makeMap(std::move(result));
        };

        // =================================================================
        // http_delete(url, headers={}) — HTTP DELETE
        // =================================================================
        t["http_delete"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.empty() || args.size() > 2)
                throw ArityError("http_delete", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("http_delete() URL must be a string", line);

            std::string url = args[0].asString();
            std::string cmd = "curl -s -w '\\n%{http_code}' -X DELETE";

            if (args.size() == 2 && args[1].isMap())
            {
                for (auto it = args[1].asMap().begin(); it.valid(); it.next())
                    cmd += " -H '" + it.key().asString() + ": " + it.value().asString() + "'";
            }
            cmd += " '" + url + "' 2>/dev/null";

            std::string raw = captureNetCmd(cmd);
            XMap result;
            auto lastNl = raw.rfind('\n');
            if (lastNl != std::string::npos)
            {
                result.set("status", XObject::makeInt(0));
                try
                {
                    result.set("status", XObject::makeInt(std::stoi(raw.substr(lastNl + 1))));
                }
                catch (...)
                {
                }
                result.set("body", XObject::makeString(raw.substr(0, lastNl)));
            }
            else
            {
                result.set("status", XObject::makeInt(0));
                result.set("body", XObject::makeString(raw));
            }
            return XObject::makeMap(std::move(result));
        };

        // =================================================================
        // download(url, path) — download file from URL
        // =================================================================
        t["download"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("download", 2, (int)args.size(), line);
            if (!args[0].isString() || !args[1].isString())
                throw TypeError("download() expects (url: string, path: string)", line);

            std::string url = args[0].asString();
            std::string path = args[1].asString();

#ifdef _WIN32
            std::string cmd = "curl -sL -o \"" + path + "\" \"" + url + "\" 2>&1";
#else
            std::string cmd;
            if (hasCmd("curl"))
                cmd = "curl -sL -o '" + path + "' '" + url + "' 2>&1";
            else if (hasCmd("wget"))
                cmd = "wget -q -O '" + path + "' '" + url + "' 2>&1";
            else
                throw RuntimeError("download() requires curl or wget", line);
#endif
            int ret = system(cmd.c_str());
            if (ret != 0)
                throw RuntimeError("download() failed: " + url, line);
            return XObject::makeBool(true);
        };

        // =================================================================
        // dns_lookup(hostname) — resolve hostname → list of IP addresses
        // Uses native getaddrinfo() — no external commands needed
        // =================================================================
        t["dns_lookup"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("dns_lookup", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("dns_lookup() expects a string hostname", line);

            std::string host = args[0].asString();

#ifdef _WIN32
            WSADATA wsaData;
            WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
            struct addrinfo hints = {}, *res = nullptr;
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;

            int status = getaddrinfo(host.c_str(), nullptr, &hints, &res);
            if (status != 0)
            {
#ifdef _WIN32
                WSACleanup();
#endif
                throw RuntimeError("dns_lookup() failed for: " + host, line);
            }

            std::vector<XObject> ips;
            for (struct addrinfo *p = res; p != nullptr; p = p->ai_next)
            {
                char ipStr[INET6_ADDRSTRLEN];
                if (p->ai_family == AF_INET)
                {
                    struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
                    inet_ntop(AF_INET, &(ipv4->sin_addr), ipStr, sizeof(ipStr));
                }
                else if (p->ai_family == AF_INET6)
                {
                    struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
                    inet_ntop(AF_INET6, &(ipv6->sin6_addr), ipStr, sizeof(ipStr));
                }
                else
                    continue;

                std::string ip(ipStr);
                // Deduplicate
                bool found = false;
                for (auto &existing : ips)
                    if (existing.asString() == ip)
                    {
                        found = true;
                        break;
                    }
                if (!found)
                    ips.push_back(XObject::makeString(ip));
            }
            freeaddrinfo(res);
#ifdef _WIN32
            WSACleanup();
#endif
            return XObject::makeList(std::move(ips));
        };

        // =================================================================
        // nslookup(hostname) — wrapper around system nslookup
        // =================================================================
        t["nslookup"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("nslookup", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("nslookup() expects a string hostname", line);
            return XObject::makeString(captureNetCmd("nslookup " + args[0].asString() + " 2>/dev/null"));
        };

        // =================================================================
        // host_lookup(hostname) — simple DNS lookup via `host` command
        // =================================================================
        t["host_lookup"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("host_lookup", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("host_lookup() expects a string hostname", line);
#ifdef _WIN32
            return XObject::makeString(captureNetCmd("nslookup " + args[0].asString() + " 2>nul"));
#else
            return XObject::makeString(captureNetCmd("host " + args[0].asString() + " 2>/dev/null"));
#endif
        };

        // =================================================================
        // whois(domain) — domain registration info
        // =================================================================
        t["whois"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("whois", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("whois() expects a string domain", line);
            return XObject::makeString(captureNetCmd("whois " + args[0].asString() + " 2>/dev/null"));
        };

        // =================================================================
        // traceroute(host) — trace network path
        // =================================================================
        t["traceroute"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("traceroute", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("traceroute() expects a string host", line);
#ifdef _WIN32
            return XObject::makeString(captureNetCmd("tracert " + args[0].asString() + " 2>&1"));
#else
            // Try traceroute, fallback to tracepath
            if (hasCmd("traceroute"))
                return XObject::makeString(captureNetCmd("traceroute -m 15 " + args[0].asString() + " 2>&1"));
            else if (hasCmd("tracepath"))
                return XObject::makeString(captureNetCmd("tracepath " + args[0].asString() + " 2>&1"));
            else
                throw RuntimeError("traceroute() requires traceroute or tracepath", line);
#endif
        };

        // =================================================================
        // netstat() — show active network connections
        // =================================================================
        t["netstat"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() > 0)
                throw ArityError("netstat", 0, (int)args.size(), line);
#ifdef _WIN32
            return XObject::makeString(captureNetCmd("netstat -an 2>&1"));
#else
            if (hasCmd("netstat"))
                return XObject::makeString(captureNetCmd("netstat -tuln 2>/dev/null"));
            else if (hasCmd("ss"))
                return XObject::makeString(captureNetCmd("ss -tuln 2>/dev/null"));
            else
                throw RuntimeError("netstat() requires netstat or ss", line);
#endif
        };

        // =================================================================
        // ss() — socket statistics
        // =================================================================
        t["ss"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() > 0)
                throw ArityError("ss", 0, (int)args.size(), line);
#ifdef _WIN32
            return XObject::makeString(captureNetCmd("netstat -an 2>&1"));
#else
            return XObject::makeString(captureNetCmd("ss -tuln 2>/dev/null"));
#endif
        };

        // =================================================================
        // ifconfig() — show network interfaces
        // =================================================================
        t["ifconfig"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() > 0)
                throw ArityError("ifconfig", 0, (int)args.size(), line);
#ifdef _WIN32
            return XObject::makeString(captureNetCmd("ipconfig 2>&1"));
#else
            if (hasCmd("ifconfig"))
                return XObject::makeString(captureNetCmd("ifconfig 2>/dev/null"));
            else
                return XObject::makeString(captureNetCmd("ip addr 2>/dev/null"));
#endif
        };

        // =================================================================
        // ip_cmd(args_str) — modern network interface management
        // =================================================================
        t["ip_cmd"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.empty() || args.size() > 1)
                throw ArityError("ip_cmd", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("ip_cmd() expects a string argument", line);
#ifdef _WIN32
            return XObject::makeString(captureNetCmd("ipconfig 2>&1"));
#else
            return XObject::makeString(captureNetCmd("ip " + args[0].asString() + " 2>/dev/null"));
#endif
        };

        // =================================================================
        // route() — show routing table
        // =================================================================
        t["route"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() > 0)
                throw ArityError("route", 0, (int)args.size(), line);
#ifdef _WIN32
            return XObject::makeString(captureNetCmd("route print 2>&1"));
#else
            if (hasCmd("route"))
                return XObject::makeString(captureNetCmd("route -n 2>/dev/null"));
            else
                return XObject::makeString(captureNetCmd("ip route 2>/dev/null"));
#endif
        };

        // =================================================================
        // iptables(args_str) — Linux firewall rules
        // =================================================================
        t["iptables"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.empty() || args.size() > 1)
                throw ArityError("iptables", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("iptables() expects a string argument", line);
#ifdef _WIN32
            throw RuntimeError("iptables() is not available on Windows", line);
#else
            return XObject::makeString(captureNetCmd("iptables " + args[0].asString() + " 2>&1"));
#endif
        };

        // =================================================================
        // ufw(args_str) — Uncomplicated firewall frontend
        // =================================================================
        t["ufw"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.empty() || args.size() > 1)
                throw ArityError("ufw", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("ufw() expects a string argument", line);
#ifdef _WIN32
            throw RuntimeError("ufw() is not available on Windows", line);
#else
            return XObject::makeString(captureNetCmd("ufw " + args[0].asString() + " 2>&1"));
#endif
        };

        // =================================================================
        // nc(host, port) — raw TCP connection, returns response
        // =================================================================
        t["nc"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("nc", 2, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("nc() host must be a string", line);
            if (!args[1].isInt())
                throw TypeError("nc() port must be an integer", line);

            std::string host = args[0].asString();
            int port = (int)args[1].asInt();

            // Use native sockets for a quick connect test
#ifdef _WIN32
            WSADATA wsaData;
            WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
            struct addrinfo hints = {}, *res = nullptr;
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;
            std::string portStr = std::to_string(port);

            int status = getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res);
            if (status != 0)
            {
#ifdef _WIN32
                WSACleanup();
#endif
                throw RuntimeError("nc() cannot resolve: " + host, line);
            }

            int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
            if (sock < 0)
            {
                freeaddrinfo(res);
#ifdef _WIN32
                WSACleanup();
#endif
                throw RuntimeError("nc() socket creation failed", line);
            }

            // Set timeout
            struct timeval tv;
            tv.tv_sec = 5;
            tv.tv_usec = 0;
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
            setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof(tv));

            bool connected = (connect(sock, res->ai_addr, res->ai_addrlen) == 0);
            freeaddrinfo(res);

#ifdef _WIN32
            closesocket(sock);
            WSACleanup();
#else
            close(sock);
#endif
            XMap result;
            result.set("connected", XObject::makeBool(connected));
            result.set("host", XObject::makeString(host));
            result.set("port", XObject::makeInt(port));
            return XObject::makeMap(std::move(result));
        };

        // =================================================================
        // telnet_connect(host, port) — alias for nc, raw TCP test
        // =================================================================
        t["telnet_connect"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("telnet_connect", 2, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("telnet_connect() host must be a string", line);
            if (!args[1].isInt())
                throw TypeError("telnet_connect() port must be an integer", line);

            std::string host = args[0].asString();
            int port = (int)args[1].asInt();

#ifdef _WIN32
            WSADATA wsaData;
            WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
            struct addrinfo hints = {}, *res = nullptr;
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;
            std::string portStr = std::to_string(port);

            int status = getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res);
            if (status != 0)
            {
#ifdef _WIN32
                WSACleanup();
#endif
                throw RuntimeError("telnet_connect() cannot resolve: " + host, line);
            }

            int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
            if (sock < 0)
            {
                freeaddrinfo(res);
#ifdef _WIN32
                WSACleanup();
#endif
                throw RuntimeError("telnet_connect() socket creation failed", line);
            }

            struct timeval tv;
            tv.tv_sec = 5;
            tv.tv_usec = 0;
            setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));

            bool connected = (connect(sock, res->ai_addr, res->ai_addrlen) == 0);
            freeaddrinfo(res);
#ifdef _WIN32
            closesocket(sock);
            WSACleanup();
#else
            close(sock);
#endif
            return XObject::makeBool(connected);
        };

        // =================================================================
        // rsync(src, dest) — sync files using rsync
        // =================================================================
        t["rsync"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("rsync", 2, (int)args.size(), line);
            if (!args[0].isString() || !args[1].isString())
                throw TypeError("rsync() expects (src: string, dest: string)", line);
#ifdef _WIN32
            throw RuntimeError("rsync() is not available on Windows", line);
#else
            if (!hasCmd("rsync"))
                throw RuntimeError("rsync() requires rsync to be installed", line);
            std::string cmd = "rsync -avz '" + args[0].asString() + "' '" + args[1].asString() + "' 2>&1";
            return XObject::makeString(captureNetCmd(cmd));
#endif
        };

        // =================================================================
        // local_ip() — get local machine IP address
        // =================================================================
        t["local_ip"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() > 0)
                throw ArityError("local_ip", 0, (int)args.size(), line);
#ifdef _WIN32
            WSADATA wsaData;
            WSAStartup(MAKEWORD(2, 2), &wsaData);
            char hostname[256];
            gethostname(hostname, sizeof(hostname));
            struct addrinfo hints = {}, *res = nullptr;
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            int status = getaddrinfo(hostname, nullptr, &hints, &res);
            std::string ip = "127.0.0.1";
            if (status == 0 && res)
            {
                char ipStr[INET_ADDRSTRLEN];
                struct sockaddr_in *addr = (struct sockaddr_in *)res->ai_addr;
                inet_ntop(AF_INET, &(addr->sin_addr), ipStr, sizeof(ipStr));
                ip = ipStr;
                freeaddrinfo(res);
            }
            WSACleanup();
            return XObject::makeString(ip);
#else
            // Use getifaddrs to find a non-loopback IPv4 address
            struct ifaddrs *ifas = nullptr;
            if (getifaddrs(&ifas) == -1)
                return XObject::makeString("127.0.0.1");

            std::string bestIp = "127.0.0.1";
            for (struct ifaddrs *ifa = ifas; ifa != nullptr; ifa = ifa->ifa_next)
            {
                if (!ifa->ifa_addr)
                    continue;
                if (ifa->ifa_addr->sa_family != AF_INET)
                    continue;

                char ipStr[INET_ADDRSTRLEN];
                struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
                inet_ntop(AF_INET, &(addr->sin_addr), ipStr, sizeof(ipStr));

                std::string ip(ipStr);
                if (ip != "127.0.0.1")
                {
                    bestIp = ip;
                    break;
                }
            }
            freeifaddrs(ifas);
            return XObject::makeString(bestIp);
#endif
        };

        // =================================================================
        // public_ip() — get public IP via external service
        // =================================================================
        t["public_ip"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() > 0)
                throw ArityError("public_ip", 0, (int)args.size(), line);

            // Try multiple services
            std::string ip = captureNetCmd("curl -s --max-time 5 ifconfig.me 2>/dev/null");
            if (ip.empty())
                ip = captureNetCmd("curl -s --max-time 5 api.ipify.org 2>/dev/null");
            if (ip.empty())
                ip = captureNetCmd("curl -s --max-time 5 icanhazip.com 2>/dev/null");
            if (ip.empty())
                throw RuntimeError("public_ip() could not determine public IP (requires internet + curl)", line);
            return XObject::makeString(ip);
        };
    }

} // namespace xell
