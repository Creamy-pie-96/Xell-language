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
        clear_glyph_atlas();

        if (canvas_texture_)
        {
            SDL_DestroyTexture(canvas_texture_);
            canvas_texture_ = nullptr;
        }

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
        int win_w = 0, win_h = 0;
        SDL_GetWindowSize(window_, &win_w, &win_h);

        bool canvas_recreated = false;
        if (!canvas_texture_ || canvas_w_ != win_w || canvas_h_ != win_h)
        {
            if (canvas_texture_)
                SDL_DestroyTexture(canvas_texture_);
            canvas_texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888,
                                                SDL_TEXTUREACCESS_TARGET, win_w, win_h);
            canvas_w_ = win_w;
            canvas_h_ = win_h;
            canvas_recreated = true;
            buffer.mark_all_dirty();
        }

        int rows = buffer.get_rows();
        int cols = buffer.get_cols();

        if (last_scroll_offset_ > 0 && scroll_offset == 0)
        {
            buffer.mark_all_dirty();
        }
        last_scroll_offset_ = scroll_offset;

        if (scroll_offset > 0)
        {
            // Scrolling in history: draw directly into canvas_texture_
            SDL_SetRenderTarget(renderer_, canvas_texture_);
            SDL_SetRenderDrawColor(renderer_,
                                   Color::default_bg().r, Color::default_bg().g,
                                   Color::default_bg().b, 255);
            SDL_RenderClear(renderer_);

            int sb_size = buffer.scrollback_size();
            int sb_start = sb_size - scroll_offset;
            if (sb_start < 0)
                sb_start = 0;

            int screen_row = 0;
            for (int sb_idx = sb_start; sb_idx < sb_size && screen_row < rows; ++sb_idx, ++screen_row)
            {
                draw_scrollback_line(screen_row, buffer.scrollback_line(sb_idx));
            }

            int live_row = 0;
            for (; screen_row < rows && live_row < rows; ++screen_row, ++live_row)
            {
                for (int c = 0; c < cols; ++c)
                {
                    draw_cell(screen_row, c, buffer.cell_at(live_row, c));
                }
            }
            SDL_SetRenderTarget(renderer_, nullptr);
        }
        else
        {
            // Live view: damage-region and dirty-cell tracking
            bool has_dirty = canvas_recreated;
            if (!has_dirty)
            {
                for (int r = 0; r < rows; ++r)
                {
                    for (int c = 0; c < cols; ++c)
                    {
                        if (buffer.cell_at(r, c).dirty)
                        {
                            has_dirty = true;
                            break;
                        }
                    }
                    if (has_dirty)
                        break;
                }
            }

            if (has_dirty)
            {
                SDL_SetRenderTarget(renderer_, canvas_texture_);

                if (canvas_recreated)
                {
                    SDL_SetRenderDrawColor(renderer_,
                                           Color::default_bg().r, Color::default_bg().g,
                                           Color::default_bg().b, 255);
                    SDL_RenderClear(renderer_);
                }

                // 1. Draw backgrounds as merged runs for dirty cells
                for (int r = 0; r < rows; ++r)
                {
                    int c = 0;
                    while (c < cols)
                    {
                        if (!buffer.cell_at(r, c).dirty)
                        {
                            c++;
                            continue;
                        }

                        Color bg = buffer.cell_at(r, c).bg;
                        int start_c = c;
                        while (c < cols && buffer.cell_at(r, c).dirty)
                        {
                            const auto &next = buffer.cell_at(r, c);
                            if (next.bg.r != bg.r || next.bg.g != bg.g ||
                                next.bg.b != bg.b || next.bg.a != bg.a)
                                break;
                            c++;
                        }

                        SDL_Rect run_rect = {
                            start_c * cell_w_,
                            r * cell_h_,
                            (c - start_c) * cell_w_,
                            cell_h_};
                        SDL_SetRenderDrawColor(renderer_, bg.r, bg.g, bg.b, bg.a);
                        SDL_RenderFillRect(renderer_, &run_rect);
                    }
                }

                // 2. Draw glyphs and underlines for dirty cells
                for (int r = 0; r < rows; ++r)
                {
                    for (int c = 0; c < cols; ++c)
                    {
                        const auto &cell = buffer.cell_at(r, c);
                        if (!cell.dirty)
                            continue;

                        if (cell.ch != U' ' && cell.ch != 0)
                        {
                            draw_glyph(r, c, cell.ch, cell.fg, cell.bold);
                        }

                        if (cell.underline)
                        {
                            int x = c * cell_w_;
                            int uy = r * cell_h_ + cell_h_ - 2;
                            SDL_SetRenderDrawColor(renderer_, cell.fg.r, cell.fg.g, cell.fg.b, cell.fg.a);
                            SDL_RenderDrawLine(renderer_, x, uy, x + cell_w_ - 1, uy);
                        }
                    }
                }

                SDL_SetRenderTarget(renderer_, nullptr);
                buffer.clear_dirty_flags();
            }
        }

        // Copy up-to-date canvas texture to window
        SDL_RenderCopy(renderer_, canvas_texture_, nullptr, nullptr);

        // Draw cursor overlay on window backbuffer
        if (cursor_visible && buffer.cursor_visible && scroll_offset == 0)
        {
            draw_cursor(buffer.cursor_row, buffer.cursor_col,
                        buffer.cell_at(buffer.cursor_row, buffer.cursor_col));
        }

        // Draw selection overlay on window backbuffer
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
            draw_glyph(row, col, cell.ch, cell.fg, cell.bold);
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
            draw_glyph(row, col, cell.ch, inverse_fg, cell.bold);
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
    }

    // =============================================================================
    // Glyph atlas management & rendering
    // =============================================================================

    void Renderer::init_glyph_atlas()
    {
        if (glyph_atlas_)
            return;

        glyph_atlas_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888,
                                         SDL_TEXTUREACCESS_STATIC, ATLAS_WIDTH, ATLAS_HEIGHT);
        if (glyph_atlas_)
        {
            SDL_SetTextureBlendMode(glyph_atlas_, SDL_BLENDMODE_BLEND);
            std::vector<uint32_t> blank(ATLAS_WIDTH * ATLAS_HEIGHT, 0);
            SDL_UpdateTexture(glyph_atlas_, nullptr, blank.data(), ATLAS_WIDTH * sizeof(uint32_t));
        }

        atlas_x_ = 0;
        atlas_y_ = 0;
        atlas_row_h_ = 0;
        atlas_map_.clear();
    }

    void Renderer::clear_glyph_atlas()
    {
        if (glyph_atlas_)
        {
            SDL_DestroyTexture(glyph_atlas_);
            glyph_atlas_ = nullptr;
        }
        atlas_map_.clear();
        atlas_x_ = 0;
        atlas_y_ = 0;
        atlas_row_h_ = 0;
    }

    const Renderer::AtlasGlyph *Renderer::get_atlas_glyph(char32_t ch, bool bold)
    {
        if (!glyph_atlas_)
            init_glyph_atlas();
        if (!glyph_atlas_)
            return nullptr;

        AtlasKey key = {ch, bold};
        auto it = atlas_map_.find(key);
        if (it != atlas_map_.end())
            return &it->second;

        TTF_Font *f = bold ? font_bold_ : font_;
        if (bold && f == font_)
            TTF_SetFontStyle(f, TTF_STYLE_BOLD);

        SDL_Color white = {255, 255, 255, 255};
        SDL_Surface *surface = nullptr;
        if (ch <= 0xFFFF)
        {
            surface = TTF_RenderGlyph_Blended(f, static_cast<Uint16>(ch), white);
        }
        else
        {
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
            surface = TTF_RenderUTF8_Blended(f, utf8, white);
        }

        if (bold && f == font_)
            TTF_SetFontStyle(f, TTF_STYLE_NORMAL);

        if (!surface)
            return nullptr;

        SDL_Surface *formatted = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ARGB8888, 0);
        SDL_FreeSurface(surface);
        if (!formatted)
            return nullptr;

        if (atlas_x_ + formatted->w > ATLAS_WIDTH)
        {
            atlas_x_ = 0;
            atlas_y_ += atlas_row_h_ + 1;
            atlas_row_h_ = 0;
        }

        if (atlas_y_ + formatted->h > ATLAS_HEIGHT)
        {
            clear_glyph_atlas();
            init_glyph_atlas();
        }

        SDL_Rect slot = {atlas_x_, atlas_y_, formatted->w, formatted->h};
        SDL_UpdateTexture(glyph_atlas_, &slot, formatted->pixels, formatted->pitch);

        AtlasGlyph glyph = {slot, formatted->w, formatted->h};
        atlas_map_[key] = glyph;

        atlas_x_ += formatted->w + 1;
        atlas_row_h_ = std::max(atlas_row_h_, formatted->h);

        SDL_FreeSurface(formatted);
        return &atlas_map_[key];
    }

    void Renderer::draw_glyph(int row, int col, char32_t ch, Color fg, bool bold)
    {
        const AtlasGlyph *glyph = get_atlas_glyph(ch, bold);
        if (!glyph)
            return;

        if (current_atlas_fg_.r != fg.r || current_atlas_fg_.g != fg.g ||
            current_atlas_fg_.b != fg.b || current_atlas_fg_.a != fg.a)
        {
            SDL_SetTextureColorMod(glyph_atlas_, fg.r, fg.g, fg.b);
            SDL_SetTextureAlphaMod(glyph_atlas_, fg.a);
            current_atlas_fg_ = fg;
        }

        int x = col * cell_w_;
        int y = row * cell_h_;
        SDL_Rect dst = {
            x + (cell_w_ - glyph->tex_w) / 2,
            y + (cell_h_ - glyph->tex_h) / 2,
            glyph->tex_w,
            glyph->tex_h};
        SDL_RenderCopy(renderer_, glyph_atlas_, &glyph->src_rect, &dst);
    }

    // =============================================================================
    // Draw a right-click context menu
    // =============================================================================

    void Renderer::draw_context_menu(int x, int y, int w, int h,
                                     int item_count,
                                     std::function<const char *(int)> get_label,
                                     int hover_index)
    {
        if (!renderer_ || !font_ || item_count <= 0)
            return;

        // Clamp to window bounds
        int win_w, win_h;
        SDL_GetRendererOutputSize(renderer_, &win_w, &win_h);
        if (x + w > win_w)
            x = win_w - w;
        if (y + h > win_h)
            y = win_h - h;
        if (x < 0)
            x = 0;
        if (y < 0)
            y = 0;

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
            SDL_SetRenderDrawColor(renderer_, 90, 90, 95, 160); // subtle when at bottom
        SDL_RenderFillRect(renderer_, &thumb);
    }

} // namespace xterm
