// =============================================================================
// Xell Parser Tests
// =============================================================================
// Comprehensive tests for the Xell lexer + parser pipeline.
// No external dependencies — minimal test framework included.
// =============================================================================

#include "../src/lexer/lexer.hpp"
#include "../src/parser/parser.hpp"
#include "../src/lib/errors/error.hpp"
#include <iostream>
#include <string>
#include <functional>
#include <cmath>
#include <sstream>

using namespace xell;

// ---- ostream support for TokenType (used by XASSERT_EQ) --------------------

inline std::ostream &operator<<(std::ostream &os, TokenType t)
{
    auto &names = tokenTypeNames();
    auto it = names.find(static_cast<int>(t));
    if (it != names.end())
        os << it->second;
    else
        os << "TokenType(" << static_cast<int>(t) << ")";
    return os;
}

// ---- Minimal test framework ------------------------------------------------

static int g_passed = 0;
static int g_failed = 0;

#define XASSERT(cond)                                                      \
    do                                                                     \
    {                                                                      \
        if (!(cond))                                                       \
        {                                                                  \
            std::ostringstream os;                                         \
            os << "Assertion failed: " #cond " (line " << __LINE__ << ")"; \
            throw std::runtime_error(os.str());                            \
        }                                                                  \
    } while (0)

#define XASSERT_EQ(a, b)                                      \
    do                                                        \
    {                                                         \
        if ((a) != (b))                                       \
        {                                                     \
            std::ostringstream os;                            \
            os << "Expected '" << (b) << "' but got '" << (a) \
               << "' (line " << __LINE__ << ")";              \
            throw std::runtime_error(os.str());               \
        }                                                     \
    } while (0)

static void runTest(const std::string &name, std::function<void()> fn)
{
    try
    {
        fn();
        std::cout << "  \033[32mPASS\033[0m: " << name << "\n";
        g_passed++;
    }
    catch (const std::exception &e)
    {
        std::cout << "  \033[31mFAIL\033[0m: " << name << "\n        " << e.what() << "\n";
        g_failed++;
    }
}

// ---- Helpers ----------------------------------------------------------------

static Program parseSource(const std::string &source)
{
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    return parser.parse();
}

template <typename T>
T *firstStmt(Program &prog)
{
    XASSERT(prog.statements.size() >= 1);
    return dynamic_cast<T *>(prog.statements[0].get());
}

template <typename T>
T *asExpr(Expr *e)
{
    auto *p = dynamic_cast<T *>(e);
    XASSERT(p != nullptr);
    return p;
}

// =============================================================================
// LEXER SMOKE TESTS
// =============================================================================

void test_lexer_simple_tokens()
{
    Lexer lexer("x = 10");
    auto tokens = lexer.tokenize();
    XASSERT_EQ(tokens.size(), (size_t)4); // IDENT EQUAL NUMBER EOF
    XASSERT_EQ(tokens[0].type, TokenType::IDENTIFIER);
    XASSERT_EQ(tokens[0].value, std::string("x"));
    XASSERT_EQ(tokens[1].type, TokenType::EQUAL);
    XASSERT_EQ(tokens[2].type, TokenType::NUMBER);
    XASSERT_EQ(tokens[2].value, std::string("10"));
    XASSERT_EQ(tokens[3].type, TokenType::EOF_TOKEN);
}

void test_lexer_all_keywords()
{
    Lexer lexer("fn give if elif else for while in bring from as true false none and or not is eq ne gt lt ge le of");
    auto tokens = lexer.tokenize();
    XASSERT_EQ(tokens[0].type, TokenType::FN);
    XASSERT_EQ(tokens[1].type, TokenType::GIVE);
    XASSERT_EQ(tokens[2].type, TokenType::IF);
    XASSERT_EQ(tokens[3].type, TokenType::ELIF);
    XASSERT_EQ(tokens[4].type, TokenType::ELSE);
    XASSERT_EQ(tokens[5].type, TokenType::FOR);
    XASSERT_EQ(tokens[6].type, TokenType::WHILE);
    XASSERT_EQ(tokens[7].type, TokenType::IN);
    XASSERT_EQ(tokens[8].type, TokenType::BRING);
    XASSERT_EQ(tokens[9].type, TokenType::FROM);
    XASSERT_EQ(tokens[10].type, TokenType::AS);
    XASSERT_EQ(tokens[11].type, TokenType::TRUE_KW);
    XASSERT_EQ(tokens[12].type, TokenType::FALSE_KW);
    XASSERT_EQ(tokens[13].type, TokenType::NONE_KW);
    XASSERT_EQ(tokens[14].type, TokenType::AND);
    XASSERT_EQ(tokens[15].type, TokenType::OR);
    XASSERT_EQ(tokens[16].type, TokenType::NOT);
    XASSERT_EQ(tokens[17].type, TokenType::IS);
    XASSERT_EQ(tokens[18].type, TokenType::EQ);
    XASSERT_EQ(tokens[19].type, TokenType::NE);
    XASSERT_EQ(tokens[20].type, TokenType::GT);
    XASSERT_EQ(tokens[21].type, TokenType::LT);
    XASSERT_EQ(tokens[22].type, TokenType::GE);
    XASSERT_EQ(tokens[23].type, TokenType::LE);
    XASSERT_EQ(tokens[24].type, TokenType::OF);
}

