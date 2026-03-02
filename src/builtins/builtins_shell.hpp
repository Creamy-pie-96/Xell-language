#pragma once

// =============================================================================
// Shell Utility builtins — shell-like convenience functions
// =============================================================================
//
// These complement the existing OS/IO builtins with shell-style utilities:
//
// Output:      error(msg)        — print to stderr
// Terminal:    clear(), reset()
// Session:     logout()
// Aliases:     alias(name, cmd), unalias(name)
// Environment: export_env(name, val), env_list(), printenv(name?),
//              set_env(name, val)
// Lookup:      which(cmd), whereis(cmd), type_cmd(cmd)
// Help:        man(topic)
// History:     history()
// Misc:        yes_cmd(text?), true_val(), false_val()
//
// Already exist (NOT duplicated here):
//   print (IO), exit (IO), type/typeof (Type),
//   env_get/env_set/env_unset/env_has (OS), run/run_capture (OS), pid (OS)
//   source/bring (interpreter)
//
// =============================================================================

#include "builtin_registry.hpp"
#include "../lib/errors/error.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdlib>
#include <cstdio>
#include <sstream>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
extern char **environ;
#endif

namespace xell
{

    // Helper: capture shell command output (shared pattern)
    static inline std::string captureShellCmd(const std::string &cmd)
    {
        std::string result;
        FILE *fp = popen(cmd.c_str(), "r");
        if (!fp)
            return "";
        char buf[4096];
        while (fgets(buf, sizeof(buf), fp))
            result += buf;
        pclose(fp);
        // Trim trailing newline(s)
        while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
            result.pop_back();
        return result;
    }

    inline void registerShellBuiltins(BuiltinTable &t, std::vector<std::string> &output)
    {
        // =================================================================
        //  Alias table — shared mutable state for alias/unalias
        // =================================================================
        // Using a shared_ptr so lambdas all reference the same table.
        auto aliasTable = std::make_shared<std::unordered_map<std::string, std::string>>();

        // =================================================================
        //  History buffer — records every expression evaluated
        // =================================================================
        auto historyBuffer = std::make_shared<std::vector<std::string>>();

        // ---- error(msg...) — print to stderr --------------------------------
        t["error"] = [](std::vector<XObject> &args, int /*line*/) -> XObject
        {
            std::string msg;
            for (size_t i = 0; i < args.size(); ++i)
            {
                if (i > 0)
                    msg += " ";
                msg += args[i].toString();
            }
            std::cerr << msg << std::endl;
            return XObject::makeInt(0);
        };

        // ---- clear() — clear terminal screen --------------------------------
        t["clear"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("clear", 0, (int)args.size(), line);
#ifdef _WIN32
            std::system("cls");
#else
            // ANSI escape: clear screen + move cursor to top-left
            std::cout << "\033[2J\033[H" << std::flush;
#endif
            return XObject::makeNone();
        };

        // ---- reset() — reset terminal state ---------------------------------
        t["reset"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("reset", 0, (int)args.size(), line);
#ifdef _WIN32
            std::system("cls");
#else
            // Full terminal reset
            std::cout << "\033c" << std::flush;
#endif
            return XObject::makeNone();
        };

        // ---- logout() — exit with code 0 ------------------------------------
        t["logout"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("logout", 0, (int)args.size(), line);
            std::exit(0);
            return XObject::makeNone(); // never reached
        };

        // ---- alias(name, cmd) — define a shell alias ------------------------
        t["alias"] = [aliasTable](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() == 0)
            {
                // List all aliases → return as map
                XMap map;
                for (auto &[name, cmd] : *aliasTable)
                    map.set(name, XObject::makeString(cmd));
                return XObject::makeMap(std::move(map));
            }
            if (args.size() == 2)
            {
                if (!args[0].isString())
                    throw TypeError("alias() name must be a string", line);
                if (!args[1].isString())
                    throw TypeError("alias() command must be a string", line);
                (*aliasTable)[args[0].asString()] = args[1].asString();
                return XObject::makeNone();
            }
            throw ArityError("alias", 2, (int)args.size(), line);
        };

