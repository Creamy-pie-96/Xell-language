#pragma once

// =============================================================================
// visual_effects.hpp — Visual Effects & Polish for the Xell Terminal IDE
// =============================================================================
// Phase 6: Cursor styles, smooth scroll, minimap, bracket matching,
//           indent guides, code folding, animations.
//
// These are composable "overlays" that EditorView can integrate into
// its render pipeline. Each component is independent and optional.
// =============================================================================

#include <string>
#include <vector>
#include <stack>
#include <unordered_map>
#include <cmath>
#include <algorithm>
#include "../terminal/types.hpp"
#include "../editor/text_buffer.hpp"
#include "../theme/theme_loader.hpp"

namespace xterm
{

    // =====================================================================
    // 6.1 — Cursor Styles
    // =====================================================================

    enum class CursorShape
    {
        BLOCK,     // █ — default
        BAR,       // | — insert mode
        UNDERLINE, // _ — replace mode
    };

    struct CursorConfig
    {
        CursorShape shape = CursorShape::BLOCK;
        bool blink = true;
        int blinkRateMs = 530; // milliseconds per blink cycle
        bool visible = true;   // toggles during blink

        // Different cursors for different modes
        CursorShape normalShape = CursorShape::BLOCK;
        CursorShape insertShape = CursorShape::BAR;
    };

    class CursorRenderer
    {
    public:
        CursorRenderer() = default;

        void setConfig(const CursorConfig &config) { config_ = config; }
        const CursorConfig &config() const { return config_; }

        // Call each frame — handles blink timing
        void tick(int deltaMs)
        {
            if (!config_.blink)
            {
                config_.visible = true;
                return;
            }
            blinkAccum_ += deltaMs;
            if (blinkAccum_ >= config_.blinkRateMs)
            {
                config_.visible = !config_.visible;
                blinkAccum_ -= config_.blinkRateMs;
            }
        }

        // Reset blink (e.g., after cursor movement)
        void resetBlink()
        {
            config_.visible = true;
            blinkAccum_ = 0;
        }

        // Render cursor into a cell
        void renderCursor(Cell &cell, Color cursorFg, Color cursorBg) const
        {
            if (!config_.visible)
                return;

            switch (config_.shape)
            {
            case CursorShape::BLOCK:
                // Invert colors
                cell.bg = cursorFg;
                cell.fg = cursorBg;
                cell.dirty = true;
                break;

            case CursorShape::BAR:
                // Draw thin bar (use left-half block)
                cell.ch = U'▏';
                cell.fg = cursorFg;
                cell.dirty = true;
                break;

            case CursorShape::UNDERLINE:
                // Draw underline
                cell.underline = true;
                cell.fg = cursorFg;
                cell.dirty = true;
                break;
            }
        }

    private:
        CursorConfig config_;
        int blinkAccum_ = 0;
    };

    // =====================================================================
    // 6.2 — Smooth Scrolling
    // =====================================================================

    class SmoothScroll
    {
    public:
        SmoothScroll() = default;

        void setEnabled(bool enabled) { enabled_ = enabled; }
        bool enabled() const { return enabled_; }

        // Target scroll position (in lines)
        void scrollTo(int targetLine)
        {
            targetY_ = targetLine;
            if (!enabled_)
                currentY_ = (float)targetLine;
        }

        void scrollBy(int delta)
        {
            targetY_ += delta;
            if (targetY_ < 0)
                targetY_ = 0;
            if (!enabled_)
                currentY_ = (float)targetY_;
        }

        // Update animation state
        void tick(int deltaMs)
        {
            if (!enabled_ || std::abs(currentY_ - (float)targetY_) < 0.01f)
            {
                currentY_ = (float)targetY_;
                return;
            }

            // Exponential ease-out
            float speed = smoothSpeed_ * (float)deltaMs / 16.0f;
            currentY_ += ((float)targetY_ - currentY_) * speed;

            // Snap if very close
            if (std::abs(currentY_ - (float)targetY_) < 0.1f)
                currentY_ = (float)targetY_;
        }

