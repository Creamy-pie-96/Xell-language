#pragma once

// =============================================================================
// git_panel.hpp — Git Integration for the Xell Terminal IDE
// =============================================================================
// Phase 5: Git gutter diff, inline diff, file explorer status, commit panel.
//
// Shells out to the git binary for all operations — no libgit2 dependency.
//
// Features:
//   5.1 — Gutter diff markers (green/red/blue bars)
//   5.2 — Inline diff view (Ctrl+D)
//   5.3 — Git status in file explorer (colored dots)
//   5.4 — Commit panel (stage, commit, push)
// =============================================================================

#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include "../terminal/types.hpp"
#include "../theme/theme_loader.hpp"
#include "panel.hpp"

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

namespace xterm
{

    // ─── Diff line status ────────────────────────────────────────────────

    enum class DiffLineStatus
    {
        UNCHANGED,
        ADDED,
        DELETED,
        MODIFIED,
    };

    // ─── File git status ─────────────────────────────────────────────────

    enum class GitFileStatus
    {
        CLEAN,
        MODIFIED,
        ADDED,
        DELETED,
        RENAMED,
        UNTRACKED,
        IGNORED,
        CONFLICT,
    };

    // ─── Git diff hunk ───────────────────────────────────────────────────

    struct DiffHunk
    {
        int oldStart = 0;
        int oldCount = 0;
        int newStart = 0;
        int newCount = 0;
        std::vector<std::pair<DiffLineStatus, std::string>> lines;
    };

    // ─── Git file entry ──────────────────────────────────────────────────

    struct GitFileEntry
    {
        std::string path;
        GitFileStatus status = GitFileStatus::CLEAN;
        bool staged = false;
    };

    // ─── Shell helper ────────────────────────────────────────────────────

    static inline std::string gitCapture(const std::string &cmd, const std::string &workDir = "")
    {
        // Use git -C for directory switching (portable, no shell cd)
        std::string fullCmd = cmd;
        if (!workDir.empty())
        {
            // If the command starts with "git ", insert -C <workDir>
            if (cmd.substr(0, 4) == "git ")
                fullCmd = "git -C \"" + workDir + "\" " + cmd.substr(4);
        }

#ifdef _WIN32
        fullCmd += " 2>NUL";
#else
        fullCmd += " 2>/dev/null";
#endif

        FILE *fp = popen(fullCmd.c_str(), "r");
        if (!fp)
            return "";

        std::string result;
        char buf[4096];
        while (fgets(buf, sizeof(buf), fp))
            result += buf;
        pclose(fp);

        while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
            result.pop_back();
        return result;
    }

    // =====================================================================
    // GitEngine — Core git operations
    // =====================================================================

    class GitEngine
    {
    public:
        void setWorkDir(const std::string &dir)
        {
            workDir_ = dir;
            detectGitRoot();
        }

        bool isGitRepo() const { return !gitRoot_.empty(); }
        const std::string &gitRoot() const { return gitRoot_; }

        // ── Branch info ─────────────────────────────────────────────

        std::string currentBranch()
        {
            return gitCapture("git rev-parse --abbrev-ref HEAD", workDir_);
        }

        std::string shortHash()
        {
            return gitCapture("git rev-parse --short HEAD", workDir_);
        }

        // ── File status ─────────────────────────────────────────────

        std::vector<GitFileEntry> getStatus()
        {
            std::vector<GitFileEntry> files;
            std::string output = gitCapture("git status --porcelain=v1", workDir_);
            if (output.empty())
                return files;

            std::istringstream iss(output);
            std::string line;
            while (std::getline(iss, line))
            {
                if (line.size() < 4)
                    continue;

                GitFileEntry entry;
                char index = line[0];
                char worktree = line[1];
                entry.path = line.substr(3);

                // Determine status
                if (index == '?' || worktree == '?')
                    entry.status = GitFileStatus::UNTRACKED;
                else if (index == 'A' || worktree == 'A')
                    entry.status = GitFileStatus::ADDED;
                else if (index == 'D' || worktree == 'D')
                    entry.status = GitFileStatus::DELETED;
                else if (index == 'R' || worktree == 'R')
                    entry.status = GitFileStatus::RENAMED;
                else if (index == 'M' || worktree == 'M')
                    entry.status = GitFileStatus::MODIFIED;
                else if (index == 'U' || worktree == 'U')
                    entry.status = GitFileStatus::CONFLICT;

                // Staged if index shows the modification
                entry.staged = (index != ' ' && index != '?' && index != '!');

                files.push_back(entry);
            }

            return files;
        }

