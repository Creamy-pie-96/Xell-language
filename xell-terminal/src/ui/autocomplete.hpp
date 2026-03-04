#pragma once

// =============================================================================
// autocomplete.hpp — Autocompletion popup for the Xell Terminal IDE
// =============================================================================
// Floating overlay that provides:
//   - Keyword completions (54 keywords from language_data.json)
//   - Builtin completions (372 builtins across 23 categories)
//   - User-defined symbol completions (from current buffer scan)
//   - Snippet completions
//   - Fuzzy matching with scoring
// =============================================================================

#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <sstream>
#include <cctype>
#include "../terminal/types.hpp"
#include "../theme/theme_loader.hpp"

namespace xterm
{

    // ─── Completion item ─────────────────────────────────────────────────

    enum class CompletionKind
    {
        Keyword,
        Builtin,
        Function,
        Variable,
        Class,
        Module,
        Snippet,
        Constant,
    };

    struct CompletionItem
    {
        std::string label;       // main text
        std::string detail;      // short description
        std::string signature;   // function signature
        std::string category;    // builtin category
        CompletionKind kind = CompletionKind::Keyword;
        int score = 0;           // fuzzy match score
    };

    // ─── Completion database ─────────────────────────────────────────────

    class CompletionDB
    {
    public:
        void loadFromJSON(const std::string &path)
        {
            std::ifstream file(path);
            if (!file.is_open())
            {
                loadDefaults();
                return;
            }

            std::string content((std::istreambuf_iterator<char>(file)),
                                 std::istreambuf_iterator<char>());

            parseLanguageData(content);
        }

        void loadDefaults()
        {
            // Core keywords
            const char *keywords[] = {
                "fn", "let", "be", "if", "elif", "else", "for", "in", "while",
                "ret", "give", "bring", "as", "from", "class", "extends",
                "new", "self", "super", "static", "abstract", "interface",
                "implements", "mixin", "with", "struct", "module", "use",
                "try", "catch", "finally", "throw", "is", "not", "and", "or",
                "true", "false", "none", "skip", "stop", "del", "typeof",
                "switch", "case", "default", "enum", "property", "get", "set",
                "decorator", "print", "input", "assert"};

            for (auto kw : keywords)
            {
                CompletionItem item;
                item.label = kw;
                item.kind = CompletionKind::Keyword;
                item.detail = "keyword";
                allItems_.push_back(item);
            }

            // Common builtins
            struct BuiltinDef
            {
                const char *name;
                const char *cat;
                const char *sig;
            };
            BuiltinDef builtins[] = {
                {"len", "collection", "len(collection)"},
                {"push", "list", "push(list, item)"},
                {"pop", "list", "pop(list)"},
                {"append", "list", "append(list, item)"},
                {"sort", "list", "sort(list)"},
                {"reverse", "list", "reverse(list)"},
                {"join", "string", "join(list, sep)"},
                {"split", "string", "split(str, sep)"},
                {"contains", "collection", "contains(collection, item)"},
                {"keys", "map", "keys(map)"},
                {"values", "map", "values(map)"},
                {"range", "generator", "range(start, end, step?)"},
                {"to_int", "casting", "to_int(value)"},
                {"to_float", "casting", "to_float(value)"},
                {"to_str", "casting", "to_str(value)"},
                {"typeof", "type", "typeof(value)"},
                {"abs", "math", "abs(number)"},
                {"round", "math", "round(number, decimals?)"},
                {"min", "math", "min(a, b)"},
                {"max", "math", "max(a, b)"},
                {"read", "fs", "read(path)"},
                {"write", "fs", "write(path, content)"},
                {"exists", "fs", "exists(path)"},
                {"mkdir", "fs", "mkdir(path)"},
                {"sleep", "util", "sleep(seconds)"},
                {"exec", "shell", "exec(command)"},
            };

            for (auto &b : builtins)
            {
                CompletionItem item;
                item.label = b.name;
                item.kind = CompletionKind::Builtin;
                item.category = b.cat;
                item.signature = b.sig;
                item.detail = std::string(b.cat) + " builtin";
                allItems_.push_back(item);
            }
        }

        void addUserSymbol(const std::string &name, CompletionKind kind,
                           const std::string &detail = "")
        {
            // Don't add duplicates
            for (auto &item : userItems_)
                if (item.label == name)
                    return;

            CompletionItem item;
            item.label = name;
            item.kind = kind;
            item.detail = detail;
            userItems_.push_back(item);
        }

