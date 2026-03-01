// =============================================================================
// renderer.cpp — SDL2 + SDL_ttf terminal renderer
// =============================================================================
// Draws the cell grid onto an SDL2 window. Uses a glyph texture cache so
// repeated characters (e.g., spaces, common letters) aren't re-rendered from
// the font every frame.
// =============================================================================

#include "renderer.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <algorithm>
#include <cstdio>
#include <string>

namespace xterm
{

    // =============================================================================
    // Lifecycle
    // =============================================================================

    Renderer::~Renderer()
    {
        shutdown();
    }

    bool Renderer::init(int window_width, int window_height,
                        const std::string &font_path, int font_size)
    {
        font_size_ = font_size;

        // Init SDL2
        if (SDL_Init(SDL_INIT_VIDEO) != 0)
        {
            std::fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
            return false;
        }

        // Init SDL_ttf
        if (TTF_Init() != 0)
        {
            std::fprintf(stderr, "TTF_Init error: %s\n", TTF_GetError());
            SDL_Quit();
            return false;
        }

        // Create window
        window_ = SDL_CreateWindow(
            "Xell Terminal",
            SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
            window_width, window_height,
            SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
        if (!window_)
        {
            std::fprintf(stderr, "SDL_CreateWindow error: %s\n", SDL_GetError());
            TTF_Quit();
            SDL_Quit();
            return false;
        }

        // Create renderer (hardware-accelerated with vsync)
        renderer_ = SDL_CreateRenderer(window_, -1,
                                       SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (!renderer_)
        {
            // Fallback to software renderer
            renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
            if (!renderer_)
            {
                std::fprintf(stderr, "SDL_CreateRenderer error: %s\n", SDL_GetError());
                SDL_DestroyWindow(window_);
                TTF_Quit();
                SDL_Quit();
                return false;
            }
        }

        // Enable blending for text rendering
        SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);

        // Load font
        font_ = TTF_OpenFont(font_path.c_str(), font_size);
        if (!font_)
        {
            std::fprintf(stderr, "TTF_OpenFont error: %s\n", TTF_GetError());
            SDL_DestroyRenderer(renderer_);
            SDL_DestroyWindow(window_);
            TTF_Quit();
            SDL_Quit();
            return false;
        }

        // We use the same font for bold too (just render with TTF_STYLE_BOLD).
        // If you have a separate bold font, load it here.
        font_bold_ = font_;

        // Determine cell size from font metrics.
        // We render a reference character 'M' to get the advance width.
        int advance;
        if (TTF_GlyphMetrics(font_, 'M', nullptr, nullptr, nullptr, nullptr, &advance) == 0)
        {
            cell_w_ = advance;
        }
        else
        {
            cell_w_ = font_size * 6 / 10; // rough fallback
        }
        cell_h_ = TTF_FontLineSkip(font_);

        // Ensure minimum cell dimensions
        if (cell_w_ < 1)
            cell_w_ = 8;
        if (cell_h_ < 1)
            cell_h_ = 16;

        return true;
    }

    void Renderer::shutdown()
    {
        clear_glyph_cache();

        if (font_)
        {
            TTF_CloseFont(font_);
            font_ = nullptr;
            font_bold_ = nullptr;
        }

        if (renderer_)
        {
            SDL_DestroyRenderer(renderer_);
            renderer_ = nullptr;
        }

        if (window_)
        {
            SDL_DestroyWindow(window_);
            window_ = nullptr;
        }

        TTF_Quit();
        SDL_Quit();
    }

    // =============================================================================
    // Render the screen buffer
    // =============================================================================

    void Renderer::render(ScreenBuffer &buffer, bool cursor_visible, int scroll_offset,
                          const TextSelection *selection)
    {
        // Clear the entire window to the default background
        SDL_SetRenderDrawColor(renderer_,
                               Color::default_bg().r, Color::default_bg().g,
                               Color::default_bg().b, 255);
        SDL_RenderClear(renderer_);

        int rows = buffer.get_rows();
        int cols = buffer.get_cols();

        if (scroll_offset > 0)
        {
            // We're scrolled back into the scrollback buffer.
            // Draw scrollback lines for the top portion, and live lines for the rest.
            int sb_size = buffer.scrollback_size();
            int sb_start = sb_size - scroll_offset; // first scrollback line to show
            if (sb_start < 0)
                sb_start = 0;

            int screen_row = 0;

            // Draw scrollback lines
            for (int sb_idx = sb_start; sb_idx < sb_size && screen_row < rows; ++sb_idx, ++screen_row)
            {
                draw_scrollback_line(screen_row, buffer.scrollback_line(sb_idx));
            }

            // Draw live grid lines for any remaining rows
            int live_start = (scroll_offset > sb_size) ? 0 : (scroll_offset - (sb_size - sb_start));
            // Actually, simpler approach: just show the top (rows - remaining) lines from live
            int live_row = 0;
            for (; screen_row < rows && live_row < rows; ++screen_row, ++live_row)
            {
                for (int c = 0; c < cols; ++c)
                {
                    draw_cell(screen_row, c, buffer.get_cell(live_row, c));
                }
            }
        }
        else
        {
            // Normal view — draw the live grid
            for (int r = 0; r < rows; ++r)
            {
                for (int c = 0; c < cols; ++c)
                {
                    Cell cell = buffer.get_cell(r, c);
                    if (cell.dirty)
                    {
                        draw_cell(r, c, cell);
                    }
                }
            }
        }

        // Draw cursor
        if (cursor_visible && buffer.cursor_visible && scroll_offset == 0)
        {
            Cell cell_under = buffer.get_cell(buffer.cursor_row, buffer.cursor_col);
            draw_cursor(buffer.cursor_row, buffer.cursor_col, cell_under);
        }

        // Draw selection overlay
        if (selection && selection->has_selection)
        {
            SelectionPoint lo, hi;
            selection->get_ordered(lo, hi);

            for (int vrow = lo.row; vrow <= hi.row && vrow < rows; ++vrow)
            {
                if (vrow < 0)
                    continue;
                int c_start = (vrow == lo.row) ? lo.col : 0;
                int c_end = (vrow == hi.row) ? hi.col : (cols - 1);
                if (c_start < 0)
                    c_start = 0;
                if (c_end >= cols)
                    c_end = cols - 1;

                SDL_Rect sel_rect = {
                    c_start * cell_w_,
                    vrow * cell_h_,
                    (c_end - c_start + 1) * cell_w_,
                    cell_h_};
                SDL_SetRenderDrawColor(renderer_, 80, 130, 200, 100); // blue tint
                SDL_RenderFillRect(renderer_, &sel_rect);
            }
        }

        // Mark all cells as clean after rendering
        // (We do a full pass because we want to track dirty state properly)
        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c)
            {
                Cell cell = buffer.get_cell(r, c);
                cell.dirty = false;
                buffer.set_cell(r, c, cell);
            }
    }

