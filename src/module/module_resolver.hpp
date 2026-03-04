#pragma once

// =============================================================================
// Module Resolver — Handles module search and path resolution for Xell
// =============================================================================
//
// Implements the search algorithm from the bring_module_plan:
//   1. Session cache hit? → return cached module
//   2. Build search path: from_dirs + cwd + XELL_PATH + stdlib
//   3. Walk search path: .xell_meta fast path, or slow scan
//   4. Cache check: .xelc bytecode vs source hash
//   5. Execute module file once
//   6. Store in session cache
//
// TODO(bytecode): Steps 4-5 currently always re-parse from source.
// When bytecode/VM is added, implement .xelc loading for fast path.
// =============================================================================

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <functional>
#include "../hash/hash_algorithm.hpp"

namespace xell
{
    // Forward declarations
    struct XModule;
    class XObject;

    // ========================================================================
    // ModuleResolver — finds and caches user-defined modules
    // ========================================================================

    class ModuleResolver
    {
    public:
        ModuleResolver() = default;

        /// Set the current source file (for relative path resolution)
        void setSourceFile(const std::string &path) { sourceFile_ = path; }

        /// Get the current source file
        const std::string &sourceFile() const { return sourceFile_; }

        /// Check if a module is already cached in this session
        bool isSessionCached(const std::string &moduleName) const
        {
            return sessionCache_.count(moduleName) > 0;
        }

        /// Get a cached module object
        std::shared_ptr<XModule> getSessionCached(const std::string &moduleName) const
        {
            auto it = sessionCache_.find(moduleName);
            if (it != sessionCache_.end())
                return it->second;
            return nullptr;
        }

        /// Store a module in the session cache
        void cacheModule(const std::string &moduleName, std::shared_ptr<XModule> mod)
        {
            sessionCache_[moduleName] = std::move(mod);
        }

        /// Build the search path for module resolution
        /// Order: fromDir (if given) → directory of current file → XELL_PATH dirs → stdlib
        std::vector<std::string> buildSearchPath(const std::string &fromDir = "") const;

        /// Resolve a file path relative to the current source file
        static std::string resolveRelative(const std::string &basePath, const std::string &relative);

        /// Get canonical (absolute) path
        static std::string canonicalize(const std::string &path);

        /// Compute SHA-256 hash of a file's contents
        static std::string hashFile(const std::string &filePath);

        /// Check if a path is already being imported (circular detection)
        bool isImporting(const std::string &path) const
        {
            return importingFiles_.count(path) > 0;
        }

        /// Mark a file as currently being imported
        void markImporting(const std::string &path)
        {
            importingFiles_.insert(path);
        }

        /// Unmark a file as being imported
        void unmarkImporting(const std::string &path)
        {
            importingFiles_.erase(path);
        }

        /// Share importing guard from another resolver (for child interpreters)
        void shareImportGuard(const std::unordered_set<std::string> &guard)
        {
            importingFiles_ = guard;
        }

        /// Get the importing files set
        const std::unordered_set<std::string> &importingFiles() const
        {
            return importingFiles_;
        }

        /// Clear session cache (for reset)
        void clear()
        {
            sessionCache_.clear();
            importingFiles_.clear();
        }

        // ====================================================================
        // File-based module resolution (Phase 5)
        // ====================================================================

        /// Callback type for executing a module file and returning the module objects
        /// defined within. The interpreter passes a callback that parses + executes
        /// a .xell file and returns a map of module_name → XModule.
        using ModuleFileExecutor = std::function<
            std::unordered_map<std::string, std::shared_ptr<XModule>>(const std::string &filePath)>;

        /// Set the executor callback (set by interpreter during init)
        void setFileExecutor(ModuleFileExecutor fn) { fileExecutor_ = std::move(fn); }

        /// Try to resolve a module by searching the filesystem.
        /// 1. Check .xell_meta in each search path dir (fast path)
        /// 2. Slow-path scan .xell files if no .xell_meta
        /// 3. Parse + execute the file, cache the module, return it.
        /// Returns nullptr if not found.
        std::shared_ptr<XModule> resolveFromFileSystem(
            const std::string &moduleName,
            const std::string &fromDir = "",
            std::vector<std::string> *warnings = nullptr);

        /// Read .xell_meta from a directory, return the mapping module→file.
        /// Returns empty map if .xell_meta not found or unparseable.
        static std::unordered_map<std::string, std::string>
        readMetaIndex(const std::string &dirPath);

        /// Slow-path scan: find all .xell files in a directory, parse them to
        /// discover module names. Returns module_name→file_path.
        static std::unordered_map<std::string, std::string>
        slowPathScan(const std::string &dirPath);

    private:
        std::string sourceFile_;
        std::unordered_map<std::string, std::shared_ptr<XModule>> sessionCache_;
        std::unordered_set<std::string> importingFiles_;
        ModuleFileExecutor fileExecutor_;
    };

} // namespace xell
