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
#include <fstream>
#include "panel.hpp"
#include "../theme/theme_loader.hpp"

namespace xterm
{
    namespace fs = std::filesystem;

    // ─── File icon helper ────────────────────────────────────────────────
    // Maps file extensions to Nerd Font icons for display in file tree
    // and tab bar. Uses devicons/material design icons from Nerd Fonts.
    //
    // Nerd Font icon codepoints reference:
    //   https://www.nerdfonts.com/cheat-sheet

    // Helper: convert a char32_t Nerd Font codepoint to a UTF-8 string
    inline std::string nfIcon(char32_t cp)
    {
        std::string s;
        if (cp < 0x80)
        {
            s += (char)cp;
        }
        else if (cp < 0x800)
        {
            s += (char)(0xC0 | (cp >> 6));
            s += (char)(0x80 | (cp & 0x3F));
        }
        else if (cp < 0x10000)
        {
            s += (char)(0xE0 | (cp >> 12));
            s += (char)(0x80 | ((cp >> 6) & 0x3F));
            s += (char)(0x80 | (cp & 0x3F));
        }
        else
        {
            s += (char)(0xF0 | (cp >> 18));
            s += (char)(0x80 | ((cp >> 12) & 0x3F));
            s += (char)(0x80 | ((cp >> 6) & 0x3F));
            s += (char)(0x80 | (cp & 0x3F));
        }
        return s;
    }

    inline std::string fileIconForName(const std::string &name, bool isDir)
    {
        // Nerd Font devicon codepoints
        // Folders
        if (isDir)
        {
            // Special folder names
            if (name == ".git")
                return nfIcon(0xe5fb); //
            if (name == "node_modules")
                return nfIcon(0xe718); //
            if (name == "src")
                return nfIcon(0xf07c); //
            if (name == "test" || name == "tests")
                return nfIcon(0xf07c);
            if (name == "build" || name == "dist")
                return nfIcon(0xf07c);
            return nfIcon(0xf07b); //  (generic folder)
        }

        // Special filenames
        std::string lower = name;
        for (auto &c : lower)
            c = (char)std::tolower((unsigned char)c);

        if (lower == "makefile" || lower == "cmakeLists.txt")
            return nfIcon(0xe779); //
        if (lower == "dockerfile")
            return nfIcon(0xf308); //
        if (lower == ".gitignore" || lower == ".gitmodules")
            return nfIcon(0xe5fb);
        if (lower == "license" || lower == "licence")
            return nfIcon(0xf0219);
        if (lower == "readme.md")
            return nfIcon(0xf48a); //

        // Extract extension
        auto dotPos = name.rfind('.');
        if (dotPos == std::string::npos)
            return nfIcon(0xf15b); //  (generic file)

        std::string ext = name.substr(dotPos);
        for (auto &c : ext)
            c = (char)std::tolower((unsigned char)c);

        // Programming languages
        if (ext == ".xel" || ext == ".xell")
            return nfIcon(0xe7a8); //  (lambda/custom)
        if (ext == ".cpp" || ext == ".cc" || ext == ".cxx")
            return nfIcon(0xe61d); //
        if (ext == ".hpp" || ext == ".h" || ext == ".hxx")
            return nfIcon(0xe61d); //
        if (ext == ".c")
            return nfIcon(0xe61e); //
        if (ext == ".py")
            return nfIcon(0xe73c); //
        if (ext == ".js")
            return nfIcon(0xe74e); //
        if (ext == ".ts")
            return nfIcon(0xe628); //
        if (ext == ".rs")
            return nfIcon(0xe7a8); //
        if (ext == ".go")
            return nfIcon(0xe626); //
        if (ext == ".java")
            return nfIcon(0xe738); //
        if (ext == ".rb")
            return nfIcon(0xe739); //
        if (ext == ".lua")
            return nfIcon(0xe620); //
        if (ext == ".sh" || ext == ".bash" || ext == ".zsh")
            return nfIcon(0xf489); //
        if (ext == ".ps1")
            return nfIcon(0xf489);
        if (ext == ".swift")
            return nfIcon(0xe755); //
        if (ext == ".kt")
            return nfIcon(0xe634); //
        if (ext == ".dart")
            return nfIcon(0xe798); //
        if (ext == ".r")
            return nfIcon(0xf25d);

        // Web
        if (ext == ".html" || ext == ".htm")
            return nfIcon(0xe736); //
        if (ext == ".css")
            return nfIcon(0xe749); //
        if (ext == ".scss" || ext == ".sass")
            return nfIcon(0xe74b); //
        if (ext == ".less")
            return nfIcon(0xe758); //
        if (ext == ".vue")
            return nfIcon(0xe6a0); //
        if (ext == ".svelte")
            return nfIcon(0xe697);
        if (ext == ".jsx")
            return nfIcon(0xe7ba); //
        if (ext == ".tsx")
            return nfIcon(0xe7ba);

        // Data / config
        if (ext == ".json")
            return nfIcon(0xe60b); //
        if (ext == ".yaml" || ext == ".yml")
            return nfIcon(0xf481);
        if (ext == ".toml")
            return nfIcon(0xe60b);
        if (ext == ".xml")
            return nfIcon(0xf481);
        if (ext == ".csv")
            return nfIcon(0xf1c3);
        if (ext == ".sql")
            return nfIcon(0xe706); //
        if (ext == ".env")
            return nfIcon(0xf462);
        if (ext == ".ini" || ext == ".cfg" || ext == ".conf")
            return nfIcon(0xe615);

        // Documents
        if (ext == ".md" || ext == ".markdown")
            return nfIcon(0xe73e); //
        if (ext == ".txt")
            return nfIcon(0xf15c);
        if (ext == ".pdf")
            return nfIcon(0xf1c1);
        if (ext == ".doc" || ext == ".docx")
            return nfIcon(0xf1c2);

        // Images
        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
            ext == ".gif" || ext == ".bmp" || ext == ".svg" ||
            ext == ".ico" || ext == ".webp")
            return nfIcon(0xf1c5);

