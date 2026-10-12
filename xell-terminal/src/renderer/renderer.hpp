#pragma once

// =============================================================================
// renderer.hpp — SDL2 + SDL_ttf terminal renderer
// =============================================================================
// Renders the ScreenBuffer as a 2D grid of monospace characters using SDL2.
// Only redraws cells marked as dirty for performance.
// =============================================================================

#include "../terminal/screen_buffer.hpp"
#include "../terminal/types.hpp"
#include <string>
#include <unordered_map>
#include <functional>

#include <SDL2/SDL_rect.h>

struct SDL_Window;
struct SDL_Renderer;
struct _TTF_Font;
typedef struct _TTF_Font TTF_Font;
struct SDL_Texture;

namespace xterm
{

    class Renderer
    {
    public:
        Renderer() = default;
        ~Renderer();

        /// Initialize SDL2 window, renderer, and font.
        bool init(int window_width, int window_height,
                  const std::string &font_path, int font_size);

        /// Render the entire screen buffer.
        /// @param buffer         The screen buffer to render
        /// @param cursor_visible Whether the cursor should be drawn
        /// @param scroll_offset  How many lines scrolled back (0 = live view)
        /// @param selection      Optional text selection to highlight
        void render(ScreenBuffer &buffer, bool cursor_visible, int scroll_offset = 0,
                    const TextSelection *selection = nullptr);

        /// Call after render() to present the frame.
        void present();

        /// Get the size of a single cell in pixels.
        void get_cell_size(int &width, int &height) const;

        /// Calculate terminal dimensions from pixel dimensions.
        void get_terminal_size(int window_w, int window_h, int &rows, int &cols) const;

        /// Get the SDL window.
        SDL_Window *get_window() const { return window_; }

        /// Draw a right-click context menu overlay.
        /// @param x, y          Top-left pixel of the menu
        /// @param w, h          Size of the menu
        /// @param item_count    Number of items
        /// @param get_label     Callback to get label for item i
        /// @param hover_index   Currently hovered item (-1 = none)
        void draw_context_menu(int x, int y, int w, int h,
                               int item_count,
                               std::function<const char *(int)> get_label,
                               int hover_index);

        /// Draw a scrollbar on the right edge of the terminal.
        /// @param visible_rows  Number of visible rows
        /// @param scrollback    Total scrollback lines
        /// @param scroll_offset Current scroll position (0 = live)
        void draw_scrollbar(int visible_rows, int scrollback, int scroll_offset);

        /// Clean up all resources.
        void shutdown();

    private:
        SDL_Window *window_ = nullptr;
        SDL_Renderer *renderer_ = nullptr;
        TTF_Font *font_ = nullptr;
        TTF_Font *font_bold_ = nullptr;

        int cell_w_ = 0; // cell width in pixels
        int cell_h_ = 0; // cell height in pixels
        int font_size_ = 0;

        // Offscreen canvas texture for dirty-cell tracking
        SDL_Texture *canvas_texture_ = nullptr;
        int canvas_w_ = 0;
        int canvas_h_ = 0;
        int last_scroll_offset_ = 0;

        // Single atlas texture for batched glyph rendering
        SDL_Texture *glyph_atlas_ = nullptr;
        int atlas_x_ = 0;
        int atlas_y_ = 0;
        int atlas_row_h_ = 0;
        static constexpr int ATLAS_WIDTH = 2048;
        static constexpr int ATLAS_HEIGHT = 2048;

        struct AtlasKey
        {
            char32_t ch;
            bool bold;
            bool operator==(const AtlasKey &o) const
            {
                return ch == o.ch && bold == o.bold;
            }
        };

        struct AtlasKeyHash
        {
            size_t operator()(const AtlasKey &k) const
            {
                return std::hash<char32_t>{}(k.ch) ^ (std::hash<bool>{}(k.bold) << 1);
            }
        };

        struct AtlasGlyph
        {
            SDL_Rect src_rect;
            int tex_w;
            int tex_h;
        };

        std::unordered_map<AtlasKey, AtlasGlyph, AtlasKeyHash> atlas_map_;
        Color current_atlas_fg_ = {255, 255, 255, 255};

        void init_glyph_atlas();
        const AtlasGlyph *get_atlas_glyph(char32_t ch, bool bold);
        void clear_glyph_atlas();
        void draw_glyph(int row, int col, char32_t ch, Color fg, bool bold);
        void draw_cell(int row, int col, const Cell &cell);
        void draw_cursor(int row, int col, const Cell &cell);
        void draw_scrollback_line(int screen_row, const std::vector<Cell> &line);
    };

} // namespace xterm
