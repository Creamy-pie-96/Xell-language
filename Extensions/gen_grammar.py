#!/usr/bin/env python3
"""
gen_grammar.py — Dynamic grammar generator for ScriptIt
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Single source of truth: the C++ parser files.

Reads:
  - scriptit_types.hpp   → keywords, operators, math functions, builtin list
  - scriptit_builtins.hpp → builtin free-function names (from get_builtins())
  - scriptit_methods.hpp  → dot-method names per type (unused for grammar, kept for docs)

Generates:
  - syntaxes/scriptit.tmLanguage.json   (TextMate grammar)
  - updates configurationDefaults.editor.tokenColorCustomizations in package.json
  - generates color_customizer/token_data.json  (for the dynamic customizer)

Usage:
    python3 scripts/gen_grammar.py
    python3 scripts/gen_grammar.py --check   # verify grammar is up-to-date (CI)
"""

import re
import json
import sys
import os
from pathlib import Path
from collections import OrderedDict

# ─── Paths ────────────────────────────────────────────────

SCRIPT_DIR = Path(__file__).resolve().parent
ROOT = SCRIPT_DIR.parent
REPL_DIR = ROOT / "include" / "pythonic" / "REPL"
TYPES_HPP = REPL_DIR / "scriptit_types.hpp"
BUILTINS_HPP = REPL_DIR / "scriptit_builtins.hpp"
METHODS_HPP = REPL_DIR / "scriptit_methods.hpp"

VSCODE_DIR = REPL_DIR / "scriptit-vscode"
TMLANG_OUT = VSCODE_DIR / "syntaxes" / "scriptit.tmLanguage.json"
PACKAGE_JSON = VSCODE_DIR / "package.json"
TOKEN_DATA_OUT = VSCODE_DIR / "color_customizer" / "token_data.json"


# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# 1. EXTRACT DATA FROM C++ HEADERS
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

def read_file(path):
    with open(path, "r") as f:
        return f.read()


def extract_keywords(src):
    """Extract keywords from the keyword map in scriptit_types.hpp.
    Looks for the keywords enum/map — specifically the KeywordXxx entries."""
    keywords = []
    # Match KeywordXxx entries in the TokenType enum
    for m in re.finditer(r'Keyword(\w+)', src):
        kw_name = m.group(1)
        # Convert PascalCase to lowercase: KeywordVar -> var, KeywordElif -> elif
        # The actual keyword string is the lowercase version
        kw_lower = kw_name[0].lower() + kw_name[1:]
        # Fix: some are camelCase in enum but lowercase in the language
        # Actually they're just PascalCase of the keyword: Var->var, Fn->fn, etc.
        kw_lower = re.sub(r'([A-Z])', lambda m: '_' + m.group(1).lower(), kw_name).strip('_').replace('__', '_')
        # Simpler: just lowercase them
        kw_lower = kw_name.lower()
        keywords.append(kw_lower)
    return sorted(set(keywords))


def extract_math_functions(src):
    """Extract math function names from is_math_function()."""
    match = re.search(r'is_math_function.*?funcs\s*=\s*\{([^}]+)\}', src, re.DOTALL)
    if not match:
        return []
    block = match.group(1)
    return sorted(set(re.findall(r'"(\w+)"', block)))


def extract_builtin_functions(src):
    """Extract builtin function names from is_builtin_function()."""
    match = re.search(r'is_builtin_function.*?funcs\s*=\s*\{([^}]+)\}', src, re.DOTALL)
    if not match:
        return []
    block = match.group(1)
    return sorted(set(re.findall(r'"(\w+)"', block)))


def extract_operators(src):
    """Extract operators from get_operator_precedence().
    The precedence map has nested braces like {{"||", 1}, {"&&", 2}, ...}
    so we need to match the full outer brace pair."""
    # Find the line(s) containing the precedence map
    match = re.search(r'get_operator_precedence.*?precedence\s*=\s*\{(.*?)\};', src, re.DOTALL)
    if not match:
        return []
    block = match.group(1)
    # Extract the first element of each {key, value} pair — these are the operators
    ops = []
    for m in re.finditer(r'\{\s*"([^"]+)"\s*,\s*\d+\s*\}', block):
        ops.append(m.group(1))
    return ops


def extract_builtins_from_map(src):
    """Extract builtin names from the get_builtins() function in scriptit_builtins.hpp.
    Matches {"name", [](std::stack<var>...) entries in the map (not JSON strings in comments)."""
    match = re.search(r'get_builtins\(\).*?builtins\s*=\s*\{(.*?)\};\s*return', src, re.DOTALL)
    if not match:
        return []
    block = match.group(1)
    names = []
    # Match builtin entries: {"name", [](std::stack<var>...
    for m in re.finditer(r'\{\s*"(\w+)"\s*,\s*\[\]', block):
        names.append(m.group(1))
    return sorted(set(names))


# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# 2. CLASSIFY EXTRACTED DATA
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

