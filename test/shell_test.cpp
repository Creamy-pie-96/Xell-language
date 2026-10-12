// =============================================================================
// Shell Chaining Tests
// =============================================================================
// Tests for Xell's shell command chaining operators:
//   |   — pipe operator (builds pipeline strings)
//   &&  — shell AND (execute right only if left succeeds: exit code 0)
//   ||  — shell OR  (execute right only if left fails: exit code non-0)
//   set_e() / unset_e() — exit-on-error mode (like bash set -e)
//   exit_code() — returns last command's exit code
//   Grouping — (expr1 && expr2) || expr3
// =============================================================================

#include "../src/interpreter/interpreter.hpp"
#include "../src/lexer/lexer.hpp"
#include "../src/parser/parser.hpp"
#include <iostream>
#include <sstream>
#include <functional>
#include <string>
#include <vector>

using namespace xell;

// ---- Minimal test framework ------------------------------------------------

static int g_passed = 0;
static int g_failed = 0;

static void runTest(const std::string &name, std::function<void()> fn)
{
    try
    {
        fn();
        std::cout << "  PASS: " << name << "\n";
        g_passed++;
    }
    catch (const std::exception &e)
    {
        std::cout << "  FAIL: " << name << "\n        " << e.what() << "\n";
        g_failed++;
    }
}

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

#define XASSERT_EQ(a, b)                                 \
    do                                                   \
    {                                                    \
        if ((a) != (b))                                  \
        {                                                \
            std::ostringstream os;                       \
            os << "Expected [" << (a) << "] == [" << (b) \
               << "] (line " << __LINE__ << ")";         \
            throw std::runtime_error(os.str());          \
        }                                                \
    } while (0)

// Helper: run Xell source and return the output lines
static std::vector<std::string> runXell(const std::string &source)
{
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto program = parser.parse();
    Interpreter interp;
    interp.run(program);
    return interp.output();
}

// Helper: run Xell source and expect a specific exception type
template <typename ExcType>
static bool expectError(const std::string &source)
{
    try
    {
        runXell(source);
        return false;
    }
    catch (const ExcType &)
    {
        return true;
    }
    catch (...)
    {
        return false;
    }
}

// ============================================================================
// PIPE OPERATOR TESTS
// ============================================================================

void testPipeBasic()
{
    // |> concatenates two strings with " | "
    auto out = runXell(R"(
        result = "ls -la" |> "grep foo"
        print(result)
    )");
    XASSERT_EQ(out.size(), 1u);
    XASSERT_EQ(out[0], "ls -la | grep foo");
}

void testPipeChain()
{
    // Multiple pipes chain left-to-right
    auto out = runXell(R"(
        result = "cat file.txt" |> "sort" |> "uniq" |> "head -10"
        print(result)
    )");
    XASSERT_EQ(out.size(), 1u);
    XASSERT_EQ(out[0], "cat file.txt | sort | uniq | head -10");
}

void testPipeWithVariables()
{
    auto out = runXell(R"(
        cmd1 = "find . -name '*.cpp'"
        cmd2 = "wc -l"
        pipeline = cmd1 |> cmd2
        print(pipeline)
    )");
    XASSERT_EQ(out.size(), 1u);
    XASSERT_EQ(out[0], "find . -name '*.cpp' | wc -l");
}

void testPipeTypeError()
{
    // |> on non-strings should throw TypeError
    XASSERT(expectError<TypeError>("x = 42 |> 10"));
}

void testPipeWithRunCapture()
{
    // Build a pipeline and run it
    auto out = runXell(R"(
        pipeline = "echo hello" |> "cat"
        result = run_capture(pipeline)
        print(result["stdout"])
    )");
    XASSERT_EQ(out.size(), 1u);
    // echo hello | cat should output "hello\n"
    XASSERT(out[0].find("hello") != std::string::npos);
}

// ============================================================================
// SHELL AND (&&) TESTS
// ============================================================================

void testShellAndSuccess()
{
    // 0 (success) && run right → returns right
    auto out = runXell(R"(
        result = run("true") && run("echo done")
        print(result)
    )");
    XASSERT_EQ(out.size(), 1u);
    XASSERT_EQ(out[0], "0"); // run("echo done") returns 0
}

