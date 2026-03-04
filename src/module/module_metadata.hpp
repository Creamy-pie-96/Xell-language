#pragma once

// =============================================================================
// Module Metadata — .xell_meta index file reader/writer
// =============================================================================
//
// Manages the .xell_meta JSON index files that map module names to source files.
// Provides O(1) module lookup by name within a directory.
//
// Format:
//   {
//     "xell_meta_version": 1,
//     "generated": "2026-03-03T10:00:00",
//     "modules": {
//       "lib": { "file": "math.xell", "hash": "...", "exports": [...] }
//     }
//   }
//
// TODO(bytecode): When bytecode/VM is added, .xell_meta will also track
// .xelc paths and bytecode format versions for cache validation.
// =============================================================================

#include <string>
#include <vector>
#include <unordered_map>

namespace xell
{

    // ========================================================================
    // ModuleMetaEntry — one module's metadata in .xell_meta
    // ========================================================================

    struct ModuleMetaEntry
    {
        std::string moduleName;
        std::string fileName;             // relative filename within the directory
        std::string sourceHash;           // SHA-256 of source file
        std::vector<std::string> exports; // exported names

        // Submodule metadata (nested)
        struct SubmoduleMeta
        {
            std::string name;
            std::vector<std::string> exports;
        };
        std::vector<SubmoduleMeta> submodules;
    };

    // ========================================================================
    // ModuleMetadata — the full .xell_meta index for one directory
    // ========================================================================

    struct ModuleMetadata
    {
        int version = 1;
        std::string generated; // ISO timestamp
        std::string directory; // directory path

        // module name → entry
        std::unordered_map<std::string, ModuleMetaEntry> modules;

        /// Look up which file contains a module. Returns empty string if not found.
        std::string findFile(const std::string &moduleName) const
        {
            auto it = modules.find(moduleName);
            if (it != modules.end())
                return it->second.fileName;
            return "";
        }

        /// Check if a module exists in this metadata
        bool hasModule(const std::string &moduleName) const
        {
            return modules.count(moduleName) > 0;
        }
    };

    // ========================================================================
    // Metadata I/O functions
    // ========================================================================

    // TODO(bytecode): Implement full JSON read/write for .xell_meta
    // For now, the module system works without .xell_meta (slow path scan)
    // These will be implemented when the full cache system is built.

    /// Try to read .xell_meta from a directory. Returns empty metadata if not found.
    // ModuleMetadata readModuleMetadata(const std::string &dirPath);

    /// Write .xell_meta to a directory. Creates or updates entries.
    // void writeModuleMetadata(const std::string &dirPath, const ModuleMetadata &meta);

} // namespace xell
