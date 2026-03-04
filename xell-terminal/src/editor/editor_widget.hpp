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
#include <filesystem>
#include "text_buffer.hpp"
#include "editor_view.hpp"
#include "editor_input.hpp"
#include "../terminal/types.hpp"
#include "../theme/theme_loader.hpp"

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
            // Start with one empty tab
            newFile();
        }

        // ── File operations ─────────────────────────────────────────

        void openFile(const std::string &path)
        {
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

    private:
        const ThemeData &theme_;

        std::vector<EditorTab> tabs_;
        int activeTab_ = -1;
        std::unique_ptr<EditorInput> input_;

        int totalCols_ = 80;
        int totalRows_ = 24;
        bool showTabBar_ = true;
        std::string statusMessage_;

        // Theme colors
        Color tabBarBg_ = {30, 30, 30};
        Color tabActiveBg_ = {38, 38, 38};
        Color tabInactiveBg_ = {24, 24, 24};
        Color tabActiveFg_ = {230, 230, 230};
        Color tabInactiveFg_ = {128, 128, 128};
        Color tabBorder_ = {51, 51, 51};
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

                // Tab label: " filename.xel × "
                std::string label = " ";
                if (tab.filePath.empty())
                    label += "Untitled";
                else
                    label += std::filesystem::path(tab.filePath).filename().string();
                if (tab.buffer->isModified())
                    label += " ●";
                label += " ";

                Color fg = active ? tabActiveFg_ : tabInactiveFg_;
                Color bg = active ? tabActiveBg_ : tabInactiveBg_;

                for (int j = 0; j < (int)label.size() && col < totalCols_; j++, col++)
                {
                    row[col].ch = (char32_t)label[j];
                    row[col].fg = fg;
                    row[col].bg = bg;
                    row[col].dirty = true;
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

            // Write left
            for (int i = 0; i < (int)left.size() && i < totalCols_; i++)
            {
                row[i].ch = (char32_t)left[i];
            }

            // Write right (right-aligned)
            int rightStart = totalCols_ - (int)right.size();
            for (int i = 0; i < (int)right.size() && rightStart + i < totalCols_; i++)
            {
                if (rightStart + i >= 0)
                    row[rightStart + i].ch = (char32_t)right[i];
            }

            // Center: transient message
            if (!statusMessage_.empty())
            {
                int msgStart = (totalCols_ - (int)statusMessage_.size()) / 2;
                if (msgStart < (int)left.size() + 2)
                    msgStart = (int)left.size() + 2;
                for (int i = 0; i < (int)statusMessage_.size() && msgStart + i < rightStart - 1; i++)
                {
                    if (msgStart + i < totalCols_)
                        row[msgStart + i].ch = (char32_t)statusMessage_[i];
                }
            }
        }
    };

} // namespace xterm