void test_lexer_all_operators()
{
    Lexer lexer("+ - * / ++ -- = == != > < >= <= -> . ! ( ) [ ] { } , : ;");
    auto tokens = lexer.tokenize();
    XASSERT_EQ(tokens[0].type, TokenType::PLUS);
    XASSERT_EQ(tokens[1].type, TokenType::MINUS);
    XASSERT_EQ(tokens[2].type, TokenType::STAR);
    XASSERT_EQ(tokens[3].type, TokenType::SLASH);
    XASSERT_EQ(tokens[4].type, TokenType::PLUS_PLUS);
    XASSERT_EQ(tokens[5].type, TokenType::MINUS_MINUS);
    XASSERT_EQ(tokens[6].type, TokenType::EQUAL);
    XASSERT_EQ(tokens[7].type, TokenType::EQUAL_EQUAL);
    XASSERT_EQ(tokens[8].type, TokenType::BANG_EQUAL);
    XASSERT_EQ(tokens[9].type, TokenType::GREATER);
    XASSERT_EQ(tokens[10].type, TokenType::LESS);
    XASSERT_EQ(tokens[11].type, TokenType::GREATER_EQUAL);
    XASSERT_EQ(tokens[12].type, TokenType::LESS_EQUAL);
    XASSERT_EQ(tokens[13].type, TokenType::ARROW);
    XASSERT_EQ(tokens[14].type, TokenType::DOT);
    XASSERT_EQ(tokens[15].type, TokenType::BANG);
    XASSERT_EQ(tokens[16].type, TokenType::LPAREN);
    XASSERT_EQ(tokens[17].type, TokenType::RPAREN);
    XASSERT_EQ(tokens[18].type, TokenType::LBRACKET);
    XASSERT_EQ(tokens[19].type, TokenType::RBRACKET);
    XASSERT_EQ(tokens[20].type, TokenType::LBRACE);
    XASSERT_EQ(tokens[21].type, TokenType::RBRACE);
    XASSERT_EQ(tokens[22].type, TokenType::COMMA);
    XASSERT_EQ(tokens[23].type, TokenType::COLON);
    XASSERT_EQ(tokens[24].type, TokenType::SEMICOLON);
}

void test_lexer_string()
{
    Lexer lexer("\"Hello, {name}!\"");
    auto tokens = lexer.tokenize();
    XASSERT_EQ(tokens[0].type, TokenType::STRING);
    XASSERT_EQ(tokens[0].value, std::string("Hello, {name}!"));
}

void test_lexer_number_with_decimal()
{
    Lexer lexer("3.14 42 100");
    auto tokens = lexer.tokenize();
    XASSERT_EQ(tokens[0].type, TokenType::NUMBER);
    XASSERT_EQ(tokens[0].value, std::string("3.14"));
    XASSERT_EQ(tokens[1].type, TokenType::NUMBER);
    XASSERT_EQ(tokens[1].value, std::string("42"));
}

void test_lexer_newlines()
{
    Lexer lexer("x = 1\ny = 2\n\nz = 3");
    auto tokens = lexer.tokenize();
    int newlineCount = 0;
    for (auto &t : tokens)
    {
        if (t.type == TokenType::NEWLINE)
            newlineCount++;
    }
    XASSERT_EQ(newlineCount, 2); // consecutive newlines collapsed
}

void test_lexer_comments()
{
    Lexer lexer("x = 1 # this is a comment\ny = 2");
    auto tokens = lexer.tokenize();
    XASSERT_EQ(tokens[0].type, TokenType::IDENTIFIER);
    XASSERT_EQ(tokens[1].type, TokenType::EQUAL);
    XASSERT_EQ(tokens[2].type, TokenType::NUMBER);
    XASSERT_EQ(tokens[3].type, TokenType::NEWLINE);
    XASSERT_EQ(tokens[4].type, TokenType::IDENTIFIER);
    XASSERT_EQ(tokens[4].value, std::string("y"));
}

void test_lexer_multiline_comment()
{
    Lexer lexer("x = --> this is\na comment <-- 10");
    auto tokens = lexer.tokenize();
    XASSERT_EQ(tokens[0].type, TokenType::IDENTIFIER);
    XASSERT_EQ(tokens[1].type, TokenType::EQUAL);
    XASSERT_EQ(tokens[2].type, TokenType::NUMBER);
    XASSERT_EQ(tokens[2].value, std::string("10"));
}

void test_lexer_arrow_vs_comment()
{
    Lexer lexer("config->host");
    auto tokens = lexer.tokenize();
    XASSERT_EQ(tokens[0].type, TokenType::IDENTIFIER);
    XASSERT_EQ(tokens[1].type, TokenType::ARROW);
    XASSERT_EQ(tokens[2].type, TokenType::IDENTIFIER);
    XASSERT_EQ(tokens[2].value, std::string("host"));
}

void test_lexer_newlines_suppressed_in_brackets()
{
    Lexer lexer("[1,\n2,\n3]");
    auto tokens = lexer.tokenize();
    for (auto &t : tokens)
    {
        XASSERT(t.type != TokenType::NEWLINE);
    }
}

void test_lexer_bang_standalone()
{
    Lexer lexer("!x");
    auto tokens = lexer.tokenize();
    XASSERT_EQ(tokens[0].type, TokenType::BANG);
    XASSERT_EQ(tokens[0].value, std::string("!"));
    XASSERT_EQ(tokens[1].type, TokenType::IDENTIFIER);
}

void test_lexer_plus_plus()
{
    Lexer lexer("x++ ++y");
    auto tokens = lexer.tokenize();
    XASSERT_EQ(tokens[0].type, TokenType::IDENTIFIER);
    XASSERT_EQ(tokens[1].type, TokenType::PLUS_PLUS);
    XASSERT_EQ(tokens[2].type, TokenType::PLUS_PLUS);
    XASSERT_EQ(tokens[3].type, TokenType::IDENTIFIER);
}

void test_lexer_minus_minus()
{
    Lexer lexer("count--");
    auto tokens = lexer.tokenize();
    XASSERT_EQ(tokens[0].type, TokenType::IDENTIFIER);
    XASSERT_EQ(tokens[1].type, TokenType::MINUS_MINUS);
}

void test_lexer_minus_minus_vs_comment()
{
    // "-->" should be a comment, "--" without ">" should be MINUS_MINUS
    Lexer lexer("x-- --> comment <-- y");
    auto tokens = lexer.tokenize();
    XASSERT_EQ(tokens[0].type, TokenType::IDENTIFIER);
    XASSERT_EQ(tokens[0].value, std::string("x"));
    XASSERT_EQ(tokens[1].type, TokenType::MINUS_MINUS);
    XASSERT_EQ(tokens[2].type, TokenType::IDENTIFIER);
    XASSERT_EQ(tokens[2].value, std::string("y"));
}

void test_lexer_escape_sequences()
{
    Lexer lexer("\"hello\\nworld\\t!\"");
    auto tokens = lexer.tokenize();
    XASSERT_EQ(tokens[0].type, TokenType::STRING);
    XASSERT_EQ(tokens[0].value, std::string("hello\nworld\t!"));
}

// =============================================================================
// PARSER TESTS — LITERALS & EXPRESSIONS
// =============================================================================

