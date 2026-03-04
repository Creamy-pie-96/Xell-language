#!/usr/bin/env python3
"""
gen_xell_grammar.py — Fully dynamic grammar, token & snippet generator for Xell
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Single source of truth: the C++ source files.

Reads:
  - src/lexer/token.hpp              → keyword names from TokenType enum
  - src/builtins/builtins_*.hpp      → builtin function names from t["name"] patterns
  - src/builtins/register_all.hpp    → Tier 2 module names from regModule() calls

Generates:
  - Extensions/xell-vscode/syntaxes/xell.tmLanguage.json
  - Extensions/xell-vscode/color_customizer/token_data.json
  - Extensions/xell-vscode/snippets/xell.json
  - Extensions/xell-vscode/src/server/language_data.json   (completions, hover, diagnostics)
  - Extensions/xell-vscode/language-configuration.json      (indent patterns with all block keywords)
  - stdlib/dialect_template.xesy                            (@convert dialect template)

Usage:
    python3 Extensions/gen_xell_grammar.py              # generate all files
    python3 Extensions/gen_xell_grammar.py --check      # verify files are up-to-date
    python3 Extensions/gen_xell_grammar.py --install     # generate + build + install extension
    python3 Extensions/gen_xell_grammar.py --gen_xesy [path]  # generate a .xesy template only
"""

import re
import json
import sys
import subprocess
import shutil
import glob as globmod
from pathlib import Path
from collections import OrderedDict

# ─── Paths ────────────────────────────────────────────────

SCRIPT_DIR = Path(__file__).resolve().parent
ROOT = SCRIPT_DIR.parent
SRC_DIR = ROOT / "src"
TOKEN_HPP = SRC_DIR / "lexer" / "token.hpp"
LEXER_CPP = SRC_DIR / "lexer" / "lexer.cpp"
BUILTINS_DIR = SRC_DIR / "builtins"

VSCODE_DIR = SCRIPT_DIR / "xell-vscode"
TMLANG_OUT = VSCODE_DIR / "syntaxes" / "xell.tmLanguage.json"
TOKEN_DATA_OUT = VSCODE_DIR / "color_customizer" / "token_data.json"
SNIPPETS_OUT = VSCODE_DIR / "snippets" / "xell.json"
LANG_DATA_OUT = VSCODE_DIR / "src" / "server" / "language_data.json"
LANG_CONFIG_OUT = VSCODE_DIR / "language-configuration.json"
TERMINAL_COLORS_OUT = VSCODE_DIR / "color_customizer" / "terminal_colors.json"
XESY_TEMPLATE_OUT = ROOT / "stdlib" / "dialect_template.xesy"
REGISTER_ALL_HPP = BUILTINS_DIR / "register_all.hpp"


# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# 1. EXTRACT DATA FROM C++ HEADERS (fully dynamic)
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

def read_file(path):
    with open(path, "r") as f:
        return f.read()


def extract_keyword_map(token_src):
    """
    Dynamically extract the keywordMap() from lexer.cpp.
    Returns dict: keyword_string → ENUM_NAME.
    e.g. {"fn": "FN", "bring": "BRING", "module": "MODULE", ...}
    """
    # keywordMap() lives in lexer.cpp, not token.hpp
    if not LEXER_CPP.exists():
        print(f"WARNING: {LEXER_CPP} not found")
        return {}

    lexer_src = read_file(LEXER_CPP)

    # Find the keywordMap function body — the map = { ... };
    m = re.search(r'keywordMap\s*\(\s*\)\s*\{.*?map\s*=\s*\{(.*?)\}\s*;', lexer_src, re.DOTALL)
    if not m:
        print("WARNING: Could not find keywordMap() in lexer.cpp")
        return {}

    body = m.group(1)
    # Match: {"keyword", TokenType::ENUM_NAME}
    pairs = re.findall(r'\{\s*"(\w+)"\s*,\s*TokenType::(\w+)\s*\}', body)
    return {kw: enum for kw, enum in pairs}


def extract_enum_categories(src):
    """
    Parse the TokenType enum, reading section comments to automatically
    determine categories. Returns dict: category_name → [ENUM_NAME, ...].
    e.g. {"Control flow": ["FN", "GIVE", "IF", ...], "Import / module": ["BRING", ...]}
    """
    m = re.search(r'enum\s+class\s+TokenType\s*\{(.*?)\}', src, re.DOTALL)
    if not m:
        return {}

    body = m.group(1)
    categories = {}
    current_category = "other"

    for line in body.split('\n'):
        line = line.strip()

        # Check for section comment like: // Control flow keywords
        comment_m = re.match(r'^//\s*(.+?)(?:\s+keywords?)?$', line)
        if comment_m:
            current_category = comment_m.group(1).strip()
            continue

        # Check for enum entry
        enum_m = re.match(r'^([A-Z][A-Z_0-9]+)\s*,?', line)
        if enum_m:
            name = enum_m.group(1)
            categories.setdefault(current_category, []).append(name)

    return categories


def extract_keywords_dynamic(token_src):
    """Extract all keyword strings from keywordMap() — fully dynamic."""
    kw_map = extract_keyword_map(token_src)
    return sorted(kw_map.keys())


# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# CATEGORY → GRAMMAR SCOPE mapping (driven by enum comments)
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

# Maps section comment labels → TextMate grammar scope classification.
# If a new section comment is added in token.hpp but not here,
# keywords fall into "declaration" as default.
CATEGORY_TO_CLASS = {
    "Boolean & None":       "constants",
    "Control flow":         "control",     # will be sub-classified below
    "Import / module":      "import",      # will be sub-classified below
    "Enum":                 "oop_decl",
    "OOP":                  "oop_decl",    # will be sub-classified below
    "Generator":            "generator",
    "Async":                "async",
    "Logical":              "logical",
    "Comparison":           "comparison",
    "Utility":              "special",
}


def classify_keywords(token_src):
    """
    Fully dynamic keyword classification.
    Reads enum categories from token.hpp + keywordMap from lexer.cpp
    to determine which keywords belong to which grammar scope class.
    """
    kw_map = extract_keyword_map(token_src)       # {"fn": "FN", "module": "MODULE", ...}
    enum_cats = extract_enum_categories(token_src) # {"Control flow": ["FN","GIVE",...], ...}

    # Reverse: ENUM_NAME → category_comment
    enum_to_cat = {}
    for cat_comment, names in enum_cats.items():
        for name in names:
            enum_to_cat[name] = cat_comment

    # Build fine-grained classification
    result = {
        "conditional":  [],   # if, elif, else, incase
        "loop":         [],   # for, while, loop, in
        "control_flow": [],   # break, continue
        "fn_decl":      [],   # fn
        "return":       [],   # give
        "error":        [],   # try, catch, finally
        "binding":      [],   # let, be, immutable
        "import":       [],   # bring, from, as
        "module":       [],   # module, export, requires
        "oop_decl":     [],   # class, struct, interface, abstract, mixin, enum
        "oop_modifier": [],   # inherits, implements, with
        "access":       [],   # private, protected, public, static
        "generator":    [],   # yield
        "async":        [],   # async, await
        "logical":      [],   # and, or, not
        "comparison":   [],   # is, eq, ne, gt, lt, ge, le
        "constants":    [],   # true, false, none
        "special":      [],   # of
    }

    # Fine-grained sub-classification rules
    CONDITIONAL_WORDS = {"if", "elif", "else", "incase"}
    LOOP_WORDS = {"for", "while", "loop", "in"}
    CONTROL_FLOW_WORDS = {"break", "continue"}
    FN_DECL_WORDS = {"fn"}
    RETURN_WORDS = {"give"}
    ERROR_WORDS = {"try", "catch", "finally"}
    BINDING_WORDS = {"let", "be", "immutable"}
    IMPORT_WORDS = {"bring", "from", "as"}
    MODULE_WORDS = {"module", "export", "requires"}
    OOP_DECL_WORDS = {"class", "struct", "interface", "abstract", "mixin", "enum"}
    OOP_MODIFIER_WORDS = {"inherits", "implements", "with"}
    ACCESS_WORDS = {"private", "protected", "public", "static"}

    for kw_str, enum_name in kw_map.items():
        # Use fine-grained sub-classification
        if kw_str in CONDITIONAL_WORDS:
            result["conditional"].append(kw_str)
        elif kw_str in LOOP_WORDS:
            result["loop"].append(kw_str)
        elif kw_str in CONTROL_FLOW_WORDS:
            result["control_flow"].append(kw_str)
        elif kw_str in FN_DECL_WORDS:
            result["fn_decl"].append(kw_str)
        elif kw_str in RETURN_WORDS:
            result["return"].append(kw_str)
        elif kw_str in ERROR_WORDS:
            result["error"].append(kw_str)
        elif kw_str in BINDING_WORDS:
            result["binding"].append(kw_str)
        elif kw_str in IMPORT_WORDS:
            result["import"].append(kw_str)
        elif kw_str in MODULE_WORDS:
            result["module"].append(kw_str)
        elif kw_str in OOP_DECL_WORDS:
            result["oop_decl"].append(kw_str)
        elif kw_str in OOP_MODIFIER_WORDS:
            result["oop_modifier"].append(kw_str)
        elif kw_str in ACCESS_WORDS:
            result["access"].append(kw_str)
        else:
            # Fall back to enum category mapping
            cat_comment = enum_to_cat.get(enum_name, "other")
            matched = False
            for prefix, cls in CATEGORY_TO_CLASS.items():
                if cat_comment.startswith(prefix) or prefix in cat_comment:
                    result[cls].append(kw_str)
                    matched = True
                    break
            if not matched:
                result["special"].append(kw_str)

    # Sort each class
    for cls in result:
        result[cls] = sorted(result[cls])

    return result


def extract_builtins_from_file(filepath):
    """Extract builtin names from a single builtins_*.hpp file."""
    content = read_file(filepath)
    builtins = []
    for m in re.finditer(r't\s*\[\s*"(\w+)"\s*\]', content):
        builtins.append(m.group(1))
    return builtins


def extract_all_builtins():
    """Extract builtins from each builtins_*.hpp, categorized by filename."""
    categories = {}
    if not BUILTINS_DIR.exists():
        return categories
    for hpp in sorted(BUILTINS_DIR.glob("builtins_*.hpp")):
        cat = hpp.stem.replace("builtins_", "")
        names = extract_builtins_from_file(hpp)
        if names:
            categories[cat] = names
    return categories


def extract_tier2_modules():
    """Extract Tier 2 module names from register_all.hpp."""
    if not REGISTER_ALL_HPP.exists():
        return []
    content = read_file(REGISTER_ALL_HPP)
    # Match: regModule("name", true, ...
    return re.findall(r'regModule\s*\(\s*"(\w+)"\s*,\s*true\b', content)


# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# 2. EXTRACT BUILTINS (unchanged — already dynamic)
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━


# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# 3. GENERATE TMLANGUAGE JSON
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

def word_alt(words):
    """Build \\b(word1|word2|...)\\b alternation, longest first."""
    return "\\b(" + "|".join(sorted(words, key=lambda w: (-len(w), w))) + ")\\b"


