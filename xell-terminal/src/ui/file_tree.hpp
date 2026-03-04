#pragma once

// =============================================================================
// file_tree.hpp — File explorer panel for the Xell Terminal IDE
// =============================================================================
// Tree view of a project directory with keyboard navigation, expand/collapse,
// and file open callbacks. Renders using theme colors.
// =============================================================================

#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <functional>
#include "panel.hpp"
#include "../theme/theme_loader.hpp"

namespace xterm
{
    namespace fs = std::filesystem;

    // ─── Tree node ───────────────────────────────────────────────────────

    struct FileNode
    {
        std::string name;
        std::string fullPath;
        bool isDir = false;
        bool expanded = false;
        int depth = 0;
        std::vector<FileNode> children;
    };

    // ─── File Tree Panel ─────────────────────────────────────────────────

    class FileTreePanel : public Panel
    {
    public:
        // Callback when user opens a file
        using OpenFileCallback = std::function<void(const std::string &)>;

        FileTreePanel(const ThemeData &theme, OpenFileCallback onOpen = nullptr)
            : theme_(theme), onOpenFile_(onOpen)
        {
            loadColors();
        }

        PanelType type() const override { return PanelType::FileExplorer; }
        std::string title() const override { return "Explorer"; }

        // ── Set the root directory to display ────────────────────────

        void setRoot(const std::string &path)
        {
            rootPath_ = path;
            root_ = scanDirectory(path, 0);
            root_.expanded = true;
            flattenTree();
            selectedIdx_ = 0;
            scrollTop_ = 0;
        }

        // ── Refresh the tree ─────────────────────────────────────────

        void refresh()
        {
            if (!rootPath_.empty())
            {
                // Remember expanded state
                auto expanded = getExpandedPaths();
                root_ = scanDirectory(rootPath_, 0);
                root_.expanded = true;
                restoreExpanded(root_, expanded);
                flattenTree();
            }
        }

        // ── Rendering ───────────────────────────────────────────────

        std::vector<std::vector<Cell>> render() const override
        {
            auto grid = makeGrid(bgColor_);

            if (rect_.h < 1 || rect_.w < 1)
                return grid;

            // Title bar
            if (rect_.h > 1)
            {
                writeString(grid[0], 1, " EXPLORER", titleFg_, titleBg_, true);
                for (auto &c : grid[0])
                {
                    c.bg = titleBg_;
                    c.dirty = true;
                }
            }

            // Tree entries
            int startRow = 1;
            for (int i = 0; i < rect_.h - startRow && scrollTop_ + i < (int)flatList_.size(); i++)
            {
                int idx = scrollTop_ + i;
                auto &node = flatList_[idx];
                int row = startRow + i;

                bool isSelected = (idx == selectedIdx_);
                Color bg = isSelected ? selectedBg_ : bgColor_;
                Color fg = node.isDir ? dirFg_ : fileFg_;

                // Fill background
                for (auto &c : grid[row])
                {
                    c.bg = bg;
                    c.dirty = true;
                }

                // Indent
                int indent = node.depth * 2 + 1;

                // Icon
                std::string icon;
                if (node.isDir)
                    icon = node.expanded ? "▾ " : "▸ ";
                else
                    icon = "  ";

                writeString(grid[row], indent, icon + node.name, fg, bg, isSelected);
            }

            return grid;
        }

        // ── Keyboard handling ───────────────────────────────────────

        bool handleKeyDown(const SDL_Event &event) override
        {
            auto key = event.key.keysym.sym;
            bool ctrl = (event.key.keysym.mod & KMOD_CTRL) != 0;
            (void)ctrl;

            switch (key)
            {
            case SDLK_UP:
                if (selectedIdx_ > 0)
                    selectedIdx_--;
                ensureVisible();
                return true;

            case SDLK_DOWN:
                if (selectedIdx_ < (int)flatList_.size() - 1)
                    selectedIdx_++;
                ensureVisible();
                return true;

            case SDLK_LEFT:
                collapseOrParent();
                return true;

            case SDLK_RIGHT:
                expandOrChild();
                return true;

            case SDLK_RETURN:
                activateSelected();
                return true;

            default:
                break;
            }
            return false;
        }

        bool handleMouseClick(int row, int /*col*/, bool /*shift*/) override
        {
            int idx = scrollTop_ + row - 1; // -1 for title bar
            if (idx >= 0 && idx < (int)flatList_.size())
            {
                selectedIdx_ = idx;
                activateSelected();
                return true;
            }
            return false;
        }

        bool handleMouseWheel(int delta) override
        {
            scrollTop_ -= delta * 3;
            scrollTop_ = std::clamp(scrollTop_, 0, std::max(0, (int)flatList_.size() - (rect_.h - 1)));
            return true;
        }

        // ── Accessors ──────────────────────────────────────────────

        const std::string &rootPath() const { return rootPath_; }
        void setOnOpenFile(OpenFileCallback cb) { onOpenFile_ = cb; }

    private:
        const ThemeData &theme_;
        OpenFileCallback onOpenFile_;

        FileNode root_;
        std::string rootPath_;
        std::vector<FileNode> flatList_; // flattened visible entries
        int selectedIdx_ = 0;
        int scrollTop_ = 0;

        // Theme colors
        Color bgColor_ = {24, 24, 24};
        Color titleBg_ = {30, 30, 30};
        Color titleFg_ = {187, 187, 187};
        Color selectedBg_ = {38, 79, 120};
        Color dirFg_ = {220, 195, 120};
        Color fileFg_ = {204, 204, 204};