void test_parse_number_literal()
{
    auto prog = parseSource("42");
    auto *es = firstStmt<ExprStmt>(prog);
    XASSERT(es != nullptr);
    auto *num = asExpr<IntLiteral>(es->expr.get());
    XASSERT_EQ(num->value, (int64_t)42);
}

void test_parse_float_literal()
{
    auto prog = parseSource("3.14");
    auto *es = firstStmt<ExprStmt>(prog);
    auto *num = asExpr<FloatLiteral>(es->expr.get());
    XASSERT(std::abs(num->value - 3.14) < 0.001);
}

void test_parse_string_literal()
{
    auto prog = parseSource("\"hello world\"");
    auto *es = firstStmt<ExprStmt>(prog);
    auto *str = asExpr<StringLiteral>(es->expr.get());
    XASSERT_EQ(str->value, std::string("hello world"));
}

void test_parse_bool_true()
{
    auto prog = parseSource("true");
    auto *es = firstStmt<ExprStmt>(prog);
    auto *b = asExpr<BoolLiteral>(es->expr.get());
    XASSERT_EQ(b->value, true);
}

void test_parse_bool_false()
{
    auto prog = parseSource("false");
    auto *es = firstStmt<ExprStmt>(prog);
    auto *b = asExpr<BoolLiteral>(es->expr.get());
    XASSERT_EQ(b->value, false);
}

void test_parse_none_literal()
{
    auto prog = parseSource("none");
    auto *es = firstStmt<ExprStmt>(prog);
    XASSERT(dynamic_cast<NoneLiteral *>(es->expr.get()) != nullptr);
}

void test_parse_identifier()
{
    auto prog = parseSource("myVar");
    auto *es = firstStmt<ExprStmt>(prog);
    auto *id = asExpr<Identifier>(es->expr.get());
    XASSERT_EQ(id->name, std::string("myVar"));
}

// =============================================================================
// PARSER TESTS — ASSIGNMENT
// =============================================================================

void test_parse_simple_assignment()
{
    auto prog = parseSource("x = 10");
    auto *assign = firstStmt<Assignment>(prog);
    XASSERT(assign != nullptr);
    XASSERT_EQ(assign->name, std::string("x"));
    auto *num = asExpr<IntLiteral>(assign->value.get());
    XASSERT_EQ(num->value, (int64_t)10);
}

void test_parse_string_assignment()
{
    auto prog = parseSource("name = \"xell\"");
    auto *assign = firstStmt<Assignment>(prog);
    XASSERT_EQ(assign->name, std::string("name"));
    auto *str = asExpr<StringLiteral>(assign->value.get());
    XASSERT_EQ(str->value, std::string("xell"));
}

void test_parse_bool_assignment()
{
    auto prog = parseSource("debug = true");
    auto *assign = firstStmt<Assignment>(prog);
    auto *b = asExpr<BoolLiteral>(assign->value.get());
    XASSERT_EQ(b->value, true);
}

void test_parse_expr_assignment()
{
    auto prog = parseSource("result = x + 1");
    auto *assign = firstStmt<Assignment>(prog);
    auto *binop = asExpr<BinaryExpr>(assign->value.get());
    XASSERT_EQ(binop->op, std::string("+"));
}

// =============================================================================
// PARSER TESTS — ARITHMETIC EXPRESSIONS
// =============================================================================

void test_parse_addition()
{
    auto prog = parseSource("x = 1 + 2");
    auto *assign = firstStmt<Assignment>(prog);
    auto *bin = asExpr<BinaryExpr>(assign->value.get());
    XASSERT_EQ(bin->op, std::string("+"));
    auto *left = asExpr<IntLiteral>(bin->left.get());
    auto *right = asExpr<IntLiteral>(bin->right.get());
    XASSERT_EQ(left->value, (int64_t)1);
    XASSERT_EQ(right->value, (int64_t)2);
}

void test_parse_operator_precedence()
{
    // 1 + 2 * 3 → 1 + (2 * 3)
    auto prog = parseSource("x = 1 + 2 * 3");
    auto *assign = firstStmt<Assignment>(prog);
    auto *add = asExpr<BinaryExpr>(assign->value.get());
    XASSERT_EQ(add->op, std::string("+"));
    auto *mul = asExpr<BinaryExpr>(add->right.get());
    XASSERT_EQ(mul->op, std::string("*"));
}

void test_parse_grouped_expression()
{
    // (1 + 2) * 3
    auto prog = parseSource("x = (1 + 2) * 3");
    auto *assign = firstStmt<Assignment>(prog);
    auto *mul = asExpr<BinaryExpr>(assign->value.get());
    XASSERT_EQ(mul->op, std::string("*"));
    auto *add = asExpr<BinaryExpr>(mul->left.get());
    XASSERT_EQ(add->op, std::string("+"));
}

void test_parse_subtraction_and_division()
{
    auto prog = parseSource("x = 10 - 4 / 2");
    auto *assign = firstStmt<Assignment>(prog);
    auto *sub = asExpr<BinaryExpr>(assign->value.get());
    XASSERT_EQ(sub->op, std::string("-"));
    auto *div = asExpr<BinaryExpr>(sub->right.get());
    XASSERT_EQ(div->op, std::string("/"));
}

void test_parse_unary_not_keyword()
{
    auto prog = parseSource("x = not true");
    auto *assign = firstStmt<Assignment>(prog);
    auto *unary = asExpr<UnaryExpr>(assign->value.get());
    XASSERT_EQ(unary->op, std::string("not"));
    auto *b = asExpr<BoolLiteral>(unary->operand.get());
    XASSERT_EQ(b->value, true);
}

void test_parse_unary_bang()
{
    // ! should work the same as 'not'
    auto prog = parseSource("x = !true");
    auto *assign = firstStmt<Assignment>(prog);
    auto *unary = asExpr<UnaryExpr>(assign->value.get());
    XASSERT_EQ(unary->op, std::string("not")); // normalized to "not"
    auto *b = asExpr<BoolLiteral>(unary->operand.get());
    XASSERT_EQ(b->value, true);
}

void test_parse_unary_minus()
{
    auto prog = parseSource("x = -5");
    auto *assign = firstStmt<Assignment>(prog);
    auto *unary = asExpr<UnaryExpr>(assign->value.get());
    XASSERT_EQ(unary->op, std::string("-"));
    auto *num = asExpr<IntLiteral>(unary->operand.get());
    XASSERT_EQ(num->value, (int64_t)5);
}

