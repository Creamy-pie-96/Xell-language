#pragma once

// =============================================================================
// editor_widget.hpp — Unified editor component for the Xell Terminal IDE
// =============================================================================
// Combines TextBuffer + EditorView + EditorInput + StatusBar into a single
// component that can be dropped into the terminal's main loop.
//
// Usage:
//   ThemeData theme = loadDefaultTheme();
//   EditorWidget editor(theme);
//   editor.openFile("hello.xel");
//   editor.resize(cols, rows);
//
//   // In the event loop:
//   auto action = editor.handleEvent(sdl_event);
//   if (action == EditorAction::SAVE) editor.save();
//
//   // In the render loop:
//   auto output = editor.render();
//   // blit output.cells to the screen
// =============================================================================

#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <filesystem>
#include "text_buffer.hpp"
#include "editor_view.hpp"
#include "editor_input.hpp"
#include "../terminal/types.hpp"
#include "../theme/theme_loader.hpp"
#include "../ui/file_tree.hpp" // for fileIconForName

namespace xterm
{

    // ─── Status bar info ─────────────────────────────────────────────────

    struct StatusBarInfo
    {
        std::string filename;
        bool modified;
        int cursorRow; // 1-based
        int cursorCol; // 1-based
        int totalLines;
        std::string encoding = "UTF-8";
        std::string language = "Xell";
        std::string message; // transient status message
    };

    // ─── Tab (for future multi-tab support) ──────────────────────────────

    struct EditorTab
    {
        std::unique_ptr<TextBuffer> buffer;
        std::unique_ptr<EditorView> view;
        std::string filePath;
    };

    // ─── Editor Widget ───────────────────────────────────────────────────

    class EditorWidget
    {
    public:
        explicit EditorWidget(const ThemeData &theme)
            : theme_(theme)
        {
            loadThemeColors();
            // Start with no tabs — user opens files from file tree
        }

        // ── File operations ─────────────────────────────────────────

        void openFile(const std::string &path)
        {
            // Check if file is already open — switch to existing tab
            for (int i = 0; i < (int)tabs_.size(); i++)
            {
                if (tabs_[i].filePath == path)
                {
                    switchTab(i);
                    statusMessage_ = "Switched to: " + std::filesystem::path(path).filename().string();
                    return;
                }
            }

            auto buf = std::make_unique<TextBuffer>();
            if (buf->loadFromFile(path))
            {
                auto view = std::make_unique<EditorView>(*buf, theme_);
                view->setRect(editorRect());

                tabs_.push_back({std::move(buf), std::move(view), path});
                activeTab_ = (int)tabs_.size() - 1;
                input_ = std::make_unique<EditorInput>(*activeView());
                statusMessage_ = "Opened: " + std::filesystem::path(path).filename().string();
            }
            else
            {
                statusMessage_ = "Failed to open: " + path;
            }
        }

        void newFile()
        {
            auto buf = std::make_unique<TextBuffer>();
            auto view = std::make_unique<EditorView>(*buf, theme_);
            view->setRect(editorRect());

            tabs_.push_back({std::move(buf), std::move(view), ""});
            activeTab_ = (int)tabs_.size() - 1;
            input_ = std::make_unique<EditorInput>(*activeView());
            statusMessage_ = "New file";
        }

        bool save()
        {
            if (activeTab_ < 0)
                return false;
            auto &tab = tabs_[activeTab_];
            if (tab.filePath.empty())
            {
                statusMessage_ = "No file path — use Save As";
                return false;
            }
            if (tab.buffer->saveToFile(tab.filePath))
            {
                tab.buffer->markSaved();
                statusMessage_ = "Saved: " + std::filesystem::path(tab.filePath).filename().string();
                return true;
            }
            statusMessage_ = "Save failed!";
            return false;
        }

        bool saveAs(const std::string &path)
        {
            if (activeTab_ < 0)
                return false;
            auto &tab = tabs_[activeTab_];
            if (tab.buffer->saveToFile(path))
            {
                tab.filePath = path;
                tab.buffer->markSaved();
                statusMessage_ = "Saved: " + std::filesystem::path(path).filename().string();
                return true;
            }
            statusMessage_ = "Save failed!";
            return false;
        }

