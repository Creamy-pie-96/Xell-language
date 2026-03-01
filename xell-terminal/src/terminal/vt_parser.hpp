#pragma once

// =============================================================================
// vt_parser.hpp — VT100/ANSI escape sequence parser (state machine)
// =============================================================================
// Implements a proper state machine that processes raw bytes from the PTY,
// decodes escape sequences, and updates the ScreenBuffer accordingly.
//
// The parser handles:
//   - Plain text characters
//   - C0 control codes (\r, \n, \b, \t, \a, etc.)
//   - CSI sequences (ESC [ ... )
//   - SGR (Select Graphic Rendition) for colors and attributes
//   - Cursor movement, erase, and mode commands
//   - OSC sequences (Operating System Command) — e.g. window title
// =============================================================================

#include "screen_buffer.hpp"
#include <string>
#include <vector>
#include <functional>

namespace xterm
{

    class VTParser
    {
    public:
        explicit VTParser(ScreenBuffer &buffer);

        /// Feed raw bytes from the PTY into the parser.
        /// This is the main entry point — call it whenever data arrives.
        void feed(const std::string &data);

        /// Callback for window title changes (set via OSC sequences).
        std::function<void(const std::string &)> on_title_change;

        /// Callback for bell character.
        std::function<void()> on_bell;

    private:
        // The state machine states
        enum class State
        {
            Ground,           // Normal text mode
            Escape,           // Got ESC, waiting for next char
            CSI_Entry,        // Got ESC[, collecting params
            CSI_Param,        // Inside CSI parameter digits
            CSI_Intermediate, // CSI intermediate bytes
            OSC_String,       // OSC string (e.g., window title)
            DCS_Entry,        // Device Control String (ignored but consumed)
        };

        State state_ = State::Ground;
        ScreenBuffer &buffer_;

        // Current text style applied to new characters
        Cell current_style_;

        // CSI parameter accumulation
        std::string csi_params_;       // raw parameter string (digits and semicolons)
        std::string csi_intermediate_; // intermediate bytes (space, !, ", etc.)

        // OSC string accumulation
        std::string osc_string_;

        // --- State handlers ---
        void handle_ground(char ch);
        void handle_escape(char ch);
        void handle_csi_entry(char ch);
        void handle_csi_param(char ch);
        void handle_csi_intermediate(char ch);
        void handle_osc_string(char ch);

        // --- CSI command dispatch ---
        void dispatch_csi(char cmd);

        // --- SGR (Select Graphic Rendition) ---
        void handle_sgr(const std::vector<int> &params);

        // --- Helper: parse CSI params into vector of ints ---
        std::vector<int> parse_params() const;

        // --- Reset state to ground ---
        void reset_to_ground();

        // --- UTF-8 decoder state ---
        // Accumulates bytes of a multi-byte UTF-8 sequence and emits the
        // decoded char32_t once all bytes have arrived.
        char32_t utf8_codepoint_ = 0;
        int utf8_remaining_ = 0;     // bytes still expected
        void emit_char(char32_t ch); // put decoded char on screen
    };

} // namespace xterm