def build_tmlanguage(kw_classes, builtin_cats):
    grammar = OrderedDict()
    grammar["$schema"] = "https://raw.githubusercontent.com/martinring/tmlanguage/master/tmlanguage.json"
    grammar["name"] = "Xell"
    grammar["scopeName"] = "source.xell"
    grammar["fileTypes"] = ["xel", "nxel"]
    grammar["comment"] = (
        "AUTO-GENERATED by gen_xell_grammar.py — DO NOT EDIT MANUALLY. "
        "Re-run: python3 Extensions/gen_xell_grammar.py"
    )

    includes = [
        "#block-comment", "#line-comment", "#strings",
        "#convert-decorator",
        "#function-definition", "#for-loop", "#for-in-loop", "#bring-statement",
        "#fn-declaration-keywords", "#return-keywords",
        "#conditional-keywords", "#loop-keywords", "#control-flow-keywords",
        "#error-keywords", "#binding-keywords",
        "#import-keywords", "#module-keywords",
        "#oop-declaration-keywords", "#oop-modifier-keywords", "#access-keywords",
        "#generator-keywords", "#async-keywords",
        "#logical-operators", "#comparison-word-operators",
        "#boolean-constants", "#none-constant",
        "#special-keywords",
    ]
    for cat in sorted(builtin_cats.keys()):
        includes.append(f"#{cat}-builtins")
    includes += [
        "#arrow-access", "#comparison-operators", "#increment-operators",
        "#assignment-operator", "#arithmetic-operators",
        "#numbers", "#method-call", "#function-call",
        "#semicolon-terminator", "#dot-terminator",
        "#punctuation", "#identifiers",
    ]
    grammar["patterns"] = [{"include": inc} for inc in includes]

    repo = OrderedDict()

    # Comments
    repo["block-comment"] = {
        "name": "comment.block.arrow.xell",
        "begin": "-->",
        "end": "<--",
        "beginCaptures": {"0": {"name": "punctuation.definition.comment.begin.xell"}},
        "endCaptures": {"0": {"name": "punctuation.definition.comment.end.xell"}},
    }
    repo["line-comment"] = {
        "name": "comment.line.number-sign.xell",
        "match": "#.*$",
    }

    # Strings with interpolation
    repo["strings"] = {
        "patterns": [{
            "name": "string.quoted.double.xell",
            "begin": "\"",
            "end": "\"",
            "patterns": [
                {
                    "name": "string.interpolation.xell",
                    "begin": "\\{",
                    "end": "\\}",
                    "beginCaptures": {"0": {"name": "punctuation.section.interpolation.begin.xell"}},
                    "endCaptures": {"0": {"name": "punctuation.section.interpolation.end.xell"}},
                    "patterns": [{"include": "source.xell"}]
                },
                {"name": "constant.character.escape.xell", "match": "\\\\."}
            ]
        }]
    }

    # @convert dialect decorator
    repo["convert-decorator"] = {
        "comment": "@convert \"dialect.xesy\" — dialect mapping directive",
        "patterns": [{
            "match": "^\\s*(@convert)\\s+(\"[^\"]*\")",
            "captures": {
                "1": {"name": "keyword.other.decorator.xell"},
                "2": {"name": "string.quoted.double.xell"},
            }
        }]
    }

    # Function definition
    repo["function-definition"] = {
        "comment": "fn name(param1, param2):",
        "patterns": [{
            "name": "meta.function.definition.xell",
            "begin": "\\b(fn)\\s+([a-zA-Z_][a-zA-Z0-9_]*)\\s*\\(",
            "beginCaptures": {
                "1": {"name": "keyword.declaration.function.xell"},
                "2": {"name": "entity.name.function.definition.xell"},
            },
            "end": "\\)\\s*(:|\\.)",
            "endCaptures": {"1": {"name": "punctuation.definition.function.xell"}},
            "patterns": [
                {"name": "variable.parameter.xell", "match": "\\b([a-zA-Z_][a-zA-Z0-9_]*)\\b"},
                {"name": "punctuation.separator.parameter.xell", "match": ","},
            ]
        }]
    }

    # For loop
    repo["for-loop"] = {
        "comment": "for i in range(...)",
        "patterns": [{
            "match": "\\b(for)\\s+([a-zA-Z_][a-zA-Z0-9_]*)\\s+(in)\\s+(range)\\b",
            "captures": {
                "1": {"name": "keyword.control.flow.xell"},
                "2": {"name": "variable.other.loop.xell"},
                "3": {"name": "keyword.control.loop.xell"},
                "4": {"name": "support.function.builtin.xell"},
            }
        }]
    }

    # For-in loop
    repo["for-in-loop"] = {
        "comment": "for item in collection",
        "patterns": [{
            "match": "\\b(for)\\s+([a-zA-Z_][a-zA-Z0-9_]*)\\s+(in)\\b",
            "captures": {
                "1": {"name": "keyword.control.flow.xell"},
                "2": {"name": "variable.other.loop.xell"},
                "3": {"name": "keyword.control.loop.xell"},
            }
        }]
    }

    # Bring statement
    repo["bring-statement"] = {
        "comment": "bring name from 'file' as alias",
        "patterns": [{
            "match": "\\b(bring)\\s+(.+?)\\s+(from)\\b",
            "captures": {
                "1": {"name": "keyword.control.import.xell"},
                "3": {"name": "keyword.control.import.xell"},
            }
        }]
    }

    # ── Fine-grained keyword groups ─────────────────────────

    # Function declaration: fn
    if kw_classes["fn_decl"]:
        repo["fn-declaration-keywords"] = {
            "comment": f"Function declaration: {', '.join(kw_classes['fn_decl'])}",
            "patterns": [
                {"name": "keyword.declaration.function.xell", "match": word_alt(kw_classes["fn_decl"])}
            ]
        }

    # Return: give
    if kw_classes["return"]:
        repo["return-keywords"] = {
            "comment": f"Return: {', '.join(kw_classes['return'])}",
            "patterns": [
                {"name": "keyword.control.return.xell", "match": word_alt(kw_classes["return"])}
            ]
        }

    # Conditionals: if, elif, else, incase
    if kw_classes["conditional"]:
        repo["conditional-keywords"] = {
            "comment": f"Conditional: {', '.join(kw_classes['conditional'])}",
            "patterns": [
                {"name": "keyword.control.conditional.xell", "match": word_alt(kw_classes["conditional"])}
            ]
        }

    # Loops: for, while, loop, in
    if kw_classes["loop"]:
        repo["loop-keywords"] = {
            "comment": f"Loop: {', '.join(kw_classes['loop'])}",
            "patterns": [
                {"name": "keyword.control.loop.xell", "match": word_alt(kw_classes["loop"] + ["range"])}
            ]
        }

    # Control flow jumps: break, continue
    if kw_classes["control_flow"]:
        repo["control-flow-keywords"] = {
            "comment": f"Control flow: {', '.join(kw_classes['control_flow'])}",
            "patterns": [
                {"name": "keyword.control.flow.xell", "match": word_alt(kw_classes["control_flow"])}
            ]
        }

    # Error handling: try, catch, finally
    if kw_classes["error"]:
        repo["error-keywords"] = {
            "comment": f"Error handling: {', '.join(kw_classes['error'])}",
            "patterns": [
                {"name": "keyword.control.trycatch.xell", "match": word_alt(kw_classes["error"])}
            ]
        }

    # Binding: let, be, immutable
    if kw_classes["binding"]:
        repo["binding-keywords"] = {
            "comment": f"Binding: {', '.join(kw_classes['binding'])}",
            "patterns": [
                {"name": "keyword.other.binding.xell", "match": word_alt(kw_classes["binding"])}
            ]
        }

    # Import: bring, from, as
    if kw_classes["import"]:
        repo["import-keywords"] = {
            "comment": f"Import: {', '.join(kw_classes['import'])}",
            "patterns": [
                {"name": "keyword.control.import.xell", "match": word_alt(kw_classes["import"])}
            ]
        }

    # Module: module, export, requires
    if kw_classes["module"]:
        repo["module-keywords"] = {
            "comment": f"Module: {', '.join(kw_classes['module'])}",
            "patterns": [
                {"name": "keyword.control.module.xell", "match": word_alt(kw_classes["module"])}
            ]
        }

    # OOP declarations: class, struct, interface, abstract, mixin, enum
    if kw_classes["oop_decl"]:
        repo["oop-declaration-keywords"] = {
            "comment": f"OOP declaration: {', '.join(kw_classes['oop_decl'])}",
            "patterns": [
                {"name": "keyword.declaration.type.xell", "match": word_alt(kw_classes["oop_decl"])}
            ]
        }

    # OOP modifiers: inherits, implements, with
    if kw_classes["oop_modifier"]:
        repo["oop-modifier-keywords"] = {
            "comment": f"OOP modifier: {', '.join(kw_classes['oop_modifier'])}",
            "patterns": [
                {"name": "keyword.other.modifier.xell", "match": word_alt(kw_classes["oop_modifier"])}
            ]
        }

    # Access modifiers: private, protected, public, static
    if kw_classes.get("access"):
        repo["access-keywords"] = {
            "comment": f"Access: {', '.join(kw_classes['access'])}",
            "patterns": [
                {"name": "storage.modifier.xell", "match": word_alt(kw_classes["access"])}
            ]
        }

    # Generator: yield
    if kw_classes["generator"]:
        repo["generator-keywords"] = {
            "comment": f"Generator: {', '.join(kw_classes['generator'])}",
            "patterns": [
                {"name": "keyword.control.yield.xell", "match": word_alt(kw_classes["generator"])}
            ]
        }

    # Async: async, await
    if kw_classes["async"]:
        repo["async-keywords"] = {
            "comment": f"Async: {', '.join(kw_classes['async'])}",
            "patterns": [
                {"name": "keyword.control.async.xell", "match": word_alt(kw_classes["async"])}
            ]
        }

    # Logical operators: and, or, not
    if kw_classes["logical"]:
        repo["logical-operators"] = {
            "comment": f"Logical: {', '.join(kw_classes['logical'])}",
            "patterns": [
                {"name": "keyword.operator.logical.xell", "match": word_alt(kw_classes["logical"])}
            ]
        }

    # Comparison word operators: is, eq, ne, gt, lt, ge, le
    if kw_classes["comparison"]:
        repo["comparison-word-operators"] = {
            "comment": f"Comparison words: {', '.join(kw_classes['comparison'])}",
            "patterns": [
                {"name": "keyword.operator.comparison.word.xell", "match": word_alt(kw_classes["comparison"])}
            ]
        }

    # Special: of
    if kw_classes["special"]:
        repo["special-keywords"] = {
            "comment": f"Special: {', '.join(kw_classes['special'])}",
            "patterns": [
                {"name": "keyword.other.special.xell", "match": word_alt(kw_classes["special"])}
            ]
        }

    # Constants
    bools = [k for k in kw_classes["constants"] if k in ("true", "false")]
    if bools:
        repo["boolean-constants"] = {
            "patterns": [
                {"name": "constant.language.boolean.true.xell", "match": "\\btrue\\b"},
                {"name": "constant.language.boolean.false.xell", "match": "\\bfalse\\b"},
            ]
        }
    if "none" in kw_classes["constants"]:
        repo["none-constant"] = {
            "patterns": [
                {"name": "constant.language.none.xell", "match": "\\bnone\\b"}
            ]
        }

    # Builtin categories — dynamically generated
    SCOPE_MAP = {
        "io": "support.function.builtin.xell",
        "util": "support.function.builtin.xell",
        "type": "support.type.conversion.xell",
        "collection": "support.function.builtin.xell",
        "math": "support.function.math.xell",
        "os": "support.function.builtin.os.xell",
    }

    for cat in sorted(builtin_cats.keys()):
        names = sorted(builtin_cats[cat])
        scope = SCOPE_MAP.get(cat, f"support.function.{cat}.xell")
        repo[f"{cat}-builtins"] = {
            "comment": f"{cat.upper()} builtins: {', '.join(names)}",
            "patterns": [{
                "name": scope,
                "match": "\\b(" + "|".join(names) + ")\\b(?=\\s*\\()"
            }]
        }

    # Operators
    repo["arrow-access"] = {
        "comment": "-> map key access operator",
        "patterns": [{"name": "keyword.operator.access.xell", "match": "->"}]
    }
    repo["comparison-operators"] = {
        "patterns": [{"name": "keyword.operator.comparison.xell", "match": "==|!=|<=|>=|<|>"}]
    }
    repo["increment-operators"] = {
        "patterns": [{"name": "keyword.operator.increment.xell", "match": "\\+\\+|--"}]
    }
    repo["assignment-operator"] = {
        "patterns": [{"name": "keyword.operator.assignment.xell", "match": "="}]
    }
    repo["arithmetic-operators"] = {
        "patterns": [{"name": "keyword.operator.arithmetic.xell", "match": "\\+|-|\\*|/|%"}]
    }

    # Numbers
    repo["numbers"] = {
        "patterns": [
            {"name": "constant.numeric.float.xell", "match": "(?<![a-zA-Z_\\.])\\d+\\.\\d+"},
            {"name": "constant.numeric.integer.xell", "match": "(?<![a-zA-Z_\\.])\\d+(?!\\.\\d)"},
        ]
    }

    # Method call
    repo["method-call"] = {
        "comment": ".name( — method call via dot",
        "patterns": [{
            "match": "\\.([a-zA-Z_][a-zA-Z0-9_]*)\\s*(?=\\()",
            "captures": {"1": {"name": "entity.name.function.method.xell"}}
        }]
    }

    # Function call
    repo["function-call"] = {
        "comment": "name( — function call",
        "patterns": [{
            "match": "\\b([a-zA-Z_][a-zA-Z0-9_]*)\\s*(?=\\()",
            "captures": {"1": {"name": "entity.name.function.call.xell"}}
        }]
    }

    # Terminators
    repo["semicolon-terminator"] = {
        "patterns": [{"name": "punctuation.terminator.block.xell", "match": "^\\s*;\\s*$"}]
    }
    repo["dot-terminator"] = {
        "patterns": [{"name": "punctuation.terminator.statement.xell", "match": "\\.(?=\\s*$|\\s*#)"}]
    }

    # Punctuation
    repo["punctuation"] = {
        "patterns": [
            {"name": "punctuation.bracket.round.xell", "match": "[()]"},
            {"name": "punctuation.bracket.square.xell", "match": "[\\[\\]]"},
            {"name": "punctuation.bracket.curly.xell", "match": "[{}]"},
            {"name": "punctuation.separator.comma.xell", "match": ","},
            {"name": "punctuation.separator.colon.xell", "match": ":"},
        ]
    }

    # Identifiers
    repo["identifiers"] = {
        "comment": "Catch-all: remaining identifiers → variable color",
        "patterns": [
            {"name": "variable.other.xell", "match": "\\b[a-zA-Z_][a-zA-Z0-9_]*\\b"}
        ]
    }

    grammar["repository"] = repo
    return grammar


# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# 4. GENERATE token_data.json FOR CUSTOMIZER
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

DEFAULT_COLORS = {
    "comment.block.arrow.xell": "#98c379",
    "comment.line.number-sign.xell": "#5c6370",
    "punctuation.definition.comment.begin.xell": "#98c379",
    "punctuation.definition.comment.end.xell": "#98c379",
    "constant.character.escape.xell": "#d19a66",
    "constant.language.boolean.true.xell": "#c678dd",
    "constant.language.boolean.false.xell": "#c678dd",
    "constant.language.none.xell": "#c678dd",
    "constant.numeric.float.xell": "#d19a66",
    "constant.numeric.integer.xell": "#d19a66",
    "string.quoted.double.xell": "#98c379",
    "string.interpolation.xell": "#d19a66",
    "punctuation.section.interpolation.begin.xell": "#c678dd",
    "punctuation.section.interpolation.end.xell": "#c678dd",
    "entity.name.function.call.xell": "#00ffff",
    "entity.name.function.definition.xell": "#00ffff",
    "entity.name.function.method.xell": "#00ffff",
    "support.function.builtin.xell": "#00ffff",
    "support.function.builtin.os.xell": "#00ffff",
    "support.function.math.xell": "#00ffff",
    "support.type.conversion.xell": "#008080",
    # Conditional: if, elif, else, incase — warm red
    "keyword.control.conditional.xell": "#e06c75",
    # Loop: for, while, loop, in — slightly different red
    "keyword.control.loop.xell": "#e06c75",
    # Control flow jumps: break, continue — bold red
    "keyword.control.flow.xell": "#ef596f",
    # Error handling: try, catch, finally — orange/amber
    "keyword.control.trycatch.xell": "#d19a66",
    # Import: bring, from, as — magenta/pink
    "keyword.control.import.xell": "#c678dd",
    # Module: module, export, requires — bright purple
    "keyword.control.module.xell": "#b267e6",
    # Return: give — yellow
    "keyword.control.return.xell": "#e5c07b",
    # Generator: yield — yellow (same family as return)
    "keyword.control.yield.xell": "#e5c07b",
    # Async: async, await — sky blue
    "keyword.control.async.xell": "#61afef",
    # Function declaration: fn — yellow/gold
    "keyword.declaration.function.xell": "#e5c07b",
    # OOP declaration: class, struct, interface, abstract, mixin, enum — teal
    "keyword.declaration.type.xell": "#56b6c2",
    # OOP modifier: inherits, implements, with — lighter teal
    "keyword.other.modifier.xell": "#56b6c2",
    # Binding: let, be, immutable — soft blue
    "keyword.other.binding.xell": "#61afef",
    # Special: of — muted gold
    "keyword.other.special.xell": "#e5c07b",
    # Access modifiers: private, protected, public, static — storage orange
    "storage.modifier.xell": "#d19a66",
    "keyword.operator.arithmetic.xell": "#c678dd",
    "keyword.operator.assignment.xell": "#c678dd",
    "keyword.operator.comparison.xell": "#c678dd",
    "keyword.operator.comparison.word.xell": "#c678dd",
    "keyword.operator.logical.xell": "#c678dd",
    "keyword.operator.increment.xell": "#c678dd",
    "keyword.operator.access.xell": "#61afef",
    "punctuation.bracket.round.xell": "#abb2bf",
    "punctuation.bracket.square.xell": "#abb2bf",
    "punctuation.bracket.curly.xell": "#abb2bf",
    "punctuation.separator.colon.xell": "#abb2bf",
    "punctuation.separator.comma.xell": "#abb2bf",
    "punctuation.terminator.block.xell": "#abb2bf",
    "punctuation.terminator.statement.xell": "#abb2bf",
    "variable.other.xell": "#eeeeee",
    "variable.other.loop.xell": "#eeeeee",
    "variable.parameter.xell": "#eeeeee",
    "meta.function.definition.xell": "#eeeeee",
}

DEFAULT_STYLES = {
    "comment.line.number-sign.xell": "italic",
    "keyword.control.flow.xell": "bold",
    "keyword.control.loop.xell": "bold",
    "keyword.control.conditional.xell": "bold",
    "keyword.control.trycatch.xell": "bold",
    "keyword.control.import.xell": "bold",
    "keyword.control.module.xell": "bold",
    "keyword.control.return.xell": "bold",
    "keyword.control.yield.xell": "bold",
    "keyword.control.async.xell": "bold",
    "keyword.declaration.function.xell": "bold",
    "keyword.declaration.type.xell": "bold",
    "keyword.other.binding.xell": "bold",
    "keyword.other.modifier.xell": "bold italic",
    "keyword.other.special.xell": "bold",
    "keyword.operator.access.xell": "bold",
    "punctuation.section.interpolation.begin.xell": "bold",
    "punctuation.section.interpolation.end.xell": "bold",
}


def _tok(id_, scope, label, example):
    color = DEFAULT_COLORS.get(scope, "#eeeeee")
    style = DEFAULT_STYLES.get(scope, "")
    bold = "bold" in style
    italic = "italic" in style
    return {
        "id": id_,
        "scope": scope,
        "color": color,
        "bold": bold,
        "italic": italic,
        "label": label,
        "example": example,
    }


