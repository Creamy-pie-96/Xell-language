// =============================================================================
// screen_buffer.cpp — 2D cell grid with scrollback buffer
// =============================================================================

#include "screen_buffer.hpp"
#include <algorithm>

namespace xterm
{

    // -----------------------------------------------------------------------------
    // Construction
    // -----------------------------------------------------------------------------

    ScreenBuffer::ScreenBuffer(int rows, int cols)
        : rows_(rows), cols_(cols)
    {
        grid_.resize(rows_);
        for (auto &row : grid_)
            row = make_empty_row();
    }

    // -----------------------------------------------------------------------------
    // Grid operations
    // -----------------------------------------------------------------------------

    void ScreenBuffer::resize(int new_rows, int new_cols)
    {
        if (new_rows == rows_ && new_cols == cols_)
            return;

        // Build a new grid
        std::vector<std::vector<Cell>> new_grid(new_rows);
        for (int r = 0; r < new_rows; ++r)
        {
            new_grid[r].resize(new_cols);
            for (int c = 0; c < new_cols; ++c)
            {
                if (r < rows_ && c < cols_)
                    new_grid[r][c] = grid_[r][c];
                // else: default Cell (space)
            }
        }

        grid_ = std::move(new_grid);
        rows_ = new_rows;
        cols_ = new_cols;
        clamp_cursor();
        mark_all_dirty();
    }

    void ScreenBuffer::set_cell(int row, int col, const Cell &cell)
    {
        if (row >= 0 && row < rows_ && col >= 0 && col < cols_)
        {
            grid_[row][col] = cell;
            grid_[row][col].dirty = true;
        }
    }

    Cell ScreenBuffer::get_cell(int row, int col) const
    {
        if (row >= 0 && row < rows_ && col >= 0 && col < cols_)
            return grid_[row][col];
        return Cell{};
    }

    void ScreenBuffer::clear()
    {
        for (auto &row : grid_)
            row = make_empty_row();
        cursor_row = 0;
        cursor_col = 0;
    }

    void ScreenBuffer::clear_row(int row)
    {
        if (row >= 0 && row < rows_)
            grid_[row] = make_empty_row();
    }

    void ScreenBuffer::scroll_up(int lines)
    {
        for (int i = 0; i < lines; ++i)
        {
            // Push top row into scrollback
            scrollback_.push_back(std::move(grid_[0]));
            if ((int)scrollback_.size() > MAX_SCROLLBACK)
                scrollback_.pop_front();

            // Shift everything up
            for (int r = 0; r < rows_ - 1; ++r)
                grid_[r] = std::move(grid_[r + 1]);

            // New empty row at bottom
            grid_[rows_ - 1] = make_empty_row();
        }
        mark_all_dirty();
    }

    void ScreenBuffer::scroll_down(int lines)
    {
        for (int i = 0; i < lines; ++i)
        {
            // Shift everything down
            for (int r = rows_ - 1; r > 0; --r)
                grid_[r] = std::move(grid_[r - 1]);

            // New empty row at top
            grid_[0] = make_empty_row();
        }
        mark_all_dirty();
    }

    // -----------------------------------------------------------------------------
    // Erase operations
    // -----------------------------------------------------------------------------

    void ScreenBuffer::erase_display(int mode)
    {
        switch (mode)
        {
        case 0: // Erase below (cursor to end)
            for (int c = cursor_col; c < cols_; ++c)
                grid_[cursor_row][c].reset();
            for (int r = cursor_row + 1; r < rows_; ++r)
                grid_[r] = make_empty_row();
            break;
        case 1: // Erase above (start to cursor)
            for (int r = 0; r < cursor_row; ++r)
                grid_[r] = make_empty_row();
            for (int c = 0; c <= cursor_col; ++c)
                grid_[cursor_row][c].reset();
            break;
        case 2: // Erase entire screen
            for (auto &row : grid_)
                row = make_empty_row();
            break;
        case 3: // Erase screen + scrollback
            for (auto &row : grid_)
                row = make_empty_row();
            scrollback_.clear();
            break;
        }
        mark_all_dirty();
    }

    void ScreenBuffer::erase_line(int mode)
    {
        if (cursor_row < 0 || cursor_row >= rows_)
            return;
        auto &row = grid_[cursor_row];
        switch (mode)
        {
        case 0: // Erase to end of line
            for (int c = cursor_col; c < cols_; ++c)
                row[c].reset();
            break;
        case 1: // Erase to beginning of line
            for (int c = 0; c <= cursor_col; ++c)
                row[c].reset();
            break;
        case 2: // Erase entire line
            for (auto &cell : row)
                cell.reset();
            break;
        }
    }

    // -----------------------------------------------------------------------------
    // Cursor movement helpers
    // -----------------------------------------------------------------------------

    void ScreenBuffer::move_cursor(int row, int col)
    {
        cursor_row = row;
        cursor_col = col;
        clamp_cursor();
    }

