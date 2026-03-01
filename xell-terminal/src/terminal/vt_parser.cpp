// =============================================================================
// vt_parser.cpp — VT100/ANSI escape sequence state machine
// =============================================================================
// A proper state machine parser. Bytes flow through states; each state decides
// whether to consume, emit, or transition. No hacky string-search shortcuts.
// =============================================================================

#include "vt_parser.hpp"
#include <algorithm>
#include <cstdlib>

namespace xterm
{

    // =============================================================================
    // Construction
    // =============================================================================

    VTParser::VTParser(ScreenBuffer &buffer) : buffer_(buffer)
    {
        current_style_ = Cell{};
    }

    // =============================================================================
    // Main entry: feed raw bytes
    // =============================================================================

    void VTParser::feed(const std::string &data)
    {
        for (char byte : data)
        {
            unsigned char ch = static_cast<unsigned char>(byte);

            switch (state_)
            {
            case State::Ground:
                handle_ground(byte);
                break;
            case State::Escape:
                handle_escape(byte);
                break;
            case State::CSI_Entry:
                handle_csi_entry(byte);
                break;
            case State::CSI_Param:
                handle_csi_param(byte);
                break;
            case State::CSI_Intermediate:
                handle_csi_intermediate(byte);
                break;
            case State::OSC_String:
                handle_osc_string(byte);
                break;
            case State::DCS_Entry:
                // Consume everything until ST (ESC \ or BEL)
                if (ch == 0x1B || ch == 0x07)
                    reset_to_ground();
                break;
            }
        }
    }

    // =============================================================================
    // State: Ground — normal text (with UTF-8 decoding)
    // =============================================================================

    void VTParser::handle_ground(char ch)
    {
        unsigned char uch = static_cast<unsigned char>(ch);

        // If we're in the middle of a multi-byte UTF-8 sequence, accumulate
        if (utf8_remaining_ > 0)
        {
            if ((uch & 0xC0) == 0x80)
            {
                // Valid continuation byte (10xxxxxx)
                utf8_codepoint_ = (utf8_codepoint_ << 6) | (uch & 0x3F);
                utf8_remaining_--;
                if (utf8_remaining_ == 0)
                {
                    // Complete! Emit the decoded character.
                    emit_char(utf8_codepoint_);
                }
            }
            else
            {
                // Invalid continuation — reset and re-process this byte
                utf8_remaining_ = 0;
                utf8_codepoint_ = 0;
                handle_ground(ch); // retry
            }
            return;
        }

        if (uch == 0x1B)
        {
            // ESC — start escape sequence
            state_ = State::Escape;
            return;
        }

        // C0 control characters
        switch (uch)
        {
        case '\r': // CR
            buffer_.carriage_return();
            return;
        case '\n': // LF
            buffer_.newline();
            return;
        case '\b': // BS
            buffer_.backspace();
            return;
        case '\t': // TAB
            buffer_.tab();
            return;
        case '\a': // BEL (bell)
            if (on_bell)
                on_bell();
            return;
        case 0x00: // NUL — ignore
        case 0x7F: // DEL — ignore
            return;
        }

        // Ignore other C0 codes (0x01-0x06, 0x0E-0x1A, 0x1C-0x1F)
        if (uch < 0x20)
            return;

        // --- UTF-8 decoding ---
        // Check if this is a multi-byte UTF-8 lead byte
        if (uch < 0x80)
        {
            // ASCII (0xxxxxxx) — single byte, emit directly
            emit_char(static_cast<char32_t>(uch));
        }
        else if ((uch & 0xE0) == 0xC0)
        {
            // 2-byte sequence (110xxxxx)
            utf8_codepoint_ = uch & 0x1F;
            utf8_remaining_ = 1;
        }
        else if ((uch & 0xF0) == 0xE0)
        {
            // 3-byte sequence (1110xxxx) — covers most Unicode including box-drawing
            utf8_codepoint_ = uch & 0x0F;
            utf8_remaining_ = 2;
        }
        else if ((uch & 0xF8) == 0xF0)
        {
            // 4-byte sequence (11110xxx) — covers emoji, etc.
            utf8_codepoint_ = uch & 0x07;
            utf8_remaining_ = 3;
        }
        else
        {
            // Invalid lead byte — skip
        }
    }

    // =============================================================================
    // Emit a decoded character to the screen buffer
    // =============================================================================

    void VTParser::emit_char(char32_t ch)
    {
        buffer_.put_char(ch, current_style_);
    }

    // =============================================================================
    // State: Escape — just got ESC (0x1B)
    // =============================================================================