def build_token_data(kw_classes, builtin_cats):
    groups = []

    # --- Comments ---
    groups.append({
        "title": "Comments",
        "tokens": [
            _tok("block_arrow", "comment.block.arrow.xell", "--> block comment <--", "--> ... <--"),
            _tok("line_number_sign", "comment.line.number-sign.xell", "# line comment", "# note"),
            _tok("comment_begin", "punctuation.definition.comment.begin.xell", "Comment start (-->)", "-->"),
            _tok("comment_end", "punctuation.definition.comment.end.xell", "Comment end (<--)", "<--"),
        ]
    })

    # --- Literals & Constants ---
    groups.append({
        "title": "Literals & Constants",
        "tokens": [
            _tok("character_escape", "constant.character.escape.xell", "Escape chars (\\n, \\t)", "\\n"),
            _tok("boolean_true", "constant.language.boolean.true.xell", "Boolean true", "true"),
            _tok("boolean_false", "constant.language.boolean.false.xell", "Boolean false", "false"),
            _tok("language_none", "constant.language.none.xell", "none", "none"),
            _tok("numeric_float", "constant.numeric.float.xell", "Float numbers", "3.14"),
            _tok("numeric_int", "constant.numeric.integer.xell", "Integer numbers", "42"),
        ]
    })

    # --- Strings ---
    groups.append({
        "title": "Strings",
        "tokens": [
            _tok("string_double", "string.quoted.double.xell", "String literal", '"hello"'),
            _tok("interpolation", "string.interpolation.xell", "String interpolation", '"{name}"'),
            _tok("interpolation_begin", "punctuation.section.interpolation.begin.xell", "Interpolation start {", "{"),
            _tok("interpolation_end", "punctuation.section.interpolation.end.xell", "Interpolation end }", "}"),
        ]
    })

    # --- Functions ---
    groups.append({
        "title": "Functions",
        "tokens": [
            _tok("fn_call", "entity.name.function.call.xell", "Function call", "myFunc()"),
            _tok("fn_def", "entity.name.function.definition.xell", "Function definition name", "fn greet():"),
            _tok("fn_method", "entity.name.function.method.xell", "Method call", ".method()"),
        ]
    })

    # --- Built-in Functions (dynamically from categories) ---
    builtin_tokens = []
    core_names = []
    for cat in ("io", "util", "collection"):
        if cat in builtin_cats:
            core_names.extend(builtin_cats[cat])
    if core_names:
        builtin_tokens.append(
            _tok("builtin_core", "support.function.builtin.xell",
                 f"Core builtins ({', '.join(sorted(core_names))})",
                 ", ".join(sorted(core_names)[:5]) + ("..." if len(core_names) > 5 else ""))
        )
    if "type" in builtin_cats:
        builtin_tokens.append(
            _tok("builtin_type", "support.type.conversion.xell",
                 f"Type builtins ({', '.join(sorted(builtin_cats['type']))})",
                 ", ".join(sorted(builtin_cats["type"])))
        )
    if "math" in builtin_cats:
        builtin_tokens.append(
            _tok("builtin_math", "support.function.math.xell",
                 f"Math builtins ({', '.join(sorted(builtin_cats['math']))})",
                 ", ".join(sorted(builtin_cats["math"])))
        )
    if "os" in builtin_cats:
        builtin_tokens.append(
            _tok("builtin_os", "support.function.builtin.os.xell",
                 f"OS builtins ({', '.join(sorted(builtin_cats['os'])[:8])}...)",
                 ", ".join(sorted(builtin_cats["os"])[:5]) + "...")
        )
    for cat in sorted(builtin_cats.keys()):
        if cat not in ("io", "util", "collection", "type", "math", "os"):
            builtin_tokens.append(
                _tok(f"builtin_{cat}", f"support.function.{cat}.xell",
                     f"{cat.title()} builtins ({', '.join(sorted(builtin_cats[cat]))})",
                     ", ".join(sorted(builtin_cats[cat])[:3]))
            )
    if builtin_tokens:
        groups.append({"title": "Built-in Functions", "tokens": builtin_tokens})

    # --- Keywords (fine-grained categories) ---
    kw_tokens = []
    if kw_classes["fn_decl"]:
        kw_tokens.append(_tok("kw_fn", "keyword.declaration.function.xell",
                              f"Function declaration ({', '.join(kw_classes['fn_decl'])})",
                              " ".join(kw_classes["fn_decl"])))
    if kw_classes["return"]:
        kw_tokens.append(_tok("kw_return", "keyword.control.return.xell",
                              f"Return ({', '.join(kw_classes['return'])})",
                              " ".join(kw_classes["return"])))
    if kw_classes["conditional"]:
        kw_tokens.append(_tok("kw_conditional", "keyword.control.conditional.xell",
                              f"Conditional ({', '.join(kw_classes['conditional'])})",
                              " ".join(kw_classes["conditional"])))
    if kw_classes["loop"]:
        kw_tokens.append(_tok("kw_loop", "keyword.control.loop.xell",
                              f"Loop ({', '.join(kw_classes['loop'])})",
                              " ".join(kw_classes["loop"])))
    if kw_classes["control_flow"]:
        kw_tokens.append(_tok("kw_control_flow", "keyword.control.flow.xell",
                              f"Control flow ({', '.join(kw_classes['control_flow'])})",
                              " ".join(kw_classes["control_flow"])))
    if kw_classes["error"]:
        kw_tokens.append(_tok("kw_error", "keyword.control.trycatch.xell",
                              f"Error handling ({', '.join(kw_classes['error'])})",
                              " ".join(kw_classes["error"])))
    if kw_classes["binding"]:
        kw_tokens.append(_tok("kw_binding", "keyword.other.binding.xell",
                              f"Binding ({', '.join(kw_classes['binding'])})",
                              " ".join(kw_classes["binding"])))
    if kw_classes["import"]:
        kw_tokens.append(_tok("kw_import", "keyword.control.import.xell",
                              f"Import ({', '.join(kw_classes['import'])})",
                              " ".join(kw_classes["import"])))
    if kw_classes["module"]:
        kw_tokens.append(_tok("kw_module", "keyword.control.module.xell",
                              f"Module ({', '.join(kw_classes['module'])})",
                              " ".join(kw_classes["module"])))
    if kw_classes["oop_decl"]:
        kw_tokens.append(_tok("kw_oop_decl", "keyword.declaration.type.xell",
                              f"OOP declaration ({', '.join(kw_classes['oop_decl'])})",
                              " ".join(kw_classes["oop_decl"])))
    if kw_classes["oop_modifier"]:
        kw_tokens.append(_tok("kw_oop_modifier", "keyword.other.modifier.xell",
                              f"OOP modifier ({', '.join(kw_classes['oop_modifier'])})",
                              " ".join(kw_classes["oop_modifier"])))
    if kw_classes.get("access"):
        kw_tokens.append(_tok("kw_access", "storage.modifier.xell",
                              f"Access ({', '.join(kw_classes['access'])})",
                              " ".join(kw_classes["access"])))
    if kw_classes["generator"]:
        kw_tokens.append(_tok("kw_generator", "keyword.control.yield.xell",
                              f"Generator ({', '.join(kw_classes['generator'])})",
                              " ".join(kw_classes["generator"])))
    if kw_classes["async"]:
        kw_tokens.append(_tok("kw_async", "keyword.control.async.xell",
                              f"Async ({', '.join(kw_classes['async'])})",
                              " ".join(kw_classes["async"])))
    if kw_classes["special"]:
        kw_tokens.append(_tok("kw_special", "keyword.other.special.xell",
                              f"Special ({', '.join(kw_classes['special'])})",
                              " ".join(kw_classes["special"])))
    if kw_tokens:
        groups.append({"title": "Keywords", "tokens": kw_tokens})

    # --- Operators ---
    groups.append({
        "title": "Operators",
        "tokens": [
            _tok("op_arithmetic", "keyword.operator.arithmetic.xell", "Arithmetic (+, -, *, /, %)", "+ - * / %"),
            _tok("op_assignment", "keyword.operator.assignment.xell", "Assignment (=)", "="),
            _tok("op_comparison", "keyword.operator.comparison.xell", "Comparison (==, !=, <, >, <=, >=)", "== !="),
            _tok("op_comparison_word", "keyword.operator.comparison.word.xell",
                 f"Word comparison ({', '.join(kw_classes.get('comparison', []))})",
                 " ".join(kw_classes.get("comparison", [])[:4])),
            _tok("op_logical", "keyword.operator.logical.xell",
                 f"Logical ({', '.join(kw_classes.get('logical', []))})",
                 " ".join(kw_classes.get("logical", []))),
            _tok("op_increment", "keyword.operator.increment.xell", "Increment/Decrement (++, --)", "++ --"),
            _tok("op_access", "keyword.operator.access.xell", "Map access (->)", "->"),
        ]
    })

    # --- Punctuation ---
    groups.append({
        "title": "Punctuation",
        "tokens": [
            _tok("bracket_round", "punctuation.bracket.round.xell", "Parentheses ( )", "( )"),
            _tok("bracket_square", "punctuation.bracket.square.xell", "Brackets [ ]", "[ ]"),
            _tok("bracket_curly", "punctuation.bracket.curly.xell", "Braces { }", "{ }"),
            _tok("sep_colon", "punctuation.separator.colon.xell", "Colon :", ":"),
            _tok("sep_comma", "punctuation.separator.comma.xell", "Comma ,", ","),
            _tok("term_block", "punctuation.terminator.block.xell", "Block end (;)", ";"),
            _tok("term_statement", "punctuation.terminator.statement.xell", "Statement end (.)", "."),
        ]
    })

    # --- Variables ---
    groups.append({
        "title": "Variables",
        "tokens": [
            _tok("var_other", "variable.other.xell", "Variable", "myVar"),
            _tok("var_loop", "variable.other.loop.xell", "Loop variable", "i"),
            _tok("var_param", "variable.parameter.xell", "Function parameter", "(param)"),
        ]
    })

    return groups


# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# 4a. GENERATE terminal_colors.json FOR XELL-TERMINAL
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

# Editor UI colors for the terminal IDE
EDITOR_UI_COLORS = {
    "editor_bg":         "#121212",
    "editor_fg":         "#cccccc",
    "gutter_bg":         "#1a1a1a",
    "gutter_fg":         "#555555",
    "gutter_active_fg":  "#cccccc",
    "line_highlight":    "#1e1e2e",
    "selection_bg":      "#264f78",
    "cursor":            "#ffffff",
    "scrollbar_bg":      "#1a1a1a",
    "scrollbar_thumb":   "#555555",
    "panel_border":      "#333333",
    "tab_active_bg":     "#1e1e2e",
    "tab_active_fg":     "#ffffff",
    "tab_inactive_bg":   "#121212",
    "tab_inactive_fg":   "#888888",
    "status_bar_bg":     "#007acc",
    "status_bar_fg":     "#ffffff",
    "popup_bg":          "#252526",
    "popup_fg":          "#cccccc",
    "popup_border":      "#454545",
    "popup_selected_bg": "#094771",
    "popup_selected_fg": "#ffffff",
    "error_fg":          "#f44747",
    "warning_fg":        "#cca700",
    "info_fg":           "#3794ff",
    "diff_added":        "#2ea04366",
    "diff_removed":      "#f8514966",
    "diff_modified":     "#0078d466",
    "search_match_bg":   "#515c6a",
    "search_active_bg":  "#eea825",
    "bracket_match_bg":  "#2d5b7e",
    "indent_guide":      "#333333",
    "indent_guide_active": "#555555",
}


