// ═══════════════════════════════════════════════════════════
// Xell Completion Items
// ═══════════════════════════════════════════════════════════

import { CompletionItem, CompletionItemKind } from 'vscode-languageserver/node';

// ── Keywords ─────────────────────────────────────────────

export const XELL_KEYWORDS: CompletionItem[] = [
    { label: 'fn', kind: CompletionItemKind.Keyword, detail: 'Define a function', data: 'kw_fn' },
    { label: 'give', kind: CompletionItemKind.Keyword, detail: 'Give back a value from function', data: 'kw_give' },
    { label: 'if', kind: CompletionItemKind.Keyword, detail: 'Conditional statement', data: 'kw_if' },
    { label: 'elif', kind: CompletionItemKind.Keyword, detail: 'Else-if branch', data: 'kw_elif' },
    { label: 'else', kind: CompletionItemKind.Keyword, detail: 'Else branch', data: 'kw_else' },
    { label: 'for', kind: CompletionItemKind.Keyword, detail: 'For loop', data: 'kw_for' },
    { label: 'while', kind: CompletionItemKind.Keyword, detail: 'While loop', data: 'kw_while' },
    { label: 'in', kind: CompletionItemKind.Keyword, detail: 'Iterator keyword', data: 'kw_in' },
    { label: 'bring', kind: CompletionItemKind.Keyword, detail: 'Import from another file', data: 'kw_bring' },
    { label: 'from', kind: CompletionItemKind.Keyword, detail: 'Import source', data: 'kw_from' },
    { label: 'as', kind: CompletionItemKind.Keyword, detail: 'Import alias', data: 'kw_as' },
    { label: 'of', kind: CompletionItemKind.Keyword, detail: 'Method-of syntax', data: 'kw_of' },
    { label: 'is', kind: CompletionItemKind.Keyword, detail: 'Equality check (alias for ==)', data: 'kw_is' },
    { label: 'not', kind: CompletionItemKind.Keyword, detail: 'Logical negation', data: 'kw_not' },
    { label: 'and', kind: CompletionItemKind.Keyword, detail: 'Logical AND', data: 'kw_and' },
    { label: 'or', kind: CompletionItemKind.Keyword, detail: 'Logical OR', data: 'kw_or' },
    { label: 'eq', kind: CompletionItemKind.Keyword, detail: 'Equal (alias for ==)', data: 'kw_eq' },
    { label: 'ne', kind: CompletionItemKind.Keyword, detail: 'Not equal (alias for !=)', data: 'kw_ne' },
    { label: 'gt', kind: CompletionItemKind.Keyword, detail: 'Greater than (alias for >)', data: 'kw_gt' },
    { label: 'lt', kind: CompletionItemKind.Keyword, detail: 'Less than (alias for <)', data: 'kw_lt' },
    { label: 'ge', kind: CompletionItemKind.Keyword, detail: 'Greater or equal (alias for >=)', data: 'kw_ge' },
    { label: 'le', kind: CompletionItemKind.Keyword, detail: 'Less or equal (alias for <=)', data: 'kw_le' },
    { label: 'true', kind: CompletionItemKind.Constant, detail: 'Boolean true', data: 'const_true' },
    { label: 'false', kind: CompletionItemKind.Constant, detail: 'Boolean false', data: 'const_false' },
    { label: 'none', kind: CompletionItemKind.Constant, detail: 'None value', data: 'const_none' },
];

// ── Builtin Functions ────────────────────────────────────

export const XELL_BUILTINS: CompletionItem[] = [
    { label: 'print', kind: CompletionItemKind.Function, detail: 'Print values to stdout', data: 'fn_print' },
    { label: 'assert', kind: CompletionItemKind.Function, detail: 'Assert a condition is true', data: 'fn_assert' },
    { label: 'len', kind: CompletionItemKind.Function, detail: 'Get length of collection', data: 'fn_len' },
    { label: 'type', kind: CompletionItemKind.Function, detail: 'Get type name of value', data: 'fn_type' },
    { label: 'str', kind: CompletionItemKind.Function, detail: 'Convert to string', data: 'fn_str' },
    { label: 'num', kind: CompletionItemKind.Function, detail: 'Convert to number', data: 'fn_num' },
    { label: 'push', kind: CompletionItemKind.Function, detail: 'Add item to list', data: 'fn_push' },
    { label: 'pop', kind: CompletionItemKind.Function, detail: 'Remove last item from list', data: 'fn_pop' },
    { label: 'keys', kind: CompletionItemKind.Function, detail: 'Get map keys as list', data: 'fn_keys' },
    { label: 'values', kind: CompletionItemKind.Function, detail: 'Get map values as list', data: 'fn_values' },
    { label: 'range', kind: CompletionItemKind.Function, detail: 'Generate a list of numbers', data: 'fn_range' },
    { label: 'set', kind: CompletionItemKind.Function, detail: 'Set a value in map or list', data: 'fn_set' },
    { label: 'has', kind: CompletionItemKind.Function, detail: 'Check if key exists in map', data: 'fn_has' },
];