    void VTParser::handle_escape(char ch)
    {
        unsigned char uch = static_cast<unsigned char>(ch);

        switch (uch)
        {
        case '[': // CSI introducer
            state_ = State::CSI_Entry;
            csi_params_.clear();
            csi_intermediate_.clear();
            return;

        case ']': // OSC introducer
            state_ = State::OSC_String;
            osc_string_.clear();
            return;

        case 'P': // DCS
            state_ = State::DCS_Entry;
            return;

        case 'M': // Reverse Index — cursor up, scroll if at top
            buffer_.reverse_index();
            reset_to_ground();
            return;

        case 'D': // Index — cursor down, scroll if at bottom
            buffer_.newline();
            reset_to_ground();
            return;

        case 'E': // Next Line
            buffer_.carriage_return();
            buffer_.newline();
            reset_to_ground();
            return;

        case '7': // Save cursor (DECSC)
            buffer_.saved_cursor_row = buffer_.cursor_row;
            buffer_.saved_cursor_col = buffer_.cursor_col;
            reset_to_ground();
            return;

        case '8': // Restore cursor (DECRC)
            buffer_.cursor_row = buffer_.saved_cursor_row;
            buffer_.cursor_col = buffer_.saved_cursor_col;
            reset_to_ground();
            return;

        case 'c': // Full Reset (RIS)
            buffer_.clear();
            current_style_ = Cell{};
            reset_to_ground();
            return;

        case '\\': // ST (String Terminator) — just go back to ground
            reset_to_ground();
            return;

        default:
            // Unknown escape — drop it and go back to ground
            reset_to_ground();
            return;
        }
    }

    // =============================================================================
    // State: CSI_Entry — just got ESC [
    // =============================================================================

    void VTParser::handle_csi_entry(char ch)
    {
        unsigned char uch = static_cast<unsigned char>(ch);

        // Private mode prefix characters: ?, >, =, <
        // e.g., ESC[?25h (cursor visibility), ESC[>1u (kitty keyboard protocol)
        if (uch == '?' || uch == '>' || uch == '=' || uch == '<')
        {
            csi_params_ += ch;
            state_ = State::CSI_Param;
            return;
        }

        // Delegate to param handler
        state_ = State::CSI_Param;
        handle_csi_param(ch);
    }

    // =============================================================================
    // State: CSI_Param — collecting digits and semicolons
    // =============================================================================

    void VTParser::handle_csi_param(char ch)
    {
        unsigned char uch = static_cast<unsigned char>(ch);

        // Parameter bytes: digits (0x30-0x39) and semicolons (0x3B)
        if ((uch >= '0' && uch <= '9') || uch == ';')
        {
            csi_params_ += ch;
            return;
        }

        // Intermediate bytes: 0x20-0x2F (space, !, ", #, $, etc.)
        if (uch >= 0x20 && uch <= 0x2F)
        {
            csi_intermediate_ += ch;
            state_ = State::CSI_Intermediate;
            return;
        }

        // Final byte: 0x40-0x7E — dispatch the command
        if (uch >= 0x40 && uch <= 0x7E)
        {
            dispatch_csi(ch);
            reset_to_ground();
            return;
        }

        // Anything else is an error — abort
        reset_to_ground();
    }

    // =============================================================================
    // State: CSI_Intermediate — intermediate bytes between params and final
    // =============================================================================

    void VTParser::handle_csi_intermediate(char ch)
    {
        unsigned char uch = static_cast<unsigned char>(ch);

        if (uch >= 0x20 && uch <= 0x2F)
        {
            csi_intermediate_ += ch;
            return;
        }

        if (uch >= 0x40 && uch <= 0x7E)
        {
            dispatch_csi(ch);
            reset_to_ground();
            return;
        }

        reset_to_ground();
    }

    // =============================================================================
    // State: OSC_String — Operating System Command (window title, etc.)
    // =============================================================================