def build_terminal_colors(kw_classes, builtin_cats):
    """
    Build terminal_colors.json — the single source of truth for all colors
    in the xell-terminal IDE. Contains both editor UI colors and per-scope
    token colors derived from the same DEFAULT_COLORS/DEFAULT_STYLES used
    for the VS Code extension.

    This file is consumed by xell-terminal's theme_loader.hpp.
    """

    # Build token_colors array from DEFAULT_COLORS + DEFAULT_STYLES
    token_colors = []
    for scope, color in DEFAULT_COLORS.items():
        style = DEFAULT_STYLES.get(scope, "")
        bold = "bold" in style
        italic = "italic" in style
        token_colors.append({
            "scope": scope,
            "fg": color,
            "bold": bold,
            "italic": italic,
        })

    # Also add wildcard entries for builtin categories that use support.function.*
    # so the terminal can match support.function.archive.xell etc.
    for cat in sorted(builtin_cats.keys()):
        scope = f"support.function.{cat}.xell"
        if scope not in DEFAULT_COLORS:
            token_colors.append({
                "scope": scope,
                "fg": "#00ffff",  # default builtin color
                "bold": False,
                "italic": False,
            })

    # Build the complete TokenType → scope mapping for C++ consumption
    # This maps each Xell TokenType enum name to its TextMate scope
    token_type_map = {}

    # Keywords → scopes (from kw_classes)
    KW_CLASS_TO_SCOPE = {
        "conditional":  "keyword.control.conditional.xell",
        "loop":         "keyword.control.loop.xell",
        "control_flow": "keyword.control.flow.xell",
        "fn_decl":      "keyword.declaration.function.xell",
        "return":       "keyword.control.return.xell",
        "error":        "keyword.control.trycatch.xell",
        "binding":      "keyword.other.binding.xell",
        "import":       "keyword.control.import.xell",
        "module":       "keyword.control.module.xell",
        "oop_decl":     "keyword.declaration.type.xell",
        "oop_modifier": "keyword.other.modifier.xell",
        "access":       "storage.modifier.xell",
        "generator":    "keyword.control.yield.xell",
        "async":        "keyword.control.async.xell",
        "logical":      "keyword.operator.logical.xell",
        "comparison":   "keyword.operator.comparison.word.xell",
        "constants":    "constant.language.boolean.true.xell",  # true/false/none all same color
        "special":      "keyword.other.special.xell",
    }

    kw_map = extract_keyword_map(read_file(TOKEN_HPP))
    for kw_str, enum_name in kw_map.items():
        for cls, kw_list in kw_classes.items():
            if kw_str in kw_list:
                scope = KW_CLASS_TO_SCOPE.get(cls, "keyword.other.special.xell")
                token_type_map[enum_name] = scope
                break

    # Operators → scopes
    OP_MAP = {
        "PLUS": "keyword.operator.arithmetic.xell",
        "MINUS": "keyword.operator.arithmetic.xell",
        "STAR": "keyword.operator.arithmetic.xell",
        "SLASH": "keyword.operator.arithmetic.xell",
        "PERCENT": "keyword.operator.arithmetic.xell",
        "PLUS_PLUS": "keyword.operator.increment.xell",
        "MINUS_MINUS": "keyword.operator.increment.xell",
        "EQUAL": "keyword.operator.assignment.xell",
        "PLUS_EQUAL": "keyword.operator.assignment.xell",
        "MINUS_EQUAL": "keyword.operator.assignment.xell",
        "STAR_EQUAL": "keyword.operator.assignment.xell",
        "SLASH_EQUAL": "keyword.operator.assignment.xell",
        "PERCENT_EQUAL": "keyword.operator.assignment.xell",
        "EQUAL_EQUAL": "keyword.operator.comparison.xell",
        "BANG_EQUAL": "keyword.operator.comparison.xell",
        "GREATER": "keyword.operator.comparison.xell",
        "LESS": "keyword.operator.comparison.xell",
        "GREATER_EQUAL": "keyword.operator.comparison.xell",
        "LESS_EQUAL": "keyword.operator.comparison.xell",
        "BANG": "keyword.operator.logical.xell",
        "ARROW": "keyword.operator.access.xell",
        "FAT_ARROW": "keyword.operator.access.xell",
    }
    token_type_map.update(OP_MAP)

    # Literals → scopes
    LITERAL_MAP = {
        "NUMBER": "constant.numeric.integer.xell",
        "IMAGINARY": "constant.numeric.float.xell",
        "STRING": "string.quoted.double.xell",
        "RAW_STRING": "string.quoted.double.xell",
        "BYTE_STRING": "string.quoted.double.xell",
        "TRUE_KW": "constant.language.boolean.true.xell",
        "FALSE_KW": "constant.language.boolean.false.xell",
        "NONE_KW": "constant.language.none.xell",
    }
    token_type_map.update(LITERAL_MAP)

    # Punctuation → scopes
    PUNCT_MAP = {
        "LPAREN": "punctuation.bracket.round.xell",
        "RPAREN": "punctuation.bracket.round.xell",
        "LBRACKET": "punctuation.bracket.square.xell",
        "RBRACKET": "punctuation.bracket.square.xell",
        "LBRACE": "punctuation.bracket.curly.xell",
        "RBRACE": "punctuation.bracket.curly.xell",
        "COMMA": "punctuation.separator.comma.xell",
        "COLON": "punctuation.separator.colon.xell",
        "SEMICOLON": "punctuation.terminator.block.xell",
        "DOT": "punctuation.terminator.statement.xell",
        "ELLIPSIS": "keyword.operator.access.xell",
        "AT": "keyword.other.special.xell",
    }
    token_type_map.update(PUNCT_MAP)

    # Special → scopes
    SPECIAL_MAP = {
        "IDENTIFIER": "variable.other.xell",
        "PIPE": "keyword.operator.arithmetic.xell",
        "AMP_AMP": "keyword.operator.logical.xell",
        "PIPE_PIPE": "keyword.operator.logical.xell",
        "TILDE": "keyword.operator.arithmetic.xell",
    }
    token_type_map.update(SPECIAL_MAP)

    return OrderedDict([
        ("version", 1),
        ("theme", "xell-default"),
        ("editor_ui", EDITOR_UI_COLORS),
        ("token_colors", token_colors),
        ("token_type_map", token_type_map),
    ])


# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# 4b. GENERATE xell.json SNIPPETS
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

# Module-level descriptions for generate snippet docs
MODULE_DESCRIPTIONS = {
    "json":     "JSON/CSV/TOML/YAML functions",
    "regex":    "Regular expression functions",
    "datetime": "Date and time functions",
    "fs":       "Advanced filesystem functions",
    "textproc": "Text processing functions",
    "process":  "Process management functions",
    "sysmon":   "System monitoring functions",
    "net":      "Networking functions (http, ping, dns, etc.)",
    "archive":  "Archive/compression functions",
}

# Short prefix aliases for common modules
MODULE_PREFIX_ALIAS = {
    "json": "bringjson",
    "regex": "bringregex",
    "datetime": "bringdt",
    "fs": "bringfs",
    "net": "bringnet",
    "textproc": "bringtxt",
    "process": "bringproc",
    "sysmon": "bringsys",
    "archive": "bringarc",
}


def build_snippets(kw_classes, builtin_cats, tier2_modules):
    """Build the complete xell.json snippets dict from extracted data."""

    snips = OrderedDict()

    # ── Core language constructs (static) ────────────────────────────

    # ── @convert dialect directive ────────────────────────────

    snips["Convert Dialect Decorator"] = {
        "prefix": "@convert",
        "body": '@convert "${1:dialect.xesy}"',
        "description": "Declare a dialect mapping file. Place at the top of a .xel file.",
    }

    snips["Function Definition"] = {
        "prefix": "fn",
        "body": ["fn ${1:name}(${2:params}) :", "    ${3:# body}", ";"],
        "description": "Define a new function",
    }
    snips["Function with Give"] = {
        "prefix": "fnr",
        "body": ["fn ${1:name}(${2:params}) :", "    give ${3:expression}", ";"],
        "description": "Define a function that gives back a value",
    }
    snips["If Statement"] = {
        "prefix": "if",
        "body": ["if ${1:condition} :", "    ${2:# body}", ";"],
        "description": "If conditional block",
    }
    snips["If-Else"] = {
        "prefix": "ife",
        "body": ["if ${1:condition} :", "    ${2:# then}", ";",
                  "else :", "    ${3:# else}", ";"],
        "description": "If-else conditional block",
    }
    snips["If-Elif-Else"] = {
        "prefix": "ifee",
        "body": ["if ${1:condition} :", "    ${2:# body}", ";",
                  "elif ${3:condition2} :", "    ${4:# body2}", ";",
                  "else :", "    ${5:# else}", ";"],
        "description": "If-elif-else block",
    }
    snips["For Loop"] = {
        "prefix": "for",
        "body": ["for ${1:item} in ${2:collection} :", "    ${3:# body}", ";"],
        "description": "For-in loop over collection",
    }
    snips["For Range Loop"] = {
        "prefix": "forr",
        "body": ["for ${1:i} in range(${2:n}) :", "    ${3:# body}", ";"],
        "description": "For loop with range(n)",
    }
    snips["While Loop"] = {
        "prefix": "while",
        "body": ["while ${1:condition} :", "    ${2:# body}", ";"],
        "description": "While loop",
    }
    snips["Assignment"] = {
        "prefix": "var",
        "body": "${1:name} = ${2:value}",
        "description": "Assign a value to a variable",
    }
    snips["Print"] = {
        "prefix": "pr",
        "body": "print(${1:expression})",
        "description": "Print a value",
    }
    snips["Give (Return)"] = {
        "prefix": "give",
        "body": "give ${1:expression}",
        "description": "Give back a value from a function",
    }

    # ── Bring / import ───────────────────────────────────────────────

    snips["Bring (Import)"] = {
        "prefix": "bring",
        "body": "bring ${1:name} from \"${2:./file.xel}\"",
        "description": "Bring names from another file",
    }
    snips["Bring All"] = {
        "prefix": "bringa",
        "body": "bring * from \"${1:./file.xel}\"",
        "description": "Bring all names from another file",
    }
    snips["Bring As"] = {
        "prefix": "bringas",
        "body": "bring ${1:name} from \"${2:./file.xel}\" as ${3:alias}",
        "description": "Bring a name with an alias",
    }

    # Dynamic: module bring with choice list from Tier 2 modules
    if tier2_modules:
        mod_choice = "|".join(sorted(tier2_modules))
        snips["Bring Module"] = {
            "prefix": "bringmod",
            "body": f"bring * from \"${{1|{mod_choice}|}}\"",
            "description": "Bring all functions from a built-in module",
        }
        snips["Bring Module Selective"] = {
            "prefix": "bringsel",
            "body": f"bring ${{2:func_name}} from \"${{1|{mod_choice}|}}\"",
            "description": "Bring specific function from a built-in module",
        }

    # Dynamic: per-module bring snippets
    for mod in sorted(tier2_modules):
        alias = MODULE_PREFIX_ALIAS.get(mod, f"bring{mod}")
        desc = MODULE_DESCRIPTIONS.get(mod, f"{mod.title()} functions")
        title = f"Bring {mod.title()} Module"
        snips[title] = {
            "prefix": alias,
            "body": f"bring * from \"{mod}\"",
            "description": f"Bring {desc}",
        }

    # ── Data literals ────────────────────────────────────────────────

    snips["Map Literal"] = {
        "prefix": "map",
        "body": ["{", "    ${1:key}: ${2:value}", "}"],
        "description": "Create a map literal",
    }
    snips["List Literal"] = {
        "prefix": "list",
        "body": "[${1:items}]",
        "description": "Create a list literal",
    }

    # ── Shell / OS ───────────────────────────────────────────────────

    snips["Run Command"] = {
        "prefix": "run",
        "body": "run(\"${1:command}\")",
        "description": "Run an external command",
    }
    snips["Run Capture"] = {
        "prefix": "runc",
        "body": "${1:result} = run_capture(\"${2:command}\")",
        "description": "Run and capture output of a command",
    }

    # ── Dynamic: builtin function snippets ───────────────────────────
    # Generate a snippet for every builtin function that takes at least
    # one obvious string/value argument (heuristic: popular names).

    BUILTIN_SNIPPETS = {
        # name → (prefix, body, description)
        "mkdir":      ("mkdir",  "mkdir(\"${1:path}\")",                       "Create a directory"),
        "read":       ("readf",  "${1:content} = read(\"${2:file}\")",         "Read file contents"),
        "write":      ("writef", "write(\"${1:file}\", ${2:data})",            "Write data to a file"),
        "append":     ("appendf","append(\"${1:file}\", ${2:data})",           "Append data to a file"),
        "exists":     ("exists", "exists(\"${1:path}\")",                      "Check if path exists"),
        "typeof":     ("typeof", "typeof(${1:value})",                         "Get type of value"),
        "len":        ("len",    "len(${1:collection})",                       "Get length of collection"),
        "push":       ("push",   "push(${1:list}, ${2:item})",                "Push item to list"),
        "split":      ("split",  "split(${1:text}, \"${2:delimiter}\")",       "Split string"),
        "join":       ("join",   "join(${1:list}, \"${2:delimiter}\")",        "Join list into string"),
        "keys":       ("keys",   "keys(${1:map})",                            "Get map keys"),
        "values":     ("values", "values(${1:map})",                          "Get map values"),
        "sort":       ("sort",   "sort(${1:list})",                           "Sort a list"),
        "reverse":    ("rev",    "reverse(${1:list})",                        "Reverse a list"),
        "range":      ("range",  "range(${1:start}, ${2:end})",               "Generate a range"),
        "input":      ("input",  "${1:val} = input(\"${2:prompt}\")",          "Read user input"),
        "sleep":      ("sleep",  "sleep(${1:seconds})",                       "Pause execution"),
        "abs":        ("abs",    "abs(${1:number})",                          "Absolute value"),
        "round":      ("round",  "round(${1:number})",                        "Round a number"),
        "to_int":     ("toint",  "to_int(${1:value})",                        "Convert to integer"),
        "to_float":   ("tofloat","to_float(${1:value})",                      "Convert to float"),
        "to_str":     ("tostr",  "to_str(${1:value})",                        "Convert to string"),
    }

    all_builtin_names = set()
    for names in builtin_cats.values():
        all_builtin_names.update(names)

    for name, (prefix, body, desc) in sorted(BUILTIN_SNIPPETS.items()):
        if name in all_builtin_names:
            snips[f"Builtin: {name}"] = {
                "prefix": prefix,
                "body": body,
                "description": desc,
            }

    return snips


# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# 4c. GENERATE language_data.json FOR SERVER (completions, hover, diagnostics)
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

# Hover / signature info for builtins.
# Key = function name.  Generated code will merge these into language_data.json.
# If a builtin is NOT in this dict, it still gets a completion item — just no hover doc.
HOVER_DOCS: dict[str, dict] = {
    # I/O
    "print":       {"sig": "print(value1, value2, ...)", "detail": "Print one or more values to stdout, separated by spaces.", "params": ["value1 — first value to print", "... — additional values"]},
    "input":       {"sig": "input(prompt)", "detail": "Read a line of input from the user.", "params": ["prompt — prompt string"]},
    # Type / Introspection
    "len":         {"sig": "len(collection)", "detail": "Return the number of elements in a list, map, or string.", "params": ["collection — list, map, or string"]},
    "type":        {"sig": "type(value)", "detail": "Return the type name of value as a string.", "params": ["value — any value"]},
    "str":         {"sig": "str(value)", "detail": "Convert value to string.", "params": ["value — any value"]},
    "num":         {"sig": "num(value)", "detail": "Convert string to number.", "params": ["value — numeric string or number"]},
    "typeof":      {"sig": "typeof(value)", "detail": "Return the type name of value as a string.", "params": ["value — any value"]},
    "to_int":      {"sig": "to_int(value)", "detail": "Convert value to integer.", "params": ["value — any value"]},
    "to_float":    {"sig": "to_float(value)", "detail": "Convert value to float.", "params": ["value — any value"]},
    "to_str":      {"sig": "to_str(value)", "detail": "Convert value to string.", "params": ["value — any value"]},
    # Collections
    "push":        {"sig": "push(list, item)", "detail": "Append item to end of list. Mutates the list.", "params": ["list — the list", "item — value to append"]},
    "pop":         {"sig": "pop(list)", "detail": "Remove and return the last item from a list.", "params": ["list — the list"]},
    "keys":        {"sig": "keys(map)", "detail": "Return all keys of a map as a list.", "params": ["map — a map"]},
    "values":      {"sig": "values(map)", "detail": "Return all values of a map as a list.", "params": ["map — a map"]},
    "range":       {"sig": "range(start, end [, step])", "detail": "Generate a list of numbers from start to end (exclusive).", "params": ["start — starting number", "end — ending number (exclusive)", "step — increment (default 1)"]},
    "set":         {"sig": "set(collection, key, value)", "detail": "Set a value at a key/index in a map or list.", "params": ["collection — map or list", "key — key or index", "value — new value"]},
    "has":         {"sig": "has(map, key)", "detail": "Check if a key exists in a map.", "params": ["map — a map", "key — key to check"]},
    "assert":      {"sig": "assert(condition, message)", "detail": "Assert a condition is true. Throws error with message if false.", "params": ["condition — boolean expression", "message — error message on failure"]},
    "sort":        {"sig": "sort(list)", "detail": "Sort a list in ascending order.", "params": ["list — the list"]},
    "reverse":     {"sig": "reverse(list)", "detail": "Reverse a list in-place.", "params": ["list — the list"]},
    "slice":       {"sig": "slice(list, start [, end])", "detail": "Get a sub-list from start to end index.", "params": ["list — the list", "start — start index", "end — end index (exclusive)"]},
    "join":        {"sig": "join(list, delimiter)", "detail": "Join list elements into a string.", "params": ["list — list of strings", "delimiter — separator string"]},
    "split":       {"sig": "split(text, delimiter)", "detail": "Split string into a list.", "params": ["text — the string", "delimiter — separator"]},
    "map":         {"sig": "map(list, fn)", "detail": "Apply function to each element, return new list.", "params": ["list — the list", "fn — function to apply"]},
    "filter":      {"sig": "filter(list, fn)", "detail": "Keep elements where fn returns true.", "params": ["list — the list", "fn — predicate function"]},
    "reduce":      {"sig": "reduce(list, fn, initial)", "detail": "Reduce list to single value using accumulator.", "params": ["list — the list", "fn — (acc, item) function", "initial — initial accumulator value"]},
    "contains":    {"sig": "contains(collection, item)", "detail": "Check if list or string contains item.", "params": ["collection — list or string", "item — value to search"]},
    "index_of":    {"sig": "index_of(collection, item)", "detail": "Find index of first occurrence.", "params": ["collection — list or string", "item — value to find"]},
    # OS / Filesystem
    "mkdir":       {"sig": "mkdir(path)", "detail": "Create directory (and parents) recursively.", "params": ["path — directory path"]},
    "rm":          {"sig": "rm(path)", "detail": "Remove a file or directory (recursive).", "params": ["path — path to remove"]},
    "cp":          {"sig": "cp(source, dest)", "detail": "Copy a file or directory.", "params": ["source — source path", "dest — destination path"]},
    "mv":          {"sig": "mv(source, dest)", "detail": "Move/rename a file or directory.", "params": ["source — current path", "dest — new path"]},
    "exists":      {"sig": "exists(path)", "detail": "Check if a path exists.", "params": ["path — path to check"]},
    "is_file":     {"sig": "is_file(path)", "detail": "Check if path is a regular file.", "params": ["path — path to check"]},
    "is_dir":      {"sig": "is_dir(path)", "detail": "Check if path is a directory.", "params": ["path — path to check"]},
    "ls":          {"sig": "ls(path)", "detail": "List directory contents as list of names.", "params": ["path — directory path"]},
    "read":        {"sig": "read(path)", "detail": "Read entire file contents as string.", "params": ["path — file path"]},
    "write":       {"sig": "write(path, data)", "detail": "Write string data to file (overwrites).", "params": ["path — file path", "data — string content"]},
    "append":      {"sig": "append(path, data)", "detail": "Append string data to end of file.", "params": ["path — file path", "data — string content"]},
    "file_size":   {"sig": "file_size(path)", "detail": "Get file size in bytes.", "params": ["path — file path"]},
    "cwd":         {"sig": "cwd()", "detail": "Get current working directory as string."},
    "cd":          {"sig": "cd(path)", "detail": "Change the current working directory.", "params": ["path — new directory"]},
    "abspath":     {"sig": "abspath(path)", "detail": "Get absolute path from relative path.", "params": ["path — path"]},
    "basename":    {"sig": "basename(path)", "detail": "Get the file name portion of a path.", "params": ["path — file path"]},
    "dirname":     {"sig": "dirname(path)", "detail": "Get the directory portion of a path.", "params": ["path — file path"]},
    "ext":         {"sig": "ext(path)", "detail": "Get the file extension (including the dot).", "params": ["path — file path"]},
    "env_get":     {"sig": "env_get(name)", "detail": "Get environment variable value.", "params": ["name — variable name"]},
    "env_set":     {"sig": "env_set(name, value)", "detail": "Set environment variable.", "params": ["name — variable name", "value — string value"]},
    "env_unset":   {"sig": "env_unset(name)", "detail": "Unset an environment variable.", "params": ["name — variable name"]},
    "env_has":     {"sig": "env_has(name)", "detail": "Check if environment variable exists.", "params": ["name — variable name"]},
    "run":         {"sig": "run(command)", "detail": "Run an external command. Returns exit code.", "params": ["command — shell command string"]},
    "run_capture": {"sig": "run_capture(command)", "detail": "Run command and capture output. Returns map with exit_code, stdout, stderr.", "params": ["command — shell command string"]},
    "pid":         {"sig": "pid()", "detail": "Get the current process ID."},
    "sleep":       {"sig": "sleep(seconds)", "detail": "Pause execution for given seconds.", "params": ["seconds — duration"]},
    # Math
    "floor":       {"sig": "floor(x)", "detail": "Largest integer ≤ x.", "params": ["x — any number"]},
    "ceil":        {"sig": "ceil(x)", "detail": "Smallest integer ≥ x.", "params": ["x — any number"]},
    "round":       {"sig": "round(x [, decimals])", "detail": "Round x to nearest integer or to given decimal places.", "params": ["x — any number", "decimals — decimal places (optional)"]},
    "abs":         {"sig": "abs(x)", "detail": "Absolute value of x.", "params": ["x — any number"]},
    "mod":         {"sig": "mod(a, b)", "detail": "Modulo: remainder of a / b.", "params": ["a — dividend", "b — divisor"]},
    "sqrt":        {"sig": "sqrt(x)", "detail": "Square root of x.", "params": ["x — non-negative number"]},
    "pow":         {"sig": "pow(base, exp)", "detail": "Raise base to the power of exp.", "params": ["base — base number", "exp — exponent"]},
    "sin":         {"sig": "sin(x)", "detail": "Sine of x (radians).", "params": ["x — angle in radians"]},
    "cos":         {"sig": "cos(x)", "detail": "Cosine of x (radians).", "params": ["x — angle in radians"]},
    "tan":         {"sig": "tan(x)", "detail": "Tangent of x (radians).", "params": ["x — angle in radians"]},
    "log":         {"sig": "log(x)", "detail": "Natural logarithm of x.", "params": ["x — positive number"]},
    "log10":       {"sig": "log10(x)", "detail": "Base-10 logarithm of x.", "params": ["x — positive number"]},
    "max":         {"sig": "max(a, b)", "detail": "Return the larger of two values.", "params": ["a — first value", "b — second value"]},
    "min":         {"sig": "min(a, b)", "detail": "Return the smaller of two values.", "params": ["a — first value", "b — second value"]},
    "random":      {"sig": "random()", "detail": "Generate a random float between 0 and 1."},
    "random_int":  {"sig": "random_int(min, max)", "detail": "Generate a random integer in [min, max].", "params": ["min — minimum", "max — maximum"]},
    # String
    "upper":       {"sig": "upper(text)", "detail": "Convert string to uppercase.", "params": ["text — a string"]},
    "lower":       {"sig": "lower(text)", "detail": "Convert string to lowercase.", "params": ["text — a string"]},
    "trim":        {"sig": "trim(text)", "detail": "Remove leading/trailing whitespace.", "params": ["text — a string"]},
    "starts_with": {"sig": "starts_with(text, prefix)", "detail": "Check if string starts with prefix.", "params": ["text — a string", "prefix — prefix to check"]},
    "ends_with":   {"sig": "ends_with(text, suffix)", "detail": "Check if string ends with suffix.", "params": ["text — a string", "suffix — suffix to check"]},
    "replace":     {"sig": "replace(text, old, new)", "detail": "Replace all occurrences.", "params": ["text — a string", "old — search string", "new — replacement"]},
    "substr":      {"sig": "substr(text, start [, length])", "detail": "Get a substring.", "params": ["text — a string", "start — start index", "length — characters to extract"]},
    "char_at":     {"sig": "char_at(text, index)", "detail": "Get character at index.", "params": ["text — a string", "index — position"]},
}