        // ── Get file status for a specific file ─────────────────────

        GitFileStatus getFileStatus(const std::string &filePath)
        {
            auto status = getStatus();
            for (auto &f : status)
            {
                if (filePath.find(f.path) != std::string::npos ||
                    f.path.find(std::filesystem::path(filePath).filename().string()) != std::string::npos)
                    return f.status;
            }
            return GitFileStatus::CLEAN;
        }

        // ── Gutter diff markers for a file ──────────────────────────

        std::unordered_map<int, DiffLineStatus> getGutterDiff(const std::string &filePath)
        {
            std::unordered_map<int, DiffLineStatus> markers;

            std::string relPath = makeRelative(filePath);
            if (relPath.empty())
                return markers;

            std::string output = gitCapture("git diff -U0 -- \"" + relPath + "\"", workDir_);
            if (output.empty())
                return markers;

            // Parse unified diff
            std::istringstream iss(output);
            std::string line;
            while (std::getline(iss, line))
            {
                if (line.substr(0, 3) != "@@ ")
                    continue;

                // Parse @@ -old,count +new,count @@
                int newStart = 0, newCount = 0;
                auto plusPos = line.find('+', 3);
                if (plusPos == std::string::npos)
                    continue;

                std::string newRange = line.substr(plusPos + 1);
                auto spacePos = newRange.find(' ');
                if (spacePos != std::string::npos)
                    newRange = newRange.substr(0, spacePos);

                auto commaPos = newRange.find(',');
                if (commaPos != std::string::npos)
                {
                    newStart = std::stoi(newRange.substr(0, commaPos));
                    newCount = std::stoi(newRange.substr(commaPos + 1));
                }
                else
                {
                    newStart = std::stoi(newRange);
                    newCount = 1;
                }

                // Parse old range for deletion detection
                auto minusPos = line.find('-', 3);
                int oldCount = 0;
                if (minusPos != std::string::npos && minusPos < plusPos)
                {
                    std::string oldRange = line.substr(minusPos + 1, plusPos - minusPos - 2);
                    auto oc = oldRange.find(',');
                    if (oc != std::string::npos)
                        oldCount = std::stoi(oldRange.substr(oc + 1));
                    else
                        oldCount = 1;
                }

                // Determine marker type
                if (newCount == 0 && oldCount > 0)
                {
                    // Pure deletion — mark the line after as deleted
                    markers[newStart] = DiffLineStatus::DELETED;
                }
                else if (oldCount == 0 && newCount > 0)
                {
                    // Pure addition
                    for (int i = 0; i < newCount; i++)
                        markers[newStart + i] = DiffLineStatus::ADDED;
                }
                else
                {
                    // Modification
                    for (int i = 0; i < newCount; i++)
                        markers[newStart + i] = DiffLineStatus::MODIFIED;
                }
            }

            return markers;
        }

        // ── Full diff for inline view ────────────────────────────────

        std::vector<DiffHunk> getFileDiff(const std::string &filePath)
        {
            std::vector<DiffHunk> hunks;

            std::string relPath = makeRelative(filePath);
            if (relPath.empty())
                return hunks;

            std::string output = gitCapture("git diff -- \"" + relPath + "\"", workDir_);
            if (output.empty())
                return hunks;

            // Parse unified diff into hunks
            std::istringstream iss(output);
            std::string line;
            DiffHunk *currentHunk = nullptr;

            while (std::getline(iss, line))
            {
                if (line.substr(0, 3) == "@@ ")
                {
                    hunks.push_back({});
                    currentHunk = &hunks.back();

                    // Parse hunk header
                    auto plusPos = line.find('+', 3);
                    auto minusPos = line.find('-', 3);
                    if (minusPos != std::string::npos && plusPos != std::string::npos)
                    {
                        std::string oldRange = line.substr(minusPos + 1, plusPos - minusPos - 2);
                        std::string newRange = line.substr(plusPos + 1);
                        auto sp = newRange.find(' ');
                        if (sp != std::string::npos)
                            newRange = newRange.substr(0, sp);

                        auto oc = oldRange.find(',');
                        currentHunk->oldStart = std::stoi(oldRange.substr(0, oc != std::string::npos ? oc : oldRange.size()));
                        currentHunk->oldCount = (oc != std::string::npos) ? std::stoi(oldRange.substr(oc + 1)) : 1;

                        auto nc = newRange.find(',');
                        currentHunk->newStart = std::stoi(newRange.substr(0, nc != std::string::npos ? nc : newRange.size()));
                        currentHunk->newCount = (nc != std::string::npos) ? std::stoi(newRange.substr(nc + 1)) : 1;
                    }
                    continue;
                }

                if (!currentHunk)
                    continue;

                if (line.empty())
                {
                    currentHunk->lines.push_back({DiffLineStatus::UNCHANGED, ""});
                }
                else if (line[0] == '+')
                {
                    currentHunk->lines.push_back({DiffLineStatus::ADDED, line.substr(1)});
                }
                else if (line[0] == '-')
                {
                    currentHunk->lines.push_back({DiffLineStatus::DELETED, line.substr(1)});
                }
                else if (line[0] == ' ')
                {
                    currentHunk->lines.push_back({DiffLineStatus::UNCHANGED, line.substr(1)});
                }
            }

            return hunks;
        }

