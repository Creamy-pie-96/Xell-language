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
#include <regex>
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
#include "autocomplete.hpp"
#include "dashboard_panel.hpp"
#include "debug_client.hpp"

namespace xterm
{

    // ─── Focus regions ───────────────────────────────────────────────────

    enum class FocusRegion
    {
        Sidebar,
        Editor,
        BottomPanel,
        Dashboard,
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
              dashboardPanel_(theme),
              gitPanel_(theme),
              acPopup_(theme)
        {
            loadColors();

            // Wire file tree to open files in editor
            fileTree_.setOnOpenFile([this](const std::string &path)
                                    { editor_.openFile(path);
                                     setFocus(FocusRegion::Editor); });

            // Wire git engine to git panel
            gitPanel_.setEngine(&gitEngine_);

            // Wire dashboard panel to jump to line in editor
            dashboardPanel_.setOnJumpToLine([this](int line)
                                            { editor_.goToLine(line - 1); // symbols are 1-based, goToLine is 0-based
                                              setFocus(FocusRegion::Editor); });

            // Wire dashboard panel to open imported file at a specific line
            dashboardPanel_.setOnOpenFileAtLine([this](const std::string &filePath, int line)
                                                { editor_.openFile(filePath);
                                                  editor_.goToLine(line > 0 ? line - 1 : 0);
                                                  setFocus(FocusRegion::Editor); });

            // Wire variables tab click to jump to variable definition
            replPanel_.setOnJumpToLine([this](int line)
                                       { editor_.goToLine(line - 1); // lines from symbols are 1-based
                                         setFocus(FocusRegion::Editor); });

            // Wire variables/objects tab to open cross-file symbols
            replPanel_.setOnOpenFileAtLine([this](const std::string &filePath, int line)
                                           { editor_.openFile(filePath);
                                             editor_.goToLine(line > 0 ? line - 1 : 0);
                                             setFocus(FocusRegion::Editor); });

            // Wire lifecycle provider: lazy lookup from trace cache
            replPanel_.setLifecycleProvider([this](const std::string &varName)
                                                -> std::vector<std::string>
                                            { return lifecycleLookup(varName); });

            // Wire dashboard lifecycle provider too
            dashboardPanel_.setLifecycleProvider([this](const std::string &varName)
                                                     -> std::vector<std::string>
                                                 { return lifecycleLookup(varName); });
            // Load config (applies defaults if no file exists)
            configManager_.load();
            configManager_.applyToEffects(effects_);

            // Load autocomplete data (language_data.json + snippets)
            {
                std::string langPath = resolveAssetPath("language_data.json");
                std::string snipPath = resolveAssetPath("xell_snippets.json");
                std::cerr << "[autocomplete] language_data: " << langPath << "\n";
                std::cerr << "[autocomplete] snippets:      " << snipPath << "\n";
                acDB_.loadFromJSON(langPath);
                snippetEngine_.loadSnippets(snipPath);
                acDB_.loadSnippets(snippetEngine_);
                std::cerr << "[autocomplete] loaded " << snippetEngine_.snippets().size()
                          << " snippets\n";
            }
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

            // Poll for async code execution results
            replPanel_.pollAsyncResult();

            // Autocomplete debounce: auto-show popup after typing delay
            if (acPendingShow_ && focus_ == FocusRegion::Editor)
            {
                auto msSinceKey = std::chrono::duration_cast<std::chrono::milliseconds>(
                                      now - acLastKeyTime_)
                                      .count();
                if (msSinceKey >= AC_DEBOUNCE_MS)
                {
                    acPendingShow_ = false;
                    if (!acModuleName_.empty() || (int)acPrefix_.size() >= AC_MIN_PREFIX)
                    {
                        showAutocomplete();
                    }
                }
            }

            // Poll debug session for state updates
            pollDebugSession();

            // Check if editor had a gutter breakpoint toggle
            int toggledBP = editor_.consumeBreakpointToggle();
            if (toggledBP >= 0 && debugState_.active)
            {
                auto bps = editor_.breakpoints();
                if (bps && bps->count(toggledBP))
                    debugClient_.sendAddBreakpoint(toggledBP + 1); // 1-based for interpreter
                else
                    debugClient_.sendRemoveBreakpoint(toggledBP + 1);
            }
        }

        // Call this when the editor buffer changes (key input, paste, etc.)
        void notifyBufferChanged()
        {
            lastEditTime_ = Clock::now();
            lintDirty_ = true;
            autoSavePending_ = true;

            // Update autocomplete prefix and trigger debounce
            if (focus_ == FocusRegion::Editor)
            {
                updateAcPrefix();
                acLastKeyTime_ = Clock::now();

                if (acPopup_.isVisible())
                {
                    // Already showing — update filter immediately
                    if (acPrefix_.empty() && acModuleName_.empty())
                        dismissAutocomplete();
                    else if (!acModuleName_.empty() && acDB_.hasModuleMembers(acModuleName_))
                    {
                        std::vector<CompletionItem> members;
                        if (acModuleName_ == "__any__")
                            members = acDB_.matchAnyMembers(acPrefix_);
                        else
                            members = acDB_.matchModuleMembers(acModuleName_, acPrefix_);
                        acPopup_.updateFilterItems(acPrefix_, members);
                        updateGhostText();
                    }
                    else
                    {
                        acPopup_.updateFilter(acPrefix_, acDB_);
                        updateGhostText();
                    }
                }
                else if (!acModuleName_.empty() && acDB_.hasModuleMembers(acModuleName_))
                {
                    // -> was typed — immediately show members
                    acPendingShow_ = true;
                    acLastKeyTime_ = Clock::now();
                }
                else if ((int)acPrefix_.size() >= AC_MIN_PREFIX)
                {
                    // Start debounce for auto-show
                    acPendingShow_ = true;
                }
                else
                {
                    // Too short — dismiss ghost
                    editor_.clearGhostText();
                }
            }
        }

        // ── Focus management ────────────────────────────────────────

        FocusRegion focus() const { return focus_; }

        void setFocus(FocusRegion region)
        {
            focus_ = region;
            fileTree_.setFocused(region == FocusRegion::Sidebar);
            // Dismiss autocomplete when leaving editor
            if (region != FocusRegion::Editor)
            {
                if (acPopup_.isVisible())
                    dismissAutocomplete();
                if (activeSnippet_.active)
                    commitSnippet();
            }
        }

