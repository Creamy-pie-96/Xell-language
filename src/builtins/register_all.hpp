#pragma once

// =============================================================================
// register_all.hpp — wire every builtin category into the interpreter
// =============================================================================
//
// Usage (from Interpreter::registerBuiltins):
//     registerAllBuiltins(builtins_, output_);
//
// To add a new category:
//   1. #include "builtins_<category>.hpp"
//   2. Call register<Category>Builtins(t, ...) below.
//
// =============================================================================

#include "builtin_registry.hpp"
#include "builtins_io.hpp"
#include "builtins_math.hpp"
#include "builtins_type.hpp"
#include "builtins_collection.hpp"
#include "builtins_util.hpp"
#include "builtins_os.hpp"
#include "builtins_hash.hpp"
#include "../interpreter/shell_state.hpp"

namespace xell
{

    /// Registers every built-in function into the given table.
    /// @param t          The interpreter's builtin table.
    /// @param output     Reference to the interpreter's captured output vector
    ///                   (needed by IO builtins like print).
    /// @param shellState Reference to the interpreter's shell state
    ///                   (needed by OS builtins for set_e / exit_code).
    inline void registerAllBuiltins(BuiltinTable &t, std::vector<std::string> &output,
                                    ShellState &shellState)
    {
        registerIOBuiltins(t, output);
        registerMathBuiltins(t);
        registerTypeBuiltins(t);
        registerCollectionBuiltins(t);
        registerUtilBuiltins(t);
        registerOSBuiltins(t, shellState);
        registerHashBuiltins(t);
    }

} // namespace xell
