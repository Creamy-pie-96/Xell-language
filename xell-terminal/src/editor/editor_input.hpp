#pragma once

// =============================================================================
// editor_input.hpp — Keyboard handler for the Xell Editor View
// =============================================================================
// Translates SDL keyboard events into EditorView commands.
// Works alongside the terminal's InputHandler — when the editor mode is
// active, key events are routed here instead of to the PTY.
// =============================================================================

#include <SDL2/SDL.h>
#include <string>
#include <functional>
#include "editor_view.hpp"

namespace xterm
{

    // ─── Command types the editor input handler can produce ──────────────

    enum class EditorAction
    {
        NONE,          // not handled — pass to default handler
        HANDLED,       // handled, no further action
        SAVE,          // Ctrl+S
        SAVE_AS,       // Ctrl+Shift+S
        OPEN_FILE,     // Ctrl+O
        CLOSE_TAB,     // Ctrl+W
        FIND,          // Ctrl+F
        FIND_REPLACE,  // Ctrl+H
        GOTO_LINE,     // Ctrl+G
        COMMAND_PALETTE, // Ctrl+Shift+P
        TOGGLE_TERMINAL, // Ctrl+`
        NEW_FILE,      // Ctrl+N
        QUIT_EDITOR,   // Ctrl+Q — switch back to terminal mode
        COPY,          // Ctrl+C (no selection → line copy)
        CUT,           // Ctrl+X
        PASTE,         // Ctrl+V
    };

    // ─── Editor Input Handler ────────────────────────────────────────────

    class EditorInput
    {
    public:
        explicit EditorInput(EditorView &view) : view_(view) {}

        // Handle an SDL_KEYDOWN event. Returns the action.
        EditorAction handleKeyDown(const SDL_Event &event)
        {
            const auto &key = event.key.keysym;
            bool ctrl = (key.mod & KMOD_CTRL) != 0;
            bool shift = (key.mod & KMOD_SHIFT) != 0;
            bool alt = (key.mod & KMOD_ALT) != 0;
            (void)alt; // reserved for future

            // ── Ctrl+key combos ──────────────────────────────────────
            if (ctrl)
            {
                switch (key.sym)
                {
                case SDLK_s:
                    return shift ? EditorAction::SAVE_AS : EditorAction::SAVE;
                case SDLK_o:
                    return EditorAction::OPEN_FILE;
                case SDLK_w:
                    return EditorAction::CLOSE_TAB;
                case SDLK_n:
                    return EditorAction::NEW_FILE;
                case SDLK_q:
                    return EditorAction::QUIT_EDITOR;
                case SDLK_f:
                    return EditorAction::FIND;
                case SDLK_h:
                    return EditorAction::FIND_REPLACE;
                case SDLK_g:
                    return EditorAction::GOTO_LINE;
                case SDLK_p:
                    if (shift) return EditorAction::COMMAND_PALETTE;
                    break;
                case SDLK_BACKQUOTE:
                    return EditorAction::TOGGLE_TERMINAL;

                case SDLK_c:
                    return EditorAction::COPY;
                case SDLK_x:
                    return EditorAction::CUT;
                case SDLK_v:
                    return EditorAction::PASTE;

                case SDLK_z:
                    if (shift)
                        view_.redo();
                    else
                        view_.undo();
                    return EditorAction::HANDLED;

                case SDLK_y:
                    view_.redo();
                    return EditorAction::HANDLED;

                case SDLK_a:
                    view_.selectAll();
                    return EditorAction::HANDLED;

                case SDLK_d:
                    view_.duplicateLine();
                    return EditorAction::HANDLED;

                case SDLK_l:
                    view_.deleteLine();
                    return EditorAction::HANDLED;

                case SDLK_LEFT:
                    view_.moveCursorWordLeft(shift);
                    return EditorAction::HANDLED;

                case SDLK_RIGHT:
                    view_.moveCursorWordRight(shift);
                    return EditorAction::HANDLED;

                case SDLK_UP:
                    // Ctrl+Up: scroll up without moving cursor
                    view_.scrollBy(-1);
                    return EditorAction::HANDLED;

                case SDLK_DOWN:
                    // Ctrl+Down: scroll down without moving cursor
                    view_.scrollBy(1);
                    return EditorAction::HANDLED;

                case SDLK_HOME:
                    view_.moveCursorToStart(shift);
                    return EditorAction::HANDLED;

                case SDLK_END:
                    view_.moveCursorToEnd(shift);
                    return EditorAction::HANDLED;

                case SDLK_RIGHTBRACKET:
                    view_.indent();
                    return EditorAction::HANDLED;

                case SDLK_LEFTBRACKET:
                    view_.outdent();
                    return EditorAction::HANDLED;

                default:
                    break;
                }
                return EditorAction::NONE; // unhandled ctrl combo
            }

            // ── Plain / Shift+key ────────────────────────────────────
            switch (key.sym)
            {
            case SDLK_UP:
                view_.moveCursorUp(shift);
                return EditorAction::HANDLED;

            case SDLK_DOWN:
                view_.moveCursorDown(shift);
                return EditorAction::HANDLED;

            case SDLK_LEFT:
                view_.moveCursorLeft(shift);
                return EditorAction::HANDLED;

            case SDLK_RIGHT:
                view_.moveCursorRight(shift);
                return EditorAction::HANDLED;

            case SDLK_HOME:
                view_.moveCursorHome(shift);
                return EditorAction::HANDLED;

            case SDLK_END:
                view_.moveCursorEnd(shift);
                return EditorAction::HANDLED;

            case SDLK_PAGEUP:
                view_.pageUp(shift);
                return EditorAction::HANDLED;

            case SDLK_PAGEDOWN:
                view_.pageDown(shift);
                return EditorAction::HANDLED;

            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                view_.insertNewline();
                return EditorAction::HANDLED;

            case SDLK_BACKSPACE:
                view_.backspace();
                return EditorAction::HANDLED;

            case SDLK_DELETE:
                view_.deleteForward();
                return EditorAction::HANDLED;

            case SDLK_TAB:
                if (shift)
                    view_.outdent();
                else
                    view_.indent();
                return EditorAction::HANDLED;

            case SDLK_ESCAPE:
                // Clear selection, or if none, could trigger other action
                if (view_.selection().active)
                {
                    view_.clearSelection();
                    return EditorAction::HANDLED;
                }
                return EditorAction::NONE; // let parent decide

            default:
                break;
            }

            return EditorAction::NONE;
        }