        void closeTab()
        {
            if (activeTab_ < 0 || tabs_.empty())
                return;
            tabs_.erase(tabs_.begin() + activeTab_);
            if (activeTab_ >= (int)tabs_.size())
                activeTab_ = (int)tabs_.size() - 1;
            if (activeTab_ >= 0)
                input_ = std::make_unique<EditorInput>(*activeView());
            else
                input_.reset();
        }

        void closeTabByPath(const std::string &path)
        {
            for (int i = 0; i < (int)tabs_.size(); i++)
            {
                if (tabs_[i].filePath == path)
                {
                    tabs_.erase(tabs_.begin() + i);
                    if (activeTab_ >= (int)tabs_.size())
                        activeTab_ = (int)tabs_.size() - 1;
                    if (activeTab_ >= 0)
                        input_ = std::make_unique<EditorInput>(*activeView());
                    else
                        input_.reset();
                    return;
                }
            }
        }

        void closeTabByIndex(int index)
        {
            if (index < 0 || index >= (int)tabs_.size())
                return;
            tabs_.erase(tabs_.begin() + index);
            if (activeTab_ >= (int)tabs_.size())
                activeTab_ = (int)tabs_.size() - 1;
            if (activeTab_ >= 0)
                input_ = std::make_unique<EditorInput>(*activeView());
            else
                input_.reset();
        }

        void switchTab(int index)
        {
            if (index >= 0 && index < (int)tabs_.size())
            {
                activeTab_ = index;
                input_ = std::make_unique<EditorInput>(*activeView());
            }
        }

        void nextTab()
        {
            if (tabs_.size() > 1)
                switchTab((activeTab_ + 1) % (int)tabs_.size());
        }

        void prevTab()
        {
            if (tabs_.size() > 1)
                switchTab((activeTab_ - 1 + (int)tabs_.size()) % (int)tabs_.size());
        }

        // ── Layout ──────────────────────────────────────────────────

        void resize(int totalCols, int totalRows)
        {
            totalCols_ = totalCols;
            totalRows_ = totalRows;

            // Recompute layout for all tabs
            auto rect = editorRect();
            for (auto &tab : tabs_)
                tab.view->setRect(rect);
        }

        // ── Event handling ──────────────────────────────────────────

        EditorAction handleEvent(const SDL_Event &event)
        {
            if (!input_)
                return EditorAction::NONE;

            if (event.type == SDL_KEYDOWN)
                return input_->handleKeyDown(event);

            if (event.type == SDL_TEXTINPUT)
                return input_->handleTextInput(event);

            if (event.type == SDL_MOUSEWHEEL)
            {
                // Check if mouse is in editor area
                return input_->handleMouseWheel(event.wheel.y);
            }

            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)
            {
                int screenRow = event.button.y; // caller should convert to cell coords
                int screenCol = event.button.x;
                bool shift = (SDL_GetModState() & KMOD_SHIFT) != 0;
                return input_->handleMouseClick(screenRow, screenCol, shift);
            }

            if (event.type == SDL_MOUSEMOTION && (event.motion.state & SDL_BUTTON_LMASK))
            {
                int screenRow = event.motion.y;
                int screenCol = event.motion.x;
                return input_->handleMouseDrag(screenRow, screenCol);
            }

            return EditorAction::NONE;
        }

        // ── Cell-coordinate mouse handlers (called by LayoutManager) ─

