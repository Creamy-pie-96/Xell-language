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

        // Glyph texture cache to avoid re-rendering the same character repeatedly
        struct GlyphKey
        {
            char32_t ch;
            uint8_t fg_r, fg_g, fg_b;
            bool bold;
            bool operator==(const GlyphKey &o) const
            {
                return ch == o.ch && fg_r == o.fg_r && fg_g == o.fg_g &&
                       fg_b == o.fg_b && bold == o.bold;
            }
        };

        struct GlyphKeyHash
        {
            size_t operator()(const GlyphKey &k) const
            {
                size_t h = std::hash<char32_t>{}(k.ch);
                h ^= std::hash<uint8_t>{}(k.fg_r) << 1;
                h ^= std::hash<uint8_t>{}(k.fg_g) << 2;
                h ^= std::hash<uint8_t>{}(k.fg_b) << 3;
                h ^= std::hash<bool>{}(k.bold) << 4;
                return h;
            }
        };

        std::unordered_map<GlyphKey, SDL_Texture *, GlyphKeyHash> glyph_cache_;

        void draw_cell(int row, int col, const Cell &cell);
        void draw_cursor(int row, int col, const Cell &cell);
        void draw_scrollback_line(int screen_row, const std::vector<Cell> &line);
        SDL_Texture *get_glyph_texture(char32_t ch, Color fg, bool bold);
        void clear_glyph_cache();
    };

} // namespace xterm