        // Get the current fractional scroll position
        float currentScroll() const { return currentY_; }
        int currentScrollInt() const { return (int)std::round(currentY_); }
        int targetScroll() const { return targetY_; }

        // Is currently animating?
        bool isAnimating() const
        {
            return enabled_ && std::abs(currentY_ - (float)targetY_) > 0.01f;
        }

        void setSmoothSpeed(float speed) { smoothSpeed_ = speed; }

    private:
        bool enabled_ = true;
        int targetY_ = 0;
        float currentY_ = 0.0f;
        float smoothSpeed_ = 0.3f; // 0.0 = no smoothing, 1.0 = instant
    };

    // =====================================================================
    // 6.3 — Code Minimap
    // =====================================================================

    class Minimap
    {
    public:
        Minimap() = default;

        void setEnabled(bool enabled) { enabled_ = enabled; }
        bool enabled() const { return enabled_; }
        void setWidth(int w) { width_ = w; }
        int width() const { return width_; }

        // Render minimap for the given buffer into a column of cells
        std::vector<std::vector<Cell>> render(const TextBuffer &buffer,
                                              int viewportStart,
                                              int viewportHeight,
                                              int minimapHeight,
                                              Color bgColor,
                                              Color fgColor,
                                              Color viewportColor) const
        {
            std::vector<std::vector<Cell>> cells(minimapHeight, std::vector<Cell>(width_));

            int totalLines = buffer.lineCount();
            if (totalLines == 0)
                return cells;

            // Fill background
            for (int r = 0; r < minimapHeight; r++)
                for (int c = 0; c < width_; c++)
                {
                    cells[r][c].ch = U' ';
                    cells[r][c].bg = bgColor;
                    cells[r][c].fg = fgColor;
                    cells[r][c].dirty = true;
                }

            // Scale: how many buffer lines per minimap row
            float scale = (float)totalLines / (float)minimapHeight;
            if (scale < 1.0f)
                scale = 1.0f;

            for (int r = 0; r < minimapHeight; r++)
            {
                int bufLine = (int)(r * scale);
                if (bufLine >= totalLines)
                    break;

                const std::string &line = buffer.getLine(bufLine);

                // Is this line in the viewport?
                bool inViewport = (bufLine >= viewportStart && bufLine < viewportStart + viewportHeight);

                Color rowBg = inViewport ? viewportColor : bgColor;
                for (int c = 0; c < width_; c++)
                    cells[r][c].bg = rowBg;

                // Render a condensed version of the line
                for (int c = 0; c < width_ && c < (int)line.size(); c++)
                {
                    char ch = line[c];
                    if (ch == ' ' || ch == '\t')
                        continue;

                    // Use dot characters for condensed view
                    cells[r][c].ch = U'·';
                    cells[r][c].fg = {120, 120, 120};
                    if (inViewport)
                        cells[r][c].fg = {160, 160, 160};
                    cells[r][c].dirty = true;
                }
            }

            return cells;
        }

    private:
        bool enabled_ = true;
        int width_ = 10; // columns
    };

    // =====================================================================
    // 6.4 — Bracket Matching (`:` / `;` pairs for Xell)
    // =====================================================================

    struct BracketPair
    {
        int openLine = -1;
        int openCol = -1;
        int closeLine = -1;
        int closeCol = -1;
        int depth = 0;
    };

    class BracketMatcher
    {
    public:
        BracketMatcher() = default;

        void setEnabled(bool enabled) { enabled_ = enabled; }
        bool enabled() const { return enabled_; }

        void setRainbowEnabled(bool enabled) { rainbow_ = enabled; }
        bool rainbowEnabled() const { return rainbow_; }

