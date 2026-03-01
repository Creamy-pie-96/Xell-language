#pragma once

// =============================================================================
// OS builtins — filesystem, environment, and process built-in functions
// =============================================================================
//
// Maps Xell function names to xell::os:: C++ implementations.
// These give Xell scripts the power of bash/powershell for task automation.
//
// Filesystem:  mkdir, rmdir, cp, mv, exists, is_file, is_dir,
//              ls, read, write, append, file_size,
//              cwd, cd, abspath, basename, dirname, ext
//
// Environment: env_get, env_set, env_unset, env_has
//
// Process:     run, run_capture, pid
//
// =============================================================================

#include "builtin_registry.hpp"
#include "../os/os.hpp"
#include "../interpreter/shell_state.hpp"

namespace xell
{

    inline void registerOSBuiltins(BuiltinTable &t, ShellState &shellState)
    {
        // =================================================================
        // Filesystem builtins
        // =================================================================

        // ---- mkdir(path) ---------------------------------------------------
        t["mkdir"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("mkdir", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("mkdir() expects a string path", line);
            try
            {
                os::make_dir(args[0].asString());
            }
            catch (const XellError &)
            {
                throw; // re-throw Xell errors as-is
            }
            return XObject::makeNone();
        };

        // ---- rmdir(path) / rm(path) ----------------------------------------
        t["rm"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("rm", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("rm() expects a string path", line);
            try
            {
                os::remove_path(args[0].asString());
            }
            catch (const FileNotFoundError &)
            {
                throw FileNotFoundError(args[0].asString(), line);
            }
            catch (const XellError &)
            {
                throw;
            }
            return XObject::makeNone();
        };

        // ---- cp(src, dst) --------------------------------------------------
        t["cp"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("cp", 2, (int)args.size(), line);
            if (!args[0].isString() || !args[1].isString())
                throw TypeError("cp() expects two string paths", line);
            try
            {
                os::copy_path(args[0].asString(), args[1].asString());
            }
            catch (const FileNotFoundError &)
            {
                throw FileNotFoundError(args[0].asString(), line);
            }
            catch (const XellError &)
            {
                throw;
            }
            return XObject::makeNone();
        };

        // ---- mv(src, dst) --------------------------------------------------
        t["mv"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("mv", 2, (int)args.size(), line);
            if (!args[0].isString() || !args[1].isString())
                throw TypeError("mv() expects two string paths", line);
            try
            {
                os::move_path(args[0].asString(), args[1].asString());
            }
            catch (const FileNotFoundError &)
            {
                throw FileNotFoundError(args[0].asString(), line);
            }
            catch (const XellError &)
            {
                throw;
            }
            return XObject::makeNone();
        };

        // ---- exists(path) --------------------------------------------------
        t["exists"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("exists", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("exists() expects a string path", line);
            return XObject::makeBool(os::path_exists(args[0].asString()));
        };

        // ---- is_file(path) -------------------------------------------------
        t["is_file"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("is_file", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("is_file() expects a string path", line);
            return XObject::makeBool(os::is_file(args[0].asString()));
        };

        // ---- is_dir(path) --------------------------------------------------
        t["is_dir"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("is_dir", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("is_dir() expects a string path", line);
            return XObject::makeBool(os::is_dir(args[0].asString()));
        };

        // ---- ls(path) — returns list of child names -------------------------
        t["ls"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("ls", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("ls() expects a string path", line);
            try
            {
                auto entries = os::list_dir(args[0].asString());
                XList result;
                result.reserve(entries.size());
                for (auto &e : entries)
                    result.push_back(XObject::makeString(std::move(e)));
                return XObject::makeList(std::move(result));
            }
            catch (const FileNotFoundError &)
            {
                throw FileNotFoundError(args[0].asString(), line);
            }
            catch (const XellError &)
            {
                throw;
            }
        };

        // ---- read(path) — returns file contents as string -------------------
        t["read"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("read", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("read() expects a string path", line);
            try
            {
                return XObject::makeString(os::read_file(args[0].asString()));
            }
            catch (const FileNotFoundError &)
            {
                throw FileNotFoundError(args[0].asString(), line);
            }
            catch (const XellError &)
            {
                throw;
            }
        };

        // ---- write(path, content) -------------------------------------------
        t["write"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("write", 2, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("write() expects a string path as first arg", line);
            if (!args[1].isString())
                throw TypeError("write() expects a string content as second arg", line);
            try
            {
                os::write_file(args[0].asString(), args[1].asString());
            }
            catch (const XellError &)
            {
                throw;
            }
            return XObject::makeNone();
        };

        // ---- append(path, content) ------------------------------------------
        t["append"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("append", 2, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("append() expects a string path as first arg", line);
            if (!args[1].isString())
                throw TypeError("append() expects a string content as second arg", line);
            try
            {
                os::append_file(args[0].asString(), args[1].asString());
            }
            catch (const XellError &)
            {
                throw;
            }
            return XObject::makeNone();
        };

        // ---- file_size(path) — returns number of bytes ----------------------
        t["file_size"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("file_size", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("file_size() expects a string path", line);
            try
            {
                return XObject::makeInt(static_cast<int64_t>(os::file_size(args[0].asString())));
            }
            catch (const FileNotFoundError &)
            {
                throw FileNotFoundError(args[0].asString(), line);
            }
            catch (const XellError &)
            {
                throw;
            }
        };

        // ---- cwd() — returns current working directory ----------------------
        t["cwd"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("cwd", 0, (int)args.size(), line);
            return XObject::makeString(os::cwd());
        };

        // ---- cd(path) — change working directory ----------------------------
        t["cd"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("cd", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("cd() expects a string path", line);
            try
            {
                os::change_dir(args[0].asString());
            }
            catch (const FileNotFoundError &)
            {
                throw FileNotFoundError(args[0].asString(), line);
            }
            catch (const XellError &)
            {
                throw;
            }
            return XObject::makeNone();
        };

        // ---- abspath(path) — resolve to absolute ----------------------------
        t["abspath"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("abspath", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("abspath() expects a string path", line);
            return XObject::makeString(os::absolute_path(args[0].asString()));
        };

        // ---- basename(path) — filename component ----------------------------
        t["basename"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("basename", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("basename() expects a string path", line);
            return XObject::makeString(os::file_name(args[0].asString()));
        };

        // ---- dirname(path) — parent directory component ---------------------
        t["dirname"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("dirname", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("dirname() expects a string path", line);
            return XObject::makeString(os::parent_path(args[0].asString()));
        };

        // ---- ext(path) — file extension -------------------------------------
        t["ext"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("ext", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("ext() expects a string path", line);
            return XObject::makeString(os::extension(args[0].asString()));
        };

        // =================================================================
        // Environment builtins
        // =================================================================

        // ---- env_get(name) --------------------------------------------------
        t["env_get"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("env_get", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("env_get() expects a string name", line);
            return XObject::makeString(os::env_get(args[0].asString()));
        };

        // ---- env_set(name, value) -------------------------------------------
        t["env_set"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("env_set", 2, (int)args.size(), line);
            if (!args[0].isString() || !args[1].isString())
                throw TypeError("env_set() expects two string arguments", line);
            os::env_set(args[0].asString(), args[1].asString());
            return XObject::makeNone();
        };

        // ---- env_unset(name) ------------------------------------------------
        t["env_unset"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("env_unset", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("env_unset() expects a string name", line);
            os::env_unset(args[0].asString());
            return XObject::makeNone();
        };

        // ---- env_has(name) --------------------------------------------------
        t["env_has"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("env_has", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("env_has() expects a string name", line);
            return XObject::makeBool(os::env_has(args[0].asString()));
        };

        // =================================================================
        // Process builtins
        // =================================================================

        // ---- run(command) — returns exit code --------------------------------
        t["run"] = [&shellState](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("run", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("run() expects a string command", line);
            try
            {
                std::string cmd = args[0].asString();
                int code = os::run(cmd);
                shellState.lastExitCode = code;
                if (shellState.exitOnError && code != 0)
                    throw CommandFailedError(cmd, code, line);
                return XObject::makeInt(static_cast<int64_t>(code));
            }
            catch (const CommandFailedError &)
            {
                throw; // re-throw set_e failures
            }
            catch (const ProcessError &e)
            {
                throw ProcessError(std::string(e.detail()), line);
            }
        };

        // ---- run_capture(command) — returns map {exit_code, stdout, stderr} --
        t["run_capture"] = [&shellState](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("run_capture", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("run_capture() expects a string command", line);
            try
            {
                std::string cmd = args[0].asString();
                auto result = os::run_capture(cmd);
                shellState.lastExitCode = result.exitCode;
                XMap m;
                m.set("exit_code", XObject::makeInt(static_cast<int64_t>(result.exitCode)));
                m.set("stdout", XObject::makeString(std::move(result.stdoutOutput)));
                m.set("stderr", XObject::makeString(std::move(result.stderrOutput)));
                if (shellState.exitOnError && result.exitCode != 0)
                    throw CommandFailedError(cmd, result.exitCode, line);
                return XObject::makeMap(std::move(m));
            }
            catch (const CommandFailedError &)
            {
                throw;
            }
            catch (const ProcessError &e)
            {
                throw ProcessError(std::string(e.detail()), line);
            }
        };

        // ---- pid() — returns current process ID -----------------------------
        t["pid"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("pid", 0, (int)args.size(), line);
            return XObject::makeInt(static_cast<int64_t>(os::get_pid()));
        };

        // =================================================================
        // Shell control builtins
        // =================================================================

        // ---- set_e() — enable exit-on-error mode (like bash set -e) --------
        t["set_e"] = [&shellState](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("set_e", 0, (int)args.size(), line);
            shellState.exitOnError = true;
            return XObject::makeNone();
        };

        // ---- unset_e() — disable exit-on-error mode -------------------------
        t["unset_e"] = [&shellState](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("unset_e", 0, (int)args.size(), line);
            shellState.exitOnError = false;
            return XObject::makeNone();
        };

        // ---- exit_code() — returns the last command's exit code -------------
        t["exit_code"] = [&shellState](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("exit_code", 0, (int)args.size(), line);
            return XObject::makeInt(static_cast<int64_t>(shellState.lastExitCode));
        };
    }

} // namespace xell
