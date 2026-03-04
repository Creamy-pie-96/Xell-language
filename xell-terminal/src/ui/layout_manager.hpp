#pragma once

// =============================================================================
// layout_manager.hpp — Layout orchestrator for the Xell Terminal IDE
// =============================================================================
// Manages the IDE layout: sidebar (file tree), editor area, bottom panel.
// Handles resize, focus routing, and rendering composition.
//
// Layout:
// ┌──────────────────────────────────────────────────────────────┐
// │ Tab Bar (editor tabs)                                        │
// ├─────────────┬────────────────────────────────────────────────┤
// │  Sidebar    │  Editor Area                                   │
// │  (file tree)│                                                │
// │             │                                                │
// │             │                                                │
// │             ├────────────────────────────────────────────────┤
// │             │  Bottom Panel (REPL / Terminal / Output)       │
// │             │                                                │
// ├─────────────┴────────────────────────────────────────────────┤
// │ Status Bar                                                   │
// └──────────────────────────────────────────────────────────────┘
// =============================================================================

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <SDL2/SDL.h>
#include "../terminal/types.hpp"
#include "../editor/editor_view.hpp"
#include "../editor/editor_widget.hpp"
#include "../theme/theme_loader.hpp"
#include "panel.hpp"
#include "file_tree.hpp"

namespace xterm
{

    // ─── Focus regions ───────────────────────────────────────────────────

    enum class FocusRegion
    {
        Sidebar,
        Editor,
        BottomPanel,
    };

    // ─── Layout Manager ──────────────────────────────────────────────────

    class LayoutManager
    {
    public:
        LayoutManager(const ThemeData &theme)
            : theme_(theme),
              editor_(theme),
              fileTree_(theme)
        {
            loadColors();

            // Wire file tree to open files in editor
            fileTree_.setOnOpenFile([this](const std::string &path)
                                   { editor_.openFile(path);
                                     setFocus(FocusRegion::Editor); });
        }

        // ── Initialization ──────────────────────────────────────────

        void setProjectRoot(const std::string &path)
        {
            fileTree_.setRoot(path);
        }

        // ── Window resize ───────────────────────────────────────────

        void resize(int cols, int rows)
        {
            totalCols_ = cols;
            totalRows_ = rows;
            recalcLayout();
        }

        // ── Focus management ────────────────────────────────────────

        FocusRegion focus() const { return focus_; }

        void setFocus(FocusRegion region)
        {
            focus_ = region;
            fileTree_.setFocused(region == FocusRegion::Sidebar);
        }

        void cycleFocus()
        {
            switch (focus_)
            {
            case FocusRegion::Sidebar:
                setFocus(FocusRegion::Editor);
                break;
            case FocusRegion::Editor:
                if (showBottomPanel_)
                    setFocus(FocusRegion::BottomPanel);
                else
                    setFocus(FocusRegion::Sidebar);
                break;
            case FocusRegion::BottomPanel:
                setFocus(FocusRegion::Sidebar);
                break;
            }
        }

        // ── Toggle panels ───────────────────────────────────────────

        void toggleSidebar()
        {
            showSidebar_ = !showSidebar_;
            recalcLayout();
        }

        void toggleBottomPanel()
        {
            showBottomPanel_ = !showBottomPanel_;
            recalcLayout();
        }

        bool sidebarVisible() const { return showSidebar_; }
        bool bottomPanelVisible() const { return showBottomPanel_; }

        // ── Event routing ───────────────────────────────────────────

        enum class LayoutAction
        {
            NONE,
            HANDLED,
            TOGGLE_SIDEBAR,
            TOGGLE_BOTTOM,
            CYCLE_FOCUS,
            QUIT,
            SAVE,
            SAVE_AS,
            OPEN_FILE,
            NEW_FILE,
            FIND,
            GOTO_LINE,
            COPY,
            CUT,
            PASTE,
        };

        LayoutAction handleEvent(const SDL_Event &event)
        {
            if (event.type == SDL_KEYDOWN)
            {
                auto key = event.key.keysym;
                bool ctrl = (key.mod & KMOD_CTRL) != 0;
                bool shift = (key.mod & KMOD_SHIFT) != 0;
                (void)shift;

                // Global shortcuts (regardless of focus)
                if (ctrl)
                {
                    switch (key.sym)
                    {
                    case SDLK_b:
                        toggleSidebar();
                        return LayoutAction::TOGGLE_SIDEBAR;
                    case SDLK_BACKQUOTE:
                        toggleBottomPanel();
                        return LayoutAction::TOGGLE_BOTTOM;
                    case SDLK_TAB:
                        cycleFocus();
                        return LayoutAction::CYCLE_FOCUS;
                    default:
                        break;
                    }
                }
            }

            // Route to focused region
            switch (focus_)
            {
            case FocusRegion::Sidebar:
                if (event.type == SDL_KEYDOWN || event.type == SDL_TEXTINPUT)
                {
                    if (fileTree_.handleKeyDown(event))
                        return LayoutAction::HANDLED;
                }
                break;

            case FocusRegion::Editor:
            {
                auto action = editor_.handleEvent(event);
                return mapEditorAction(action);
            }

            case FocusRegion::BottomPanel:
                // TODO: route to bottom panel when implemented
                break;
            }

            return LayoutAction::NONE;
        }

