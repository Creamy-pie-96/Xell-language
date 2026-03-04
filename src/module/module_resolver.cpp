#include "module_resolver.hpp"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <filesystem>
#include <regex>

// Portable dirname
static std::string parentDir(const std::string &path)
{
    auto pos = path.find_last_of("/\\");
    if (pos == std::string::npos)
        return ".";
    return path.substr(0, pos);
}

namespace xell
{

    std::vector<std::string> ModuleResolver::buildSearchPath(const std::string &fromDir) const
    {
        std::vector<std::string> path;

        // 1. from "dir" — explicit directory
        if (!fromDir.empty())
        {
            std::string resolved = fromDir;
            if (!sourceFile_.empty() && !fromDir.empty() &&
                fromDir[0] != '/' && !(fromDir.size() > 1 && fromDir[1] == ':'))
            {
                resolved = parentDir(sourceFile_) + "/" + fromDir;
            }
            path.push_back(canonicalize(resolved));
        }

        // 2. Directory of the current source file
        if (!sourceFile_.empty())
        {
            path.push_back(parentDir(canonicalize(sourceFile_)));
        }
        else
        {
            path.push_back(".");
        }

        // 3. XELL_PATH environment variable (colon-separated on Unix, semicolon on Windows)
        const char *xellPath = std::getenv("XELL_PATH");
        if (xellPath)
        {
            std::string pathStr(xellPath);
#ifdef _WIN32
            char delim = ';';
#else
            char delim = ':';
#endif
            std::istringstream ss(pathStr);
            std::string dir;
            while (std::getline(ss, dir, delim))
            {
                if (!dir.empty())
                    path.push_back(dir);
            }
        }

        // 4. Standard library directory
        // TODO(bytecode): Add stdlib path from installation
        // For now, check relative to the executable or a known location

        return path;
    }

    std::string ModuleResolver::resolveRelative(const std::string &basePath, const std::string &relative)
    {
        if (!relative.empty() && (relative[0] == '/' || (relative.size() > 1 && relative[1] == ':')))
            return relative; // absolute
        return parentDir(basePath) + "/" + relative;
    }

    std::string ModuleResolver::canonicalize(const std::string &path)
    {
#ifdef _WIN32
        char buf[_MAX_PATH];
        if (_fullpath(buf, path.c_str(), _MAX_PATH))
            return buf;
        return path;
#else
        char *resolved = ::realpath(path.c_str(), nullptr);
        if (resolved)
        {
            std::string result(resolved);
            ::free(resolved);
            return result;
        }
        return path;
#endif
    }

    std::string ModuleResolver::hashFile(const std::string &filePath)
    {
        std::ifstream f(filePath, std::ios::binary);
        if (!f.is_open())
            return "";
        std::ostringstream ss;
        ss << f.rdbuf();
        std::string content = ss.str();
        return hash::sha256_string(content);
    }

    // ====================================================================
    // .xell_meta fast-path reader (simple JSON parser)
    // ====================================================================
    //
    // Reads .xell_meta JSON and extracts the "modules" mapping:
    //   module_name → filename (relative to the directory)
    //
    // This is a lightweight parser — just enough to extract the mapping.
    // Uses regex to parse the simple expected format.

    std::unordered_map<std::string, std::string>
    ModuleResolver::readMetaIndex(const std::string &dirPath)
    {
        std::unordered_map<std::string, std::string> result;
        namespace fs = std::filesystem;

        fs::path metaPath = fs::path(dirPath) / ".xell_meta";
        if (!fs::exists(metaPath))
            return result;

        std::ifstream f(metaPath.string());
        if (!f.is_open())
            return result;

        std::string content((std::istreambuf_iterator<char>(f)),
                            std::istreambuf_iterator<char>());

        // Simple extraction: find "module_name": { "file": "filename.xell", ... }
        // Uses a regex to find module entries in the JSON.
        // Pattern: "NAME" : { "file" : "VALUE"
        std::string pat = "\"(\\w+)\"\\s*:\\s*\\{\\s*\"file\"\\s*:\\s*\"([^\"]+)\"";
        std::regex entryRe(pat);
        auto begin = std::sregex_iterator(content.begin(), content.end(), entryRe);
        auto end = std::sregex_iterator();

        // But we need to only match inside "modules" block, not top-level keys
        // Find the "modules" block start
        auto modulesPos = content.find("\"modules\"");
        if (modulesPos == std::string::npos)
            return result;

        std::string modulesBlock = content.substr(modulesPos);
        begin = std::sregex_iterator(modulesBlock.begin(), modulesBlock.end(), entryRe);

        for (auto it = begin; it != end; ++it)
        {
            std::string modName = (*it)[1].str();
            std::string fileName = (*it)[2].str();
            result[modName] = fileName;
        }

        return result;
    }