        // Build / project
        if (ext == ".cmake")
            return nfIcon(0xe779);
        if (ext == ".lock")
            return nfIcon(0xf023); //

        // Archives
        if (ext == ".zip" || ext == ".tar" || ext == ".gz" ||
            ext == ".bz2" || ext == ".xz" || ext == ".7z" ||
            ext == ".rar")
            return nfIcon(0xf410); //

        // Executable
        if (ext == ".exe" || ext == ".out" || ext == ".bin")
            return nfIcon(0xf489);

        return nfIcon(0xf15b); //  default file
    }

    // Returns a color tint for file icons based on extension
    inline Color fileIconColor(const std::string &name, bool isDir)
    {
        if (isDir)
            return {86, 156, 214}; // blue for folders

        auto dotPos = name.rfind('.');
        if (dotPos == std::string::npos)
            return {180, 180, 180};

        std::string ext = name.substr(dotPos);
        for (auto &c : ext)
            c = (char)std::tolower((unsigned char)c);

        if (ext == ".xel" || ext == ".xell")
            return {255, 198, 109}; // warm gold
        if (ext == ".cpp" || ext == ".cc" || ext == ".cxx" ||
            ext == ".hpp" || ext == ".h" || ext == ".hxx" ||
            ext == ".c")
            return {0, 136, 204}; // blue
        if (ext == ".py")
            return {55, 170, 220}; // python blue
        if (ext == ".js")
            return {241, 224, 90}; // yellow
        if (ext == ".ts")
            return {49, 120, 198}; // typescript blue
        if (ext == ".rs")
            return {222, 165, 132}; // rust orange
        if (ext == ".go")
            return {0, 173, 216}; // go cyan
        if (ext == ".html" || ext == ".htm")
            return {227, 76, 38}; // orange
        if (ext == ".css")
            return {86, 61, 124}; // purple
        if (ext == ".json")
            return {241, 224, 90}; // yellow
        if (ext == ".md" || ext == ".markdown")
            return {65, 131, 196}; // blue
        if (ext == ".sh" || ext == ".bash" || ext == ".zsh")
            return {78, 201, 176}; // teal
        if (ext == ".java")
            return {176, 114, 25}; // java brown
        if (ext == ".rb")
            return {204, 52, 45}; // ruby red
        if (ext == ".lua")
            return {0, 0, 128}; // lua navy
        if (ext == ".svg" || ext == ".png" || ext == ".jpg")
            return {160, 110, 200}; // purple
        if (ext == ".yaml" || ext == ".yml" || ext == ".toml")
            return {160, 160, 160};
        if (ext == ".vue")
            return {65, 184, 131}; // vue green

        return {180, 180, 180}; // default gray
    }

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

            // Determine if and where to show the new file/folder prompt inline
            bool showingNewPrompt = (editMode_ == EditMode::NEW_FILE || editMode_ == EditMode::NEW_FOLDER);
            // The prompt should appear as the FIRST child of the target directory
            int promptAfterIdx = -1; // flat list index AFTER which the prompt appears
            int promptDepth = 0;
            if (showingNewPrompt && selectedIdx_ >= 0 && selectedIdx_ < (int)flatList_.size())
            {
                auto &sel = flatList_[selectedIdx_];
                if (sel.isDir)
                {
                    // Insert as first child of this directory (right after the dir entry)
                    promptAfterIdx = selectedIdx_;
                    promptDepth = sel.depth + 1;
                }
                else
                {
                    // Selected a file — find its parent directory by walking backwards
                    // to find the nearest item with depth == sel.depth - 1 that isDir
                    int parentIdx = -1;
                    for (int i = selectedIdx_ - 1; i >= 0; i--)
                    {
                        if (flatList_[i].isDir && flatList_[i].depth == sel.depth - 1)
                        {
                            parentIdx = i;
                            break;
                        }
                    }
                    if (parentIdx >= 0)
                    {
                        // Insert right after the parent directory (first child position)
                        promptAfterIdx = parentIdx;
                        promptDepth = sel.depth;
                    }
                    else
                    {
                        // Root level file — insert at the very top
                        promptAfterIdx = -1;
                        promptDepth = 0;
                    }
                }
            }
            else if (showingNewPrompt)
            {
                // No selection — show at the very top of tree
                promptAfterIdx = -1; // before first item
                promptDepth = 0;
            }

            // Render tree entries with potential inline prompt
            int visibleRow = startRow;
            int flatIdx = scrollTop_;
            bool promptRendered = false;

            // Special case: prompt before first item
            if (showingNewPrompt && promptAfterIdx < scrollTop_)
            {
                if (visibleRow < rect_.h)
                {
                    renderNewPromptRow(grid, visibleRow, promptDepth);
                    visibleRow++;
                    promptRendered = true;
                }
            }

            while (visibleRow < rect_.h && flatIdx < (int)flatList_.size())
            {
                auto &node = flatList_[flatIdx];
                bool isSelected = (flatIdx == selectedIdx_);
                bool isHovered = (flatIdx == hoverIdx_ && !isSelected);
                Color bg = isSelected ? selectedBg_ : (isHovered ? hoverBg_ : bgColor_);
                Color fg = node.isDir ? dirFg_ : fileFg_;

                // Fill background
                for (auto &c : grid[visibleRow])
                {
                    c.bg = bg;
                    c.dirty = true;
                }

                // Indent
                int indent = node.depth * 2 + 1;

                // Check if this row has inline editing
                if (isSelected && editMode_ == EditMode::RENAME)
                {
                    // Show icon + edit field
                    std::string icon = node.isDir ? (node.expanded ? "▾ " : "▸ ") : "  ";
                    writeString(grid[visibleRow], indent, icon, fg, bg, false);
                    int editCol = indent + 2; // after icon

                    // Edit field background
                    Color editBg = {45, 45, 45};
                    Color editFg = {255, 255, 255};
                    for (int c = editCol; c < rect_.w; c++)
                    {
                        grid[visibleRow][c].bg = editBg;
                        grid[visibleRow][c].dirty = true;
                    }
                    writeString(grid[visibleRow], editCol, editText_, editFg, editBg, false);

                    // Cursor
                    int cursorCol = editCol + editCursor_;
                    if (cursorCol < rect_.w)
                    {
                        grid[visibleRow][cursorCol].underline = true;
                        grid[visibleRow][cursorCol].fg = {255, 255, 255};
                        grid[visibleRow][cursorCol].dirty = true;
                    }
                }
                else
                {
                    // Normal item rendering: icon based on file extension with Nerd Font
                    if (node.isDir)
                    {
                        std::string arrow = node.expanded ? "▾ " : "▸ ";
                        std::string folderIcon = fileIconForName(node.name, true);
                        Color iconColor = fileIconColor(node.name, true);
                        int pos = writeString(grid[visibleRow], indent, arrow, fg, bg, false);
                        pos = writeString(grid[visibleRow], pos, folderIcon + " ", iconColor, bg, false);
                        writeString(grid[visibleRow], pos, node.name, fg, bg, isSelected);
                    }
                    else
                    {
                        std::string fileIcon = fileIconForName(node.name, false);
                        Color iconColor = fileIconColor(node.name, false);
                        int pos = writeString(grid[visibleRow], indent, fileIcon + " ", iconColor, bg, false);
                        writeString(grid[visibleRow], pos, node.name, fg, bg, isSelected);
                    }
                }

                visibleRow++;

                // Insert the new file/folder prompt right after the selected item
                if (showingNewPrompt && !promptRendered && flatIdx == promptAfterIdx && visibleRow < rect_.h)
                {
                    renderNewPromptRow(grid, visibleRow, promptDepth);
                    visibleRow++;
                    promptRendered = true;
                }

                flatIdx++;
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

            // If editing (new file/folder prompt) and clicking elsewhere, cancel it
            if (isEditing())
            {
                cancelEdit();
            }

            if (idx >= 0 && idx < (int)flatList_.size())
            {
                selectedIdx_ = idx;
                activateSelected();
                return true;
            }
            return false;
        }

        // Select without activating (for right-click context menus)
        bool selectAt(int row)
        {
            int idx = scrollTop_ + row - 1;
            if (idx >= 0 && idx < (int)flatList_.size())
            {
                selectedIdx_ = idx;
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

        // Selected file/folder path for context menu actions
        std::string selectedFilePath() const
        {
            if (selectedIdx_ >= 0 && selectedIdx_ < (int)flatList_.size())
                return flatList_[selectedIdx_].fullPath;
            return "";
        }
        bool selectedIsDir() const
        {
            if (selectedIdx_ >= 0 && selectedIdx_ < (int)flatList_.size())
                return flatList_[selectedIdx_].isDir;
            return false;
        }

        // ── Hover support ────────────────────────────────────────────

        void setHoverRow(int localRow) { hoverIdx_ = scrollTop_ + localRow - 1; }

        // ── Inline rename / new file ────────────────────────────────

        bool isEditing() const { return editMode_ != EditMode::NONE; }

        void startRename()
        {
            if (selectedIdx_ < 0 || selectedIdx_ >= (int)flatList_.size())
                return;
            editMode_ = EditMode::RENAME;
            editText_ = flatList_[selectedIdx_].name;
            editCursor_ = (int)editText_.size();
        }

        void startNewFile()
        {
            editMode_ = EditMode::NEW_FILE;
            editText_ = "";
            editCursor_ = 0;
        }

        void startNewFolder()
        {
            editMode_ = EditMode::NEW_FOLDER;
            editText_ = "";
            editCursor_ = 0;
        }

        void cancelEdit()
        {
            editMode_ = EditMode::NONE;
            editText_.clear();
        }

        // Returns the path of the created/renamed file (empty on cancel)
        std::string commitEdit()
        {
            if (editMode_ == EditMode::NONE || editText_.empty())
            {
                cancelEdit();
                return "";
            }

            std::string result;
            namespace fss = std::filesystem;
            try
            {
                if (editMode_ == EditMode::RENAME)
                {
                    std::string oldPath = selectedFilePath();
                    if (!oldPath.empty())
                    {
                        std::string parent = fss::path(oldPath).parent_path().string();
                        std::string newPath = parent + "/" + editText_;
                        if (!fss::exists(newPath))
                        {
                            fss::rename(oldPath, newPath);
                            result = newPath;
                        }
                    }
                }
                else if (editMode_ == EditMode::NEW_FILE)
                {
                    std::string dir = selectedIsDir() ? selectedFilePath()
                                                      : fss::path(selectedFilePath()).parent_path().string();
                    if (dir.empty())
                        dir = rootPath_;
                    std::string newPath = dir + "/" + editText_;
                    if (!fss::exists(newPath))
                    {
                        std::ofstream(newPath).close();
                        result = newPath;
                    }
                }
                else if (editMode_ == EditMode::NEW_FOLDER)
                {
                    std::string dir = selectedIsDir() ? selectedFilePath()
                                                      : fss::path(selectedFilePath()).parent_path().string();
                    if (dir.empty())
                        dir = rootPath_;
                    std::string newPath = dir + "/" + editText_;
                    if (!fss::exists(newPath))
                    {
                        fss::create_directories(newPath);
                        result = newPath;
                    }
                }
            }
            catch (...)
            {
            }

            editMode_ = EditMode::NONE;
            editText_.clear();
            refresh();
            return result;
        }

        // Handle text input for inline editing
        bool handleEditKey(const SDL_Event &event)
        {
            if (editMode_ == EditMode::NONE)
                return false;

            auto key = event.key.keysym.sym;

            if (key == SDLK_RETURN)
            {
                // commit is handled externally by LayoutManager
                return true;
            }
            else if (key == SDLK_ESCAPE)
            {
                cancelEdit();
                return true;
            }
            else if (key == SDLK_BACKSPACE)
            {
                if (editCursor_ > 0 && !editText_.empty())
                {
                    editText_.erase(editCursor_ - 1, 1);
                    editCursor_--;
                }
                return true;
            }
            else if (key == SDLK_DELETE)
            {
                if (editCursor_ < (int)editText_.size())
                    editText_.erase(editCursor_, 1);
                return true;
            }
            else if (key == SDLK_LEFT)
            {
                if (editCursor_ > 0)
                    editCursor_--;
                return true;
            }
            else if (key == SDLK_RIGHT)
            {
                if (editCursor_ < (int)editText_.size())
                    editCursor_++;
                return true;
            }
            else if (key == SDLK_HOME)
            {
                editCursor_ = 0;
                return true;
            }
            else if (key == SDLK_END)
            {
                editCursor_ = (int)editText_.size();
                return true;
            }
            return false;
        }

        bool handleEditTextInput(const std::string &text)
        {
            if (editMode_ == EditMode::NONE)
                return false;
            editText_.insert(editCursor_, text);
            editCursor_ += (int)text.size();
            return true;
        }

        enum class EditMode
        {
            NONE,
            RENAME,
            NEW_FILE,
            NEW_FOLDER
        };
        EditMode editMode() const { return editMode_; }
        const std::string &editText() const { return editText_; }
        int editCursor() const { return editCursor_; }

    private:
        const ThemeData &theme_;

        // Render the new file/folder name prompt at the given row
        void renderNewPromptRow(std::vector<std::vector<Cell>> &grid, int row, int depth) const
        {
            if (row < 0 || row >= (int)grid.size())
                return;
            Color editBg = {45, 45, 45};
            Color editFg = {255, 255, 255};
            for (int c = 0; c < rect_.w && c < (int)grid[row].size(); c++)
            {
                grid[row][c].bg = editBg;
                grid[row][c].ch = U' ';
                grid[row][c].dirty = true;
            }
            int indent = depth * 2 + 1;
            std::string prompt = (editMode_ == EditMode::NEW_FILE) ? "📄 " : "📁 ";
            writeString(grid[row], indent, prompt + editText_, editFg, editBg, false);

            int cursorCol = indent + utf8Len(prompt) + editCursor_;
            if (cursorCol < rect_.w && cursorCol < (int)grid[row].size())
            {
                grid[row][cursorCol].underline = true;
                grid[row][cursorCol].fg = {255, 255, 255};
                grid[row][cursorCol].dirty = true;
            }
        }
        OpenFileCallback onOpenFile_;

        FileNode root_;
        std::string rootPath_;
        std::vector<FileNode> flatList_; // flattened visible entries
        int selectedIdx_ = 0;
        int scrollTop_ = 0;
        int hoverIdx_ = -1;

        // Inline editing state
        EditMode editMode_ = EditMode::NONE;
        std::string editText_;
        int editCursor_ = 0;

        // Theme colors
        Color bgColor_ = {24, 24, 24};
        Color titleBg_ = {30, 30, 30};
        Color titleFg_ = {187, 187, 187};
        Color selectedBg_ = {38, 79, 120};
        Color hoverBg_ = {30, 40, 55};
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
