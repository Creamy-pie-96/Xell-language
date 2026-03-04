#pragma once

// =============================================================================
// highlighter.hpp — Syntax highlighting for the Xell Terminal IDE
// =============================================================================
// Uses the Xell Lexer to tokenize source code, then maps each token to a
// color using the theme loaded from terminal_colors.json.
//
// Full grammar-based highlighting including:
//   - Keywords, operators, literals via Xell lexer token types
//   - 400+ builtin function names (archive, bytes, casting, collection, etc.)
//   - Function-call detection: name( → cyan
//   - Method-call detection: .name( → cyan
//   - Block comments: -->...<--
//   - Line comments: #...
//   - Decorators: @name
//   - String interpolation: {expr} inside strings
// =============================================================================

#include <string>
#include <vector>
#include <unordered_map>
#include "../terminal/types.hpp"
#include "../theme/theme_loader.hpp"

// These are the Xell lexer headers from the main Xell project.
// They must be on the include path (CMake will handle this).
#include "lexer/lexer.hpp"
#include "lexer/token.hpp"

namespace xterm
{

    // ─── Colored span: a range of columns with a specific style ──────────────

    struct ColoredSpan
    {
        int startCol; // 0-based column
        int endCol;   // exclusive
        Color fg;
        Color bg; // usually transparent (use editor_bg)
        bool bold = false;
        bool italic = false;
        bool underline = false;
    };

    // ─── Highlighter: tokenize a line and produce colored spans ──────────────

    class Highlighter
    {
    public:
        explicit Highlighter(const ThemeData &theme)
            : theme_(theme)
        {
            buildTokenTypeToStyleMap();
            buildBuiltinStyleCache();
        }

        // Track block comment state across lines
        // Call before highlighting a buffer — resets multiline state.
        void resetMultilineState() { inBlockComment_ = false; }
        bool inBlockComment() const { return inBlockComment_; }
        void setBlockComment(bool v) { inBlockComment_ = v; }

        // Highlight a single line of source code → vector of ColoredSpan
        // The spans cover the entire line (gaps filled with default fg)
        std::vector<ColoredSpan> highlightLine(const std::string &line)
        {
            std::vector<ColoredSpan> spans;

            if (line.empty())
                return spans;

            // ── Handle block comments: -->...<-- ──────────────────────────
            // If we're inside a block comment from a previous line
            if (inBlockComment_)
            {
                auto endPos = line.find("<--");
                if (endPos != std::string::npos)
                {
                    int endCol = (int)(endPos + 3);
                    spans.push_back({0, endCol, blockCommentFg_, {0, 0, 0, 0}, false, false});
                    inBlockComment_ = false;
                    // Continue highlighting the rest of the line after <--
                    if (endCol < (int)line.size())
                    {
                        std::string rest = line.substr(endCol);
                        auto restSpans = highlightSubLine(rest, endCol);
                        spans.insert(spans.end(), restSpans.begin(), restSpans.end());
                    }
                    return spans;
                }
                else
                {
                    // Entire line is inside block comment
                    spans.push_back({0, (int)line.size(), blockCommentFg_, {0, 0, 0, 0}, false, false});
                    return spans;
                }
            }

            // Check if this line starts a block comment
            {
                auto startPos = line.find("-->");
                if (startPos != std::string::npos)
                {
                    // Highlight before the block comment
                    if (startPos > 0)
                    {
                        std::string before = line.substr(0, startPos);
                        auto beforeSpans = highlightSubLine(before, 0);
                        spans.insert(spans.end(), beforeSpans.begin(), beforeSpans.end());
                    }

                    // Check if block comment ends on this same line
                    auto endPos = line.find("<--", startPos + 3);
                    if (endPos != std::string::npos)
                    {
                        int commentEnd = (int)(endPos + 3);
                        spans.push_back({(int)startPos, commentEnd, blockCommentFg_, {0, 0, 0, 0}, false, false});
                        // Continue after the block comment
                        if (commentEnd < (int)line.size())
                        {
                            std::string rest = line.substr(commentEnd);
                            auto restSpans = highlightSubLine(rest, commentEnd);
                            spans.insert(spans.end(), restSpans.begin(), restSpans.end());
                        }
                    }
                    else
                    {
                        // Block comment continues to next line
                        spans.push_back({(int)startPos, (int)line.size(), blockCommentFg_, {0, 0, 0, 0}, false, false});
                        inBlockComment_ = true;
                    }
                    return spans;
                }
            }

            // Normal line — delegate to sub-line highlighter
            return highlightSubLine(line, 0);
        }