        EditorAction handleLocalClick(int cellRow, int cellCol, bool shift)
        {
            if (!input_)
                return EditorAction::NONE;
            // cellRow/cellCol are already in editor-local cell coordinates
            // Row 0 = tab bar, row 1..h-2 = code, row h-1 = status bar
            if (cellRow == 0)
            {
                handleTabBarClick(cellCol);
                return EditorAction::HANDLED;
            }

            // Check if click is on the scrollbar column (rightmost) — vertical scrollbar
            auto view = activeView();
            if (view && view->getRect().w > 0)
            {
                int editorW = view->getRect().w;
                int visibleH = view->codeAreaHeight();
                int gutterW = view->gutterWidth();
                int codeW = view->codeAreaWidth();

                // Vertical scrollbar (rightmost column)
                if (cellCol >= editorW - 1)
                {
                    // Scrollbar click — jump to proportional position
                    int codeRow = cellRow - 1; // subtract tab bar row
                    int totalLines = tabs_[activeTab_].buffer->lineCount();
                    if (totalLines > visibleH && visibleH > 0)
                    {
                        float ratio = (float)codeRow / (float)visibleH;
                        int targetLine = (int)(ratio * totalLines);
                        view->scrollTo(targetLine);
                    }
                    return EditorAction::HANDLED;
                }

                // Horizontal scrollbar (bottom row of code area)
                int hBarRow = visibleH; // +1 for tab bar = visibleH in cellRow coords
                if (cellRow == hBarRow && cellCol >= gutterW && cellCol < gutterW + codeW - 1)
                {
                    int maxWidth = view->maxLineWidth();
                    if (maxWidth > codeW)
                    {
                        int trackWidth = codeW - 1;
                        float ratio = (float)(cellCol - gutterW) / (float)trackWidth;
                        int targetCol = (int)(ratio * maxWidth) - codeW / 2;
                        view->scrollHorizontalTo(std::max(0, targetCol));
                        return EditorAction::HANDLED;
                    }
                }
            }

            // ── Gutter click: toggle breakpoint ─────────────────────
            if (view)
            {
                int gutterW = view->gutterWidth();
                int codeRow = cellRow - 1; // subtract tab bar
                if (cellCol < gutterW && codeRow >= 0)
                {
                    int bufRow = view->scrollTopLine() + codeRow;
                    if (bufRow < tabs_[activeTab_].buffer->lineCount())
                    {
                        view->toggleBreakpoint(bufRow);
                        // Signal to layout_manager via a flag
                        lastToggledBreakpointLine_ = bufRow;
                        return EditorAction::HANDLED;
                    }
                }
            }

            // Don't move cursor if clicking past the last line of the file
            if (view)
            {
                int codeRow = cellRow - 1; // subtract tab bar
                int bufRow = view->scrollTopLine() + codeRow;
                if (bufRow >= tabs_[activeTab_].buffer->lineCount())
                    return EditorAction::HANDLED; // consume click but don't move cursor
            }

            // Subtract 1 for tab bar
            return input_->handleMouseClick(cellRow - 1, cellCol, shift);
        }

        EditorAction handleLocalDrag(int cellRow, int cellCol)
        {
            if (!input_)
                return EditorAction::NONE;

            // Check for scrollbar drag
            auto view = activeView();
            if (view && view->getRect().w > 0)
            {
                int editorW = view->getRect().w;
                int visibleH = view->codeAreaHeight();
                int gutterW = view->gutterWidth();
                int codeW = view->codeAreaWidth();

                // Vertical scrollbar drag
                if (cellCol >= editorW - 1)
                {
                    int codeRow = cellRow - 1;
                    int totalLines = tabs_[activeTab_].buffer->lineCount();
                    if (totalLines > visibleH && visibleH > 0)
                    {
                        float ratio = (float)codeRow / (float)visibleH;
                        int targetLine = (int)(ratio * totalLines);
                        view->scrollTo(targetLine);
                    }
                    return EditorAction::HANDLED;
                }

                // Horizontal scrollbar drag
                int hBarRow = visibleH;
                if (cellRow == hBarRow && cellCol >= gutterW && cellCol < gutterW + codeW - 1)
                {
                    int maxWidth = view->maxLineWidth();
                    if (maxWidth > codeW)
                    {
                        int trackWidth = codeW - 1;
                        float ratio = (float)(cellCol - gutterW) / (float)trackWidth;
                        int targetCol = (int)(ratio * maxWidth) - codeW / 2;
                        view->scrollHorizontalTo(std::max(0, targetCol));
                        return EditorAction::HANDLED;
                    }
                }
            }

            return input_->handleMouseDrag(cellRow - 1, cellCol);
        }

        EditorAction handleLocalWheel(int delta)
        {
            if (!input_)
                return EditorAction::NONE;
            return input_->handleMouseWheel(delta);
        }

        // Horizontal scroll (for 2-finger swipe and Shift+scroll)
        EditorAction handleLocalHScroll(int delta)
        {
            auto view = activeView();
            if (!view)
                return EditorAction::NONE;
            int maxW = view->maxLineWidth();
            int codeW = view->codeAreaWidth();
            int maxScroll = std::max(0, maxW - codeW + 1);
            int newCol = std::clamp(view->scrollLeftCol() + delta, 0, maxScroll);
            view->scrollHorizontalTo(newCol);
            return EditorAction::HANDLED;
        }