# These classifications are stable (they map to TextMate scopes).
# If you add a keyword to the parser, it'll auto-appear; you only need
# to add a NEW classification if you invent a new *kind* of token.

CONTROL_KEYWORDS = {"if", "elif", "else", "for", "while", "break", "pass", "return", "switch", "case", "default"}
LOOP_KEYWORDS = {"in", "range", "from", "to", "step"}
SPECIAL_KEYWORDS = {"let", "be", "new", "are", "of"}
CONTEXT_KEYWORDS = {"with", "as"}
LOGICAL_WORD_OPS = {"and", "or", "not", "is", "points"}

# Type conversion functions (anything that's both a builtin AND a type name)
TYPE_FUNCTIONS = {
    "str", "int", "float", "double", "long", "long_long", "long_double",
    "uint", "ulong", "ulong_long", "auto_numeric", "bool",
    "list", "set", "dict", "graph", "range_list"
}


def classify_builtins(all_builtins, math_fns):
    """Split builtins into: pure builtins (for highlighting) vs math vs type-conv."""
    pure_builtins = []
    for b in all_builtins:
        if b in TYPE_FUNCTIONS:
            continue
        if b in math_fns:
            continue
        pure_builtins.append(b)
    return sorted(set(pure_builtins))


def classify_operators(ops):
    """Split operators into categories for TextMate scopes."""
    edge_ops = []
    comparison_ops = []
    logical_symbol_ops = []
    for op in ops:
        if op in ("->", "<->", "---"):
            edge_ops.append(op)
        elif op in ("==", "!=", "<=", ">=", "<", ">"):
            comparison_ops.append(op)
        elif op in ("&&", "||"):
            logical_symbol_ops.append(op)
        # Others (arithmetic, assignment, etc.) are handled by regex patterns
    return edge_ops, comparison_ops, logical_symbol_ops


# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# 3. GENERATE TMLANGUAGE JSON
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

def escape_re(s):
    """Escape a string for use in a TextMate regex (JSON-encoded later)."""
    return s.replace("\\", "\\\\").replace("|", "\\|").replace(".", "\\.").replace("+", "\\+").replace("*", "\\*").replace("?", "\\?").replace("(", "\\(").replace(")", "\\)").replace("[", "\\[").replace("]", "\\]").replace("{", "\\{").replace("}", "\\}").replace("^", "\\^").replace("$", "\\$")


def word_alt(words):
    """Build a \\b(word1|word2|...)\\b alternation."""
    return "\\b(" + "|".join(sorted(words, key=lambda w: (-len(w), w))) + ")\\b"