// =============================================================================
// PARSER TESTS — INCREMENT / DECREMENT
// =============================================================================

void test_parse_prefix_increment()
{
    auto prog = parseSource("++x");
    auto *es = firstStmt<ExprStmt>(prog);
    auto *unary = asExpr<UnaryExpr>(es->expr.get());
    XASSERT_EQ(unary->op, std::string("++"));
    auto *id = asExpr<Identifier>(unary->operand.get());
    XASSERT_EQ(id->name, std::string("x"));
}

void test_parse_prefix_decrement()
{
    auto prog = parseSource("--count");
    auto *es = firstStmt<ExprStmt>(prog);
    auto *unary = asExpr<UnaryExpr>(es->expr.get());
    XASSERT_EQ(unary->op, std::string("--"));
    auto *id = asExpr<Identifier>(unary->operand.get());
    XASSERT_EQ(id->name, std::string("count"));
}

void test_parse_postfix_increment()
{
    auto prog = parseSource("x++");
    auto *es = firstStmt<ExprStmt>(prog);
    auto *post = asExpr<PostfixExpr>(es->expr.get());
    XASSERT_EQ(post->op, std::string("++"));
    auto *id = asExpr<Identifier>(post->operand.get());
    XASSERT_EQ(id->name, std::string("x"));
}

void test_parse_postfix_decrement()
{
    auto prog = parseSource("count--");
    auto *es = firstStmt<ExprStmt>(prog);
    auto *post = asExpr<PostfixExpr>(es->expr.get());
    XASSERT_EQ(post->op, std::string("--"));
    auto *id = asExpr<Identifier>(post->operand.get());
    XASSERT_EQ(id->name, std::string("count"));
}

void test_parse_increment_in_while()
{
    auto prog = parseSource(
        "while count lt 5 :\n"
        "    count++\n"
        ";");
    auto *ws = firstStmt<WhileStmt>(prog);
    XASSERT_EQ(ws->body.size(), (size_t)1);
    auto *body = dynamic_cast<ExprStmt *>(ws->body[0].get());
    XASSERT(body != nullptr);
    auto *post = asExpr<PostfixExpr>(body->expr.get());
    XASSERT_EQ(post->op, std::string("++"));
}

// =============================================================================
// PARSER TESTS — COMPARISON & LOGICAL
// =============================================================================

void test_parse_equality_symbolic()
{
    auto prog = parseSource("x = a == b");
    auto *assign = firstStmt<Assignment>(prog);
    auto *bin = asExpr<BinaryExpr>(assign->value.get());
    XASSERT_EQ(bin->op, std::string("=="));
}

void test_parse_equality_is_keyword()
{
    auto prog = parseSource("x = a is b");
    auto *assign = firstStmt<Assignment>(prog);
    auto *bin = asExpr<BinaryExpr>(assign->value.get());
    XASSERT_EQ(bin->op, std::string("==")); // normalized
}

void test_parse_equality_eq_keyword()
{
    auto prog = parseSource("x = a eq b");
    auto *assign = firstStmt<Assignment>(prog);
    auto *bin = asExpr<BinaryExpr>(assign->value.get());
    XASSERT_EQ(bin->op, std::string("==")); // normalized
}

void test_parse_ne_keyword()
{
    auto prog = parseSource("x = a ne b");
    auto *assign = firstStmt<Assignment>(prog);
    auto *bin = asExpr<BinaryExpr>(assign->value.get());
    XASSERT_EQ(bin->op, std::string("!="));
}

void test_parse_gt_keyword()
{
    auto prog = parseSource("x = count gt 5");
    auto *assign = firstStmt<Assignment>(prog);
    auto *bin = asExpr<BinaryExpr>(assign->value.get());
    XASSERT_EQ(bin->op, std::string(">"));
}

void test_parse_lt_keyword()
{
    auto prog = parseSource("x = count lt 5");
    auto *assign = firstStmt<Assignment>(prog);
    auto *bin = asExpr<BinaryExpr>(assign->value.get());
    XASSERT_EQ(bin->op, std::string("<"));
}

void test_parse_ge_keyword()
{
    auto prog = parseSource("x = score ge 90");
    auto *assign = firstStmt<Assignment>(prog);
    auto *bin = asExpr<BinaryExpr>(assign->value.get());
    XASSERT_EQ(bin->op, std::string(">="));
}

void test_parse_le_keyword()
{
    auto prog = parseSource("x = score le 100");
    auto *assign = firstStmt<Assignment>(prog);
    auto *bin = asExpr<BinaryExpr>(assign->value.get());
    XASSERT_EQ(bin->op, std::string("<="));
}

void test_parse_logical_and()
{
    auto prog = parseSource("x = a and b");
    auto *assign = firstStmt<Assignment>(prog);
    auto *bin = asExpr<BinaryExpr>(assign->value.get());
    XASSERT_EQ(bin->op, std::string("and"));
}

void test_parse_logical_or()
{
    auto prog = parseSource("x = a or b");
    auto *assign = firstStmt<Assignment>(prog);
    auto *bin = asExpr<BinaryExpr>(assign->value.get());
    XASSERT_EQ(bin->op, std::string("or"));
}

void test_parse_complex_logical()
{
    // a > 0 and a < 10 or b == 0  → (a>0 and a<10) or (b==0)
    auto prog = parseSource("x = a > 0 and a < 10 or b == 0");
    auto *assign = firstStmt<Assignment>(prog);
    auto *orExpr = asExpr<BinaryExpr>(assign->value.get());
    XASSERT_EQ(orExpr->op, std::string("or"));
    auto *andExpr = asExpr<BinaryExpr>(orExpr->left.get());
    XASSERT_EQ(andExpr->op, std::string("and"));
    auto *eq = asExpr<BinaryExpr>(orExpr->right.get());
    XASSERT_EQ(eq->op, std::string("=="));
}

void test_parse_bang_in_condition()
{
    auto prog = parseSource(
        "if !ready :\n"
        "    print \"not ready\"\n"
        ";");
    auto *ifStmt = firstStmt<IfStmt>(prog);
    auto *unary = asExpr<UnaryExpr>(ifStmt->condition.get());
    XASSERT_EQ(unary->op, std::string("not")); // normalized
}

// =============================================================================
// PARSER TESTS — LIST & MAP LITERALS
// =============================================================================