void testShellAndFailure()
{
    // non-0 (failure) && don't run right → returns left
    auto out = runXell(R"(
        result = run("false") && run("echo should-not-run")
        print(result)
    )");
    XASSERT_EQ(out.size(), 1u);
    XASSERT_EQ(out[0], "1"); // run("false") returns 1, right side not evaluated
}

void testShellAndWithNumbers()
{
    // 0 = success → eval right
    auto out = runXell(R"(
        result = 0 && 42
        print(result)
    )");
    XASSERT_EQ(out.size(), 1u);
    XASSERT_EQ(out[0], "42");
}

void testShellAndWithNonZero()
{
    // non-0 = failure → return left
    auto out = runXell(R"(
        result = 1 && 42
        print(result)
    )");
    XASSERT_EQ(out.size(), 1u);
    XASSERT_EQ(out[0], "1");
}

void testShellAndShortCircuit()
{
    // Verify right side is NOT evaluated on failure
    // Use a function call with side effect (print) to verify
    auto out = runXell(R"(
        fn side_effect():
            print("side-effect-ran")
            give 99
        ;
        result = 1 && side_effect()
        print(result)
    )");
    // If short-circuit works, side_effect never runs, so only result prints
    XASSERT_EQ(out.size(), 1u);
    XASSERT_EQ(out[0], "1"); // 1=fail, so left is returned
}

// ============================================================================
// SHELL OR (||) TESTS
// ============================================================================

void testShellOrSuccess()
{
    // 0 (success) || don't run right → returns left
    auto out = runXell(R"(
        result = run("true") || run("echo fallback")
        print(result)
    )");
    XASSERT_EQ(out.size(), 1u);
    XASSERT_EQ(out[0], "0"); // run("true") returns 0, right side skipped
}

void testShellOrFailure()
{
    // non-0 (failure) || run right → returns right
    auto out = runXell(R"(
        result = run("false") || run("echo recovered")
        print(result)
    )");
    XASSERT_EQ(out.size(), 1u);
    XASSERT_EQ(out[0], "0"); // run("echo recovered") returns 0
}

void testShellOrWithNumbers()
{
    // 0 = success → return left
    auto out = runXell(R"(
        result = 0 || 99
        print(result)
    )");
    XASSERT_EQ(out.size(), 1u);
    XASSERT_EQ(out[0], "0");
}

void testShellOrWithNonZero()
{
    // non-0 = failure → eval right
    auto out = runXell(R"(
        result = 1 || 99
        print(result)
    )");
    XASSERT_EQ(out.size(), 1u);
    XASSERT_EQ(out[0], "99");
}

void testShellOrShortCircuit()
{
    // Verify right side is NOT evaluated on success
    auto out = runXell(R"(
        fn side_effect():
            print("side-effect-ran")
            give 99
        ;
        result = 0 || side_effect()
        print(result)
    )");
    // If short-circuit works, side_effect never runs, so only result prints
    XASSERT_EQ(out.size(), 1u);
    XASSERT_EQ(out[0], "0"); // 0=success, so left is returned
}

// ============================================================================
// CHAINING COMBINATIONS
// ============================================================================

void testAndOrChain()
{
    // cmd1 && cmd2 || cmd3
    // true && true || ... → returns cmd2 result (0)
    auto out = runXell(R"(
        result = run("true") && run("true") || run("echo fallback")
        print(result)
    )");
    XASSERT_EQ(out.size(), 1u);
    XASSERT_EQ(out[0], "0");
}

void testOrAndChain()
{
    // && has higher precedence than ||
    // run("false") || run("true") && run("echo yes")
    // = run("false") || (run("true") && run("echo yes"))
    // = 1 || (0 && 0) = 1 || 0 = 1 (fail → eval right) = 0
    auto out = runXell(R"(
        result = run("false") || run("true") && run("echo yes")
        print(result)
    )");
    XASSERT_EQ(out.size(), 1u);
    XASSERT_EQ(out[0], "0");
}

// ============================================================================
// COMMAND GROUPING WITH PARENTHESES
// ============================================================================