def build_tmlanguage(keywords, math_fns, builtins, type_fns, edge_ops, comp_ops, logical_sym_ops):
    """Build the complete tmLanguage JSON structure."""

    # Classify keywords into their groups
    control_kws = sorted(k for k in CONTROL_KEYWORDS)
    loop_kws = sorted(k for k in LOOP_KEYWORDS)
    special_kws = sorted(k for k in SPECIAL_KEYWORDS)
    context_kws = sorted(k for k in CONTEXT_KEYWORDS)
    logical_kws = sorted(k for k in LOGICAL_WORD_OPS)

    # Pure builtins (not type functions, not math)
    pure_builtins = classify_builtins(builtins, math_fns)

    grammar = OrderedDict()
    grammar["$schema"] = "https://raw.githubusercontent.com/martinring/tmlanguage/master/tmlanguage.json"
    grammar["name"] = "ScriptIt"
    grammar["scopeName"] = "source.scriptit"
    grammar["fileTypes"] = ["si", "sit"]

    # --- Auto-generated notice ---
    grammar["comment"] = (
        "AUTO-GENERATED by gen_grammar.py — DO NOT EDIT MANUALLY. "
        "Change the parser (scriptit_types.hpp, scriptit_builtins.hpp) "
        "and re-run: python3 scripts/gen_grammar.py"
    )

    # Top-level patterns (order matters for precedence)
    grammar["patterns"] = [
        {"include": "#block-comment"},
        {"include": "#line-comment"},
        {"include": "#strings"},
        {"include": "#function-definition"},
        {"include": "#forward-declaration"},
        {"include": "#var-declaration"},
        {"include": "#let-declaration"},
        {"include": "#new-declaration"},
        {"include": "#for-loop"},
        {"include": "#for-in-loop"},
        {"include": "#with-as"},
        {"include": "#special-keywords"},
        {"include": "#control-keywords"},
        {"include": "#loop-keywords"},
        {"include": "#context-keywords"},
        {"include": "#logical-operators"},
        {"include": "#var-keyword"},
        {"include": "#boolean-constants"},
        {"include": "#none-constant"},
        {"include": "#builtin-functions"},
        {"include": "#math-functions"},
        {"include": "#type-functions"},
        {"include": "#edge-operators"},
        {"include": "#comparison-operators"},
        {"include": "#compound-assignment"},
        {"include": "#increment-operators"},
        {"include": "#assignment-operator"},
        {"include": "#arithmetic-operators"},
        {"include": "#logical-symbol-operators"},
        {"include": "#numbers"},
        {"include": "#method-call"},
        {"include": "#function-call"},
        {"include": "#ref-parameter"},
        {"include": "#semicolon-terminator"},
        {"include": "#dot-terminator"},
        {"include": "#punctuation"},
        {"include": "#identifiers"},
    ]

    # Repository of patterns
    repo = OrderedDict()

    # --- Comments ---
    repo["block-comment"] = {
        "name": "comment.block.arrow.scriptit",
        "begin": "-->",
        "end": "<--",
        "beginCaptures": {"0": {"name": "punctuation.definition.comment.begin.scriptit"}},
        "endCaptures": {"0": {"name": "punctuation.definition.comment.end.scriptit"}},
    }
    repo["line-comment"] = {
        "name": "comment.line.number-sign.scriptit",
        "match": "#.*$",
    }

    # --- Strings ---
    repo["strings"] = {
        "patterns": [
            {
                "name": "string.quoted.double.scriptit",
                "begin": "\"",
                "end": "\"",
                "patterns": [{"name": "constant.character.escape.scriptit", "match": "\\\\."}],
            },
            {
                "name": "string.quoted.single.scriptit",
                "begin": "'",
                "end": "'",
                "patterns": [{"name": "constant.character.escape.scriptit", "match": "\\\\."}],
            },
        ]
    }

    # --- Function definition ---
    repo["function-definition"] = {
        "comment": "fn name(param1, @param2):  — captures fn, name, each param",
        "patterns": [{
            "name": "meta.function.definition.scriptit",
            "begin": "\\b(fn)\\s+([a-zA-Z_][a-zA-Z0-9_]*)\\s*\\(",
            "beginCaptures": {
                "1": {"name": "keyword.declaration.function.scriptit"},
                "2": {"name": "entity.name.function.definition.scriptit"},
            },
            "end": "\\)\\s*(:|\\.)",
            "endCaptures": {"1": {"name": "punctuation.definition.function.scriptit"}},
            "patterns": [
                {
                    "comment": "Reference parameter with @",
                    "match": "(@)([a-zA-Z_][a-zA-Z0-9_]*)",
                    "captures": {
                        "1": {"name": "storage.modifier.reference.scriptit"},
                        "2": {"name": "variable.parameter.reference.scriptit"},
                    },
                },
                {
                    "comment": "Regular parameter",
                    "name": "variable.parameter.scriptit",
                    "match": "\\b([a-zA-Z_][a-zA-Z0-9_]*)\\b",
                },
                {"name": "punctuation.separator.parameter.scriptit", "match": ","},
            ],
        }],
    }

    # --- Forward declaration ---
    repo["forward-declaration"] = {
        "comment": "fn name(params). — forward declaration ending with dot",
        "patterns": [{
            "name": "meta.forward-declaration.scriptit",
            "match": "\\b(fn)\\s+([a-zA-Z_][a-zA-Z0-9_]*)\\s*\\([^)]*\\)\\s*\\.",
            "captures": {
                "1": {"name": "keyword.declaration.function.scriptit"},
                "2": {"name": "entity.name.function.forward.scriptit"},
            },
        }],
    }

    # --- var declaration ---
    repo["var-declaration"] = {
        "comment": "var x = ... OR var x, y, z = ...",
        "patterns": [{
            "name": "meta.var.declaration.scriptit",
            "begin": "\\b(var)\\s+",
            "beginCaptures": {"1": {"name": "storage.type.var.scriptit"}},
            "end": "(?==)|(?=\\.\\s*$)|(?=\\.\\s*#)|$",
            "patterns": [
                {"name": "variable.other.declaration.scriptit", "match": "\\b([a-zA-Z_][a-zA-Z0-9_]*)\\b"},
                {"name": "punctuation.separator.comma.scriptit", "match": ","},
            ],
        }],
    }

    # --- let declaration ---
    repo["let-declaration"] = {
        "comment": "let x be ...",
        "patterns": [{
            "match": "\\b(let)\\s+([a-zA-Z_][a-zA-Z0-9_]*)\\s+(be)\\b",
            "captures": {
                "1": {"name": "keyword.other.special.scriptit"},
                "2": {"name": "variable.other.declaration.scriptit"},
                "3": {"name": "keyword.other.special.scriptit"},
            },
        }],
    }

    # --- new declaration ---
    repo["new-declaration"] = {
        "comment": "new x are [...]",
        "patterns": [{
            "match": "\\b(new)\\s+([a-zA-Z_][a-zA-Z0-9_]*)\\s+(are)\\b",
            "captures": {
                "1": {"name": "keyword.other.special.scriptit"},
                "2": {"name": "variable.other.declaration.scriptit"},
                "3": {"name": "keyword.other.special.scriptit"},
            },
        }],
    }

    # --- for loop ---
    repo["for-loop"] = {
        "comment": "for i in range(...)",
        "patterns": [{
            "match": "\\b(for)\\s+([a-zA-Z_][a-zA-Z0-9_]*)\\s+(in)\\s+(range)\\b",
            "captures": {
                "1": {"name": "keyword.control.flow.scriptit"},
                "2": {"name": "variable.other.loop.scriptit"},
                "3": {"name": "keyword.control.loop.scriptit"},
                "4": {"name": "keyword.control.loop.scriptit"},
            },
        }],
    }

    # --- for-in loop ---
    repo["for-in-loop"] = {
        "comment": "for item in collection",
        "patterns": [{
            "match": "\\b(for)\\s+([a-zA-Z_][a-zA-Z0-9_]*)\\s+(in)\\b",
            "captures": {
                "1": {"name": "keyword.control.flow.scriptit"},
                "2": {"name": "variable.other.loop.scriptit"},
                "3": {"name": "keyword.control.loop.scriptit"},
            },
        }],
    }

    # --- with-as ---
    repo["with-as"] = {
        "comment": "with open(...) as handle:",
        "patterns": [{
            "match": "\\b(with)\\b(.+?)\\b(as)\\s+([a-zA-Z_][a-zA-Z0-9_]*)\\s*:",
            "captures": {
                "1": {"name": "keyword.control.context.scriptit"},
                "3": {"name": "keyword.control.context.scriptit"},
                "4": {"name": "variable.other.declaration.scriptit"},
            },
        }],
    }

    # --- Dynamic keyword groups (from parser) ---
    repo["special-keywords"] = {
        "comment": f"fn, give, {', '.join(special_kws)} — auto-generated from parser",
        "patterns": [
            {"name": "keyword.declaration.function.scriptit", "match": "\\b(fn)\\b"},
            {"name": "keyword.control.return.scriptit", "match": "\\b(give)\\b"},
            {"name": "keyword.other.special.scriptit", "match": word_alt(special_kws)},
        ],
    }

    repo["control-keywords"] = {
        "comment": f"Auto-generated: {', '.join(control_kws)}",
        "patterns": [{"name": "keyword.control.flow.scriptit", "match": word_alt(control_kws)}],
    }

    repo["loop-keywords"] = {
        "comment": f"Auto-generated: {', '.join(loop_kws)}",
        "patterns": [{"name": "keyword.control.loop.scriptit", "match": word_alt(loop_kws)}],
    }

    repo["context-keywords"] = {
        "comment": f"Auto-generated: {', '.join(context_kws)}",
        "patterns": [{"name": "keyword.control.context.scriptit", "match": word_alt(context_kws)}],
    }

    repo["logical-operators"] = {
        "comment": f"Auto-generated: {', '.join(logical_kws)}",
        "patterns": [{"name": "keyword.operator.logical.scriptit", "match": word_alt(logical_kws)}],
    }

    repo["var-keyword"] = {
        "comment": "Standalone var keyword",
        "patterns": [{"name": "storage.type.var.scriptit", "match": "\\b(var)\\b"}],
    }

    # --- Boolean / None constants ---
    repo["boolean-constants"] = {
        "patterns": [
            {"name": "constant.language.boolean.true.scriptit", "match": "\\bTrue\\b"},
            {"name": "constant.language.boolean.false.scriptit", "match": "\\bFalse\\b"},
        ]
    }
    repo["none-constant"] = {
        "patterns": [{"name": "constant.language.none.scriptit", "match": "\\bNone\\b"}],
    }

    # --- Dynamic function groups (from parser) ---
    repo["builtin-functions"] = {
        "comment": f"Auto-generated: {len(pure_builtins)} builtin functions",
        "patterns": [{
            "name": "support.function.builtin.scriptit",
            "match": "\\b(" + "|".join(sorted(pure_builtins, key=lambda w: (-len(w), w))) + ")\\b(?=\\s*\\()",
        }],
    }

    repo["math-functions"] = {
        "comment": f"Auto-generated: {len(math_fns)} math functions",
        "patterns": [{
            "name": "support.function.math.scriptit",
            "match": "\\b(" + "|".join(sorted(math_fns, key=lambda w: (-len(w), w))) + ")\\b(?=\\s*\\()",
        }],
    }

    repo["type-functions"] = {
        "comment": f"Auto-generated: {len(type_fns)} type conversion functions",
        "patterns": [{
            "name": "support.type.conversion.scriptit",
            "match": "\\b(" + "|".join(sorted(type_fns, key=lambda w: (-len(w), w))) + ")\\b(?=\\s*\\()",
        }],
    }

    # --- Operators (from parser) ---
    # Edge operators need careful ordering: <-> before ->, --- before --
    repo["edge-operators"] = {
        "comment": f"Auto-generated edge/arrow operators: {', '.join(edge_ops)}",
        "patterns": [],
    }
    # Order: longest first to avoid partial matches
    for op in sorted(edge_ops, key=lambda x: -len(x)):
        if op == "->":
            # Don't match --> (comment start)
            repo["edge-operators"]["patterns"].append(
                {"name": "keyword.operator.edge.scriptit", "match": "->(?!-)"}
            )
        elif op == "<->":
            repo["edge-operators"]["patterns"].append(
                {"name": "keyword.operator.edge.scriptit", "match": "<->"}
            )
        elif op == "---":
            repo["edge-operators"]["patterns"].append(
                {"name": "keyword.operator.edge.scriptit", "match": "---"}
            )
        else:
            repo["edge-operators"]["patterns"].append(
                {"name": "keyword.operator.edge.scriptit", "match": op}
            )

    repo["comparison-operators"] = {
        "patterns": [{"name": "keyword.operator.comparison.scriptit", "match": "==|!=|<=|>=|<|>"}],
    }

    repo["compound-assignment"] = {
        "patterns": [{"name": "keyword.operator.assignment.compound.scriptit", "match": "\\+=|-=|\\*=|/=|%="}],
    }

    repo["increment-operators"] = {
        "patterns": [{"name": "keyword.operator.increment.scriptit", "match": "\\+\\+|--"}],
    }

    repo["assignment-operator"] = {
        "patterns": [{"name": "keyword.operator.assignment.scriptit", "match": "="}],
    }

    repo["arithmetic-operators"] = {
        "patterns": [{"name": "keyword.operator.arithmetic.scriptit", "match": "\\+|-|\\*|/|%|\\^"}],
    }

    repo["logical-symbol-operators"] = {
        "patterns": [{"name": "keyword.operator.logical.symbol.scriptit", "match": "&&|\\|\\|"}],
    }

    # --- Numbers ---
    repo["numbers"] = {
        "patterns": [
            {"name": "constant.numeric.float.scriptit", "match": "(?<![a-zA-Z_\\.])\\d+\\.\\d+"},
            {"name": "constant.numeric.integer.scriptit", "match": "(?<![a-zA-Z_\\.])\\d+(?!\\.\\d)"},
        ]
    }

    # --- Method call ---
    repo["method-call"] = {
        "comment": ".name( — method call",
        "patterns": [{
            "match": "\\.([a-zA-Z_][a-zA-Z0-9_]*)\\s*(?=\\()",
            "captures": {"1": {"name": "entity.name.function.method.scriptit"}},
        }],
    }

    # --- Function call ---
    repo["function-call"] = {
        "comment": "name( — function call",
        "patterns": [{
            "match": "\\b([a-zA-Z_][a-zA-Z0-9_]*)\\s*(?=\\()",
            "captures": {"1": {"name": "entity.name.function.call.scriptit"}},
        }],
    }

    # --- Ref parameter ---
    repo["ref-parameter"] = {
        "comment": "@variable — pass-by-reference",
        "patterns": [{
            "match": "(@)([a-zA-Z_][a-zA-Z0-9_]*)",
            "captures": {
                "1": {"name": "storage.modifier.reference.scriptit"},
                "2": {"name": "variable.other.reference.scriptit"},
            },
        }],
    }

    # --- Terminators ---
    repo["semicolon-terminator"] = {
        "patterns": [{"name": "punctuation.terminator.block.scriptit", "match": "^\\s*;\\s*$"}],
    }
    repo["dot-terminator"] = {
        "patterns": [{"name": "punctuation.terminator.statement.scriptit", "match": "\\.(?=\\s*$|\\s*#)"}],
    }

    # --- Punctuation ---
    repo["punctuation"] = {
        "patterns": [
            {"name": "punctuation.bracket.round.scriptit", "match": "[()]"},
            {"name": "punctuation.bracket.square.scriptit", "match": "[\\[\\]]"},
            {"name": "punctuation.bracket.curly.scriptit", "match": "[{}]"},
            {"name": "punctuation.separator.comma.scriptit", "match": ","},
            {"name": "punctuation.separator.colon.scriptit", "match": ":"},
        ]
    }

    # --- Identifiers ---
    repo["identifiers"] = {
        "comment": "Catch-all: remaining identifiers → variable color",
        "patterns": [{"name": "variable.other.scriptit", "match": "\\b[a-zA-Z_][a-zA-Z0-9_]*\\b"}],
    }

    grammar["repository"] = repo
    return grammar


# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# 4. GENERATE TOKEN DATA FOR CUSTOMIZER
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

# Default colors for each scope category
SCOPE_DEFAULTS = {
    # Keywords
    "keyword.control.flow.scriptit": {"color": "#e06c75", "bold": True, "italic": False, "label": "Control (if, else, for, while, break)", "example": "if", "group": "Keywords"},
    "keyword.control.loop.scriptit": {"color": "#e06c75", "bold": True, "italic": False, "label": "Loop (in, range, from, to, step)", "example": "for", "group": "Keywords"},
    "keyword.declaration.function.scriptit": {"color": "#e5c07b", "bold": True, "italic": False, "label": "fn keyword", "example": "fn", "group": "Keywords"},
    "keyword.control.return.scriptit": {"color": "#e5c07b", "bold": True, "italic": False, "label": "give (return)", "example": "give", "group": "Keywords"},
    "keyword.other.special.scriptit": {"color": "#e5c07b", "bold": True, "italic": False, "label": "let, be, new, are, of", "example": "let", "group": "Keywords"},
    "keyword.control.context.scriptit": {"color": "#e06c75", "bold": True, "italic": False, "label": "with, as", "example": "with", "group": "Keywords"},
    "storage.type.var.scriptit": {"color": "#FFFFFF", "bold": True, "italic": False, "label": "var keyword", "example": "var", "group": "Keywords"},
    "storage.modifier.reference.scriptit": {"color": "#e5c07b", "bold": True, "italic": False, "label": "@ reference", "example": "@", "group": "Keywords"},
    # Functions
    "entity.name.function.definition.scriptit": {"color": "#00ffff", "bold": False, "italic": False, "label": "Function definition name", "example": "myFunc", "group": "Functions & Methods"},
    "entity.name.function.call.scriptit": {"color": "#00ffff", "bold": False, "italic": False, "label": "Function call", "example": "add", "group": "Functions & Methods"},
    "entity.name.function.method.scriptit": {"color": "#00ffff", "bold": False, "italic": False, "label": "Method call (.push, .pop)", "example": ".push", "group": "Functions & Methods"},
    "entity.name.function.forward.scriptit": {"color": "#00ffff", "bold": False, "italic": False, "label": "Forward declaration", "example": "myFunc", "group": "Functions & Methods"},
    "support.function.builtin.scriptit": {"color": "#00ffff", "bold": False, "italic": False, "label": "Builtin (print, len, input)", "example": "print", "group": "Functions & Methods"},
    "support.function.math.scriptit": {"color": "#00ffff", "bold": False, "italic": False, "label": "Math (sin, cos, sqrt)", "example": "sqrt", "group": "Functions & Methods"},
    "support.type.conversion.scriptit": {"color": "#008080", "bold": False, "italic": False, "label": "Type conversion (int, str, graph)", "example": "int()", "group": "Functions & Methods"},
    # Variables
    "variable.other.scriptit": {"color": "#eeeeee", "bold": False, "italic": False, "label": "Variables", "example": "name", "group": "Variables & Parameters"},
    "variable.other.declaration.scriptit": {"color": "#eeeeee", "bold": False, "italic": False, "label": "Declaration (after var)", "example": "x", "group": "Variables & Parameters"},
    "variable.parameter.scriptit": {"color": "#eeeeee", "bold": False, "italic": False, "label": "Function parameters", "example": "a, b", "group": "Variables & Parameters"},
    "variable.parameter.reference.scriptit": {"color": "#eeeeee", "bold": False, "italic": False, "label": "Reference parameters", "example": "@x", "group": "Variables & Parameters"},
    "variable.other.reference.scriptit": {"color": "#eeeeee", "bold": False, "italic": False, "label": "Reference variable", "example": "@x", "group": "Variables & Parameters"},
    "variable.other.loop.scriptit": {"color": "#eeeeee", "bold": False, "italic": False, "label": "Loop variable", "example": "i", "group": "Variables & Parameters"},
    # Literals
    "constant.numeric.integer.scriptit": {"color": "#d19a66", "bold": False, "italic": False, "label": "Integer numbers", "example": "42", "group": "Literals & Constants"},
    "constant.numeric.float.scriptit": {"color": "#d19a66", "bold": False, "italic": False, "label": "Float numbers", "example": "3.14", "group": "Literals & Constants"},
    "constant.language.boolean.true.scriptit": {"color": "#c678dd", "bold": False, "italic": False, "label": "True", "example": "True", "group": "Literals & Constants"},
    "constant.language.boolean.false.scriptit": {"color": "#c678dd", "bold": False, "italic": False, "label": "False", "example": "False", "group": "Literals & Constants"},
    "constant.language.none.scriptit": {"color": "#c678dd", "bold": False, "italic": False, "label": "None", "example": "None", "group": "Literals & Constants"},
    "string.quoted.double.scriptit": {"color": "#98c379", "bold": False, "italic": False, "label": "Strings (double quote)", "example": '"hello"', "group": "Literals & Constants"},
    "string.quoted.single.scriptit": {"color": "#98c379", "bold": False, "italic": False, "label": "Strings (single quote)", "example": "'hi'", "group": "Literals & Constants"},
    "constant.character.escape.scriptit": {"color": "#d19a66", "bold": False, "italic": False, "label": "Escape chars (\\n, \\t)", "example": "\\n", "group": "Literals & Constants"},
    # Comments
    "comment.line.number-sign.scriptit": {"color": "#5c6370", "bold": False, "italic": True, "label": "# line comment", "example": "# note", "group": "Comments"},
    "comment.block.arrow.scriptit": {"color": "#98c379", "bold": False, "italic": False, "label": "--> block comment <--", "example": "--> ... <--", "group": "Comments"},
    # Operators
    "keyword.operator.comparison.scriptit": {"color": "#c678dd", "bold": False, "italic": False, "label": "Comparison (==, !=, <, >)", "example": "==", "group": "Operators"},
    "keyword.operator.arithmetic.scriptit": {"color": "#c678dd", "bold": False, "italic": False, "label": "Arithmetic (+, -, *, /)", "example": "+", "group": "Operators"},
    "keyword.operator.assignment.scriptit": {"color": "#c678dd", "bold": False, "italic": False, "label": "Assignment (=)", "example": "=", "group": "Operators"},
    "keyword.operator.assignment.compound.scriptit": {"color": "#c678dd", "bold": False, "italic": False, "label": "Compound (+=, -=, *=)", "example": "+=", "group": "Operators"},
    "keyword.operator.increment.scriptit": {"color": "#c678dd", "bold": False, "italic": False, "label": "Increment (++, --)", "example": "++", "group": "Operators"},
    "keyword.operator.logical.scriptit": {"color": "#c678dd", "bold": False, "italic": False, "label": "Logical (and, or, not)", "example": "and", "group": "Operators"},
    "keyword.operator.logical.symbol.scriptit": {"color": "#c678dd", "bold": False, "italic": False, "label": "Logical symbols (&&, ||)", "example": "&&", "group": "Operators"},
    "keyword.operator.edge.scriptit": {"color": "#61afef", "bold": True, "italic": False, "label": "Edge / Arrow (->, <->, ---)", "example": "->", "group": "Operators"},
    # Punctuation
    "punctuation.bracket.round.scriptit": {"color": "#abb2bf", "bold": False, "italic": False, "label": "Parentheses ( )", "example": "()", "group": "Punctuation"},
    "punctuation.bracket.square.scriptit": {"color": "#abb2bf", "bold": False, "italic": False, "label": "Square brackets [ ]", "example": "[]", "group": "Punctuation"},
    "punctuation.bracket.curly.scriptit": {"color": "#abb2bf", "bold": False, "italic": False, "label": "Curly braces { }", "example": "{}", "group": "Punctuation"},
    "punctuation.separator.comma.scriptit": {"color": "#abb2bf", "bold": False, "italic": False, "label": "Comma", "example": ",", "group": "Punctuation"},
    "punctuation.separator.colon.scriptit": {"color": "#abb2bf", "bold": False, "italic": False, "label": "Colon", "example": ":", "group": "Punctuation"},
    "punctuation.separator.parameter.scriptit": {"color": "#abb2bf", "bold": False, "italic": False, "label": "Parameter comma", "example": ",", "group": "Punctuation"},
    "punctuation.terminator.block.scriptit": {"color": "#abb2bf", "bold": False, "italic": False, "label": "Semicolon (block end)", "example": ";", "group": "Punctuation"},
    "punctuation.terminator.statement.scriptit": {"color": "#abb2bf", "bold": False, "italic": False, "label": "Dot (statement end)", "example": ".", "group": "Punctuation"},
    "punctuation.definition.function.scriptit": {"color": "#abb2bf", "bold": False, "italic": False, "label": "Function def punctuation", "example": ":", "group": "Punctuation"},
    "punctuation.definition.comment.begin.scriptit": {"color": "#98c379", "bold": False, "italic": False, "label": "Comment start (-->)", "example": "-->", "group": "Comments"},
    "punctuation.definition.comment.end.scriptit": {"color": "#98c379", "bold": False, "italic": False, "label": "Comment end (<--)", "example": "<--", "group": "Comments"},
}