// ── OS Builtin Functions ─────────────────────────────────

export const XELL_OS_BUILTINS: CompletionItem[] = [
    { label: 'mkdir', kind: CompletionItemKind.Function, detail: 'Create directory (recursive)', data: 'fn_mkdir' },
    { label: 'rm', kind: CompletionItemKind.Function, detail: 'Remove file or directory', data: 'fn_rm' },
    { label: 'cp', kind: CompletionItemKind.Function, detail: 'Copy file or directory', data: 'fn_cp' },
    { label: 'mv', kind: CompletionItemKind.Function, detail: 'Move/rename file or directory', data: 'fn_mv' },
    { label: 'exists', kind: CompletionItemKind.Function, detail: 'Check if path exists', data: 'fn_exists' },
    { label: 'is_file', kind: CompletionItemKind.Function, detail: 'Check if path is a file', data: 'fn_is_file' },
    { label: 'is_dir', kind: CompletionItemKind.Function, detail: 'Check if path is a directory', data: 'fn_is_dir' },
    { label: 'ls', kind: CompletionItemKind.Function, detail: 'List directory contents', data: 'fn_ls' },
    { label: 'read', kind: CompletionItemKind.Function, detail: 'Read file contents', data: 'fn_read' },
    { label: 'write', kind: CompletionItemKind.Function, detail: 'Write data to file', data: 'fn_write' },
    { label: 'append', kind: CompletionItemKind.Function, detail: 'Append data to file', data: 'fn_append' },
    { label: 'file_size', kind: CompletionItemKind.Function, detail: 'Get file size in bytes', data: 'fn_file_size' },
    { label: 'cwd', kind: CompletionItemKind.Function, detail: 'Get current working directory', data: 'fn_cwd' },
    { label: 'cd', kind: CompletionItemKind.Function, detail: 'Change working directory', data: 'fn_cd' },
    { label: 'abspath', kind: CompletionItemKind.Function, detail: 'Get absolute path', data: 'fn_abspath' },
    { label: 'basename', kind: CompletionItemKind.Function, detail: 'Get file name from path', data: 'fn_basename' },
    { label: 'dirname', kind: CompletionItemKind.Function, detail: 'Get directory from path', data: 'fn_dirname' },
    { label: 'ext', kind: CompletionItemKind.Function, detail: 'Get file extension', data: 'fn_ext' },
    { label: 'env_get', kind: CompletionItemKind.Function, detail: 'Get environment variable', data: 'fn_env_get' },
    { label: 'env_set', kind: CompletionItemKind.Function, detail: 'Set environment variable', data: 'fn_env_set' },
    { label: 'env_unset', kind: CompletionItemKind.Function, detail: 'Unset environment variable', data: 'fn_env_unset' },
    { label: 'env_has', kind: CompletionItemKind.Function, detail: 'Check if env variable exists', data: 'fn_env_has' },
    { label: 'run', kind: CompletionItemKind.Function, detail: 'Run external command', data: 'fn_run' },
    { label: 'run_capture', kind: CompletionItemKind.Function, detail: 'Run and capture command output', data: 'fn_run_capture' },
    { label: 'pid', kind: CompletionItemKind.Function, detail: 'Get current process ID', data: 'fn_pid' },
];

// ── Math Functions ───────────────────────────────────────

export const XELL_MATH: CompletionItem[] = [
    { label: 'floor', kind: CompletionItemKind.Function, detail: 'Floor (round down)', data: 'math_floor' },
    { label: 'ceil', kind: CompletionItemKind.Function, detail: 'Ceiling (round up)', data: 'math_ceil' },
    { label: 'round', kind: CompletionItemKind.Function, detail: 'Round to nearest', data: 'math_round' },
    { label: 'abs', kind: CompletionItemKind.Function, detail: 'Absolute value', data: 'math_abs' },
    { label: 'mod', kind: CompletionItemKind.Function, detail: 'Modulo operation', data: 'math_mod' },
];

// ── All Completions Combined ─────────────────────────────

export const ALL_COMPLETIONS: CompletionItem[] = [
    ...XELL_KEYWORDS,
    ...XELL_BUILTINS,
    ...XELL_OS_BUILTINS,
    ...XELL_MATH
];
