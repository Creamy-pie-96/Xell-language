#pragma once

// =============================================================================
// Builtin Registry — central type definitions for Xell built-in functions
// =============================================================================
//
// Every built-in module (io, math, collection, …) registers its functions into
// a  BuiltinTable  (unordered_map<string, BuiltinFn>).
//
// To add a new category of builtins:
//   1. Create  src/builtins/builtins_<category>.hpp
//   2. Write a free function:
//          void registerXxxBuiltins(BuiltinTable &t, <optional captures>);
//   3. Call it from  register_all.hpp → registerAllBuiltins().
//
// =============================================================================

#include "../interpreter/xobject.hpp"
#include "../lib/errors/error.hpp"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace xell
{

    /// Signature every built-in function must match.
    using BuiltinFn = std::function<XObject(std::vector<XObject> &args, int line)>;

    /// The table the interpreter owns; modules insert into it.
    using BuiltinTable = std::unordered_map<std::string, BuiltinFn>;

} // namespace xell