void test_parse_list_literal()
{
    auto prog = parseSource("ports = [3000, 8080, 9000]");
    auto *assign = firstStmt<Assignment>(prog);
    auto *list = asExpr<ListLiteral>(assign->value.get());
    XASSERT_EQ(list->elements.size(), (size_t)3);
}

void test_parse_empty_list()
{
    auto prog = parseSource("x = []");
    auto *assign = firstStmt<Assignment>(prog);
    auto *list = asExpr<ListLiteral>(assign->value.get());
    XASSERT_EQ(list->elements.size(), (size_t)0);
}

void test_parse_nested_list()
{
    auto prog = parseSource("x = [[1, 2], [3, 4]]");
    auto *assign = firstStmt<Assignment>(prog);
    auto *list = asExpr<ListLiteral>(assign->value.get());
    XASSERT_EQ(list->elements.size(), (size_t)2);
    auto *inner1 = asExpr<ListLiteral>(list->elements[0].get());
    XASSERT_EQ(inner1->elements.size(), (size_t)2);
}

void test_parse_map_literal()
{
    auto prog = parseSource("config = { host: \"localhost\", port: 3000, debug: true }");
    auto *assign = firstStmt<Assignment>(prog);
    auto *map = asExpr<MapLiteral>(assign->value.get());
    XASSERT_EQ(map->entries.size(), (size_t)3);
    XASSERT_EQ(map->entries[0].first, std::string("host"));
    XASSERT_EQ(map->entries[1].first, std::string("port"));
    XASSERT_EQ(map->entries[2].first, std::string("debug"));
}

void test_parse_empty_map()
{
    auto prog = parseSource("x = {}");
    auto *assign = firstStmt<Assignment>(prog);
    auto *map = asExpr<MapLiteral>(assign->value.get());
    XASSERT_EQ(map->entries.size(), (size_t)0);
}

void test_parse_multiline_map()
{
    auto prog = parseSource(
        "config = {\n"
        "    host: \"localhost\",\n"
        "    port: 3000\n"
        "}");
    auto *assign = firstStmt<Assignment>(prog);
    auto *map = asExpr<MapLiteral>(assign->value.get());
    XASSERT_EQ(map->entries.size(), (size_t)2);
}

void test_parse_mixed_list_types()
{
    auto prog = parseSource("mixed = [1, \"hello\", true, none]");
    auto *assign = firstStmt<Assignment>(prog);
    auto *list = asExpr<ListLiteral>(assign->value.get());
    XASSERT_EQ(list->elements.size(), (size_t)4);
    asExpr<IntLiteral>(list->elements[0].get());
    asExpr<StringLiteral>(list->elements[1].get());
    asExpr<BoolLiteral>(list->elements[2].get());
    XASSERT(dynamic_cast<NoneLiteral *>(list->elements[3].get()) != nullptr);
}

// =============================================================================
// PARSER TESTS — FUNCTION CALLS
// =============================================================================

void test_parse_call_no_args()
{
    auto prog = parseSource("foo()");
    auto *es = firstStmt<ExprStmt>(prog);
    auto *call = asExpr<CallExpr>(es->expr.get());
    XASSERT_EQ(call->callee, std::string("foo"));
    XASSERT_EQ(call->args.size(), (size_t)0);
}

void test_parse_call_with_args()
{
    auto prog = parseSource("greet(\"Alice\", \"Hello\")");
    auto *es = firstStmt<ExprStmt>(prog);
    auto *call = asExpr<CallExpr>(es->expr.get());
    XASSERT_EQ(call->callee, std::string("greet"));
    XASSERT_EQ(call->args.size(), (size_t)2);
}

void test_parse_call_with_expr_args()
{
    auto prog = parseSource("add(x + 1, y * 2)");
    auto *es = firstStmt<ExprStmt>(prog);
    auto *call = asExpr<CallExpr>(es->expr.get());
    XASSERT_EQ(call->args.size(), (size_t)2);
    asExpr<BinaryExpr>(call->args[0].get());
}

void test_parse_parenless_call_single_arg()
{
    auto prog = parseSource("print \"hello\"");
    auto *es = firstStmt<ExprStmt>(prog);
    auto *call = asExpr<CallExpr>(es->expr.get());
    XASSERT_EQ(call->callee, std::string("print"));
    XASSERT_EQ(call->args.size(), (size_t)1);
    auto *arg = asExpr<StringLiteral>(call->args[0].get());
    XASSERT_EQ(arg->value, std::string("hello"));
}

void test_parse_parenless_call_multi_args()
{
    auto prog = parseSource("copy \"a.txt\" \"b.txt\"");
    auto *es = firstStmt<ExprStmt>(prog);
    auto *call = asExpr<CallExpr>(es->expr.get());
    XASSERT_EQ(call->callee, std::string("copy"));
    XASSERT_EQ(call->args.size(), (size_t)2);
}

void test_parse_parenless_call_identifier_arg()
{
    auto prog = parseSource("print result");
    auto *es = firstStmt<ExprStmt>(prog);
    auto *call = asExpr<CallExpr>(es->expr.get());
    XASSERT_EQ(call->callee, std::string("print"));
    XASSERT_EQ(call->args.size(), (size_t)1);
    asExpr<Identifier>(call->args[0].get());
}

void test_parse_nested_call()
{
    auto prog = parseSource("result = add(1, multiply(2, 3))");
    auto *assign = firstStmt<Assignment>(prog);
    auto *call = asExpr<CallExpr>(assign->value.get());
    XASSERT_EQ(call->callee, std::string("add"));
    auto *inner = asExpr<CallExpr>(call->args[1].get());
    XASSERT_EQ(inner->callee, std::string("multiply"));
}

void test_parse_call_result_assignment()
{
    auto prog = parseSource("result = setup(\"my-app\", \"1.0\")");
    auto *assign = firstStmt<Assignment>(prog);
    auto *call = asExpr<CallExpr>(assign->value.get());
    XASSERT_EQ(call->callee, std::string("setup"));
    XASSERT_EQ(call->args.size(), (size_t)2);
}

// =============================================================================
// PARSER TESTS — INDEX & MEMBER ACCESS
// =============================================================================

