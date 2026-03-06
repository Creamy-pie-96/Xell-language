#pragma once

// =============================================================================
// dashboard_panel.hpp — Code Structure Dashboard (Right Panel)
// =============================================================================
// Deep hierarchical tree of all symbols from --check-symbols JSON.
// Full recursive nesting: modules → classes → functions → params → variables.
// Tree-drawing connectors: │ ├─ └─.  Expand/collapse ▼/►.
// Click item → jump to definition line (or open source file for imports).
// Click variable with lifecycle → popup overlay with BORN/CHANGED/DIED events.
// Ctrl+Shift+D toggles visibility.
// =============================================================================

#include <string>
#include <vector>
#include <functional>
#include <algorithm>
#include "../terminal/types.hpp"
#include "../theme/theme_loader.hpp"
#include "panel.hpp"

namespace xterm
{

    // ─── Dashboard entry (one symbol, recursive tree) ────────────────

    struct DashboardEntry
    {
        std::string icon; // UTF-8 icon: "ƒ", "◆", "◇", "↻", "◊", "•", "α", "←"
        std::string name;
        std::string kind;       // "function", "class", "module", "struct", "variable",
                                // "parameter", "field", "import", "scope", etc.
        std::string detail;     // e.g. "fn greet(name)"
        std::string sourceFile; // originating file path for imports (empty = current file)
        int line = 0;
        int lineEnd = 0;
        Color iconColor;
        bool expanded = true;    // tree node: expanded by default
        bool isVariable = false; // true for variables/fields → lifecycle indicator
        std::vector<DashboardEntry> children;
    };

    // ─── Flattened row for rendering ─────────────────────────────────

    struct FlatRow
    {
        DashboardEntry *entry;
        int depth;                // nesting depth (0 = top-level)
        bool isLast;              // is last child at its level
        std::vector<bool> guides; // per ancestor: true = needs │ continuation line
        bool hasChildren;
    };

    // ─── DashboardPanel ──────────────────────────────────────────────

    class DashboardPanel : public Panel
    {
    public:
        explicit DashboardPanel(const ThemeData &theme)
            : theme_(theme)
        {
            loadColors();
        }

        PanelType type() const override { return PanelType::Variables; }
        std::string title() const override { return "Dashboard"; }

        // ── Load symbols from --check-symbols JSON ──────────────────

