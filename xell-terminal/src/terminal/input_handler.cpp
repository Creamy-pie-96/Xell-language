// =============================================================================
// input_handler.cpp — SDL keyboard events → PTY byte sequences
// =============================================================================
// Translates SDL key events into the escape sequences and control codes that
// a terminal expects. Handles arrow keys, function keys, Ctrl+key combos, etc.
// =============================================================================

#include "input_handler.hpp"
#include <string>

namespace xterm
{

    std::string InputHandler::translate(const SDL_Event &event)
    {
        if (event.type != SDL_KEYDOWN)
            return {};

        const auto &key = event.key;
        SDL_Keycode sym = key.keysym.sym;
        Uint16 mod = key.keysym.mod;

        bool ctrl = (mod & KMOD_CTRL) != 0;
        bool shift = (mod & KMOD_SHIFT) != 0;
        bool alt = (mod & KMOD_ALT) != 0;

        // ---- Ctrl + letter combos ----
        if (ctrl && !alt)
        {
            // Ctrl+A through Ctrl+Z → 0x01 through 0x1A
            if (sym >= SDLK_a && sym <= SDLK_z)
            {
                char c = static_cast<char>(sym - SDLK_a + 1);
                return std::string(1, c);
            }
            // Ctrl+[ → ESC (0x1B)
            if (sym == SDLK_LEFTBRACKET)
                return "\x1b";
            // Ctrl+\ → 0x1C
            if (sym == SDLK_BACKSLASH)
                return "\x1c";
            // Ctrl+] → 0x1D
            if (sym == SDLK_RIGHTBRACKET)
                return "\x1d";
        }

        // ---- Special keys ----
        switch (sym)
        {
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            // Kitty keyboard protocol: Shift+Enter = \033[13;2u
            // This is what the Xell REPL uses to distinguish "execute" from "newline"
            if (shift)
                return "\033[13;2u";
            // Alt+Enter also means "execute" in the Xell REPL
            if (alt)
                return "\033\r"; // ESC followed by CR
            return "\r";

        case SDLK_BACKSPACE:
            return "\x7f";

        case SDLK_TAB:
            if (shift)
                return "\033[Z"; // Shift+Tab (backtab)
            return "\t";

        case SDLK_ESCAPE:
            return "\x1b";

        // ---- Arrow keys ----
        case SDLK_UP:
            if (ctrl)
                return "\033[1;5A";
            if (shift)
                return "\033[1;2A";
            if (alt)
                return "\033[1;3A";
            return "\033[A";

        case SDLK_DOWN:
            if (ctrl)
                return "\033[1;5B";
            if (shift)
                return "\033[1;2B";
            if (alt)
                return "\033[1;3B";
            return "\033[B";

        case SDLK_RIGHT:
            if (ctrl)
                return "\033[1;5C";
            if (shift)
                return "\033[1;2C";
            if (alt)
                return "\033[1;3C";
            return "\033[C";

        case SDLK_LEFT:
            if (ctrl)
                return "\033[1;5D";
            if (shift)
                return "\033[1;2D";
            if (alt)
                return "\033[1;3D";
            return "\033[D";

        // ---- Navigation keys ----
        case SDLK_HOME:
            return "\033[H";

        case SDLK_END:
            return "\033[F";

        case SDLK_INSERT:
            return "\033[2~";

        case SDLK_DELETE:
            return "\033[3~";

        case SDLK_PAGEUP:
            return "\033[5~";

        case SDLK_PAGEDOWN:
            return "\033[6~";

        // ---- Function keys ----
        case SDLK_F1:
            return "\033OP";
        case SDLK_F2:
            return "\033OQ";
        case SDLK_F3:
            return "\033OR";
        case SDLK_F4:
            return "\033OS";
        case SDLK_F5:
            return "\033[15~";
        case SDLK_F6:
            return "\033[17~";
        case SDLK_F7:
            return "\033[18~";
        case SDLK_F8:
            return "\033[19~";
        case SDLK_F9:
            return "\033[20~";
        case SDLK_F10:
            return "\033[21~";
        case SDLK_F11:
            return "\033[23~";
        case SDLK_F12:
            return "\033[24~";

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