void test_parse_index_access()
{
    auto prog = parseSource("x = ports[0]");
    auto *assign = firstStmt<Assignment>(prog);
    auto *idx = asExpr<IndexAccess>(assign->value.get());
    auto *obj = asExpr<Identifier>(idx->object.get());
    XASSERT_EQ(obj->name, std::string("ports"));
}

void test_parse_member_access()
{
    auto prog = parseSource("x = config->host");
    auto *assign = firstStmt<Assignment>(prog);
    auto *mem = asExpr<MemberAccess>(assign->value.get());
    auto *obj = asExpr<Identifier>(mem->object.get());
    XASSERT_EQ(obj->name, std::string("config"));
    XASSERT_EQ(mem->member, std::string("host"));
}

void test_parse_chained_access()
{
    auto prog = parseSource("x = config->tags[0]");
    auto *assign = firstStmt<Assignment>(prog);
    auto *idx = asExpr<IndexAccess>(assign->value.get());
    auto *mem = asExpr<MemberAccess>(idx->object.get());
    XASSERT_EQ(mem->member, std::string("tags"));
}

void test_parse_string_index_access()
{
    auto prog = parseSource("x = config[\"host\"]");
    auto *assign = firstStmt<Assignment>(prog);
    auto *idx = asExpr<IndexAccess>(assign->value.get());
    auto *key = asExpr<StringLiteral>(idx->index.get());
    XASSERT_EQ(key->value, std::string("host"));
}

// =============================================================================
// PARSER TESTS — IF / ELIF / ELSE
// =============================================================================

void test_parse_simple_if()
{
    auto prog = parseSource(
        "if x == 1 :\n"
        "    print \"yes\"\n"
        ";");
    auto *ifStmt = firstStmt<IfStmt>(prog);
    XASSERT(ifStmt != nullptr);
    auto *cond = asExpr<BinaryExpr>(ifStmt->condition.get());
    XASSERT_EQ(cond->op, std::string("=="));
    XASSERT_EQ(ifStmt->body.size(), (size_t)1);
    XASSERT_EQ(ifStmt->elifs.size(), (size_t)0);
    XASSERT_EQ(ifStmt->elseBody.size(), (size_t)0);
}

void test_parse_if_else()
{
    auto prog = parseSource(
        "if x :\n"
        "    print \"yes\"\n"
        ";\n"
        "else :\n"
        "    print \"no\"\n"
        ";");
    auto *ifStmt = firstStmt<IfStmt>(prog);
    XASSERT_EQ(ifStmt->body.size(), (size_t)1);
    XASSERT_EQ(ifStmt->elseBody.size(), (size_t)1);
}

void test_parse_if_elif_else()
{
    auto prog = parseSource(
        "if score ge 90 :\n"
        "    print \"A\"\n"
        ";\n"
        "elif score ge 75 :\n"
        "    print \"B\"\n"
        ";\n"
        "elif score ge 60 :\n"
        "    print \"C\"\n"
        ";\n"
        "else :\n"
        "    print \"F\"\n"
        ";");
    auto *ifStmt = firstStmt<IfStmt>(prog);
    XASSERT_EQ(ifStmt->elifs.size(), (size_t)2);
    XASSERT_EQ(ifStmt->elseBody.size(), (size_t)1);
    auto *elifCond = asExpr<BinaryExpr>(ifStmt->elifs[0].condition.get());
    XASSERT_EQ(elifCond->op, std::string(">="));
}

void test_parse_if_with_logical()
{
    auto prog = parseSource(
        "if debug is true and host is \"localhost\" :\n"
        "    print \"local debug\"\n"
        ";");
    auto *ifStmt = firstStmt<IfStmt>(prog);
    auto *andExpr = asExpr<BinaryExpr>(ifStmt->condition.get());
    XASSERT_EQ(andExpr->op, std::string("and"));
}

// =============================================================================
// PARSER TESTS — FOR LOOP
// =============================================================================

void test_parse_for_loop()
{
    auto prog = parseSource(
        "for port in ports :\n"
        "    print port\n"
        ";");
    auto *forStmt = firstStmt<ForStmt>(prog);
    XASSERT_EQ(forStmt->varName, std::string("port"));
    auto *iter = asExpr<Identifier>(forStmt->iterable.get());
    XASSERT_EQ(iter->name, std::string("ports"));
    XASSERT_EQ(forStmt->body.size(), (size_t)1);
}

void test_parse_for_with_call_iterable()
{
    auto prog = parseSource(
        "for file in list_files(\"./src\") :\n"
        "    print file\n"
        ";");
    auto *forStmt = firstStmt<ForStmt>(prog);
    auto *call = asExpr<CallExpr>(forStmt->iterable.get());
    XASSERT_EQ(call->callee, std::string("list_files"));
}

// =============================================================================
// PARSER TESTS — WHILE LOOP
// =============================================================================

void test_parse_while_loop()
{
    auto prog = parseSource(
        "while count lt 5 :\n"
        "    count = count + 1\n"
        ";");
    auto *ws = firstStmt<WhileStmt>(prog);
    auto *cond = asExpr<BinaryExpr>(ws->condition.get());
    XASSERT_EQ(cond->op, std::string("<")); // lt → <
    XASSERT_EQ(ws->body.size(), (size_t)1);
}

// =============================================================================
// PARSER TESTS — FUNCTION DEFINITION
// =============================================================================

void test_parse_fn_def_no_params()
{
    auto prog = parseSource("fn noop() :\n;\n");
    auto *fn = firstStmt<FnDef>(prog);
    XASSERT_EQ(fn->name, std::string("noop"));
    XASSERT_EQ(fn->params.size(), (size_t)0);
    XASSERT_EQ(fn->body.size(), (size_t)0);
}

void test_parse_fn_def_with_params()
{
    auto prog = parseSource(
        "fn greet(name, greeting) :\n"
        "    print greeting\n"
        ";");
    auto *fn = firstStmt<FnDef>(prog);
    XASSERT_EQ(fn->name, std::string("greet"));
    XASSERT_EQ(fn->params.size(), (size_t)2);
    XASSERT_EQ(fn->params[0], std::string("name"));
    XASSERT_EQ(fn->params[1], std::string("greeting"));
}