        // ── Staging operations ───────────────────────────────────────

        bool stageFile(const std::string &filePath)
        {
            std::string relPath = makeRelative(filePath);
            return gitCapture("git add -- \"" + relPath + "\"", workDir_).empty() ||
                   true; // git add produces no output on success
        }

        bool unstageFile(const std::string &filePath)
        {
            std::string relPath = makeRelative(filePath);
            gitCapture("git reset HEAD -- \"" + relPath + "\"", workDir_);
            return true;
        }

        bool stageAll()
        {
            gitCapture("git add -A", workDir_);
            return true;
        }

        // ── Commit ───────────────────────────────────────────────────

        bool commit(const std::string &message)
        {
            if (message.empty())
                return false;
            // Escape message for shell (platform-aware)
            std::string escaped;
#ifdef _WIN32
            // Windows cmd.exe: escape double quotes and caret
            for (char c : message)
            {
                if (c == '"')
                    escaped += "\\\"";
                else if (c == '%')
                    escaped += "%%";
                else
                    escaped += c;
            }
#else
            // Unix: escape shell metacharacters inside double quotes
            for (char c : message)
            {
                if (c == '"' || c == '\\' || c == '$' || c == '`')
                    escaped += '\\';
                escaped += c;
            }
#endif
            std::string result = gitCapture("git commit -m \"" + escaped + "\"", workDir_);
            return !result.empty();
        }

        // ── Push/Pull ────────────────────────────────────────────────

        std::string push()
        {
            return gitCapture("git push", workDir_);
        }

        std::string pull()
        {
            return gitCapture("git pull", workDir_);
        }

        // ── Log ──────────────────────────────────────────────────────

        struct LogEntry
        {
            std::string hash;
            std::string author;
            std::string date;
            std::string message;
        };

        std::vector<LogEntry> getLog(int count = 20)
        {
            std::vector<LogEntry> entries;
            std::string output = gitCapture(
                "git log --oneline --format=\"%h|%an|%ar|%s\" -" + std::to_string(count),
                workDir_);

            std::istringstream iss(output);
            std::string line;
            while (std::getline(iss, line))
            {
                LogEntry entry;
                size_t p1 = line.find('|');
                size_t p2 = line.find('|', p1 + 1);
                size_t p3 = line.find('|', p2 + 1);

                if (p1 != std::string::npos && p2 != std::string::npos && p3 != std::string::npos)
                {
                    entry.hash = line.substr(0, p1);
                    entry.author = line.substr(p1 + 1, p2 - p1 - 1);
                    entry.date = line.substr(p2 + 1, p3 - p2 - 1);
                    entry.message = line.substr(p3 + 1);
                    entries.push_back(entry);
                }
            }

            return entries;
        }

    private:
        std::string workDir_;
        std::string gitRoot_;

        void detectGitRoot()
        {
            gitRoot_ = gitCapture("git rev-parse --show-toplevel", workDir_);
        }

        std::string makeRelative(const std::string &filePath)
        {
            if (gitRoot_.empty())
                return "";
            try
            {
                auto rel = std::filesystem::relative(filePath, gitRoot_);
                return rel.string();
            }
            catch (...)
            {
                return filePath;
            }
        }
    };

    // =====================================================================
    // GitGutter — Diff markers for the editor gutter
    // =====================================================================

    class GitGutter
    {
    public:
        explicit GitGutter(GitEngine &engine) : engine_(engine) {}

        void update(const std::string &filePath)
        {
            markers_ = engine_.getGutterDiff(filePath);
        }

        // Get gutter marker character and color for a line (1-based)
        struct GutterMark
        {
            char32_t ch = U' ';
            Color color = {128, 128, 128};
        };