    void ScreenBuffer::advance_cursor()
    {
        cursor_col++;
        if (cursor_col >= cols_)
        {
            cursor_col = 0;
            cursor_row++;
            if (cursor_row >= rows_)
            {
                scroll_up(1);
                cursor_row = rows_ - 1;
            }
        }
    }

    void ScreenBuffer::newline()
    {
        cursor_row++;
        if (cursor_row >= rows_)
        {
            scroll_up(1);
            cursor_row = rows_ - 1;
        }
    }

    void ScreenBuffer::carriage_return()
    {
        cursor_col = 0;
    }

    void ScreenBuffer::backspace()
    {
        if (cursor_col > 0)
            cursor_col--;
    }

    void ScreenBuffer::tab()
    {
        // Move to next tab stop (every 8 columns)
        int next_tab = ((cursor_col / 8) + 1) * 8;
        cursor_col = std::min(next_tab, cols_ - 1);
    }

    void ScreenBuffer::reverse_index()
    {
        if (cursor_row == 0)
        {
            scroll_down(1);
        }
        else
        {
            cursor_row--;
        }
    }

    // -----------------------------------------------------------------------------
    // Put a character at cursor position and advance
    // -----------------------------------------------------------------------------

    void ScreenBuffer::put_char(char32_t ch, const Cell &style)
    {
        if (cursor_row >= 0 && cursor_row < rows_ &&
            cursor_col >= 0 && cursor_col < cols_)
        {
            auto &cell = grid_[cursor_row][cursor_col];
            cell.ch = ch;
            cell.fg = style.fg;
            cell.bg = style.bg;
            cell.bold = style.bold;
            cell.italic = style.italic;
            cell.underline = style.underline;
            cell.dirty = true;
        }
        advance_cursor();
    }

    // -----------------------------------------------------------------------------
    // Scrollback
    // -----------------------------------------------------------------------------

    int ScreenBuffer::scrollback_size() const
    {
        return static_cast<int>(scrollback_.size());
    }

    const std::vector<Cell> &ScreenBuffer::scrollback_line(int index) const
    {
        return scrollback_[index];
    }

    // -----------------------------------------------------------------------------
    // Mark all cells dirty
    // -----------------------------------------------------------------------------

    void ScreenBuffer::mark_all_dirty()
    {
        for (auto &row : grid_)
            for (auto &cell : row)
                cell.dirty = true;
    }

    // -----------------------------------------------------------------------------
    // Internal helpers
    // -----------------------------------------------------------------------------

    // Encode a single char32_t codepoint as UTF-8 and append to string
    static void append_utf8(std::string &out, char32_t cp)
    {
        if (cp < 0x80)
        {
            out += static_cast<char>(cp);
        }
        else if (cp < 0x800)
        {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
        else if (cp < 0x10000)
        {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
        else if (cp < 0x110000)
        {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    // Extract a row of cells as UTF-8 text, trimming trailing spaces
    static std::string row_to_string(const std::vector<Cell> &cells,
                                     int col_start, int col_end)
    {
        // Clamp
        if (col_start < 0)
            col_start = 0;
        if (col_end >= static_cast<int>(cells.size()))
            col_end = static_cast<int>(cells.size()) - 1;

        std::string line;
        for (int c = col_start; c <= col_end; ++c)
            append_utf8(line, cells[c].ch);

        // Trim trailing spaces
        while (!line.empty() && line.back() == ' ')
            line.pop_back();

        return line;
    }

    std::string ScreenBuffer::extract_text(const TextSelection &sel,
                                           int scroll_offset) const
    {
        if (!sel.has_selection)
            return {};

        SelectionPoint lo, hi;
        sel.get_ordered(lo, hi);

        std::string result;

        for (int vrow = lo.row; vrow <= hi.row; ++vrow)
        {
            int c_start = (vrow == lo.row) ? lo.col : 0;
            int c_end = (vrow == hi.row) ? hi.col : (cols_ - 1);

            // vrow is the visual row:
            //   vrow 0 = top visible line (might be a scrollback line)
            //   The actual line index into scrollback or grid depends on scroll_offset.
            int sb_size = static_cast<int>(scrollback_.size());
            int abs_line = vrow - scroll_offset; // negative = scrollback

            std::string line;
            if (abs_line < 0)
            {
                // It's in the scrollback
                int sb_idx = sb_size + abs_line; // count from end
                if (sb_idx >= 0 && sb_idx < sb_size)
                    line = row_to_string(scrollback_[sb_idx], c_start, c_end);
            }
            else if (abs_line < rows_)
            {
                // It's in the live grid
                line = row_to_string(grid_[abs_line], c_start, c_end);
            }

            if (vrow > lo.row && !result.empty())
                result += '\n';
            result += line;
        }

        return result;
    }

    void ScreenBuffer::clamp_cursor()
    {
        cursor_row = std::clamp(cursor_row, 0, std::max(0, rows_ - 1));
        cursor_col = std::clamp(cursor_col, 0, std::max(0, cols_ - 1));
    }

    std::vector<Cell> ScreenBuffer::make_empty_row() const
    {
        return std::vector<Cell>(cols_);
    }

} // namespace xterm
