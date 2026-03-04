#pragma once

// =============================================================================
// types.hpp — Core data types for Xell Terminal
// =============================================================================
// Defines Color and Cell, the fundamental building blocks of the terminal grid.
// =============================================================================

#include <cstdint>
#include <cstddef> // size_t
#include <string>  // std::string
#include <vector>  // std::vector

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

    // ─── UTF-8 utilities ─────────────────────────────────────────────────

    // Decode one UTF-8 codepoint from a string, advancing index past it.
    // Returns U+FFFD on malformed input.
    inline char32_t utf8Decode(const std::string &s, size_t &i)
    {
        if (i >= s.size())
            return 0;
        unsigned char c = static_cast<unsigned char>(s[i]);
        char32_t cp;
        int extra;
        if (c < 0x80)
        {
            cp = c;
            extra = 0;
        }
        else if (c < 0xC0)
        {
            i++;
            return 0xFFFD;
        }
        else if (c < 0xE0)
        {
            cp = c & 0x1F;
            extra = 1;
        }
        else if (c < 0xF0)
        {
            cp = c & 0x0F;
            extra = 2;
        }
        else
        {
            cp = c & 0x07;
            extra = 3;
        }
        for (int j = 0; j < extra && i + 1 < s.size(); j++)
        {
            i++;
            cp = (cp << 6) | (static_cast<unsigned char>(s[i]) & 0x3F);
        }
        i++;
        return cp;
    }

    // Count the number of Unicode codepoints in a UTF-8 string
    inline int utf8Len(const std::string &s)
    {
        int len = 0;
        size_t i = 0;
        while (i < s.size())
        {
            utf8Decode(s, i);
            len++;
        }
        return len;
    }

    // Write a UTF-8 string into a cell row, properly decoding codepoints
    inline int utf8Write(std::vector<Cell> &row, int startCol, const std::string &text,
                         Color fg, Color bg, bool bold = false)
    {
        size_t si = 0;
        int col = startCol;
        while (si < text.size() && col < (int)row.size())
        {
            char32_t cp = utf8Decode(text, si);
            if (col >= 0)
            {
                row[col].ch = cp;
                row[col].fg = fg;
                row[col].bg = bg;
                row[col].bold = bold;
                row[col].dirty = true;
            }
            col++;
        }
        return col; // return next column after writing
    }

} // namespace xterm
