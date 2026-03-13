#pragma once

// =============================================================================
// Completer — tab completion for the Xell REPL
// =============================================================================
// Provides completion candidates from:
//   1. Language keywords (fn, give, if, for, while, …)
//   2. Built-in functions (print, len, push, …)
//   3. User-defined identifiers from the current environment
// =============================================================================

#include "../interpreter/environment.hpp"
#include <string>
#include <vector>
#include <algorithm>

namespace xell
{

    class Completer
    {
    public:
        Completer()
        {
            // Initialize default keywords
            initializeKeywords();
        }

        /// Set a pointer to the current environment for user-defined completions
        /// and to extract builtin functions
        void setEnvironment(Environment *env)
        {
            env_ = env;
            if (env_)
                extractBuiltins();
        }

        /// Get completions matching a prefix
        std::vector<std::string> complete(const std::string &prefix) const
        {
            if (prefix.empty())
                return {};

            std::vector<std::string> matches;

            // Keywords
            for (auto &w : keywords_)
            {
                if (w.size() >= prefix.size() &&
                    w.substr(0, prefix.size()) == prefix)
                    matches.push_back(w);
            }

            // Builtins (extracted from environment on first use)
            for (auto &w : builtins_)
            {
                if (w.size() >= prefix.size() &&
                    w.substr(0, prefix.size()) == prefix)
                    // Avoid duplicates
                    if (std::find(matches.begin(), matches.end(), w) == matches.end())
                        matches.push_back(w);
            }

            // User-defined variables from environment
            if (env_)
            {
                auto names = env_->allNames();
                for (auto &n : names)
                {
                    if (n.size() >= prefix.size() &&
                        n.substr(0, prefix.size()) == prefix)
                    {
                        // Avoid duplicates
                        if (std::find(matches.begin(), matches.end(), n) == matches.end())
                            matches.push_back(n);
                    }
                }
            }

            std::sort(matches.begin(), matches.end());
            return matches;
        }

        /// Find the longest common prefix among completions
        static std::string commonPrefix(const std::vector<std::string> &matches)
        {
            if (matches.empty())
                return "";
            if (matches.size() == 1)
                return matches[0];
            std::string prefix = matches[0];
            for (size_t i = 1; i < matches.size(); i++)
            {
                size_t j = 0;
                while (j < prefix.size() && j < matches[i].size() &&
                       prefix[j] == matches[i][j])
                    j++;
                prefix = prefix.substr(0, j);
            }
            return prefix;
        }

    private:
        std::vector<std::string> keywords_;
        std::vector<std::string> builtins_;
        Environment *env_ = nullptr;

        void initializeKeywords()
        {
            // All language keywords
            keywords_ = {
                // Control flow
                "fn", "give", "if", "elif", "else", "for", "while", "in",
                "break", "continue", "try", "catch", "finally", "throw",
                "incase", "let", "be", "loop", "do",
                // Import / module
                "bring", "from", "as", "module", "export", "requires",
                // OOP
                "struct", "class", "inherits", "abstract", "interface",
                "mixin", "property", "get", "set",
                // Enum
                "enum",
                // Operators / expressions
                "and", "or", "not", "is", "of",
                // Decorators
                "decorator",
                // Literals
                "true", "false", "none"};
            std::sort(keywords_.begin(), keywords_.end());
        }

        void extractBuiltins()
        {
            if (!env_)
                return;

            // Get all names from the environment that are likely builtins
            // (This includes globally registered functions and classes)
            auto names = env_->allNames();

            // Filter to include functions, classes, and commonly known builtins
            // Exclude user variables by checking common builtin patterns
            for (auto &name : names)
            {
                // Common builtin prefixes/patterns to include
                if (isLikelyBuiltin(name))
                    builtins_.push_back(name);
            }

            std::sort(builtins_.begin(), builtins_.end());
            // Remove duplicates
            builtins_.erase(std::unique(builtins_.begin(), builtins_.end()),
                            builtins_.end());
        }

        bool isLikelyBuiltin(const std::string &name) const
        {
            // Include all-lowercase names and CapitalCase names (classes)
            // Exclude names with underscores that look like user variables
            if (name.empty())
                return false;

            // Exclude private/magic names
            if (name.substr(0, 2) == "__")
                return false;

            // Include if all lowercase (functions) or starts with capital (classes)
            if (std::islower(name[0]) || std::isupper(name[0]))
                return true;

            return false;
        }
    };

} // namespace xell