void test_parse_fn_def_with_give()
{
    auto prog = parseSource(
        "fn add(a, b) :\n"
        "    give a + b\n"
        ";");
    auto *fn = firstStmt<FnDef>(prog);
    XASSERT_EQ(fn->body.size(), (size_t)1);
    auto *give = dynamic_cast<GiveStmt *>(fn->body[0].get());
    XASSERT(give != nullptr);
    XASSERT(give->value != nullptr);
    auto *addExpr = asExpr<BinaryExpr>(give->value.get());
    XASSERT_EQ(addExpr->op, std::string("+"));
}

void test_parse_fn_def_give_no_value()
{
    auto prog = parseSource(
        "fn doSomething() :\n"
        "    mkdir \"output\"\n"
        "    give\n"
        ";");
    auto *fn = firstStmt<FnDef>(prog);
    XASSERT_EQ(fn->body.size(), (size_t)2);
    auto *give = dynamic_cast<GiveStmt *>(fn->body[1].get());
    XASSERT(give != nullptr);
    XASSERT(give->value == nullptr);
}

// =============================================================================
// PARSER TESTS — BRING (IMPORT)
// =============================================================================

void test_parse_bring_all()
{
    auto prog = parseSource("bring * from \"./helpers.xel\"");
    auto *bring = firstStmt<BringStmt>(prog);
    XASSERT_EQ(bring->bringAll, true);
    XASSERT_EQ(bring->path, std::string("./helpers.xel"));
}

void test_parse_bring_named()
{
    auto prog = parseSource("bring setup from \"./helpers.xel\"");
    auto *bring = firstStmt<BringStmt>(prog);
    XASSERT_EQ(bring->bringAll, false);
    XASSERT_EQ(bring->names.size(), (size_t)1);
    XASSERT_EQ(bring->names[0], std::string("setup"));
}

void test_parse_bring_multiple_with_aliases()
{
    auto prog = parseSource("bring setup, deploy from \"./helpers.xel\" as s, d");
    auto *bring = firstStmt<BringStmt>(prog);
    XASSERT_EQ(bring->names.size(), (size_t)2);
    XASSERT_EQ(bring->names[0], std::string("setup"));
    XASSERT_EQ(bring->names[1], std::string("deploy"));
    XASSERT_EQ(bring->aliases.size(), (size_t)2);
    XASSERT_EQ(bring->aliases[0], std::string("s"));
    XASSERT_EQ(bring->aliases[1], std::string("d"));
}

// =============================================================================
// PARSER TESTS — MULTIPLE STATEMENTS & COMPLEX
// =============================================================================

void test_parse_multiple_statements()
{
    auto prog = parseSource("x = 10\ny = 20\nz = x + y");
    XASSERT_EQ(prog.statements.size(), (size_t)3);
}

void test_parse_dot_terminated_statements()
{
    auto prog = parseSource("x = 10 .\ny = 20 .");
    XASSERT_EQ(prog.statements.size(), (size_t)2);
}

void test_parse_nested_if_for()
{
    auto prog = parseSource(
        "if ready :\n"
        "    for file in files :\n"
        "        print file\n"
        "    ;\n"
        ";");
    auto *ifStmt = firstStmt<IfStmt>(prog);
    XASSERT_EQ(ifStmt->body.size(), (size_t)1);
    auto *inner = dynamic_cast<ForStmt *>(ifStmt->body[0].get());
    XASSERT(inner != nullptr);
    XASSERT_EQ(inner->body.size(), (size_t)1);
}

void test_parse_comments_ignored()
{
    auto prog = parseSource(
        "# comment\n"
        "x = 10  # inline\n"
        "--> block <--\n"
        "y = 20");
    XASSERT_EQ(prog.statements.size(), (size_t)2);
}

void test_parse_string_interpolation_preserved()
{
    auto prog = parseSource("msg = \"Hello, {name}! Port: {port}\"");
    auto *assign = firstStmt<Assignment>(prog);
    auto *str = asExpr<StringLiteral>(assign->value.get());
    XASSERT_EQ(str->value, std::string("Hello, {name}! Port: {port}"));
}

void test_parse_full_program()
{
    auto prog = parseSource(
        "name = \"john\"\n"
        "ports = [3000, 8080]\n"
        "config = { host: \"localhost\", debug: true }\n"
        "\n"
        "fn setup(project, version) :\n"
        "    mkdir \"{project}/src\"\n"
        "    mkdir \"{project}/tests\"\n"
        "    path = \"{project}/.config\"\n"
        "    give path\n"
        ";\n"
        "\n"
        "if config->debug == true :\n"
        "    print \"debug mode\"\n"
        ";\n"
        "else :\n"
        "    print \"production\"\n"
        ";\n"
        "\n"
        "for port in ports :\n"
        "    print \"starting on port\"\n"
        ";\n"
        "\n"
        "result = setup(\"my-app\", \"1.0\")\n"
        "print result\n");

    // 8 statements: 3 assignments, fn, if/else, for, assignment, print
    XASSERT_EQ(prog.statements.size(), (size_t)8);

    auto *fn = dynamic_cast<FnDef *>(prog.statements[3].get());
    XASSERT(fn != nullptr);
    XASSERT_EQ(fn->name, std::string("setup"));
    XASSERT_EQ(fn->params.size(), (size_t)2);
    XASSERT_EQ(fn->body.size(), (size_t)4);

    auto *ifStmt = dynamic_cast<IfStmt *>(prog.statements[4].get());
    XASSERT(ifStmt != nullptr);
    XASSERT_EQ(ifStmt->elseBody.size(), (size_t)1);
}

// =============================================================================
// MAIN — Run all tests
// =============================================================================