    void Renderer::present()
    {
        SDL_RenderPresent(renderer_);
    }

    // =============================================================================
    // Cell size queries
    // =============================================================================

    void Renderer::get_cell_size(int &width, int &height) const
    {
        width = cell_w_;
        height = cell_h_;
    }

    void Renderer::get_terminal_size(int window_w, int window_h, int &rows, int &cols) const
    {
        cols = (cell_w_ > 0) ? (window_w / cell_w_) : 80;
        rows = (cell_h_ > 0) ? (window_h / cell_h_) : 24;
        if (cols < 1)
            cols = 1;
        if (rows < 1)
            rows = 1;
    }

    // =============================================================================
    // Draw a single cell
    // =============================================================================

    void Renderer::draw_cell(int row, int col, const Cell &cell)
    {
        int x = col * cell_w_;
        int y = row * cell_h_;

        // Draw background rectangle
        SDL_Rect bg_rect = {x, y, cell_w_, cell_h_};
        SDL_SetRenderDrawColor(renderer_, cell.bg.r, cell.bg.g, cell.bg.b, cell.bg.a);
        SDL_RenderFillRect(renderer_, &bg_rect);

        // Draw character (skip spaces for performance)
        if (cell.ch != U' ' && cell.ch != 0)
        {
            SDL_Texture *tex = get_glyph_texture(cell.ch, cell.fg, cell.bold);
            if (tex)
            {
                int tex_w, tex_h;
                SDL_QueryTexture(tex, nullptr, nullptr, &tex_w, &tex_h);

                // Center the glyph in the cell
                SDL_Rect dst = {
                    x + (cell_w_ - tex_w) / 2,
                    y + (cell_h_ - tex_h) / 2,
                    tex_w,
                    tex_h};
                SDL_RenderCopy(renderer_, tex, nullptr, &dst);
            }
        }

        // Draw underline if set
        if (cell.underline)
        {
            int uy = y + cell_h_ - 2;
            SDL_SetRenderDrawColor(renderer_, cell.fg.r, cell.fg.g, cell.fg.b, cell.fg.a);
            SDL_RenderDrawLine(renderer_, x, uy, x + cell_w_ - 1, uy);
        }
    }