    void VTParser::handle_osc_string(char ch)
    {
        unsigned char uch = static_cast<unsigned char>(ch);

        // OSC is terminated by BEL (0x07) or ST (ESC \)
        if (uch == 0x07)
        {
            // Process the OSC command
            // Format: number;string
            // 0 or 2 = set window title
            auto semi = osc_string_.find(';');
            if (semi != std::string::npos)
            {
                std::string code_str = osc_string_.substr(0, semi);
                std::string payload = osc_string_.substr(semi + 1);
                int code = std::atoi(code_str.c_str());

                if ((code == 0 || code == 2) && on_title_change)
                {
                    on_title_change(payload);
                }
            }
            reset_to_ground();
            return;
        }

        if (uch == 0x1B)
        {
            // Could be start of ST (ESC \) — for simplicity, treat ESC as terminator
            // (A more precise parser would enter a sub-state for the backslash.)
            auto semi = osc_string_.find(';');
            if (semi != std::string::npos)
            {
                std::string code_str = osc_string_.substr(0, semi);
                std::string payload = osc_string_.substr(semi + 1);
                int code = std::atoi(code_str.c_str());
                if ((code == 0 || code == 2) && on_title_change)
                    on_title_change(payload);
            }
            reset_to_ground();
            return;
        }

        osc_string_ += ch;
    }

    // =============================================================================
    // CSI command dispatch
    // =============================================================================

    void VTParser::dispatch_csi(char cmd)
    {
        auto params = parse_params();
        bool is_private = (!csi_params_.empty() &&
                          (csi_params_[0] == '?' || csi_params_[0] == '>' ||
                           csi_params_[0] == '=' || csi_params_[0] == '<'));

        // Default parameter is 1 for most commands, 0 for some
        auto param = [&](int idx, int def = 1) -> int
        {
            if (idx < (int)params.size() && params[idx] > 0)
                return params[idx];
            return def;
        };

        switch (cmd)
        {
        // --- Cursor movement ---
        case 'A': // CUU — Cursor Up
            buffer_.cursor_row -= param(0);
            buffer_.cursor_row = std::max(0, buffer_.cursor_row);
            break;

        case 'B': // CUD — Cursor Down
            buffer_.cursor_row += param(0);
            buffer_.cursor_row = std::min(buffer_.get_rows() - 1, buffer_.cursor_row);
            break;

        case 'C': // CUF — Cursor Forward (Right)
            buffer_.cursor_col += param(0);
            buffer_.cursor_col = std::min(buffer_.get_cols() - 1, buffer_.cursor_col);
            break;

        case 'D': // CUB — Cursor Backward (Left)
            buffer_.cursor_col -= param(0);
            buffer_.cursor_col = std::max(0, buffer_.cursor_col);
            break;

        case 'E': // CNL — Cursor Next Line
            buffer_.cursor_row += param(0);
            buffer_.cursor_col = 0;
            buffer_.cursor_row = std::min(buffer_.get_rows() - 1, buffer_.cursor_row);
            break;

        case 'F': // CPL — Cursor Previous Line
            buffer_.cursor_row -= param(0);
            buffer_.cursor_col = 0;
            buffer_.cursor_row = std::max(0, buffer_.cursor_row);
            break;

        case 'G':                                 // CHA — Cursor Horizontal Absolute
            buffer_.cursor_col = param(0, 1) - 1; // 1-based
            buffer_.cursor_col = std::clamp(buffer_.cursor_col, 0, buffer_.get_cols() - 1);
            break;

        case 'H':                                 // CUP — Cursor Position
        case 'f':                                 // HVP — Horizontal Vertical Position (same as CUP)
            buffer_.cursor_row = param(0, 1) - 1; // 1-based
            buffer_.cursor_col = (params.size() >= 2 ? param(1, 1) : 1) - 1;
            buffer_.cursor_row = std::clamp(buffer_.cursor_row, 0, buffer_.get_rows() - 1);
            buffer_.cursor_col = std::clamp(buffer_.cursor_col, 0, buffer_.get_cols() - 1);
            break;

        case 'd': // VPA — Vertical Position Absolute
            buffer_.cursor_row = param(0, 1) - 1;
            buffer_.cursor_row = std::clamp(buffer_.cursor_row, 0, buffer_.get_rows() - 1);
            break;

        // --- Erase ---
        case 'J': // ED — Erase Display
            buffer_.erase_display(param(0, 0));
            break;

        case 'K': // EL — Erase Line
            buffer_.erase_line(param(0, 0));
            break;

        // --- Scroll ---
        case 'S': // SU — Scroll Up
            buffer_.scroll_up(param(0));
            break;

        case 'T': // SD — Scroll Down
            buffer_.scroll_down(param(0));
            break;

        // --- SGR — Select Graphic Rendition ---
        case 'm':
            handle_sgr(params);
            break;

        // --- Cursor visibility and save/restore ---
        case 'h': // SM — Set Mode
            if (is_private)
            {
                int mode = param(0, 0);
                if (mode == 25)
                {
                    buffer_.cursor_visible = true; // DECTCEM: show cursor
                }
                // mode 1049: alternate screen buffer — not implemented yet
            }
            break;

        case 'l': // RM — Reset Mode
            if (is_private)
            {
                int mode = param(0, 0);
                if (mode == 25)
                {
                    buffer_.cursor_visible = false; // DECTCEM: hide cursor
                }
            }
            break;

        case 's': // SCP — Save Cursor Position
            buffer_.saved_cursor_row = buffer_.cursor_row;
            buffer_.saved_cursor_col = buffer_.cursor_col;
            break;

        case 'u': // RCP — Restore Cursor Position, or kitty keyboard protocol response
            // Only restore cursor for plain ESC[u, not ESC[>...u (kitty protocol)
            if (!is_private && csi_params_.empty())
            {
                buffer_.cursor_row = buffer_.saved_cursor_row;
                buffer_.cursor_col = buffer_.saved_cursor_col;
            }
            // ESC[>1u, ESC[<u, etc. are kitty keyboard protocol — silently consume
            break;

        // --- Insert/Delete lines ---
        case 'L': // IL — Insert Lines
            // Insert N blank lines at cursor, pushing existing lines down
            for (int i = 0; i < param(0); ++i)
                buffer_.scroll_down(1);
            break;

        case 'M': // DL — Delete Lines
            for (int i = 0; i < param(0); ++i)
                buffer_.scroll_up(1);
            break;

        // --- Delete/Insert Characters ---
        case 'P':
        { // DCH — Delete Characters
            int n = param(0);
            int r = buffer_.cursor_row;
            int c = buffer_.cursor_col;
            int cols = buffer_.get_cols();
            for (int i = c; i < cols - n; ++i)
                buffer_.set_cell(r, i, buffer_.get_cell(r, i + n));
            for (int i = cols - n; i < cols; ++i)
            {
                Cell blank;
                buffer_.set_cell(r, i, blank);
            }
            break;
        }

        case '@':
        { // ICH — Insert Characters
            int n = param(0);
            int r = buffer_.cursor_row;
            int c = buffer_.cursor_col;
            int cols = buffer_.get_cols();
            for (int i = cols - 1; i >= c + n; --i)
                buffer_.set_cell(r, i, buffer_.get_cell(r, i - n));
            for (int i = c; i < c + n && i < cols; ++i)
            {
                Cell blank;
                buffer_.set_cell(r, i, blank);
            }
            break;
        }

        case 'X':
        { // ECH — Erase Characters
            int n = param(0);
            int r = buffer_.cursor_row;
            int c = buffer_.cursor_col;
            int cols = buffer_.get_cols();
            for (int i = c; i < c + n && i < cols; ++i)
            {
                Cell blank;
                buffer_.set_cell(r, i, blank);
            }
            break;
        }

        // --- Device Status Report ---
        case 'n':
            // 6 = Report Cursor Position — we'd need to write back to the PTY
            // For now, silently ignore
            break;

        // --- Tab clear ---
        case 'g':
            // 0 = clear current tab stop, 3 = clear all — ignore for now
            break;

        default:
            // Unknown CSI command — silently ignore
            break;
        }
    }