        // Highlight an entire buffer → one vector of spans per line
        std::vector<std::vector<ColoredSpan>> highlightAll(const std::vector<std::string> &lines)
        {
            resetMultilineState();
            std::vector<std::vector<ColoredSpan>> result;
            result.reserve(lines.size());
            for (auto &line : lines)
                result.push_back(highlightLine(line));
            return result;
        }

    private:
        const ThemeData &theme_;
        Color defaultFg_ = Color::default_fg();
        Color blockCommentFg_{152, 195, 121}; // #98c379 green

        bool inBlockComment_ = false;

        // Pre-computed: TokenType int → style
        struct CachedStyle
        {
            Color fg;
            bool bold = false;
            bool italic = false;
        };
        std::unordered_map<int, CachedStyle> typeStyleCache_;

        // Builtin function names → style (all 400+ builtins from the grammar)
        std::unordered_map<std::string, CachedStyle> builtinStyleCache_;

        // Style caches for special patterns
        CachedStyle functionCallStyle_;
        CachedStyle methodCallStyle_;
        CachedStyle decoratorStyle_;
        CachedStyle fnDefNameStyle_;

        // ── Highlight a sub-line (no block-comment handling) ─────────────
        std::vector<ColoredSpan> highlightSubLine(const std::string &line, int colOffset) const
        {
            std::vector<ColoredSpan> spans;

            if (line.empty())
                return spans;

            // Quick path: line comments (#...)
            {
                size_t i = 0;
                while (i < line.size() && (line[i] == ' ' || line[i] == '\t'))
                    i++;
                if (i < line.size() && line[i] == '#')
                {
                    auto it = theme_.scopeColors.find("comment.line.number-sign.xell");
                    Color fg = it != theme_.scopeColors.end() ? it->second.fg : Color{92, 99, 112};
                    bool italic = it != theme_.scopeColors.end() ? it->second.italic : true;
                    spans.push_back({colOffset, colOffset + (int)line.size(), fg, {0, 0, 0, 0}, false, italic});
                    return spans;
                }
            }

            // Tokenize using the real Xell lexer
            try
            {
                xell::Lexer lexer(line);
                auto tokens = lexer.tokenize();

                // Walk through the line, matching each token's value to find its position.
                // The Token struct only has (type, value, line) — no column.
                // We scan forward through the source to locate each token.
                size_t scanPos = 0;

                for (size_t ti = 0; ti < tokens.size(); ++ti)
                {
                    const auto &tok = tokens[ti];
                    if (tok.type == xell::TokenType::EOF_TOKEN)
                        break;
                    if (tok.type == xell::TokenType::NEWLINE)
                        continue;

                    // Determine what text to search for in the line
                    std::string searchText = tok.value;
                    if (tok.type == xell::TokenType::STRING)
                        searchText = "\"" + tok.value + "\"";
                    else if (tok.type == xell::TokenType::RAW_STRING)
                        searchText = "r\"" + tok.value + "\"";
                    else if (tok.type == xell::TokenType::BYTE_STRING)
                        searchText = "b\"" + tok.value + "\"";

                    // Find this token in the line starting from scanPos
                    size_t foundPos = line.find(searchText, scanPos);
                    if (foundPos == std::string::npos)
                    {
                        // Token not found at expected position — skip
                        continue;
                    }

                    int col = (int)foundPos + colOffset;
                    int len = (int)searchText.size();

                    // Fill gap between last span end and this token (whitespace → default)
                    int lastEnd = spans.empty() ? colOffset : spans.back().endCol;
                    if (col > lastEnd)
                    {
                        spans.push_back({lastEnd, col, defaultFg_, {0, 0, 0, 0}, false, false});
                    }

                    // ── Contextual style resolution ──────────────────────

                    CachedStyle style = getStyleForToken(tok);

                    // Check if this is a function call: IDENTIFIER followed by LPAREN
                    if (tok.type == xell::TokenType::IDENTIFIER)
                    {
                        // Check if next non-whitespace token is LPAREN
                        bool nextIsLParen = false;
                        for (size_t j = ti + 1; j < tokens.size(); ++j)
                        {
                            if (tokens[j].type == xell::TokenType::NEWLINE ||
                                tokens[j].type == xell::TokenType::EOF_TOKEN)
                                break;
                            if (tokens[j].type == xell::TokenType::LPAREN)
                            {
                                nextIsLParen = true;
                                break;
                            }
                            break; // any other token means no
                        }

                        // Check if preceded by DOT (method call)
                        bool precededByDot = false;
                        if (ti > 0)
                        {
                            for (int j = (int)ti - 1; j >= 0; --j)
                            {
                                if (tokens[j].type == xell::TokenType::DOT)
                                {
                                    precededByDot = true;
                                    break;
                                }
                                break; // any other token means no
                            }
                        }

                        // Check if preceded by AT (decorator name)
                        bool precededByAt = false;
                        if (ti > 0)
                        {
                            for (int j = (int)ti - 1; j >= 0; --j)
                            {
                                if (tokens[j].type == xell::TokenType::AT)
                                {
                                    precededByAt = true;
                                    break;
                                }
                                break;
                            }
                        }

                        // Check if preceded by FN (function definition name)
                        bool precededByFn = false;
                        if (ti > 0)
                        {
                            for (int j = (int)ti - 1; j >= 0; --j)
                            {
                                if (tokens[j].type == xell::TokenType::FN)
                                {
                                    precededByFn = true;
                                    break;
                                }
                                break;
                            }
                        }

                        if (precededByFn)
                        {
                            // fn definition name → entity.name.function.definition
                            style = fnDefNameStyle_;
                        }
                        else if (precededByAt)
                        {
                            // Decorator name → same as decorator style
                            style = decoratorStyle_;
                        }
                        else if (precededByDot && nextIsLParen)
                        {
                            // Method call: .name( → entity.name.function.method
                            style = methodCallStyle_;
                        }
                        else if (nextIsLParen)
                        {
                            // First check builtin — builtins followed by ( get builtin color
                            auto bit = builtinStyleCache_.find(tok.value);
                            if (bit != builtinStyleCache_.end())
                            {
                                style = bit->second;
                            }
                            else
                            {
                                // Generic function call → entity.name.function.call
                                style = functionCallStyle_;
                            }
                        }
                        else
                        {
                            // Plain identifier — check if it's a known builtin name
                            auto bit = builtinStyleCache_.find(tok.value);
                            if (bit != builtinStyleCache_.end())
                            {
                                style = bit->second;
                            }
                            // otherwise keep default variable style from typeStyleCache_
                        }
                    }

                    // ── Handle string interpolation: color {expr} inside strings ──
                    if (tok.type == xell::TokenType::STRING && tok.value.find('{') != std::string::npos)
                    {
                        // Split the string into parts: regular text and {interpolation}
                        auto interpSpans = highlightStringInterpolation(searchText, col);
                        spans.insert(spans.end(), interpSpans.begin(), interpSpans.end());
                    }
                    else
                    {
                        spans.push_back({col, col + len, style.fg, {0, 0, 0, 0}, style.bold, style.italic});
                    }

                    scanPos = foundPos + len;
                }

                // Fill remainder of line with default
                int lastEnd = spans.empty() ? colOffset : spans.back().endCol;
                if (lastEnd < colOffset + (int)line.size())
                {
                    spans.push_back({lastEnd, colOffset + (int)line.size(), defaultFg_, {0, 0, 0, 0}, false, false});
                }
            }
            catch (...)
            {
                // If tokenization fails, return the whole line as default color
                spans.push_back({colOffset, colOffset + (int)line.size(), defaultFg_, {0, 0, 0, 0}, false, false});
            }

            return spans;
        }