        void clearUserSymbols() { userItems_.clear(); }

        // Scan a buffer for user-defined names (simple regex-free scan)
        void scanBuffer(const std::vector<std::string> &lines)
        {
            clearUserSymbols();
            std::unordered_set<std::string> seen;

            for (auto &line : lines)
            {
                // Look for: fn name, let name, class name, be name
                auto addMatch = [&](const std::string &prefix, CompletionKind kind)
                {
                    size_t pos = 0;
                    while ((pos = line.find(prefix, pos)) != std::string::npos)
                    {
                        size_t start = pos + prefix.size();
                        // Skip whitespace
                        while (start < line.size() && line[start] == ' ')
                            start++;
                        // Read identifier
                        size_t end = start;
                        while (end < line.size() && (isalnum(line[end]) || line[end] == '_'))
                            end++;
                        if (end > start)
                        {
                            std::string name = line.substr(start, end - start);
                            if (seen.find(name) == seen.end())
                            {
                                seen.insert(name);
                                addUserSymbol(name, kind);
                            }
                        }
                        pos = end;
                    }
                };

                addMatch("fn ", CompletionKind::Function);
                addMatch("let ", CompletionKind::Variable);
                addMatch("be ", CompletionKind::Variable);
                addMatch("class ", CompletionKind::Class);
                addMatch("struct ", CompletionKind::Class);
                addMatch("module ", CompletionKind::Module);
            }
        }

        // Fuzzy match: find completions matching prefix
        std::vector<CompletionItem> match(const std::string &prefix, int maxResults = 20) const
        {
            if (prefix.empty())
                return {};

            std::vector<CompletionItem> results;
            std::string lowerPrefix = toLower(prefix);

            auto score = [&](const CompletionItem &item) -> int
            {
                std::string lowerLabel = toLower(item.label);

                // Exact prefix match — highest score
                if (lowerLabel.substr(0, lowerPrefix.size()) == lowerPrefix)
                    return 1000 - (int)item.label.size(); // shorter = better

                // Fuzzy: all chars appear in order
                int pi = 0;
                int bonus = 0;
                for (int li = 0; li < (int)lowerLabel.size() && pi < (int)lowerPrefix.size(); li++)
                {
                    if (lowerLabel[li] == lowerPrefix[pi])
                    {
                        if (li == 0 || lowerLabel[li - 1] == '_')
                            bonus += 10; // word boundary match
                        pi++;
                    }
                }
                if (pi == (int)lowerPrefix.size())
                    return 500 + bonus - (int)item.label.size();

                return -1; // no match
            };

            // Score all items
            for (auto &item : allItems_)
            {
                int s = score(item);
                if (s >= 0)
                {
                    CompletionItem copy = item;
                    copy.score = s;
                    results.push_back(copy);
                }
            }
            for (auto &item : userItems_)
            {
                int s = score(item);
                if (s >= 0)
                {
                    CompletionItem copy = item;
                    copy.score = s + 100; // user symbols get priority boost
                    results.push_back(copy);
                }
            }

            // Sort by score (highest first)
            std::sort(results.begin(), results.end(),
                      [](const CompletionItem &a, const CompletionItem &b)
                      { return a.score > b.score; });

            if ((int)results.size() > maxResults)
                results.resize(maxResults);

            return results;
        }

    private:
        std::vector<CompletionItem> allItems_;
        std::vector<CompletionItem> userItems_;

        static std::string toLower(const std::string &s)
        {
            std::string result = s;
            for (auto &c : result)
                c = std::tolower(c);
            return result;
        }

        // Minimal JSON parser for language_data.json
        void parseLanguageData(const std::string &json)
        {
            // Parse keywords
            size_t kwPos = json.find("\"keywords\"");
            if (kwPos != std::string::npos)
            {
                size_t arrStart = json.find('[', kwPos);
                if (arrStart != std::string::npos)
                    parseKeywordArray(json, arrStart);
            }

            // Parse builtins
            size_t biPos = json.find("\"builtins\"");
            if (biPos != std::string::npos)
            {
                size_t arrStart = json.find('[', biPos);
                if (arrStart != std::string::npos)
                    parseBuiltinArray(json, arrStart);
            }
        }