# Keyword hover docs
KEYWORD_HOVER_DOCS: dict[str, dict] = {
    "fn":       {"sig": "fn name(params):", "detail": "Define a function. Body follows on indented lines, ends with ;"},
    "give":     {"sig": "give expression", "detail": "Give back a value from a function."},
    "if":       {"sig": "if condition:", "detail": "Conditional branch. Body indented, elif/else optional."},
    "elif":     {"sig": "elif condition:", "detail": "Else-if branch after an if statement."},
    "else":     {"sig": "else:", "detail": "Else branch — runs when no if/elif matched."},
    "for":      {"sig": "for item in collection:", "detail": "For loop over a list or range. Body indented, ends with ;"},
    "while":    {"sig": "while condition:", "detail": "While loop. Body indented, ends with ;"},
    "in":       {"sig": "for x in items:", "detail": "Iterator keyword — iterates over collection elements."},
    "bring":    {"sig": "bring name of module", "detail": "Import names from a module or file."},
    "from":     {"sig": 'from "dir" bring ...', "detail": "Specify search directory for file-based imports."},
    "as":       {"sig": "bring X as alias", "detail": "Alias an imported name."},
    "module":   {"sig": "module name:", "detail": "Define a module with exported members."},
    "export":   {"sig": "export fn/class/var", "detail": "Mark a declaration as exported from a module."},
    "requires": {"sig": "requires module_name", "detail": "Declare a module dependency."},
    "class":    {"sig": "class Name:", "detail": "Define a class with methods and fields."},
    "struct":   {"sig": "struct Name:", "detail": "Define a struct (value type with fields)."},
    "enum":     {"sig": "enum Name:", "detail": "Define an enumeration."},
    "inherits": {"sig": "class Child inherits Parent:", "detail": "Inherit from a parent class."},
    "interface":{"sig": "interface Name:", "detail": "Define an interface with required methods."},
    "implements":{"sig": "class X implements IFace:", "detail": "Implement an interface."},
    "abstract": {"sig": "abstract class Name:", "detail": "Define an abstract class."},
    "mixin":    {"sig": "mixin Name:", "detail": "Define a mixin for shared behavior."},
    "with":     {"sig": "class X with MixinA:", "detail": "Include a mixin into a class."},
    "try":      {"sig": "try:", "detail": "Begin a try-catch block for error handling."},
    "catch":    {"sig": "catch e:", "detail": "Handle an error caught from the try block."},
    "finally":  {"sig": "finally:", "detail": "Code that always runs after try/catch."},
    "break":    {"sig": "break", "detail": "Exit the current loop."},
    "continue": {"sig": "continue", "detail": "Skip to the next iteration of the loop."},
    "incase":   {"sig": "incase value:", "detail": "Switch/match statement."},
    "let":      {"sig": "let name = value", "detail": "Declare an immutable binding."},
    "be":       {"sig": "let x be value", "detail": "Alternative immutable binding syntax."},
    "loop":     {"sig": "loop:", "detail": "Infinite loop. Break to exit."},
    "yield":    {"sig": "yield value", "detail": "Yield a value from a generator function."},
    "async":    {"sig": "async fn name():", "detail": "Declare an async function."},
    "await":    {"sig": "await expression", "detail": "Wait for an async operation to complete."},
    "immutable":{"sig": "immutable name = value", "detail": "Declare an immutable variable."},
    "private":  {"sig": "private fn/field", "detail": "Restrict access to class internals only."},
    "protected":{"sig": "protected fn/field", "detail": "Restrict access to class and subclasses."},
    "public":   {"sig": "public fn/field", "detail": "Allow access from anywhere."},
    "static":   {"sig": "static fn/field", "detail": "Declare a class-level (not instance) member."},
    "true":     {"sig": "true", "detail": "Boolean constant: true."},
    "false":    {"sig": "false", "detail": "Boolean constant: false."},
    "none":     {"sig": "none", "detail": "The absence of a value."},
    "and":      {"sig": "a and b", "detail": "Logical AND operator."},
    "or":       {"sig": "a or b", "detail": "Logical OR operator."},
    "not":      {"sig": "not a", "detail": "Logical negation operator."},
    "is":       {"sig": "a is b", "detail": "Equality check (alias for ==)."},
    "eq":       {"sig": "a eq b", "detail": "Equal (alias for ==)."},
    "ne":       {"sig": "a ne b", "detail": "Not equal (alias for !=)."},
    "gt":       {"sig": "a gt b", "detail": "Greater than (alias for >)."},
    "lt":       {"sig": "a lt b", "detail": "Less than (alias for <)."},
    "ge":       {"sig": "a ge b", "detail": "Greater or equal (alias for >=)."},
    "le":       {"sig": "a le b", "detail": "Less or equal (alias for <=)."},
    "of":       {"sig": "bring X of module", "detail": "Specify the module source for an import."},
}

# Descriptions for keyword completion items
KEYWORD_COMPLETION_DESC: dict[str, str] = {
    "fn": "Define a function", "give": "Give back a value from function",
    "if": "Conditional statement", "elif": "Else-if branch", "else": "Else branch",
    "for": "For loop", "while": "While loop", "in": "Iterator keyword",
    "break": "Break out of loop", "continue": "Skip to next iteration",
    "try": "Try-catch block", "catch": "Catch an error", "finally": "Finally block",
    "incase": "Switch/match statement", "let": "Immutable binding", "be": "Let-be binding",
    "loop": "Infinite loop",
    "bring": "Import from module or file", "from": "Import source directory",
    "as": "Import alias", "module": "Define a module", "export": "Export from module",
    "requires": "Module dependency",
    "enum": "Define an enumeration",
    "struct": "Define a struct", "class": "Define a class",
    "inherits": "Class inheritance", "immutable": "Immutable variable",
    "private": "Private access", "protected": "Protected access",
    "public": "Public access", "static": "Static member",
    "interface": "Define an interface", "implements": "Implement interface",
    "abstract": "Abstract class", "mixin": "Define a mixin", "with": "Include mixin",
    "yield": "Yield from generator", "async": "Async function", "await": "Await async",
    "true": "Boolean true", "false": "Boolean false", "none": "None value",
    "and": "Logical AND", "or": "Logical OR", "not": "Logical NOT",
    "is": "Equality check", "eq": "Equal", "ne": "Not equal",
    "gt": "Greater than", "lt": "Less than", "ge": "Greater or equal", "le": "Less or equal",
    "of": "Module source",
}


def build_language_data(kw_classes, builtin_cats, keywords):
    """Build the language_data.json consumed by the TypeScript language server."""
    data = OrderedDict()

    # All keywords with classification and completion kind
    kw_items = []
    constant_kws = set(kw_classes.get("constants", []))
    for kw in sorted(keywords):
        kind = "Constant" if kw in constant_kws else "Keyword"
        desc = KEYWORD_COMPLETION_DESC.get(kw, f"{kw} keyword")
        hover = KEYWORD_HOVER_DOCS.get(kw)
        item: dict = {"name": kw, "kind": kind, "detail": desc}
        if hover:
            item["hover"] = hover
        kw_items.append(item)
    data["keywords"] = kw_items

    # All builtins with category and completion kind
    builtin_items = []
    for cat in sorted(builtin_cats.keys()):
        for name in sorted(builtin_cats[cat]):
            item: dict = {"name": name, "category": cat, "kind": "Function"}
            hover = HOVER_DOCS.get(name)
            if hover:
                item["hover"] = hover
            builtin_items.append(item)
    data["builtins"] = builtin_items

    # Block-opening keywords for indent rules (keywords that precede a colon to open a block)
    BLOCK_KEYWORDS = {
        "fn", "if", "elif", "else", "for", "while", "loop", "try", "catch", "finally",
        "incase", "class", "struct", "enum", "interface", "abstract", "mixin", "module",
    }
    data["blockKeywords"] = sorted(BLOCK_KEYWORDS & set(keywords))

    # All keyword names as a set (for diagnostics keyword filtering)
    data["allKeywordNames"] = sorted(keywords)

    # @convert dialect system — canonical name lists for .xesy generation
    all_builtin_names = sorted({n for names in builtin_cats.values() for n in names})
    data["convertDirective"] = {
        "syntax": '@convert "path.xesy"',
        "description": "Declare a .xesy dialect mapping file at the top of a .xel file. "
                       "The file maps canonical Xell keywords/builtins to custom names. "
                       "Empty values mean 'keep canonical'.",
        "canonicalKeywords": sorted(keywords),
        "canonicalBuiltins": all_builtin_names,
    }

    return data