        // ── Rendering ───────────────────────────────────────────────

        struct ScreenOutput
        {
            std::vector<std::vector<Cell>> cells;
            int cursorRow = -1;
            int cursorCol = -1;
        };

        ScreenOutput render()
        {
            ScreenOutput out;
            out.cells.resize(totalRows_);
            for (auto &row : out.cells)
            {
                row.resize(totalCols_);
                for (auto &c : row)
                {
                    c.ch = U' ';
                    c.bg = bgColor_;
                    c.fg = fgColor_;
                    c.dirty = true;
                }
            }

            // ── Sidebar ──────────────────────────────────────────────
            if (showSidebar_)
            {
                auto sidebarCells = fileTree_.render();
                blitPanel(out.cells, sidebarCells, sidebarRect_);

                // Draw vertical border
                for (int r = 0; r < totalRows_; r++)
                {
                    int borderCol = sidebarWidth_;
                    if (borderCol < totalCols_)
                    {
                        out.cells[r][borderCol].ch = U'│';
                        out.cells[r][borderCol].fg = borderColor_;
                        out.cells[r][borderCol].bg = bgColor_;
                        out.cells[r][borderCol].dirty = true;
                    }
                }
            }

            // ── Editor ───────────────────────────────────────────────
            auto editorOut = editor_.render();
            blitPanel(out.cells, editorOut.cells, editorRect_);

            if (editorOut.cursorScreenRow >= 0 && focus_ == FocusRegion::Editor)
            {
                out.cursorRow = editorRect_.y + editorOut.cursorScreenRow;
                out.cursorCol = editorRect_.x + editorOut.cursorScreenCol;
            }

            // ── Bottom panel ─────────────────────────────────────────
            if (showBottomPanel_)
            {
                // Draw horizontal border
                int borderRow = bottomRect_.y - 1;
                if (borderRow >= 0 && borderRow < totalRows_)
                {
                    for (int c = (showSidebar_ ? sidebarWidth_ + 1 : 0); c < totalCols_; c++)
                    {
                        out.cells[borderRow][c].ch = U'─';
                        out.cells[borderRow][c].fg = borderColor_;
                        out.cells[borderRow][c].bg = bgColor_;
                        out.cells[borderRow][c].dirty = true;
                    }
                }

                // Bottom panel content (placeholder for now)
                renderBottomPanel(out.cells);
            }

            return out;
        }

        // ── Accessors ───────────────────────────────────────────────

        EditorWidget &editor() { return editor_; }
        FileTreePanel &fileTree() { return fileTree_; }
        const ThemeData &theme() const { return theme_; }

        // ── Clipboard interface (called by main loop) ───────────────

        std::string getClipboardText() const { return editor_.getClipboardText(); }
        void pasteText(const std::string &text) { editor_.pasteText(text); }
        void cut() { editor_.cut(); }

    private:
        const ThemeData &theme_;
        EditorWidget editor_;
        FileTreePanel fileTree_;

        int totalCols_ = 80;
        int totalRows_ = 24;

        FocusRegion focus_ = FocusRegion::Editor;

        bool showSidebar_ = true;
        bool showBottomPanel_ = false;
        int sidebarWidth_ = 28;
        int bottomPanelHeight_ = 10;

        // Computed rects
        Rect sidebarRect_ = {0, 0, 28, 24};
        Rect editorRect_ = {29, 0, 51, 24};
        Rect bottomRect_ = {29, 14, 51, 10};

        // Theme colors
        Color bgColor_ = {18, 18, 18};
        Color fgColor_ = {204, 204, 204};
        Color borderColor_ = {51, 51, 51};

        void loadColors()
        {
            bgColor_ = getUIColor(theme_, "editor_bg", bgColor_);
            fgColor_ = getUIColor(theme_, "editor_fg", fgColor_);
            borderColor_ = getUIColor(theme_, "panel_border", borderColor_);
        }

