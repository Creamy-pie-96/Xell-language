#pragma once

// =============================================================================
// os.hpp — Unified OS abstraction interface for Xell
// =============================================================================
//
// This is the ONLY header the rest of the codebase includes for OS operations.
// It provides a clean, cross-platform API hiding all platform differences.
//
// Implementation is split across:
//   fs.cpp      — filesystem operations  (std::filesystem)
//   env.cpp     — environment variables  (#ifdef for set/unset)
//   process.cpp — subprocess execution   (fork+exec / CreateProcess)
//
// Every function here throws xell errors (IOError, FileNotFoundError, etc.)
// with line=0 since OS layer doesn't know about source lines — the builtin
// wrappers supply the real line number.
// =============================================================================

#include <string>
#include <vector>
#include <cstdint>

namespace xell
{
    namespace os
    {

        // =====================================================================
        // Filesystem
        // =====================================================================

        /// Create a directory (and all parents). No-op if already exists.
        void make_dir(const std::string &path);

        /// Remove a file or directory (recursive for dirs).
        void remove_path(const std::string &path);

        /// Copy a file or directory (recursive for dirs).
        void copy_path(const std::string &src, const std::string &dst);

        /// Move / rename a file or directory.
        void move_path(const std::string &src, const std::string &dst);

        /// Check if a path exists (file or directory).
        bool path_exists(const std::string &path);

        /// Check if a path is a regular file.
        bool is_file(const std::string &path);

        /// Check if a path is a directory.
        bool is_dir(const std::string &path);

        /// List immediate children of a directory (names only, not full paths).
        std::vector<std::string> list_dir(const std::string &path);

        /// List immediate children with full paths.
        std::vector<std::string> list_dir_full(const std::string &path);

        /// Read entire file contents into a string.
        std::string read_file(const std::string &path);

        /// Write string to file (overwrites if exists, creates if not).
        void write_file(const std::string &path, const std::string &content);

        /// Append string to file (creates if not).
        void append_file(const std::string &path, const std::string &content);

        /// Get file size in bytes. Throws if not a file.
        std::uint64_t file_size(const std::string &path);

        /// Get current working directory.
        std::string cwd();

        /// Change current working directory.
        void change_dir(const std::string &path);

        /// Resolve to absolute path.
        std::string absolute_path(const std::string &path);

        /// Get the filename component of a path.
        std::string file_name(const std::string &path);

        /// Get the parent directory of a path.
        std::string parent_path(const std::string &path);

        /// Get the file extension (including the dot).
        std::string extension(const std::string &path);

        // =====================================================================
        // Environment Variables
        // =====================================================================

        /// Get an environment variable. Returns empty string if not set.
        std::string env_get(const std::string &name);

        /// Set an environment variable (overwrites if exists).
        void env_set(const std::string &name, const std::string &value);

        /// Unset (remove) an environment variable.
        void env_unset(const std::string &name);

        /// Check if an environment variable is set.
        bool env_has(const std::string &name);

        // =====================================================================
        // Process Execution
        // =====================================================================

        /// Result of a subprocess execution.
        struct ProcessResult
        {
            int exitCode;
            std::string stdoutOutput;
            std::string stderrOutput;
        };

        /// Run a command, inheriting stdin/stdout/stderr. Returns exit code.
        int run(const std::string &command);

        /// Run a command and capture stdout + stderr. Returns ProcessResult.
        ProcessResult run_capture(const std::string &command);

        /// Get the current process ID.
        int get_pid();

    } // namespace os
} // namespace xell