        void loadColors()
        {
            bgColor_ = getUIColor(theme_, "sidebar_bg", bgColor_);
            titleBg_ = getUIColor(theme_, "sidebar_header_bg", titleBg_);
            titleFg_ = getUIColor(theme_, "sidebar_header_fg", titleFg_);
            selectedBg_ = getUIColor(theme_, "selection_bg", selectedBg_);
        }

        // ── Directory scanning ──────────────────────────────────────

        FileNode scanDirectory(const std::string &path, int depth)
        {
            FileNode node;
            node.fullPath = path;
            node.name = fs::path(path).filename().string();
            node.isDir = fs::is_directory(path);
            node.depth = depth;

            if (node.isDir && depth < maxDepth_)
            {
                try
                {
                    std::vector<FileNode> dirs, files;
                    for (auto &entry : fs::directory_iterator(path))
                    {
                        std::string name = entry.path().filename().string();

                        // Skip hidden files, build dirs, caches
                        if (name.empty() || name[0] == '.')
                            continue;
                        if (name == "build" || name == "build_release" ||
                            name == "__pycache__" || name == "node_modules" ||
                            name == "__xelcache__")
                            continue;

                        auto child = scanDirectory(entry.path().string(), depth + 1);
                        if (child.isDir)
                            dirs.push_back(std::move(child));
                        else
                            files.push_back(std::move(child));
                    }

                    // Sort: directories first (alphabetical), then files (alphabetical)
                    std::sort(dirs.begin(), dirs.end(),
                              [](const FileNode &a, const FileNode &b)
                              { return a.name < b.name; });
                    std::sort(files.begin(), files.end(),
                              [](const FileNode &a, const FileNode &b)
                              { return a.name < b.name; });

                    for (auto &d : dirs)
                        node.children.push_back(std::move(d));
                    for (auto &f : files)
                        node.children.push_back(std::move(f));
                }
                catch (...)
                {
                    // Permission denied, etc.
                }
            }
            return node;
        }

        // ── Flatten visible tree into a list ────────────────────────

        void flattenTree()
        {
            flatList_.clear();
            flattenNode(root_);
        }

        void flattenNode(const FileNode &node)
        {
            flatList_.push_back(node);
            if (node.isDir && node.expanded)
            {
                for (auto &child : node.children)
                    flattenNode(child);
            }
        }

        // ── Navigation helpers ──────────────────────────────────────

        void collapseOrParent()
        {
            if (selectedIdx_ < 0 || selectedIdx_ >= (int)flatList_.size())
                return;
            auto &node = flatList_[selectedIdx_];
            if (node.isDir && node.expanded)
            {
                // Find the actual node and collapse it
                setExpanded(node.fullPath, false);
                flattenTree();
            }
            else
            {
                // Go to parent
                int parentDepth = node.depth - 1;
                for (int i = selectedIdx_ - 1; i >= 0; i--)
                {
                    if (flatList_[i].depth == parentDepth && flatList_[i].isDir)
                    {
                        selectedIdx_ = i;
                        break;
                    }
                }
            }
            ensureVisible();
        }

        void expandOrChild()
        {
            if (selectedIdx_ < 0 || selectedIdx_ >= (int)flatList_.size())
                return;
            auto &node = flatList_[selectedIdx_];
            if (node.isDir && !node.expanded)
            {
                setExpanded(node.fullPath, true);
                flattenTree();
            }
            else if (node.isDir && node.expanded && selectedIdx_ + 1 < (int)flatList_.size())
            {
                selectedIdx_++;
            }
            ensureVisible();
        }

        void activateSelected()
        {
            if (selectedIdx_ < 0 || selectedIdx_ >= (int)flatList_.size())
                return;
            auto &node = flatList_[selectedIdx_];
            if (node.isDir)
            {
                // Toggle expand
                setExpanded(node.fullPath, !node.expanded);
                flattenTree();
            }
            else
            {
                // Open file
                if (onOpenFile_)
                    onOpenFile_(node.fullPath);
            }
        }

        void ensureVisible()
        {
            if (selectedIdx_ < scrollTop_)
                scrollTop_ = selectedIdx_;
            if (selectedIdx_ >= scrollTop_ + rect_.h - 1)
                scrollTop_ = selectedIdx_ - rect_.h + 2;
            scrollTop_ = std::max(0, scrollTop_);
        }

        // ── Expansion state ─────────────────────────────────────────

        void setExpanded(const std::string &path, bool expanded)
        {
            setExpandedHelper(root_, path, expanded);
        }

        void setExpandedHelper(FileNode &node, const std::string &path, bool expanded)
        {
            if (node.fullPath == path)
            {
                node.expanded = expanded;
                return;
            }
            for (auto &child : node.children)
                setExpandedHelper(child, path, expanded);
        }

        std::vector<std::string> getExpandedPaths() const
        {
            std::vector<std::string> result;
            collectExpanded(root_, result);
            return result;
        }

        void collectExpanded(const FileNode &node, std::vector<std::string> &result) const
        {
            if (node.isDir && node.expanded)
            {
                result.push_back(node.fullPath);
                for (auto &child : node.children)
                    collectExpanded(child, result);
            }
        }

        void restoreExpanded(FileNode &node, const std::vector<std::string> &paths)
        {
            for (auto &p : paths)
            {
                if (node.fullPath == p)
                {
                    node.expanded = true;
                    break;
                }
            }
            for (auto &child : node.children)
                restoreExpanded(child, paths);
        }

        static constexpr int maxDepth_ = 8;
    };

} // namespace xterm