        void parseKeywordArray(const std::string &json, size_t pos)
        {
            // Find each {"name":"..."} object
            while (pos < json.size())
            {
                size_t namePos = json.find("\"name\"", pos);
                if (namePos == std::string::npos)
                    break;

                // Check if we've left the keywords array
                size_t arrEnd = json.find(']', pos);
                if (arrEnd != std::string::npos && namePos > arrEnd)
                    break;

                std::string name = extractString(json, namePos);
                if (name.empty())
                {
                    pos = namePos + 6;
                    continue;
                }

                CompletionItem item;
                item.label = name;
                item.kind = CompletionKind::Keyword;

                // Try to extract detail
                size_t detailPos = json.find("\"detail\"", namePos);
                if (detailPos != std::string::npos && detailPos < namePos + 500)
                    item.detail = extractString(json, detailPos);

                // Try to extract hover.sig
                size_t sigPos = json.find("\"sig\"", namePos);
                if (sigPos != std::string::npos && sigPos < namePos + 500)
                    item.signature = extractString(json, sigPos);

                allItems_.push_back(item);
                pos = namePos + name.size() + 8;
            }
        }

        void parseBuiltinArray(const std::string &json, size_t pos)
        {
            while (pos < json.size())
            {
                size_t namePos = json.find("\"name\"", pos);
                if (namePos == std::string::npos)
                    break;

                std::string name = extractString(json, namePos);
                if (name.empty())
                {
                    pos = namePos + 6;
                    continue;
                }

                CompletionItem item;
                item.label = name;
                item.kind = CompletionKind::Builtin;

                // Category
                size_t catPos = json.find("\"category\"", namePos);
                if (catPos != std::string::npos && catPos < namePos + 300)
                    item.category = extractString(json, catPos);

                item.detail = item.category.empty() ? "builtin" : item.category + " builtin";

                // Try hover.sig
                size_t sigPos = json.find("\"sig\"", namePos);
                if (sigPos != std::string::npos && sigPos < namePos + 500)
                    item.signature = extractString(json, sigPos);

                allItems_.push_back(item);
                pos = namePos + name.size() + 8;
            }
        }

        std::string extractString(const std::string &json, size_t keyPos) const
        {
            // Find the colon after the key
            size_t colon = json.find(':', keyPos);
            if (colon == std::string::npos)
                return "";

            // Find opening quote
            size_t start = json.find('"', colon + 1);
            if (start == std::string::npos)
                return "";
            start++;

            // Find closing quote (handle escapes)
            std::string result;
            for (size_t i = start; i < json.size(); i++)
            {
                if (json[i] == '\\' && i + 1 < json.size())
                {
                    result += json[i + 1];
                    i++;
                }
                else if (json[i] == '"')
                {
                    break;
                }
                else
                {
                    result += json[i];
                }
            }
            return result;
        }
    };

    // ─── Autocomplete popup (visual component) ──────────────────────────

    class AutocompletePopup
    {
    public:
        AutocompletePopup(const ThemeData &theme) : theme_(theme)
        {
            loadColors();
        }

        // Show the popup at a screen position with these completions
        void show(int screenRow, int screenCol, const std::string &prefix,
                  CompletionDB &db)
        {
            prefix_ = prefix;
            items_ = db.match(prefix);
            if (items_.empty())
            {
                visible_ = false;
                return;
            }
            visible_ = true;
            selectedIdx_ = 0;
            anchorRow_ = screenRow;
            anchorCol_ = screenCol;
        }

        void hide()
        {
            visible_ = false;
            items_.clear();
        }

        bool isVisible() const { return visible_; }

        // Update filter (as user types more)
        void updateFilter(const std::string &prefix, CompletionDB &db)
        {
            if (!visible_)
                return;
            prefix_ = prefix;
            items_ = db.match(prefix);
            if (items_.empty())
                hide();
            else
                selectedIdx_ = std::min(selectedIdx_, (int)items_.size() - 1);
        }

        // Navigation
        void moveUp()
        {
            if (selectedIdx_ > 0)
                selectedIdx_--;
        }

        void moveDown()
        {
            if (selectedIdx_ < (int)items_.size() - 1)
                selectedIdx_++;
        }

        // Accept the selected item
        const CompletionItem *accept() const
        {
            if (!visible_ || items_.empty())
                return nullptr;
            return &items_[selectedIdx_];
        }

        // Render the popup as an overlay (cells to blit on top of screen)
        struct PopupRender
        {
            std::vector<std::vector<Cell>> cells;
            int x, y, w, h;
        };

