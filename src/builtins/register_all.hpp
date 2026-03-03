#pragma once

// =============================================================================
// register_all.hpp — wire every builtin category into the interpreter
// =============================================================================
//
// Two registration modes:
//
// 1. registerAllBuiltins(t, output, shellState)
//       Legacy — registers everything into one flat table.
//       Used by child interpreters created during `bring "file.xel"`.
//
// 2. registerBuiltinsWithModules(active, all, registry, output, shellState)
//       Module-aware — registers ALL builtins into `all`, but only copies
//       Tier 1 (always-available) builtins into `active`.  Also populates
//       the ModuleRegistry so `bring * from "json"` etc. can be resolved.
//
// Tier 1 (no bring needed): io, math, type, collection, util, os, hash,
//     string, list, map, bytes, generator, shell
//
// Tier 2 (require bring): datetime, regex, fs, textproc, process,
//     sysmon, net, archive, json
//
// To add a new category:
//   1. #include "builtins_<category>.hpp"
//   2. Add a regModule(...) call below with the correct tier.
//
// =============================================================================

#include "builtin_registry.hpp"
#include "module_registry.hpp"
#include "builtins_io.hpp"
#include "builtins_math.hpp"
#include "builtins_type.hpp"
#include "builtins_collection.hpp"
#include "builtins_util.hpp"
#include "builtins_os.hpp"
#include "builtins_hash.hpp"
#include "builtins_string.hpp"
#include "builtins_list.hpp"
#include "builtins_map.hpp"
#include "builtins_bytes.hpp"
#include "builtins_generator.hpp"
#include "builtins_datetime.hpp"
#include "builtins_regex.hpp"
#include "builtins_fs.hpp"
#include "builtins_textproc.hpp"
#include "builtins_process.hpp"
#include "builtins_sysmon.hpp"
#include "builtins_network.hpp"
#include "builtins_archive.hpp"
#include "builtins_json.hpp"
#include "builtins_shell.hpp"
#include "builtins_casting.hpp"
#include "../interpreter/shell_state.hpp"

namespace xell
{

    // ---- Legacy: flat registration (used by child interpreters in bring) ----

    /// Registers every built-in function into a single table.
    inline void registerAllBuiltins(BuiltinTable &t, std::vector<std::string> &output,
                                    ShellState &shellState)
    {
        registerIOBuiltins(t, output);
        registerMathBuiltins(t);
        registerTypeBuiltins(t);
        registerCollectionBuiltins(t);
        registerUtilBuiltins(t);
        registerOSBuiltins(t, shellState);
        registerHashBuiltins(t);
        registerStringBuiltins(t);
        registerListBuiltins(t);
        registerMapBuiltins(t);
        registerBytesBuiltins(t);
        registerGeneratorBuiltins(t);
        registerDateTimeBuiltins(t);
        registerRegexBuiltins(t);
        registerFSBuiltins(t);
        registerTextProcBuiltins(t);
        registerProcessBuiltins(t);
        registerSysMonBuiltins(t);
        registerNetworkBuiltins(t);
        registerArchiveBuiltins(t);
        registerJSONBuiltins(t);
        registerShellBuiltins(t, output);
        registerCastingBuiltins(t);
    }

    // ---- Module-aware registration (used by top-level interpreter) ----------

    /// Register all builtins into `allBuiltins`, copy only Tier 1 into
    /// `activeBuiltins`, and populate the ModuleRegistry.
    inline void registerBuiltinsWithModules(
        BuiltinTable &activeBuiltins,
        BuiltinTable &allBuiltins,
        ModuleRegistry &registry,
        std::vector<std::string> &output,
        ShellState &shellState)
    {
        // Helper: register one module's functions, capture their names,
        // and optionally promote to the active table (Tier 1).
        auto regModule = [&](const std::string &moduleName,
                             bool tier2,
                             auto registerFn)
        {
            // Snapshot current keys
            std::unordered_set<std::string> before;
            before.reserve(allBuiltins.size());
            for (const auto &[k, _] : allBuiltins)
                before.insert(k);

            // Run the register function into allBuiltins
            registerFn(allBuiltins);

            // Capture newly added names
            std::vector<std::string> names;
            for (const auto &[k, fn] : allBuiltins)
            {
                if (!before.count(k))
                {
                    names.push_back(k);
                    if (!tier2)
                        activeBuiltins[k] = fn;
                }
            }

            registry.registerModule(moduleName, std::move(names), tier2);
        };

        // ── Tier 1: always available (no bring needed) ──────────────────

        regModule("io", false, [&](BuiltinTable &t)
                  { registerIOBuiltins(t, output); });
        regModule("math", false, [&](BuiltinTable &t)
                  { registerMathBuiltins(t); });
        regModule("type", false, [&](BuiltinTable &t)
                  { registerTypeBuiltins(t); });
        regModule("collection", false, [&](BuiltinTable &t)
                  { registerCollectionBuiltins(t); });
        regModule("util", false, [&](BuiltinTable &t)
                  { registerUtilBuiltins(t); });
        regModule("os", false, [&](BuiltinTable &t)
                  { registerOSBuiltins(t, shellState); });
        regModule("hash", false, [&](BuiltinTable &t)
                  { registerHashBuiltins(t); });
        regModule("string", false, [&](BuiltinTable &t)
                  { registerStringBuiltins(t); });
        regModule("list", false, [&](BuiltinTable &t)
                  { registerListBuiltins(t); });
        regModule("map", false, [&](BuiltinTable &t)
                  { registerMapBuiltins(t); });
        regModule("bytes", false, [&](BuiltinTable &t)
                  { registerBytesBuiltins(t); });
        regModule("generator", false, [&](BuiltinTable &t)
                  { registerGeneratorBuiltins(t); });
        regModule("shell", false, [&](BuiltinTable &t)
                  { registerShellBuiltins(t, output); });
        regModule("casting", false, [&](BuiltinTable &t)
                  { registerCastingBuiltins(t); });

        // ── Tier 2: require `bring * from "module"` ────────────────────

        regModule("datetime", true, [&](BuiltinTable &t)
                  { registerDateTimeBuiltins(t); });
        regModule("regex", true, [&](BuiltinTable &t)
                  { registerRegexBuiltins(t); });
        regModule("fs", true, [&](BuiltinTable &t)
                  { registerFSBuiltins(t); });
        regModule("textproc", true, [&](BuiltinTable &t)
                  { registerTextProcBuiltins(t); });
        regModule("process", true, [&](BuiltinTable &t)
                  { registerProcessBuiltins(t); });
        regModule("sysmon", true, [&](BuiltinTable &t)
                  { registerSysMonBuiltins(t); });
        regModule("net", true, [&](BuiltinTable &t)
                  { registerNetworkBuiltins(t); });
        regModule("archive", true, [&](BuiltinTable &t)
                  { registerArchiveBuiltins(t); });
        regModule("json", true, [&](BuiltinTable &t)
                  { registerJSONBuiltins(t); });
    }

} // namespace xell
