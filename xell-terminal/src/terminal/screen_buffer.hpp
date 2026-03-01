#pragma once

// =============================================================================
// screen_buffer.hpp — 2D cell grid with scrollback buffer
// =============================================================================
// Maintains the visible terminal grid plus a scrollback history so the user
// can scroll up to see past output. Thread-safe: callers must hold the mutex
// externally (the main loop coordinates this).
// =============================================================================

#include "types.hpp"
#include <vector>
#include <deque>
#include <string>
#include <algorithm>

namespace xterm
{

    // =========================================================================
    // Text selection (for copy support)
    // =========================================================================
    struct SelectionPoint
    {
        int row = 0; // row in the virtual buffer (negative = scrollback)
        int col = 0;

        bool operator<(const SelectionPoint &o) const
        {
            return (row < o.row) || (row == o.row && col < o.col);
        }
        bool operator==(const SelectionPoint &o) const
        {
            return row == o.row && col == o.col;
        }
        bool operator!=(const SelectionPoint &o) const { return !(*this == o); }
    };

    struct TextSelection
    {
        bool active = false;        // currently dragging
        bool has_selection = false; // a completed or in-progress selection exists
        SelectionPoint start;
        SelectionPoint end;

        /// Return ordered (top-left, bottom-right) range
        void get_ordered(SelectionPoint &lo, SelectionPoint &hi) const
        {
            if (start < end || start == end)
            {
                lo = start;
                hi = end;
            }
            else
            {
                lo = end;
                hi = start;
            }
        }

        /// Is the cell (row,col) inside the selection?
        bool contains(int row, int col) const
        {
            if (!has_selection)
                return false;
            SelectionPoint lo, hi;
            get_ordered(lo, hi);
            if (row < lo.row || row > hi.row)
                return false;
            if (row == lo.row && row == hi.row)
                return col >= lo.col && col <= hi.col;
            if (row == lo.row)
                return col >= lo.col;
            if (row == hi.row)
                return col <= hi.col;
            return true; // middle rows are fully selected
        }

        void clear()
        {
            active = false;
            has_selection = false;
        }
    };

    class ScreenBuffer
    {
    public:
        ScreenBuffer(int rows, int cols);

        // --- Grid operations ---
        void resize(int new_rows, int new_cols);
        void set_cell(int row, int col, const Cell &cell);
        Cell get_cell(int row, int col) const;
        void clear();
        void clear_row(int row);
        void scroll_up(int lines = 1);
        void scroll_down(int lines = 1);

        // --- Erase operations (used by VT parser) ---
        void erase_display(int mode); // 0=below, 1=above, 2=all, 3=all+scrollback
        void erase_line(int mode);    // 0=to end, 1=to start, 2=whole line

        // --- Cursor helpers ---
        void move_cursor(int row, int col);
        void advance_cursor();
        void newline();
        void carriage_return();
        void backspace();
        void tab();
        void reverse_index(); // scroll down if at top

        // --- Insert a character at cursor position ---
        void put_char(char32_t ch, const Cell &style);

        // --- Scrollback ---
        int scrollback_size() const;
        const std::vector<Cell> &scrollback_line(int index) const;

        // --- Text extraction (for copy) ---
        /// Extract text from the selection range. scroll_offset is how many
        /// scrollback lines the view is shifted by.
        std::string extract_text(const TextSelection &sel, int scroll_offset) const;

        // --- Mark all cells as dirty (force full redraw) ---
        void mark_all_dirty();

        // --- Accessors ---
        int get_rows() const { return rows_; }
        int get_cols() const { return cols_; }

        // Cursor state (public for direct access by VT parser)
        int cursor_row = 0;
        int cursor_col = 0;
        bool cursor_visible = true;

        // Saved cursor position (for ESC[s / ESC[u)
        int saved_cursor_row = 0;
        int saved_cursor_col = 0;

    private:
        int rows_;
        int cols_;
        std::vector<std::vector<Cell>> grid_;

        // Scrollback buffer — stores lines that scrolled off the top
        std::deque<std::vector<Cell>> scrollback_;
        static constexpr int MAX_SCROLLBACK = 5000;

        void clamp_cursor();
        std::vector<Cell> make_empty_row() const;
    };

} // namespace xterm