        GutterMark getMarker(int lineNum) const
        {
            auto it = markers_.find(lineNum);
            if (it == markers_.end())
                return {};

            GutterMark mark;
            switch (it->second)
            {
            case DiffLineStatus::ADDED:
                mark.ch = U'▎';
                mark.color = {80, 200, 80}; // green
                break;
            case DiffLineStatus::DELETED:
                mark.ch = U'▸';
                mark.color = {255, 80, 80}; // red
                break;
            case DiffLineStatus::MODIFIED:
                mark.ch = U'▎';
                mark.color = {80, 150, 255}; // blue
                break;
            default:
                break;
            }
            return mark;
        }

        bool hasChanges() const { return !markers_.empty(); }

    private:
        GitEngine &engine_;
        std::unordered_map<int, DiffLineStatus> markers_;
    };

    // =====================================================================
    // GitPanel — Bottom panel for git operations
    // =====================================================================

    class GitPanel : public Panel
    {
    public:
        explicit GitPanel(const ThemeData &theme)
            : theme_(theme)
        {
            loadPanelColors();
        }

        PanelType type() const override { return PanelType::Git; }
        std::string title() const override { return "Git"; }

        void setEngine(GitEngine *engine) { engine_ = engine; }

        // ── Refresh status ──────────────────────────────────────────

        void refresh()
        {
            if (!engine_ || !engine_->isGitRepo())
                return;

            branch_ = engine_->currentBranch();
            files_ = engine_->getStatus();
            log_ = engine_->getLog(15);
        }

        // ── Commit message editing ──────────────────────────────────

        void handleChar(char ch)
        {
            if (ch == '\n' || ch == '\r')
            {
                if (!commitMsg_.empty() && engine_)
                {
                    engine_->commit(commitMsg_);
                    commitMsg_.clear();
                    msgCursor_ = 0;
                    refresh();
                    statusMessage_ = "Committed!";
                }
            }
            else if (ch == '\b' || ch == 127)
            {
                if (msgCursor_ > 0)
                {
                    commitMsg_.erase(msgCursor_ - 1, 1);
                    msgCursor_--;
                }
            }
            else
            {
                commitMsg_.insert(msgCursor_, 1, ch);
                msgCursor_++;
            }
        }

        // ── Keyboard ────────────────────────────────────────────────

        bool handleKeyDown(const SDL_Event &event) override
        {
            if (event.type != SDL_KEYDOWN)
                return false;

            auto key = event.key.keysym;
            bool ctrl = (key.mod & KMOD_CTRL) != 0;

            switch (key.sym)
            {
            case SDLK_UP:
                if (selectedFile_ > 0)
                    selectedFile_--;
                return true;

            case SDLK_DOWN:
                if (selectedFile_ < (int)files_.size() - 1)
                    selectedFile_++;
                return true;

            case SDLK_SPACE:
                // Toggle stage/unstage
                if (selectedFile_ >= 0 && selectedFile_ < (int)files_.size() && engine_)
                {
                    auto &f = files_[selectedFile_];
                    if (f.staged)
                        engine_->unstageFile(f.path);
                    else
                        engine_->stageFile(f.path);
                    refresh();
                }
                return true;

            case SDLK_a:
                if (ctrl && engine_)
                {
                    engine_->stageAll();
                    refresh();
                }
                return true;

            case SDLK_p:
                if (ctrl && engine_)
                {
                    statusMessage_ = engine_->push();
                }
                return true;

            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                handleChar('\n');
                return true;

            case SDLK_BACKSPACE:
                handleChar('\b');
                return true;

            default:
                break;
            }

            return false;
        }

        // ── Rendering ────────────────────────────────────────────────