        // Find the matching bracket for the cursor position
        BracketPair findMatch(const TextBuffer &buffer, int cursorRow, int cursorCol) const
        {
            if (!enabled_)
                return {};

            int totalLines = buffer.lineCount();
            if (cursorRow >= totalLines)
                return {};

            const std::string &line = buffer.getLine(cursorRow);
            if (cursorCol >= (int)line.size())
                return {};

            char ch = line[cursorCol];
            BracketPair result;

            if (ch == ':')
            {
                // Search forward for matching `;`
                result.openLine = cursorRow;
                result.openCol = cursorCol;
                int depth = 1;
                for (int r = cursorRow; r < totalLines && depth > 0; r++)
                {
                    const std::string &l = buffer.getLine(r);
                    int startC = (r == cursorRow) ? cursorCol + 1 : 0;
                    for (int c = startC; c < (int)l.size() && depth > 0; c++)
                    {
                        if (l[c] == ':' && !isInString(l, c))
                            depth++;
                        else if (l[c] == ';' && !isInString(l, c))
                        {
                            depth--;
                            if (depth == 0)
                            {
                                result.closeLine = r;
                                result.closeCol = c;
                            }
                        }
                    }
                }
                result.depth = countDepth(buffer, cursorRow, cursorCol);
            }
            else if (ch == ';')
            {
                // Search backward for matching `:`
                result.closeLine = cursorRow;
                result.closeCol = cursorCol;
                int depth = 1;
                for (int r = cursorRow; r >= 0 && depth > 0; r--)
                {
                    const std::string &l = buffer.getLine(r);
                    int startC = (r == cursorRow) ? cursorCol - 1 : (int)l.size() - 1;
                    for (int c = startC; c >= 0 && depth > 0; c--)
                    {
                        if (l[c] == ';' && !isInString(l, c))
                            depth++;
                        else if (l[c] == ':' && !isInString(l, c))
                        {
                            depth--;
                            if (depth == 0)
                            {
                                result.openLine = r;
                                result.openCol = c;
                            }
                        }
                    }
                }
                result.depth = countDepth(buffer, result.openLine, result.openCol);
            }
            else
            {
                // Also match standard brackets: (), [], {}
                static const std::string opens = "([{";
                static const std::string closes = ")]}";

                size_t openIdx = opens.find(ch);
                size_t closeIdx = closes.find(ch);

                if (openIdx != std::string::npos)
                {
                    char open = opens[openIdx];
                    char close = closes[openIdx];
                    result.openLine = cursorRow;
                    result.openCol = cursorCol;
                    int depth = 1;
                    for (int r = cursorRow; r < totalLines && depth > 0; r++)
                    {
                        const std::string &l = buffer.getLine(r);
                        int startC = (r == cursorRow) ? cursorCol + 1 : 0;
                        for (int c = startC; c < (int)l.size() && depth > 0; c++)
                        {
                            if (l[c] == open && !isInString(l, c))
                                depth++;
                            else if (l[c] == close && !isInString(l, c))
                            {
                                depth--;
                                if (depth == 0)
                                {
                                    result.closeLine = r;
                                    result.closeCol = c;
                                }
                            }
                        }
                    }
                }
                else if (closeIdx != std::string::npos)
                {
                    char open = opens[closeIdx];
                    char close = closes[closeIdx];
                    result.closeLine = cursorRow;
                    result.closeCol = cursorCol;
                    int depth = 1;
                    for (int r = cursorRow; r >= 0 && depth > 0; r--)
                    {
                        const std::string &l = buffer.getLine(r);
                        int startC = (r == cursorRow) ? cursorCol - 1 : (int)l.size() - 1;
                        for (int c = startC; c >= 0 && depth > 0; c--)
                        {
                            if (l[c] == close && !isInString(l, c))
                                depth++;
                            else if (l[c] == open && !isInString(l, c))
                            {
                                depth--;
                                if (depth == 0)
                                {
                                    result.openLine = r;
                                    result.openCol = c;
                                }
                            }
                        }
                    }
                }
            }

            return result;
        }

        // Get rainbow color for a given nesting depth
        Color rainbowColor(int depth) const
        {
            static const std::vector<Color> colors = {
                {255, 215, 0},   // gold
                {187, 134, 252}, // purple
                {3, 218, 198},   // teal
                {255, 167, 38},  // orange
                {130, 177, 255}, // blue
                {244, 143, 177}, // pink
            };
            return colors[depth % colors.size()];
        }

