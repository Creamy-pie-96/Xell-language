#pragma once

// =============================================================================
// ShellState — shared state for shell-mode features (set_e, exit codes, etc.)
// =============================================================================

namespace xell
{

    struct ShellState
    {
        bool exitOnError = false; // set_e() mode
        int lastExitCode = 0;     // last run() exit code
    };

} // namespace xell