        // ── Clipboard operations ────────────────────────────────────

        std::string getClipboardText() const
        {
            if (!activeView())
                return "";
            std::string text = activeView()->getSelectedText();
            if (text.empty())
            {
                // Copy whole line if no selection
                auto &buf = *tabs_[activeTab_].buffer;
                auto cursor = activeView()->cursor();
                text = buf.getLine(cursor.row) + "\n";
            }
            return text;
        }

        void pasteText(const std::string &text)
        {
            if (activeView())
                activeView()->insertText(text);
        }

        void cut()
        {
            if (!activeView())
                return;
            if (activeView()->selection().active)
            {
                // Cut is handled by caller: they get text, then we delete
                activeView()->deleteSelected();
            }
        }

        // ── Rendering ───────────────────────────────────────────────

        struct FullRenderOutput
        {
            std::vector<std::vector<Cell>> cells; // [row][col] — full screen
            int cursorScreenRow = -1;
            int cursorScreenCol = -1;
        };

        FullRenderOutput render()
        {
            FullRenderOutput out;
            out.cells.resize(totalRows_);
            for (auto &row : out.cells)
                row.resize(totalCols_);

            // ── Tab bar (row 0) ──────────────────────────────────────
            if (showTabBar_)
                renderTabBar(out.cells[0]);

            // ── Editor area ──────────────────────────────────────────
            if (activeTab_ >= 0 && activeView())
            {
                auto editorOut = activeView()->render();
                int startRow = showTabBar_ ? 1 : 0;
                for (int r = 0; r < (int)editorOut.cells.size() && startRow + r < totalRows_ - 1; r++)
                {
                    for (int c = 0; c < (int)editorOut.cells[r].size() && c < totalCols_; c++)
                    {
                        out.cells[startRow + r][c] = editorOut.cells[r][c];
                    }
                }

                // Map cursor position
                if (editorOut.cursorScreenRow >= 0)
                {
                    out.cursorScreenRow = editorOut.cursorScreenRow + startRow;
                    out.cursorScreenCol = editorOut.cursorScreenCol;
                }
            }

            // ── Status bar (last row) ────────────────────────────────
            renderStatusBar(out.cells[totalRows_ - 1]);

            return out;
        }

        // ── Status info ─────────────────────────────────────────────

        StatusBarInfo getStatusInfo() const
        {
            StatusBarInfo info;
            if (activeTab_ >= 0)
            {
                auto &tab = tabs_[activeTab_];
                info.filename = tab.filePath.empty() ? "Untitled" : tab.buffer->fileName();
                info.modified = tab.buffer->isModified();
                info.cursorRow = activeView()->cursor().row + 1;
                info.cursorCol = activeView()->cursor().col + 1;
                info.totalLines = tab.buffer->lineCount();
            }
            info.message = statusMessage_;
            return info;
        }

        // ── Accessors ───────────────────────────────────────────────

        int tabCount() const { return (int)tabs_.size(); }
        int activeTabIndex() const { return activeTab_; }
        bool hasUnsavedChanges() const
        {
            for (auto &tab : tabs_)
                if (tab.buffer->isModified())
                    return true;
            return false;
        }

        void setShowTabBar(bool show) { showTabBar_ = show; }
        void setStatusMessage(const std::string &msg) { statusMessage_ = msg; }

        // Go to a specific line (0-based)
        void goToLine(int line)
        {
            auto view = activeView();
            if (!view || activeTab_ < 0)
                return;
            auto &buf = *tabs_[activeTab_].buffer;
            line = std::max(0, std::min(line, buf.lineCount() - 1));
            view->setCursor({line, 0});
            statusMessage_ = "Line " + std::to_string(line + 1);
        }

        // Check if editor has a selection
        bool hasSelection() const
        {
            auto view = activeView();
            return view && view->selection().active;
        }

        // Get selected text (empty if no selection)
        std::string getSelectedText() const
        {
            auto view = activeView();
            if (!view || activeTab_ < 0)
                return "";
            return view->getSelectedText();
        }

