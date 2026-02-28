// ═══════════════════════════════════════════════════════════
// Xell Hover / Signature Information
// ═══════════════════════════════════════════════════════════

export interface HoverEntry {
    signature: string;
    detail: string;
    params?: string[];
}

export const HOVER_INFO: Record<string, HoverEntry> = {

    // ── I/O ──────────────────────────────────────────────
    print: {
        signature: 'print(value1, value2, ...)',
        detail: 'Print one or more values to stdout, separated by spaces.',
        params: ['value1 — first value to print', '... — additional values']
    },

    // ── Type / Introspection ─────────────────────────────
    len: {
        signature: 'len(collection)',
        detail: 'Return the number of elements in a list, map, or string.',
        params: ['collection — list, map, or string']
    },
    type: {
        signature: 'type(value)',
        detail: 'Return the type name of value as a string (e.g. "string", "number", "list").',
        params: ['value — any value']
    },
    str: {
        signature: 'str(value)',
        detail: 'Convert value to string.',
        params: ['value — any value']
    },
    num: {
        signature: 'num(value)',
        detail: 'Convert string to number.',
        params: ['value — numeric string or number']
    },

    // ── Collections ──────────────────────────────────────
    push: {
        signature: 'push(list, item)',
        detail: 'Append item to end of list. Mutates the list.',
        params: ['list — the list', 'item — value to append']
    },
    pop: {
        signature: 'pop(list)',
        detail: 'Remove and return the last item from a list.',
        params: ['list — the list']
    },
    keys: {
        signature: 'keys(map)',
        detail: 'Return all keys of a map as a list.',
        params: ['map — a map']
    },
    values: {
        signature: 'values(map)',
        detail: 'Return all values of a map as a list.',
        params: ['map — a map']
    },
    range: {
        signature: 'range(start, end [, step])',
        detail: 'Generate a list of numbers from start to end (exclusive), with optional step.',
        params: ['start — starting number', 'end — ending number (exclusive)', 'step — increment (default 1)']
    },
    set: {
        signature: 'set(collection, key, value)',
        detail: 'Set a value at a key/index in a map or list.',
        params: ['collection — map or list', 'key — key or index', 'value — new value']
    },
    has: {
        signature: 'has(map, key)',
        detail: 'Check if a key exists in a map.',
        params: ['map — a map', 'key — key to check']
    },
    assert: {
        signature: 'assert(condition, message)',
        detail: 'Assert a condition is true. Throws error with message if false.',
        params: ['condition — boolean expression', 'message — error message on failure']
    },

    // ── OS / Filesystem ──────────────────────────────────
    mkdir: {
        signature: 'mkdir(path)',
        detail: 'Create directory (and parent directories) recursively.',
        params: ['path — directory path to create']
    },
    rm: {
        signature: 'rm(path)',
        detail: 'Remove a file or directory (recursive).',
        params: ['path — path to remove']
    },
    cp: {
        signature: 'cp(source, destination)',
        detail: 'Copy a file or directory.',
        params: ['source — source path', 'destination — destination path']
    },
    mv: {
        signature: 'mv(source, destination)',
        detail: 'Move/rename a file or directory.',
        params: ['source — current path', 'destination — new path']
    },
    exists: {
        signature: 'exists(path)',
        detail: 'Check if a path exists. Returns true/false.',
        params: ['path — path to check']
    },
    is_file: {
        signature: 'is_file(path)',
        detail: 'Check if path is a regular file.',
        params: ['path — path to check']
    },
    is_dir: {
        signature: 'is_dir(path)',
        detail: 'Check if path is a directory.',
        params: ['path — path to check']
    },
    ls: {
        signature: 'ls(path)',
        detail: 'List directory contents. Returns list of names.',
        params: ['path — directory path']
    },
    read: {
        signature: 'read(path)',
        detail: 'Read entire file contents as string.',
        params: ['path — file path']
    },
    write: {
        signature: 'write(path, data)',
        detail: 'Write string data to file (overwrites).',
        params: ['path — file path', 'data — string content to write']
    },
    append: {
        signature: 'append(path, data)',
        detail: 'Append string data to end of file.',
        params: ['path — file path', 'data — string content to append']
    },
    file_size: {
        signature: 'file_size(path)',
        detail: 'Get file size in bytes.',
        params: ['path — file path']
    },
    cwd: {
        signature: 'cwd()',
        detail: 'Get current working directory as string.',
    },
    cd: {
        signature: 'cd(path)',
        detail: 'Change the current working directory.',
        params: ['path — new directory']
    },
    abspath: {
        signature: 'abspath(path)',
        detail: 'Get absolute path from a relative path.',
        params: ['path — relative or absolute path']
    },
    basename: {
        signature: 'basename(path)',
        detail: 'Get the file name portion of a path.',
        params: ['path — file path']
    },
    dirname: {
        signature: 'dirname(path)',
        detail: 'Get the directory portion of a path.',
        params: ['path — file path']
    },
    ext: {
        signature: 'ext(path)',
        detail: 'Get the file extension (including the dot).',
        params: ['path — file path']
    },

    // ── Environment Variables ────────────────────────────
    env_get: {
        signature: 'env_get(name)',
        detail: 'Get environment variable value. Returns string or none.',
        params: ['name — variable name']
    },
    env_set: {
        signature: 'env_set(name, value)',
        detail: 'Set environment variable.',
        params: ['name — variable name', 'value — string value']
    },
    env_unset: {
        signature: 'env_unset(name)',
        detail: 'Unset (remove) an environment variable.',
        params: ['name — variable name']
    },
    env_has: {
        signature: 'env_has(name)',
        detail: 'Check if environment variable exists. Returns true/false.',
        params: ['name — variable name']
    },

    // ── Process Execution ────────────────────────────────
    run: {
        signature: 'run(command)',
        detail: 'Run an external command. Returns exit code as number.',
        params: ['command — shell command string']
    },
    run_capture: {
        signature: 'run_capture(command)',
        detail: 'Run command and capture output. Returns map with exit_code, stdout, stderr.',
        params: ['command — shell command string']
    },
    pid: {
        signature: 'pid()',
        detail: 'Get the current process ID.',
    },

    // ── Math Functions ───────────────────────────────────
    floor: { signature: 'floor(x)', detail: 'Largest integer ≤ x.', params: ['x — any number'] },
    ceil: { signature: 'ceil(x)', detail: 'Smallest integer ≥ x.', params: ['x — any number'] },
    round: { signature: 'round(x)', detail: 'Round x to the nearest integer.', params: ['x — any number'] },
    abs: { signature: 'abs(x)', detail: 'Absolute value of x.', params: ['x — any number'] },
    mod: { signature: 'mod(a, b)', detail: 'Modulo: remainder of a / b.', params: ['a — dividend', 'b — divisor'] },

    // ── Keywords ─────────────────────────────────────────
    fn: { signature: 'fn name(params) :', detail: 'Define a function. Body follows on indented lines, ends with ;' },
    give: { signature: 'give expression', detail: 'Give back a value from a function.' },
    'for': { signature: 'for item in collection :', detail: 'For loop over a list or range. Body indented, ends with ;' },
    'while': { signature: 'while condition :', detail: 'While loop. Body indented, ends with ;' },
    'if': { signature: 'if condition :', detail: 'Conditional branch. Body indented, elif/else optional.' },
    bring: { signature: 'bring name from "file.xel"', detail: 'Import names from another Xell file.' },
};
