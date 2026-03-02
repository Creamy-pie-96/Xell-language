#pragma once

// =============================================================================
// Archive & Compression builtins — zip, tar, gzip, bzip2, xz
// =============================================================================
// zip, unzip_archive, tar_create, tar_extract,
// gzip_compress, gunzip_decompress,
// bzip2_compress, bunzip2_decompress,
// xz_compress, xz_decompress
//
// All operations wrap system commands for maximum compatibility.
// Each function validates inputs, calls the appropriate tool,
// and throws on failure.
// =============================================================================

#include "builtin_registry.hpp"
#include "../lib/errors/error.hpp"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <sstream>
#include <filesystem>

namespace xell
{

    // Helper: run archive command and check exit code
    static inline int runArchiveCmd(const std::string &cmd)
    {
        int ret = system(cmd.c_str());
#ifdef _WIN32
        return ret;
#else
        return WEXITSTATUS(ret);
#endif
    }

    // Helper: check if a tool is available
    static inline bool hasArchiveTool(const std::string &name)
    {
#ifdef _WIN32
        std::string check = "where " + name + " >nul 2>&1";
#else
        std::string check = "command -v " + name + " >/dev/null 2>&1";
#endif
        return system(check.c_str()) == 0;
    }

    static inline void registerArchiveBuiltins(BuiltinTable &t)
    {
        // =================================================================
        // zip_archive(src, dest) — create zip archive
        // src can be a file or directory, dest is the .zip output path
        // =================================================================
        t["zip_archive"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("zip_archive", 2, (int)args.size(), line);
            if (!args[0].isString() || !args[1].isString())
                throw TypeError("zip_archive() expects (src: string, dest: string)", line);

            std::string src = args[0].asString();
            std::string dest = args[1].asString();

            if (!std::filesystem::exists(src))
                throw FileNotFoundError(src, line);

#ifdef _WIN32
            // Use PowerShell Compress-Archive
            std::string cmd = "powershell -Command \"Compress-Archive -Path '" +
                              src + "' -DestinationPath '" + dest + "' -Force\" 2>&1";
#else
            std::string cmd;
            if (std::filesystem::is_directory(src))
                cmd = "cd '" + src + "' && zip -r '" +
                      std::filesystem::absolute(dest).string() + "' . 2>&1";
            else
                cmd = "zip -j '" + dest + "' '" + src + "' 2>&1";
#endif
            int ret = runArchiveCmd(cmd);
            if (ret != 0)
                throw RuntimeError("zip_archive() failed (exit code " + std::to_string(ret) + ")", line);
            return XObject::makeBool(true);
        };

        // =================================================================
        // unzip_archive(src, dest=".") — extract zip archive
        // =================================================================
        t["unzip_archive"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.empty() || args.size() > 2)
                throw ArityError("unzip_archive", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("unzip_archive() src must be a string", line);

            std::string src = args[0].asString();
            std::string dest = ".";
            if (args.size() == 2)
            {
                if (!args[1].isString())
                    throw TypeError("unzip_archive() dest must be a string", line);
                dest = args[1].asString();
            }

            if (!std::filesystem::exists(src))
                throw FileNotFoundError(src, line);

            // Create dest if it doesn't exist
            std::filesystem::create_directories(dest);

#ifdef _WIN32
            std::string cmd = "powershell -Command \"Expand-Archive -Path '" +
                              src + "' -DestinationPath '" + dest + "' -Force\" 2>&1";
#else
            std::string cmd = "unzip -o '" + src + "' -d '" + dest + "' 2>&1";
#endif
            int ret = runArchiveCmd(cmd);
            if (ret != 0)
                throw RuntimeError("unzip_archive() failed (exit code " + std::to_string(ret) + ")", line);
            return XObject::makeBool(true);
        };

        // =================================================================
        // tar_create(src, dest) — create tar archive
        // Detects compression from extension: .tar.gz/.tgz → gzip,
        // .tar.bz2 → bzip2, .tar.xz → xz, .tar → none
        // =================================================================
        t["tar_create"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("tar_create", 2, (int)args.size(), line);
            if (!args[0].isString() || !args[1].isString())
                throw TypeError("tar_create() expects (src: string, dest: string)", line);

            std::string src = args[0].asString();
            std::string dest = args[1].asString();

            if (!std::filesystem::exists(src))
                throw FileNotFoundError(src, line);

            // Determine compression flag from extension
            std::string flag = "";
            if (dest.size() >= 7 && dest.substr(dest.size() - 7) == ".tar.gz")
                flag = "z";
            else if (dest.size() >= 4 && dest.substr(dest.size() - 4) == ".tgz")
                flag = "z";
            else if (dest.size() >= 8 && dest.substr(dest.size() - 8) == ".tar.bz2")
                flag = "j";
            else if (dest.size() >= 7 && dest.substr(dest.size() - 7) == ".tar.xz")
                flag = "J";
            // else plain .tar

#ifdef _WIN32
            std::string cmd = "tar -c" + flag + "f \"" + dest + "\" \"" + src + "\" 2>&1";
#else
            std::string cmd = "tar -c" + flag + "f '" + dest + "' '" + src + "' 2>&1";
#endif
            int ret = runArchiveCmd(cmd);
            if (ret != 0)
                throw RuntimeError("tar_create() failed (exit code " + std::to_string(ret) + ")", line);
            return XObject::makeBool(true);
        };

        // =================================================================
        // tar_extract(src, dest=".") — extract tar archive
        // Auto-detects compression from extension
        // =================================================================
        t["tar_extract"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.empty() || args.size() > 2)
                throw ArityError("tar_extract", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("tar_extract() src must be a string", line);

            std::string src = args[0].asString();
            std::string dest = ".";
            if (args.size() == 2)
            {
                if (!args[1].isString())
                    throw TypeError("tar_extract() dest must be a string", line);
                dest = args[1].asString();
            }

            if (!std::filesystem::exists(src))
                throw FileNotFoundError(src, line);

            std::filesystem::create_directories(dest);

#ifdef _WIN32
            std::string cmd = "tar -xf \"" + src + "\" -C \"" + dest + "\" 2>&1";
#else
            std::string cmd = "tar -xf '" + src + "' -C '" + dest + "' 2>&1";
#endif
            int ret = runArchiveCmd(cmd);
            if (ret != 0)
                throw RuntimeError("tar_extract() failed (exit code " + std::to_string(ret) + ")", line);
            return XObject::makeBool(true);
        };

        // =================================================================
        // gzip_compress(path) — compress file with gzip
        // Replaces file with .gz version
        // =================================================================
        t["gzip_compress"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("gzip_compress", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("gzip_compress() expects a file path string", line);

            std::string path = args[0].asString();
            if (!std::filesystem::exists(path))
                throw FileNotFoundError(path, line);

#ifdef _WIN32
            // Windows: use tar with gzip or PowerShell
            std::string cmd = "powershell -Command \"$bytes = [System.IO.File]::ReadAllBytes('" +
                              path + "'); $ms = New-Object System.IO.MemoryStream; $gz = New-Object System.IO.Compression.GZipStream($ms, [System.IO.Compression.CompressionMode]::Compress); $gz.Write($bytes, 0, $bytes.Length); $gz.Close(); [System.IO.File]::WriteAllBytes('" +
                              path + ".gz', $ms.ToArray())\" 2>&1";
#else
            std::string cmd = "gzip -f '" + path + "' 2>&1";
#endif
            int ret = runArchiveCmd(cmd);
            if (ret != 0)
                throw RuntimeError("gzip_compress() failed", line);
            return XObject::makeString(path + ".gz");
        };

        // =================================================================
        // gunzip_decompress(path) — decompress .gz file
        // =================================================================
        t["gunzip_decompress"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("gunzip_decompress", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("gunzip_decompress() expects a file path string", line);

            std::string path = args[0].asString();
            if (!std::filesystem::exists(path))
                throw FileNotFoundError(path, line);

            // Determine output name
            std::string outPath = path;
            if (outPath.size() >= 3 && outPath.substr(outPath.size() - 3) == ".gz")
                outPath = outPath.substr(0, outPath.size() - 3);
            else
                outPath += ".out";

#ifdef _WIN32
            std::string cmd = "powershell -Command \"$fs = [System.IO.File]::OpenRead('" +
                              path + "'); $gz = New-Object System.IO.Compression.GZipStream($fs, [System.IO.Compression.CompressionMode]::Decompress); $out = [System.IO.File]::Create('" +
                              outPath + "'); $gz.CopyTo($out); $gz.Close(); $out.Close(); $fs.Close()\" 2>&1";
#else
            std::string cmd = "gunzip -f -k '" + path + "' 2>&1";
#endif
            int ret = runArchiveCmd(cmd);
            if (ret != 0)
                throw RuntimeError("gunzip_decompress() failed", line);
            return XObject::makeString(outPath);
        };

        // =================================================================
        // bzip2_compress(path) — compress file with bzip2
        // =================================================================
        t["bzip2_compress"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("bzip2_compress", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("bzip2_compress() expects a file path string", line);

            std::string path = args[0].asString();
            if (!std::filesystem::exists(path))
                throw FileNotFoundError(path, line);

#ifdef _WIN32
            throw RuntimeError("bzip2_compress() not available on Windows without bzip2", line);
#else
            std::string cmd = "bzip2 -f -k '" + path + "' 2>&1";
            int ret = runArchiveCmd(cmd);
            if (ret != 0)
                throw RuntimeError("bzip2_compress() failed", line);
            return XObject::makeString(path + ".bz2");
#endif
        };

        // =================================================================
        // bunzip2_decompress(path) — decompress .bz2 file
        // =================================================================
        t["bunzip2_decompress"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("bunzip2_decompress", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("bunzip2_decompress() expects a file path string", line);

            std::string path = args[0].asString();
            if (!std::filesystem::exists(path))
                throw FileNotFoundError(path, line);

            std::string outPath = path;
            if (outPath.size() >= 4 && outPath.substr(outPath.size() - 4) == ".bz2")
                outPath = outPath.substr(0, outPath.size() - 4);
            else
                outPath += ".out";

#ifdef _WIN32
            throw RuntimeError("bunzip2_decompress() not available on Windows without bzip2", line);
#else
            std::string cmd = "bunzip2 -f -k '" + path + "' 2>&1";
            int ret = runArchiveCmd(cmd);
            if (ret != 0)
                throw RuntimeError("bunzip2_decompress() failed", line);
            return XObject::makeString(outPath);
#endif
        };

        // =================================================================
        // xz_compress(path) — compress file with xz
        // =================================================================
        t["xz_compress"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("xz_compress", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("xz_compress() expects a file path string", line);

            std::string path = args[0].asString();
            if (!std::filesystem::exists(path))
                throw FileNotFoundError(path, line);

#ifdef _WIN32
            throw RuntimeError("xz_compress() not available on Windows without xz", line);
#else
            std::string cmd = "xz -f -k '" + path + "' 2>&1";
            int ret = runArchiveCmd(cmd);
            if (ret != 0)
                throw RuntimeError("xz_compress() failed", line);
            return XObject::makeString(path + ".xz");
#endif
        };

        // =================================================================
        // xz_decompress(path) — decompress .xz file
        // =================================================================
        t["xz_decompress"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("xz_decompress", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("xz_decompress() expects a file path string", line);

            std::string path = args[0].asString();
            if (!std::filesystem::exists(path))
                throw FileNotFoundError(path, line);

            std::string outPath = path;
            if (outPath.size() >= 3 && outPath.substr(outPath.size() - 3) == ".xz")
                outPath = outPath.substr(0, outPath.size() - 3);
            else
                outPath += ".out";

#ifdef _WIN32
            throw RuntimeError("xz_decompress() not available on Windows without xz", line);
#else
            std::string cmd = "xz -d -f -k '" + path + "' 2>&1";
            int ret = runArchiveCmd(cmd);
            if (ret != 0)
                throw RuntimeError("xz_decompress() failed", line);
            return XObject::makeString(outPath);
#endif
        };
    }

} // namespace xell