void testGroupedAndOr()
{
    // (cmd1 && cmd2) || cmd3
    // (false && ...) || echo recovered
    // (1) || 0 → 0
    auto out = runXell(R"(
        result = (run("false") && run("echo nope")) || run("echo recovered")
        print(result)
    )");
    XASSERT_EQ(out.size(), 1u);
    XASSERT_EQ(out[0], "0"); // echo recovered returns 0
}

void testGroupedOrAnd()
{
    // (cmd1 || cmd2) && cmd3
    // (false || true) && echo done
    // 0 && 0 → 0
    auto out = runXell(R"(
        result = (run("false") || run("true")) && run("echo done")
        print(result)
    )");
    XASSERT_EQ(out.size(), 1u);
    XASSERT_EQ(out[0], "0");
}

void testNestedGrouping()
{
    // ((a && b) || c) && d
    auto out = runXell(R"(
        result = ((run("true") && run("true")) || run("echo nope")) && run("echo final")
        print(result)
    )");
    XASSERT_EQ(out.size(), 1u);
    XASSERT_EQ(out[0], "0");
}

// ============================================================================
// SET_E / UNSET_E / EXIT_CODE TESTS
// ============================================================================

void testExitCodeAfterSuccess()
{
    auto out = runXell(R"(
        run("true")
        print(exit_code())
    )");
    XASSERT_EQ(out.size(), 1u);
    XASSERT_EQ(out[0], "0");
}

void testExitCodeAfterFailure()
{
    auto out = runXell(R"(
        run("false")
        print(exit_code())
    )");
    XASSERT_EQ(out.size(), 1u);
    XASSERT_EQ(out[0], "1");
}

