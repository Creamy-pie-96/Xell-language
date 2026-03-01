#pragma once

// =============================================================================
// input_handler.hpp — Keyboard input → PTY byte sequence translation
// =============================================================================

#include <string>
#include <SDL2/SDL.h>

namespace xterm
{

    class InputHandler
    {
    public:
        /// Translate an SDL keyboard event into the byte sequence that should be
        /// sent to the PTY. Returns empty string if the event is not relevant
        /// (e.g., key-up events, modifier-only presses).
        static std::string translate(const SDL_Event &event);

        /// Translate an SDL_TextInput event (for regular character input).
        static std::string translate_text(const SDL_Event &event);
    };

} // namespace xterm