        // ── Highlight string interpolation: "hello {name}" ───────────────
        std::vector<ColoredSpan> highlightStringInterpolation(const std::string &str, int baseCol) const
        {
            std::vector<ColoredSpan> spans;

            // Get string and interpolation colors
            auto strIt = theme_.scopeColors.find("string.quoted.double.xell");
            Color strFg = strIt != theme_.scopeColors.end() ? strIt->second.fg : Color{152, 195, 121};

            auto interpBracketIt = theme_.scopeColors.find("punctuation.section.interpolation.begin.xell");
            Color bracketFg = interpBracketIt != theme_.scopeColors.end() ? interpBracketIt->second.fg : Color{198, 120, 221};
            bool bracketBold = interpBracketIt != theme_.scopeColors.end() ? interpBracketIt->second.bold : true;

            auto interpIt = theme_.scopeColors.find("string.interpolation.xell");
            Color interpFg = interpIt != theme_.scopeColors.end() ? interpIt->second.fg : Color{209, 154, 102};

            int pos = 0;
            int len = (int)str.size();

            while (pos < len)
            {
                // Find next {
                int braceStart = -1;
                for (int i = pos; i < len; ++i)
                {
                    if (str[i] == '{')
                    {
                        braceStart = i;
                        break;
                    }
                }

                if (braceStart < 0)
                {
                    // Rest is plain string
                    spans.push_back({baseCol + pos, baseCol + len, strFg, {0, 0, 0, 0}, false, false});
                    break;
                }

                // Plain string before the brace
                if (braceStart > pos)
                {
                    spans.push_back({baseCol + pos, baseCol + braceStart, strFg, {0, 0, 0, 0}, false, false});
                }

                // Find matching }
                int braceEnd = -1;
                int depth = 0;
                for (int i = braceStart; i < len; ++i)
                {
                    if (str[i] == '{')
                        depth++;
                    else if (str[i] == '}')
                    {
                        depth--;
                        if (depth == 0)
                        {
                            braceEnd = i;
                            break;
                        }
                    }
                }

                if (braceEnd < 0)
                {
                    // No matching } — rest is plain string
                    spans.push_back({baseCol + braceStart, baseCol + len, strFg, {0, 0, 0, 0}, false, false});
                    break;
                }

                // { bracket
                spans.push_back({baseCol + braceStart, baseCol + braceStart + 1, bracketFg, {0, 0, 0, 0}, bracketBold, false});
                // interpolation content
                if (braceEnd > braceStart + 1)
                {
                    spans.push_back({baseCol + braceStart + 1, baseCol + braceEnd, interpFg, {0, 0, 0, 0}, false, false});
                }
                // } bracket
                spans.push_back({baseCol + braceEnd, baseCol + braceEnd + 1, bracketFg, {0, 0, 0, 0}, bracketBold, false});

                pos = braceEnd + 1;
            }

            return spans;
        }