void testSetEStopsOnFailure()
{
    // set_e + run("false") should throw CommandFailedError
    XASSERT(expectError<CommandFailedError>(R"(
        set_e()
        run("false")
        print("should not reach here")
    )"));
}

void testSetEAllowsSuccess()
{
    // set_e + run("true") should work fine
    auto out = runXell(R"(
        set_e()
        run("true")
        print("ok")
    )");
    XASSERT_EQ(out.size(), 1u);
    XASSERT_EQ(out[0], "ok");
}

void testUnsetEResumesNormal()
{
    // set_e → unset_e → run("false") should NOT throw
    auto out = runXell(R"(
        set_e()
        unset_e()
        run("false")
        print("survived")
    )");
    XASSERT_EQ(out.size(), 1u);
    XASSERT_EQ(out[0], "survived");
}

void testSetEWithRunCapture()
{
    // set_e + run_capture on a failing command should throw
    XASSERT(expectError<CommandFailedError>(R"(
        set_e()
        run_capture("false")
        print("should not reach here")
    )"));
}

void testExitCodeUpdatesOnEachRun()
{
    auto out = runXell(R"(
        run("true")
        print(exit_code())
        run("false")
        print(exit_code())
        run("true")
        print(exit_code())
    )");
    XASSERT_EQ(out.size(), 3u);
    XASSERT_EQ(out[0], "0");
    XASSERT_EQ(out[1], "1");
    XASSERT_EQ(out[2], "0");
}

// ============================================================================
// PIPE + RUN INTEGRATION
// ============================================================================

void testPipeBuildAndRun()
{
    // Build a pipeline string, then execute it
    auto out = runXell(R"(
        result = run_capture("echo hello world" |> "tr 'a-z' 'A-Z'")
        print(result["stdout"])
    )");
    XASSERT_EQ(out.size(), 1u);
    XASSERT(out[0].find("HELLO WORLD") != std::string::npos);
}

void testPipeWithAndChain()
{
    // Build pipeline then chain: run(pipeline) && run(next)
    auto out = runXell(R"(
        result = run("echo hello" |> "cat") && run("echo done")
        print(result)
    )");
    XASSERT_EQ(out.size(), 1u);
    XASSERT_EQ(out[0], "0");
}

// ============================================================================
// MIXED LOGICAL AND SHELL OPERATORS
// ============================================================================

void testShellOpsVsLogicalOps()
{
    // Shell && (0=true) vs logical 'and' (truthy)
    // 0 && 5 → shell: 0 is success → eval right → 5
    // 0 and 5 → logical: 0 is falsy → return 0
    auto out = runXell(R"(
        shell_result = 0 && 5
        logic_result = 0 and 5
        print(shell_result)
        print(logic_result)
    )");
    XASSERT_EQ(out.size(), 2u);
    XASSERT_EQ(out[0], "5"); // shell: 0=success → eval right
    XASSERT_EQ(out[1], "0"); // logical: 0=falsy → return left
}

void testShellOrVsLogicalOr()
{
    // 0 || 5 → shell: 0 is success → return left (0)
    // 0 or 5 → logical: 0 is falsy → eval right (5)
    auto out = runXell(R"(
        shell_result = 0 || 5
        logic_result = 0 or 5
        print(shell_result)
        print(logic_result)
    )");
    XASSERT_EQ(out.size(), 2u);
    XASSERT_EQ(out[0], "0"); // shell: 0=success → return left
    XASSERT_EQ(out[1], "5"); // logical: 0=falsy → eval right
}

// ============================================================================
// WITH MAP RESULTS (run_capture returns {exit_code, stdout, stderr})
// ============================================================================

void testShellAndWithMapResult()
{
    // run_capture returns a map with exit_code → && checks it
    auto out = runXell(R"(
        result = run_capture("true") && run_capture("echo hello")
        print(result["stdout"])
    )");
    XASSERT_EQ(out.size(), 1u);
    XASSERT(out[0].find("hello") != std::string::npos);
}

void testShellOrWithMapResult()
{
    // run_capture("false") returns map with exit_code=1 → || evals right
    auto out = runXell(R"(
        result = run_capture("false") || run_capture("echo fallback")
        print(result["stdout"])
    )");
    XASSERT_EQ(out.size(), 1u);
    XASSERT(out[0].find("fallback") != std::string::npos);
}

// ============================================================================
// PRECEDENCE TESTS
// ============================================================================

void testPipeHigherThanAnd()
{
    // "a" | "b" && "c" | "d"
    // Should parse as: ("a" | "b") && ("c" | "d")
    // since | binds tighter than &&
    auto out = runXell(R"(
        result = 0 && ("ls" |> "grep foo")
        print(result)
    )");
    XASSERT_EQ(out.size(), 1u);
    XASSERT_EQ(out[0], "ls | grep foo");
}

void testAndHigherThanOr()
{
    // a || b && c  →  a || (b && c)
    // 1 || 0 && 42  →  1 || (0 && 42)  →  1 || 42  →  42 (1=fail so eval right)
    auto out = runXell(R"(
        result = 1 || 0 && 42
        print(result)
    )");
    XASSERT_EQ(out.size(), 1u);
    XASSERT_EQ(out[0], "42");
}

void testAllOpsPrecedence()
{
    // || < && < | < or < and  (shell ops have lowest precedence)
    // This ensures logical operators still work in their traditional context
    auto out = runXell(R"(
        a = true and true
        b = false or true
        print(a)
        print(b)
    )");
    XASSERT_EQ(out.size(), 2u);
    XASSERT_EQ(out[0], "true");
    XASSERT_EQ(out[1], "true");
}

// ============================================================================
// LEXER TOKEN TESTS
// ============================================================================

void testLexerPipeToken()
{
    Lexer lexer("a |> b");
    auto tokens = lexer.tokenize();
    XASSERT(tokens.size() >= 3);
    XASSERT(tokens[1].type == TokenType::PIPE);
    XASSERT_EQ(tokens[1].value, "|>");
}

void testLexerAmpAmpToken()
{
    Lexer lexer("a && b");
    auto tokens = lexer.tokenize();
    XASSERT(tokens.size() >= 3);
    XASSERT(tokens[1].type == TokenType::AMP_AMP);
    XASSERT_EQ(tokens[1].value, "&&");
}

void testLexerPipePipeToken()
{
    Lexer lexer("a || b");
    auto tokens = lexer.tokenize();
    XASSERT(tokens.size() >= 3);
    XASSERT(tokens[1].type == TokenType::PIPE_PIPE);
    XASSERT_EQ(tokens[1].value, "||");
}

void testLexerSingleAmpToken()
{
    // Single & is now the bitwise AND token (AMP)
    Lexer lexer("a & b");
    auto tokens = lexer.tokenize();
    XASSERT(tokens.size() >= 3);
    XASSERT(tokens[1].type == TokenType::AMP);
    XASSERT_EQ(tokens[1].value, "&");
}

// ============================================================================
// PHASE 1: SHELL BLOCK LEXER TESTS
// ============================================================================

void testLexerShellBlockPlain()
{
    Lexer lexer("shell { echo hello }");
    auto tokens = lexer.tokenize();
    XASSERT(tokens.size() >= 2);
    XASSERT(tokens[0].type == TokenType::SHELL);
    XASSERT(tokens[1].type == TokenType::RAW_SHELL_BLOCK);
    XASSERT_EQ(tokens[1].line, 1);
    XASSERT_EQ(tokens[1].segments.size(), 1u);
    XASSERT(!tokens[1].segments[0].isExpr);
    XASSERT_EQ(tokens[1].segments[0].text, " echo hello ");
}

void testLexerShellBlockCaptureNoArgs()
{
    Lexer lexer("shell->capture { echo hello }");
    auto tokens = lexer.tokenize();
    XASSERT(tokens.size() >= 4);
    XASSERT(tokens[0].type == TokenType::SHELL);
    XASSERT(tokens[1].type == TokenType::ARROW);
    XASSERT(tokens[2].type == TokenType::IDENTIFIER);
    XASSERT_EQ(tokens[2].value, "capture");
    XASSERT(tokens[3].type == TokenType::RAW_SHELL_BLOCK);
}

void testLexerShellBlockCaptureWithArgs()
{
    Lexer lexer("shell->capture(timeout: 5, cwd: \"/tmp\") { ls }");
    auto tokens = lexer.tokenize();
    XASSERT(tokens.size() >= 10);
    XASSERT(tokens[0].type == TokenType::SHELL);
    XASSERT(tokens[1].type == TokenType::ARROW);
    XASSERT(tokens[2].type == TokenType::IDENTIFIER);
    XASSERT_EQ(tokens[2].value, "capture");
    XASSERT(tokens[3].type == TokenType::LPAREN);
    bool foundBlock = false;
    for (const auto &tok : tokens)
    {
        if (tok.type == TokenType::RAW_SHELL_BLOCK)
        {
            foundBlock = true;
            XASSERT_EQ(tok.segments.size(), 1u);
            XASSERT_EQ(tok.segments[0].text, " ls ");
        }
    }
    XASSERT(foundBlock);
}

void testLexerShellBlockPlainWithArgs()
{
    Lexer lexer("shell(check: false) { ls }");
    auto tokens = lexer.tokenize();
    XASSERT(tokens[0].type == TokenType::SHELL);
    XASSERT(tokens[1].type == TokenType::LPAREN);
    bool foundBlock = false;
    for (const auto &tok : tokens)
    {
        if (tok.type == TokenType::RAW_SHELL_BLOCK)
        {
            foundBlock = true;
            XASSERT_EQ(tok.segments.size(), 1u);
            XASSERT_EQ(tok.segments[0].text, " ls ");
        }
    }
    XASSERT(foundBlock);
}

void testLexerShellBlockVarExpansion()
{
    Lexer lexer("shell { echo ${var} }");
    auto tokens = lexer.tokenize();
    XASSERT(tokens.size() >= 2);
    XASSERT(tokens[1].type == TokenType::RAW_SHELL_BLOCK);
    XASSERT_EQ(tokens[1].segments.size(), 1u);
    XASSERT(!tokens[1].segments[0].isExpr);
    XASSERT_EQ(tokens[1].segments[0].text, " echo ${var} ");
}

void testLexerShellBlockBraceExpansion()
{
    Lexer lexer("shell { echo {a,b} }");
    auto tokens = lexer.tokenize();
    XASSERT(tokens.size() >= 2);
    XASSERT(tokens[1].type == TokenType::RAW_SHELL_BLOCK);
    XASSERT_EQ(tokens[1].segments.size(), 1u);
    XASSERT(!tokens[1].segments[0].isExpr);
    XASSERT_EQ(tokens[1].segments[0].text, " echo {a,b} ");
}

void testLexerShellBlockFunctionDef()
{
    Lexer lexer("shell { f() { echo 1; } }");
    auto tokens = lexer.tokenize();
    XASSERT(tokens.size() >= 2);
    XASSERT(tokens[1].type == TokenType::RAW_SHELL_BLOCK);
    XASSERT_EQ(tokens[1].segments.size(), 1u);
    XASSERT(!tokens[1].segments[0].isExpr);
    XASSERT_EQ(tokens[1].segments[0].text, " f() { echo 1; } ");
}

void testLexerShellBlockBracesInQuotes()
{
    Lexer lexer("shell { echo '{' and '}' and \"{\" and \"}\" }");
    auto tokens = lexer.tokenize();
    XASSERT(tokens.size() >= 2);
    XASSERT(tokens[1].type == TokenType::RAW_SHELL_BLOCK);
    XASSERT_EQ(tokens[1].segments.size(), 1u);
    XASSERT(!tokens[1].segments[0].isExpr);
    XASSERT_EQ(tokens[1].segments[0].text, " echo '{' and '}' and \"{\" and \"}\" ");
}

void testLexerShellBlockComments()
{
    Lexer lexer("shell {\n# comment with { and }\necho 1\n}");
    auto tokens = lexer.tokenize();
    XASSERT(tokens.size() >= 2);
    XASSERT(tokens[1].type == TokenType::RAW_SHELL_BLOCK);
    XASSERT_EQ(tokens[1].segments.size(), 1u);
    XASSERT(!tokens[1].segments[0].isExpr);
    XASSERT_EQ(tokens[1].segments[0].text, "\n# comment with { and }\necho 1\n");
}

void testLexerShellBlockEscapedBraces()
{
    Lexer lexer("shell { echo \\{ and \\} }");
    auto tokens = lexer.tokenize();
    XASSERT(tokens.size() >= 2);
    XASSERT(tokens[1].type == TokenType::RAW_SHELL_BLOCK);
    XASSERT_EQ(tokens[1].segments.size(), 1u);
    XASSERT(!tokens[1].segments[0].isExpr);
    XASSERT_EQ(tokens[1].segments[0].text, " echo \\{ and \\} ");
}

void testLexerShellBlockAtInSingleQuotes()
{
    Lexer lexer("shell { echo '@{literal}' }");
    auto tokens = lexer.tokenize();
    XASSERT(tokens.size() >= 2);
    XASSERT(tokens[1].type == TokenType::RAW_SHELL_BLOCK);
    XASSERT_EQ(tokens[1].segments.size(), 1u);
    XASSERT(!tokens[1].segments[0].isExpr);
    XASSERT_EQ(tokens[1].segments[0].text, " echo '@{literal}' ");
}

void testLexerShellBlockEscapedAt()
{
    Lexer lexer("shell { echo \\@{literal} }");
    auto tokens = lexer.tokenize();
    XASSERT(tokens.size() >= 2);
    XASSERT(tokens[1].type == TokenType::RAW_SHELL_BLOCK);
    XASSERT_EQ(tokens[1].segments.size(), 1u);
    XASSERT(!tokens[1].segments[0].isExpr);
    XASSERT_EQ(tokens[1].segments[0].text, " echo @{literal} ");
}

void testLexerShellBlockDictInInterpolation()
{
    Lexer lexer("shell { echo @{{\"a\": 1, \"b\": {2: 3}}} }");
    auto tokens = lexer.tokenize();
    XASSERT(tokens.size() >= 2);
    XASSERT(tokens[1].type == TokenType::RAW_SHELL_BLOCK);
    XASSERT_EQ(tokens[1].segments.size(), 3u);
    XASSERT(!tokens[1].segments[0].isExpr);
    XASSERT_EQ(tokens[1].segments[0].text, " echo ");
    XASSERT(tokens[1].segments[1].isExpr);
    XASSERT_EQ(tokens[1].segments[1].text, "{\"a\": 1, \"b\": {2: 3}}");
    XASSERT_EQ((int)tokens[1].segments[1].quoteContext, (int)ShellQuoteContext::UNQUOTED);
    XASSERT(!tokens[1].segments[2].isExpr);
    XASSERT_EQ(tokens[1].segments[2].text, " ");
}

void testLexerShellBlockUnclosedBlock()
{
    bool threw = false;
    int errLine = -1;
    try
    {
        Lexer lexer("shell {\n  echo unclosed");
        lexer.tokenize();
    }
    catch (const LexerError &e)
    {
        threw = true;
        errLine = e.line();
    }
    XASSERT(threw);
    XASSERT_EQ(errLine, 1); // Reported at the line of the opening brace
}

void testLexerShellBlockEmpty()
{
    Lexer lexer("shell {}");
    auto tokens = lexer.tokenize();
    XASSERT(tokens.size() >= 2);
    XASSERT(tokens[1].type == TokenType::RAW_SHELL_BLOCK);
    XASSERT(tokens[1].segments.empty());
}

void testLexerShellBlockInterleaved()
{
    Lexer lexer("shell { cp @{src} @{dest} }");
    auto tokens = lexer.tokenize();
    XASSERT(tokens.size() >= 2);
    XASSERT(tokens[1].type == TokenType::RAW_SHELL_BLOCK);
    XASSERT_EQ(tokens[1].segments.size(), 5u);
    XASSERT(!tokens[1].segments[0].isExpr);
    XASSERT_EQ(tokens[1].segments[0].text, " cp ");
    XASSERT(tokens[1].segments[1].isExpr);
    XASSERT_EQ(tokens[1].segments[1].text, "src");
    XASSERT_EQ((int)tokens[1].segments[1].quoteContext, (int)ShellQuoteContext::UNQUOTED);
    XASSERT(!tokens[1].segments[2].isExpr);
    XASSERT_EQ(tokens[1].segments[2].text, " ");
    XASSERT(tokens[1].segments[3].isExpr);
    XASSERT_EQ(tokens[1].segments[3].text, "dest");
    XASSERT(!tokens[1].segments[4].isExpr);
    XASSERT_EQ(tokens[1].segments[4].text, " ");
}

void testLexerShellBlockDoubleQuoteInterpolation()
{
    Lexer lexer("shell { echo \"hello @{name}!\" }");
    auto tokens = lexer.tokenize();
    XASSERT(tokens.size() >= 2);
    XASSERT(tokens[1].type == TokenType::RAW_SHELL_BLOCK);
    XASSERT_EQ(tokens[1].segments.size(), 3u);
    XASSERT(!tokens[1].segments[0].isExpr);
    XASSERT_EQ(tokens[1].segments[0].text, " echo \"hello ");
    XASSERT(tokens[1].segments[1].isExpr);
    XASSERT_EQ(tokens[1].segments[1].text, "name");
    XASSERT_EQ((int)tokens[1].segments[1].quoteContext, (int)ShellQuoteContext::DOUBLE_QUOTES);
    XASSERT(!tokens[1].segments[2].isExpr);
    XASSERT_EQ(tokens[1].segments[2].text, "!\" ");
}

void testLexerShellBlockCommandSubstRejection()
{
    bool threw = false;
    try
    {
        Lexer lexer("shell { echo \"$(echo @{x})\" }");
        lexer.tokenize();
    }
    catch (const LexerError &e)
    {
        threw = true;
        std::string msg = e.what();
        XASSERT(msg.find("command substitution") != std::string::npos);
    }
    XASSERT(threw);
}

// ============================================================================
// MAIN
// ============================================================================

int main()
{
    std::cout << "============================================\n";
    std::cout << "  Shell Chaining Tests\n";
    std::cout << "============================================\n";

    // Pipe operator
    runTest("pipe: basic string concat", testPipeBasic);
    runTest("pipe: multi-stage chain", testPipeChain);
    runTest("pipe: with variables", testPipeWithVariables);
    runTest("pipe: type error on non-strings", testPipeTypeError);
    runTest("pipe: build and run_capture", testPipeWithRunCapture);

    // Shell AND
    runTest("&&: success runs right", testShellAndSuccess);
    runTest("&&: failure skips right", testShellAndFailure);
    runTest("&&: with numbers (0=success)", testShellAndWithNumbers);
    runTest("&&: with non-zero (fail)", testShellAndWithNonZero);
    runTest("&&: short-circuit on failure", testShellAndShortCircuit);

    // Shell OR
    runTest("||: success skips right", testShellOrSuccess);
    runTest("||: failure runs right", testShellOrFailure);
    runTest("||: with numbers (0=success)", testShellOrWithNumbers);
    runTest("||: with non-zero (fail)", testShellOrWithNonZero);
    runTest("||: short-circuit on success", testShellOrShortCircuit);

    // Chaining combinations
    runTest("&&/||: and-then-or chain", testAndOrChain);
    runTest("&&/||: or-then-and chain", testOrAndChain);

    // Grouping
    runTest("grouping: (a && b) || c", testGroupedAndOr);
    runTest("grouping: (a || b) && c", testGroupedOrAnd);
    runTest("grouping: nested ((a && b) || c) && d", testNestedGrouping);

    // set_e / unset_e / exit_code
    runTest("exit_code: after success", testExitCodeAfterSuccess);
    runTest("exit_code: after failure", testExitCodeAfterFailure);
    runTest("set_e: stops on failure", testSetEStopsOnFailure);
    runTest("set_e: allows success", testSetEAllowsSuccess);
    runTest("unset_e: resumes normal", testUnsetEResumesNormal);
    runTest("set_e: with run_capture", testSetEWithRunCapture);
    runTest("exit_code: updates on each run", testExitCodeUpdatesOnEachRun);

    // Integration
    runTest("pipe+run: build and execute pipeline", testPipeBuildAndRun);
    runTest("pipe+&&: pipeline then chain", testPipeWithAndChain);

    // Shell vs logical
    runTest("shell && vs logical and", testShellOpsVsLogicalOps);
    runTest("shell || vs logical or", testShellOrVsLogicalOr);

    // With map results
    runTest("&&: with run_capture map", testShellAndWithMapResult);
    runTest("||: with run_capture map", testShellOrWithMapResult);

    // Precedence
    runTest("precedence: | higher than &&", testPipeHigherThanAnd);
    runTest("precedence: && higher than ||", testAndHigherThanOr);
    runTest("precedence: logical and/or still work", testAllOpsPrecedence);

    // Lexer
    runTest("lexer: | token", testLexerPipeToken);
    runTest("lexer: && token", testLexerAmpAmpToken);
    runTest("lexer: || token", testLexerPipePipeToken);
    runTest("lexer: single & is AMP token", testLexerSingleAmpToken);

    // Shell block lexer (Phase 1)
    runTest("shell block lexer: plain block", testLexerShellBlockPlain);
    runTest("shell block lexer: capture no args", testLexerShellBlockCaptureNoArgs);
    runTest("shell block lexer: capture with args", testLexerShellBlockCaptureWithArgs);
    runTest("shell block lexer: plain with args", testLexerShellBlockPlainWithArgs);
    runTest("shell block lexer: variable expansion ${var}", testLexerShellBlockVarExpansion);
    runTest("shell block lexer: brace expansion {a,b}", testLexerShellBlockBraceExpansion);
    runTest("shell block lexer: function definition f() { ... }", testLexerShellBlockFunctionDef);
    runTest("shell block lexer: braces in quotes", testLexerShellBlockBracesInQuotes);
    runTest("shell block lexer: comments with braces", testLexerShellBlockComments);
    runTest("shell block lexer: escaped braces \\{ and \\}", testLexerShellBlockEscapedBraces);
    runTest("shell block lexer: @{ in single quotes is literal", testLexerShellBlockAtInSingleQuotes);
    runTest("shell block lexer: \\@{ is literal", testLexerShellBlockEscapedAt);
    runTest("shell block lexer: dict literal in interpolation", testLexerShellBlockDictInInterpolation);
    runTest("shell block lexer: unclosed block error line", testLexerShellBlockUnclosedBlock);
    runTest("shell block lexer: empty block", testLexerShellBlockEmpty);
    runTest("shell block lexer: interleaved segments", testLexerShellBlockInterleaved);
    runTest("shell block lexer: double quote interpolation", testLexerShellBlockDoubleQuoteInterpolation);
    runTest("shell block lexer: command substitution rejection", testLexerShellBlockCommandSubstRejection);

    std::cout << "\n============================================\n";
    std::cout << "  Total: " << (g_passed + g_failed)
              << "  |  Passed: " << g_passed
              << "  |  Failed: " << g_failed << "\n";
    std::cout << "============================================\n";

    return g_failed > 0 ? 1 : 0;
}
