// =============================================================================
// main.cpp — Xell Terminal Emulator entry point
// =============================================================================
// Wires together the PTY, VT parser, renderer, and input handler into a
// working terminal emulator. The PTY output is read in a background thread
// and fed into the VT parser which updates the shared ScreenBuffer.
// The main thread handles SDL events and rendering.
// =============================================================================

#include "pty/pty.hpp"
#include "terminal/screen_buffer.hpp"
#include "terminal/vt_parser.hpp"
#include "terminal/input_handler.hpp"
#include "renderer/renderer.hpp"

#include <SDL2/SDL.h>

#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>

#ifndef _WIN32
#include <unistd.h>
#include <sys/stat.h>
#include <climits>
#include <libgen.h>
#else
#include <io.h>
#include <direct.h>
#endif

#include <filesystem>

namespace fs = std::filesystem;

// =============================================================================
// Configuration
// =============================================================================

static constexpr int DEFAULT_WINDOW_WIDTH = 1100;
static constexpr int DEFAULT_WINDOW_HEIGHT = 650;
static constexpr int DEFAULT_FONT_SIZE = 15;

// Path to the bundled font, relative to the executable
static const char *FONT_PATH = "assets/fonts/JetBrainsMono-Regular.ttf";

// Cursor blink interval
static constexpr int CURSOR_BLINK_MS = 530;

// =============================================================================
// Utility: check if a file exists and is executable
// =============================================================================

static bool is_executable(const std::string &path)
{
    try
    {
        if (!fs::exists(path))
            return false;
        if (!fs::is_regular_file(path))
            return false;
    }
    catch (...)
    {
        return false;
    }
#ifdef _WIN32
    return _access(path.c_str(), 0) == 0;
#else
    return access(path.c_str(), X_OK) == 0;
#endif
}

// =============================================================================
// Utility: get the directory where our own executable lives
// =============================================================================

static std::string get_exe_dir()
{
    // SDL_GetBasePath returns the directory of the running executable
    // (more reliable than /proc/self/exe since we already depend on SDL)
    char *base = SDL_GetBasePath();
    if (base)
    {
        std::string dir(base);
        SDL_free(base);
        // SDL_GetBasePath includes trailing separator
        return dir;
    }

    // Fallback: try /proc/self/exe on Linux
#ifdef __linux__
    char buf[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len > 0)
    {
        buf[len] = '\0';
        return fs::path(buf).parent_path().string() + "/";
    }
#endif

    return "./";
}

// =============================================================================
// Utility: search for "xell" in $PATH (like `which xell`)
// =============================================================================

static std::string find_in_path(const std::string &name)
{
    const char *path_env = std::getenv("PATH");
    if (!path_env)
        return {};

    std::string path_str(path_env);

#ifdef _WIN32
    char delim = ';';
    std::string exe_name = name + ".exe";
#else
    char delim = ':';
    std::string exe_name = name;
#endif

    size_t start = 0;
    while (start < path_str.size())
    {
        size_t end = path_str.find(delim, start);
        if (end == std::string::npos)
            end = path_str.size();

        std::string dir = path_str.substr(start, end - start);
        if (!dir.empty())
        {
            std::string candidate = dir + "/" + exe_name;
            if (is_executable(candidate))
                return candidate;
        }
        start = end + 1;
    }
    return {};
}

// =============================================================================
// Find the Xell executable — dynamic, multi-strategy discovery
// =============================================================================
// Search order:
//   1. XELL_PATH environment variable (explicit override)
//   2. Sibling of this executable  (xell-terminal/build/xell → same dir)
//   3. Xell project build dirs     (../../build/xell, ../../build_release/xell)
//   4. $HOME/.local/bin/xell       (default --local install location)
//   5. Search $PATH for "xell"     (works if installed system-wide)
//   6. Well-known system paths     (/usr/local/bin/xell, /usr/bin/xell)
//   7. FAIL — do NOT fall back to bash; print a helpful error instead
// =============================================================================

struct ShellResult
{
    std::string path;
    std::string found_via; // for the log message
};

