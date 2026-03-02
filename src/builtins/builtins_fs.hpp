#pragma once

// =============================================================================
// File System builtins (extended) — cross-platform using std::filesystem
// =============================================================================
// Supplements builtins_os.hpp with additional FS operations.
// Existing functions (ls, cd, mkdir, rm, cp, mv, read, write, append,
// exists, is_file, is_dir, file_size, cwd, abspath, basename, dirname, ext)
// remain in builtins_os.hpp. This file adds everything else.
// =============================================================================

#include "builtin_registry.hpp"
#include "../lib/errors/error.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <regex>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <functional>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#endif

namespace fs = std::filesystem;

namespace xell
{

    inline void registerFSBuiltins(BuiltinTable &t)
    {
        // =================================================================
        // Core file operations (not already in builtins_os.hpp)
        // =================================================================

        // ls_all(path) — list including hidden files
        t["ls_all"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("ls_all", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("ls_all() expects a string path", line);
            fs::path p(args[0].asString());
            if (!fs::exists(p))
                throw FileNotFoundError(args[0].asString(), line);
            if (!fs::is_directory(p))
                throw IOError("ls_all(): not a directory: " + args[0].asString(), line);
            std::vector<XObject> result;
            for (auto &entry : fs::directory_iterator(p))
                result.push_back(XObject::makeString(entry.path().filename().string()));
            return XObject::makeList(std::move(result));
        };

        // pwd() — alias for cwd() (print working directory)
        t["pwd"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("pwd", 0, (int)args.size(), line);
            return XObject::makeString(fs::current_path().string());
        };

        // touch(path) — create empty file or update timestamp
        t["touch"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("touch", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("touch() expects a string path", line);
            fs::path p(args[0].asString());
            if (fs::exists(p))
            {
                // update modification time
                fs::last_write_time(p, fs::file_time_type::clock::now());
            }
            else
            {
                // create empty file
                std::ofstream ofs(p);
                if (!ofs)
                    throw IOError("touch() cannot create file: " + args[0].asString(), line);
            }
            return XObject::makeNone();
        };

        // cat(path) — return file contents as string (like read, but named for shell users)
        t["cat"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("cat", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("cat() expects a string path", line);
            fs::path p(args[0].asString());
            if (!fs::exists(p))
                throw FileNotFoundError(args[0].asString(), line);
            std::ifstream ifs(p);
            if (!ifs)
                throw IOError("cat() cannot read: " + args[0].asString(), line);
            std::ostringstream ss;
            ss << ifs.rdbuf();
            return XObject::makeString(ss.str());
        };

        // read_lines(path) — return list of lines
        t["read_lines"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("read_lines", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("read_lines() expects a string path", line);
            fs::path p(args[0].asString());
            if (!fs::exists(p))
                throw FileNotFoundError(args[0].asString(), line);
            std::ifstream ifs(p);
            if (!ifs)
                throw IOError("read_lines() cannot read: " + args[0].asString(), line);
            std::vector<XObject> lines;
            std::string ln;
            while (std::getline(ifs, ln))
                lines.push_back(XObject::makeString(ln));
            return XObject::makeList(std::move(lines));
        };

        // write_lines(path, lines_list) — write list of strings to file
        t["write_lines"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("write_lines", 2, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("write_lines() expects a string path as first arg", line);
            if (!args[1].isList())
                throw TypeError("write_lines() expects a list as second arg", line);
            std::ofstream ofs(args[0].asString());
            if (!ofs)
                throw IOError("write_lines() cannot write: " + args[0].asString(), line);
            const auto &lst = args[1].asList();
            for (size_t i = 0; i < lst.size(); i++)
            {
                ofs << lst[i].toString();
                if (i + 1 < lst.size())
                    ofs << "\n";
            }
            return XObject::makeNone();
        };

        // =================================================================
        // Symlinks & Links
        // =================================================================

        // is_symlink(path)
        t["is_symlink"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("is_symlink", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("is_symlink() expects a string path", line);
            return XObject::makeBool(fs::is_symlink(args[0].asString()));
        };

        // symlink(target, link_path)
        t["symlink"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("symlink", 2, (int)args.size(), line);
            if (!args[0].isString() || !args[1].isString())
                throw TypeError("symlink() expects two string paths", line);
            try
            {
                fs::path target(args[0].asString());
                fs::path link(args[1].asString());
                if (fs::is_directory(target))
                    fs::create_directory_symlink(target, link);
                else
                    fs::create_symlink(target, link);
            }
            catch (const fs::filesystem_error &e)
            {
                throw IOError(std::string("symlink() failed: ") + e.what(), line);
            }
            return XObject::makeNone();
        };

        // hardlink(target, link_path)
        t["hardlink"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("hardlink", 2, (int)args.size(), line);
            if (!args[0].isString() || !args[1].isString())
                throw TypeError("hardlink() expects two string paths", line);
            try
            {
                fs::create_hard_link(args[0].asString(), args[1].asString());
            }
            catch (const fs::filesystem_error &e)
            {
                throw IOError(std::string("hardlink() failed: ") + e.what(), line);
            }
            return XObject::makeNone();
        };

        // ln(target, link, soft=true) — create link, default soft
        t["ln"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() < 2 || args.size() > 3)
                throw ArityError("ln", 2, (int)args.size(), line);
            if (!args[0].isString() || !args[1].isString())
                throw TypeError("ln() expects string paths", line);
            bool soft = true;
            if (args.size() == 3)
            {
                if (!args[2].isBool())
                    throw TypeError("ln() third argument must be bool (soft)", line);
                soft = args[2].asBool();
            }
            try
            {
                if (soft)
                {
                    fs::path target(args[0].asString());
                    if (fs::is_directory(target))
                        fs::create_directory_symlink(target, args[1].asString());
                    else
                        fs::create_symlink(target, args[1].asString());
                }
                else
                {
                    fs::create_hard_link(args[0].asString(), args[1].asString());
                }
            }
            catch (const fs::filesystem_error &e)
            {
                throw IOError(std::string("ln() failed: ") + e.what(), line);
            }
            return XObject::makeNone();
        };

        // readlink(path) — resolve symlink to real path
        t["readlink"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("readlink", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("readlink() expects a string path", line);
            try
            {
                return XObject::makeString(fs::read_symlink(args[0].asString()).string());
            }
            catch (const fs::filesystem_error &e)
            {
                throw IOError(std::string("readlink() failed: ") + e.what(), line);
            }
        };

        // =================================================================
        // Permissions & Metadata
        // =================================================================

        // chmod(path, mode) — mode as integer (e.g. 755)
        t["chmod"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("chmod", 2, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("chmod() expects a string path", line);
            if (!args[1].isNumber())
                throw TypeError("chmod() expects a numeric mode", line);

            int mode = (int)args[1].asNumber();
#ifdef _WIN32
            // Windows: limited support, just set read-only or not
            try
            {
                auto perms = (mode & 0200) ? fs::perms::all : (fs::perms::owner_read | fs::perms::group_read | fs::perms::others_read);
                fs::permissions(args[0].asString(), perms, fs::perm_options::replace);
            }
            catch (const fs::filesystem_error &e)
            {
                throw IOError(std::string("chmod() failed: ") + e.what(), line);
            }
#else
            // Convert octal-like integer: 755 → 0755
            int octal = 0;
            int mult = 1;
            int tmp = mode;
            while (tmp > 0)
            {
                octal += (tmp % 10) * mult;
                mult *= 8;
                tmp /= 10;
            }
            if (::chmod(args[0].asString().c_str(), (mode_t)octal) != 0)
                throw IOError("chmod() failed on: " + args[0].asString(), line);
#endif
            return XObject::makeNone();
        };

        // chown(path, user, group) — Unix only, no-op on Windows
        t["chown"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 3)
                throw ArityError("chown", 3, (int)args.size(), line);
            if (!args[0].isString() || !args[1].isString() || !args[2].isString())
                throw TypeError("chown() expects three strings (path, user, group)", line);
#ifdef _WIN32
            // No-op on Windows
            return XObject::makeNone();
#else
            struct passwd *pw = getpwnam(args[1].asString().c_str());
            if (!pw)
                throw IOError("chown() unknown user: " + args[1].asString(), line);
            struct group *gr = getgrnam(args[2].asString().c_str());
            if (!gr)
                throw IOError("chown() unknown group: " + args[2].asString(), line);
            if (::chown(args[0].asString().c_str(), pw->pw_uid, gr->gr_gid) != 0)
                throw IOError("chown() failed on: " + args[0].asString(), line);
            return XObject::makeNone();
#endif
        };

        // chgrp(path, group) — Unix only, no-op on Windows
        t["chgrp"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("chgrp", 2, (int)args.size(), line);
            if (!args[0].isString() || !args[1].isString())
                throw TypeError("chgrp() expects two strings (path, group)", line);
#ifdef _WIN32
            return XObject::makeNone();
#else
            struct group *gr = getgrnam(args[1].asString().c_str());
            if (!gr)
                throw IOError("chgrp() unknown group: " + args[1].asString(), line);
            if (::chown(args[0].asString().c_str(), (uid_t)-1, gr->gr_gid) != 0)
                throw IOError("chgrp() failed on: " + args[0].asString(), line);
            return XObject::makeNone();
#endif
        };

        // stat(path) — full metadata as map
        t["stat"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("stat", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("stat() expects a string path", line);
            fs::path p(args[0].asString());
            if (!fs::exists(p))
                throw FileNotFoundError(args[0].asString(), line);

            auto status = fs::status(p);
            XMap result;
            result.set("path", XObject::makeString(fs::absolute(p).string()));
            result.set("is_file", XObject::makeBool(fs::is_regular_file(p)));
            result.set("is_dir", XObject::makeBool(fs::is_directory(p)));
            result.set("is_symlink", XObject::makeBool(fs::is_symlink(p)));

            if (fs::is_regular_file(p))
                result.set("size", XObject::makeInt(static_cast<int64_t>(fs::file_size(p))));
            else
                result.set("size", XObject::makeInt(0));

            // Last write time → Unix timestamp (convert file_clock to system_clock)
            auto ftime = fs::last_write_time(p);
            // C++17 workaround: compute offset between file_clock and system_clock
            auto sysNow = std::chrono::system_clock::now();
            auto fileNow = fs::file_time_type::clock::now();
            auto systemTime = sysNow + (ftime - fileNow);
            auto epoch = systemTime.time_since_epoch();
            auto secs = std::chrono::duration_cast<std::chrono::seconds>(epoch).count();
            result.set("modified_time", XObject::makeInt(static_cast<int64_t>(secs)));

            // Permissions as integer
            auto perms = status.permissions();
            int permInt = static_cast<int>(perms) & 0777;
            result.set("permissions", XObject::makeInt(permInt));

            // Type string
            std::string typeStr = "unknown";
            if (fs::is_regular_file(p))
                typeStr = "file";
            else if (fs::is_directory(p))
                typeStr = "directory";
            else if (fs::is_symlink(p))
                typeStr = "symlink";
            else if (fs::is_block_file(p))
                typeStr = "block";
            else if (fs::is_character_file(p))
                typeStr = "character";
            else if (fs::is_fifo(p))
                typeStr = "fifo";
            else if (fs::is_socket(p))
                typeStr = "socket";
            result.set("type", XObject::makeString(typeStr));

            return XObject::makeMap(std::move(result));
        };

        // modified_time(path) — last modification timestamp
        t["modified_time"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("modified_time", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("modified_time() expects a string path", line);
            fs::path p(args[0].asString());
            if (!fs::exists(p))
                throw FileNotFoundError(args[0].asString(), line);
            auto ftime = fs::last_write_time(p);
            auto sysNow = std::chrono::system_clock::now();
            auto fileNow = fs::file_time_type::clock::now();
            auto systemTime = sysNow + (ftime - fileNow);
            auto epoch = systemTime.time_since_epoch();
            auto secs = std::chrono::duration_cast<std::chrono::seconds>(epoch).count();
            return XObject::makeInt(static_cast<int64_t>(secs));
        };

        // created_time(path) — creation time (uses last_write_time as fallback
        // since C++17 doesn't have creation time in std::filesystem)
        t["created_time"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("created_time", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("created_time() expects a string path", line);
            fs::path p(args[0].asString());
            if (!fs::exists(p))
                throw FileNotFoundError(args[0].asString(), line);
#ifdef _WIN32
            // Windows: use GetFileTime for creation time
            WIN32_FILE_ATTRIBUTE_DATA data;
            if (GetFileAttributesExA(args[0].asString().c_str(), GetFileExInfoStandard, &data))
            {
                ULARGE_INTEGER ull;
                ull.LowPart = data.ftCreationTime.dwLowDateTime;
                ull.HighPart = data.ftCreationTime.dwHighDateTime;
                // Convert Windows FILETIME (100ns since 1601) to Unix epoch
                int64_t unixTs = (int64_t)(ull.QuadPart / 10000000ULL - 11644473600ULL);
                return XObject::makeInt(unixTs);
            }
#else
            // Linux: use stat for birth time (st_ctim as fallback)
            struct ::stat st;
            if (::stat(args[0].asString().c_str(), &st) == 0)
            {
                return XObject::makeInt(static_cast<int64_t>(st.st_ctime));
            }
#endif
            // Fallback to last_write_time
            auto ftime = fs::last_write_time(p);
            auto sysNow = std::chrono::system_clock::now();
            auto fileNow = fs::file_time_type::clock::now();
            auto systemTime = sysNow + (ftime - fileNow);
            auto epoch = systemTime.time_since_epoch();
            auto secs = std::chrono::duration_cast<std::chrono::seconds>(epoch).count();
            return XObject::makeInt(static_cast<int64_t>(secs));
        };

        // =================================================================
        // Search & Tree
        // =================================================================

        // find(dir, pattern) — recursively find files matching glob-like pattern
        t["find"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("find", 2, (int)args.size(), line);
            if (!args[0].isString() || !args[1].isString())
                throw TypeError("find() expects two strings (dir, pattern)", line);
            fs::path dir(args[0].asString());
            if (!fs::exists(dir))
                throw FileNotFoundError(args[0].asString(), line);

            std::string pattern = args[1].asString();
            // Convert glob to regex: * → .*, ? → ., . → \.
            std::string regexStr;
            for (char c : pattern)
            {
                if (c == '*')
                    regexStr += ".*";
                else if (c == '?')
                    regexStr += ".";
                else if (c == '.')
                    regexStr += "\\.";
                else
                    regexStr += c;
            }

            std::regex re(regexStr);
            std::vector<XObject> results;
            for (auto &entry : fs::recursive_directory_iterator(dir))
            {
                std::string name = entry.path().filename().string();
                if (std::regex_match(name, re))
                    results.push_back(XObject::makeString(entry.path().string()));
            }
            return XObject::makeList(std::move(results));
        };

        // find_regex(dir, pattern) — find files matching regex pattern
        t["find_regex"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("find_regex", 2, (int)args.size(), line);
            if (!args[0].isString() || !args[1].isString())
                throw TypeError("find_regex() expects two strings (dir, pattern)", line);
            fs::path dir(args[0].asString());
            if (!fs::exists(dir))
                throw FileNotFoundError(args[0].asString(), line);

            try
            {
                std::regex re(args[1].asString());
                std::vector<XObject> results;
                for (auto &entry : fs::recursive_directory_iterator(dir))
                {
                    std::string name = entry.path().filename().string();
                    if (std::regex_search(name, re))
                        results.push_back(XObject::makeString(entry.path().string()));
                }
                return XObject::makeList(std::move(results));
            }
            catch (const std::regex_error &e)
            {
                throw RuntimeError(std::string("find_regex() invalid pattern: ") + e.what(), line);
            }
        };

        // locate(name) — find file by exact name recursively from cwd
        t["locate"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("locate", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("locate() expects a string name", line);
            std::string target = args[0].asString();
            std::vector<XObject> results;
            try
            {
                for (auto &entry : fs::recursive_directory_iterator(fs::current_path()))
                {
                    if (entry.path().filename().string() == target)
                        results.push_back(XObject::makeString(entry.path().string()));
                }
            }
            catch (...)
            {
                // Permission errors during traversal — ignore
            }
            return XObject::makeList(std::move(results));
        };

        // glob(pattern) — expand glob relative to cwd
        t["glob"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("glob", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("glob() expects a string pattern", line);

            std::string pattern = args[0].asString();
            // Split into dir and file pattern
            fs::path pat(pattern);
            fs::path dir = pat.parent_path();
            std::string filePattern = pat.filename().string();

            if (dir.empty())
                dir = ".";
            if (!fs::exists(dir))
                return XObject::makeList({});

            // Convert glob to regex
            std::string regexStr;
            for (char c : filePattern)
            {
                if (c == '*')
                    regexStr += ".*";
                else if (c == '?')
                    regexStr += ".";
                else if (c == '.')
                    regexStr += "\\.";
                else
                    regexStr += c;
            }

            std::regex re(regexStr);
            std::vector<XObject> results;
            for (auto &entry : fs::directory_iterator(dir))
            {
                std::string name = entry.path().filename().string();
                if (std::regex_match(name, re))
                    results.push_back(XObject::makeString(entry.path().string()));
            }
            std::sort(results.begin(), results.end(),
                      [](const XObject &a, const XObject &b)
                      { return a.asString() < b.asString(); });
            return XObject::makeList(std::move(results));
        };

        // file_diff(file1, file2) — simple line-by-line diff
        t["file_diff"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("file_diff", 2, (int)args.size(), line);
            if (!args[0].isString() || !args[1].isString())
                throw TypeError("file_diff() expects two string paths", line);

            auto readLines = [&](const std::string &path) -> std::vector<std::string>
            {
                std::ifstream ifs(path);
                if (!ifs)
                    throw FileNotFoundError(path, line);
                std::vector<std::string> lines;
                std::string l;
                while (std::getline(ifs, l))
                    lines.push_back(l);
                return lines;
            };

            auto lines1 = readLines(args[0].asString());
            auto lines2 = readLines(args[1].asString());

            std::vector<XObject> diffs;
            size_t maxLines = std::max(lines1.size(), lines2.size());
            for (size_t i = 0; i < maxLines; i++)
            {
                std::string l1 = i < lines1.size() ? lines1[i] : "";
                std::string l2 = i < lines2.size() ? lines2[i] : "";
                if (l1 != l2)
                {
                    std::string entry;
                    if (i >= lines1.size())
                        entry = "+" + std::to_string(i + 1) + ": " + l2;
                    else if (i >= lines2.size())
                        entry = "-" + std::to_string(i + 1) + ": " + l1;
                    else
                        entry = "~" + std::to_string(i + 1) + ": -" + l1 + " +" + l2;
                    diffs.push_back(XObject::makeString(entry));
                }
            }
            return XObject::makeList(std::move(diffs));
        };

        // tree(path) — directory tree as string
        t["tree"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() < 1 || args.size() > 2)
                throw ArityError("tree", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("tree() expects a string path", line);

            int maxDepth = -1; // unlimited
            if (args.size() == 2)
            {
                if (!args[1].isNumber())
                    throw TypeError("tree() optional second arg is max depth", line);
                maxDepth = (int)args[1].asNumber();
            }

            fs::path root(args[0].asString());
            if (!fs::exists(root))
                throw FileNotFoundError(args[0].asString(), line);

            std::ostringstream out;
            std::function<void(const fs::path &, const std::string &, int)> walk;
            walk = [&](const fs::path &p, const std::string &prefix, int depth)
            {
                if (maxDepth >= 0 && depth > maxDepth)
                    return;

                std::vector<fs::directory_entry> entries;
                try
                {
                    for (auto &e : fs::directory_iterator(p))
                        entries.push_back(e);
                }
                catch (...)
                {
                    return;
                }
                std::sort(entries.begin(), entries.end(),
                          [](const fs::directory_entry &a, const fs::directory_entry &b)
                          { return a.path().filename() < b.path().filename(); });

                for (size_t i = 0; i < entries.size(); i++)
                {
                    bool last = (i == entries.size() - 1);
                    std::string connector = last ? "└── " : "├── ";
                    out << prefix << connector << entries[i].path().filename().string() << "\n";
                    if (fs::is_directory(entries[i].path()))
                    {
                        std::string childPrefix = prefix + (last ? "    " : "│   ");
                        walk(entries[i].path(), childPrefix, depth + 1);
                    }
                }
            };

            out << root.filename().string() << "\n";
            walk(root, "", 0);
            return XObject::makeString(out.str());
        };

        // =================================================================
        // Path operations
        // =================================================================

        // extension(path) — alias for ext(), get file extension
        t["extension"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("extension", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("extension() expects a string path", line);
            return XObject::makeString(fs::path(args[0].asString()).extension().string());
        };

        // stem(path) — filename without extension
        t["stem"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("stem", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("stem() expects a string path", line);
            return XObject::makeString(fs::path(args[0].asString()).stem().string());
        };

        // realpath(path) — resolve to canonical absolute path
        t["realpath"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("realpath", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("realpath() expects a string path", line);
            try
            {
                return XObject::makeString(fs::canonical(args[0].asString()).string());
            }
            catch (const fs::filesystem_error &e)
            {
                throw IOError(std::string("realpath() failed: ") + e.what(), line);
            }
        };

        // join_path(...parts) — join path parts
        t["join_path"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.empty())
                throw ArityError("join_path", 1, 0, line);
            fs::path result;
            for (auto &arg : args)
            {
                if (!arg.isString())
                    throw TypeError("join_path() expects string arguments", line);
                if (result.empty())
                    result = arg.asString();
                else
                    result /= arg.asString();
            }
            return XObject::makeString(result.string());
        };

        // normalize(path) — resolve . and ..
        t["normalize"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("normalize", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("normalize() expects a string path", line);
            return XObject::makeString(
                fs::path(args[0].asString()).lexically_normal().string());
        };

        // is_absolute(path)
        t["is_absolute"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("is_absolute", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("is_absolute() expects a string path", line);
            return XObject::makeBool(fs::path(args[0].asString()).is_absolute());
        };

        // relative_path(path, base) — get path relative to base
        t["relative_path"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("relative_path", 2, (int)args.size(), line);
            if (!args[0].isString() || !args[1].isString())
                throw TypeError("relative_path() expects two string paths", line);
            return XObject::makeString(
                fs::relative(args[0].asString(), args[1].asString()).string());
        };

        // home_dir() — user home directory
        t["home_dir"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("home_dir", 0, (int)args.size(), line);
#ifdef _WIN32
            const char *home = std::getenv("USERPROFILE");
            if (!home)
                home = std::getenv("HOMEDRIVE");
#else
            const char *home = std::getenv("HOME");
#endif
            return XObject::makeString(home ? std::string(home) : "");
        };

        // temp_dir() — OS temp directory
        t["temp_dir"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (!args.empty())
                throw ArityError("temp_dir", 0, (int)args.size(), line);
            return XObject::makeString(fs::temp_directory_path().string());
        };

        // =================================================================
        // Disk operations
        // =================================================================

        // disk_usage(path) — total size of directory in bytes
        t["disk_usage"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("disk_usage", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("disk_usage() expects a string path", line);
            fs::path p(args[0].asString());
            if (!fs::exists(p))
                throw FileNotFoundError(args[0].asString(), line);

            uint64_t total = 0;
            if (fs::is_regular_file(p))
            {
                total = fs::file_size(p);
            }
            else if (fs::is_directory(p))
            {
                for (auto &entry : fs::recursive_directory_iterator(p))
                {
                    if (fs::is_regular_file(entry.path()))
                        total += fs::file_size(entry.path());
                }
            }
            return XObject::makeInt(static_cast<int64_t>(total));
        };

        // disk_free(path) — free disk space on the volume
        t["disk_free"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("disk_free", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("disk_free() expects a string path", line);
            try
            {
                auto si = fs::space(args[0].asString());
                return XObject::makeInt(static_cast<int64_t>(si.available));
            }
            catch (const fs::filesystem_error &e)
            {
                throw IOError(std::string("disk_free() failed: ") + e.what(), line);
            }
        };

        // xxd(path) — hex dump of file as string
        t["xxd"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("xxd", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("xxd() expects a string path", line);
            std::ifstream ifs(args[0].asString(), std::ios::binary);
            if (!ifs)
                throw FileNotFoundError(args[0].asString(), line);

            std::ostringstream out;
            char buf[16];
            size_t offset = 0;
            while (ifs.read(buf, 16) || ifs.gcount() > 0)
            {
                size_t count = (size_t)ifs.gcount();
                out << std::setfill('0') << std::setw(8) << std::hex << offset << ": ";
                for (size_t i = 0; i < 16; i++)
                {
                    if (i < count)
                        out << std::setfill('0') << std::setw(2) << std::hex
                            << (int)(unsigned char)buf[i] << " ";
                    else
                        out << "   ";
                    if (i == 7)
                        out << " ";
                }
                out << " ";
                for (size_t i = 0; i < count; i++)
                {
                    char c = buf[i];
                    out << (c >= 32 && c < 127 ? c : '.');
                }
                out << "\n";
                offset += count;
                if (count < 16)
                    break;
            }
            return XObject::makeString(out.str());
        };

        // strings(path) — extract printable ASCII strings from binary file
        t["strings"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() < 1 || args.size() > 2)
                throw ArityError("strings", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("strings() expects a string path", line);

            int minLen = 4; // default minimum string length
            if (args.size() == 2)
            {
                if (!args[1].isNumber())
                    throw TypeError("strings() optional second arg is min length", line);
                minLen = (int)args[1].asNumber();
            }

            std::ifstream ifs(args[0].asString(), std::ios::binary);
            if (!ifs)
                throw FileNotFoundError(args[0].asString(), line);

            std::vector<XObject> results;
            std::string current;
            char c;
            while (ifs.get(c))
            {
                if (c >= 32 && c < 127)
                {
                    current += c;
                }
                else
                {
                    if ((int)current.size() >= minLen)
                        results.push_back(XObject::makeString(current));
                    current.clear();
                }
            }
            if ((int)current.size() >= minLen)
                results.push_back(XObject::makeString(current));

            return XObject::makeList(std::move(results));
        };
    }

} // namespace xell