        void loadSymbols(const std::string &json, const std::string &originFile = "")
        {
            entries_.clear();
            lifecycleVisible_ = false;
            originFile_ = originFile;
            if (json.empty() || json[0] != '[')
                return;

            // --- Step 1: Parse every raw symbol from JSON ---
            struct RawSymbol
            {
                std::string name, kind, detail, scope, scopeType,
                    inferredType, sourceFile;
                int line = 0, lineEnd = 0;
                std::vector<std::string> children;
                std::vector<std::string> params;
                bool consumed = false; // used during tree building
            };
            std::vector<RawSymbol> raw;

            size_t pos = 1; // skip '['
            while (pos < json.size())
            {
                pos = json.find('{', pos);
                if (pos == std::string::npos)
                    break;
                pos++;

                RawSymbol sym;
                while (pos < json.size() && json[pos] != '}')
                {
                    while (pos < json.size() &&
                           (json[pos] == ' ' || json[pos] == ',' ||
                            json[pos] == '\n' || json[pos] == '\r' || json[pos] == '\t'))
                        pos++;
                    if (pos >= json.size() || json[pos] == '}')
                        break;

                    std::string key = parseStr(json, pos);
                    while (pos < json.size() && json[pos] != ':')
                        pos++;
                    if (pos < json.size())
                        pos++;
                    while (pos < json.size() && json[pos] == ' ')
                        pos++;

                    if (pos < json.size() && json[pos] == '"')
                    {
                        std::string val = parseStr(json, pos);
                        if (key == "name")
                            sym.name = val;
                        else if (key == "kind")
                            sym.kind = val;
                        else if (key == "detail")
                            sym.detail = val;
                        else if (key == "scope")
                            sym.scope = val;
                        else if (key == "scopeType")
                            sym.scopeType = val;
                        else if (key == "inferredType")
                            sym.inferredType = val;
                        else if (key == "sourceFile")
                            sym.sourceFile = val;
                    }
                    else if (pos < json.size() && json[pos] == '[')
                    {
                        pos++;
                        while (pos < json.size() && json[pos] != ']')
                        {
                            while (pos < json.size() && (json[pos] == ' ' || json[pos] == ','))
                                pos++;
                            if (pos < json.size() && json[pos] == '"')
                            {
                                std::string item = parseStr(json, pos);
                                if (key == "children")
                                    sym.children.push_back(item);
                                else if (key == "params")
                                    sym.params.push_back(item);
                            }
                            else
                                break;
                        }
                        if (pos < json.size())
                            pos++; // skip ']'
                    }
                    else if (pos < json.size() && (json[pos] == 't' || json[pos] == 'f'))
                    {
                        if (json.substr(pos, 4) == "true")
                            pos += 4;
                        else if (json.substr(pos, 5) == "false")
                            pos += 5;
                    }
                    else if (pos < json.size() && (isdigit(json[pos]) || json[pos] == '-'))
                    {
                        size_t start = pos;
                        while (pos < json.size() &&
                               (isdigit(json[pos]) || json[pos] == '-' || json[pos] == '.'))
                            pos++;
                        int num = std::atoi(json.substr(start, pos - start).c_str());
                        if (key == "line")
                            sym.line = num;
                        else if (key == "lineEnd")
                            sym.lineEnd = num;
                    }
                }
                if (pos < json.size())
                    pos++;
                if (!sym.name.empty())
                    raw.push_back(sym);
            }

            // --- Step 2: Build recursive tree ---

            // Look up a raw symbol by name + scope (scope = parent's name)
            auto findRaw = [&](const std::string &name,
                               const std::string &scope) -> RawSymbol *
            {
                for (auto &r : raw)
                    if (r.name == name && r.scope == scope && !r.consumed)
                        return &r;
                return nullptr;
            };

            // Recursive builder: RawSymbol → DashboardEntry (+ children)
            std::function<DashboardEntry(RawSymbol &)> buildEntry;
            buildEntry = [&](RawSymbol &r) -> DashboardEntry
            {
                r.consumed = true;

                DashboardEntry e;
                e.name = r.name;
                e.kind = r.kind;
                e.detail = r.detail;
                e.line = r.line;
                e.lineEnd = r.lineEnd;
                e.sourceFile = r.sourceFile;
                e.isVariable = (r.kind == "variable" || r.kind == "field");
                assignIcon(e);

                // (a) Explicit children listed in the "children" JSON array
                for (auto &childName : r.children)
                {
                    RawSymbol *cs = findRaw(childName, r.name);
                    if (cs)
                        e.children.push_back(buildEntry(*cs));
                }

                // (b) Parameters (for functions / methods)
                for (auto &pname : r.params)
                {
                    RawSymbol *ps = findRaw(pname, r.name);
                    if (ps)
                    {
                        e.children.push_back(buildEntry(*ps));
                    }
                    else
                    {
                        // Create synthetic parameter entry
                        DashboardEntry pe;
                        pe.name = pname;
                        pe.kind = "parameter";
                        pe.detail = "param of " + r.name;
                        pe.line = r.line;
                        pe.isVariable = false;
                        assignIcon(pe);
                        e.children.push_back(pe);
                    }
                }

                // (c) Scoped symbols whose scope == this symbol's name but
                //     were NOT already pulled in via children/params arrays
                for (auto &inner : raw)
                {
                    if (inner.consumed || inner.scope != r.name)
                        continue;
                    // Avoid duplicating explicit children / params
                    bool dup = false;
                    for (auto &ch : e.children)
                        if (ch.name == inner.name)
                        {
                            dup = true;
                            break;
                        }
                    if (!dup)
                        e.children.push_back(buildEntry(inner));
                }

                return e;
            };

            // --- Step 3: Collect top-level entries ---
            std::vector<DashboardEntry> globalVars;

            for (auto &r : raw)
            {
                if (r.consumed || !r.scope.empty())
                    continue;

                // Parameters with no scope → skip (shouldn't happen)
                if (r.kind == "parameter")
                    continue;

                // Global variables → separate bucket
                if (r.kind == "variable")
                {
                    r.consumed = true;
                    DashboardEntry ge;
                    ge.name = r.name;
                    ge.kind = r.kind;
                    ge.detail = r.detail;
                    ge.line = r.line;
                    ge.lineEnd = r.lineEnd;
                    ge.isVariable = true;
                    assignIcon(ge);
                    globalVars.push_back(ge);
                    continue;
                }

                // Imports → top-level with module-exported children
                if (r.kind == "import")
                {
                    r.consumed = true;
                    DashboardEntry ie;
                    ie.name = r.name;
                    ie.kind = r.kind;
                    ie.detail = r.detail;
                    ie.line = r.line;
                    ie.lineEnd = r.lineEnd;
                    ie.sourceFile = r.sourceFile;
                    ie.isVariable = false;
                    assignIcon(ie);
                    // Attach children scoped to this import
                    for (auto &inner : raw)
                    {
                        if (inner.consumed || inner.scope != r.name)
                            continue;
                        ie.children.push_back(buildEntry(inner));
                    }
                    entries_.push_back(ie);
                    continue;
                }

                // Everything else: functions, classes, structs, modules, etc.
                entries_.push_back(buildEntry(r));
            }

            // Global scope bucket at the bottom
            if (!globalVars.empty())
            {
                DashboardEntry bucket;
                bucket.name = "[ global scope ]";
                bucket.kind = "scope";
                bucket.line = 1;
                bucket.isVariable = false;
                bucket.icon = "\xE2\x97\x8B"; // ○
                bucket.iconColor = dimColor_;
                bucket.children = std::move(globalVars);
                entries_.push_back(bucket);
            }
        }