    private:
        bool enabled_ = true;
        bool rainbow_ = true;

        // Simple check: is position inside a string literal?
        bool isInString(const std::string &line, int col) const
        {
            bool inStr = false;
            for (int i = 0; i < col; i++)
            {
                if (line[i] == '"' && (i == 0 || line[i - 1] != '\\'))
                    inStr = !inStr;
            }
            return inStr;
        }

        // Count the nesting depth at a `:` position
        int countDepth(const TextBuffer &buffer, int row, int col) const
        {
            int depth = 0;
            for (int r = 0; r <= row; r++)
            {
                const std::string &l = buffer.getLine(r);
                int maxC = (r == row) ? col : (int)l.size();
                for (int c = 0; c < maxC; c++)
                {
                    if (l[c] == ':' && !isInString(l, c))
                        depth++;
                    else if (l[c] == ';' && !isInString(l, c))
                        depth--;
                }
            }
            return depth;
        }
    };

    // =====================================================================
    // 6.5 — Indent Guides
    // =====================================================================

    class IndentGuides
    {
    public:
        IndentGuides() = default;

        void setEnabled(bool enabled) { enabled_ = enabled; }
        bool enabled() const { return enabled_; }

        void setTabSize(int tabSize) { tabSize_ = tabSize; }

        // Get indent guide columns for a visible line
        struct GuideInfo
        {
            std::vector<int> guideColumns; // column positions where guides appear
            int activeGuide = -1;          // the guide at cursor indent level
        };

        GuideInfo getGuides(const std::string &line, int cursorIndent) const
        {
            if (!enabled_)
                return {};

            GuideInfo info;
            int indent = 0;
            for (char c : line)
            {
                if (c == ' ')
                    indent++;
                else if (c == '\t')
                    indent += tabSize_;
                else
                    break;
            }

            // Place guides at each tab stop
            for (int col = tabSize_; col < indent; col += tabSize_)
                info.guideColumns.push_back(col);

            // The active guide is the one matching cursor indent
            for (int i = 0; i < (int)info.guideColumns.size(); i++)
            {
                if (info.guideColumns[i] == cursorIndent * tabSize_ ||
                    (cursorIndent > 0 && info.guideColumns[i] <= cursorIndent * tabSize_))
                    info.activeGuide = i;
            }

            return info;
        }

        // Render guide character
        void renderGuide(Cell &cell, bool isActive, Color guideColor, Color activeColor) const
        {
            cell.ch = U'│';
            cell.fg = isActive ? activeColor : guideColor;
            cell.dirty = true;
        }

    private:
        bool enabled_ = true;
        int tabSize_ = 4;
    };

    // =====================================================================
    // 6.6 — Code Folding
    // =====================================================================

    struct FoldRegion
    {
        int startLine = 0; // line with `:`
        int endLine = 0;   // line with `;`
        bool collapsed = false;
        int depth = 0;
        std::string preview; // first line of the block (shown when collapsed)
    };

    class CodeFolding
    {
    public:
        CodeFolding() = default;

        void setEnabled(bool enabled) { enabled_ = enabled; }
        bool enabled() const { return enabled_; }

