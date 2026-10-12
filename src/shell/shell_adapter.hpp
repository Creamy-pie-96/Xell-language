#pragma once

#include <string>
#include <cstdlib>
#include <unistd.h>
#include "../lexer/token.hpp"

namespace xell
{
    class ShellAdapter
    {
    public:
        static std::string findShellExecutable()
        {
            const char *shellEnv = std::getenv("SHELL");
            if (shellEnv && *shellEnv != '\0')
            {
                if (access(shellEnv, X_OK) == 0)
                    return shellEnv;
            }
            if (access("/bin/sh", X_OK) == 0)
                return "/bin/sh";
            return "sh";
        }

        static bool isPosixShell(const std::string &shellPath)
        {
            // Extract basename
            size_t slash = shellPath.rfind('/');
            std::string base = (slash == std::string::npos) ? shellPath : shellPath.substr(slash + 1);

            if (base == "sh" || base == "bash" || base == "zsh" ||
                base == "dash" || base == "ash" || base == "ksh" ||
                base == "busybox")
            {
                return true;
            }
            return false;
        }

        static std::string getFailFastPrefix(const std::string &shellPath)
        {
            if (isPosixShell(shellPath))
            {
                return "set -e\n";
            }
            return "";
        }

        static std::string formatVarRef(const std::string &varName, ShellQuoteContext quoteContext)
        {
            if (quoteContext == ShellQuoteContext::UNQUOTED)
            {
                return "\"$" + varName + "\"";
            }
            else if (quoteContext == ShellQuoteContext::DOUBLE_QUOTES)
            {
                return "$" + varName;
            }
            return "$" + varName;
        }
    };
} // namespace xell