        PopupRender render() const
        {
            PopupRender out;
            if (!visible_ || items_.empty())
                return out;

            int maxVisible = std::min((int)items_.size(), maxVisibleItems_);
            int labelWidth = 0;
            int detailWidth = 0;

            for (auto &item : items_)
            {
                labelWidth = std::max(labelWidth, (int)item.label.size());
                detailWidth = std::max(detailWidth, (int)item.detail.size());
            }

            int iconW = 2;
            int padding = 2;
            int totalWidth = iconW + labelWidth + padding + detailWidth + padding;
            totalWidth = std::min(totalWidth, 60);
            totalWidth = std::max(totalWidth, 20);

            out.w = totalWidth;
            out.h = maxVisible;
            out.x = anchorCol_;
            out.y = anchorRow_ + 1; // below cursor

            out.cells.resize(maxVisible);
            for (int i = 0; i < maxVisible; i++)
            {
                out.cells[i].resize(totalWidth);
                bool selected = (i == selectedIdx_);
                Color bg = selected ? selectedBg_ : popupBg_;
                Color fg = selected ? selectedFg_ : popupFg_;

                for (auto &c : out.cells[i])
                {
                    c.ch = U' ';
                    c.bg = bg;
                    c.fg = fg;
                    c.dirty = true;
                }

                auto &item = items_[i];

                // Kind icon
                char icon = kindIcon(item.kind);
                out.cells[i][0].ch = (char32_t)icon;
                out.cells[i][0].fg = kindColor(item.kind);
                out.cells[i][0].bold = true;

                // Label
                for (int j = 0; j < (int)item.label.size() && j + iconW < totalWidth; j++)
                {
                    out.cells[i][iconW + j].ch = (char32_t)item.label[j];
                    out.cells[i][iconW + j].fg = fg;
                    out.cells[i][iconW + j].bold = selected;
                }

                // Detail (right-aligned, dimmer)
                int detailStart = totalWidth - (int)item.detail.size() - 1;
                if (detailStart > iconW + labelWidth + 1)
                {
                    Color dimFg = {128, 128, 128};
                    for (int j = 0; j < (int)item.detail.size() && detailStart + j < totalWidth; j++)
                    {
                        out.cells[i][detailStart + j].ch = (char32_t)item.detail[j];
                        out.cells[i][detailStart + j].fg = dimFg;
                    }
                }
            }

            return out;
        }

    private:
        const ThemeData &theme_;
        std::vector<CompletionItem> items_;
        std::string prefix_;
        int selectedIdx_ = 0;
        int anchorRow_ = 0;
        int anchorCol_ = 0;
        bool visible_ = false;
        int maxVisibleItems_ = 12;

        Color popupBg_ = {38, 38, 38};
        Color popupFg_ = {204, 204, 204};
        Color selectedBg_ = {4, 57, 94};
        Color selectedFg_ = {255, 255, 255};

        void loadColors()
        {
            popupBg_ = getUIColor(theme_, "popup_bg", popupBg_);
            popupFg_ = getUIColor(theme_, "popup_fg", popupFg_);
            selectedBg_ = getUIColor(theme_, "popup_selected_bg", selectedBg_);
            selectedFg_ = getUIColor(theme_, "popup_selected_fg", selectedFg_);
        }

        static char kindIcon(CompletionKind kind)
        {
            switch (kind)
            {
            case CompletionKind::Keyword:
                return 'K';
            case CompletionKind::Builtin:
                return 'B';
            case CompletionKind::Function:
                return 'F';
            case CompletionKind::Variable:
                return 'V';
            case CompletionKind::Class:
                return 'C';
            case CompletionKind::Module:
                return 'M';
            case CompletionKind::Snippet:
                return 'S';
            case CompletionKind::Constant:
                return '#';
            }
            return '?';
        }

        Color kindColor(CompletionKind kind) const
        {
            switch (kind)
            {
            case CompletionKind::Keyword:
                return {198, 120, 221}; // purple
            case CompletionKind::Builtin:
                return {86, 182, 194};  // cyan
            case CompletionKind::Function:
                return {97, 175, 239};  // blue
            case CompletionKind::Variable:
                return {224, 108, 117}; // red
            case CompletionKind::Class:
                return {229, 192, 123}; // yellow
            case CompletionKind::Module:
                return {152, 195, 121}; // green
            case CompletionKind::Snippet:
                return {209, 154, 102}; // orange
            case CompletionKind::Constant:
                return {209, 154, 102}; // orange
            }
            return popupFg_;
        }
    };

} // namespace xterm