    // =============================================================================
    // Draw cursor (block cursor)
    // =============================================================================

    void Renderer::draw_cursor(int row, int col, const Cell &cell)
    {
        int x = col * cell_w_;
        int y = row * cell_h_;

        // Draw filled white block
        SDL_Rect cursor_rect = {x, y, cell_w_, cell_h_};
        SDL_SetRenderDrawColor(renderer_,
                               Color::cursor_color().r, Color::cursor_color().g,
                               Color::cursor_color().b, 200); // slightly transparent
        SDL_RenderFillRect(renderer_, &cursor_rect);

        // Draw the character under the cursor in inverse color (dark on light)
        if (cell.ch != U' ' && cell.ch != 0)
        {
            Color inverse_fg = Color::default_bg(); // dark color on white cursor
            SDL_Texture *tex = get_glyph_texture(cell.ch, inverse_fg, cell.bold);
            if (tex)
            {
                int tex_w, tex_h;
                SDL_QueryTexture(tex, nullptr, nullptr, &tex_w, &tex_h);
                SDL_Rect dst = {
                    x + (cell_w_ - tex_w) / 2,
                    y + (cell_h_ - tex_h) / 2,
                    tex_w, tex_h};
                SDL_RenderCopy(renderer_, tex, nullptr, &dst);
            }
        }
    }

    // =============================================================================
    // Draw a scrollback line
    // =============================================================================

    void Renderer::draw_scrollback_line(int screen_row, const std::vector<Cell> &line)
    {
        for (int c = 0; c < (int)line.size(); ++c)
        {
            draw_cell(screen_row, c, line[c]);
        }
        // Fill remaining columns with blank cells if line is shorter
        // (handled by draw_cell defaulting to empty)
    }

    // =============================================================================
    // Glyph texture cache
    // =============================================================================

    SDL_Texture *Renderer::get_glyph_texture(char32_t ch, Color fg, bool bold)
    {
        GlyphKey key = {ch, fg.r, fg.g, fg.b, bold};

        auto it = glyph_cache_.find(key);
        if (it != glyph_cache_.end())
            return it->second;

        // Render the glyph
        TTF_Font *f = bold ? font_bold_ : font_;
        if (bold && f == font_)
        {
            TTF_SetFontStyle(f, TTF_STYLE_BOLD);
        }

        SDL_Color sdl_color = {fg.r, fg.g, fg.b, fg.a};

        // For basic ASCII, use the fast path with RenderGlyph
        SDL_Surface *surface = nullptr;
        if (ch <= 0xFFFF)
        {
            surface = TTF_RenderGlyph_Blended(f, static_cast<Uint16>(ch), sdl_color);
        }
        else
        {
            // For characters beyond BMP, render as UTF-8 string
            char utf8[5] = {};
            if (ch <= 0x7F)
            {
                utf8[0] = static_cast<char>(ch);
            }
            else if (ch <= 0x7FF)
            {
                utf8[0] = static_cast<char>(0xC0 | (ch >> 6));
                utf8[1] = static_cast<char>(0x80 | (ch & 0x3F));
            }
            else if (ch <= 0xFFFF)
            {
                utf8[0] = static_cast<char>(0xE0 | (ch >> 12));
                utf8[1] = static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
                utf8[2] = static_cast<char>(0x80 | (ch & 0x3F));
            }
            else
            {
                utf8[0] = static_cast<char>(0xF0 | (ch >> 18));
                utf8[1] = static_cast<char>(0x80 | ((ch >> 12) & 0x3F));
                utf8[2] = static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
                utf8[3] = static_cast<char>(0x80 | (ch & 0x3F));
            }
            surface = TTF_RenderUTF8_Blended(f, utf8, sdl_color);
        }

        // Reset font style if we changed it
        if (bold && f == font_)
        {
            TTF_SetFontStyle(f, TTF_STYLE_NORMAL);
        }

        if (!surface)
        {
            glyph_cache_[key] = nullptr;
            return nullptr;
        }

        SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer_, surface);
        SDL_FreeSurface(surface);

        glyph_cache_[key] = texture;

