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
#include <regex>
#include "../terminal/types.hpp"
#include "../theme/theme_loader.hpp"
#include "snippet_engine.hpp"

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
        std::string label;      // main text
        std::string detail;     // short description
        std::string signature;  // function signature
        std::string category;   // builtin category
        std::string insertText; // text to insert (may differ from label, e.g. snippet placeholders)
        CompletionKind kind = CompletionKind::Keyword;
        int score = 0; // fuzzy match score
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
            loadBuiltinMembers(); // populate type members for -> access
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

        int userSymbolCount() const { return (int)userItems_.size(); }

        // Load snippet definitions as completion items
        void loadSnippets(const SnippetEngine &engine)
        {
            for (auto &sn : engine.snippets())
            {
                CompletionItem item;
                item.label = sn.prefix;
                item.kind = CompletionKind::Snippet;
                item.detail = "snippet";
                item.signature = sn.description;
                snippetItems_.push_back(item);
            }
        }

        // Scan a buffer for user-defined names (regex-based scan)
        void scanBuffer(const std::vector<std::string> &lines)
        {
            clearUserSymbols();
            std::unordered_set<std::string> seen;

            // Regex patterns for Xell constructs
            static const std::regex fnPattern(R"(^\s*fn\s+([a-zA-Z_]\w*))");
            static const std::regex letBePattern(R"(^\s*(?:let|be)\s+([a-zA-Z_]\w*))");
            static const std::regex classPattern(R"(^\s*(?:class|struct)\s+([a-zA-Z_]\w*))");
            static const std::regex modulePattern(R"(^\s*module\s+([a-zA-Z_]\w*))");
            static const std::regex bringPattern(R"(^\s*bring\s+([a-zA-Z_]\w*))");
            static const std::regex forPattern(R"(^\s*for\s+([a-zA-Z_]\w*)\s+in\b)");
            static const std::regex catchPattern(R"(^\s*catch\s+([a-zA-Z_]\w*))");
            // Variable assignment: name = value (but not ==, =>, or inside expressions)
            static const std::regex varPattern(R"(^\s*([a-zA-Z_]\w*)\s*=(?!=)(?!>))");
            // Lambda: name => body
            static const std::regex lambdaPattern(R"(^\s*(?:let|be)?\s*([a-zA-Z_]\w*)\s*=>\s*)");

            auto tryMatch = [&](const std::string &line, const std::regex &pat, CompletionKind kind)
            {
                std::smatch m;
                if (std::regex_search(line, m, pat) && m.size() > 1)
                {
                    std::string name = m[1].str();
                    if (!name.empty() && seen.find(name) == seen.end())
                    {
                        seen.insert(name);
                        addUserSymbol(name, kind);
                    }
                }
            };

            for (auto &line : lines)
            {
                tryMatch(line, fnPattern, CompletionKind::Function);
                tryMatch(line, letBePattern, CompletionKind::Variable);
                tryMatch(line, classPattern, CompletionKind::Class);
                tryMatch(line, modulePattern, CompletionKind::Module);
                tryMatch(line, bringPattern, CompletionKind::Module);
                tryMatch(line, forPattern, CompletionKind::Variable);
                tryMatch(line, catchPattern, CompletionKind::Variable);
                tryMatch(line, varPattern, CompletionKind::Variable);
                tryMatch(line, lambdaPattern, CompletionKind::Function);
            }
        }

        // ── Load symbols from --check-symbols JSON output ─────────────
        // Replaces scanBuffer with AST-derived symbols.
        // JSON format: [{"name":"x","kind":"function","detail":"fn x(a,b)","params":["a","b"]}, ...]
        void loadASTSymbols(const std::string &json)
        {
            clearUserSymbols();
            moduleMembers_.clear(); // clear old scoped members too
            if (json.empty() || json[0] != '[')
            {
                // Re-populate builtin members (they were cleared)
                loadBuiltinMembers();
                return;
            }

            // Lightweight JSON array-of-objects parser (no dependency)
            size_t pos = 1; // skip '['
            while (pos < json.size())
            {
                // Find next '{'
                pos = json.find('{', pos);
                if (pos == std::string::npos)
                    break;

                // Find matching '}'
                size_t end = json.find('}', pos);
                if (end == std::string::npos)
                    break;

                std::string obj = json.substr(pos, end - pos + 1);
                pos = end + 1;

                // Extract fields with simple string search
                std::string name = extractJsonStr(obj, "name");
                std::string kind = extractJsonStr(obj, "kind");
                std::string detail = extractJsonStr(obj, "detail");
                std::string scope = extractJsonStr(obj, "scope");

                if (name.empty())
                    continue;

                // Skip parameters (method params etc.)
                if (kind == "parameter")
                    continue;

                CompletionKind ck = CompletionKind::Variable;
                if (kind == "function" || kind == "method")
                    ck = CompletionKind::Function;
                else if (kind == "class")
                    ck = CompletionKind::Class;
                else if (kind == "module")
                    ck = CompletionKind::Module;
                else if (kind == "import")
                    ck = CompletionKind::Module;
                else if (kind == "constant")
                    ck = CompletionKind::Constant;

                // If this symbol has a parent scope, it's a member — add to moduleMembers_
                if (!scope.empty())
                {
                    CompletionItem item;
                    item.label = name;
                    item.kind = ck;
                    item.detail = detail.empty() ? (scope + " member") : detail;
                    moduleMembers_[scope].push_back(item);
                }
                else
                {
                    // Top-level symbol — add to global user completions
                    addUserSymbol(name, ck, detail);
                }
            }

            // Re-populate builtin type members (string, list, map, etc.)
            loadBuiltinMembers();
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
            for (auto &item : snippetItems_)
            {
                int s = score(item);
                if (s >= 0)
                {
                    CompletionItem copy = item;
                    copy.score = s + 50; // snippets get moderate boost
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

        // Match only members of a specific module (for -> access)
        std::vector<CompletionItem> matchModuleMembers(const std::string &moduleName,
                                                       const std::string &prefix,
                                                       int maxResults = 20) const
        {
            auto it = moduleMembers_.find(moduleName);
            if (it == moduleMembers_.end())
                return {};

            std::vector<CompletionItem> results;
            std::string lowerPrefix = toLower(prefix);

            for (auto &item : it->second)
            {
                std::string lowerLabel = toLower(item.label);
                int s = -1;
                if (prefix.empty())
                    s = 500 - (int)item.label.size(); // show all members
                else if (lowerLabel.substr(0, lowerPrefix.size()) == lowerPrefix)
                    s = 1000 - (int)item.label.size();
                else
                {
                    // Fuzzy match
                    int pi = 0;
                    for (int li = 0; li < (int)lowerLabel.size() && pi < (int)lowerPrefix.size(); li++)
                        if (lowerLabel[li] == lowerPrefix[pi])
                            pi++;
                    if (pi == (int)lowerPrefix.size())
                        s = 500 - (int)item.label.size();
                }
                if (s >= 0)
                {
                    CompletionItem copy = item;
                    copy.score = s + 200; // high priority for member access
                    results.push_back(copy);
                }
            }

            std::sort(results.begin(), results.end(),
                      [](const CompletionItem &a, const CompletionItem &b)
                      { return a.score > b.score; });
            if ((int)results.size() > maxResults)
                results.resize(maxResults);
            return results;
        }

        bool hasModuleMembers(const std::string &moduleName) const
        {
            if (moduleName == "__any__")
                return true; // __any__ always "has members" (combined fallback)
            return moduleMembers_.count(moduleName) > 0;
        }

        // Match members for __any__ (combined string+list+map+collection methods)
        // Also handles looking up a specific type key (__string__, __list__, etc.)
        // For __any__, deduplicates across all builtin type scopes
        std::vector<CompletionItem> matchAnyMembers(const std::string &prefix,
                                                    int maxResults = 25) const
        {
            std::vector<CompletionItem> combined;
            std::unordered_set<std::string> seen;

            // Gather from all builtin type scopes
            static const std::vector<std::string> builtinTypes = {
                "__string__", "__list__", "__map__", "__number__",
                "__bytes__", "__type__"};

            for (auto &typeKey : builtinTypes)
            {
                auto it = moduleMembers_.find(typeKey);
                if (it == moduleMembers_.end())
                    continue;
                for (auto &item : it->second)
                {
                    if (seen.insert(item.label).second)
                        combined.push_back(item);
                }
            }

            // Now fuzzy-match from combined
            std::vector<CompletionItem> results;
            std::string lowerPrefix = toLower(prefix);

            for (auto &item : combined)
            {
                std::string lowerLabel = toLower(item.label);
                int s = -1;
                if (prefix.empty())
                    s = 500 - (int)item.label.size();
                else if (lowerLabel.substr(0, lowerPrefix.size()) == lowerPrefix)
                    s = 1000 - (int)item.label.size();
                else
                {
                    int pi = 0;
                    for (int li = 0; li < (int)lowerLabel.size() && pi < (int)lowerPrefix.size(); li++)
                        if (lowerLabel[li] == lowerPrefix[pi])
                            pi++;
                    if (pi == (int)lowerPrefix.size())
                        s = 500 - (int)item.label.size();
                }
                if (s >= 0)
                {
                    CompletionItem copy = item;
                    copy.score = s + 150; // slightly lower than exact module members
                    results.push_back(copy);
                }
            }

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
        std::vector<CompletionItem> snippetItems_;
        std::unordered_map<std::string, std::vector<CompletionItem>> moduleMembers_;

        static std::string toLower(const std::string &s)
        {
            std::string result = s;
            for (auto &c : result)
                c = std::tolower(c);
            return result;
        }

        // Extract a string value from a JSON object substring: "key":"value"
        static std::string extractJsonStr(const std::string &obj, const std::string &key)
        {
            std::string needle = "\"" + key + "\"";
            size_t pos = obj.find(needle);
            if (pos == std::string::npos)
                return "";
            pos = obj.find(':', pos + needle.size());
            if (pos == std::string::npos)
                return "";
            pos = obj.find('"', pos + 1);
            if (pos == std::string::npos)
                return "";
            size_t end = obj.find('"', pos + 1);
            if (end == std::string::npos)
                return "";
            return obj.substr(pos + 1, end - pos - 1);
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

            // Parse debug decorators
            size_t ddPos = json.find("\"debugDecorators\"");
            if (ddPos != std::string::npos)
            {
                size_t arrStart = json.find('[', ddPos);
                if (arrStart != std::string::npos)
                    parseDecoratorArray(json, arrStart);
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

        void parseDecoratorArray(const std::string &json, size_t pos)
        {
            // Parse debug decorator completions: [{"name":"@debug on","detail":"...","insertText":"..."}, ...]
            while (pos < json.size())
            {
                size_t namePos = json.find("\"name\"", pos);
                if (namePos == std::string::npos)
                    break;

                // Check if we've left the array
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

                // Extract detail
                size_t detailPos = json.find("\"detail\"", namePos);
                if (detailPos != std::string::npos && detailPos < namePos + 500)
                    item.detail = extractString(json, detailPos);

                // Extract insertText for snippet expansion
                size_t insPos = json.find("\"insertText\"", namePos);
                if (insPos != std::string::npos && insPos < namePos + 500)
                    item.insertText = extractString(json, insPos);

                allItems_.push_back(item);
                pos = namePos + name.size() + 8;
            }
        }

        // Populate moduleMembers_ with builtin type members for -> access.
        // Maps language_data.json categories to special scope keys:
        //   "string"     → "__string__"
        //   "list"       → "__list__"  (also gets "collection" entries)
        //   "map"        → "__map__"   (also gets "collection" entries)
        //   "collection" → shared by __list__, __map__
        //   "regex"      → "__string__" (regex methods on strings)
        //   "bytes"      → "__bytes__"
        // This allows -> on string literals, list literals, etc.
        void loadBuiltinMembers()
        {
            // Category → builtin type scope keys
            // Some categories map to multiple types (collection → list, map)
            static const std::unordered_map<std::string, std::vector<std::string>> categoryToTypes = {
                {"string", {"__string__"}},
                {"regex", {"__string__"}},
                {"textproc", {"__string__"}},
                {"list", {"__list__"}},
                {"collection", {"__list__", "__map__"}},
                {"map", {"__map__"}},
                {"math", {"__number__"}},
                {"bytes", {"__bytes__"}},
                {"io", {"__io__"}},
                {"fs", {"__fs__"}},
                {"json", {"__json__"}},
                {"datetime", {"__datetime__"}},
                {"type", {"__type__"}},
                {"hash", {"__hash__"}},
            };

            for (auto &item : allItems_)
            {
                if (item.kind != CompletionKind::Builtin || item.category.empty())
                    continue;

                auto it = categoryToTypes.find(item.category);
                if (it == categoryToTypes.end())
                    continue;

                for (auto &typeKey : it->second)
                {
                    CompletionItem member;
                    member.label = item.label;
                    member.kind = CompletionKind::Function; // builtins are callable
                    member.detail = item.category + " method";
                    member.signature = item.signature;
                    moduleMembers_[typeKey].push_back(member);
                }
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
            scrollOffset_ = 0;
            anchorRow_ = screenRow;
            anchorCol_ = screenCol;
        }

        // Show the popup with pre-built items (for module member access)
        void showItems(int screenRow, int screenCol, const std::string &prefix,
                       const std::vector<CompletionItem> &items)
        {
            prefix_ = prefix;
            items_ = items;
            if (items_.empty())
            {
                visible_ = false;
                return;
            }
            visible_ = true;
            selectedIdx_ = 0;
            scrollOffset_ = 0;
            anchorRow_ = screenRow;
            anchorCol_ = screenCol;
        }

        void hide()
        {
            visible_ = false;
            items_.clear();
            scrollOffset_ = 0;
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

        // Update filter with pre-built items (for module member access)
        void updateFilterItems(const std::string &prefix,
                               const std::vector<CompletionItem> &items)
        {
            if (!visible_)
                return;
            prefix_ = prefix;
            items_ = items;
            if (items_.empty())
                hide();
            else
                selectedIdx_ = std::min(selectedIdx_, (int)items_.size() - 1);
        }

        // Navigation
        void moveUp()
        {
            if (selectedIdx_ > 0)
            {
                selectedIdx_--;
                // Scroll up if selection is above visible window
                if (selectedIdx_ < scrollOffset_)
                    scrollOffset_ = selectedIdx_;
            }
        }

        void moveDown()
        {
            if (selectedIdx_ < (int)items_.size() - 1)
            {
                selectedIdx_++;
                // Scroll down if selection is below visible window
                int maxVisible = std::min((int)items_.size(), maxVisibleItems_);
                if (selectedIdx_ >= scrollOffset_ + maxVisible)
                    scrollOffset_ = selectedIdx_ - maxVisible + 1;
            }
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
                int itemIdx = i + scrollOffset_;
                if (itemIdx >= (int)items_.size())
                    break;

                out.cells[i].resize(totalWidth);
                bool selected = (itemIdx == selectedIdx_);
                Color bg = selected ? selectedBg_ : popupBg_;
                Color fg = selected ? selectedFg_ : popupFg_;

                for (auto &c : out.cells[i])
                {
                    c.ch = U' ';
                    c.bg = bg;
                    c.fg = fg;
                    c.dirty = true;
                }

                auto &item = items_[itemIdx];

                // Kind icon
                char icon = kindIcon(item.kind);
                out.cells[i][0].ch = static_cast<char32_t>(static_cast<unsigned char>(icon));
                out.cells[i][0].fg = kindColor(item.kind);
                out.cells[i][0].bold = true;

                // Label
                {
                    size_t si = 0;
                    int j = 0;
                    while (si < item.label.size() && j + iconW < totalWidth)
                    {
                        out.cells[i][iconW + j].ch = utf8Decode(item.label, si);
                        out.cells[i][iconW + j].fg = fg;
                        out.cells[i][iconW + j].bold = selected;
                        j++;
                    }
                }

                // Detail (right-aligned, dimmer)
                int detailStart = totalWidth - (int)item.detail.size() - 1;
                if (detailStart > iconW + labelWidth + 1)
                {
                    Color dimFg = {128, 128, 128};
                    size_t si = 0;
                    int j = 0;
                    while (si < item.detail.size() && detailStart + j < totalWidth)
                    {
                        out.cells[i][detailStart + j].ch = utf8Decode(item.detail, si);
                        out.cells[i][detailStart + j].fg = dimFg;
                        j++;
                    }
                }
            }

            // Draw scrollbar indicator on right edge if there are more items than visible
            int totalItems = (int)items_.size();
            if (totalItems > maxVisible && totalWidth > 1)
            {
                // Calculate scrollbar thumb position and size
                int thumbSize = std::max(1, maxVisible * maxVisible / totalItems);
                int maxScroll = totalItems - maxVisible; // maximum scrollOffset_
                int trackRange = maxVisible - thumbSize; // range the thumb can travel
                int thumbPos = 0;
                if (maxScroll > 0)
                    thumbPos = scrollOffset_ * trackRange / maxScroll;
                thumbPos = std::clamp(thumbPos, 0, trackRange);

                Color scrollTrack = {60, 60, 60};
                Color scrollThumb = {120, 120, 120};

                int lastCol = totalWidth - 1;
                for (int i = 0; i < maxVisible; i++)
                {
                    bool isThumb = (i >= thumbPos && i < thumbPos + thumbSize);
                    out.cells[i][lastCol].ch = isThumb ? U'█' : U'░';
                    out.cells[i][lastCol].fg = isThumb ? scrollThumb : scrollTrack;
                }
            }

            return out;
        }

    private:
        const ThemeData &theme_;
        std::vector<CompletionItem> items_;
        std::string prefix_;
        int selectedIdx_ = 0;
        int scrollOffset_ = 0; // first visible item index for scrolling
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
                return {86, 182, 194}; // cyan
            case CompletionKind::Function:
                return {97, 175, 239}; // blue
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
