#pragma once

// =============================================================================
// highlighter.hpp — Syntax highlighting for the Xell Terminal IDE
// =============================================================================
// Uses the Xell Lexer to tokenize source code, then maps each token to a
// color using the theme loaded from terminal_colors.json.
//
// This is the bridge between:
//   - Xell Lexer (src/lexer/) → produces Token with TokenType
//   - Theme (terminal_colors.json) → maps TokenType enum name → scope → color
//   - Renderer → writes colored cells into the screen buffer
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
        }

        // Highlight a single line of source code → vector of ColoredSpan
        // The spans cover the entire line (gaps filled with default fg)
        std::vector<ColoredSpan> highlightLine(const std::string &line) const
        {
            std::vector<ColoredSpan> spans;

            if (line.empty())
                return spans;

            // Check for comment lines first (quick path)
            {
                size_t i = 0;
                while (i < line.size() && (line[i] == ' ' || line[i] == '\t'))
                    i++;
                if (i < line.size() && line[i] == '#')
                {
                    auto it = theme_.scopeColors.find("comment.line.number-sign.xell");
                    Color fg = it != theme_.scopeColors.end() ? it->second.fg : Color{92, 99, 112};
                    bool italic = it != theme_.scopeColors.end() ? it->second.italic : true;
                    spans.push_back({0, (int)line.size(), fg, {0, 0, 0, 0}, false, italic});
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

                for (const auto &tok : tokens)
                {
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

                    // For operators/punctuation, the value might be the operator symbol
                    // which is what the lexer stores in tok.value

                    // Find this token in the line starting from scanPos
                    size_t foundPos = line.find(searchText, scanPos);
                    if (foundPos == std::string::npos)
                    {
                        // Token not found at expected position — skip
                        continue;
                    }

                    int col = (int)foundPos;
                    int len = (int)searchText.size();

                    // Fill gap between last span end and this token (whitespace → default)
                    int lastEnd = spans.empty() ? 0 : spans.back().endCol;
                    if (col > lastEnd)
                    {
                        spans.push_back({lastEnd, col, defaultFg_, {0, 0, 0, 0}, false, false});
                    }

                    // Look up style for this token
                    auto style = getStyleForToken(tok);
                    spans.push_back({col, col + len, style.fg, {0, 0, 0, 0}, style.bold, style.italic});
                    scanPos = foundPos + len;
                }

                // Fill remainder of line with default
                int lastEnd = spans.empty() ? 0 : spans.back().endCol;
                if (lastEnd < (int)line.size())
                {
                    spans.push_back({lastEnd, (int)line.size(), defaultFg_, {0, 0, 0, 0}, false, false});
                }
            }
            catch (...)
            {
                // If tokenization fails, return the whole line as default color
                spans.push_back({0, (int)line.size(), defaultFg_, {0, 0, 0, 0}, false, false});
            }

            return spans;
        }

        // Highlight an entire buffer → one vector of spans per line
        std::vector<std::vector<ColoredSpan>> highlightAll(const std::vector<std::string> &lines) const
        {
            std::vector<std::vector<ColoredSpan>> result;
            result.reserve(lines.size());
            for (auto &line : lines)
                result.push_back(highlightLine(line));
            return result;
        }

    private:
        const ThemeData &theme_;
        Color defaultFg_ = Color::default_fg();

        // Pre-computed: TokenType int → style
        struct CachedStyle
        {
            Color fg;
            bool bold = false;
            bool italic = false;
        };
        std::unordered_map<int, CachedStyle> typeStyleCache_;

        // Scope for identifiers that are builtin names
        std::unordered_map<std::string, CachedStyle> builtinStyleCache_;

        void buildTokenTypeToStyleMap()
        {
            // Get default fg from theme
            auto it = theme_.ui.find("editor_fg");
            if (it != theme_.ui.end())
                defaultFg_ = it->second;

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
        }

        CachedStyle getStyleForToken(const xell::Token &tok) const
        {
            // Direct type lookup
            auto it = typeStyleCache_.find((int)tok.type);
            if (it != typeStyleCache_.end())
                return it->second;

            // For identifiers, check if it's a known builtin
            if (tok.type == xell::TokenType::IDENTIFIER)
            {
                auto bit = builtinStyleCache_.find(tok.value);
                if (bit != builtinStyleCache_.end())
                    return bit->second;
            }

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
