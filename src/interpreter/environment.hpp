#pragma once

// =============================================================================
// Environment — Xell's variable scope
// =============================================================================
//
// Each scope (global, function, if/for/while block) is an Environment.
// An Environment holds a string→XObject map and a non-owning pointer to its
// parent scope.
//
// Lookup:
//   get()    — walks up the scope chain, throws UndefinedVariableError on miss
//
// Assignment:
//   set()    — if the variable exists anywhere in the chain, updates it there;
//              otherwise creates it in the current scope.
//              This means globals stay global and locals stay local.
//
// Local declaration:
//   define() — always creates/overwrites in the current scope (used for function
//              parameters, for-loop variables, and built-in seeding).
//
// =============================================================================

#include "xobject.hpp"
#include "../lib/errors/error.hpp"
#include <unordered_map>
#include <string>

namespace xell
{

    class Environment
    {
    public:
        /// Construct the global scope (no parent)
        Environment() : parent_(nullptr) {}

        /// Construct a child scope
        explicit Environment(Environment *parent) : parent_(parent) {}

        /// Look up a variable, walking up the scope chain.
        /// Throws UndefinedVariableError if not found anywhere.
        XObject get(const std::string &name, int line) const
        {
            auto it = vars_.find(name);
            if (it != vars_.end())
                return it->second;
            if (parent_)
                return parent_->get(name, line);
            throw UndefinedVariableError(name, line);
        }

        /// Set a variable:
        ///   1. Walk up the chain looking for an existing binding.
        ///   2. If found, update it in-place (preserves scope level).
        ///   3. If not found anywhere, create it in the *current* scope.
        void set(const std::string &name, XObject value)
        {
            Environment *env = this;
            while (env)
            {
                auto it = env->vars_.find(name);
                if (it != env->vars_.end())
                {
                    it->second = std::move(value);
                    return;
                }
                env = env->parent_;
            }
            // Not found anywhere — create in current scope
            vars_[name] = std::move(value);
        }

        /// Force-define in the current scope (for params, for-loop vars, etc.)
        void define(const std::string &name, XObject value)
        {
            vars_[name] = std::move(value);
        }

        /// Check whether a variable exists anywhere in the chain
        bool has(const std::string &name) const
        {
            if (vars_.count(name))
                return true;
            if (parent_)
                return parent_->has(name);
            return false;
        }

        /// Parent accessor (for debugging / testing)
        Environment *parent() const { return parent_; }

        /// Collect all variable names visible from this scope (walks up chain)
        std::vector<std::string> allNames() const
        {
            std::vector<std::string> names;
            const Environment *env = this;
            while (env)
            {
                for (auto &kv : env->vars_)
                    names.push_back(kv.first);
                env = env->parent_;
            }
            return names;
        }

    private:
        std::unordered_map<std::string, XObject> vars_;
        Environment *parent_;
    };

} // namespace xell