        // Get the text buffer for regex searching
        const TextBuffer *activeBuffer() const
        {
            if (activeTab_ >= 0 && activeTab_ < (int)tabs_.size())
                return tabs_[activeTab_].buffer.get();
            return nullptr;
        }

        // Get mutable buffer (for replace operations)
        TextBuffer *activeBufferMut()
        {
            if (activeTab_ >= 0 && activeTab_ < (int)tabs_.size())
                return tabs_[activeTab_].buffer.get();
            return nullptr;
        }

        // Set cursor to specific row/col and ensure visible
        void setCursorPosition(int row, int col)
        {
            auto view = activeView();
            if (!view)
                return;
            view->setCursor({row, col});
        }

        std::string getCurrentFilePath() const
        {
            if (activeTab_ >= 0 && activeTab_ < (int)tabs_.size())
                return tabs_[activeTab_].filePath;
            return "";
        }

        // Get text from line 0 to the cursor row (inclusive)
        std::string getTextToCursorLine() const
        {
            auto view = activeView();
            if (!view || activeTab_ < 0)
                return "";
            auto &buf = *tabs_[activeTab_].buffer;
            int endRow = view->cursor().row;
            std::string result;
            for (int r = 0; r <= endRow && r < buf.lineCount(); r++)
            {
                if (r > 0)
                    result += '\n';
                result += buf.getLine(r);
            }
            return result;
        }

        // Get the full buffer content (all lines)
        std::string getFullBufferText() const
        {
            if (activeTab_ < 0 || activeTab_ >= (int)tabs_.size())
                return "";
            auto &buf = *tabs_[activeTab_].buffer;
            std::string result;
            for (int r = 0; r < buf.lineCount(); r++)
            {
                if (r > 0)
                    result += '\n';
                result += buf.getLine(r);
            }
            return result;
        }

        // Hover support for tab bar
        void setHoverCol(int col) { hoverCol_ = col; }

        // ── Inline diagnostics ──────────────────────────────────────

        void setEditorDiagnostics(const std::unordered_map<int, int> &diags)
        {
            if (activeView())
                activeView()->setDiagnostics(diags);
        }

        void clearEditorDiagnostics()
        {
            if (activeView())
                activeView()->clearDiagnostics();
        }

        // ── Debug support ───────────────────────────────────────────

        void setDebugLine(int line)
        {
            if (activeView())
                activeView()->setDebugLine(line);
        }

        void clearDebugLine()
        {
            if (activeView())
                activeView()->setDebugLine(-1);
        }

        void toggleBreakpoint(int line, const std::string &type = "pause")
        {
            if (activeView())
                activeView()->toggleBreakpoint(line, type);
        }

        void addBreakpoint(int line, const std::string &type = "pause")
        {
            if (activeView())
                activeView()->addBreakpoint(line, type);
        }

        void removeBreakpoint(int line)
        {
            if (activeView())
                activeView()->removeBreakpoint(line);
        }

        void clearBreakpoints()
        {
            if (activeView())
                activeView()->clearBreakpoints();
        }

        const std::unordered_map<int, std::string> *breakpoints() const
        {
            auto view = activeView();
            return view ? &view->breakpoints() : nullptr;
        }

        // Check if a breakpoint was toggled by a gutter click (returns line, -1 if none).
        // Consuming it resets to -1.
        int consumeBreakpointToggle()
        {
            int line = lastToggledBreakpointLine_;
            lastToggledBreakpointLine_ = -1;
            return line;
        }

        // Get the buffer row at the cursor position (0-based)
        int cursorRow() const
        {
            auto view = activeView();
            return view ? view->cursor().row : -1;
        }

        // ── Autocomplete support ────────────────────────────────────

        // Insert text at the current cursor position
        void insertTextAtCursor(const std::string &text)
        {
            auto view = activeView();
            if (!view)
                return;
            view->insertText(text);
        }

        // Set ghost text (inline autocomplete suggestion)
        void setGhostText(const std::string &text, int line, int col)
        {
            auto view = activeView();
            if (view)
                view->setGhostText(text, line, col);
        }

        void clearGhostText()
        {
            auto view = activeView();
            if (view)
                view->clearGhostText();
        }

        // Set active tab stop highlight (for snippet mode)
        void setActiveTabStop(int line, int col, int length)
        {
            auto view = activeView();
            if (view)
                view->setActiveTabStop(line, col, length);
        }

