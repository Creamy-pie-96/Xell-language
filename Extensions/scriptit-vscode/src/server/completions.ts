// ═══════════════════════════════════════════════════════════
// ScriptIt Completion Items
// ═══════════════════════════════════════════════════════════

import { CompletionItem, CompletionItemKind } from 'vscode-languageserver/node';

// ── Keywords ─────────────────────────────────────────────

export const SCRIPTIT_KEYWORDS: CompletionItem[] = [
    { label: 'fn', kind: CompletionItemKind.Keyword, detail: 'Define a function', data: 'kw_fn' },
    { label: 'give', kind: CompletionItemKind.Keyword, detail: 'Return a value from function', data: 'kw_give' },
    { label: 'var', kind: CompletionItemKind.Keyword, detail: 'Declare a variable', data: 'kw_var' },
    { label: 'if', kind: CompletionItemKind.Keyword, detail: 'Conditional statement', data: 'kw_if' },
    { label: 'elif', kind: CompletionItemKind.Keyword, detail: 'Else-if branch', data: 'kw_elif' },
    { label: 'else', kind: CompletionItemKind.Keyword, detail: 'Else branch', data: 'kw_else' },
    { label: 'for', kind: CompletionItemKind.Keyword, detail: 'For loop', data: 'kw_for' },
    { label: 'while', kind: CompletionItemKind.Keyword, detail: 'While loop', data: 'kw_while' },
    { label: 'in', kind: CompletionItemKind.Keyword, detail: 'Iterator keyword', data: 'kw_in' },
    { label: 'range', kind: CompletionItemKind.Keyword, detail: 'Range generator', data: 'kw_range' },
    { label: 'from', kind: CompletionItemKind.Keyword, detail: 'Range start', data: 'kw_from' },
    { label: 'to', kind: CompletionItemKind.Keyword, detail: 'Range end', data: 'kw_to' },
    { label: 'step', kind: CompletionItemKind.Keyword, detail: 'Range step size', data: 'kw_step' },
    { label: 'pass', kind: CompletionItemKind.Keyword, detail: 'No-op placeholder', data: 'kw_pass' },
    { label: 'of', kind: CompletionItemKind.Keyword, detail: 'Method-of syntax', data: 'kw_of' },
    { label: 'is', kind: CompletionItemKind.Keyword, detail: 'Identity/type check', data: 'kw_is' },
    { label: 'points', kind: CompletionItemKind.Keyword, detail: 'Pointer comparison', data: 'kw_points' },
    { label: 'not', kind: CompletionItemKind.Keyword, detail: 'Logical negation', data: 'kw_not' },
    { label: 'and', kind: CompletionItemKind.Keyword, detail: 'Logical AND', data: 'kw_and' },
    { label: 'or', kind: CompletionItemKind.Keyword, detail: 'Logical OR', data: 'kw_or' },
    { label: 'with', kind: CompletionItemKind.Keyword, detail: 'Context manager', data: 'kw_with' },
    { label: 'as', kind: CompletionItemKind.Keyword, detail: 'Alias in context manager', data: 'kw_as' },
    { label: 'new', kind: CompletionItemKind.Keyword, detail: 'English-style creation', data: 'kw_new' },
    { label: 'let', kind: CompletionItemKind.Keyword, detail: 'English-style assignment', data: 'kw_let' },
    { label: 'be', kind: CompletionItemKind.Keyword, detail: 'English-style value', data: 'kw_be' },
    { label: 'are', kind: CompletionItemKind.Keyword, detail: 'English-style list', data: 'kw_are' },
    { label: 'True', kind: CompletionItemKind.Constant, detail: 'Boolean true', data: 'const_true' },
    { label: 'False', kind: CompletionItemKind.Constant, detail: 'Boolean false', data: 'const_false' },
    { label: 'None', kind: CompletionItemKind.Constant, detail: 'None value', data: 'const_none' },
];

// ── Builtin Functions ────────────────────────────────────

export const SCRIPTIT_BUILTINS: CompletionItem[] = [
    { label: 'print', kind: CompletionItemKind.Function, detail: 'Print values to stdout', data: 'fn_print' },
    { label: 'pprint', kind: CompletionItemKind.Function, detail: 'Pretty-print a value', data: 'fn_pprint' },
    { label: 'input', kind: CompletionItemKind.Function, detail: 'Read user input', data: 'fn_input' },
    { label: 'len', kind: CompletionItemKind.Function, detail: 'Get length of collection', data: 'fn_len' },
    { label: 'type', kind: CompletionItemKind.Function, detail: 'Get type name of value', data: 'fn_type' },
    { label: 'isinstance', kind: CompletionItemKind.Function, detail: 'Check type of value', data: 'fn_isinstance' },
    { label: 'repr', kind: CompletionItemKind.Function, detail: 'Get string representation', data: 'fn_repr' },
    { label: 'read', kind: CompletionItemKind.Function, detail: 'Read file contents', data: 'fn_read' },
    { label: 'readLine', kind: CompletionItemKind.Function, detail: 'Read file as list of lines', data: 'fn_readline' },
    { label: 'write', kind: CompletionItemKind.Function, detail: 'Write data to file', data: 'fn_write' },
    { label: 'open', kind: CompletionItemKind.Function, detail: 'Open a file handle', data: 'fn_open' },
    { label: 'close', kind: CompletionItemKind.Function, detail: 'Close a file handle', data: 'fn_close' },
    { label: 'range_list', kind: CompletionItemKind.Function, detail: 'Create list from range', data: 'fn_range_list' },
    { label: 'append', kind: CompletionItemKind.Function, detail: 'Append item to list', data: 'fn_append' },
    { label: 'pop', kind: CompletionItemKind.Function, detail: 'Pop last item from list', data: 'fn_pop' },
    { label: 'sum', kind: CompletionItemKind.Function, detail: 'Sum of list elements', data: 'fn_sum' },
    { label: 'sorted', kind: CompletionItemKind.Function, detail: 'Return sorted copy of list', data: 'fn_sorted' },
    { label: 'reversed', kind: CompletionItemKind.Function, detail: 'Return reversed copy', data: 'fn_reversed' },
    { label: 'all', kind: CompletionItemKind.Function, detail: 'True if all elements truthy', data: 'fn_all' },
    { label: 'any', kind: CompletionItemKind.Function, detail: 'True if any element truthy', data: 'fn_any' },
    { label: 'enumerate', kind: CompletionItemKind.Function, detail: 'Enumerate with index', data: 'fn_enumerate' },
    { label: 'zip', kind: CompletionItemKind.Function, detail: 'Zip two lists together', data: 'fn_zip' },
    { label: 'map', kind: CompletionItemKind.Function, detail: 'Map function over list', data: 'fn_map' },
];