        void recalcLayout()
        {
            int contentStartCol = 0;
            int contentWidth = totalCols_;

            if (showSidebar_)
            {
                sidebarRect_ = {0, 0, sidebarWidth_, totalRows_};
                fileTree_.setRect(sidebarRect_);
                contentStartCol = sidebarWidth_ + 1; // +1 for border
                contentWidth = totalCols_ - contentStartCol;
            }

            int editorHeight = totalRows_;
            if (showBottomPanel_)
            {
                int bpHeight = std::min(bottomPanelHeight_, totalRows_ / 2);
                editorHeight = totalRows_ - bpHeight - 1; // -1 for border
                bottomRect_ = {contentStartCol, editorHeight + 1, contentWidth, bpHeight};
            }

            editorRect_ = {contentStartCol, 0, contentWidth, editorHeight};
            editor_.resize(contentWidth, editorHeight);
        }

        LayoutAction mapEditorAction(EditorAction action) const
        {
            switch (action)
            {
            case EditorAction::HANDLED:
                return LayoutAction::HANDLED;
            case EditorAction::SAVE:
                return LayoutAction::SAVE;
            case EditorAction::SAVE_AS:
                return LayoutAction::SAVE_AS;
            case EditorAction::OPEN_FILE:
                return LayoutAction::OPEN_FILE;
            case EditorAction::NEW_FILE:
                return LayoutAction::NEW_FILE;
            case EditorAction::FIND:
                return LayoutAction::FIND;
            case EditorAction::GOTO_LINE:
                return LayoutAction::GOTO_LINE;
            case EditorAction::COPY:
                return LayoutAction::COPY;
            case EditorAction::CUT:
                return LayoutAction::CUT;
            case EditorAction::PASTE:
                return LayoutAction::PASTE;
            case EditorAction::QUIT_EDITOR:
                return LayoutAction::QUIT;
            case EditorAction::TOGGLE_TERMINAL:
                return LayoutAction::TOGGLE_BOTTOM;
            default:
                return LayoutAction::NONE;
            }
        }

        void blitPanel(std::vector<std::vector<Cell>> &screen,
                        const std::vector<std::vector<Cell>> &panel,
                        const Rect &rect) const
        {
            for (int r = 0; r < (int)panel.size() && rect.y + r < totalRows_; r++)
            {
                for (int c = 0; c < (int)panel[r].size() && rect.x + c < totalCols_; c++)
                {
                    if (rect.y + r >= 0 && rect.x + c >= 0)
                        screen[rect.y + r][rect.x + c] = panel[r][c];
                }
            }
        }

        void renderBottomPanel(std::vector<std::vector<Cell>> &screen) const
        {
            // Minimal bottom panel: tab bar + empty content
            Color tabBg = {30, 30, 30};
            Color tabFg = {187, 187, 187};
            Color contentBg = {24, 24, 24};

            int startRow = bottomRect_.y;
            int startCol = bottomRect_.x;

            // Tab labels
            std::vector<std::string> tabs = {"TERMINAL", "OUTPUT", "DIAGNOSTICS"};
            if (startRow < totalRows_)
            {
                int col = startCol;
                for (int t = 0; t < (int)tabs.size(); t++)
                {
                    Color fg = (t == 0) ? Color::white() : tabFg;
                    Color bg = (t == 0) ? contentBg : tabBg;
                    std::string label = " " + tabs[t] + " ";
                    for (int i = 0; i < (int)label.size() && col < totalCols_; i++, col++)
                    {
                        screen[startRow][col].ch = (char32_t)label[i];
                        screen[startRow][col].fg = fg;
                        screen[startRow][col].bg = bg;
                        screen[startRow][col].bold = (t == 0);
                        screen[startRow][col].dirty = true;
                    }
                    if (col < totalCols_)
                    {
                        screen[startRow][col].ch = U'│';
                        screen[startRow][col].fg = borderColor_;
                        screen[startRow][col].bg = tabBg;
                        screen[startRow][col].dirty = true;
                        col++;
                    }
                }
                // Fill rest of tab bar
                for (; col < totalCols_; col++)
                {
                    screen[startRow][col].bg = tabBg;
                    screen[startRow][col].dirty = true;
                }
            }

            // Content area (empty for now — terminal integration comes in Phase 4)
            for (int r = startRow + 1; r < startRow + bottomRect_.h && r < totalRows_; r++)
            {
                for (int c = startCol; c < totalCols_; c++)
                {
                    screen[r][c].ch = U' ';
                    screen[r][c].bg = contentBg;
                    screen[r][c].dirty = true;
                }
            }
        }
    };

} // namespace xterm
