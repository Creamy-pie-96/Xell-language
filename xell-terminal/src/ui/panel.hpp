#pragma once

// =============================================================================
// panel.hpp — Base panel abstraction for the Xell Terminal IDE
// =============================================================================
// Every visual region (editor, terminal, file tree, REPL, diagnostics)
// implements this interface. The layout manager arranges panels into a
// tree of horizontal/vertical splits.
// =============================================================================

#include <string>
#include <vector>
#include <functional>
#include "../terminal/types.hpp"
#include "../editor/editor_view.hpp" // Rect

namespace xterm
{

    // ─── Panel types ─────────────────────────────────────────────────────

    enum class PanelType
    {
        Editor,
        Terminal,
        FileExplorer,
        REPL,
        Output,
        Diagnostics,
        Git,
        Variables,
    };

    // ─── Panel (abstract base) ───────────────────────────────────────────

    class Panel
    {
    public:
        virtual ~Panel() = default;

        // Identity
        virtual PanelType type() const = 0;
        virtual std::string title() const = 0;

        // Layout
        void setRect(Rect r)
        {
            rect_ = r;
            onResize();
        }
        Rect rect() const { return rect_; }

        // Focus
        bool focused() const { return focused_; }
        void setFocused(bool f) { focused_ = f; }

        // Visibility
        bool visible() const { return visible_; }
        void setVisible(bool v) { visible_ = v; }

        // Render: produce cells for this panel's rect
        virtual std::vector<std::vector<Cell>> render() const = 0;

        // Event handling (returns true if consumed)
        virtual bool handleKeyDown(const SDL_Event &) { return false; }
        virtual bool handleTextInput(const SDL_Event &) { return false; }
        virtual bool handleMouseClick(int /*row*/, int /*col*/, bool /*shift*/) { return false; }
        virtual bool handleMouseDrag(int /*row*/, int /*col*/) { return false; }
        virtual bool handleMouseWheel(int /*delta*/) { return false; }

    protected:
        Rect rect_ = {0, 0, 80, 24};
        bool focused_ = false;
        bool visible_ = true;

        virtual void onResize() {} // override for custom resize logic

        // Helper: create a filled cell grid
        std::vector<std::vector<Cell>> makeGrid(Color bg, Color fg = Color::default_fg()) const
        {
            std::vector<std::vector<Cell>> grid(rect_.h);
            for (auto &row : grid)
            {
                row.resize(rect_.w);
                for (auto &c : row)
                {
                    c.ch = U' ';
                    c.bg = bg;
                    c.fg = fg;
                    c.dirty = true;
                }
            }
            return grid;
        }

        // Helper: decode one UTF-8 codepoint from a string, advancing index
        static char32_t decodeUTF8(const std::string &s, size_t &i)
        {
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

        // Helper: write a UTF-8 string into a row (properly decoding codepoints)
        // Returns the next column position after writing
        int writeString(std::vector<Cell> &row, int col, const std::string &text,
                        Color fg, Color bg, bool bold = false) const
        {
            size_t i = 0;
            int c = col;
            while (i < text.size() && c < (int)row.size())
            {
                char32_t cp = decodeUTF8(text, i);
                if (c >= 0)
                {
                    row[c].ch = cp;
                    row[c].fg = fg;
                    row[c].bg = bg;
                    row[c].bold = bold;
                    row[c].dirty = true;
                }
                c++;
            }
            return c;
        }

        // Helper: count the number of Unicode codepoints in a UTF-8 string
        static int utf8Len(const std::string &s)
        {
            int len = 0;
            size_t i = 0;
            while (i < s.size())
            {
                decodeUTF8(s, i);
                len++;
            }
            return len;
        }
    };

} // namespace xterm