        // ── Flatten tree recursively for rendering ──────────────────

        void flattenTree(std::vector<FlatRow> &out,
                         std::vector<DashboardEntry> &nodes,
                         int depth, std::vector<bool> guides) const
        {
            for (int i = 0; i < (int)nodes.size(); i++)
            {
                auto &e = nodes[i];
                bool isLast = (i == (int)nodes.size() - 1);
                bool hasCh = !e.children.empty();

                out.push_back({&e, depth, isLast, guides, hasCh});

                if (hasCh && e.expanded)
                {
                    std::vector<bool> childGuides = guides;
                    childGuides.push_back(!isLast); // true → ancestor needs │
                    flattenTree(out, e.children, depth + 1, childGuides);
                }
            }
        }

        // ── Render ──────────────────────────────────────────────────

        std::vector<std::vector<Cell>> render() const override
        {
            auto cells = makeGrid(bgColor_, fgColor_);
            int w = rect_.w;
            int h = rect_.h;

            // Title bar
            if (h > 0)
            {
                std::string ttl = " DASHBOARD";
                writeString(cells[0], 0, ttl, titleColor_, titleBgColor_, true);
                for (int c = (int)ttl.size(); c < w; c++)
                {
                    cells[0][c].bg = titleBgColor_;
                    cells[0][c].dirty = true;
                }
            }

            if (entries_.empty())
            {
                if (h > 2)
                    writeString(cells[1], 1, "No symbols found.", dimColor_, bgColor_);
                if (h > 3)
                    writeString(cells[2], 1, "Open a .xel file.", dimColor_, bgColor_);
                return cells;
            }

            // If lifecycle overlay is active, render that instead
            if (lifecycleVisible_ && !lifecycleLines_.empty())
            {
                renderLifecycleOverlay(cells, w, h);
                return cells;
            }

            // Flatten tree
            std::vector<FlatRow> flat;
            flattenTree(flat, entries_, 0, {});

            int startIdx = scrollOffset_;
            for (int r = 0; r < (int)flat.size() - startIdx && r + 1 < h; r++)
            {
                const auto &fr = flat[startIdx + r];
                const auto &e = *fr.entry;
                int row = r + 1;

                // Hover highlight
                Color rowBg = (row == hoveredRow_) ? hoverBgColor_ : bgColor_;
                if (row == hoveredRow_)
                    for (int c = 0; c < w; c++)
                    {
                        cells[row][c].bg = rowBg;
                        cells[row][c].dirty = true;
                    }

                int col = 1;

                // Tree guide lines: │ or space for each ancestor depth
                for (int d = 0; d < (int)fr.guides.size(); d++)
                {
                    if (fr.guides[d])
                        col = writeString(cells[row], col, "\xE2\x94\x82 ", treeColor_, rowBg); // │
                    else
                        col = writeString(cells[row], col, "  ", treeColor_, rowBg);
                }

                // Connector: ├─ or └─ (only for children, depth > 0)
                if (fr.depth > 0)
                {
                    if (fr.isLast)
                        col = writeString(cells[row], col, "\xE2\x94\x94\xE2\x94\x80", treeColor_, rowBg); // └─
                    else
                        col = writeString(cells[row], col, "\xE2\x94\x9C\xE2\x94\x80", treeColor_, rowBg); // ├─
                }

                // Expand/collapse arrow for nodes with children
                if (fr.hasChildren)
                {
                    std::string arrow = e.expanded
                                            ? "\xE2\x96\xBC "  // ▼
                                            : "\xE2\x96\xB6 "; // ►
                    col = writeString(cells[row], col, arrow, dimColor_, rowBg);
                }
                else if (fr.depth > 0)
                {
                    col = writeString(cells[row], col, "  ", dimColor_, rowBg); // leaf spacer
                }

                // Icon
                col = writeString(cells[row], col, e.icon + " ", e.iconColor, rowBg);

                // Name (+ inline params for functions)
                Color nameColor = colorForKind(e.kind);
                std::string nameText = e.name;
                if ((e.kind == "function" || e.kind == "method") && !e.detail.empty())
                {
                    size_t paren = e.detail.find('(');
                    if (paren != std::string::npos)
                        nameText += e.detail.substr(paren);
                }
                col = writeString(cells[row], col, nameText, nameColor, rowBg);

                // Right-aligned info: line number / filename for imports
                std::string rightText;
                if (e.kind == "parameter")
                {
                    rightText = "param";
                }
                else if (e.line > 0)
                {
                    rightText = "ln " + std::to_string(e.line);
                    if (e.lineEnd > 0 && e.lineEnd != e.line)
                        rightText += "-" + std::to_string(e.lineEnd);
                }
                else if (!e.sourceFile.empty())
                {
                    std::string fname = e.sourceFile;
                    size_t lastSl = fname.rfind('/');
                    if (lastSl != std::string::npos)
                        fname = fname.substr(lastSl + 1);
                    rightText = fname;
                }

                if (!rightText.empty())
                {
                    int rStart = w - (int)rightText.size() - 1;
                    if (rStart > col + 1)
                        writeString(cells[row], rStart, rightText, dimColor_, rowBg);
                }
            }

            return cells;
        }

