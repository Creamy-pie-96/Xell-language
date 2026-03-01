// =============================================================================
// input_handler.cpp — SDL keyboard events → PTY byte sequences / actions
// =============================================================================
// Translates SDL key events into the escape sequences and control codes that
// a terminal expects. Also handles terminal-internal actions like copy/paste,
// zoom, etc.
// =============================================================================

#include "input_handler.hpp"
#include <string>

namespace xterm
{

    InputResult InputHandler::translate(const SDL_Event &event, bool has_selection)
    {
        if (event.type != SDL_KEYDOWN)
            return {};

        const auto &key = event.key;
        SDL_Keycode sym = key.keysym.sym;
        Uint16 mod = key.keysym.mod;

        bool ctrl = (mod & KMOD_CTRL) != 0;
        bool shift = (mod & KMOD_SHIFT) != 0;
        bool alt = (mod & KMOD_ALT) != 0;

        // ---- Terminal-internal shortcuts (never sent to PTY) ----

        // Copy: Ctrl+Shift+C, OR Ctrl+C when text is selected
        if (ctrl && sym == SDLK_c && (shift || has_selection))
            return {InputAction::COPY, {}};

        // Paste: Ctrl+Shift+V, Ctrl+V, or Shift+Insert
        if ((ctrl && sym == SDLK_v) ||
            (shift && sym == SDLK_INSERT))
            return {InputAction::PASTE, {}};

        // Select All: Ctrl+Shift+A
        if (ctrl && shift && sym == SDLK_a)
            return {InputAction::SELECT_ALL, {}};

        // Zoom: Ctrl+= (in), Ctrl+- (out), Ctrl+0 (reset)
        if (ctrl && !shift && !alt)
        {
            if (sym == SDLK_EQUALS || sym == SDLK_PLUS || sym == SDLK_KP_PLUS)
                return {InputAction::ZOOM_IN, {}};
            if (sym == SDLK_MINUS || sym == SDLK_KP_MINUS)
                return {InputAction::ZOOM_OUT, {}};
            if (sym == SDLK_0 || sym == SDLK_KP_0)
                return {InputAction::ZOOM_RESET, {}};
        }

        // ---- Ctrl + letter combos → control codes to PTY ----
        if (ctrl && !alt && !shift)
        {
            // Ctrl+A through Ctrl+Z → 0x01 through 0x1A
            if (sym >= SDLK_a && sym <= SDLK_z)
            {
                char c = static_cast<char>(sym - SDLK_a + 1);
                return {InputAction::SEND_TO_PTY, std::string(1, c)};
            }
            // Ctrl+[ → ESC (0x1B)
            if (sym == SDLK_LEFTBRACKET)
                return {InputAction::SEND_TO_PTY, "\x1b"};
            // Ctrl+\ → 0x1C
            if (sym == SDLK_BACKSLASH)
                return {InputAction::SEND_TO_PTY, "\x1c"};
            // Ctrl+] → 0x1D
            if (sym == SDLK_RIGHTBRACKET)
                return {InputAction::SEND_TO_PTY, "\x1d"};
        }

        // ---- Special keys ----
        std::string seq;
        switch (sym)
        {
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            // Kitty keyboard protocol: Shift+Enter = \033[13;2u
            if (shift)
                seq = "\033[13;2u";
            else if (alt)
                seq = "\033\r"; // ESC followed by CR
            else
                seq = "\r";
            return {InputAction::SEND_TO_PTY, seq};

        case SDLK_BACKSPACE:
            return {InputAction::SEND_TO_PTY, "\x7f"};

        case SDLK_TAB:
            return {InputAction::SEND_TO_PTY, shift ? "\033[Z" : "\t"};

        case SDLK_ESCAPE:
            return {InputAction::SEND_TO_PTY, "\x1b"};

        // ---- Arrow keys ----
        case SDLK_UP:
            if (ctrl) seq = "\033[1;5A";
            else if (shift) seq = "\033[1;2A";
            else if (alt) seq = "\033[1;3A";
            else seq = "\033[A";
            return {InputAction::SEND_TO_PTY, seq};

        case SDLK_DOWN:
            if (ctrl) seq = "\033[1;5B";
            else if (shift) seq = "\033[1;2B";
            else if (alt) seq = "\033[1;3B";
            else seq = "\033[B";
            return {InputAction::SEND_TO_PTY, seq};

        case SDLK_RIGHT:
            if (ctrl) seq = "\033[1;5C";
            else if (shift) seq = "\033[1;2C";
            else if (alt) seq = "\033[1;3C";
            else seq = "\033[C";
            return {InputAction::SEND_TO_PTY, seq};

        case SDLK_LEFT:
            if (ctrl) seq = "\033[1;5D";
            else if (shift) seq = "\033[1;2D";
            else if (alt) seq = "\033[1;3D";
            else seq = "\033[D";
            return {InputAction::SEND_TO_PTY, seq};

        // ---- Navigation keys ----
        case SDLK_HOME:  return {InputAction::SEND_TO_PTY, "\033[H"};
        case SDLK_END:   return {InputAction::SEND_TO_PTY, "\033[F"};
        case SDLK_INSERT:return {InputAction::SEND_TO_PTY, "\033[2~"};
        case SDLK_DELETE: return {InputAction::SEND_TO_PTY, "\033[3~"};
        case SDLK_PAGEUP: return {InputAction::SEND_TO_PTY, "\033[5~"};
        case SDLK_PAGEDOWN: return {InputAction::SEND_TO_PTY, "\033[6~"};

        // ---- Function keys ----
        case SDLK_F1:  return {InputAction::SEND_TO_PTY, "\033OP"};
        case SDLK_F2:  return {InputAction::SEND_TO_PTY, "\033OQ"};
        case SDLK_F3:  return {InputAction::SEND_TO_PTY, "\033OR"};
        case SDLK_F4:  return {InputAction::SEND_TO_PTY, "\033OS"};
        case SDLK_F5:  return {InputAction::SEND_TO_PTY, "\033[15~"};
        case SDLK_F6:  return {InputAction::SEND_TO_PTY, "\033[17~"};
        case SDLK_F7:  return {InputAction::SEND_TO_PTY, "\033[18~"};
        case SDLK_F8:  return {InputAction::SEND_TO_PTY, "\033[19~"};
        case SDLK_F9:  return {InputAction::SEND_TO_PTY, "\033[20~"};
        case SDLK_F10: return {InputAction::SEND_TO_PTY, "\033[21~"};
        case SDLK_F11: return {InputAction::SEND_TO_PTY, "\033[23~"};
        case SDLK_F12: return {InputAction::SEND_TO_PTY, "\033[24~"};

        default:
            break;
        }

        // Regular keys are handled by SDL_TextInput events, not here
        return {};
    }

    std::string InputHandler::translate_text(const SDL_Event &event)
    {
        if (event.type != SDL_TEXTINPUT)
            return {};

        // SDL_TextInput gives us UTF-8 text directly
        return event.text.text;
    }

} // namespace xterm