// ── Math Functions ───────────────────────────────────────

export const SCRIPTIT_MATH: CompletionItem[] = [
    { label: 'sin', kind: CompletionItemKind.Function, detail: 'Sine (radians)', data: 'math_sin' },
    { label: 'cos', kind: CompletionItemKind.Function, detail: 'Cosine (radians)', data: 'math_cos' },
    { label: 'tan', kind: CompletionItemKind.Function, detail: 'Tangent (radians)', data: 'math_tan' },
    { label: 'asin', kind: CompletionItemKind.Function, detail: 'Arc sine', data: 'math_asin' },
    { label: 'acos', kind: CompletionItemKind.Function, detail: 'Arc cosine', data: 'math_acos' },
    { label: 'atan', kind: CompletionItemKind.Function, detail: 'Arc tangent', data: 'math_atan' },
    { label: 'log', kind: CompletionItemKind.Function, detail: 'Natural logarithm', data: 'math_log' },
    { label: 'log2', kind: CompletionItemKind.Function, detail: 'Base-2 logarithm', data: 'math_log2' },
    { label: 'log10', kind: CompletionItemKind.Function, detail: 'Base-10 logarithm', data: 'math_log10' },
    { label: 'sqrt', kind: CompletionItemKind.Function, detail: 'Square root', data: 'math_sqrt' },
    { label: 'abs', kind: CompletionItemKind.Function, detail: 'Absolute value', data: 'math_abs' },
    { label: 'ceil', kind: CompletionItemKind.Function, detail: 'Ceiling (round up)', data: 'math_ceil' },
    { label: 'floor', kind: CompletionItemKind.Function, detail: 'Floor (round down)', data: 'math_floor' },
    { label: 'round', kind: CompletionItemKind.Function, detail: 'Round to nearest', data: 'math_round' },
    { label: 'cot', kind: CompletionItemKind.Function, detail: 'Cotangent', data: 'math_cot' },
    { label: 'sec', kind: CompletionItemKind.Function, detail: 'Secant', data: 'math_sec' },
    { label: 'csc', kind: CompletionItemKind.Function, detail: 'Cosecant', data: 'math_csc' },
    { label: 'min', kind: CompletionItemKind.Function, detail: 'Minimum of two values', data: 'math_min' },
    { label: 'max', kind: CompletionItemKind.Function, detail: 'Maximum of two values', data: 'math_max' },
];

// ── Type Conversion Functions ────────────────────────────

export const SCRIPTIT_TYPES: CompletionItem[] = [
    { label: 'str', kind: CompletionItemKind.Function, detail: 'Convert to string', data: 'type_str' },
    { label: 'int', kind: CompletionItemKind.Function, detail: 'Convert to integer', data: 'type_int' },
    { label: 'float', kind: CompletionItemKind.Function, detail: 'Convert to float', data: 'type_float' },
    { label: 'double', kind: CompletionItemKind.Function, detail: 'Convert to double', data: 'type_double' },
    { label: 'long', kind: CompletionItemKind.Function, detail: 'Convert to long', data: 'type_long' },
    { label: 'long_long', kind: CompletionItemKind.Function, detail: 'Convert to long long', data: 'type_long_long' },
    { label: 'long_double', kind: CompletionItemKind.Function, detail: 'Convert to long double', data: 'type_long_double' },
    { label: 'uint', kind: CompletionItemKind.Function, detail: 'Convert to unsigned int', data: 'type_uint' },
    { label: 'ulong', kind: CompletionItemKind.Function, detail: 'Convert to unsigned long', data: 'type_ulong' },
    { label: 'ulong_long', kind: CompletionItemKind.Function, detail: 'Convert to unsigned long long', data: 'type_ulong_long' },
    { label: 'auto_numeric', kind: CompletionItemKind.Function, detail: 'Auto-detect numeric type', data: 'type_auto_numeric' },
    { label: 'bool', kind: CompletionItemKind.Function, detail: 'Convert to boolean', data: 'type_bool' },
    { label: 'list', kind: CompletionItemKind.Function, detail: 'Create or convert to list', data: 'type_list' },
    { label: 'set', kind: CompletionItemKind.Function, detail: 'Create or convert to set', data: 'type_set' },
    { label: 'dict', kind: CompletionItemKind.Function, detail: 'Create or convert to dict', data: 'type_dict' },
];

// ── All Completions Combined ─────────────────────────────

export const ALL_COMPLETIONS: CompletionItem[] = [
    ...SCRIPTIT_KEYWORDS,
    ...SCRIPTIT_BUILTINS,
    ...SCRIPTIT_MATH,
    ...SCRIPTIT_TYPES
];