        // ── Lifecycle overlay rendering ─────────────────────────────

        void renderLifecycleOverlay(std::vector<std::vector<Cell>> &cells,
                                    int w, int h) const
        {
            Color hdrFg = {128, 128, 128};
            Color bornClr = {78, 201, 176};    // teal
            Color changeClr = {220, 220, 170}; // yellow
            Color diedClr = {244, 71, 71};     // red

            // Row 1: title + close button
            if (h > 1)
            {
                std::string ttl = " Lifecycle: '" + lifecycleName_ + "'";
                writeString(cells[1], 1, ttl, {86, 156, 214}, bgColor_, true);
                std::string close = "[x Close]";
                int cStart = w - (int)close.size() - 1;
                if (cStart > 0)
                    writeString(cells[1], cStart, close, diedClr, bgColor_);
            }

            // Row 2: column headers
            if (h > 2)
            {
                int c = 2;
                c = writeString(cells[2], c, "Line", hdrFg, bgColor_, true);
                c = writeString(cells[2], c, "  ", hdrFg, bgColor_);
                c = writeString(cells[2], c, "Event", hdrFg, bgColor_, true);
                c = writeString(cells[2], c, "       ", hdrFg, bgColor_);
                c = writeString(cells[2], c, "Type", hdrFg, bgColor_, true);
                c = writeString(cells[2], c, "  ", hdrFg, bgColor_);
                c = writeString(cells[2], c, "Value", hdrFg, bgColor_, true);
                c = writeString(cells[2], c, "     ", hdrFg, bgColor_);
                c = writeString(cells[2], c, "By Whom", hdrFg, bgColor_, true);
            }

            // Row 3: separator ───
            if (h > 3)
            {
                for (int cc = 1; cc < w - 1; cc++)
                {
                    cells[3][cc].ch = U'\u2500'; // ─
                    cells[3][cc].fg = {51, 51, 51};
                    cells[3][cc].dirty = true;
                }
            }

            // Data rows (one per lifecycle event)
            int startRow = 4;
            for (int i = 0; i < (int)lifecycleLines_.size() && startRow + i < h; i++)
            {
                const auto &evt = lifecycleLines_[i];
                int row = startRow + i;
                Color fg = (evt.find("BORN") != std::string::npos) ? bornClr : (evt.find("CHANGED") != std::string::npos) ? changeClr
                                                                           : (evt.find("DIED") != std::string::npos)      ? diedClr
                                                                                                                          : fgColor_;
                writeString(cells[row], 2, evt, fg, bgColor_);
            }
        }

