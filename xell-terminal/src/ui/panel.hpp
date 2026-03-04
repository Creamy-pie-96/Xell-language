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

        // Helper: write a string into a row
        void writeString(std::vector<Cell> &row, int col, const std::string &text,
                         Color fg, Color bg, bool bold = false) const
        {
            for (int i = 0; i < (int)text.size() && col + i < (int)row.size(); i++)
            {
                if (col + i >= 0)
                {
                    row[col + i].ch = (char32_t)text[i];
                    row[col + i].fg = fg;
                    row[col + i].bg = bg;
                    row[col + i].bold = bold;
                    row[col + i].dirty = true;
                }
            }
        }
    };

} // namespace xterm
