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
#include <fstream>
#include <unordered_map>
#include <chrono>
#include <SDL2/SDL.h>
#include "../terminal/types.hpp"
#include "../editor/editor_view.hpp"
#include "../editor/editor_widget.hpp"
#include "../theme/theme_loader.hpp"
#include "panel.hpp"
#include "file_tree.hpp"
#include "repl_panel.hpp"
#include "git_panel.hpp"
#include "visual_effects.hpp"
#include "config_manager.hpp"

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
              fileTree_(theme),
              replPanel_(theme),
              gitPanel_(theme)
        {
            loadColors();

            // Wire file tree to open files in editor
            fileTree_.setOnOpenFile([this](const std::string &path)
                                    { editor_.openFile(path);
                                     setFocus(FocusRegion::Editor); });

            // Wire git engine to git panel
            gitPanel_.setEngine(&gitEngine_);

            // Load config (applies defaults if no file exists)
            configManager_.load();
            configManager_.applyToEffects(effects_);
        }

        // ── Initialization ──────────────────────────────────────────

        void setProjectRoot(const std::string &path)
        {
            fileTree_.setRoot(path);
            gitEngine_.setWorkDir(path);
            gitPanel_.refresh();
        }

        // ── Window resize ───────────────────────────────────────────

        void resize(int cols, int rows)
        {
            totalCols_ = cols;
            totalRows_ = rows;
            recalcLayout();
        }

        // ── Cell size (for pixel→cell conversion of mouse events) ───

        void setCellSize(int w, int h)
        {
            cellW_ = w;
            cellH_ = h;
        }

        int cellW() const { return cellW_; }
        int cellH() const { return cellH_; }

        // ── Tick (called each frame for timers) ─────────────────────

        void tick()
        {
            auto now = Clock::now();
            auto msSinceEdit = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   now - lastEditTime_)
                                   .count();

            // Live lint — run after LINT_DELAY_MS of idle
            if (lintDirty_ && msSinceEdit >= LINT_DELAY_MS)
            {
                lintDirty_ = false;
                runLintOnBuffer();
            }

            // Autosave — after AUTOSAVE_DELAY_MS of idle, if no errors and file modified
            if (autoSavePending_ && msSinceEdit >= AUTOSAVE_DELAY_MS && !hasErrors_)
            {
                autoSavePending_ = false;
                if (editor_.activeTabIndex() >= 0 && editor_.hasUnsavedChanges())
                {
                    std::string path = editor_.getCurrentFilePath();
                    if (!path.empty())
                    {
                        editor_.save();
                        editor_.setStatusMessage("Auto-saved ✓");
                    }
                }
            }
        }

        // Call this when the editor buffer changes (key input, paste, etc.)
        void notifyBufferChanged()
        {
            lastEditTime_ = Clock::now();
            lintDirty_ = true;
            autoSavePending_ = true;
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
            RUN_SELECTION,      // Ctrl+Enter — run selected code in REPL
            RUN_FILE,           // Ctrl+Shift+B / Ctrl+R — run current file
            INLINE_EVAL,        // Ctrl+Shift+E — evaluate line inline
            GIT_DIFF,           // Ctrl+D — show diff view
            SWITCH_TO_TERMINAL, // Ctrl+T — switch to terminal mode
        };

        LayoutAction handleEvent(const SDL_Event &event)
        {
            if (event.type == SDL_KEYDOWN)
            {
                auto key = event.key.keysym;
                bool ctrl = (key.mod & KMOD_CTRL) != 0;
                bool shift = (key.mod & KMOD_SHIFT) != 0;

                // Global shortcuts (regardless of focus)
                if (ctrl)
                {
                    switch (key.sym)
                    {
                    case SDLK_b:
                        if (!shift)
                        {
                            toggleSidebar();
                            return LayoutAction::TOGGLE_SIDEBAR;
                        }
                        else
                        {
                            // Ctrl+Shift+B — Run file
                            handleRunFile();
                            return LayoutAction::RUN_FILE;
                        }
                    case SDLK_BACKQUOTE:
                        toggleBottomPanel();
                        return LayoutAction::TOGGLE_BOTTOM;
                    case SDLK_TAB:
                        cycleFocus();
                        return LayoutAction::CYCLE_FOCUS;
                    case SDLK_RETURN:
                        // Ctrl+Enter — Run selection in REPL
                        handleRunSelection();
                        return LayoutAction::RUN_SELECTION;
                    case SDLK_e:
                        if (shift)
                        {
                            // Ctrl+Shift+E — Inline eval
                            handleInlineEval();
                            return LayoutAction::INLINE_EVAL;
                        }
                        break;
                    case SDLK_d:
                        if (!shift)
                        {
                            // Ctrl+D — Git diff
                            return LayoutAction::GIT_DIFF;
                        }
                        break;
                    case SDLK_n:
                        if (!shift)
                        {
                            // Ctrl+N — New file in selected directory
                            handleNewFile();
                            return LayoutAction::NEW_FILE;
                        }
                        break;
                    case SDLK_r:
                        if (!shift)
                        {
                            // Ctrl+R — Run current file
                            handleRunFile();
                            return LayoutAction::RUN_FILE;
                        }
                        break;
                    case SDLK_t:
                        if (!shift)
                        {
                            // Ctrl+T — Switch to terminal mode
                            return LayoutAction::SWITCH_TO_TERMINAL;
                        }
                        break;
                    default:
                        break;
                    }
                }
            }

            // --- Mouse click: proper pixel→cell conversion, region routing ---
            if (event.type == SDL_MOUSEBUTTONDOWN)
            {
                int clickCol = event.button.x / cellW_;
                int clickRow = event.button.y / cellH_;

                if (event.button.button == SDL_BUTTON_LEFT)
                {
                    // Context menu takes priority if shown
                    if (showFileContextMenu_)
                    {
                        std::string action = handleContextMenuClick(clickRow, clickCol);
                        showFileContextMenu_ = false;
                        if (!action.empty())
                        {
                            executeContextMenuAction(action);
                            return LayoutAction::HANDLED;
                        }
                        // Clicked outside menu — just dismiss it
                        return LayoutAction::HANDLED;
                    }

                    // If sidebar is hidden, double-click at col 0 to reopen
                    if (!showSidebar_ && clickCol == 0 && event.button.clicks >= 2)
                    {
                        toggleSidebar();
                        return LayoutAction::TOGGLE_SIDEBAR;
                    }

                    // Check sidebar resize border (border column only)
                    if (showSidebar_ && clickCol == sidebarWidth_)
                    {
                        if (event.button.clicks >= 2)
                        {
                            // Double-click: toggle sidebar collapsed
                            toggleSidebar();
                            return LayoutAction::TOGGLE_SIDEBAR;
                        }
                        resizingSidebar_ = true;
                        return LayoutAction::HANDLED;
                    }

                    // Check bottom panel resize border (just the border row)
                    if (showBottomPanel_ && clickRow == bottomRect_.y - 1)
                    {
                        if (event.button.clicks >= 2)
                        {
                            // Double-click: toggle bottom panel
                            toggleBottomPanel();
                            return LayoutAction::TOGGLE_BOTTOM;
                        }
                        resizingBottom_ = true;
                        return LayoutAction::HANDLED;
                    }

                    // Route to sidebar
                    if (showSidebar_ && clickCol < sidebarWidth_)
                    {
                        setFocus(FocusRegion::Sidebar);
                        int localRow = clickRow - sidebarRect_.y;
                        int localCol = clickCol - sidebarRect_.x;
                        fileTree_.handleMouseClick(localRow, localCol, false);
                        return LayoutAction::HANDLED;
                    }

                    // Route to bottom panel
                    if (showBottomPanel_ && clickRow >= bottomRect_.y)
                    {
                        setFocus(FocusRegion::BottomPanel);
                        int localRow = clickRow - bottomRect_.y;
                        int localCol = clickCol - bottomRect_.x;

                        // Tab bar click (row 0 of bottom panel)
                        if (localRow == 0)
                        {
                            handleBottomTabClick(localCol);
                        }
                        else
                        {
                            replPanel_.handleMouseClick(localRow, localCol, (SDL_GetModState() & KMOD_SHIFT) != 0);
                        }
                        return LayoutAction::HANDLED;
                    }

                    // Route to editor region
                    if (clickCol >= editorRect_.x && clickRow >= editorRect_.y &&
                        clickRow < editorRect_.y + editorRect_.h)
                    {
                        setFocus(FocusRegion::Editor);
                        // Convert to editor-local cell coords
                        int editorRow = clickRow - editorRect_.y;
                        int editorCol = clickCol - editorRect_.x;
                        editor_.handleLocalClick(editorRow, editorCol, (SDL_GetModState() & KMOD_SHIFT) != 0);
                        return LayoutAction::HANDLED;
                    }
                }
                else if (event.button.button == SDL_BUTTON_RIGHT)
                {
                    // Right-click context menu for file tree
                    if (showSidebar_ && clickCol < sidebarWidth_)
                    {
                        int localRow = clickRow - sidebarRect_.y;
                        bool hit = fileTree_.selectAt(localRow);
                        showFileContextMenu_ = true;
                        contextMenuRow_ = clickRow;
                        contextMenuCol_ = clickCol;

                        if (hit)
                        {
                            // Determine if it's a file or folder
                            if (fileTree_.selectedIsDir())
                                contextTarget_ = ContextTarget::FOLDER;
                            else
                                contextTarget_ = ContextTarget::FILE;
                        }
                        else
                        {
                            contextTarget_ = ContextTarget::EMPTY;
                        }
                        return LayoutAction::HANDLED;
                    }
                    showFileContextMenu_ = false;
                }
            }

            // --- Mouse motion: resize drags and editor drag selection ---
            if (event.type == SDL_MOUSEMOTION)
            {
                int motionCol = event.motion.x / cellW_;
                int motionRow = event.motion.y / cellH_;

                // Track hover position for highlights
                hoverRow_ = motionRow;
                hoverCol_ = motionCol;

                // Update file tree hover
                if (showSidebar_ && motionCol < sidebarWidth_)
                {
                    int localRow = motionRow - sidebarRect_.y;
                    fileTree_.setHoverRow(localRow);
                }
                else
                {
                    fileTree_.setHoverRow(-1);
                }

                // Update editor tab hover
                if (motionRow == editorRect_.y && motionCol >= editorRect_.x)
                {
                    editor_.setHoverCol(motionCol - editorRect_.x);
                }
                else
                {
                    editor_.setHoverCol(-1);
                }

                // Update bottom tab hover
                if (showBottomPanel_ && motionRow == bottomRect_.y && motionCol >= bottomRect_.x)
                {
                    replPanel_.setHoverCol(motionCol - bottomRect_.x);
                }
                else
                {
                    replPanel_.setHoverCol(-1);
                }

                if (resizingSidebar_)
                {
                    sidebarWidth_ = std::clamp(motionCol, 12, totalCols_ / 2);
                    recalcLayout();
                    return LayoutAction::HANDLED;
                }

                if (resizingBottom_)
                {
                    int newBottomHeight = totalRows_ - motionRow - 1;
                    bottomPanelHeight_ = std::clamp(newBottomHeight, 4, totalRows_ * 2 / 3);
                    recalcLayout();
                    return LayoutAction::HANDLED;
                }

                // Drag selection in editor
                if ((event.motion.state & SDL_BUTTON_LMASK) && focus_ == FocusRegion::Editor)
                {
                    int editorRow = motionRow - editorRect_.y;
                    int editorCol = motionCol - editorRect_.x;
                    editor_.handleLocalDrag(editorRow, editorCol);
                    return LayoutAction::HANDLED;
                }
            }

            // --- Mouse button up: end resize drags ---
            if (event.type == SDL_MOUSEBUTTONUP)
            {
                resizingSidebar_ = false;
                resizingBottom_ = false;
            }

            // --- Mouse wheel: forward to focused widget ---
            if (event.type == SDL_MOUSEWHEEL)
            {
                switch (focus_)
                {
                case FocusRegion::Sidebar:
                    fileTree_.handleMouseWheel(event.wheel.y);
                    return LayoutAction::HANDLED;
                case FocusRegion::Editor:
                    editor_.handleLocalWheel(event.wheel.y);
                    return LayoutAction::HANDLED;
                case FocusRegion::BottomPanel:
                    replPanel_.handleMouseWheel(event.wheel.y);
                    return LayoutAction::HANDLED;
                }
            }

            // Route to focused region
            switch (focus_)
            {
            case FocusRegion::Sidebar:
                if (event.type == SDL_KEYDOWN)
                {
                    // If file tree is in inline edit mode, route keys there first
                    if (fileTree_.isEditing())
                    {
                        auto key = event.key.keysym.sym;
                        if (key == SDLK_RETURN)
                        {
                            auto mode = fileTree_.editMode();
                            std::string oldPath = fileTree_.selectedFilePath();
                            std::string result = fileTree_.commitEdit();
                            if (!result.empty())
                            {
                                if (mode == FileTreePanel::EditMode::RENAME)
                                {
                                    // If the renamed file was open, close old tab and reopen
                                    editor_.closeTabByPath(oldPath);
                                    if (!std::filesystem::is_directory(result))
                                        editor_.openFile(result);
                                }
                                else if (mode == FileTreePanel::EditMode::NEW_FILE)
                                {
                                    editor_.openFile(result);
                                }
                                // NEW_FOLDER: just refresh, no tab to open
                            }
                            return LayoutAction::HANDLED;
                        }
                        if (fileTree_.handleEditKey(event))
                            return LayoutAction::HANDLED;
                    }
                    if (fileTree_.handleKeyDown(event))
                        return LayoutAction::HANDLED;
                }
                if (event.type == SDL_TEXTINPUT)
                {
                    if (fileTree_.isEditing())
                    {
                        std::string text = event.text.text;
                        fileTree_.handleEditTextInput(text);
                        return LayoutAction::HANDLED;
                    }
                }
                break;

            case FocusRegion::Editor:
            {
                auto action = editor_.handleEvent(event);
                // Handle close tab internally
                if (action == EditorAction::CLOSE_TAB)
                {
                    editor_.closeTab();
                    return LayoutAction::HANDLED;
                }
                // Notify that the buffer may have changed (for live linting + autosave)
                if (action == EditorAction::HANDLED && event.type == SDL_KEYDOWN)
                    notifyBufferChanged();
                if (event.type == SDL_TEXTINPUT)
                    notifyBufferChanged();
                return mapEditorAction(action);
            }

            case FocusRegion::BottomPanel:
                if (event.type == SDL_KEYDOWN)
                {
                    if (replPanel_.handleKeyDown(event))
                        return LayoutAction::HANDLED;
                }
                if (event.type == SDL_TEXTINPUT)
                {
                    std::string text = event.text.text;
                    replPanel_.handleTextInput(text);
                    return LayoutAction::HANDLED;
                }
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

                // Render REPL panel content
                replPanel_.setRect(bottomRect_);
                auto panelCells = replPanel_.render();
                blitPanel(out.cells, panelCells, bottomRect_);
            }

            // ── File context menu overlay ────────────────────────────
            if (showFileContextMenu_)
            {
                renderContextMenu(out.cells);
            }

            return out;
        }

        // ── Accessors ───────────────────────────────────────────────

        EditorWidget &editor() { return editor_; }
        FileTreePanel &fileTree() { return fileTree_; }
        REPLPanel &replPanel() { return replPanel_; }
        GitPanel &gitPanel() { return gitPanel_; }
        GitEngine &gitEngine() { return gitEngine_; }
        VisualEffects &effects() { return effects_; }
        ConfigManager &configManager() { return configManager_; }
        const ThemeData &theme() const { return theme_; }

        // ── Clipboard interface (called by main loop) ───────────────

        std::string getClipboardText() const { return editor_.getClipboardText(); }
        void pasteText(const std::string &text) { editor_.pasteText(text); }
        void cut() { editor_.cut(); }

        // ── Bottom tab bar click handler ────────────────────────────

        void handleBottomTabClick(int localCol)
        {
            // Tab labels: " TERMINAL " " OUTPUT " " DIAGNOSTICS " " VARIABLES "
            // Each separated by │
            struct TabDef
            {
                std::string label;
                BottomTab tab;
            };
            std::vector<TabDef> tabs = {
                {" TERMINAL ", BottomTab::TERMINAL},
                {" OUTPUT ", BottomTab::OUTPUT},
                {" DIAGNOSTICS ", BottomTab::DIAGNOSTICS},
                {" VARIABLES ", BottomTab::VARIABLES},
            };

            int col = 0;
            for (auto &td : tabs)
            {
                int tabEnd = col + (int)td.label.size();
                if (localCol >= col && localCol < tabEnd)
                {
                    replPanel_.setActiveTab(td.tab);
                    return;
                }
                col = tabEnd + 1; // +1 for │ separator
            }
        }

        // ── Live linting (on buffer content via stdin) ──────────────

        void runLintOnBuffer()
        {
            if (editor_.activeTabIndex() < 0)
                return;

            // Only lint .xel/.xell files
            std::string filePath = editor_.getCurrentFilePath();
            if (!filePath.empty())
            {
                std::string ext = std::filesystem::path(filePath).extension().string();
                if (ext != ".xel" && ext != ".xell")
                    return;
            }

            // Get full buffer content from editor (unsaved, in-memory)
            std::string fullContent = editor_.getFullBufferText();
            if (fullContent.empty())
                return;

            // Skip if content hasn't changed
            if (fullContent == lastLintedContent_)
                return;
            lastLintedContent_ = fullContent;

            // Pipe buffer content directly to xell --check via stdin (no temp files)
            std::string xellBin = findXellBinary();
            int exitCode = 0;
            std::string output = captureCommandWithStdin(
                xellBin + " --check", fullContent, exitCode);

            // Parse output into diagnostic lines
            std::vector<std::string> lines;
            if (!output.empty())
            {
                std::string line;
                for (char c : output)
                {
                    if (c == '\n')
                    {
                        if (!line.empty())
                            lines.push_back(line);
                        line.clear();
                    }
                    else
                    {
                        line += c;
                    }
                }
                if (!line.empty())
                    lines.push_back(line);
            }

            if (exitCode == 0 && lines.empty())
            {
                lines.push_back("No errors found ✓");
                hasErrors_ = false;
            }
            else
            {
                hasErrors_ = !lines.empty();
            }

            replPanel_.setDiagnosticLines(lines);

            // Parse diagnostic line numbers for inline markers
            std::unordered_map<int, int> diagMap;
            for (auto &dl : lines)
            {
                int lineNum = -1;
                int severity = 0;

                size_t firstColon = dl.find(':');
                if (firstColon != std::string::npos && firstColon + 1 < dl.size())
                {
                    size_t secondColon = dl.find(':', firstColon + 1);
                    if (secondColon != std::string::npos)
                    {
                        std::string numStr = dl.substr(firstColon + 1, secondColon - firstColon - 1);
                        try
                        {
                            lineNum = std::stoi(numStr) - 1;
                        }
                        catch (...)
                        {
                        }
                    }
                }
                if (lineNum < 0)
                {
                    size_t pos = dl.find("Line ");
                    if (pos == std::string::npos)
                        pos = dl.find("line ");
                    if (pos != std::string::npos)
                    {
                        size_t start = pos + 5;
                        std::string numStr;
                        while (start < dl.size() && isdigit(dl[start]))
                            numStr += dl[start++];
                        if (!numStr.empty())
                        {
                            try
                            {
                                lineNum = std::stoi(numStr) - 1;
                            }
                            catch (...)
                            {
                            }
                        }
                    }
                }

                if (lineNum >= 0)
                {
                    if (dl.find("warning") != std::string::npos || dl.find("WARNING") != std::string::npos)
                        severity = 1;
                    diagMap[lineNum] = severity;
                }
            }

            if (!diagMap.empty())
                editor_.setEditorDiagnostics(diagMap);
            else
                editor_.clearEditorDiagnostics();
        }

        // ── Dynamic linting (on save) ────────────────────────────────

        void runLintOnCurrentFile()
        {
            if (editor_.activeTabIndex() < 0)
                return;

            auto info = editor_.getStatusInfo();
            std::string filePath;
            // Get the active file path from editor tabs
            if (editor_.activeTabIndex() >= 0 && editor_.activeTabIndex() < editor_.tabCount())
            {
                // We need access to the file path — let's use getCurrentFilePath
                filePath = editor_.getCurrentFilePath();
            }

            if (filePath.empty() || !std::filesystem::exists(filePath))
                return;

            // Only lint .xel files
            std::string ext = std::filesystem::path(filePath).extension().string();
            if (ext != ".xel" && ext != ".xell")
                return;

            // Use the shared findXellBinary from repl_panel.hpp
            std::string xellBin = findXellBinary();

            // Run xell --check <file> and capture output
            std::string cmd = xellBin + " --check \"" + filePath + "\"";
            int exitCode = 0;
            std::string output = captureCommand(cmd, exitCode);

            // Parse output into diagnostic lines
            std::vector<std::string> lines;
            if (!output.empty())
            {
                std::string line;
                for (char c : output)
                {
                    if (c == '\n')
                    {
                        if (!line.empty())
                            lines.push_back(line);
                        line.clear();
                    }
                    else
                    {
                        line += c;
                    }
                }
                if (!line.empty())
                    lines.push_back(line);
            }

            if (exitCode == 0 && lines.empty())
            {
                lines.push_back("No errors found ✓");
            }

            replPanel_.setDiagnosticLines(lines);

            // Parse diagnostic line numbers for inline markers
            // Format: "filename:line:col: error: message" or "Line N: ..."
            std::unordered_map<int, int> diagMap; // line (0-based) → severity
            for (auto &dl : lines)
            {
                // Try pattern: "filename:LINE:COL: error/warning: msg"
                // or "Line LINE: ..."
                int lineNum = -1;
                int severity = 0; // 0=error, 1=warning

                // Pattern 1: "something:N:N: error:"
                size_t firstColon = dl.find(':');
                if (firstColon != std::string::npos && firstColon + 1 < dl.size())
                {
                    size_t secondColon = dl.find(':', firstColon + 1);
                    if (secondColon != std::string::npos)
                    {
                        std::string numStr = dl.substr(firstColon + 1, secondColon - firstColon - 1);
                        try
                        {
                            lineNum = std::stoi(numStr) - 1; // convert to 0-based
                        }
                        catch (...)
                        {
                        }
                    }
                }

                // Pattern 2: "Line N:" or "line N:"
                if (lineNum < 0)
                {
                    size_t pos = dl.find("Line ");
                    if (pos == std::string::npos)
                        pos = dl.find("line ");
                    if (pos != std::string::npos)
                    {
                        size_t start = pos + 5;
                        std::string numStr;
                        while (start < dl.size() && isdigit(dl[start]))
                            numStr += dl[start++];
                        if (!numStr.empty())
                        {
                            try
                            {
                                lineNum = std::stoi(numStr) - 1;
                            }
                            catch (...)
                            {
                            }
                        }
                    }
                }

                if (lineNum >= 0)
                {
                    if (dl.find("warning") != std::string::npos)
                        severity = 1;
                    diagMap[lineNum] = severity;
                }
            }

            if (!diagMap.empty())
                editor_.setEditorDiagnostics(diagMap);
            else
                editor_.clearEditorDiagnostics();

            // Auto-show DIAGNOSTICS tab if there are errors
            if (exitCode != 0 && !lines.empty())
            {
                replPanel_.setActiveTab(BottomTab::DIAGNOSTICS);
                if (!showBottomPanel_)
                    toggleBottomPanel();
                editor_.setStatusMessage("⚠ " + std::to_string(lines.size()) + " diagnostic(s)");
            }
            else
            {
                editor_.setStatusMessage("✓ No lint errors");
            }
        }

    private:
        const ThemeData &theme_;
        EditorWidget editor_;
        FileTreePanel fileTree_;
        REPLPanel replPanel_;
        GitPanel gitPanel_;
        GitEngine gitEngine_;
        VisualEffects effects_;
        ConfigManager configManager_;

        int totalCols_ = 80;
        int totalRows_ = 24;
        int cellW_ = 8;  // pixel width of one cell
        int cellH_ = 16; // pixel height of one cell

        FocusRegion focus_ = FocusRegion::Editor;

        bool showSidebar_ = true;
        bool showBottomPanel_ = true;
        int sidebarWidth_ = 28;
        int bottomPanelHeight_ = 10;

        // Computed rects
        Rect sidebarRect_ = {0, 0, 28, 24};
        Rect editorRect_ = {29, 0, 51, 24};
        Rect bottomRect_ = {29, 14, 51, 10};

        // Resize drag state
        bool resizingSidebar_ = false;
        bool resizingBottom_ = false;

        // Live linting state
        using Clock = std::chrono::steady_clock;
        Clock::time_point lastEditTime_ = Clock::now();
        Clock::time_point lastAutoSaveTime_ = Clock::now();
        bool lintDirty_ = false;
        bool hasErrors_ = false;
        bool autoSavePending_ = false;
        std::string lastLintedContent_; // to avoid re-linting same content
        static constexpr int LINT_DELAY_MS = 500;
        static constexpr int AUTOSAVE_DELAY_MS = 2000;

        // File context menu state
        bool showFileContextMenu_ = false;
        int contextMenuRow_ = 0;
        int contextMenuCol_ = 0;

        // Mouse hover state (cell coordinates)
        int hoverRow_ = -1;
        int hoverCol_ = -1;

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

        // ── Context menu definitions ────────────────────────────────

        static constexpr int CONTEXT_MENU_WIDTH = 22;

        enum class ContextTarget
        {
            FILE,
            FOLDER,
            EMPTY
        };
        mutable ContextTarget contextTarget_ = ContextTarget::EMPTY;

        std::vector<std::string> contextMenuItems() const
        {
            switch (contextTarget_)
            {
            case ContextTarget::FILE:
                return {"  Open",
                        "  ─────────────────",
                        "  Rename",
                        "  Delete",
                        "  ─────────────────",
                        "  Copy Path",
                        "  Copy Relative Path"};
            case ContextTarget::FOLDER:
                return {"  New File",
                        "  New Folder",
                        "  ─────────────────",
                        "  Rename",
                        "  Delete",
                        "  ─────────────────",
                        "  Copy Path",
                        "  Copy Relative Path"};
            case ContextTarget::EMPTY:
            default:
                return {"  New File",
                        "  New Folder"};
            }
        }

        void renderContextMenu(std::vector<std::vector<Cell>> &cells) const
        {
            auto items = contextMenuItems();
            int menuH = (int)items.size();
            int startRow = contextMenuRow_;
            int startCol = contextMenuCol_;

            // Clamp to screen bounds
            if (startRow + menuH >= totalRows_)
                startRow = totalRows_ - menuH - 1;
            if (startCol + CONTEXT_MENU_WIDTH >= totalCols_)
                startCol = totalCols_ - CONTEXT_MENU_WIDTH - 1;
            startRow = std::max(0, startRow);
            startCol = std::max(0, startCol);

            Color menuBg = {45, 45, 45};
            Color menuFg = {220, 220, 220};
            Color menuBorder = {80, 80, 80};
            Color sepFg = {80, 80, 80};

            for (int i = 0; i < menuH; i++)
            {
                int r = startRow + i;
                if (r >= totalRows_)
                    break;
                bool isSep = (items[i].find("───") != std::string::npos);
                Color fg = isSep ? sepFg : menuFg;

                // Fill background first
                for (int c = 0; c < CONTEXT_MENU_WIDTH && startCol + c < totalCols_; c++)
                {
                    cells[r][startCol + c].bg = menuBg;
                    cells[r][startCol + c].fg = fg;
                    cells[r][startCol + c].ch = U' ';
                    cells[r][startCol + c].dirty = true;
                }
                // Write text with UTF-8 decode
                utf8Write(cells[r], startCol, items[i], fg, menuBg);
                // Right border
                if (startCol + CONTEXT_MENU_WIDTH < totalCols_)
                {
                    auto &cell = cells[r][startCol + CONTEXT_MENU_WIDTH];
                    cell.ch = U'│';
                    cell.fg = menuBorder;
                    cell.bg = menuBg;
                    cell.dirty = true;
                }
            }
            // Bottom border
            int bottomR = startRow + menuH;
            if (bottomR < totalRows_)
            {
                for (int c = 0; c <= CONTEXT_MENU_WIDTH && startCol + c < totalCols_; c++)
                {
                    auto &cell = cells[bottomR][startCol + c];
                    cell.ch = U'─';
                    cell.fg = menuBorder;
                    cell.bg = menuBg;
                    cell.dirty = true;
                }
            }
        }

        // Handle context menu item click (returns action string, empty = no hit)
        std::string handleContextMenuClick(int clickRow, int clickCol)
        {
            auto items = contextMenuItems();
            int menuH = (int)items.size();
            int startRow = contextMenuRow_;
            int startCol = contextMenuCol_;
            if (startRow + menuH >= totalRows_)
                startRow = totalRows_ - menuH - 1;
            if (startCol + CONTEXT_MENU_WIDTH >= totalCols_)
                startCol = totalCols_ - CONTEXT_MENU_WIDTH - 1;
            startRow = std::max(0, startRow);
            startCol = std::max(0, startCol);

            if (clickRow >= startRow && clickRow < startRow + menuH &&
                clickCol >= startCol && clickCol < startCol + CONTEXT_MENU_WIDTH)
            {
                int idx = clickRow - startRow;
                if (items[idx].find("───") != std::string::npos)
                    return ""; // separator
                // Trim leading spaces
                std::string item = items[idx];
                while (!item.empty() && item[0] == ' ')
                    item.erase(item.begin());
                return item;
            }
            return "";
        }

        void executeContextMenuAction(const std::string &action)
        {
            std::string path = fileTree_.selectedFilePath();

            namespace fs = std::filesystem;
            if (action == "Open")
            {
                if (!path.empty() && !fs::is_directory(path))
                {
                    editor_.openFile(path);
                    setFocus(FocusRegion::Editor);
                }
            }
            else if (action == "New File")
            {
                fileTree_.startNewFile();
                setFocus(FocusRegion::Sidebar);
            }
            else if (action == "New Folder")
            {
                fileTree_.startNewFolder();
                setFocus(FocusRegion::Sidebar);
            }
            else if (action == "Delete")
            {
                try
                {
                    // Close any open tab for this file before deleting
                    editor_.closeTabByPath(path);
                    fs::remove_all(path);
                    fileTree_.refresh();
                }
                catch (...)
                {
                }
            }
            else if (action == "Rename")
            {
                fileTree_.startRename();
                setFocus(FocusRegion::Sidebar);
            }
            else if (action == "Copy Path")
            {
                SDL_SetClipboardText(path.c_str());
                editor_.setStatusMessage("Copied: " + path);
            }
            else if (action == "Copy Relative Path")
            {
                std::string root = fileTree_.rootPath();
                std::string rel = path;
                if (path.size() > root.size() && path.substr(0, root.size()) == root)
                    rel = path.substr(root.size() + 1);
                SDL_SetClipboardText(rel.c_str());
                editor_.setStatusMessage("Copied: " + rel);
            }
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

        LayoutAction mapEditorAction(EditorAction action)
        {
            switch (action)
            {
            case EditorAction::HANDLED:
                return LayoutAction::HANDLED;
            case EditorAction::SAVE:
            {
                // Run save then lint
                editor_.save();
                runLintOnCurrentFile();
                return LayoutAction::HANDLED; // save is handled internally now
            }
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

        // ── New file helper ───────────────────────────────────────

        void handleNewFile()
        {
            // Open sidebar and start inline name prompt
            if (!showSidebar_)
                toggleSidebar();
            fileTree_.startNewFile();
            setFocus(FocusRegion::Sidebar);
        }

        // ── REPL integration helpers ─────────────────────────────────

        void handleRunSelection()
        {
            if (editor_.activeTabIndex() < 0)
                return;

            // Run all code from the beginning of the file to the cursor line (inclusive)
            std::string code = editor_.getTextToCursorLine();
            if (code.empty())
                return;

            // Show bottom panel if hidden
            if (!showBottomPanel_)
            {
                showBottomPanel_ = true;
                recalcLayout();
            }

            // Send to REPL — show results in OUTPUT tab
            replPanel_.setActiveTab(BottomTab::OUTPUT);
            replPanel_.runCode(code);
        }

        void handleRunFile()
        {
            if (editor_.activeTabIndex() < 0)
                return;

            std::string fullPath = editor_.getCurrentFilePath();
            if (fullPath.empty())
            {
                editor_.setStatusMessage("Save file first to run");
                return;
            }

            // Show bottom panel
            if (!showBottomPanel_)
            {
                showBottomPanel_ = true;
                recalcLayout();
            }

            // Run the file using its full path
            replPanel_.runFile(fullPath);
        }

        void handleInlineEval()
        {
            if (editor_.activeTabIndex() < 0)
                return;

            std::string code = editor_.getClipboardText();
            if (code.empty())
                return;

            // Strip trailing newline for single-line eval
            while (!code.empty() && (code.back() == '\n' || code.back() == '\r'))
                code.pop_back();

            std::string result = replPanel_.evalInline(code);
            if (!result.empty())
            {
                auto cursor = editor_.getStatusInfo();
                replPanel_.setGhostText(cursor.cursorRow - 1, "  # → " + result);
                editor_.setStatusMessage("= " + result);
            }
        }
    };

} // namespace xterm
