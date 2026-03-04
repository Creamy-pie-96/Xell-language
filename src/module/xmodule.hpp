#pragma once

// =============================================================================
// XModule — Xell module object type
// =============================================================================
//
// Represents a first-class module object in Xell's runtime. Modules are
// the result of `module name : ... ;` declarations. They hold:
//   - Exported names (functions, classes, variables, submodules)
//   - Dunder metadata (__name__, __path__, __exports__, etc.)
//   - Reference to their execution environment
//
// Design:
//   - A module is a first-class Xell object (XType::MODULE)
//   - Accessing members via -> dispatches to the export registry
//   - Submodules are just nested XModule objects in the export registry
//   - Private (non-exported) names are invisible outside
//
// TODO(bytecode): When bytecode/VM is added, modules will cache their
// compiled bytecode in __xelcache__/ and load from .xelc files.
// =============================================================================

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include "../interpreter/xobject.hpp"

namespace xell
{
    // Forward declaration — Environment defined in interpreter/environment.hpp
    class Environment;

    // ========================================================================
    // XModule — the module payload type (stored in XObject via XType::MODULE)
    // ========================================================================

    struct XModule
    {
        // ---- Identity ----
        std::string name;       // module name (e.g., "lib" or "lib->math_lib")
        std::string parentName; // parent module name; empty for top-level
        std::string filePath;   // absolute path to source file
        std::string cachedPath; // path to .xelc bytecode cache (future)

        // ---- Exports ----
        // All exported names → their values. Only these are visible via ->
        std::unordered_map<std::string, XObject> exports;

        // ---- Submodules ----
        // Exported submodule names → XModule objects
        std::unordered_map<std::string, std::shared_ptr<XModule>> submodules;

        // ---- Metadata (dunder variables) ----
        std::string version;       // __version__ (user-set, empty if not declared)
        bool isMainModule = false; // true when file is run directly (xell file.xell)

        // ---- Ownership ----
        // The module owns a child environment where its body was executed.
        // This keeps closures and AST pointers alive.
        std::shared_ptr<Environment> ownedEnv;

        // ---- Helpers ----

        /// Get the full qualified name (e.g., "lib->math_lib")
        std::string qualifiedName() const
        {
            if (parentName.empty())
                return name;
            return parentName + "->" + name;
        }

        /// Get the list of exported names
        std::vector<std::string> exportNames() const
        {
            std::vector<std::string> names;
            names.reserve(exports.size());
            for (const auto &kv : exports)
                names.push_back(kv.first);
            return names;
        }

        /// Get the list of exported submodule names
        std::vector<std::string> submoduleNames() const
        {
            std::vector<std::string> names;
            names.reserve(submodules.size());
            for (const auto &kv : submodules)
                names.push_back(kv.first);
            return names;
        }

        /// Look up an export by name. Returns nullptr if not found.
        const XObject *getExport(const std::string &exportName) const
        {
            auto it = exports.find(exportName);
            if (it != exports.end())
                return &it->second;
            return nullptr;
        }

        /// Check if a name is exported
        bool hasExport(const std::string &exportName) const
        {
            return exports.count(exportName) > 0;
        }

        /// Check if a submodule exists
        bool hasSubmodule(const std::string &subName) const
        {
            return submodules.count(subName) > 0;
        }

        XModule() = default;
        explicit XModule(std::string name) : name(std::move(name)) {}
    };

} // namespace xell
