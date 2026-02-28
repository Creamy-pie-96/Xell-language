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
            // Keywords
            addWords({"fn", "give", "if", "elif", "else", "for", "while", "in",
                      "bring", "from", "as", "and", "or", "not",
                      "is", "eq", "ne", "gt", "lt", "ge", "le", "of",
                      "true", "false", "none"});

            // IO builtins
            addWords({"print", "assert"});

            // Type builtins
            addWords({"type", "str", "num", "len"});

            // Collection builtins
            addWords({"push", "pop", "keys", "values", "range", "set", "has"});

            // Math builtins
            addWords({"floor", "ceil", "round", "abs", "mod"});

            // Utility builtins
            addWords({"input"});

            // OS builtins
            addWords({"mkdir", "rm", "cp", "mv", "exists", "is_file", "is_dir",
                      "ls", "read", "write", "append", "file_size",
                      "cwd", "cd", "abspath", "basename", "dirname", "ext",
                      "env_get", "env_set", "env_unset", "env_has",
                      "run", "run_capture", "pid"});
        }

        /// Set a pointer to the current environment for user-defined completions
        void setEnvironment(Environment *env) { env_ = env; }

        /// Get completions matching a prefix
        std::vector<std::string> complete(const std::string &prefix) const
        {
            if (prefix.empty())
                return {};

            std::vector<std::string> matches;

            // Static words
            for (auto &w : words_)
            {
                if (w.size() >= prefix.size() &&
                    w.substr(0, prefix.size()) == prefix)
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
        std::vector<std::string> words_;
        Environment *env_ = nullptr;

        void addWords(std::initializer_list<std::string> ws)
        {
            for (auto &w : ws)
                words_.push_back(w);
        }
    };

} // namespace xell