    // =============================================================================
    // SGR — Select Graphic Rendition
    // =============================================================================

    void VTParser::handle_sgr(const std::vector<int> &params)
    {
        // If no params, treat as reset (SGR 0)
        if (params.empty())
        {
            current_style_ = Cell{};
            return;
        }

        for (size_t i = 0; i < params.size(); ++i)
        {
            int code = params[i];

            switch (code)
            {
            case 0: // Reset all attributes
                current_style_ = Cell{};
                break;

            case 1: // Bold
                current_style_.bold = true;
                break;

            case 2: // Dim (faint) — treat as not bold
                current_style_.bold = false;
                break;

            case 3: // Italic
                current_style_.italic = true;
                break;

            case 4: // Underline
                current_style_.underline = true;
                break;

            case 22: // Normal intensity (not bold, not faint)
                current_style_.bold = false;
                break;

            case 23: // Not italic
                current_style_.italic = false;
                break;

            case 24: // Not underlined
                current_style_.underline = false;
                break;

            // --- Standard foreground colors (30-37) ---
            case 30:
            case 31:
            case 32:
            case 33:
            case 34:
            case 35:
            case 36:
            case 37:
                current_style_.fg = Color::ansi(code - 30);
                break;

            // --- Default foreground ---
            case 39:
                current_style_.fg = Color::default_fg();
                break;

            // --- Standard background colors (40-47) ---
            case 40:
            case 41:
            case 42:
            case 43:
            case 44:
            case 45:
            case 46:
            case 47:
                current_style_.bg = Color::ansi(code - 40);
                break;

            // --- Default background ---
            case 49:
                current_style_.bg = Color::default_bg();
                break;

            // --- Bright foreground colors (90-97) ---
            case 90:
            case 91:
            case 92:
            case 93:
            case 94:
            case 95:
            case 96:
            case 97:
                current_style_.fg = Color::ansi_bright(code - 90);
                break;

            // --- Bright background colors (100-107) ---
            case 100:
            case 101:
            case 102:
            case 103:
            case 104:
            case 105:
            case 106:
            case 107:
                current_style_.bg = Color::ansi_bright(code - 100);
                break;

            // --- Extended colors (256-color and true color) ---
            case 38: // Set foreground: 38;2;r;g;b or 38;5;n
                if (i + 1 < params.size())
                {
                    if (params[i + 1] == 2 && i + 4 < params.size())
                    {
                        // True color: 38;2;r;g;b
                        current_style_.fg = Color(
                            static_cast<uint8_t>(params[i + 2]),
                            static_cast<uint8_t>(params[i + 3]),
                            static_cast<uint8_t>(params[i + 4]));
                        i += 4;
                    }
                    else if (params[i + 1] == 5 && i + 2 < params.size())
                    {
                        // 256-color: 38;5;n — map to approximate color
                        int n = params[i + 2];
                        if (n >= 0 && n <= 7)
                            current_style_.fg = Color::ansi(n);
                        else if (n >= 8 && n <= 15)
                            current_style_.fg = Color::ansi_bright(n - 8);
                        else if (n >= 16 && n <= 231)
                        {
                            // 6x6x6 color cube
                            int idx = n - 16;
                            int r = (idx / 36) * 51;
                            int g = ((idx / 6) % 6) * 51;
                            int b = (idx % 6) * 51;
                            current_style_.fg = Color(r, g, b);
                        }
                        else if (n >= 232 && n <= 255)
                        {
                            // Grayscale ramp
                            int v = (n - 232) * 10 + 8;
                            current_style_.fg = Color(v, v, v);
                        }
                        i += 2;
                    }
                }
                break;

            case 48: // Set background: 48;2;r;g;b or 48;5;n
                if (i + 1 < params.size())
                {
                    if (params[i + 1] == 2 && i + 4 < params.size())
                    {
                        current_style_.bg = Color(
                            static_cast<uint8_t>(params[i + 2]),
                            static_cast<uint8_t>(params[i + 3]),
                            static_cast<uint8_t>(params[i + 4]));
                        i += 4;
                    }
                    else if (params[i + 1] == 5 && i + 2 < params.size())
                    {
                        int n = params[i + 2];
                        if (n >= 0 && n <= 7)
                            current_style_.bg = Color::ansi(n);
                        else if (n >= 8 && n <= 15)
                            current_style_.bg = Color::ansi_bright(n - 8);
                        else if (n >= 16 && n <= 231)
                        {
                            int idx = n - 16;
                            int r = (idx / 36) * 51;
                            int g = ((idx / 6) % 6) * 51;
                            int b = (idx % 6) * 51;
                            current_style_.bg = Color(r, g, b);
                        }
                        else if (n >= 232 && n <= 255)
                        {
                            int v = (n - 232) * 10 + 8;
                            current_style_.bg = Color(v, v, v);
                        }
                        i += 2;
                    }
                }
                break;

            default:
                // Unknown SGR code — ignore
                break;
            }
        }
    }

    // =============================================================================
    // Parse CSI parameter string "1;2;3" → {1, 2, 3}
    // =============================================================================

    std::vector<int> VTParser::parse_params() const
    {
        std::vector<int> result;

        // Skip leading private mode prefix (?, >, =, <)
        std::string raw = csi_params_;
        if (!raw.empty() && (raw[0] == '?' || raw[0] == '>' || raw[0] == '=' || raw[0] == '<'))
            raw = raw.substr(1);

        if (raw.empty())
            return result;

        // Split on ';'
        std::string current;
        for (char c : raw)
        {
            if (c == ';')
            {
                result.push_back(current.empty() ? 0 : std::atoi(current.c_str()));
                current.clear();
            }
            else
            {
                current += c;
            }
        }
        result.push_back(current.empty() ? 0 : std::atoi(current.c_str()));

        return result;
    }

    // =============================================================================
    // Reset
    // =============================================================================

    void VTParser::reset_to_ground()
    {
        state_ = State::Ground;
        csi_params_.clear();
        csi_intermediate_.clear();
        osc_string_.clear();
    }

} // namespace xterm