static ShellResult find_xell_path()
{
    // --- 1. Explicit override via environment variable ---
    const char *env = std::getenv("XELL_PATH");
    if (env && std::strlen(env) > 0 && is_executable(env))
        return {env, "XELL_PATH env var"};

    // --- 2. Sibling of this executable ---
    // If xell-terminal lives in xell-terminal/build/, check for xell there too
    std::string exe_dir = get_exe_dir();

#ifdef _WIN32
    std::string xell_name = "xell.exe";
#else
    std::string xell_name = "xell";
#endif

    {
        std::string sibling = exe_dir + xell_name;
        if (is_executable(sibling))
            return {sibling, "same directory as xell-terminal"};
    }

    // --- 3. Xell project build directories (relative to terminal binary) ---
    // Terminal binary:  <project>/xell-terminal/build/xell-terminal
    // Xell binary:      <project>/build/xell  or  <project>/build_release/xell
    // exe_dir = <project>/xell-terminal/build/
    //   parent_path() = .../xell-terminal/build
    //   parent_path() = .../xell-terminal
    //   parent_path() = <project>
    try
    {
        fs::path exe_fs = fs::path(exe_dir);
        // Remove trailing separator by canonicalizing
        if (exe_fs.filename().empty())
            exe_fs = exe_fs.parent_path();
        fs::path project_root = exe_fs.parent_path().parent_path();

        std::string build_candidates[] = {
            (project_root / "build" / xell_name).string(),
            (project_root / "build_release" / xell_name).string(),
            (project_root / xell_name).string(),
        };

        for (const auto &candidate : build_candidates)
        {
            if (is_executable(candidate))
                return {fs::canonical(candidate).string(),
                        "project build dir"};
        }
    }
    catch (...)
    {
        // filesystem errors are non-fatal; just continue searching
    }

    // --- 4. User-local install ($HOME/.local/bin/xell) ---
    const char *home = std::getenv("HOME");
#ifdef _WIN32
    if (!home)
        home = std::getenv("USERPROFILE");
#endif
    if (home)
    {
        std::string local_bin = std::string(home) + "/.local/bin/" + xell_name;
        if (is_executable(local_bin))
            return {local_bin, "$HOME/.local/bin"};
    }

    // --- 5. Search $PATH (like `which xell`) ---
    {
        std::string from_path = find_in_path("xell");
        if (!from_path.empty())
            return {from_path, "$PATH"};
    }

    // --- 6. Well-known system install paths ---
    const char *system_paths[] = {
        "/usr/local/bin/xell",
        "/usr/bin/xell",
        "/opt/xell/bin/xell",
    };
    for (const char *p : system_paths)
    {
        if (is_executable(p))
            return {p, "system install"};
    }

    // --- 7. NOT FOUND ---
    return {"", ""};
}

// =============================================================================
// Resolve font path relative to the executable
// =============================================================================

static std::string resolve_font_path()
{
    // SDL_GetBasePath() returns the directory containing the executable
    char *base = SDL_GetBasePath();
    if (base)
    {
        std::string path = std::string(base) + FONT_PATH;
        SDL_free(base);
        return path;
    }
    return FONT_PATH;
}

// =============================================================================
// Main
// =============================================================================