        // Scan buffer for foldable regions
        void scanRegions(const TextBuffer &buffer)
        {
            if (!enabled_)
                return;

            // Preserve collapse state
            std::unordered_map<int, bool> oldState;
            for (auto &r : regions_)
                oldState[r.startLine] = r.collapsed;

            regions_.clear();

            int totalLines = buffer.lineCount();
            std::stack<int> openStack; // stack of start lines

            for (int line = 0; line < totalLines; line++)
            {
                const std::string &text = buffer.getLine(line);
                for (int c = 0; c < (int)text.size(); c++)
                {
                    if (text[c] == ':' && !isInString(text, c))
                    {
                        openStack.push(line);
                    }
                    else if (text[c] == ';' && !isInString(text, c) && !openStack.empty())
                    {
                        int startLine = openStack.top();
                        openStack.pop();

                        if (line > startLine) // only fold multi-line blocks
                        {
                            FoldRegion region;
                            region.startLine = startLine;
                            region.endLine = line;
                            region.depth = (int)openStack.size();
                            region.preview = buffer.getLine(startLine);
                            // Trim preview
                            if (region.preview.size() > 60)
                                region.preview = region.preview.substr(0, 57) + "...";

                            // Restore collapse state
                            auto it = oldState.find(startLine);
                            if (it != oldState.end())
                                region.collapsed = it->second;

                            regions_.push_back(region);
                        }
                    }
                }
            }

            // Sort by start line
            std::sort(regions_.begin(), regions_.end(),
                      [](const FoldRegion &a, const FoldRegion &b)
                      { return a.startLine < b.startLine; });
        }

        // Toggle fold at a line
        bool toggleFold(int line)
        {
            for (auto &r : regions_)
            {
                if (r.startLine == line)
                {
                    r.collapsed = !r.collapsed;
                    return true;
                }
            }
            return false;
        }

        // Fold all
        void foldAll()
        {
            for (auto &r : regions_)
                r.collapsed = true;
        }

        // Unfold all
        void unfoldAll()
        {
            for (auto &r : regions_)
                r.collapsed = false;
        }

        // Is a line hidden (inside a collapsed fold)?
        bool isLineHidden(int line) const
        {
            for (auto &r : regions_)
            {
                if (r.collapsed && line > r.startLine && line <= r.endLine)
                    return true;
            }
            return false;
        }

        // Get the fold indicator for a line in the gutter
        enum class FoldIndicator
        {
            NONE,
            FOLD_START_OPEN,      // ▼ — foldable, expanded
            FOLD_START_COLLAPSED, // ▶ — foldable, collapsed
            FOLD_BODY,            // │ — inside a fold region
            FOLD_END,             // └ — end of fold region
        };

        FoldIndicator getIndicator(int line) const
        {
            for (auto &r : regions_)
            {
                if (r.startLine == line)
                    return r.collapsed ? FoldIndicator::FOLD_START_COLLAPSED : FoldIndicator::FOLD_START_OPEN;
                if (r.endLine == line && !r.collapsed)
                    return FoldIndicator::FOLD_END;
                if (!r.collapsed && line > r.startLine && line < r.endLine)
                    return FoldIndicator::FOLD_BODY;
            }
            return FoldIndicator::NONE;
        }

        // Get the gutter character for a fold indicator
        char32_t indicatorChar(FoldIndicator ind) const
        {
            switch (ind)
            {
            case FoldIndicator::FOLD_START_OPEN:
                return U'▼';
            case FoldIndicator::FOLD_START_COLLAPSED:
                return U'▶';
            case FoldIndicator::FOLD_BODY:
                return U'│';
            case FoldIndicator::FOLD_END:
                return U'└';
            default:
                return U' ';
            }
        }

        // Map visible line → buffer line (accounting for folds)
        int visibleToBuffer(int visibleLine) const
        {
            int bufLine = 0;
            int visible = 0;
            while (visible < visibleLine)
            {
                bufLine++;
                if (!isLineHidden(bufLine))
                    visible++;
            }
            return bufLine;
        }

        // Count visible lines
        int visibleLineCount(int totalBufferLines) const
        {
            int count = 0;
            for (int i = 0; i < totalBufferLines; i++)
                if (!isLineHidden(i))
                    count++;
            return count;
        }

        const std::vector<FoldRegion> &regions() const { return regions_; }

    private:
        bool enabled_ = true;
        std::vector<FoldRegion> regions_;

        bool isInString(const std::string &line, int col) const
        {
            bool inStr = false;
            for (int i = 0; i < col; i++)
            {
                if (line[i] == '"' && (i == 0 || line[i - 1] != '\\'))
                    inStr = !inStr;
            }
            return inStr;
        }
    };