        void clearActiveTabStop()
        {
            auto view = activeView();
            if (view)
                view->clearActiveTabStop();
        }

        // Set selection range in the active editor
        void setSelection(int startRow, int startCol, int endRow, int endCol)
        {
            auto view = activeView();
            if (!view)
                return;
            view->setCursor({startRow, startCol});
            view->selection().active = true;
            view->selection().anchor = {startRow, startCol};
            view->selection().cursor = {endRow, endCol};
            view->setCursor({endRow, endCol});
        }

        void clearSelection()
        {
            auto view = activeView();
            if (view)
                view->clearSelection();
        }

        // Check if ghost text is currently showing
        bool hasGhostText() const
        {
            auto view = activeView();
            return view && view->hasGhostText();
        }

        // Get the ghost text string (for Tab acceptance)
        std::string getGhostText() const
        {
            auto view = activeView();
            if (view && view->hasGhostText())
                return view->ghostText();
            return "";
        }

        // ── Tab bar click ────────────────────────────────────────

        void handleTabBarClick(int col)
        {
            // Walk through tabs to find which one was clicked
            int c = 0;
            for (int i = 0; i < (int)tabs_.size(); i++)
            {
                auto &tab = tabs_[i];
                std::string label = " ";
                std::string filename;
                if (tab.filePath.empty())
                    filename = "Untitled";
                else
                    filename = std::filesystem::path(tab.filePath).filename().string();
                label += fileIconForName(filename, false) + " " + filename;
                if (tab.buffer->isModified())
                    label += " \xE2\x97\x8F"; // ●
                label += " \xC3\x97 ";        // ×

                int labelLen = utf8Len(label);
                int tabEnd = c + labelLen;
                if (col >= c && col < tabEnd)
                {
                    // Check if click is on the × (last 2 display columns: "× ")
                    if (col >= tabEnd - 2)
                    {
                        closeTabByIndex(i);
                    }
                    else
                    {
                        switchTab(i);
                    }
                    return;
                }
                c = tabEnd + 1; // +1 for separator │
            }
        }

    private:
        const ThemeData &theme_;

        std::vector<EditorTab> tabs_;
        int activeTab_ = -1;
        std::unique_ptr<EditorInput> input_;

        int totalCols_ = 80;
        int totalRows_ = 24;
        bool showTabBar_ = true;
        std::string statusMessage_;
        int hoverCol_ = -1;

        // Breakpoint toggle notification: set by gutter click, consumed by layout manager
        int lastToggledBreakpointLine_ = -1;

        // Theme colors
        Color tabBarBg_ = {30, 30, 30};
        Color tabActiveBg_ = {38, 38, 38};
        Color tabInactiveBg_ = {24, 24, 24};
        Color tabActiveFg_ = {230, 230, 230};
        Color tabInactiveFg_ = {128, 128, 128};
        Color tabBorder_ = {51, 51, 51};
        Color tabHoverBg_ = {45, 45, 45};
        Color statusBarBg_ = {0, 122, 204};
        Color statusBarFg_ = {255, 255, 255};

        void loadThemeColors()
        {
            tabBarBg_ = getUIColor(theme_, "tab_bar_bg", tabBarBg_);
            tabActiveBg_ = getUIColor(theme_, "tab_active_bg", tabActiveBg_);
            tabInactiveBg_ = getUIColor(theme_, "tab_inactive_bg", tabInactiveBg_);
            tabActiveFg_ = getUIColor(theme_, "tab_active_fg", tabActiveFg_);
            tabInactiveFg_ = getUIColor(theme_, "tab_inactive_fg", tabInactiveFg_);
            tabBorder_ = getUIColor(theme_, "tab_border", tabBorder_);
            statusBarBg_ = getUIColor(theme_, "statusbar_bg", statusBarBg_);
            statusBarFg_ = getUIColor(theme_, "statusbar_fg", statusBarFg_);
        }

        EditorView *activeView() const
        {
            if (activeTab_ >= 0 && activeTab_ < (int)tabs_.size())
                return tabs_[activeTab_].view.get();
            return nullptr;
        }

        Rect editorRect() const
        {
            int startRow = showTabBar_ ? 1 : 0;
            int height = totalRows_ - startRow - 1; // -1 for status bar
            return {0, startRow, totalCols_, std::max(1, height)};
        }