        // ---- unalias(name) — remove an alias --------------------------------
        t["unalias"] = [aliasTable](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("unalias", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("unalias() expects a string name", line);
            auto it = aliasTable->find(args[0].asString());
            if (it == aliasTable->end())
                throw RuntimeError("unalias: no such alias: " + args[0].asString(), line);
            aliasTable->erase(it);
            return XObject::makeNone();
        };

        // ---- export_env(name, val) — set env var (exported to children) -----
        t["export_env"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("export_env", 2, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("export_env() name must be a string", line);
            if (!args[1].isString())
                throw TypeError("export_env() value must be a string", line);
#ifdef _WIN32
            _putenv_s(args[0].asString().c_str(), args[1].asString().c_str());
#else
            setenv(args[0].asString().c_str(), args[1].asString().c_str(), 1);
#endif
            return XObject::makeNone();
        };

        // ---- env_list() — list all environment variables → map --------------
        t["env_list"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("env_list", 0, (int)args.size(), line);
            XMap map;
#ifdef _WIN32
            char *env = GetEnvironmentStringsA();
            if (env)
            {
                for (char *p = env; *p; p += strlen(p) + 1)
                {
                    std::string entry(p);
                    size_t eq = entry.find('=');
                    if (eq != std::string::npos && eq > 0)
                    {
                        map.set(entry.substr(0, eq),
                                XObject::makeString(entry.substr(eq + 1)));
                    }
                }
                FreeEnvironmentStringsA(env);
            }
#else
            for (char **e = ::environ; *e; ++e)
            {
                std::string entry(*e);
                size_t eq = entry.find('=');
                if (eq != std::string::npos)
                {
                    map.set(entry.substr(0, eq),
                            XObject::makeString(entry.substr(eq + 1)));
                }
            }
#endif
            return XObject::makeMap(std::move(map));
        };

        // ---- printenv(name?) — get specific env var or all ------------------
        t["printenv"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() > 1)
                throw ArityError("printenv", 1, (int)args.size(), line);
            if (args.size() == 1)
            {
                if (!args[0].isString())
                    throw TypeError("printenv() expects a string name", line);
                const char *val = std::getenv(args[0].asString().c_str());
                if (!val)
                    return XObject::makeNone();
                return XObject::makeString(val);
            }
            // No args → return all as map
            XMap map;
#ifdef _WIN32
            char *env = GetEnvironmentStringsA();
            if (env)
            {
                for (char *p = env; *p; p += strlen(p) + 1)
                {
                    std::string entry(p);
                    size_t eq = entry.find('=');
                    if (eq != std::string::npos && eq > 0)
                        map.set(entry.substr(0, eq),
                                XObject::makeString(entry.substr(eq + 1)));
                }
                FreeEnvironmentStringsA(env);
            }
#else
            for (char **e = ::environ; *e; ++e)
            {
                std::string entry(*e);
                size_t eq = entry.find('=');
                if (eq != std::string::npos)
                    map.set(entry.substr(0, eq),
                            XObject::makeString(entry.substr(eq + 1)));
            }
#endif
            return XObject::makeMap(std::move(map));
        };

        // ---- set_env(name, val) — alias for export_env ----------------------
        t["set_env"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("set_env", 2, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("set_env() name must be a string", line);
            if (!args[1].isString())
                throw TypeError("set_env() value must be a string", line);
#ifdef _WIN32
            _putenv_s(args[0].asString().c_str(), args[1].asString().c_str());
#else
            setenv(args[0].asString().c_str(), args[1].asString().c_str(), 1);
#endif
            return XObject::makeNone();
        };

        // ---- which(cmd) — find executable path ------------------------------
        t["which"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("which", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("which() expects a string command name", line);
#ifdef _WIN32
            std::string result = captureShellCmd("where " + args[0].asString() + " 2>nul");
#else
            std::string result = captureShellCmd("which " + args[0].asString() + " 2>/dev/null");
#endif
            if (result.empty())
                return XObject::makeNone();
            return XObject::makeString(result);
        };

        // ---- whereis(cmd) — find binary, source, man page -------------------
        t["whereis"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("whereis", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("whereis() expects a string command name", line);
#ifdef _WIN32
            std::string result = captureShellCmd("where " + args[0].asString() + " 2>nul");
#else
            std::string result = captureShellCmd("whereis " + args[0].asString() + " 2>/dev/null");
#endif
            if (result.empty())
                return XObject::makeNone();
            return XObject::makeString(result);
        };

        // ---- type_cmd(cmd) — describe command type (alias, builtin, etc.) ---
        t["type_cmd"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("type_cmd", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("type_cmd() expects a string command name", line);
#ifdef _WIN32
            // Windows: use where
            std::string result = captureShellCmd("where " + args[0].asString() + " 2>nul");
            if (result.empty())
                return XObject::makeString(args[0].asString() + ": not found");
            return XObject::makeString(args[0].asString() + " is " + result);
#else
            std::string result = captureShellCmd("type " + args[0].asString() + " 2>/dev/null");
            if (result.empty())
                return XObject::makeString(args[0].asString() + ": not found");
            return XObject::makeString(result);
#endif
        };

        // ---- man(topic) — show manual page (returns text) -------------------
        t["man"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("man", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("man() expects a string topic", line);
#ifdef _WIN32
            // Windows: use help
            std::string result = captureShellCmd("help " + args[0].asString() + " 2>nul");
#else
            // Use man with col to strip formatting codes
            std::string result = captureShellCmd("man " + args[0].asString() +
                                                 " 2>/dev/null | col -bx 2>/dev/null");
#endif
            if (result.empty())
                return XObject::makeString("No manual entry for " + args[0].asString());
            return XObject::makeString(result);
        };

        // ---- history() — return list of recorded commands -------------------
        t["history"] = [historyBuffer](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("history", 0, (int)args.size(), line);
            std::vector<XObject> result;
            for (size_t i = 0; i < historyBuffer->size(); ++i)
            {
                XMap entry;
                entry.set("index", XObject::makeInt((int64_t)(i + 1)));
                entry.set("command", XObject::makeString((*historyBuffer)[i]));
                result.push_back(XObject::makeMap(std::move(entry)));
            }
            return XObject::makeList(std::move(result));
        };

        // ---- history_add(cmd) — add entry to history (internal helper) ------
        t["history_add"] = [historyBuffer](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("history_add", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("history_add() expects a string", line);
            historyBuffer->push_back(args[0].asString());
            return XObject::makeNone();
        };

        // ---- yes_cmd(text?) — return repeated text (not infinite; N lines) --
        t["yes_cmd"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() > 2)
                throw ArityError("yes_cmd", 1, (int)args.size(), line);
            std::string text = "y";
            int count = 100; // sensible default — not infinite
            if (args.size() >= 1)
            {
                if (!args[0].isString())
                    throw TypeError("yes_cmd() text must be a string", line);
                text = args[0].asString();
            }
            if (args.size() == 2)
            {
                if (!args[1].isInt())
                    throw TypeError("yes_cmd() count must be an integer", line);
                count = (int)args[1].asInt();
                if (count < 0)
                    count = 0;
            }
            std::string result;
            for (int i = 0; i < count; ++i)
            {
                if (i > 0)
                    result += "\n";
                result += text;
            }
            return XObject::makeString(result);
        };

        // ---- true_val() — return true ---------------------------------------
        t["true_val"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("true_val", 0, (int)args.size(), line);
            return XObject::makeBool(true);
        };

        // ---- false_val() — return false -------------------------------------
        t["false_val"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("false_val", 0, (int)args.size(), line);
            return XObject::makeBool(false);
        };

        // ---- source_file(path) — read and return file contents ---------------
        // Different from interpreter's `source`/`bring` which execute code.
        // This just reads the file as text (like bash's `source` for config files).
        t["source_file"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("source_file", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("source_file() expects a file path string", line);
            std::ifstream file(args[0].asString());
            if (!file.is_open())
                throw FileNotFoundError(args[0].asString(), line);
            std::ostringstream ss;
            ss << file.rdbuf();
            return XObject::makeString(ss.str());
        };
    }

} // namespace xell
