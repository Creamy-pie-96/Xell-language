#pragma once

// =============================================================================
// module_registry.hpp — built-in module metadata for the bring system
// =============================================================================
//
// The ModuleRegistry knows which built-in function names belong to which
// "module" and whether that module is Tier 1 (always available) or Tier 2
// (requires `bring * from "module"`).
//
// Tier 1 — core scripting primitives, always in scope:
//     io, math, type, collection, util, os, hash, string, list, map,
//     bytes, generator, shell, (+ interpreter-level: map/filter/reduce/any/all)
//
// Tier 2 — specialized domains, need `bring`:
//     datetime, regex, fs, textproc, process, sysmon, net, archive, json
//
// Usage from the Interpreter:
//     ModuleRegistry registry;
//     if (registry.isBuiltinModule("json")) { ... }
//
// =============================================================================

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>

namespace xell
{

    class ModuleRegistry
    {
    public:
        /// Register a module with its function names.
        /// @param name       Module name (e.g. "json", "net")
        /// @param functions  List of function names this module provides
        /// @param tier2      true if the module requires `bring` to use
        void registerModule(const std::string &name,
                            std::vector<std::string> functions,
                            bool tier2)
        {
            modules_[name] = std::move(functions);
            if (tier2)
                tier2Modules_.insert(name);
        }

        /// Is `name` a recognised built-in module?
        bool isBuiltinModule(const std::string &name) const
        {
            return modules_.count(name) > 0;
        }

        /// Is `name` a Tier 2 (bring-required) module?
        bool isTier2(const std::string &name) const
        {
            return tier2Modules_.count(name) > 0;
        }

        /// Get all function names belonging to a module (empty if unknown).
        const std::vector<std::string> &moduleFunctions(const std::string &name) const
        {
            static const std::vector<std::string> empty;
            auto it = modules_.find(name);
            return it != modules_.end() ? it->second : empty;
        }

        /// Check whether a specific function exists inside a module.
        bool moduleHasFunction(const std::string &moduleName,
                               const std::string &funcName) const
        {
            auto it = modules_.find(moduleName);
            if (it == modules_.end())
                return false;
            return std::find(it->second.begin(), it->second.end(), funcName)
                   != it->second.end();
        }

        /// Get the set of ALL Tier 2 function names (union of all T2 modules).
        std::unordered_set<std::string> allTier2Functions() const
        {
            std::unordered_set<std::string> result;
            for (const auto &modName : tier2Modules_)
            {
                auto it = modules_.find(modName);
                if (it != modules_.end())
                    for (const auto &fn : it->second)
                        result.insert(fn);
            }
            return result;
        }

        /// Get all module names.
        std::vector<std::string> allModuleNames() const
        {
            std::vector<std::string> names;
            names.reserve(modules_.size());
            for (const auto &[k, v] : modules_)
                names.push_back(k);
            std::sort(names.begin(), names.end());
            return names;
        }

        /// Get only Tier 2 module names.
        std::vector<std::string> tier2ModuleNames() const
        {
            std::vector<std::string> names(tier2Modules_.begin(), tier2Modules_.end());
            std::sort(names.begin(), names.end());
            return names;
        }

        /// Find which module a function belongs to (empty string if not found).
        std::string findModuleForFunction(const std::string &funcName) const
        {
            for (const auto &[modName, funcs] : modules_)
            {
                if (std::find(funcs.begin(), funcs.end(), funcName) != funcs.end())
                    return modName;
            }
            return "";
        }

    private:
        std::unordered_map<std::string, std::vector<std::string>> modules_;
        std::unordered_set<std::string> tier2Modules_;
    };

} // namespace xell
