#pragma once

// =============================================================================
// History — command history with persistence
// =============================================================================
// Stores history entries in memory and optionally saves/loads from a file.
// Supports UP/DOWN navigation with a cursor index.
// =============================================================================

#include <string>
#include <vector>
#include <fstream>
#include <cstdlib>

namespace xell
{

    class History
    {
    public:
        explicit History(size_t maxSize = 1000) : maxSize_(maxSize) {}

        /// Add a line to history (skips empty/duplicate of last)
        void add(const std::string &line)
        {
            if (line.empty())
                return;
            if (!entries_.empty() && entries_.back() == line)
                return;
            entries_.push_back(line);
            if (entries_.size() > maxSize_)
                entries_.erase(entries_.begin());
            resetCursor();
        }

        /// Navigate up (older). Returns true if moved, fills `out`.
        bool up(std::string &out)
        {
            if (entries_.empty())
                return false;
            if (cursor_ > 0)
                cursor_--;
            out = entries_[cursor_];
            return true;
        }

        /// Navigate down (newer). Returns true if moved, fills `out`.
        /// If at bottom, returns empty string (current input).
        bool down(std::string &out)
        {
            if (entries_.empty())
                return false;
            if (cursor_ < entries_.size() - 1)
            {
                cursor_++;
                out = entries_[cursor_];
                return true;
            }
            else
            {
                cursor_ = entries_.size();
                out.clear();
                return true;
            }
        }

        /// Reset cursor to past-the-end (ready for new navigation)
        void resetCursor() { cursor_ = entries_.size(); }

        /// Get all entries
        const std::vector<std::string> &entries() const { return entries_; }

        /// Search backwards for prefix match
        bool searchBackward(const std::string &prefix, std::string &out)
        {
            for (int i = (int)entries_.size() - 1; i >= 0; i--)
            {
                if (entries_[i].substr(0, prefix.size()) == prefix && entries_[i] != prefix)
                {
                    out = entries_[i];
                    return true;
                }
            }
            return false;
        }

        /// Load from file
        void load(const std::string &path)
        {
            std::ifstream f(path);
            if (!f.is_open())
                return;
            std::string line;
            while (std::getline(f, line))
            {
                if (!line.empty())
                    entries_.push_back(line);
            }
            if (entries_.size() > maxSize_)
                entries_.erase(entries_.begin(),
                               entries_.begin() + (entries_.size() - maxSize_));
            resetCursor();
        }

        /// Save to file
        void save(const std::string &path) const
        {
            std::ofstream f(path);
            if (!f.is_open())
                return;
            for (auto &e : entries_)
                f << e << '\n';
        }

        /// Get default history file path (~/.xell_history)
        static std::string defaultPath()
        {
            const char *home = std::getenv("HOME");
            if (!home)
                home = std::getenv("USERPROFILE");
            if (!home)
                return ".xell_history";
            return std::string(home) + "/.xell_history";
        }

    private:
        std::vector<std::string> entries_;
        size_t maxSize_;
        size_t cursor_ = 0;
    };

} // namespace xell
