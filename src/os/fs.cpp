// =============================================================================
// fs.cpp — Filesystem operations for Xell
// =============================================================================
//
// All functions use C++17 std::filesystem which is inherently cross-platform.
// Errors are translated into Xell error types (IOError, FileNotFoundError).
//
// =============================================================================

#include "os.hpp"
#include "../lib/errors/error.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace xell
{
    namespace os
    {

        // ---- make_dir -------------------------------------------------------
        void make_dir(const std::string &path)
        {
            std::error_code ec;
            fs::create_directories(path, ec);
            if (ec)
                throw IOError("cannot create directory '" + path + "': " + ec.message(), 0);
        }

        // ---- remove_path ----------------------------------------------------
        void remove_path(const std::string &path)
        {
            if (!fs::exists(path))
                throw FileNotFoundError(path, 0);
            std::error_code ec;
            fs::remove_all(path, ec);
            if (ec)
                throw IOError("cannot remove '" + path + "': " + ec.message(), 0);
        }

        // ---- copy_path ------------------------------------------------------
        void copy_path(const std::string &src, const std::string &dst)
        {
            if (!fs::exists(src))
                throw FileNotFoundError(src, 0);
            std::error_code ec;
            fs::copy(src, dst,
                     fs::copy_options::recursive | fs::copy_options::overwrite_existing,
                     ec);
            if (ec)
                throw IOError("cannot copy '" + src + "' to '" + dst + "': " + ec.message(), 0);
        }

        // ---- move_path ------------------------------------------------------
        void move_path(const std::string &src, const std::string &dst)
        {
            if (!fs::exists(src))
                throw FileNotFoundError(src, 0);
            std::error_code ec;
            fs::rename(src, dst, ec);
            if (ec)
                throw IOError("cannot move '" + src + "' to '" + dst + "': " + ec.message(), 0);
        }

        // ---- path_exists ----------------------------------------------------
        bool path_exists(const std::string &path)
        {
            return fs::exists(path);
        }

        // ---- is_file --------------------------------------------------------
        bool is_file(const std::string &path)
        {
            return fs::is_regular_file(path);
        }

        // ---- is_dir ---------------------------------------------------------
        bool is_dir(const std::string &path)
        {
            return fs::is_directory(path);
        }

        // ---- list_dir (names only) ------------------------------------------
        std::vector<std::string> list_dir(const std::string &path)
        {
            if (!fs::exists(path))
                throw FileNotFoundError(path, 0);
            if (!fs::is_directory(path))
                throw IOError("'" + path + "' is not a directory", 0);

            std::vector<std::string> result;
            for (const auto &entry : fs::directory_iterator(path))
                result.push_back(entry.path().filename().string());
            return result;
        }

        // ---- list_dir_full (full paths) -------------------------------------
        std::vector<std::string> list_dir_full(const std::string &path)
        {
            if (!fs::exists(path))
                throw FileNotFoundError(path, 0);
            if (!fs::is_directory(path))
                throw IOError("'" + path + "' is not a directory", 0);

            std::vector<std::string> result;
            for (const auto &entry : fs::directory_iterator(path))
                result.push_back(entry.path().string());
            return result;
        }

        // ---- read_file ------------------------------------------------------
        std::string read_file(const std::string &path)
        {
            if (!fs::exists(path))
                throw FileNotFoundError(path, 0);
            if (!fs::is_regular_file(path))
                throw IOError("'" + path + "' is not a regular file", 0);

            std::ifstream in(path, std::ios::binary);
            if (!in)
                throw IOError("cannot open '" + path + "' for reading", 0);

            std::ostringstream ss;
            ss << in.rdbuf();
            return ss.str();
        }

        // ---- write_file -----------------------------------------------------
        void write_file(const std::string &path, const std::string &content)
        {
            // Ensure parent directory exists
            auto parent = fs::path(path).parent_path();
            if (!parent.empty() && !fs::exists(parent))
            {
                std::error_code ec;
                fs::create_directories(parent, ec);
                if (ec)
                    throw IOError("cannot create parent directory for '" + path + "': " + ec.message(), 0);
            }

            std::ofstream out(path, std::ios::binary | std::ios::trunc);
            if (!out)
                throw IOError("cannot open '" + path + "' for writing", 0);
            out << content;
        }

        // ---- append_file ----------------------------------------------------
        void append_file(const std::string &path, const std::string &content)
        {
            std::ofstream out(path, std::ios::binary | std::ios::app);
            if (!out)
                throw IOError("cannot open '" + path + "' for appending", 0);
            out << content;
        }

        // ---- file_size ------------------------------------------------------
        std::uint64_t file_size(const std::string &path)
        {
            if (!fs::exists(path))
                throw FileNotFoundError(path, 0);
            if (!fs::is_regular_file(path))
                throw IOError("'" + path + "' is not a regular file", 0);

            std::error_code ec;
            auto sz = fs::file_size(path, ec);
            if (ec)
                throw IOError("cannot get size of '" + path + "': " + ec.message(), 0);
            return sz;
        }

        // ---- cwd ------------------------------------------------------------
        std::string cwd()
        {
            std::error_code ec;
            auto p = fs::current_path(ec);
            if (ec)
                throw IOError("cannot get current directory: " + ec.message(), 0);
            return p.string();
        }

        // ---- change_dir -----------------------------------------------------
        void change_dir(const std::string &path)
        {
            if (!fs::exists(path))
                throw FileNotFoundError(path, 0);
            if (!fs::is_directory(path))
                throw IOError("'" + path + "' is not a directory", 0);

            std::error_code ec;
            fs::current_path(path, ec);
            if (ec)
                throw IOError("cannot change to directory '" + path + "': " + ec.message(), 0);
        }

        // ---- absolute_path --------------------------------------------------
        std::string absolute_path(const std::string &path)
        {
            std::error_code ec;
            auto p = fs::absolute(path, ec);
            if (ec)
                throw IOError("cannot resolve absolute path for '" + path + "': " + ec.message(), 0);
            return p.string();
        }

        // ---- file_name ------------------------------------------------------
        std::string file_name(const std::string &path)
        {
            return fs::path(path).filename().string();
        }

        // ---- parent_path ----------------------------------------------------
        std::string parent_path(const std::string &path)
        {
            return fs::path(path).parent_path().string();
        }

        // ---- extension ------------------------------------------------------
        std::string extension(const std::string &path)
        {
            return fs::path(path).extension().string();
        }

    } // namespace os
} // namespace xell
