#pragma once

// =============================================================================
// types.hpp — Core data types for Xell Terminal
// =============================================================================
// Defines Color and Cell, the fundamental building blocks of the terminal grid.
// =============================================================================

#include <cstdint>

namespace xterm
{

    // -----------------------------------------------------------------------------
    // Color — RGBA color used for foreground/background of each cell
    // -----------------------------------------------------------------------------
    struct Color
    {
        uint8_t r, g, b, a;

        constexpr Color() : r(0), g(0), b(0), a(255) {}
        constexpr Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
            : r(r), g(g), b(b), a(a) {}

        bool operator==(const Color &o) const
        {
            return r == o.r && g == o.g && b == o.b && a == o.a;
        }
        bool operator!=(const Color &o) const { return !(*this == o); }

        // Predefined colors
        static constexpr Color white() { return {255, 255, 255}; }
        static constexpr Color black() { return {0, 0, 0}; }
        static constexpr Color default_fg() { return {204, 204, 204}; } // #CCCCCC
        static constexpr Color default_bg() { return {18, 18, 18}; }    // #121212
        static constexpr Color cursor_color() { return {255, 255, 255}; }

        // Standard ANSI 8-color palette (normal)
        static constexpr Color ansi(int index)
        {
            switch (index)
            {
            case 0:
                return {0, 0, 0}; // Black
            case 1:
                return {205, 49, 49}; // Red
            case 2:
                return {13, 188, 121}; // Green
            case 3:
                return {229, 229, 16}; // Yellow
            case 4:
                return {36, 114, 200}; // Blue
            case 5:
                return {188, 63, 188}; // Magenta
            case 6:
                return {17, 168, 205}; // Cyan
            case 7:
                return {204, 204, 204}; // White
            default:
                return default_fg();
            }
        }

        // Bright ANSI colors
        static constexpr Color ansi_bright(int index)
        {
            switch (index)
            {
            case 0:
                return {102, 102, 102}; // Bright Black (Gray)
            case 1:
                return {241, 76, 76}; // Bright Red
            case 2:
                return {35, 209, 139}; // Bright Green
            case 3:
                return {245, 245, 67}; // Bright Yellow
            case 4:
                return {59, 142, 234}; // Bright Blue
            case 5:
                return {214, 112, 214}; // Bright Magenta
            case 6:
                return {41, 184, 219}; // Bright Cyan
            case 7:
                return {242, 242, 242}; // Bright White
            default:
                return white();
            }
        }
    };

    // -----------------------------------------------------------------------------
    // Cell — the atom of the terminal display (one character position)
    // -----------------------------------------------------------------------------
    struct Cell
    {
        char32_t ch = U' ';
        Color fg = Color::default_fg();
        Color bg = Color::default_bg();
        bool bold = false;
        bool italic = false;
        bool underline = false;
        bool dirty = true; // needs redraw this frame

        void reset()
        {
            ch = U' ';
            fg = Color::default_fg();
            bg = Color::default_bg();
            bold = false;
            italic = false;
            underline = false;
            dirty = true;
        }
    };

} // namespace xterm