    // =====================================================================
    // 6.7 — Animations (fade, slide)
    // =====================================================================

    class Animation
    {
    public:
        Animation() = default;

        void start(float durationMs)
        {
            duration_ = durationMs;
            elapsed_ = 0;
            active_ = true;
        }

        void tick(int deltaMs)
        {
            if (!active_)
                return;
            elapsed_ += (float)deltaMs;
            if (elapsed_ >= duration_)
            {
                elapsed_ = duration_;
                active_ = false;
            }
        }

        // Returns 0.0 to 1.0
        float progress() const
        {
            if (!active_ && elapsed_ >= duration_)
                return 1.0f;
            if (duration_ <= 0)
                return 1.0f;
            return elapsed_ / duration_;
        }

        // Ease-out cubic
        float easeOut() const
        {
            float t = progress();
            return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
        }

        // Ease-in-out
        float easeInOut() const
        {
            float t = progress();
            return t < 0.5f ? 4 * t * t * t : 1 - std::pow(-2 * t + 2, 3) / 2;
        }

        bool isActive() const { return active_; }
        bool isComplete() const { return !active_ && elapsed_ >= duration_; }
        void reset()
        {
            active_ = false;
            elapsed_ = 0;
        }

    private:
        float duration_ = 200.0f;
        float elapsed_ = 0;
        bool active_ = false;
    };

    // ─── Popup fade animation ────────────────────────────────────────────

    class PopupAnimation
    {
    public:
        PopupAnimation() = default;

        void fadeIn(float durationMs = 120.0f)
        {
            anim_.start(durationMs);
            fadingIn_ = true;
            everStarted_ = true;
        }

        void fadeOut(float durationMs = 80.0f)
        {
            anim_.start(durationMs);
            fadingIn_ = false;
            everStarted_ = true;
        }

        void tick(int deltaMs) { anim_.tick(deltaMs); }

        // Alpha value 0.0 to 1.0
        float alpha() const
        {
            float t = anim_.easeOut();
            return fadingIn_ ? t : (1.0f - t);
        }

        bool isVisible() const
        {
            if (!everStarted_)
                return false;
            return fadingIn_ || anim_.isActive();
        }

    private:
        Animation anim_;
        bool fadingIn_ = false;
        bool everStarted_ = false;
    };

    // ─── Status message slide-in ─────────────────────────────────────────

    class SlideAnimation
    {
    public:
        SlideAnimation() = default;

        void slideIn(int fromCol, int toCol, float durationMs = 200.0f)
        {
            fromCol_ = fromCol;
            toCol_ = toCol;
            anim_.start(durationMs);
        }

        void tick(int deltaMs) { anim_.tick(deltaMs); }

        int currentCol() const
        {
            float t = anim_.easeOut();
            return fromCol_ + (int)((toCol_ - fromCol_) * t);
        }

        bool isActive() const { return anim_.isActive(); }

    private:
        Animation anim_;
        int fromCol_ = 0;
        int toCol_ = 0;
    };

    // =====================================================================
    // VisualEffects — Aggregated container for all visual effects
    // =====================================================================

    struct VisualEffects
    {
        CursorRenderer cursor;
        SmoothScroll smoothScroll;
        Minimap minimap;
        BracketMatcher bracketMatcher;
        IndentGuides indentGuides;
        CodeFolding codeFolding;
        PopupAnimation autocompleteAnim;
        PopupAnimation hoverAnim;
        SlideAnimation statusAnim;

        // Apply all per-frame updates
        void tick(int deltaMs)
        {
            cursor.tick(deltaMs);
            smoothScroll.tick(deltaMs);
            autocompleteAnim.tick(deltaMs);
            hoverAnim.tick(deltaMs);
            statusAnim.tick(deltaMs);
        }

        // Check if any animation needs redraw
        bool needsRedraw() const
        {
            return smoothScroll.isAnimating() ||
                   autocompleteAnim.isVisible() ||
                   statusAnim.isActive();
        }
    };

} // namespace xterm