        // ── Build token type → style map ─────────────────────────────────
        void buildTokenTypeToStyleMap()
        {
            // Get default fg from theme
            auto it = theme_.ui.find("editor_fg");
            if (it != theme_.ui.end())
                defaultFg_ = it->second;

            // Get block comment color from theme
            auto bcIt = theme_.scopeColors.find("comment.block.arrow.xell");
            if (bcIt != theme_.scopeColors.end())
                blockCommentFg_ = bcIt->second.fg;

            // Build the TokenType → style cache using the token_type_map from theme
            for (auto &[enumName, scope] : theme_.tokenTypeMap)
            {
                auto tokenStyle = getTokenStyle(theme_, scope);
                int typeInt = enumNameToInt(enumName);
                if (typeInt >= 0)
                {
                    typeStyleCache_[typeInt] = {tokenStyle.fg, tokenStyle.bold, tokenStyle.italic};
                }
            }

            // Pre-compute function call, method call, decorator styles
            {
                auto s = getTokenStyle(theme_, "entity.name.function.call.xell");
                functionCallStyle_ = {s.fg, s.bold, s.italic};
            }
            {
                auto s = getTokenStyle(theme_, "entity.name.function.method.xell");
                methodCallStyle_ = {s.fg, s.bold, s.italic};
            }
            {
                auto s = getTokenStyle(theme_, "entity.name.function.definition.xell");
                fnDefNameStyle_ = {s.fg, s.bold, s.italic};
            }
            {
                // Decorators use the AT token style or a specific decorator scope
                auto s = getTokenStyle(theme_, "keyword.other.special.xell");
                decoratorStyle_ = {s.fg, s.bold, s.italic};
            }
        }