        // Evict old entries if cache gets too large (prevent memory bloat)
        if (glyph_cache_.size() > 8192)
        {
            clear_glyph_cache();
        }

        return texture;
    }

    void Renderer::clear_glyph_cache()
    {
        for (auto &[key, tex] : glyph_cache_)
        {
            if (tex)
                SDL_DestroyTexture(tex);
        }
        glyph_cache_.clear();
    }

    // =============================================================================
    // Draw a right-click context menu
    // =============================================================================

    void Renderer::draw_context_menu(int x, int y, int w, int h,
                                     int item_count,
                                     std::function<const char*(int)> get_label,
                                     int hover_index)
    {
        if (!renderer_ || !font_ || item_count <= 0)
            return;

        // Clamp to window bounds
        int win_w, win_h;
        SDL_GetRendererOutputSize(renderer_, &win_w, &win_h);
        if (x + w > win_w) x = win_w - w;
        if (y + h > win_h) y = win_h - h;
        if (x < 0) x = 0;
        if (y < 0) y = 0;

        // Draw shadow
        SDL_Rect shadow = {x + 3, y + 3, w, h};
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 80);
        SDL_RenderFillRect(renderer_, &shadow);

        // Draw background
        SDL_Rect bg = {x, y, w, h};
        SDL_SetRenderDrawColor(renderer_, 45, 45, 48, 245);
        SDL_RenderFillRect(renderer_, &bg);

        // Draw border
        SDL_SetRenderDrawColor(renderer_, 80, 80, 85, 255);
        SDL_RenderDrawRect(renderer_, &bg);

        // Draw items
        int item_h = 28;
        int text_y = y + 4;

        for (int i = 0; i < item_count; ++i)
        {
            SDL_Rect item_rect = {x + 1, text_y, w - 2, item_h};

            // Highlight hovered item
            if (i == hover_index)
            {
                SDL_SetRenderDrawColor(renderer_, 62, 62, 66, 255);
                SDL_RenderFillRect(renderer_, &item_rect);
            }

            // Render text
            const char *label = get_label(i);
            if (label && label[0])
            {
                SDL_Color text_color = {210, 210, 210, 255};
                SDL_Surface *surf = TTF_RenderUTF8_Blended(font_, label, text_color);
                if (surf)
                {
                    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer_, surf);
                    if (tex)
                    {
                        SDL_Rect dst = {x + 4, text_y + (item_h - surf->h) / 2,
                                        surf->w, surf->h};
                        SDL_RenderCopy(renderer_, tex, nullptr, &dst);
                        SDL_DestroyTexture(tex);
                    }
                    SDL_FreeSurface(surf);
                }
            }

            text_y += item_h;
        }
    }

    // =============================================================================
    // Draw a scrollbar on the right edge
    // =============================================================================

    void Renderer::draw_scrollbar(int visible_rows, int scrollback, int scroll_offset)
    {
        if (!renderer_ || scrollback <= 0)
            return;

        int win_w, win_h;
        SDL_GetRendererOutputSize(renderer_, &win_w, &win_h);

        const int bar_width = 8;
        const int bar_x = win_w - bar_width;
        const int bar_height = win_h;

        // Draw scrollbar track (subtle background)
        SDL_Rect track = {bar_x, 0, bar_width, bar_height};
        SDL_SetRenderDrawColor(renderer_, 40, 40, 42, 120);
        SDL_RenderFillRect(renderer_, &track);

        // Calculate thumb size and position
        int total_lines = visible_rows + scrollback;
        float visible_ratio = static_cast<float>(visible_rows) / total_lines;
        int thumb_height = std::max(20, static_cast<int>(bar_height * visible_ratio));

        // Thumb position: scroll_offset 0 = bottom, scrollback = top
        float scroll_ratio = (scrollback > 0)
            ? static_cast<float>(scroll_offset) / scrollback
            : 0.0f;
        int thumb_y = static_cast<int>((bar_height - thumb_height) * (1.0f - scroll_ratio));

        // Draw thumb
        SDL_Rect thumb = {bar_x + 1, thumb_y, bar_width - 2, thumb_height};
        if (scroll_offset > 0)
            SDL_SetRenderDrawColor(renderer_, 140, 140, 145, 200); // brighter when scrolled
        else
            SDL_SetRenderDrawColor(renderer_, 90, 90, 95, 160);    // subtle when at bottom
        SDL_RenderFillRect(renderer_, &thumb);
    }

} // namespace xterm