int main()
{
    std::cout << "\n===== Xell Lexer Tests =====\n";
    runTest("lexer: simple tokens", test_lexer_simple_tokens);
    runTest("lexer: all keywords", test_lexer_all_keywords);
    runTest("lexer: all operators", test_lexer_all_operators);
    runTest("lexer: string with interpolation", test_lexer_string);
    runTest("lexer: numbers", test_lexer_number_with_decimal);
    runTest("lexer: newlines collapsed", test_lexer_newlines);
    runTest("lexer: single-line comment", test_lexer_comments);
    runTest("lexer: multi-line comment", test_lexer_multiline_comment);
    runTest("lexer: arrow vs comment", test_lexer_arrow_vs_comment);
    runTest("lexer: newlines in brackets", test_lexer_newlines_suppressed_in_brackets);
    runTest("lexer: ! standalone", test_lexer_bang_standalone);
    runTest("lexer: ++ token", test_lexer_plus_plus);
    runTest("lexer: -- token", test_lexer_minus_minus);
    runTest("lexer: -- vs --> comment", test_lexer_minus_minus_vs_comment);
    runTest("lexer: escape sequences", test_lexer_escape_sequences);

    std::cout << "\n===== Parser: Literals =====\n";
    runTest("parse: number literal", test_parse_number_literal);
    runTest("parse: float literal", test_parse_float_literal);
    runTest("parse: string literal", test_parse_string_literal);
    runTest("parse: bool true", test_parse_bool_true);
    runTest("parse: bool false", test_parse_bool_false);
    runTest("parse: none literal", test_parse_none_literal);
    runTest("parse: identifier", test_parse_identifier);

    std::cout << "\n===== Parser: Assignment =====\n";
    runTest("parse: simple assignment", test_parse_simple_assignment);
    runTest("parse: string assignment", test_parse_string_assignment);
    runTest("parse: bool assignment", test_parse_bool_assignment);
    runTest("parse: expression assignment", test_parse_expr_assignment);

    std::cout << "\n===== Parser: Arithmetic =====\n";
    runTest("parse: addition", test_parse_addition);
    runTest("parse: operator precedence", test_parse_operator_precedence);
    runTest("parse: grouped expression", test_parse_grouped_expression);
    runTest("parse: subtraction and division", test_parse_subtraction_and_division);
    runTest("parse: unary 'not' keyword", test_parse_unary_not_keyword);
    runTest("parse: unary '!' operator", test_parse_unary_bang);
    runTest("parse: unary minus", test_parse_unary_minus);

    std::cout << "\n===== Parser: Increment / Decrement =====\n";
    runTest("parse: prefix ++", test_parse_prefix_increment);
    runTest("parse: prefix --", test_parse_prefix_decrement);
    runTest("parse: postfix ++", test_parse_postfix_increment);
    runTest("parse: postfix --", test_parse_postfix_decrement);
    runTest("parse: ++ inside while loop", test_parse_increment_in_while);

    std::cout << "\n===== Parser: Comparison & Logical =====\n";
    runTest("parse: ==", test_parse_equality_symbolic);
    runTest("parse: 'is' keyword", test_parse_equality_is_keyword);
    runTest("parse: 'eq' keyword", test_parse_equality_eq_keyword);
    runTest("parse: 'ne' keyword", test_parse_ne_keyword);
    runTest("parse: 'gt' keyword", test_parse_gt_keyword);
    runTest("parse: 'lt' keyword", test_parse_lt_keyword);
    runTest("parse: 'ge' keyword", test_parse_ge_keyword);
    runTest("parse: 'le' keyword", test_parse_le_keyword);
    runTest("parse: 'and'", test_parse_logical_and);
    runTest("parse: 'or'", test_parse_logical_or);
    runTest("parse: complex logical precedence", test_parse_complex_logical);
    runTest("parse: ! in if condition", test_parse_bang_in_condition);

    std::cout << "\n===== Parser: Lists & Maps =====\n";
    runTest("parse: list literal", test_parse_list_literal);
    runTest("parse: empty list", test_parse_empty_list);
    runTest("parse: nested list", test_parse_nested_list);
    runTest("parse: map literal", test_parse_map_literal);
    runTest("parse: empty map", test_parse_empty_map);
    runTest("parse: multiline map", test_parse_multiline_map);
    runTest("parse: mixed list types", test_parse_mixed_list_types);

    std::cout << "\n===== Parser: Function Calls =====\n";
    runTest("parse: call no args", test_parse_call_no_args);
    runTest("parse: call with args", test_parse_call_with_args);
    runTest("parse: call with expression args", test_parse_call_with_expr_args);
    runTest("parse: paren-less call single arg", test_parse_parenless_call_single_arg);
    runTest("parse: paren-less call multi args", test_parse_parenless_call_multi_args);
    runTest("parse: paren-less call identifier", test_parse_parenless_call_identifier_arg);
    runTest("parse: nested call", test_parse_nested_call);
    runTest("parse: call result assignment", test_parse_call_result_assignment);

    std::cout << "\n===== Parser: Access =====\n";
    runTest("parse: index access", test_parse_index_access);
    runTest("parse: member access ->", test_parse_member_access);
    runTest("parse: chained ->[]", test_parse_chained_access);
    runTest("parse: string index access", test_parse_string_index_access);

    std::cout << "\n===== Parser: Control Flow =====\n";
    runTest("parse: simple if", test_parse_simple_if);
    runTest("parse: if/else", test_parse_if_else);
    runTest("parse: if/elif/else", test_parse_if_elif_else);
    runTest("parse: if with logical", test_parse_if_with_logical);

    std::cout << "\n===== Parser: Loops =====\n";
    runTest("parse: for loop", test_parse_for_loop);
    runTest("parse: for with call iterable", test_parse_for_with_call_iterable);
    runTest("parse: while loop", test_parse_while_loop);

    std::cout << "\n===== Parser: Functions =====\n";
    runTest("parse: fn def no params", test_parse_fn_def_no_params);
    runTest("parse: fn def with params", test_parse_fn_def_with_params);
    runTest("parse: fn def with give", test_parse_fn_def_with_give);
    runTest("parse: fn def give no value", test_parse_fn_def_give_no_value);

    std::cout << "\n===== Parser: Bring =====\n";
    runTest("parse: bring all", test_parse_bring_all);
    runTest("parse: bring named", test_parse_bring_named);
    runTest("parse: bring with aliases", test_parse_bring_multiple_with_aliases);

    std::cout << "\n===== Parser: Complex =====\n";
    runTest("parse: multiple statements", test_parse_multiple_statements);
    runTest("parse: dot-terminated statements", test_parse_dot_terminated_statements);
    runTest("parse: nested if/for", test_parse_nested_if_for);
    runTest("parse: comments ignored", test_parse_comments_ignored);
    runTest("parse: string interpolation", test_parse_string_interpolation_preserved);
    runTest("parse: full program", test_parse_full_program);

    // Summary
    std::cout << "\n============================================\n";
    std::cout << "  Total: " << (g_passed + g_failed)
              << "  |  \033[32mPassed: " << g_passed
              << "\033[0m  |  \033[31mFailed: " << g_failed << "\033[0m\n";
    std::cout << "============================================\n\n";

    return g_failed > 0 ? 1 : 0;
}