        // ── Build builtin function name cache ────────────────────────────
        // Uses the builtin_names section from terminal_colors.json, which is
        // generated by gen_xell_grammar.py from C++ source analysis.
        // No hardcoded lists — single source of truth.
        void buildBuiltinStyleCache()
        {
            for (auto &[name, scope] : theme_.builtinNames)
            {
                auto tokenStyle = getTokenStyle(theme_, scope);
                builtinStyleCache_[name] = {tokenStyle.fg, tokenStyle.bold, tokenStyle.italic};
            }
        }

        CachedStyle getStyleForToken(const xell::Token &tok) const
        {
            // Direct type lookup
            auto it = typeStyleCache_.find((int)tok.type);
            if (it != typeStyleCache_.end())
                return it->second;

            return {defaultFg_, false, false};
        }

        // Convert TokenType enum name string → int value
        // This maps the strings from terminal_colors.json (e.g. "FN", "GIVE")
        // to xell::TokenType enum values.
        static int enumNameToInt(const std::string &name)
        {
            static const std::unordered_map<std::string, int> map = {
                {"NUMBER", (int)xell::TokenType::NUMBER},
                {"IMAGINARY", (int)xell::TokenType::IMAGINARY},
                {"STRING", (int)xell::TokenType::STRING},
                {"RAW_STRING", (int)xell::TokenType::RAW_STRING},
                {"BYTE_STRING", (int)xell::TokenType::BYTE_STRING},
                {"TRUE_KW", (int)xell::TokenType::TRUE_KW},
                {"FALSE_KW", (int)xell::TokenType::FALSE_KW},
                {"NONE_KW", (int)xell::TokenType::NONE_KW},
                {"FN", (int)xell::TokenType::FN},
                {"GIVE", (int)xell::TokenType::GIVE},
                {"IF", (int)xell::TokenType::IF},
                {"ELIF", (int)xell::TokenType::ELIF},
                {"ELSE", (int)xell::TokenType::ELSE},
                {"FOR", (int)xell::TokenType::FOR},
                {"WHILE", (int)xell::TokenType::WHILE},
                {"IN", (int)xell::TokenType::IN},
                {"BREAK", (int)xell::TokenType::BREAK},
                {"CONTINUE", (int)xell::TokenType::CONTINUE},
                {"TRY", (int)xell::TokenType::TRY},
                {"CATCH", (int)xell::TokenType::CATCH},
                {"FINALLY", (int)xell::TokenType::FINALLY},
                {"INCASE", (int)xell::TokenType::INCASE},
                {"LET", (int)xell::TokenType::LET},
                {"BE", (int)xell::TokenType::BE},
                {"LOOP", (int)xell::TokenType::LOOP},
                {"BRING", (int)xell::TokenType::BRING},
                {"FROM", (int)xell::TokenType::FROM},
                {"AS", (int)xell::TokenType::AS},
                {"MODULE", (int)xell::TokenType::MODULE},
                {"EXPORT", (int)xell::TokenType::EXPORT},
                {"REQUIRES", (int)xell::TokenType::REQUIRES},
                {"ENUM", (int)xell::TokenType::ENUM},
                {"STRUCT", (int)xell::TokenType::STRUCT},
                {"CLASS", (int)xell::TokenType::CLASS},
                {"INHERITS", (int)xell::TokenType::INHERITS},
                {"IMMUTABLE", (int)xell::TokenType::IMMUTABLE},
                {"PRIVATE", (int)xell::TokenType::PRIVATE},
                {"PROTECTED", (int)xell::TokenType::PROTECTED},
                {"PUBLIC", (int)xell::TokenType::PUBLIC},
                {"STATIC", (int)xell::TokenType::STATIC},
                {"INTERFACE", (int)xell::TokenType::INTERFACE},
                {"IMPLEMENTS", (int)xell::TokenType::IMPLEMENTS},
                {"ABSTRACT", (int)xell::TokenType::ABSTRACT},
                {"MIXIN", (int)xell::TokenType::MIXIN},
                {"WITH", (int)xell::TokenType::WITH},
                {"YIELD", (int)xell::TokenType::YIELD},
                {"ASYNC", (int)xell::TokenType::ASYNC},
                {"AWAIT", (int)xell::TokenType::AWAIT},
                {"AND", (int)xell::TokenType::AND},
                {"OR", (int)xell::TokenType::OR},
                {"NOT", (int)xell::TokenType::NOT},
                {"IS", (int)xell::TokenType::IS},
                {"EQ", (int)xell::TokenType::EQ},
                {"NE", (int)xell::TokenType::NE},
                {"GT", (int)xell::TokenType::GT},
                {"LT", (int)xell::TokenType::LT},
                {"GE", (int)xell::TokenType::GE},
                {"LE", (int)xell::TokenType::LE},
                {"OF", (int)xell::TokenType::OF},
                {"PLUS", (int)xell::TokenType::PLUS},
                {"MINUS", (int)xell::TokenType::MINUS},
                {"STAR", (int)xell::TokenType::STAR},
                {"SLASH", (int)xell::TokenType::SLASH},
                {"PERCENT", (int)xell::TokenType::PERCENT},
                {"PLUS_PLUS", (int)xell::TokenType::PLUS_PLUS},
                {"MINUS_MINUS", (int)xell::TokenType::MINUS_MINUS},
                {"EQUAL", (int)xell::TokenType::EQUAL},
                {"EQUAL_EQUAL", (int)xell::TokenType::EQUAL_EQUAL},
                {"BANG", (int)xell::TokenType::BANG},
                {"BANG_EQUAL", (int)xell::TokenType::BANG_EQUAL},
                {"PLUS_EQUAL", (int)xell::TokenType::PLUS_EQUAL},
                {"MINUS_EQUAL", (int)xell::TokenType::MINUS_EQUAL},
                {"STAR_EQUAL", (int)xell::TokenType::STAR_EQUAL},
                {"SLASH_EQUAL", (int)xell::TokenType::SLASH_EQUAL},
                {"PERCENT_EQUAL", (int)xell::TokenType::PERCENT_EQUAL},
                {"GREATER", (int)xell::TokenType::GREATER},
                {"LESS", (int)xell::TokenType::LESS},
                {"GREATER_EQUAL", (int)xell::TokenType::GREATER_EQUAL},
                {"LESS_EQUAL", (int)xell::TokenType::LESS_EQUAL},
                {"ARROW", (int)xell::TokenType::ARROW},
                {"FAT_ARROW", (int)xell::TokenType::FAT_ARROW},
                {"DOT", (int)xell::TokenType::DOT},
                {"ELLIPSIS", (int)xell::TokenType::ELLIPSIS},
                {"AT", (int)xell::TokenType::AT},
                {"LPAREN", (int)xell::TokenType::LPAREN},
                {"RPAREN", (int)xell::TokenType::RPAREN},
                {"LBRACKET", (int)xell::TokenType::LBRACKET},
                {"RBRACKET", (int)xell::TokenType::RBRACKET},
                {"LBRACE", (int)xell::TokenType::LBRACE},
                {"RBRACE", (int)xell::TokenType::RBRACE},
                {"COMMA", (int)xell::TokenType::COMMA},
                {"COLON", (int)xell::TokenType::COLON},
                {"SEMICOLON", (int)xell::TokenType::SEMICOLON},
                {"PIPE", (int)xell::TokenType::PIPE},
                {"AMP_AMP", (int)xell::TokenType::AMP_AMP},
                {"PIPE_PIPE", (int)xell::TokenType::PIPE_PIPE},
                {"TILDE", (int)xell::TokenType::TILDE},
                {"IDENTIFIER", (int)xell::TokenType::IDENTIFIER},
            };

            auto it = map.find(name);
            return it != map.end() ? it->second : -1;
        }
    };

} // namespace xterm