        std::vector<std::vector<Cell>> render() const override
        {
            int w = rect_.w;
            int h = rect_.h;
            std::vector<std::vector<Cell>> cells(h, std::vector<Cell>(w));

            for (int r = 0; r < h; r++)
                for (int c = 0; c < w; c++)
                {
                    cells[r][c].ch = U' ';
                    cells[r][c].bg = bgColor_;
                    cells[r][c].fg = fgColor_;
                    cells[r][c].dirty = true;
                }

            if (!engine_ || !engine_->isGitRepo())
            {
                writeString(cells, 0, 0, "Not a git repository", {128, 128, 128}, w);
                return cells;
            }

            // Row 0: Branch info
            std::string branchInfo = " ⎇ " + branch_ + " │ " + std::to_string(files_.size()) + " changes";
            writeString(cells, 0, 0, branchInfo, branchColor_, w);

            // Rows 1..N: Changed files
            int maxFileRows = std::min((int)files_.size(), h - 3);
            for (int i = 0; i < maxFileRows; i++)
            {
                auto &f = files_[i];
                bool selected = (i == selectedFile_);

                Color bg = selected ? selBgColor_ : bgColor_;
                for (int c = 0; c < w; c++)
                    cells[i + 1][c].bg = bg;

                // Status icon
                char32_t icon = U' ';
                Color statusColor = fgColor_;
                switch (f.status)
                {
                case GitFileStatus::MODIFIED:
                    icon = U'M';
                    statusColor = modifiedColor_;
                    break;
                case GitFileStatus::ADDED:
                    icon = U'A';
                    statusColor = addedColor_;
                    break;
                case GitFileStatus::DELETED:
                    icon = U'D';
                    statusColor = deletedColor_;
                    break;
                case GitFileStatus::RENAMED:
                    icon = U'R';
                    statusColor = {86, 156, 214};
                    break;
                case GitFileStatus::UNTRACKED:
                    icon = U'U';
                    statusColor = untrackedColor_;
                    break;
                case GitFileStatus::CONFLICT:
                    icon = U'C';
                    statusColor = {255, 0, 0};
                    break;
                default:
                    break;
                }

                // Staged indicator
                char32_t stagedIcon = f.staged ? U'✓' : U' ';
                cells[i + 1][1].ch = stagedIcon;
                cells[i + 1][1].fg = addedColor_;
                cells[i + 1][1].dirty = true;

                cells[i + 1][3].ch = icon;
                cells[i + 1][3].fg = statusColor;
                cells[i + 1][3].dirty = true;

                // File path
                writeString(cells, i + 1, 5, f.path, statusColor, w);
            }

            // Commit message input (second to last row)
            int commitRow = h - 2;
            if (commitRow > 0 && commitRow < h)
            {
                writeString(cells, commitRow, 0, " Commit: ", {128, 128, 128}, w);
                {
                    size_t si = 0;
                    int col = 9;
                    while (si < commitMsg_.size() && col < w)
                    {
                        cells[commitRow][col].ch = utf8Decode(commitMsg_, si);
                        cells[commitRow][col].fg = fgColor_;
                        cells[commitRow][col].dirty = true;
                        col++;
                    }
                }
            }

            // Status / help (last row)
            int helpRow = h - 1;
            if (helpRow > 0 && helpRow < h)
            {
                std::string help = " Space:stage  Ctrl+A:stage all  Enter:commit  Ctrl+P:push";
                if (!statusMessage_.empty())
                    help = " " + statusMessage_;
                writeString(cells, helpRow, 0, help, {100, 100, 100}, w);
            }

            return cells;
        }

        // ── Status bar info ─────────────────────────────────────────

        std::string branchForStatusBar() const
        {
            if (!engine_ || !engine_->isGitRepo())
                return "";
            return branch_;
        }

        int changedFileCount() const { return (int)files_.size(); }
        const std::vector<GitFileEntry> &changedFiles() const { return files_; }

    private:
        const ThemeData &theme_;
        GitEngine *engine_ = nullptr;

        mutable std::string branch_;
        mutable std::vector<GitFileEntry> files_;
        mutable std::vector<GitEngine::LogEntry> log_;
        mutable int selectedFile_ = 0;

        mutable std::string commitMsg_;
        mutable int msgCursor_ = 0;
        mutable std::string statusMessage_;

        // Colors
        Color bgColor_ = {24, 24, 24};
        Color fgColor_ = {204, 204, 204};
        Color selBgColor_ = {38, 38, 50};
        Color branchColor_ = {86, 156, 214};
        Color addedColor_ = {80, 200, 80};
        Color modifiedColor_ = {229, 192, 123};
        Color deletedColor_ = {244, 71, 71};
        Color untrackedColor_ = {128, 128, 128};

        void loadPanelColors()
        {
            bgColor_ = getUIColor(theme_, "terminal_bg", bgColor_);
            fgColor_ = getUIColor(theme_, "terminal_fg", fgColor_);
        }

        void writeString(std::vector<std::vector<Cell>> &cells, int row, int col,
                         const std::string &text, Color fg, int maxW) const
        {
            size_t si = 0;
            int c = col;
            while (si < text.size() && c < maxW)
            {
                if (row >= 0 && row < (int)cells.size())
                {
                    cells[row][c].ch = utf8Decode(text, si);
                    cells[row][c].fg = fg;
                    cells[row][c].dirty = true;
                }
                else
                {
                    // Still advance the decoder even if row is out of range
                    utf8Decode(text, si);
                }
                c++;
            }
        }
    };

} // namespace xterm