        // ── Mouse handling ──────────────────────────────────────────

        bool handleMouseClick(int row, int /*col*/, bool /*shift*/) override
        {
            if (row < 1)
                return false;

            // Lifecycle overlay: click title row to close
            if (lifecycleVisible_)
            {
                if (row <= 1)
                    lifecycleVisible_ = false;
                return true; // consume all clicks in overlay mode
            }

            // Flatten tree
            std::vector<FlatRow> flat;
            flattenTree(flat, entries_, 0, {});

            int idx = scrollOffset_ + (row - 1);
            if (idx < 0 || idx >= (int)flat.size())
                return false;

            auto &item = flat[idx];
            auto *entry = item.entry;

            // Toggle expand / collapse for nodes with children
            if (item.hasChildren)
                entry->expanded = !entry->expanded;

            // Variable with lifecycle → show lifecycle popup
            if (entry->isVariable && lifecycleProvider_)
            {
                auto events = lifecycleProvider_(entry->name);
                if (!events.empty())
                {
                    lifecycleName_ = entry->name;
                    lifecycleLines_ = events;
                    lifecycleVisible_ = true;
                    return true;
                }
            }

            // Jump to line (or open source file for imports)
            // For imported symbols: sourceFile is set → open that file
            // For origin file symbols: sourceFile is empty → use originFile_
            std::string targetFile = !entry->sourceFile.empty()
                                         ? entry->sourceFile
                                         : originFile_;
            if (entry->line > 0)
            {
                if (!targetFile.empty() && onOpenFileAtLine_)
                    onOpenFileAtLine_(targetFile, entry->line);
                else if (onJumpToLine_)
                    onJumpToLine_(entry->line);
            }
            else if (!targetFile.empty() && onOpenFileAtLine_)
            {
                onOpenFileAtLine_(targetFile, 1);
            }
            return true;
        }

        bool handleMouseWheel(int delta) override
        {
            if (lifecycleVisible_)
                return true; // no scrolling in overlay
            scrollOffset_ = std::max(0, scrollOffset_ - delta);
            return true;
        }

        // ── Callbacks ───────────────────────────────────────────────

        void setOnJumpToLine(std::function<void(int)> cb)
        {
            onJumpToLine_ = cb;
        }

        void setOnOpenFileAtLine(std::function<void(const std::string &, int)> cb)
        {
            onOpenFileAtLine_ = cb;
        }

        void setLifecycleProvider(
            std::function<std::vector<std::string>(const std::string &)> fn)
        {
            lifecycleProvider_ = fn;
        }

        void setHoverRow(int row) { hoveredRow_ = row; }

    private:
        const ThemeData &theme_;
        mutable std::vector<DashboardEntry> entries_;
        mutable int scrollOffset_ = 0;
        mutable int hoveredRow_ = -1;
        std::string originFile_; // file path whose symbols are displayed
        std::function<void(int)> onJumpToLine_;
        std::function<void(const std::string &, int)> onOpenFileAtLine_;
        std::function<std::vector<std::string>(const std::string &)> lifecycleProvider_;