        void cycleFocus()
        {
            switch (focus_)
            {
            case FocusRegion::Sidebar:
                setFocus(FocusRegion::Editor);
                break;
            case FocusRegion::Editor:
                if (showDashboard_)
                    setFocus(FocusRegion::Dashboard);
                else if (showBottomPanel_)
                    setFocus(FocusRegion::BottomPanel);
                else
                    setFocus(FocusRegion::Sidebar);
                break;
            case FocusRegion::Dashboard:
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

        void toggleDashboard()
        {
            showDashboard_ = !showDashboard_;
            recalcLayout();
        }

        bool sidebarVisible() const { return showSidebar_; }
        bool bottomPanelVisible() const { return showBottomPanel_; }
        bool dashboardVisible() const { return showDashboard_; }

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
            // ── Goto-line dialog intercept ───────────────────────────
            // ── Goto-line dialog intercept ───────────────────────────
            if (showGotoLine_)
            {
                if (event.type == SDL_KEYDOWN)
                {
                    auto key = event.key.keysym;
                    if (key.sym == SDLK_ESCAPE)
                    {
                        showGotoLine_ = false;
                    }
                    else if (key.sym == SDLK_RETURN || key.sym == SDLK_KP_ENTER)
                    {
                        if (!gotoLineInput_.empty())
                        {
                            try
                            {
                                int line = std::stoi(gotoLineInput_) - 1; // 0-based
                                editor_.goToLine(line);
                            }
                            catch (...)
                            {
                            }
                        }
                        showGotoLine_ = false;
                    }
                    else if (key.sym == SDLK_BACKSPACE)
                    {
                        if (!gotoLineInput_.empty() && gotoLineCursor_ > 0)
                        {
                            gotoLineInput_.erase(gotoLineCursor_ - 1, 1);
                            gotoLineCursor_--;
                        }
                    }
                    return LayoutAction::HANDLED; // keyboard consumed by dialog
                }
                else if (event.type == SDL_TEXTINPUT)
                {
                    std::string text = event.text.text;
                    // Only allow digits
                    for (char c : text)
                    {
                        if (c >= '0' && c <= '9')
                        {
                            gotoLineInput_.insert(gotoLineCursor_, 1, c);
                            gotoLineCursor_++;
                        }
                    }
                    return LayoutAction::HANDLED; // text input consumed by dialog
                }
                // NOTE: Mouse events fall through so clicks can dismiss the dialog
            }

            // ── Find/Replace dialog intercept ────────────────────────
            if (findReplaceActive_)
            {
                if (event.type == SDL_KEYDOWN)
                {
                    auto key = event.key.keysym;
                    if (key.sym == SDLK_ESCAPE)
                    {
                        findReplaceActive_ = false;
                    }
                    else if (key.sym == SDLK_RETURN || key.sym == SDLK_KP_ENTER)
                    {
                        if (findReplaceMode_) // replace mode
                            doReplace();
                        else
                            findNext();
                    }
                    else if (key.sym == SDLK_BACKSPACE)
                    {
                        if (findReplaceEditingReplace_)
                        {
                            if (!replaceInput_.empty() && replaceCursor_ > 0)
                            {
                                replaceInput_.erase(replaceCursor_ - 1, 1);
                                replaceCursor_--;
                            }
                        }
                        else
                        {
                            if (!findInput_.empty() && findCursor_ > 0)
                            {
                                findInput_.erase(findCursor_ - 1, 1);
                                findCursor_--;
                                updateFindMatches();
                            }
                        }
                    }
                    else if (key.sym == SDLK_TAB)
                    {
                        if (findReplaceMode_)
                            findReplaceEditingReplace_ = !findReplaceEditingReplace_;
                    }
                    else if (key.sym == SDLK_F3 || (key.sym == SDLK_g && (key.mod & KMOD_CTRL)))
                    {
                        if ((key.mod & KMOD_SHIFT))
                            findPrev();
                        else
                            findNext();
                    }
                    bool ctrl = (key.mod & KMOD_CTRL) != 0;
                    if (ctrl && key.sym == SDLK_h && !findReplaceMode_)
                    {
                        findReplaceMode_ = true; // switch to replace mode
                    }
                    return LayoutAction::HANDLED; // keyboard consumed by dialog
                }
                else if (event.type == SDL_TEXTINPUT)
                {
                    std::string text = event.text.text;
                    if (findReplaceEditingReplace_)
                    {
                        replaceInput_.insert(replaceCursor_, text);
                        replaceCursor_ += (int)text.size();
                    }
                    else
                    {
                        findInput_.insert(findCursor_, text);
                        findCursor_ += (int)text.size();
                        updateFindMatches();
                    }
                    return LayoutAction::HANDLED; // text input consumed by dialog
                }
                // NOTE: Mouse events fall through to the mouse handler below
                // so dialog buttons and input fields can be clicked
            }

            // ── Autocomplete popup intercept ─────────────────────────
            if (focus_ == FocusRegion::Editor && (acPopup_.isVisible() || activeSnippet_.active))
            {
                if (event.type == SDL_KEYDOWN)
                {
                    auto key = event.key.keysym;
                    bool ctrl = (key.mod & KMOD_CTRL) != 0;
                    bool shift = (key.mod & KMOD_SHIFT) != 0;

                    if (acPopup_.isVisible())
                    {
                        switch (key.sym)
                        {
                        case SDLK_UP:
                            acPopup_.moveUp();
                            updateGhostText();
                            return LayoutAction::HANDLED;
                        case SDLK_DOWN:
                            acPopup_.moveDown();
                            updateGhostText();
                            return LayoutAction::HANDLED;
                        case SDLK_RETURN:
                        case SDLK_KP_ENTER:
                            acceptCompletion();
                            return LayoutAction::HANDLED;
                        case SDLK_TAB:
                            if (!shift)
                            {
                                acceptCompletion();
                                return LayoutAction::HANDLED;
                            }
                            break;
                        case SDLK_ESCAPE:
                            dismissAutocomplete();
                            return LayoutAction::HANDLED;
                        default:
                            break;
                        }
                    }
                    else if (activeSnippet_.active)
                    {
                        // Tab navigation within active snippet
                        if (key.sym == SDLK_TAB && !ctrl)
                        {
                            if (shift)
                                snippetPrevStop();
                            else
                                snippetNextStop();
                            return LayoutAction::HANDLED;
                        }
                        if (key.sym == SDLK_ESCAPE)
                        {
                            commitSnippet();
                            return LayoutAction::HANDLED;
                        }
                    }
                }
                // Let text input fall through (for typing while popup is visible)
            }

            // ── Ghost text Tab accept (no popup, no snippet, just ghost) ─
            if (focus_ == FocusRegion::Editor && !acPopup_.isVisible() && !activeSnippet_.active)
            {
                if (event.type == SDL_KEYDOWN)
                {
                    auto key = event.key.keysym;
                    bool shift = (key.mod & KMOD_SHIFT) != 0;

                    // Tab accepts ghost text when visible
                    if (key.sym == SDLK_TAB && !shift && editor_.hasGhostText())
                    {
                        acceptGhostText();
                        return LayoutAction::HANDLED;
                    }
                }
            }

            if (event.type == SDL_KEYDOWN)
            {
                auto key = event.key.keysym;
                bool ctrl = (key.mod & KMOD_CTRL) != 0;
                bool shift = (key.mod & KMOD_SHIFT) != 0;

                // ── Debug function keys (F5/F9/F10/F11/F12) ─────────
                switch (key.sym)
                {
                case SDLK_F5:
                    if (debugState_.active && debugState_.paused)
                    {
                        // Continue execution
                        debugClient_.sendContinue();
                        editor_.setStatusMessage("▶ Continue");
                    }
                    else if (!debugState_.active)
                    {
                        // Start debug session
                        handleDebugLaunch();
                    }
                    return LayoutAction::HANDLED;

                case SDLK_F9:
                {
                    // Toggle breakpoint on current line
                    int row = editor_.cursorRow();
                    if (row >= 0)
                    {
                        editor_.toggleBreakpoint(row);
                        // If debug session is active, sync to interpreter
                        if (debugState_.active)
                        {
                            auto bps = editor_.breakpoints();
                            if (bps && bps->count(row))
                                debugClient_.sendAddBreakpoint(row + 1); // 1-based for interpreter
                            else
                                debugClient_.sendRemoveBreakpoint(row + 1);
                        }
                        editor_.setStatusMessage("Breakpoint toggled at line " + std::to_string(row + 1));
                    }
                    return LayoutAction::HANDLED;
                }

                case SDLK_F10:
                    if (debugState_.active && debugState_.paused)
                    {
                        debugClient_.sendStepOver();
                        editor_.setStatusMessage("⤳ Step Over");
                    }
                    return LayoutAction::HANDLED;

                case SDLK_F11:
                    if (debugState_.active && debugState_.paused)
                    {
                        if (shift)
                        {
                            debugClient_.sendStepOut();
                            editor_.setStatusMessage("⤴ Step Out");
                        }
                        else
                        {
                            debugClient_.sendStepInto();
                            editor_.setStatusMessage("⤵ Step Into");
                        }
                    }
                    return LayoutAction::HANDLED;

                case SDLK_F12:
                    if (debugState_.active)
                    {
                        // Stop debug session
                        debugClient_.sendStop();
                        endDebugSession();
                        editor_.setStatusMessage("⏹ Debug Stopped");
                    }
                    return LayoutAction::HANDLED;

                default:
                    break;
                }

                // Global shortcuts (regardless of focus)
                if (ctrl)
                {
                    switch (key.sym)
                    {
                    case SDLK_SPACE:
                        if (!shift)
                        {
                            // Ctrl+Space — Force autocomplete
                            acManualTrigger_ = true;
                            updateAcPrefix();
                            showAutocomplete();
                            return LayoutAction::HANDLED;
                        }
                        break;
                    case SDLK_b:
                        if (!shift)
                        {
                            toggleSidebar();
                            return LayoutAction::TOGGLE_SIDEBAR;
                        }
                        break;
                    case SDLK_BACKQUOTE:
                        toggleBottomPanel();
                        return LayoutAction::TOGGLE_BOTTOM;
                    case SDLK_TAB:
                        if (shift)
                            editor_.prevTab();
                        else
                            editor_.nextTab();
                        return LayoutAction::HANDLED;
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
                            // Ctrl+D — Debug Run (selection/cursor with full lifecycle+debug)
                            handleDebugRun();
                            return LayoutAction::RUN_SELECTION;
                        }
                        else
                        {
                            // Ctrl+Shift+D — Toggle dashboard panel
                            toggleDashboard();
                            return LayoutAction::HANDLED;
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
                    case SDLK_k:
                        if (shift)
                        {
                            // Ctrl+Shift+K — Emergency stop running program
                            if (killRunningProcess())
                            {
                                editor_.setStatusMessage("⚠ Terminated with emergency key");
                                replPanel_.appendOutputLine("⚠ Terminated with emergency key (Ctrl+Shift+K)", REPLLine::ERROR);
                            }
                            else
                            {
                                editor_.setStatusMessage("No running process to stop");
                            }
                            return LayoutAction::HANDLED;
                        }
                        break;
                    case SDLK_q:
                        if (shift)
                        {
                            // Ctrl+Shift+Q — Alternative emergency stop
                            if (killRunningProcess())
                            {
                                editor_.setStatusMessage("⚠ Terminated with emergency key");
                                replPanel_.appendOutputLine("⚠ Terminated with emergency key (Ctrl+Shift+Q)", REPLLine::ERROR);
                            }
                            else
                            {
                                editor_.setStatusMessage("No running process to stop");
                            }
                            return LayoutAction::HANDLED;
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
                    // ── Goto-line dialog click ───────────────────────
                    if (showGotoLine_)
                    {
                        // Click outside the dialog → dismiss
                        int barRow = editorRect_.y;
                        int barStart = editorRect_.x;
                        int barWidth = std::min(40, editorRect_.w);
                        if (clickRow != barRow || clickCol < barStart || clickCol >= barStart + barWidth)
                        {
                            showGotoLine_ = false;
                        }
                        return LayoutAction::HANDLED;
                    }

                    // ── Find/Replace dialog click ────────────────────
                    if (findReplaceActive_)
                    {
                        // Check if click is on the dialog rows
                        if (clickRow == findDialogRow_)
                        {
                            // Clicked on find row — check buttons
                            if (findCloseBtnCol_ >= 0 && std::abs(clickCol - findCloseBtnCol_) <= 1)
                            {
                                findReplaceActive_ = false;
                            }
                            else if (findPrevBtnCol_ >= 0 && std::abs(clickCol - findPrevBtnCol_) <= 1)
                            {
                                findPrev();
                            }
                            else if (findNextBtnCol_ >= 0 && std::abs(clickCol - findNextBtnCol_) <= 1)
                            {
                                findNext();
                            }
                            else if (clickCol >= findDialogStartCol_ && clickCol < findDialogStartCol_ + findDialogWidth_)
                            {
                                // Clicked in the find input area — focus the find field
                                findReplaceEditingReplace_ = false;
                            }
                            return LayoutAction::HANDLED;
                        }
                        else if (findReplaceMode_ && clickRow == replaceDialogRow_)
                        {
                            // Clicked on replace row — check Repl/All buttons
                            if (replaceBtnCol_ >= 0 && clickCol >= replaceBtnCol_ && clickCol < replaceBtnCol_ + 4)
                            {
                                doReplace(); // Single replace
                            }
                            else if (replaceAllBtnCol_ >= 0 && clickCol >= replaceAllBtnCol_ && clickCol < replaceAllBtnCol_ + 4)
                            {
                                doReplaceAll(); // Replace all
                            }
                            else if (clickCol >= findDialogStartCol_ && clickCol < findDialogStartCol_ + findDialogWidth_)
                            {
                                // Clicked in the replace input area — focus the replace field
                                findReplaceEditingReplace_ = true;
                            }
                            return LayoutAction::HANDLED;
                        }
                        else
                        {
                            // Clicked outside dialog → dismiss
                            findReplaceActive_ = false;
                            return LayoutAction::HANDLED;
                        }
                    }

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

                    // Cancel file tree inline edit if clicking outside the sidebar
                    if (fileTree_.isEditing() && !(showSidebar_ && clickCol < sidebarWidth_))
                    {
                        fileTree_.cancelEdit();
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

                    // Check dashboard resize border (the border column left of dashboard)
                    if (showDashboard_ && clickCol == dashboardRect_.x - 1)
                    {
                        if (event.button.clicks >= 2)
                        {
                            // Double-click: toggle dashboard
                            toggleDashboard();
                            return LayoutAction::HANDLED;
                        }
                        resizingDashboard_ = true;
                        return LayoutAction::HANDLED;
                    }

                    // Route to dashboard panel (right side)
                    if (showDashboard_ && clickCol >= dashboardRect_.x && clickRow < dashboardRect_.y + dashboardRect_.h)
                    {
                        setFocus(FocusRegion::Dashboard);
                        int localRow = clickRow - dashboardRect_.y;
                        int localCol = clickCol - dashboardRect_.x;
                        dashboardPanel_.handleMouseClick(localRow, localCol, false);
                        return LayoutAction::HANDLED;
                    }

                    // Route to editor region
                    if (clickCol >= editorRect_.x && clickRow >= editorRect_.y &&
                        clickRow < editorRect_.y + editorRect_.h)
                    {
                        setFocus(FocusRegion::Editor);
                        // Dismiss autocomplete on mouse click
                        if (acPopup_.isVisible())
                            dismissAutocomplete();
                        if (activeSnippet_.active)
                            commitSnippet();
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

                // Context menu hover tracking
                if (showFileContextMenu_)
                {
                    auto items = contextMenuItems();
                    int menuH = (int)items.size();
                    int menuStartRow = contextMenuRow_;
                    int menuStartCol = contextMenuCol_;
                    if (menuStartRow + menuH >= totalRows_)
                        menuStartRow = totalRows_ - menuH - 1;
                    if (menuStartCol + CONTEXT_MENU_WIDTH >= totalCols_)
                        menuStartCol = totalCols_ - CONTEXT_MENU_WIDTH - 1;
                    menuStartRow = std::max(0, menuStartRow);
                    menuStartCol = std::max(0, menuStartCol);

                    int idx = motionRow - menuStartRow;
                    if (motionCol >= menuStartCol && motionCol < menuStartCol + CONTEXT_MENU_WIDTH &&
                        idx >= 0 && idx < menuH && items[idx].find("───") == std::string::npos)
                    {
                        contextMenuHoverIdx_ = idx;
                    }
                    else
                    {
                        contextMenuHoverIdx_ = -1;
                    }
                    return LayoutAction::HANDLED;
                }

                // Find/Replace dialog hover tracking
                if (findReplaceActive_)
                {
                    if (motionRow == findDialogRow_ || (findReplaceMode_ && motionRow == replaceDialogRow_))
                    {
                        findHoverRow_ = motionRow;
                        findHoverCol_ = motionCol;
                    }
                    else
                    {
                        findHoverRow_ = -1;
                        findHoverCol_ = -1;
                    }
                }

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

                // Update bottom panel hover (tab bar + content area)
                if (showBottomPanel_ && motionRow >= bottomRect_.y &&
                    motionRow < bottomRect_.y + bottomRect_.h &&
                    motionCol >= bottomRect_.x)
                {
                    int localRow = motionRow - bottomRect_.y;
                    int localCol = motionCol - bottomRect_.x;
                    replPanel_.setHoverCol(localCol);
                    replPanel_.setHoverRow(localRow);
                }
                else
                {
                    replPanel_.setHoverCol(-1);
                    replPanel_.setHoverRow(-1);
                }

                // Update dashboard hover
                if (showDashboard_ && motionCol >= dashboardRect_.x && motionRow >= dashboardRect_.y &&
                    motionRow < dashboardRect_.y + dashboardRect_.h)
                {
                    int localRow = motionRow - dashboardRect_.y;
                    dashboardPanel_.setHoverRow(localRow);
                }
                else
                {
                    dashboardPanel_.setHoverRow(-1);
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

                if (resizingDashboard_)
                {
                    int newWidth = totalCols_ - motionCol;
                    dashboardWidth_ = std::clamp(newWidth, 16, totalCols_ / 2);
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

                // Drag scrollbar in bottom panel
                if ((event.motion.state & SDL_BUTTON_LMASK) && focus_ == FocusRegion::BottomPanel &&
                    showBottomPanel_ && motionRow >= bottomRect_.y)
                {
                    int localRow = motionRow - bottomRect_.y;
                    int localCol = motionCol - bottomRect_.x;
                    // Vertical scrollbar drag (rightmost column)
                    if (localCol == bottomRect_.w - 1 && localRow > 0)
                    {
                        replPanel_.handleScrollbarDrag(localRow);
                        return LayoutAction::HANDLED;
                    }
                    // Horizontal scrollbar drag (last row, not last column)
                    if (localRow == bottomRect_.h - 1 && localCol < bottomRect_.w - 1 && localRow > 0)
                    {
                        replPanel_.handleHScrollbarDrag(localCol);
                        return LayoutAction::HANDLED;
                    }
                }
            }

            // --- Mouse button up: end resize drags ---
            if (event.type == SDL_MOUSEBUTTONUP)
            {
                resizingSidebar_ = false;
                resizingBottom_ = false;
                resizingDashboard_ = false;
            }

            // --- Mouse wheel: route based on mouse position, not focus ---
            if (event.type == SDL_MOUSEWHEEL)
            {
                int mx, my;
                SDL_GetMouseState(&mx, &my);
                int mouseCol = mx / cellW_;
                int mouseRow = my / cellH_;

                // Check which region the mouse is over
                if (showBottomPanel_ && mouseRow >= bottomRect_.y)
                {
                    if (event.wheel.y != 0)
                        replPanel_.handleMouseWheel(event.wheel.y);

                    // Horizontal scroll: touchpad X-axis swipe or Shift+wheel
                    bool shiftHeld = (SDL_GetModState() & KMOD_SHIFT) != 0;
                    int hDelta = event.wheel.x;
                    if (hDelta == 0 && shiftHeld && event.wheel.y != 0)
                        hDelta = event.wheel.y;
                    if (hDelta != 0)
                        replPanel_.handleHScroll(hDelta * 3);

                    return LayoutAction::HANDLED;
                }
                else if (showDashboard_ && mouseCol >= dashboardRect_.x)
                {
                    dashboardPanel_.handleMouseWheel(event.wheel.y);
                    return LayoutAction::HANDLED;
                }
                else if (showSidebar_ && mouseCol < sidebarWidth_)
                {
                    fileTree_.handleMouseWheel(event.wheel.y);
                    return LayoutAction::HANDLED;
                }
                else
                {
                    // Vertical scroll
                    if (event.wheel.y != 0)
                        editor_.handleLocalWheel(event.wheel.y);
                    // Horizontal scroll (2-finger swipe or Shift+scroll)
                    bool shiftHeld = (SDL_GetModState() & KMOD_SHIFT) != 0;
                    int hDelta = event.wheel.x;
                    if (hDelta == 0 && shiftHeld && event.wheel.y != 0)
                        hDelta = event.wheel.y; // Shift+vertical wheel → horizontal scroll
                    if (hDelta != 0)
                        editor_.handleLocalHScroll(hDelta * 3);
                    return LayoutAction::HANDLED;
                }
            }

            // Route to focused region
            //
            // SPECIAL CASE: When a child process is running and waiting for stdin,
            // route keyboard events to the bottom panel's stdin input handler
            // regardless of which region has focus. This lets users type input
            // even when the editor has focus.
            if (replPanel_.isWaitingForInput())
            {
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
            }

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

            case FocusRegion::Dashboard:
                if (event.type == SDL_KEYDOWN)
                {
                    if (dashboardPanel_.handleKeyDown(event))
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

            // ── Dashboard panel (right side) ─────────────────────────
            if (showDashboard_)
            {
                // Draw vertical border on the left edge of dashboard
                int borderCol = dashboardRect_.x - 1;
                if (borderCol >= 0 && borderCol < totalCols_)
                {
                    for (int r = 0; r < totalRows_; r++)
                    {
                        out.cells[r][borderCol].ch = U'│';
                        out.cells[r][borderCol].fg = borderColor_;
                        out.cells[r][borderCol].bg = bgColor_;
                        out.cells[r][borderCol].dirty = true;
                    }
                }

                dashboardPanel_.setRect(dashboardRect_);
                auto dbCells = dashboardPanel_.render();
                blitPanel(out.cells, dbCells, dashboardRect_);
            }

            // ── File context menu overlay ────────────────────────────
            if (showFileContextMenu_)
            {
                renderContextMenu(out.cells);
            }

            // ── Goto-line dialog overlay ─────────────────────────────
            if (showGotoLine_)
            {
                renderGotoLineDialog(out.cells);
            }

            // ── Find/Replace dialog overlay ──────────────────────────
            if (findReplaceActive_)
            {
                renderFindReplaceDialog(out.cells);
            }

            // ── Autocomplete popup overlay ───────────────────────────
            if (acPopup_.isVisible())
            {
                auto popup = acPopup_.render();
                if (popup.h > 0 && popup.w > 0)
                {
                    // Adjust popup position relative to editor rect
                    int popX = editorRect_.x + popup.x;
                    int popY = editorRect_.y + popup.y;

                    // Ensure popup doesn't go off screen
                    if (popY + popup.h > totalRows_)
                        popY = editorRect_.y + popup.y - popup.h - 1; // above cursor
                    if (popX + popup.w > totalCols_)
                        popX = totalCols_ - popup.w;
                    popX = std::max(0, popX);
                    popY = std::max(0, popY);

                    for (int r = 0; r < popup.h && popY + r < totalRows_; r++)
                    {
                        for (int c = 0; c < popup.w && popX + c < totalCols_; c++)
                        {
                            if (popY + r >= 0 && popX + c >= 0)
                                out.cells[popY + r][popX + c] = popup.cells[r][c];
                        }
                    }
                }
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
                {" OBJECTS ", BottomTab::OBJECTS},
                {" LIFECYCLE ", BottomTab::LIFECYCLE},
                {" TIMELINE ", BottomTab::TIMELINE},
                {" CALLSTACK ", BottomTab::CALLSTACK},
                {" HELP ", BottomTab::HELP},
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

            // Only lint Xell files
            std::string filePath = editor_.getCurrentFilePath();
            if (!filePath.empty())
            {
                std::string ext = std::filesystem::path(filePath).extension().string();
                if (ext != ".xel" && ext != ".xell" && ext != ".nxel" &&
                    ext != ".xesy" && ext != ".xell_meta")
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

            // Pipe buffer content to xell --check-symbols via stdin
            // stdout = JSON symbols, stderr = diagnostics
            std::string xellBin = findXellBinary();
            int exitCode = 0;
            std::string symbolsJson, diagnosticOutput;
            captureCommandSplitOutput(
                xellBin + " --check-symbols", fullContent, exitCode,
                symbolsJson, diagnosticOutput);

            // Feed AST symbols to autocomplete DB
            if (!symbolsJson.empty())
            {
                acDB_.loadASTSymbols(symbolsJson);
                // Also feed to Variables tab for static symbol display
                replPanel_.loadStaticSymbols(symbolsJson, filePath);
                // Also feed to Dashboard panel for code structure view
                dashboardPanel_.loadSymbols(symbolsJson, filePath);
            }

            // Parse diagnostics into display lines
            std::string &output = diagnosticOutput;

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
            if (ext != ".xel" && ext != ".xell" && ext != ".nxel" &&
                ext != ".xesy" && ext != ".xell_meta")
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
        DashboardPanel dashboardPanel_;
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
        bool showDashboard_ = true;
        int sidebarWidth_ = 28;
        int bottomPanelHeight_ = 10;
        int dashboardWidth_ = 24;

        // Computed rects
        Rect sidebarRect_ = {0, 0, 28, 24};
        Rect editorRect_ = {29, 0, 51, 24};
        Rect bottomRect_ = {29, 14, 51, 10};
        Rect dashboardRect_ = {0, 0, 0, 0};

        // Resize drag state
        bool resizingSidebar_ = false;
        bool resizingBottom_ = false;
        bool resizingDashboard_ = false;

        // Goto-line dialog state
        bool showGotoLine_ = false;
        std::string gotoLineInput_;
        int gotoLineCursor_ = 0;

        // Find/Replace dialog state
        bool findReplaceActive_ = false;
        bool findReplaceMode_ = false;           // false = find only, true = find + replace
        bool findReplaceEditingReplace_ = false; // which field is active
        std::string findInput_;
        std::string replaceInput_;
        int findCursor_ = 0;
        int replaceCursor_ = 0;
        std::vector<std::pair<int, int>> findMatches_; // (row, col) pairs
        int currentMatch_ = -1;

        // Dialog button positions for mouse interaction (set during render)
        mutable int findPrevBtnCol_ = -1;
        mutable int findNextBtnCol_ = -1;
        mutable int findCloseBtnCol_ = -1;
        mutable int findDialogRow_ = -1;
        mutable int findDialogStartCol_ = -1;
        mutable int findDialogWidth_ = 0;
        mutable int findInputStartCol_ = -1;
        mutable int replaceDialogRow_ = -1;
        mutable int replaceBtnCol_ = -1;
        mutable int replaceAllBtnCol_ = -1;
        mutable int replaceInputStartCol_ = -1;

        // Hover tracking for find/replace dialog buttons
        mutable int findHoverRow_ = -1;
        mutable int findHoverCol_ = -1;

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

        // Trace / lifecycle cache (lazy: populated after Ctrl+R run)
        std::string lastTracedFile_;
        std::string traceJsonCache_; // raw JSON from --trace-vars

        // ── Debug session state ──────────────────────────────────────
        DebugClient debugClient_;
        DebugState debugState_;
        std::string debugSocketPath_;
        pid_t debugChildPid_ = 0;
        bool debugLaunching_ = false; // waiting for socket connection

        // File context menu state
        bool showFileContextMenu_ = false;
        int contextMenuRow_ = 0;
        int contextMenuCol_ = 0;
        mutable int contextMenuHoverIdx_ = -1; // Which menu item is hovered

        // Mouse hover state (cell coordinates)
        int hoverRow_ = -1;
        int hoverCol_ = -1;

        // ── Autocomplete state ──────────────────────────────────────
        CompletionDB acDB_;
        AutocompletePopup acPopup_;
        SnippetEngine snippetEngine_;
        ActiveSnippet activeSnippet_; // currently expanding snippet
        std::string acPrefix_;        // current word being typed
        std::string acModuleName_;    // module name for -> member access (empty = normal mode)
        Clock::time_point acLastKeyTime_ = Clock::now();
        bool acPendingShow_ = false;               // debounce: show popup after delay
        bool acManualTrigger_ = false;             // Ctrl+Space forced popup
        static constexpr int AC_DEBOUNCE_MS = 200; // auto-show delay
        static constexpr int AC_MIN_PREFIX = 1;    // min chars for auto-show

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

        // ── Asset path resolution (mirrors theme_loader / font resolution) ──

        static std::string resolveAssetPath(const std::string &filename)
        {
            namespace fs = std::filesystem;
            std::vector<std::string> candidates;

            // 1. Relative to executable: <exe_dir>/assets/<filename>
            char *base = SDL_GetBasePath();
            if (base)
            {
                candidates.push_back(std::string(base) + "assets/" + filename);
                candidates.push_back(std::string(base) + "../share/xell-terminal/" + filename);
                SDL_free(base);
            }

            // 2. Relative to CWD
            candidates.push_back("assets/" + filename);

            // 3. Home directory (local install)
            const char *home = std::getenv("HOME");
            if (home)
            {
                candidates.push_back(std::string(home) + "/.local/share/xell-terminal/" + filename);
                candidates.push_back(std::string(home) + "/.config/xell/" + filename);
            }

            // 4. Common system install prefixes
            candidates.push_back("/usr/local/share/xell-terminal/" + filename);
            candidates.push_back("/usr/share/xell-terminal/" + filename);

            for (auto &path : candidates)
            {
                if (fs::exists(path))
                {
                    std::cerr << "[autocomplete] resolved " << filename << " -> " << path << "\n";
                    return path;
                }
            }

            // Fallback — return CWD relative path
            std::cerr << "[autocomplete] WARNING: could not find " << filename << "\n";
            return "assets/" + filename;
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
            Color hoverBg = {70, 70, 100};

            for (int i = 0; i < menuH; i++)
            {
                int r = startRow + i;
                if (r >= totalRows_)
                    break;
                bool isSep = (items[i].find("───") != std::string::npos);
                bool isHovered = (i == contextMenuHoverIdx_ && !isSep);
                Color fg = isSep ? sepFg : menuFg;
                Color bg = isHovered ? hoverBg : menuBg;

                // Fill background first
                for (int c = 0; c < CONTEXT_MENU_WIDTH && startCol + c < totalCols_; c++)
                {
                    cells[r][startCol + c].bg = bg;
                    cells[r][startCol + c].fg = fg;
                    cells[r][startCol + c].ch = U' ';
                    cells[r][startCol + c].dirty = true;
                }
                // Write text with UTF-8 decode
                utf8Write(cells[r], startCol, items[i], fg, bg);
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

            // Right-side dashboard panel
            if (showDashboard_)
            {
                int dbW = std::min(dashboardWidth_, contentWidth / 3);
                dashboardRect_ = {contentStartCol + contentWidth - dbW, 0, dbW, totalRows_};
                dashboardPanel_.setRect(dashboardRect_);
                contentWidth -= (dbW + 1); // +1 for border
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

        // ── Autocomplete helper methods ─────────────────────────────

        // Extract the word being typed at the cursor position
        void updateAcPrefix()
        {
            const auto *buf = editor_.activeBuffer();
            if (!buf)
            {
                acPrefix_.clear();
                return;
            }

            auto info = editor_.getStatusInfo();
            int row = info.cursorRow - 1; // getStatusInfo returns 1-based
            int col = info.cursorCol - 1;
            if (row < 0 || row >= buf->lineCount())
            {
                acPrefix_.clear();
                return;
            }

            const std::string &line = buf->getLine(row);
            if (col < 0 || col > (int)line.size())
                col = (int)line.size();

            // Walk backwards to find start of identifier
            int start = col;
            while (start > 0 && (std::isalnum(line[start - 1]) || line[start - 1] == '_'))
                start--;

            // Include leading @ for decorator completions (@debug, @breakpoint, etc.)
            if (start > 0 && line[start - 1] == '@')
                start--;

            acPrefix_ = line.substr(start, col - start);

            // Check for -> member access pattern
            acModuleName_.clear();
            if (start >= 2 && line[start - 1] == '>' && line[start - 2] == '-')
            {
                int arrowPos = start - 2; // position of '-' in '->'

                // Determine what precedes the -> operator
                // Walk back from '-' to identify the expression type
                int mEnd = arrowPos;

                // Skip whitespace before ->
                int p = mEnd - 1;
                while (p >= 0 && line[p] == ' ')
                    p--;

                if (p >= 0 && line[p] == '"')
                {
                    // String literal: "hello"->  → __string__
                    acModuleName_ = "__string__";
                }
                else if (p >= 0 && line[p] == ']')
                {
                    // List literal: [1,2]->  → __list__
                    acModuleName_ = "__list__";
                }
                else if (p >= 0 && line[p] == '}')
                {
                    // Map literal: {a: 1}->  → __map__
                    acModuleName_ = "__map__";
                }
                else if (p >= 0 && line[p] == ')')
                {
                    // Function call result: fn()->  → unknown type, use fallback
                    acModuleName_ = "__any__";
                }
                else if (p >= 0 && (std::isalnum(line[p]) || line[p] == '_'))
                {
                    // Identifier: module-> or variable->
                    int mStart = p;
                    while (mStart > 0 && (std::isalnum(line[mStart - 1]) || line[mStart - 1] == '_'))
                        mStart--;
                    std::string name = line.substr(mStart, p - mStart + 1);

                    if (acDB_.hasModuleMembers(name))
                    {
                        // Known module/class/struct — use exact members
                        acModuleName_ = name;
                    }
                    else
                    {
                        // Unknown variable — use fallback (all common methods)
                        acModuleName_ = "__any__";
                    }
                }
            }
        }

        void showAutocomplete()
        {
            if (acPrefix_.empty() && !acManualTrigger_ && acModuleName_.empty())
                return;

            // AST symbols are loaded during lint (--check-symbols).
            // As fallback, regex-scan the buffer if no AST symbols available yet.
            if (acDB_.userSymbolCount() == 0)
            {
                const auto *buf = editor_.activeBuffer();
                if (buf)
                    acDB_.scanBuffer(buf->lines());
            }

            // Get cursor screen position from editor info
            auto info = editor_.getStatusInfo();
            // These are 1-based — convert to 0-based screen coords
            // The editor widget has a tab bar (row 0), so cursor screen row
            // needs to account for scroll offset. Use getStatusInfo row - scrollTopLine.
            // Simpler: just use the row/col from getStatusInfo as approximate popup position
            int screenRow = info.cursorRow; // roughly maps to screen (tab bar offset handled by overlay)
            int screenCol = info.cursorCol;

            // Show with current prefix (or empty for manual trigger)
            std::string prefix = acManualTrigger_ && acPrefix_.empty() ? "" : acPrefix_;

            // If in -> member access mode, show only module/type members
            if (!acModuleName_.empty() && acDB_.hasModuleMembers(acModuleName_))
            {
                std::vector<CompletionItem> members;
                if (acModuleName_ == "__any__")
                    members = acDB_.matchAnyMembers(prefix);
                else
                    members = acDB_.matchModuleMembers(acModuleName_, prefix);

                if (!members.empty())
                    acPopup_.showItems(screenRow, screenCol - (int)acPrefix_.size(), acPrefix_, members);
                else
                    acPopup_.hide();
            }
            else
            {
                acPopup_.show(screenRow, screenCol - (int)acPrefix_.size(), acPrefix_, acDB_);
            }

            acManualTrigger_ = false;
            updateGhostText();
        }

        void dismissAutocomplete()
        {
            acPopup_.hide();
            acPrefix_.clear();
            acModuleName_.clear();
            acPendingShow_ = false;
            acManualTrigger_ = false;
            editor_.clearGhostText();
        }

        void acceptCompletion()
        {
            const CompletionItem *item = acPopup_.accept();
            if (!item)
            {
                dismissAutocomplete();
                return;
            }

            // Store fields before hiding popup
            std::string label = item->label;
            std::string insertText = item->insertText;
            CompletionKind kind = item->kind;

            acPopup_.hide();
            editor_.clearGhostText();

            // Delete the prefix we've already typed, then insert the completion
            if (!acPrefix_.empty())
            {
                auto *buf = editor_.activeBufferMut();
                if (buf)
                {
                    auto info = editor_.getStatusInfo();
                    int row = info.cursorRow - 1;
                    int col = info.cursorCol - 1;
                    int prefixLen = (int)acPrefix_.size();

                    BufferPos from = {row, col - prefixLen};
                    BufferPos to = {row, col};
                    buf->deleteRange(from, to);
                    editor_.setCursorPosition(row, col - prefixLen);
                }
            }

            // If it's a snippet, expand it
            if (kind == CompletionKind::Snippet)
            {
                const SnippetDef *def = snippetEngine_.findByPrefix(label);
                if (def)
                {
                    expandSnippet(*def);
                    acPrefix_.clear();
                    return;
                }
            }

            // If the item has insertText with snippet placeholders, expand as snippet
            if (!insertText.empty() && insertText.find("${") != std::string::npos)
            {
                SnippetDef tempDef;
                tempDef.prefix = label;
                tempDef.description = label;
                tempDef.bodyLines = {insertText};
                expandSnippet(tempDef);
                acPrefix_.clear();
                return;
            }

            // Regular completion — insert insertText if available, else label
            editor_.insertTextAtCursor(insertText.empty() ? label : insertText);
            acPrefix_.clear();
        }

        void expandSnippet(const SnippetDef &def)
        {
            auto info = editor_.getStatusInfo();
            int row = info.cursorRow - 1;
            int col = info.cursorCol - 1;

            activeSnippet_ = snippetEngine_.expand(def, row, col);

            // Insert the full snippet text
            std::string text = SnippetEngine::buildInsertText(activeSnippet_);
            editor_.insertTextAtCursor(text);

            // Navigate to first tab stop
            if (!activeSnippet_.tabStops.empty())
            {
                const TabStop &ts = activeSnippet_.tabStops[0];
                int absRow = row + ts.line;
                int absCol = ts.col;
                editor_.setCursorPosition(absRow, absCol);
                editor_.setActiveTabStop(absRow, absCol, ts.length);

                // Select the placeholder text
                if (ts.length > 0)
                {
                    editor_.setSelection(absRow, absCol, absRow, absCol + ts.length);
                }
            }
        }

        void snippetNextStop()
        {
            if (!activeSnippet_.active)
                return;

            if (!activeSnippet_.nextStop())
            {
                commitSnippet();
                return;
            }

            const TabStop *ts = activeSnippet_.current();
            if (ts)
            {
                int absRow = activeSnippet_.startLine + ts->line;
                int absCol = ts->col;
                editor_.setCursorPosition(absRow, absCol);
                editor_.setActiveTabStop(absRow, absCol, ts->length);
                if (ts->length > 0)
                    editor_.setSelection(absRow, absCol, absRow, absCol + ts->length);
            }
        }

        void snippetPrevStop()
        {
            if (!activeSnippet_.active)
                return;

            if (!activeSnippet_.prevStop())
                return;

            const TabStop *ts = activeSnippet_.current();
            if (ts)
            {
                int absRow = activeSnippet_.startLine + ts->line;
                int absCol = ts->col;
                editor_.setCursorPosition(absRow, absCol);
                editor_.setActiveTabStop(absRow, absCol, ts->length);
                if (ts->length > 0)
                    editor_.setSelection(absRow, absCol, absRow, absCol + ts->length);
            }
        }

        void commitSnippet()
        {
            activeSnippet_.active = false;
            editor_.clearActiveTabStop();
            editor_.clearSelection();
        }

        void updateGhostText()
        {
            const CompletionItem *item = acPopup_.accept();
            if (!item || acPrefix_.empty())
            {
                editor_.clearGhostText();
                return;
            }

            // Show the remaining portion of the top completion as ghost text
            std::string label = item->label;
            if (label.size() > acPrefix_.size() &&
                label.substr(0, acPrefix_.size()) == acPrefix_)
            {
                std::string ghost = label.substr(acPrefix_.size());
                auto info = editor_.getStatusInfo();
                int row = info.cursorRow - 1;
                int col = info.cursorCol - 1;
                editor_.setGhostText(ghost, row, col);
            }
            else
            {
                editor_.clearGhostText();
            }
        }

        void acceptGhostText()
        {
            // Insert the ghost text at cursor position
            if (!editor_.hasGhostText())
                return;

            std::string ghost = editor_.getGhostText();
            editor_.clearGhostText();
            editor_.insertTextAtCursor(ghost);
            acPrefix_.clear();
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
                showFindReplace(false);
                return LayoutAction::HANDLED;
            case EditorAction::FIND_REPLACE:
                showFindReplace(true);
                return LayoutAction::HANDLED;
            case EditorAction::GOTO_LINE:
                showGotoLine_ = true;
                gotoLineInput_.clear();
                gotoLineCursor_ = 0;
                return LayoutAction::HANDLED;
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

            // If there's a selection, run selected text; otherwise run from top to cursor line
            std::string code;
            if (editor_.hasSelection())
            {
                code = editor_.getSelectedText();
            }
            else
            {
                code = editor_.getTextToCursorLine();
            }

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
            // Pass the editor file's directory so module imports resolve correctly
            std::string sourceDir;
            std::string filePath = editor_.getCurrentFilePath();
            if (!filePath.empty())
                sourceDir = std::filesystem::path(filePath).parent_path().string();
            replPanel_.runCode(code, sourceDir);
        }

        // ── Debug Run (Ctrl+Shift+D) — run selection/cursor with full lifecycle + debug ──
        void handleDebugRun()
        {
            if (editor_.activeTabIndex() < 0)
                return;

            // Same logic as Ctrl+Enter: selection → run selected, else line 1 to cursor
            std::string code;
            if (editor_.hasSelection())
            {
                code = editor_.getSelectedText();
            }
            else
            {
                code = editor_.getTextToCursorLine();
            }

            if (code.empty())
                return;

            // Show bottom panel if hidden
            if (!showBottomPanel_)
            {
                showBottomPanel_ = true;
                recalcLayout();
            }

            // Switch to LIFECYCLE tab so user sees debug output
            replPanel_.setActiveTab(BottomTab::LIFECYCLE);

            // Get the editor file's directory so module imports resolve correctly
            std::string sourceDir;
            std::string filePath = editor_.getCurrentFilePath();
            if (!filePath.empty())
                sourceDir = std::filesystem::path(filePath).parent_path().string();

            // Run the code for output
            replPanel_.runCode(code, sourceDir);

            // Write code to a temp file for trace collection
            std::string tmpDir = "/tmp/xell";
            std::filesystem::create_directories(tmpDir);
            std::string tmpFile = tmpDir + "/xell_debug_run.xel";
            {
                FILE *f = fopen(tmpFile.c_str(), "w");
                if (f)
                {
                    fprintf(f, "%s\n", code.c_str());
                    fclose(f);

                    // Collect trace (lifecycle events) from the temp file
                    lastTracedFile_ = tmpFile;
                    collectTraceForFile(tmpFile, sourceDir);
                }
            }
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

            // Run the file using its full path (no lifecycle trace — use Ctrl+Shift+D for that)
            replPanel_.runFile(fullPath);
        }

        // Collect trace data lazily after file execution
        void collectTraceForFile(const std::string &filePath, const std::string &sourceDir = "")
        {
            std::string xellBin = findXellBinary();
            int exitCode = 0;
            std::string cmd = xellBin + " --trace-vars \"" + filePath + "\"";
            if (!sourceDir.empty())
                cmd += " --source-dir \"" + sourceDir + "\"";
            traceJsonCache_ = captureCommand(cmd, exitCode);
        }

        // Lifecycle lookup: given a variable name, extract lifecycle events from trace cache
        std::vector<std::string> lifecycleLookup(const std::string &varName) const
        {
            std::vector<std::string> result;
            if (traceJsonCache_.empty() || traceJsonCache_[0] != '[')
                return result;

            // Simple scan of the JSON trace array for entries matching this variable
            // Each entry: {"event":"VAR_BORN","line":5,"name":"x","type":"int","value":"10",...}
            size_t pos = 0;
            while (pos < traceJsonCache_.size())
            {
                pos = traceJsonCache_.find('{', pos);
                if (pos == std::string::npos)
                    break;
                size_t objEnd = traceJsonCache_.find('}', pos);
                if (objEnd == std::string::npos)
                    break;
                std::string obj = traceJsonCache_.substr(pos, objEnd - pos + 1);
                pos = objEnd + 1;

                // Quick check: does this entry's "name" match?
                size_t nameKey = obj.find("\"name\":\"");
                if (nameKey == std::string::npos)
                    continue;
                size_t nameStart = nameKey + 8;
                size_t nameEnd = obj.find('"', nameStart);
                if (nameEnd == std::string::npos)
                    continue;
                std::string entryName = obj.substr(nameStart, nameEnd - nameStart);
                if (entryName != varName)
                    continue;

                // Extract event, line, type, value, byWhom
                auto extractField = [&](const std::string &key) -> std::string
                {
                    std::string search = "\"" + key + "\":\"";
                    size_t k = obj.find(search);
                    if (k == std::string::npos)
                        return "";
                    size_t vs = k + search.size();
                    size_t ve = obj.find('"', vs);
                    if (ve == std::string::npos)
                        return "";
                    return obj.substr(vs, ve - vs);
                };
                auto extractInt = [&](const std::string &key) -> std::string
                {
                    std::string search = "\"" + key + "\":";
                    size_t k = obj.find(search);
                    if (k == std::string::npos)
                        return "";
                    size_t vs = k + search.size();
                    size_t ve = vs;
                    while (ve < obj.size() && (isdigit(obj[ve]) || obj[ve] == '-'))
                        ve++;
                    return obj.substr(vs, ve - vs);
                };

                std::string event = extractField("event");
                std::string line = extractInt("line");
                std::string type = extractField("type");
                std::string value = extractField("value");
                std::string byWhom = extractField("byWhom");

                // Format: "  5   BORN      str   \"hello\"   direct assignment"
                // Pad fields for alignment
                std::string lineStr = line;
                while (lineStr.size() < 5)
                    lineStr = " " + lineStr;

                std::string eventStr = event;
                // Shorten event names
                if (eventStr == "VAR_BORN")
                    eventStr = "BORN";
                if (eventStr == "VAR_CHANGED")
                    eventStr = "CHANGED";
                if (eventStr == "VAR_DIED")
                    eventStr = "DIED";
                while (eventStr.size() < 9)
                    eventStr += " ";

                std::string typeStr = type;
                while (typeStr.size() < 8)
                    typeStr += " ";

                std::string valStr = value;
                if (valStr.size() > 16)
                    valStr = valStr.substr(0, 13) + "...";
                while (valStr.size() < 16)
                    valStr += " ";

                result.push_back(lineStr + "  " + eventStr + typeStr + valStr + byWhom);
            }
            return result;
        }

        // ── Debug session management ─────────────────────────────────

        void handleDebugLaunch()
        {
            if (debugState_.active)
            {
                editor_.setStatusMessage("Debug session already active. F12 to stop.");
                return;
            }

            std::string filePath = editor_.getCurrentFilePath();
            if (filePath.empty())
            {
                editor_.setStatusMessage("Save file first to debug");
                return;
            }

            // Show bottom panel
            if (!showBottomPanel_)
            {
                showBottomPanel_ = true;
                recalcLayout();
            }

            // Clear previous state
            replPanel_.clearTimeline();
            replPanel_.clearCallStack();
            editor_.clearDebugLine();

            std::string xellBin = findXellBinary();
            std::string sourceDir = std::filesystem::path(filePath).parent_path().string();

            // Launch xell --debug directly with fork/exec.
            // We capture stderr via a pipe to read the XELL_DEBUG_SOCKET: line.
            int stderrPipe[2];
            if (pipe(stderrPipe) != 0)
            {
                editor_.setStatusMessage("❌ Failed to create pipe for debug launch");
                return;
            }

            pid_t pid = fork();
            if (pid < 0)
            {
                close(stderrPipe[0]);
                close(stderrPipe[1]);
                editor_.setStatusMessage("❌ Failed to fork for debug launch");
                return;
            }

            if (pid == 0)
            {
                // Child process — run xell --debug <file>
                setsid();             // new process group
                close(stderrPipe[0]); // close read end

                // Redirect stderr to the pipe so parent can read XELL_DEBUG_SOCKET:
                dup2(stderrPipe[1], STDERR_FILENO);
                close(stderrPipe[1]);

                // Redirect stdout to /dev/null (trace JSON goes to stdout, we don't need it now)
                int devnull = open("/dev/null", O_WRONLY);
                if (devnull >= 0)
                {
                    dup2(devnull, STDOUT_FILENO);
                    close(devnull);
                }

                // Build args
                if (sourceDir.empty())
                    execlp(xellBin.c_str(), xellBin.c_str(), "--debug", filePath.c_str(), nullptr);
                else
                    execlp(xellBin.c_str(), xellBin.c_str(), "--debug", filePath.c_str(),
                           "--source-dir", sourceDir.c_str(), nullptr);
                _exit(127);
            }

            // Parent process
            close(stderrPipe[1]); // close write end
            debugChildPid_ = pid;

            // Read stderr from the child (non-blocking with timeout).
            // The child should write XELL_DEBUG_SOCKET:<path> very quickly.
            debugSocketPath_.clear();
            {
                // Set the read end to non-blocking
                int flags = fcntl(stderrPipe[0], F_GETFL, 0);
                fcntl(stderrPipe[0], F_SETFL, flags | O_NONBLOCK);

                std::string stderrBuf;
                char buf[512];
                int elapsed = 0;
                const int intervalUs = 20000;  // 20ms
                const int timeoutUs = 3000000; // 3 seconds

                while (elapsed < timeoutUs)
                {
                    ssize_t n = read(stderrPipe[0], buf, sizeof(buf) - 1);
                    if (n > 0)
                    {
                        buf[n] = '\0';
                        stderrBuf.append(buf, n);

                        // Check if we have the socket path line
                        const std::string prefix = "XELL_DEBUG_SOCKET:";
                        auto pos = stderrBuf.find(prefix);
                        if (pos != std::string::npos)
                        {
                            auto lineEnd = stderrBuf.find('\n', pos);
                            if (lineEnd != std::string::npos)
                                debugSocketPath_ = stderrBuf.substr(pos + prefix.size(), lineEnd - pos - prefix.size());
                            else
                                debugSocketPath_ = stderrBuf.substr(pos + prefix.size());
                            // Trim whitespace
                            while (!debugSocketPath_.empty() && (debugSocketPath_.back() == '\n' || debugSocketPath_.back() == '\r' || debugSocketPath_.back() == ' '))
                                debugSocketPath_.pop_back();
                            break;
                        }
                    }
                    else if (n == 0)
                    {
                        break; // EOF — child closed stderr
                    }
                    // else n < 0 → EAGAIN, nothing to read yet

                    usleep(intervalUs);
                    elapsed += intervalUs;
                }

                close(stderrPipe[0]);
            }

            if (debugSocketPath_.empty())
            {
                editor_.setStatusMessage("❌ Failed to start debug session (no socket path)");
                // Kill the child if it's still running
                if (debugChildPid_ > 0)
                {
                    kill(debugChildPid_, SIGTERM);
                    waitpid(debugChildPid_, nullptr, WNOHANG);
                    debugChildPid_ = 0;
                }
                return;
            }

            // Connect to the debug socket
            if (!debugClient_.connect(debugSocketPath_, 5000))
            {
                editor_.setStatusMessage("❌ Failed to connect to debug socket");
                debugSocketPath_.clear();
                if (debugChildPid_ > 0)
                {
                    kill(debugChildPid_, SIGTERM);
                    waitpid(debugChildPid_, nullptr, WNOHANG);
                    debugChildPid_ = 0;
                }
                return;
            }

            debugState_.active = true;
            debugState_.paused = true; // starts paused on first statement

            // Send any existing breakpoints to the interpreter
            auto bps = editor_.breakpoints();
            if (bps)
            {
                for (auto &[line, type] : *bps)
                    debugClient_.sendAddBreakpoint(line + 1, type); // 1-based
            }

            // Switch to TIMELINE tab
            replPanel_.setActiveTab(BottomTab::TIMELINE);
            replPanel_.appendTimelineEvent("[session] Debug started: " + filePath);
            editor_.setStatusMessage("🐛 Debug session started — F5:Continue F10:StepOver F11:StepIn/Out F12:Stop");
        }

        void endDebugSession()
        {
            debugClient_.disconnect();
            // Kill the debug child process if still running
            if (debugChildPid_ > 0)
            {
                kill(-debugChildPid_, SIGTERM); // kill process group
                kill(debugChildPid_, SIGTERM);
                usleep(50000); // 50ms grace
                kill(-debugChildPid_, SIGKILL);
                kill(debugChildPid_, SIGKILL);
                waitpid(debugChildPid_, nullptr, WNOHANG); // reap zombie
            }
            debugState_ = DebugState{};
            debugSocketPath_.clear();
            debugChildPid_ = 0;
            editor_.clearDebugLine();
        }

        void pollDebugSession()
        {
            if (!debugState_.active)
                return;

            // Poll for messages from the interpreter (non-blocking)
            std::string msg = debugClient_.tryRecv();
            while (!msg.empty())
            {
                DebugState newState = DebugClient::parseState(msg);

                if (!newState.active && !newState.paused)
                {
                    // Session finished
                    replPanel_.appendTimelineEvent("[session] Debug session ended");
                    endDebugSession();
                    editor_.setStatusMessage("✓ Debug session finished");
                    return;
                }

                if (newState.paused)
                {
                    debugState_ = newState;
                    int line0 = newState.currentLine - 1; // 0-based for editor
                    editor_.setDebugLine(line0);
                    editor_.goToLine(line0);

                    // Update timeline
                    std::string event = "[line " + std::to_string(newState.currentLine) +
                                        "] paused (seq:" + std::to_string(newState.sequence) +
                                        " depth:" + std::to_string(newState.depth) + ")";
                    replPanel_.appendTimelineEvent(event);

                    // Update call stack from JSON
                    updateCallStackFromJSON(newState.stackJSON);

                    editor_.setStatusMessage("⏸ Paused at line " + std::to_string(newState.currentLine));
                }
                else
                {
                    debugState_ = newState;
                    editor_.clearDebugLine();
                }

                msg = debugClient_.tryRecv();
            }

            // Check if the connection dropped (and we didn't get a clean "finished")
            if (!debugClient_.isConnected() && debugState_.active)
            {
                // Check if child process is still alive
                if (debugChildPid_ > 0)
                {
                    int status = 0;
                    pid_t result = waitpid(debugChildPid_, &status, WNOHANG);
                    if (result > 0)
                    {
                        // Child exited — program finished (socket closed before we read "finished")
                        replPanel_.appendTimelineEvent("[session] Debug session ended (process exited)");
                        endDebugSession();
                        editor_.setStatusMessage("✓ Debug session finished");
                        return;
                    }
                }
                replPanel_.appendTimelineEvent("[session] Connection lost");
                endDebugSession();
                editor_.setStatusMessage("⚠ Debug connection lost");
            }
        }

        void updateCallStackFromJSON(const std::string &json)
        {
            // Parse a JSON array of strings: ["main:1","foo:5","bar:10"]
            std::vector<std::string> frames;
            if (json.size() < 2 || json[0] != '[')
            {
                replPanel_.setCallStack(frames);
                return;
            }

            size_t pos = 1;
            while (pos < json.size())
            {
                pos = json.find('"', pos);
                if (pos == std::string::npos)
                    break;
                pos++;
                size_t end = json.find('"', pos);
                if (end == std::string::npos)
                    break;
                frames.push_back(json.substr(pos, end - pos));
                pos = end + 1;
            }

            replPanel_.setCallStack(frames);
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

        // ── Find / Replace methods ───────────────────────────────────

        void showFindReplace(bool replaceMode)
        {
            findReplaceActive_ = true;
            findReplaceMode_ = replaceMode;
            findReplaceEditingReplace_ = false;
            findInput_.clear();
            replaceInput_.clear();
            findCursor_ = 0;
            replaceCursor_ = 0;
            findMatches_.clear();
            currentMatch_ = -1;

            // Pre-fill with selected text if any
            if (editor_.hasSelection())
            {
                findInput_ = editor_.getSelectedText();
                findCursor_ = (int)findInput_.size();
                updateFindMatches();
            }
        }

        void updateFindMatches()
        {
            findMatches_.clear();
            currentMatch_ = -1;
            if (findInput_.empty())
                return;

            const auto *buf = editor_.activeBuffer();
            if (!buf)
                return;

            try
            {
                std::regex pattern(findInput_, std::regex::ECMAScript);
                for (int row = 0; row < buf->lineCount(); row++)
                {
                    const std::string &line = buf->getLine(row);
                    auto begin = std::sregex_iterator(line.begin(), line.end(), pattern);
                    auto end = std::sregex_iterator();
                    for (auto it = begin; it != end; ++it)
                    {
                        findMatches_.push_back({row, (int)it->position()});
                    }
                }
            }
            catch (const std::regex_error &)
            {
                // Invalid regex — treat as literal search
                const auto *buf2 = editor_.activeBuffer();
                if (!buf2)
                    return;
                for (int row = 0; row < buf2->lineCount(); row++)
                {
                    const std::string &line = buf2->getLine(row);
                    size_t pos = 0;
                    while ((pos = line.find(findInput_, pos)) != std::string::npos)
                    {
                        findMatches_.push_back({row, (int)pos});
                        pos += findInput_.size();
                    }
                }
            }

            if (!findMatches_.empty())
            {
                currentMatch_ = 0;
                navigateToCurrentMatch();
            }

            editor_.setStatusMessage(std::to_string(findMatches_.size()) + " match(es)");
        }

        void navigateToCurrentMatch()
        {
            if (currentMatch_ < 0 || currentMatch_ >= (int)findMatches_.size())
                return;
            auto [row, col] = findMatches_[currentMatch_];
            editor_.setCursorPosition(row, col);
        }

        void findNext()
        {
            if (findMatches_.empty())
                return;
            currentMatch_ = (currentMatch_ + 1) % (int)findMatches_.size();
            navigateToCurrentMatch();
            editor_.setStatusMessage("Match " + std::to_string(currentMatch_ + 1) +
                                     "/" + std::to_string(findMatches_.size()));
        }

        void findPrev()
        {
            if (findMatches_.empty())
                return;
            currentMatch_ = (currentMatch_ - 1 + (int)findMatches_.size()) % (int)findMatches_.size();
            navigateToCurrentMatch();
            editor_.setStatusMessage("Match " + std::to_string(currentMatch_ + 1) +
                                     "/" + std::to_string(findMatches_.size()));
        }

        void doReplace()
        {
            if (findMatches_.empty() || currentMatch_ < 0 || currentMatch_ >= (int)findMatches_.size())
                return;

            auto *buf = editor_.activeBufferMut();
            if (!buf)
                return;

            auto [row, col] = findMatches_[currentMatch_];
            const std::string &line = buf->getLine(row);

            // Determine the match length using the same regex
            int matchLen = (int)findInput_.size(); // default for literal
            try
            {
                std::regex pattern(findInput_, std::regex::ECMAScript);
                auto begin = std::sregex_iterator(line.begin(), line.end(), pattern);
                for (auto it = begin; it != std::sregex_iterator(); ++it)
                {
                    if ((int)it->position() == col)
                    {
                        matchLen = (int)it->length();
                        break;
                    }
                }
            }
            catch (const std::regex_error &)
            {
            }

            // Replace
            BufferPos from = {row, col};
            BufferPos to = {row, col + matchLen};
            buf->replaceRange(from, to, replaceInput_);

            // Refresh matches
            updateFindMatches();
        }

        void doReplaceAll()
        {
            if (findMatches_.empty())
                return;

            auto *buf = editor_.activeBufferMut();
            if (!buf)
                return;

            int replaced = 0;
            // Replace from bottom to top so positions don't shift
            for (int i = (int)findMatches_.size() - 1; i >= 0; i--)
            {
                auto [row, col] = findMatches_[i];
                const std::string &line = buf->getLine(row);

                int matchLen = (int)findInput_.size();
                try
                {
                    std::regex pattern(findInput_, std::regex::ECMAScript);
                    auto begin = std::sregex_iterator(line.begin(), line.end(), pattern);
                    for (auto it = begin; it != std::sregex_iterator(); ++it)
                    {
                        if ((int)it->position() == col)
                        {
                            matchLen = (int)it->length();
                            break;
                        }
                    }
                }
                catch (const std::regex_error &)
                {
                }

                BufferPos from = {row, col};
                BufferPos to = {row, col + matchLen};
                buf->replaceRange(from, to, replaceInput_);
                replaced++;
            }

            editor_.setStatusMessage("Replaced " + std::to_string(replaced) + " occurrences");
            updateFindMatches();
        }

        // ── Overlay dialog rendering ─────────────────────────────────

        void renderGotoLineDialog(std::vector<std::vector<Cell>> &cells) const
        {
            // Render at top of editor area as a small bar
            int barRow = editorRect_.y;
            int barStartCol = editorRect_.x;
            int barWidth = std::min(40, editorRect_.w);

            if (barRow < 0 || barRow >= totalRows_)
                return;

            Color barBg = {60, 60, 60};
            Color barFg = {220, 220, 220};
            Color labelFg = {180, 180, 220};

            // Fill background
            for (int c = barStartCol; c < barStartCol + barWidth && c < totalCols_; c++)
            {
                cells[barRow][c].bg = barBg;
                cells[barRow][c].ch = U' ';
                cells[barRow][c].dirty = true;
            }

            // Label: "Go to Line: "
            std::string label = " Go to Line: ";
            int col = barStartCol;
            for (char ch : label)
            {
                if (col < totalCols_)
                {
                    cells[barRow][col].ch = (char32_t)ch;
                    cells[barRow][col].fg = labelFg;
                    cells[barRow][col].bg = barBg;
                    cells[barRow][col].dirty = true;
                    col++;
                }
            }

            // Input text
            for (char ch : gotoLineInput_)
            {
                if (col < barStartCol + barWidth && col < totalCols_)
                {
                    cells[barRow][col].ch = (char32_t)ch;
                    cells[barRow][col].fg = barFg;
                    cells[barRow][col].bg = barBg;
                    cells[barRow][col].dirty = true;
                    col++;
                }
            }

            // Cursor indicator (block)
            if (col < barStartCol + barWidth && col < totalCols_)
            {
                cells[barRow][col].ch = U'█';
                cells[barRow][col].fg = barFg;
                cells[barRow][col].bg = barBg;
                cells[barRow][col].dirty = true;
            }
        }

        void renderFindReplaceDialog(std::vector<std::vector<Cell>> &cells) const
        {
            int barWidth = std::min(60, editorRect_.w);
            // Position at TOP-RIGHT of editor area
            int barStartCol = editorRect_.x + editorRect_.w - barWidth;
            int startRow = editorRect_.y;
            int numRows = findReplaceMode_ ? 2 : 1;

            Color barBg = {45, 45, 45};
            Color activeBg = {55, 55, 70};
            Color labelFg = {180, 180, 220};
            Color textFg = {220, 220, 220};
            Color matchFg = {255, 200, 80};
            Color btnFg = {100, 200, 100};
            Color btnHoverFg = {160, 255, 160};
            Color btnHoverBg = {70, 70, 90};

            // ── Find row ──
            int row1 = startRow;
            if (row1 < 0 || row1 >= totalRows_)
                return;

            Color row1Bg = (!findReplaceEditingReplace_) ? activeBg : barBg;
            for (int c = barStartCol; c < barStartCol + barWidth && c < totalCols_; c++)
            {
                cells[row1][c].bg = row1Bg;
                cells[row1][c].ch = U' ';
                cells[row1][c].dirty = true;
            }

            int col = barStartCol;
            // Find label
            std::string findLabel = " Find: ";
            for (char ch : findLabel)
            {
                if (col < totalCols_)
                {
                    cells[row1][col].ch = (char32_t)ch;
                    cells[row1][col].fg = labelFg;
                    cells[row1][col].bg = row1Bg;
                    cells[row1][col].dirty = true;
                    col++;
                }
            }

            // Store find input field start column for click detection
            findInputStartCol_ = col;

            // Find input text
            for (char ch : findInput_)
            {
                if (col < barStartCol + barWidth - 12 && col < totalCols_)
                {
                    cells[row1][col].ch = (char32_t)ch;
                    cells[row1][col].fg = textFg;
                    cells[row1][col].bg = row1Bg;
                    cells[row1][col].dirty = true;
                    col++;
                }
            }

            // Cursor block (when editing find)
            if (!findReplaceEditingReplace_ && col < barStartCol + barWidth - 12 && col < totalCols_)
            {
                cells[row1][col].ch = U'█';
                cells[row1][col].fg = textFg;
                cells[row1][col].bg = row1Bg;
                cells[row1][col].dirty = true;
                col++;
            }

            // Match count + navigation buttons on the right
            struct BtnCell
            {
                char32_t ch;
                Color fg;
            };
            std::string matchStr;
            if (findMatches_.empty())
                matchStr = "0/0";
            else
                matchStr = std::to_string(currentMatch_ + 1) + "/" + std::to_string(findMatches_.size());

            // Compute button positions FIRST, then render with hover state
            int rightCellCount = 1 + (int)matchStr.size() + 1 + 1 + 1 + 1 + 1 + 1; // " N ▲ ▼ ✕"
            int rightStart = barStartCol + barWidth - rightCellCount - 1;

            // Store positions for click detection
            findPrevBtnCol_ = rightStart + 1 + (int)matchStr.size() + 1;
            findNextBtnCol_ = findPrevBtnCol_ + 2;
            findCloseBtnCol_ = findNextBtnCol_ + 2;
            findDialogRow_ = row1;
            findDialogStartCol_ = barStartCol;
            findDialogWidth_ = barWidth;

            // Build right-side cells with hover highlighting
            if (rightStart > col)
            {
                int rc = rightStart;
                // Only highlight a button cell if the mouse is ON that specific button
                auto putCell = [&](char32_t ch, Color fg, int btnCol)
                {
                    if (rc < totalCols_)
                    {
                        bool hovered = (btnCol >= 0 && findHoverRow_ == row1 &&
                                        findHoverCol_ >= 0 && std::abs(findHoverCol_ - btnCol) <= 0);
                        cells[row1][rc].ch = ch;
                        cells[row1][rc].fg = hovered ? btnHoverFg : fg;
                        cells[row1][rc].bg = hovered ? btnHoverBg : row1Bg;
                        cells[row1][rc].dirty = true;
                        rc++;
                    }
                };
                // " " count " "
                putCell(U' ', matchFg, -1);
                for (char ch : matchStr)
                    putCell((char32_t)ch, matchFg, -1);
                putCell(U' ', matchFg, -1);
                // ▲ prev
                putCell(U'▲', btnFg, findPrevBtnCol_);
                putCell(U' ', row1Bg, -1);
                // ▼ next
                putCell(U'▼', btnFg, findNextBtnCol_);
                putCell(U' ', row1Bg, -1);
                // ✕ close
                putCell(U'✕', {200, 80, 80}, findCloseBtnCol_);
            }

            // ── Replace row (if in replace mode) ──
            if (findReplaceMode_ && numRows > 1)
            {
                int row2 = startRow + 1;
                if (row2 >= totalRows_)
                    return;

                Color row2Bg = findReplaceEditingReplace_ ? activeBg : barBg;
                for (int c = barStartCol; c < barStartCol + barWidth && c < totalCols_; c++)
                {
                    cells[row2][c].bg = row2Bg;
                    cells[row2][c].ch = U' ';
                    cells[row2][c].dirty = true;
                }

                col = barStartCol;
                std::string replLabel = " Repl: ";
                for (char ch : replLabel)
                {
                    if (col < totalCols_)
                    {
                        cells[row2][col].ch = (char32_t)ch;
                        cells[row2][col].fg = labelFg;
                        cells[row2][col].bg = row2Bg;
                        cells[row2][col].dirty = true;
                        col++;
                    }
                }

                // Store replace input field start column for click detection
                replaceInputStartCol_ = col;

                for (char ch : replaceInput_)
                {
                    if (col < barStartCol + barWidth - 8 && col < totalCols_)
                    {
                        cells[row2][col].ch = (char32_t)ch;
                        cells[row2][col].fg = textFg;
                        cells[row2][col].bg = row2Bg;
                        cells[row2][col].dirty = true;
                        col++;
                    }
                }

                // Cursor block (when editing replace)
                if (findReplaceEditingReplace_ && col < barStartCol + barWidth - 8 && col < totalCols_)
                {
                    cells[row2][col].ch = U'█';
                    cells[row2][col].fg = textFg;
                    cells[row2][col].bg = row2Bg;
                    cells[row2][col].dirty = true;
                    col++;
                }

                // Replace button on the right: [⇄] [All]
                struct BtnCell2
                {
                    char32_t ch;
                    Color fg;
                };
                std::vector<BtnCell2> replBtns = {
                    {U' ', btnFg}, {U'R', btnFg}, {U'e', btnFg}, {U'p', btnFg}, {U'l', btnFg}, {U' ', barBg}, {U'A', btnFg}, {U'l', btnFg}, {U'l', btnFg}, {U' ', btnFg}};
                int rbStart = barStartCol + barWidth - (int)replBtns.size() - 1;

                if (rbStart > col)
                {
                    int rc = rbStart;
                    // "Repl" button spans rbStart+1 to rbStart+4
                    replaceBtnCol_ = rbStart + 1;
                    // "All" button spans rbStart+6 to rbStart+8
                    replaceAllBtnCol_ = rbStart + 6;

                    for (size_t i = 0; i < replBtns.size(); i++)
                    {
                        if (rc < totalCols_)
                        {
                            // Check if mouse hovers over THIS specific button group
                            bool onReplBtn = (findHoverRow_ == row2 && findHoverCol_ >= replaceBtnCol_ && findHoverCol_ < replaceBtnCol_ + 4);
                            bool onAllBtn = (findHoverRow_ == row2 && findHoverCol_ >= replaceAllBtnCol_ && findHoverCol_ < replaceAllBtnCol_ + 3);
                            bool isInReplRange = (rc >= replaceBtnCol_ && rc < replaceBtnCol_ + 4);
                            bool isInAllRange = (rc >= replaceAllBtnCol_ && rc < replaceAllBtnCol_ + 3);
                            bool hovered = (onReplBtn && isInReplRange) || (onAllBtn && isInAllRange);

                            cells[row2][rc].ch = replBtns[i].ch;
                            cells[row2][rc].fg = hovered ? btnHoverFg : replBtns[i].fg;
                            cells[row2][rc].bg = hovered ? btnHoverBg : row2Bg;
                            cells[row2][rc].dirty = true;
                            rc++;
                        }
                    }
                }

                replaceDialogRow_ = row2;
            }
        }
    };

} // namespace xterm