    // ====================================================================
    // Slow-path scan: parse all .xell files to find module names
    // ====================================================================
    //
    // Scans .xell files and looks for "module NAME :" declarations.
    // This is O(n) in the number of files — much slower than .xell_meta.

    std::unordered_map<std::string, std::string>
    ModuleResolver::slowPathScan(const std::string &dirPath)
    {
        std::unordered_map<std::string, std::string> result;
        namespace fs = std::filesystem;

        if (!fs::exists(dirPath) || !fs::is_directory(dirPath))
            return result;

        // Regex to find "module IDENTIFIER :" at the beginning of a line
        std::string modPat = "^\\s*module\\s+(\\w+)\\s*:";
        std::regex moduleRe(modPat, std::regex_constants::multiline);

        for (const auto &entry : fs::directory_iterator(dirPath))
        {
            if (entry.is_regular_file())
            {
                auto ext = entry.path().extension().string();
                if (ext != ".xel" && ext != ".xell")
                    continue;
                std::ifstream f(entry.path().string());
                if (!f.is_open())
                    continue;

                std::string content((std::istreambuf_iterator<char>(f)),
                                    std::istreambuf_iterator<char>());

                auto begin = std::sregex_iterator(content.begin(), content.end(), moduleRe);
                auto end = std::sregex_iterator();

                for (auto it = begin; it != end; ++it)
                {
                    std::string modName = (*it)[1].str();
                    result[modName] = entry.path().string();
                }
            }
        }

        return result;
    }

    // ====================================================================
    // resolveFromFileSystem — full module resolution pipeline
    // ====================================================================

    std::shared_ptr<XModule> ModuleResolver::resolveFromFileSystem(
        const std::string &moduleName,
        const std::string &fromDir,
        std::vector<std::string> *warnings)
    {
        // Already cached?
        if (isSessionCached(moduleName))
            return getSessionCached(moduleName);

        auto searchPath = buildSearchPath(fromDir);
        std::string foundFilePath;

        for (const auto &dir : searchPath)
        {
            // Fast path: check .xell_meta
            auto metaIndex = readMetaIndex(dir);
            if (!metaIndex.empty())
            {
                auto it = metaIndex.find(moduleName);
                if (it != metaIndex.end())
                {
                    namespace fs = std::filesystem;
                    foundFilePath = (fs::path(dir) / it->second).string();
                    break;
                }
                continue; // .xell_meta exists but doesn't have this module
            }

            // Slow path: scan .xell files
            auto scanned = slowPathScan(dir);
            if (!scanned.empty())
            {
                if (warnings)
                {
                    warnings->push_back("Warning: no .xell_meta found in " + dir +
                                        "\n         scanning all .xell files (slow). Run:\n"
                                        "             xell --make_module " +
                                        dir + "/\n"
                                              "         to index this directory.");
                }

                auto it = scanned.find(moduleName);
                if (it != scanned.end())
                {
                    foundFilePath = it->second;
                    break;
                }
            }
        }

        if (foundFilePath.empty())
            return nullptr;

        // Canonical path for circular detection
        std::string canonical = canonicalize(foundFilePath);

        if (isImporting(canonical))
            return nullptr; // Circular import

        // Execute the file through the callback
        if (!fileExecutor_)
            return nullptr;

        markImporting(canonical);

        try
        {
            auto modulesInFile = fileExecutor_(foundFilePath);
            unmarkImporting(canonical);

            // Cache all modules found in the file
            for (auto &[name, mod] : modulesInFile)
            {
                if (!isSessionCached(name))
                    cacheModule(name, mod);
            }

            // Return the requested module
            auto it = modulesInFile.find(moduleName);
            if (it != modulesInFile.end())
                return it->second;

            // May have been cached by the executor
            return getSessionCached(moduleName);
        }
        catch (...)
        {
            unmarkImporting(canonical);
            throw;
        }
    }

} // namespace xell