        // Lifecycle overlay state
        mutable bool lifecycleVisible_ = false;
        mutable std::string lifecycleName_;
        mutable std::vector<std::string> lifecycleLines_;

        // Colors
        Color bgColor_ = {24, 24, 24};
        Color fgColor_ = {204, 204, 204};
        Color titleColor_ = {255, 255, 255};
        Color titleBgColor_ = {30, 30, 30};
        Color dimColor_ = {100, 100, 100};
        Color treeColor_ = {70, 70, 70};      // tree connector lines
        Color fnColor_ = {220, 220, 170};     // yellow  — functions
        Color classColor_ = {78, 201, 176};   // teal    — classes
        Color moduleColor_ = {156, 220, 254}; // lt blue — modules
        Color varColor_ = {181, 206, 168};    // lt green— variables
        Color structColor_ = {206, 145, 120}; // orange  — structs
        Color loopColor_ = {197, 134, 192};   // purple  — loops/scopes
        Color paramColor_ = {128, 128, 128};  // gray    — parameters
        Color importColor_ = {197, 134, 192}; // purple  — imports
        Color hoverBgColor_ = {40, 40, 40};   // hover highlight

        void loadColors()
        {
            bgColor_ = getUIColor(theme_, "terminal_bg", bgColor_);
            titleBgColor_ = getUIColor(theme_, "tab_bar_bg", titleBgColor_);
        }

        Color colorForKind(const std::string &kind) const
        {
            if (kind == "function" || kind == "method")
                return fnColor_;
            if (kind == "class")
                return classColor_;
            if (kind == "module")
                return moduleColor_;
            if (kind == "struct")
                return structColor_;
            if (kind == "parameter")
                return paramColor_;
            if (kind == "import")
                return importColor_;
            if (kind == "for" || kind == "while" ||
                kind == "loop" || kind == "scope")
                return loopColor_;
            return varColor_;
        }

        void assignIcon(DashboardEntry &e) const
        {
            if (e.kind == "function" || e.kind == "method")
            {
                e.icon = "\xC6\x92"; // ƒ
                e.iconColor = fnColor_;
            }
            else if (e.kind == "class")
            {
                e.icon = "\xE2\x97\x86"; // ◆
                e.iconColor = classColor_;
            }
            else if (e.kind == "module")
            {
                e.icon = "\xE2\x97\x87"; // ◇
                e.iconColor = moduleColor_;
            }
            else if (e.kind == "struct")
            {
                e.icon = "\xE2\x97\x8A"; // ◊
                e.iconColor = structColor_;
            }
            else if (e.kind == "for" || e.kind == "while" || e.kind == "loop")
            {
                e.icon = "\xE2\x86\xBB"; // ↻
                e.iconColor = loopColor_;
            }
            else if (e.kind == "import")
            {
                e.icon = "\xE2\x86\x90"; // ←
                e.iconColor = importColor_;
            }
            else if (e.kind == "parameter")
            {
                e.icon = "\xCE\xB1"; // α
                e.iconColor = paramColor_;
            }
            else if (e.kind == "field" || e.kind == "property")
            {
                e.icon = "\xE2\x80\xA2"; // •
                e.iconColor = varColor_;
            }
            else
            {
                e.icon = "\xE2\x80\xA2"; // •
                e.iconColor = varColor_;
            }
        }

        static std::string parseStr(const std::string &json, size_t &pos)
        {
            if (pos >= json.size() || json[pos] != '"')
                return "";
            pos++;
            std::string result;
            while (pos < json.size() && json[pos] != '"')
            {
                if (json[pos] == '\\' && pos + 1 < json.size())
                {
                    pos++;
                    switch (json[pos])
                    {
                    case '"':
                        result += '"';
                        break;
                    case '\\':
                        result += '\\';
                        break;
                    case 'n':
                        result += '\n';
                        break;
                    case 't':
                        result += '\t';
                        break;
                    default:
                        result += json[pos];
                        break;
                    }
                }
                else
                {
                    result += json[pos];
                }
                pos++;
            }
            if (pos < json.size())
                pos++;
            return result;
        }
    };

} // namespace xterm