        // ── Tab bar rendering ────────────────────────────────────────

        void renderTabBar(std::vector<Cell> &row) const
        {
            // Fill background
            for (int c = 0; c < totalCols_; c++)
            {
                row[c].ch = U' ';
                row[c].bg = tabBarBg_;
                row[c].fg = tabInactiveFg_;
                row[c].dirty = true;
            }

            // Render each tab
            int col = 0;
            for (int i = 0; i < (int)tabs_.size() && col < totalCols_; i++)
            {
                bool active = (i == activeTab_);
                auto &tab = tabs_[i];

                // Tab label: " icon filename.xel ● × "
                std::string label = " ";
                std::string filename;
                if (tab.filePath.empty())
                    filename = "Untitled";
                else
                    filename = std::filesystem::path(tab.filePath).filename().string();
                label += fileIconForName(filename, false) + " " + filename;
                if (tab.buffer->isModified())
                    label += " \xE2\x97\x8F"; // ●
                label += " \xC3\x97 ";        // ×

                Color fg = active ? tabActiveFg_ : tabInactiveFg_;
                Color bg = active ? tabActiveBg_ : tabInactiveBg_;

                // Check if this tab is hovered (not active, not already highlighted)
                int labelLen = utf8Len(label);

                // Check if hover is on the × close button (last 3 display cols: "× ")
                bool hoverOnClose = (hoverCol_ >= col + labelLen - 3 && hoverCol_ < col + labelLen);

                if (!active && hoverCol_ >= col && hoverCol_ < col + labelLen && !hoverOnClose)
                    bg = tabHoverBg_;

                // Write label with per-character color override for × hover
                {
                    size_t si = 0;
                    int c = col;
                    int charIdx = 0;
                    while (si < label.size() && c < (int)row.size())
                    {
                        row[c].ch = utf8Decode(label, si);
                        row[c].fg = fg;
                        row[c].bg = bg;
                        // If hovering on × region, make those chars red bg
                        if (hoverOnClose && charIdx >= labelLen - 3)
                        {
                            row[c].bg = {180, 40, 40};
                            row[c].fg = {255, 255, 255};
                        }
                        row[c].dirty = true;
                        c++;
                        charIdx++;
                    }
                    col = c;
                }

                // Tab separator
                if (col < totalCols_)
                {
                    row[col].ch = U'│';
                    row[col].fg = tabBorder_;
                    row[col].bg = tabBarBg_;
                    row[col].dirty = true;
                    col++;
                }
            }
        }

        // ── Status bar rendering ─────────────────────────────────────

        void renderStatusBar(std::vector<Cell> &row) const
        {
            // Fill background
            for (int c = 0; c < totalCols_; c++)
            {
                row[c].ch = U' ';
                row[c].bg = statusBarBg_;
                row[c].fg = statusBarFg_;
                row[c].dirty = true;
            }

            // Left side: mode + file info
            std::string left = " EDIT";
            if (activeTab_ >= 0)
            {
                auto info = getStatusInfo();
                left += " │ " + info.filename;
                if (info.modified)
                    left += " [+]";
            }

            // Right side: cursor position + language
            std::string right;
            if (activeTab_ >= 0)
            {
                auto info = getStatusInfo();
                right = "Ln " + std::to_string(info.cursorRow) +
                        ", Col " + std::to_string(info.cursorCol) +
                        " │ " + info.language +
                        " │ " + info.encoding + " ";
            }

            // Write left (UTF-8 aware)
            utf8Write(row, 0, left, statusBarFg_, statusBarBg_);

            // Write right (right-aligned, UTF-8 aware)
            int rightLen = utf8Len(right);
            int rightStart = totalCols_ - rightLen;
            utf8Write(row, rightStart, right, statusBarFg_, statusBarBg_);

            // Center: transient message
            if (!statusMessage_.empty())
            {
                int leftLen = utf8Len(left);
                int msgLen = utf8Len(statusMessage_);
                int msgStart = (totalCols_ - msgLen) / 2;
                if (msgStart < leftLen + 2)
                    msgStart = leftLen + 2;
                utf8Write(row, msgStart, statusMessage_, statusBarFg_, statusBarBg_);
            }
        }
    };

} // namespace xterm