def collect_scopes_from_grammar(grammar):
    """Walk the grammar and collect all unique scope names.
    Skips meta.* scopes (they're container scopes, not for coloring)."""
    scopes = set()

    def walk(obj):
        if isinstance(obj, dict):
            if "name" in obj and obj["name"].endswith(".scriptit"):
                # Skip meta scopes — they're structural, not colorable
                if not obj["name"].startswith("meta."):
                    scopes.add(obj["name"])
            for v in obj.values():
                walk(v)
        elif isinstance(obj, list):
            for item in obj:
                walk(item)

    walk(grammar.get("repository", {}))
    return sorted(scopes)


def build_token_data(grammar):
    """Build token_data.json from the generated grammar."""
    scopes = collect_scopes_from_grammar(grammar)

    groups = OrderedDict()
    for scope in scopes:
        defaults = SCOPE_DEFAULTS.get(scope, {
            "color": "#cccccc", "bold": False, "italic": False,
            "label": scope.split(".")[-2] if len(scope.split(".")) > 2 else scope,
            "example": "", "group": "Other",
        })
        group_name = defaults.get("group", "Other")
        if group_name not in groups:
            groups[group_name] = []

        # Build a stable ID from the scope
        parts = scope.replace(".scriptit", "").split(".")
        token_id = "_".join(parts[-2:]) if len(parts) >= 2 else parts[-1]

        groups[group_name].append({
            "id": token_id,
            "scope": scope,
            "color": defaults["color"],
            "bold": defaults["bold"],
            "italic": defaults["italic"],
            "label": defaults["label"],
            "example": defaults["example"],
        })

    # Convert to the list-of-groups format the customizer expects
    result = []
    for title, tokens in groups.items():
        result.append({"title": title, "tokens": tokens})

    return result


# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# 5. UPDATE PACKAGE.JSON COLOR DEFAULTS
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

def build_color_rules(token_data):
    """Build the textMateRules array for package.json configurationDefaults."""
    rules = []
    for group in token_data:
        for token in group["tokens"]:
            settings = {"foreground": token["color"]}
            styles = []
            if token["bold"]:
                styles.append("bold")
            if token["italic"]:
                styles.append("italic")
            if styles:
                settings["fontStyle"] = " ".join(styles)
            rules.append({"scope": token["scope"], "settings": settings})
    return rules


def update_package_json(token_data):
    """Update the configurationDefaults in package.json with color rules from token data."""
    with open(PACKAGE_JSON, "r") as f:
        pkg = json.load(f, object_pairs_hook=OrderedDict)

    rules = build_color_rules(token_data)

    # Ensure configurationDefaults exists
    if "configurationDefaults" not in pkg:
        pkg["configurationDefaults"] = OrderedDict()
    pkg["configurationDefaults"]["editor.tokenColorCustomizations"] = OrderedDict([
        ("textMateRules", rules)
    ])

    with open(PACKAGE_JSON, "w") as f:
        json.dump(pkg, f, indent=2, ensure_ascii=False)
        f.write("\n")


# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# 6. MAIN
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

def main():
    check_mode = "--check" in sys.argv

    # 1. Read C++ sources
    types_src = read_file(TYPES_HPP)
    builtins_src = read_file(BUILTINS_HPP)

    # 2. Extract language constructs
    keywords = extract_keywords(types_src)
    math_fns = extract_math_functions(types_src)
    builtin_names_type = extract_builtin_functions(types_src)
    builtin_names_map = extract_builtins_from_map(builtins_src)
    all_builtins = sorted(set(builtin_names_type) | set(builtin_names_map))
    operators = extract_operators(types_src)
    type_fns = sorted(TYPE_FUNCTIONS & set(all_builtins) | TYPE_FUNCTIONS)

    edge_ops, comp_ops, logical_sym_ops = classify_operators(operators)

    print(f"📊 Extracted from parser:")
    print(f"   Keywords:  {len(keywords)} → {keywords}")
    print(f"   Math fns:  {len(math_fns)} → {math_fns}")
    print(f"   Builtins:  {len(all_builtins)} → {all_builtins}")
    print(f"   Type fns:  {len(type_fns)} → {sorted(type_fns)}")
    print(f"   Operators: {len(operators)} → {operators}")
    print(f"   Edge ops:  {edge_ops}")

    # 3. Generate grammar
    pure_builtins = classify_builtins(all_builtins, math_fns)
    grammar = build_tmlanguage(keywords, math_fns, pure_builtins, type_fns, edge_ops, comp_ops, logical_sym_ops)

    # 4. Generate token data for customizer
    token_data = build_token_data(grammar)

    # 5. Serialize
    grammar_json = json.dumps(grammar, indent=2, ensure_ascii=False) + "\n"
    token_json = json.dumps(token_data, indent=2, ensure_ascii=False) + "\n"

    if check_mode:
        # Compare with existing files
        changed = False
        if TMLANG_OUT.exists():
            existing = TMLANG_OUT.read_text()
            if existing != grammar_json:
                print(f"❌ {TMLANG_OUT.relative_to(ROOT)} is out of date!")
                changed = True
            else:
                print(f"✅ {TMLANG_OUT.relative_to(ROOT)} is up to date")
        else:
            print(f"❌ {TMLANG_OUT.relative_to(ROOT)} does not exist!")
            changed = True

        if TOKEN_DATA_OUT.exists():
            existing = TOKEN_DATA_OUT.read_text()
            if existing != token_json:
                print(f"❌ {TOKEN_DATA_OUT.relative_to(ROOT)} is out of date!")
                changed = True
            else:
                print(f"✅ {TOKEN_DATA_OUT.relative_to(ROOT)} is up to date")
        else:
            print(f"❌ {TOKEN_DATA_OUT.relative_to(ROOT)} does not exist!")
            changed = True

        if changed:
            print("\n⚠  Run: python3 scripts/gen_grammar.py")
            sys.exit(1)
        sys.exit(0)

    # 6. Write files
    os.makedirs(TMLANG_OUT.parent, exist_ok=True)
    os.makedirs(TOKEN_DATA_OUT.parent, exist_ok=True)

    TMLANG_OUT.write_text(grammar_json)
    print(f"\n✅ Generated: {TMLANG_OUT.relative_to(ROOT)}")

    TOKEN_DATA_OUT.write_text(token_json)
    print(f"✅ Generated: {TOKEN_DATA_OUT.relative_to(ROOT)}")

    update_package_json(token_data)
    print(f"✅ Updated:   {PACKAGE_JSON.relative_to(ROOT)} (configurationDefaults)")

    print(f"\n🎉 Grammar generation complete! All files derived from parser source of truth.")


if __name__ == "__main__":
    main()
