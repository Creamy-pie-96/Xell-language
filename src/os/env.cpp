// =============================================================================
// env.cpp — Environment variable operations for Xell
// =============================================================================
//
// std::getenv() is portable. Setting/unsetting requires platform ifdefs.
//
// =============================================================================

#include "os.hpp"
#include "../lib/errors/error.hpp"

#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#else
#include <cstdlib> // setenv, unsetenv
#endif

// On some systems we also need environ for listing (not used here yet)
// extern char **environ;

namespace xell
{
    namespace os
    {

        // ---- env_get --------------------------------------------------------
        std::string env_get(const std::string &name)
        {
            const char *val = std::getenv(name.c_str());
            return val ? std::string(val) : "";
        }

        // ---- env_set --------------------------------------------------------
        void env_set(const std::string &name, const std::string &value)
        {
#ifdef _WIN32
            if (!SetEnvironmentVariableA(name.c_str(), value.c_str()))
                throw IOError("cannot set environment variable '" + name + "'", 0);
#else
            if (setenv(name.c_str(), value.c_str(), 1) != 0)
                throw IOError("cannot set environment variable '" + name + "'", 0);
#endif
        }

        // ---- env_unset ------------------------------------------------------
        void env_unset(const std::string &name)
        {
#ifdef _WIN32
            if (!SetEnvironmentVariableA(name.c_str(), NULL))
                throw IOError("cannot unset environment variable '" + name + "'", 0);
#else
            if (unsetenv(name.c_str()) != 0)
                throw IOError("cannot unset environment variable '" + name + "'", 0);
#endif
        }

        // ---- env_has --------------------------------------------------------
        bool env_has(const std::string &name)
        {
            return std::getenv(name.c_str()) != nullptr;
        }

    } // namespace os
} // namespace xell