int main(int argc, char *argv[])
{
    // --- Determine which shell to run ---
    std::string shell_path;
    std::vector<std::string> shell_args;
    std::string found_via;

    if (argc >= 2)
    {
        // User specified a shell/command explicitly
        shell_path = argv[1];
        found_via = "command line argument";
        for (int i = 2; i < argc; ++i)
            shell_args.push_back(argv[i]);
    }
    else
    {
        // Auto-discover the Xell executable
        auto result = find_xell_path();
        shell_path = result.path;
        found_via = result.found_via;
    }

    // --- If Xell was not found, print a helpful error and exit ---
    if (shell_path.empty())
    {
        std::cerr << "\n";
        std::cerr << "  ╔══════════════════════════════════════════════════════╗\n";
        std::cerr << "  ║     Xell Terminal — Could not find 'xell'           ║\n";
        std::cerr << "  ╚══════════════════════════════════════════════════════╝\n";
        std::cerr << "\n";
        std::cerr << "  The Xell Terminal needs the Xell language binary to run.\n";
        std::cerr << "  Searched in:\n";
        std::cerr << "    • $XELL_PATH environment variable\n";
        std::cerr << "    • Same directory as xell-terminal\n";
        std::cerr << "    • Project build dirs (../../build/xell)\n";
        std::cerr << "    • $HOME/.local/bin/xell\n";
        std::cerr << "    • $PATH\n";
        std::cerr << "    • /usr/local/bin/xell, /usr/bin/xell\n";
        std::cerr << "\n";
        std::cerr << "  To fix this, do one of:\n";
        std::cerr << "    1. Build & install Xell:  cd <project_root> && ./install.sh\n";
        std::cerr << "    2. Set the path:          export XELL_PATH=/path/to/xell\n";
        std::cerr << "    3. Run another shell:     ./xell-terminal /bin/bash\n";
        std::cerr << "\n";
        return 1;
    }

    std::cout << "[Xell Terminal] Launching: " << shell_path
              << "  (found via " << found_via << ")" << std::endl;

    // --- Initialize renderer (SDL2) ---
    xterm::Renderer renderer;
    std::string font_path = resolve_font_path();

    if (!renderer.init(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT,
                       font_path, DEFAULT_FONT_SIZE))
    {
        std::cerr << "[Xell Terminal] Failed to initialize renderer.\n";
        std::cerr << "  Font path: " << font_path << "\n";
        std::cerr << "  Make sure SDL2, SDL2_ttf are installed and the font file exists.\n";
        return 1;
    }

    // --- Calculate initial terminal dimensions ---
    int term_rows, term_cols;
    renderer.get_terminal_size(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT,
                               term_rows, term_cols);

    // --- Create the screen buffer ---
    xterm::ScreenBuffer buffer(term_rows, term_cols);

    // --- Create the VT parser ---
    xterm::VTParser vt_parser(buffer);

    // Window title callback
    std::string window_title = "Xell Terminal";
    std::mutex title_mutex;
    vt_parser.on_title_change = [&](const std::string &title)
    {
        std::lock_guard<std::mutex> lock(title_mutex);
        window_title = title;
    };

    // --- Spawn the PTY ---
    xterm::PTY pty;
    if (!pty.spawn(shell_path, term_rows, term_cols, shell_args))
    {
        std::cerr << "[Xell Terminal] Failed to spawn PTY with shell: "
                  << shell_path << "\n";
        renderer.shutdown();
        return 1;
    }

    // --- Shared state between threads ---
    std::mutex buffer_mutex;
    std::atomic<bool> running{true};
    std::atomic<bool> has_new_data{false};

    // --- Background thread: read PTY output and feed to VT parser ---
    std::thread reader_thread([&]()
                              {
        while (running.load()) {
            std::string data = pty.read();
            if (!data.empty()) {
                std::lock_guard<std::mutex> lock(buffer_mutex);
                vt_parser.feed(data);
                has_new_data.store(true);
            } else {
                // Check if child process exited
                if (!pty.is_alive()) {
                    running.store(false);
                    // Push a quit event so the main loop wakes up
                    SDL_Event quit_event;
                    quit_event.type = SDL_QUIT;
                    SDL_PushEvent(&quit_event);
                    break;
                }
                // Sleep a tiny bit to avoid busy-waiting when no data
                std::this_thread::sleep_for(std::chrono::milliseconds(2));
            }
        } });

    // --- Scrollback state ---
    int scroll_offset = 0; // 0 = live view, >0 = scrolled back

    // --- Cursor blink state ---
    bool cursor_visible = true;
    auto last_blink = std::chrono::steady_clock::now();
    auto last_keypress = std::chrono::steady_clock::now();

    // --- Text selection state ---
    xterm::TextSelection selection;
    int cell_w, cell_h;
    renderer.get_cell_size(cell_w, cell_h);

    // --- Right-click context menu state ---
    struct ContextMenu
    {
        bool visible = false;
        int x = 0, y = 0; // pixel position
        int width = 220, height = 0;
        struct Item
        {
            std::string label;
            xterm::InputAction action;
        };
        std::vector<Item> items;
        int hover_index = -1;

        void show(int mx, int my, bool has_sel)
        {
            x = mx;
            y = my;
            items.clear();
            if (has_sel)
                items.push_back({"  Copy              Ctrl+C", xterm::InputAction::COPY});
            items.push_back({"  Paste             Ctrl+V", xterm::InputAction::PASTE});
            items.push_back({"  Select All  Ctrl+Shift+A", xterm::InputAction::SELECT_ALL});
            height = static_cast<int>(items.size()) * 28 + 8;
            hover_index = -1;
            visible = true;
        }
        void hide() { visible = false; }
        int hit_test(int mx, int my) const
        {
            if (!visible)
                return -1;
            if (mx < x || mx > x + width || my < y || my > y + height)
                return -1;
            int idx = (my - y - 4) / 28;
            if (idx < 0 || idx >= (int)items.size())
                return -1;
            return idx;
        }
    } context_menu;

    // --- Font zoom state ---
    int current_font_size = DEFAULT_FONT_SIZE;

    // --- Double-click detection ---
    Uint32 last_click_time = 0;
    int last_click_row = -1, last_click_col = -1;

    // --- Scrollbar interaction state ---
    bool scrollbar_dragging = false;
    static constexpr int SCROLLBAR_WIDTH = 8; // must match renderer
    float scrollbar_drag_start_ratio = 0.0f;

    // --- Main event/render loop ---
    while (running.load())
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_QUIT:
                running.store(false);
                break;

            case SDL_KEYDOWN:
            {
                // Dismiss context menu on any keypress
                context_menu.hide();

                // Any keypress returns to live view
                scroll_offset = 0;

                // Reset cursor blink (show cursor on keypress)
                cursor_visible = true;
                last_keypress = std::chrono::steady_clock::now();
                last_blink = last_keypress;

                bool shift = (event.key.keysym.mod & KMOD_SHIFT) != 0;

                // Shift+PageUp/Down for scrollback
                if (event.key.keysym.sym == SDLK_PAGEUP && shift)
                {
                    std::lock_guard<std::mutex> lock(buffer_mutex);
                    scroll_offset = std::min(
                        scroll_offset + term_rows,
                        buffer.scrollback_size());
                    break;
                }
                if (event.key.keysym.sym == SDLK_PAGEDOWN && shift)
                {
                    scroll_offset = std::max(scroll_offset - term_rows, 0);
                    break;
                }

                // Translate key through InputHandler
                auto result = xterm::InputHandler::translate(event, selection.has_selection);

                switch (result.action)
                {
                case xterm::InputAction::COPY:
                {
                    if (selection.has_selection)
                    {
                        std::lock_guard<std::mutex> lock(buffer_mutex);
                        std::string text = buffer.extract_text(selection, scroll_offset);
                        if (!text.empty())
                            SDL_SetClipboardText(text.c_str());
                    }
                    break;
                }
                case xterm::InputAction::PASTE:
                {
                    if (SDL_HasClipboardText())
                    {
                        char *clip = SDL_GetClipboardText();
                        if (clip)
                        {
                            pty.write(std::string(clip));
                            SDL_free(clip);
                        }
                    }
                    selection.clear();
                    break;
                }
                case xterm::InputAction::SELECT_ALL:
                {
                    std::lock_guard<std::mutex> lock(buffer_mutex);
                    selection.has_selection = true;
                    selection.active = false;
                    selection.start = {0, 0};
                    selection.end = {term_rows - 1, term_cols - 1};
                    break;
                }
                case xterm::InputAction::ZOOM_IN:
                {
                    if (current_font_size < 48)
                    {
                        current_font_size += 2;
                        renderer.shutdown();
                        int w, h;
                        SDL_GetWindowSize(renderer.get_window(), &w, &h);
                        // Re-init will create new window — skip for now
                        // TODO: proper font resize without recreating window
                    }
                    break;
                }
                case xterm::InputAction::ZOOM_OUT:
                {
                    if (current_font_size > 8)
                    {
                        current_font_size -= 2;
                        // TODO: proper font resize
                    }
                    break;
                }
                case xterm::InputAction::ZOOM_RESET:
                {
                    current_font_size = DEFAULT_FONT_SIZE;
                    // TODO: proper font resize
                    break;
                }
                case xterm::InputAction::SEND_TO_PTY:
                {
                    selection.clear(); // clear selection on typed input
                    if (!result.data.empty())
                        pty.write(result.data);
                    break;
                }
                default:
                    break;
                }
                break;
            }

            case SDL_TEXTINPUT:
            {
                // Dismiss context menu
                context_menu.hide();

                // Regular character input
                scroll_offset = 0;
                cursor_visible = true;
                last_keypress = std::chrono::steady_clock::now();
                last_blink = last_keypress;

                // Don't send text if Ctrl is held (Ctrl+C etc. already handled in KEYDOWN)
                if (SDL_GetModState() & KMOD_CTRL)
                    break;

                std::string bytes = xterm::InputHandler::translate_text(event);
                if (!bytes.empty())
                {
                    selection.clear();
                    pty.write(bytes);
                }
                break;
            }

            case SDL_MOUSEWHEEL:
            {
                // Scroll through history
                context_menu.hide();
                std::lock_guard<std::mutex> lock(buffer_mutex);
                if (event.wheel.y > 0)
                {
                    scroll_offset = std::min(
                        scroll_offset + 3,
                        buffer.scrollback_size());
                }
                else if (event.wheel.y < 0)
                {
                    scroll_offset = std::max(scroll_offset - 3, 0);
                }
                break;
            }

            case SDL_MOUSEBUTTONDOWN:
            {
                if (event.button.button == SDL_BUTTON_LEFT)
                {
                    // Check if clicking on the context menu
                    if (context_menu.visible)
                    {
                        int idx = context_menu.hit_test(event.button.x, event.button.y);
                        if (idx >= 0)
                        {
                            auto action = context_menu.items[idx].action;
                            context_menu.hide();
                            // Execute the menu action
                            if (action == xterm::InputAction::COPY && selection.has_selection)
                            {
                                std::lock_guard<std::mutex> lock(buffer_mutex);
                                std::string text = buffer.extract_text(selection, scroll_offset);
                                if (!text.empty())
                                    SDL_SetClipboardText(text.c_str());
                            }
                            else if (action == xterm::InputAction::PASTE)
                            {
                                if (SDL_HasClipboardText())
                                {
                                    char *clip = SDL_GetClipboardText();
                                    if (clip)
                                    {
                                        pty.write(std::string(clip));
                                        SDL_free(clip);
                                    }
                                }
                            }
                            else if (action == xterm::InputAction::SELECT_ALL)
                            {
                                std::lock_guard<std::mutex> lock(buffer_mutex);
                                selection.has_selection = true;
                                selection.active = false;
                                selection.start = {0, 0};
                                selection.end = {term_rows - 1, term_cols - 1};
                            }
                            break;
                        }
                        context_menu.hide();
                        break;
                    }

                    // Check if clicking on the scrollbar
                    int win_w, win_h;
                    SDL_GetWindowSize(SDL_GetWindowFromID(event.button.windowID), &win_w, &win_h);
                    int sb_size = 0;
                    {
                        std::lock_guard<std::mutex> lock(buffer_mutex);
                        sb_size = buffer.scrollback_size();
                    }
                    if (sb_size > 0 && event.button.x >= win_w - SCROLLBAR_WIDTH)
                    {
                        // Click on scrollbar: jump to position and start dragging
                        scrollbar_dragging = true;
                        int total_lines = term_rows + sb_size;
                        float click_ratio = 1.0f - (static_cast<float>(event.button.y) / win_h);
                        scroll_offset = static_cast<int>(click_ratio * sb_size);
                        if (scroll_offset < 0)
                            scroll_offset = 0;
                        if (scroll_offset > sb_size)
                            scroll_offset = sb_size;
                        break;
                    }

                    int col = event.button.x / cell_w;
                    int row = event.button.y / cell_h;

                    // Double-click detection → select word
                    Uint32 now_tick = SDL_GetTicks();
                    if (now_tick - last_click_time < 400 &&
                        row == last_click_row && col == last_click_col)
                    {
                        // Double-click: select word under cursor
                        std::lock_guard<std::mutex> lock(buffer_mutex);
                        int lo_col = col, hi_col = col;
                        while (lo_col > 0)
                        {
                            xterm::Cell c = buffer.get_cell(row, lo_col - 1);
                            if (c.ch == U' ' || c.ch == 0)
                                break;
                            lo_col--;
                        }
                        while (hi_col < term_cols - 1)
                        {
                            xterm::Cell c = buffer.get_cell(row, hi_col + 1);
                            if (c.ch == U' ' || c.ch == 0)
                                break;
                            hi_col++;
                        }
                        selection.active = false;
                        selection.has_selection = true;
                        selection.start = {row, lo_col};
                        selection.end = {row, hi_col};
                        last_click_time = 0; // prevent triple-click
                    }
                    else
                    {
                        // Single click: start drag selection
                        selection.active = true;
                        selection.has_selection = true;
                        selection.start = {row, col};
                        selection.end = {row, col};
                        last_click_time = now_tick;
                        last_click_row = row;
                        last_click_col = col;
                    }
                }
                else if (event.button.button == SDL_BUTTON_RIGHT)
                {
                    // Right-click: show context menu
                    context_menu.show(event.button.x, event.button.y,
                                      selection.has_selection);
                }
                break;
            }

            case SDL_MOUSEMOTION:
            {
                // Update context menu hover
                if (context_menu.visible)
                {
                    context_menu.hover_index = context_menu.hit_test(
                        event.motion.x, event.motion.y);
                }

                // Scrollbar drag
                if (scrollbar_dragging && (event.motion.state & SDL_BUTTON_LMASK))
                {
                    int win_w, win_h;
                    SDL_GetWindowSize(SDL_GetWindowFromID(event.motion.windowID), &win_w, &win_h);
                    int sb_size = 0;
                    {
                        std::lock_guard<std::mutex> lock(buffer_mutex);
                        sb_size = buffer.scrollback_size();
                    }
                    if (sb_size > 0 && win_h > 0)
                    {
                        float click_ratio = 1.0f - (static_cast<float>(event.motion.y) / win_h);
                        scroll_offset = static_cast<int>(click_ratio * sb_size);
                        if (scroll_offset < 0)
                            scroll_offset = 0;
                        if (scroll_offset > sb_size)
                            scroll_offset = sb_size;
                    }
                    break;
                }

                if (selection.active && (event.motion.state & SDL_BUTTON_LMASK))
                {
                    int col = event.motion.x / cell_w;
                    int row = event.motion.y / cell_h;
                    if (col < 0)
                        col = 0;
                    if (row < 0)
                        row = 0;
                    if (col >= term_cols)
                        col = term_cols - 1;
                    if (row >= term_rows)
                        row = term_rows - 1;
                    selection.end = {row, col};
                }
                break;
            }

            case SDL_MOUSEBUTTONUP:
            {
                if (event.button.button == SDL_BUTTON_LEFT)
                {
                    scrollbar_dragging = false;
                    if (selection.active)
                    {
                        selection.active = false;
                        if (selection.start == selection.end)
                            selection.clear();
                    }
                }
                break;
            }

            case SDL_WINDOWEVENT:
            {
                if (event.window.event == SDL_WINDOWEVENT_RESIZED ||
                    event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                {
                    int new_w = event.window.data1;
                    int new_h = event.window.data2;

                    int new_rows, new_cols;
                    renderer.get_terminal_size(new_w, new_h, new_rows, new_cols);

                    if (new_rows != term_rows || new_cols != term_cols)
                    {
                        std::lock_guard<std::mutex> lock(buffer_mutex);
                        term_rows = new_rows;
                        term_cols = new_cols;
                        buffer.resize(term_rows, term_cols);
                        pty.resize(term_rows, term_cols);
                        renderer.get_cell_size(cell_w, cell_h);
                        selection.clear();
                    }
                }
                // Dismiss menu on focus loss
                if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
                    context_menu.hide();
                break;
            }

            default:
                break;
            }
        }

        // --- Update window title if changed ---
        {
            std::lock_guard<std::mutex> lock(title_mutex);
            SDL_SetWindowTitle(renderer.get_window(), window_title.c_str());
        }

        // --- Cursor blink ---
        auto now = std::chrono::steady_clock::now();
        auto ms_since_blink = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  now - last_blink)
                                  .count();
        if (ms_since_blink >= CURSOR_BLINK_MS)
        {
            cursor_visible = !cursor_visible;
            last_blink = now;
        }

        // --- Render ---
        {
            std::lock_guard<std::mutex> lock(buffer_mutex);
            renderer.render(buffer, cursor_visible, scroll_offset, &selection);
        }

        // --- Draw context menu (on top of everything) ---
        if (context_menu.visible)
        {
            renderer.draw_context_menu(
                context_menu.x, context_menu.y,
                context_menu.width, context_menu.height,
                context_menu.items.size(),
                [&](int i) -> const char *
                { return context_menu.items[i].label.c_str(); },
                context_menu.hover_index);
        }

        // --- Draw scrollbar ---
        {
            std::lock_guard<std::mutex> lock(buffer_mutex);
            int sb_total = buffer.scrollback_size();
            if (sb_total > 0)
            {
                renderer.draw_scrollbar(term_rows, sb_total, scroll_offset);
            }
        }

        renderer.present();

        // Cap at ~120fps if vsync isn't doing it
        // (SDL_RENDERER_PRESENTVSYNC usually handles this)
        if (!has_new_data.load())
        {
            SDL_Delay(4); // ~250fps max when idle, saves CPU
        }
        has_new_data.store(false);
    }

    // --- Cleanup ---
    running.store(false);
    pty.kill();

    if (reader_thread.joinable())
        reader_thread.join();

    renderer.shutdown();

    std::cout << "[Xell Terminal] Goodbye.\n";
    return 0;
}
