#pragma once

// =============================================================================
// input_handler.hpp — Keyboard input → PTY byte sequence translation
// =============================================================================

#include <string>
#include <SDL2/SDL.h>

namespace xterm
{

    /// Result of translating an input event — may be a PTY sequence or a
    /// terminal-internal action (copy, paste, zoom, etc.)
    enum class InputAction
    {
        NONE,           // nothing to do
        SEND_TO_PTY,    // send .data to the PTY
        COPY,           // copy selection to clipboard
        PASTE,          // paste clipboard to PTY
        SELECT_ALL,     // select all text
        ZOOM_IN,        // increase font size
        ZOOM_OUT,       // decrease font size
        ZOOM_RESET,     // reset font size
    };

    struct InputResult
    {
        InputAction action = InputAction::NONE;
        std::string data;  // bytes to send to PTY (when action == SEND_TO_PTY)
    };

    class InputHandler
    {
    public:
        /// Translate an SDL keyboard event. Returns an InputResult describing
        /// what should happen. `has_selection` tells us if text is selected
        /// (so Ctrl+C can mean copy instead of SIGINT).
        static InputResult translate(const SDL_Event &event, bool has_selection);

        /// Translate an SDL_TextInput event (for regular character input).
        static std::string translate_text(const SDL_Event &event);
    };

} // namespace xterm