def build_language_config(block_keywords):
    """Build the language-configuration.json with dynamic block keywords."""
    # Build the indent pattern from block keywords
    kw_alt = "|".join(sorted(block_keywords, key=lambda w: (-len(w), w)))

    config = OrderedDict()
    config["comments"] = {
        "lineComment": "#",
        "blockComment": ["-->", "<--"]
    }
    config["brackets"] = [["(", ")"], ["{", "}"], ["[", "]"]]
    config["autoClosingPairs"] = [
        {"open": "(", "close": ")"},
        {"open": "{", "close": "}"},
        {"open": "[", "close": "]"},
        {"open": "\"", "close": "\"", "notIn": ["string"]}
    ]
    config["surroundingPairs"] = [
        {"open": "(", "close": ")"},
        {"open": "{", "close": "}"},
        {"open": "[", "close": "]"},
        {"open": "\"", "close": "\""}
    ]
    config["folding"] = {
        "markers": {
            "start": "^\\s*.*:\\s*$",
            "end": "^\\s*;\\s*$"
        }
    }
    config["indentationRules"] = {
        "increaseIndentPattern": f"^\\s*({kw_alt})\\b.*:\\s*(#.*)?$",
        "decreaseIndentPattern": "^\\s*(;|elif\\b|else\\b).*"
    }
    config["onEnterRules"] = [
        {
            "beforeText": f"^\\s*({kw_alt})\\b.*:\\s*(#.*)?$",
            "action": {"indent": "indent"}
        },
        {
            "beforeText": "^\\s*;\\s*$",
            "action": {"indent": "outdent"}
        }
    ]
    config["wordPattern"] = "[a-zA-Z_][a-zA-Z0-9_]*"
    return config


# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# 5. AUTO-INSTALL EXTENSION
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

def install_extension():
    """Build, package, and install the VS Code extension."""
    ext_dir = VSCODE_DIR

    def run(cmd, cwd=None):
        print(f"  → {cmd}")
        result = subprocess.run(cmd, shell=True, cwd=cwd or str(ext_dir),
                                capture_output=True, text=True)
        if result.returncode != 0:
            print(f"  ✗ FAILED: {result.stderr.strip()}")
            return False
        return True

    print("\n[install] Building and installing VS Code extension...")

    # 1. npm install
    print("[install] Step 1: npm install")
    if not run("npm install"):
        return False

    # 2. TypeScript compile
    print("[install] Step 2: TypeScript compile")
    if not run("npx tsc -b"):
        return False

    # 3. Convert SVG icon to PNG if needed
    icon_svg = ext_dir / "images" / "icon.svg"
    icon_png = ext_dir / "images" / "icon.png"
    if icon_svg.exists() and (not icon_png.exists() or
            icon_svg.stat().st_mtime > icon_png.stat().st_mtime):
        print("[install] Step 3: Converting icon SVG → PNG")
        if shutil.which("convert"):
            run(f'convert -background none -density 300 "{icon_svg}" -resize 256x256 "{icon_png}"',
                cwd=str(ext_dir / "images"))
        else:
            print("  ⚠ ImageMagick 'convert' not found — skipping PNG generation")

    # 4. Package with vsce
    print("[install] Step 4: Packaging extension")
    # Remove old .vsix files
    for old in ext_dir.glob("*.vsix"):
        old.unlink()
    if not run("npx @vscode/vsce package --allow-missing-repository"):
        return False

    # 5. Install into VS Code
    vsix_files = list(ext_dir.glob("*.vsix"))
    if not vsix_files:
        print("  ✗ No .vsix file found after packaging!")
        return False
    vsix = vsix_files[0]
    print(f"[install] Step 5: Installing {vsix.name}")
    code_cmd = "code"
    if not shutil.which(code_cmd):
        code_cmd = "code-insiders"
    if not shutil.which(code_cmd):
        print("  ✗ 'code' command not found — install manually with:")
        print(f"    code --install-extension {vsix}")
        return False
    if not run(f'{code_cmd} --install-extension "{vsix}"'):
        return False

    print(f"\n[install] ✅ Extension installed! Reload VS Code to activate.")
    return True


# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# 5b. GENERATE .xesy TEMPLATE (dialect mapping)
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

def build_xesy_template(keywords, builtin_cats):
    """
    Build a .xesy template JSON string from the dynamically extracted
    keywords and builtins. This matches the output of the C++ --gen_xesy
    command but is always in sync with the C++ sources.
    """
    lines = []
    lines.append("{")
    lines.append('  "_meta": {')
    lines.append('    "dialect_name": "My Dialect",')
    lines.append('    "author": "",')
    lines.append('    "xell_version": "0.1.0",')
    lines.append('    "description": "Custom keyword mapping for Xell. Fill in values to map canonical keywords to your dialect."')
    lines.append("  },")
    lines.append("")

    # Collect all builtins in a flat sorted list
    all_builtins = sorted({n for names in builtin_cats.values() for n in names})

    lines.append('  "_comment_keywords": "=== Language Keywords ===",')
    for i, kw in enumerate(sorted(keywords)):
        trailing = "," if (i + 1 < len(keywords) or all_builtins) else ""
        lines.append(f'  "{kw}": ""{trailing}')

    lines.append("")
    lines.append('  "_comment_builtins": "=== Built-in Functions ===",')
    for i, name in enumerate(all_builtins):
        trailing = "," if i + 1 < len(all_builtins) else ""
        lines.append(f'  "{name}": ""{trailing}')

    lines.append("}")
    return "\n".join(lines) + "\n"


def gen_xesy_to_file(output_path, keywords, builtin_cats):
    """Write a .xesy template to the given path (CLI --gen_xesy mode)."""
    content = build_xesy_template(keywords, builtin_cats)
    out = Path(output_path)
    if out.suffix != ".xesy":
        out = out.with_suffix(".xesy")
    out.parent.mkdir(parents=True, exist_ok=True)
    with open(out, "w") as f:
        f.write(content)
    print(f"[gen_grammar] ✓ Generated .xesy template: {out}")
    print(f"  Fill in values with your dialect words, then use:")
    print(f'    @convert "{out.name}"    (at the top of your .xel file)')


# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
# 6. MAIN
# ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

def main():
    check_mode = "--check" in sys.argv
    install_mode = "--install" in sys.argv
    gen_xesy_mode = "--gen_xesy" in sys.argv

    if not TOKEN_HPP.exists():
        print(f"ERROR: {TOKEN_HPP} not found!")
        sys.exit(1)

    token_src = read_file(TOKEN_HPP)
    keywords = extract_keywords_dynamic(token_src)
    kw_classes = classify_keywords(token_src)
    builtin_cats = extract_all_builtins()
    tier2_modules = extract_tier2_modules()

    all_builtins = []
    for cat, names in sorted(builtin_cats.items()):
        all_builtins.extend(names)

    # --gen_xesy mode: just generate a .xesy template and exit
    if gen_xesy_mode:
        # Find the output path argument (next arg after --gen_xesy, or default)
        try:
            idx = sys.argv.index("--gen_xesy")
            output = sys.argv[idx + 1] if idx + 1 < len(sys.argv) and not sys.argv[idx + 1].startswith("--") else "dialect.xesy"
        except (ValueError, IndexError):
            output = "dialect.xesy"
        gen_xesy_to_file(output, keywords, builtin_cats)
        sys.exit(0)

    print(f"[gen_grammar] Extracted from C++ sources:")
    print(f"  Keywords:  {len(keywords)} → {keywords}")
    print(f"  Builtins:  {len(all_builtins)} across {len(builtin_cats)} categories")
    for cat, names in sorted(builtin_cats.items()):
        print(f"    {cat:12s}: {names}")
    print(f"  Tier 2 modules: {tier2_modules}")

    grammar = build_tmlanguage(kw_classes, builtin_cats)
    grammar_json = json.dumps(grammar, indent=2) + "\n"

    token_data = build_token_data(kw_classes, builtin_cats)
    token_json = json.dumps(token_data, indent=2) + "\n"

    snippets = build_snippets(kw_classes, builtin_cats, tier2_modules)
    snippets_json = json.dumps(snippets, indent=2) + "\n"

    lang_data = build_language_data(kw_classes, builtin_cats, keywords)
    lang_data_json = json.dumps(lang_data, indent=2) + "\n"

    lang_config = build_language_config(lang_data["blockKeywords"])
    lang_config_json = json.dumps(lang_config, indent=2) + "\n"

    terminal_colors = build_terminal_colors(kw_classes, builtin_cats)
    terminal_colors_json = json.dumps(terminal_colors, indent=2) + "\n"

    xesy_template = build_xesy_template(keywords, builtin_cats)

    if check_mode:
        ok = True
        for path, new_content, name in [
            (TMLANG_OUT, grammar_json, "tmLanguage"),
            (TOKEN_DATA_OUT, token_json, "token_data"),
            (SNIPPETS_OUT, snippets_json, "snippets"),
            (LANG_DATA_OUT, lang_data_json, "language_data"),
            (LANG_CONFIG_OUT, lang_config_json, "language-configuration"),
            (TERMINAL_COLORS_OUT, terminal_colors_json, "terminal_colors"),
            (XESY_TEMPLATE_OUT, xesy_template, "dialect_template.xesy"),
        ]:
            if path.exists():
                existing = read_file(path)
                if existing == new_content:
                    print(f"[gen_grammar] ✓ {name} is up-to-date")
                else:
                    print(f"[gen_grammar] ✗ {name} is out-of-date — run gen_xell_grammar.py")
                    ok = False
            else:
                print(f"[gen_grammar] ✗ {path} not found")
                ok = False
        sys.exit(0 if ok else 1)
    else:
        TMLANG_OUT.parent.mkdir(parents=True, exist_ok=True)
        with open(TMLANG_OUT, "w") as f:
            f.write(grammar_json)
        print(f"[gen_grammar] ✓ Wrote {TMLANG_OUT}")

        TOKEN_DATA_OUT.parent.mkdir(parents=True, exist_ok=True)
        with open(TOKEN_DATA_OUT, "w") as f:
            f.write(token_json)
        print(f"[gen_grammar] ✓ Wrote {TOKEN_DATA_OUT}")

        SNIPPETS_OUT.parent.mkdir(parents=True, exist_ok=True)
        with open(SNIPPETS_OUT, "w") as f:
            f.write(snippets_json)
        print(f"[gen_grammar] ✓ Wrote {SNIPPETS_OUT}")

        LANG_DATA_OUT.parent.mkdir(parents=True, exist_ok=True)
        with open(LANG_DATA_OUT, "w") as f:
            f.write(lang_data_json)
        print(f"[gen_grammar] ✓ Wrote {LANG_DATA_OUT}")

        with open(LANG_CONFIG_OUT, "w") as f:
            f.write(lang_config_json)
        print(f"[gen_grammar] ✓ Wrote {LANG_CONFIG_OUT}")

        TERMINAL_COLORS_OUT.parent.mkdir(parents=True, exist_ok=True)
        with open(TERMINAL_COLORS_OUT, "w") as f:
            f.write(terminal_colors_json)
        print(f"[gen_grammar] ✓ Wrote {TERMINAL_COLORS_OUT}")

        XESY_TEMPLATE_OUT.parent.mkdir(parents=True, exist_ok=True)
        with open(XESY_TEMPLATE_OUT, "w") as f:
            f.write(xesy_template)
        print(f"[gen_grammar] ✓ Wrote {XESY_TEMPLATE_OUT}")

        print(f"\n[gen_grammar] Done! Generated grammar with {len(keywords)} keywords, "
              f"{len(all_builtins)} builtins, {len(snippets)} snippets, "
              f"{len(lang_data['builtins'])} completion entries, "
              f"{len(terminal_colors['token_type_map'])} terminal color mappings.")

        if install_mode:
            if not install_extension():
                sys.exit(1)


if __name__ == "__main__":
    main()