        // Handle an SDL_TEXTINPUT event (regular character typing)
        EditorAction handleTextInput(const SDL_Event &event)
        {
            std::string text = event.text.text;
            if (!text.empty())
            {
                // Insert each character (handles multi-byte UTF-8 as text)
                view_.insertText(text);
                return EditorAction::HANDLED;
            }
            return EditorAction::NONE;
        }

        // Handle mouse scroll in editor area
        EditorAction handleMouseWheel(int scrollY)
        {
            view_.scrollBy(-scrollY * scrollSpeed_);
            return EditorAction::HANDLED;
        }

        // Handle mouse click in editor area (screenRow/screenCol relative to editor rect)
        EditorAction handleMouseClick(int screenRow, int screenCol, bool shift)
        {
            auto pos = view_.screenToBuffer(screenRow, screenCol);
            if (shift)
            {
                // Extend selection
                if (!view_.selection().active)
                {
                    view_.selection().active = true;
                    view_.selection().anchor = view_.cursor();
                }
                view_.setCursor(pos);
                view_.selection().cursor = pos;
            }
            else
            {
                view_.clearSelection();
                view_.setCursor(pos);
            }
            return EditorAction::HANDLED;
        }

        // Handle mouse drag (extending selection)
        EditorAction handleMouseDrag(int screenRow, int screenCol)
        {
            auto pos = view_.screenToBuffer(screenRow, screenCol);
            if (!view_.selection().active)
            {
                view_.selection().active = true;
                view_.selection().anchor = view_.cursor();
            }
            view_.setCursor(pos);
            view_.selection().cursor = pos;
            return EditorAction::HANDLED;
        }

        // Settings
        void setScrollSpeed(int speed) { scrollSpeed_ = speed; }

    private:
        EditorView &view_;
        int scrollSpeed_ = 3;
    };

} // namespace xterm
