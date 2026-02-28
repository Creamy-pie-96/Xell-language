// ═══════════════════════════════════════════════════════════
// ScriptIt Hover / Signature Information
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
    pprint: {
        signature: 'pprint(value)',
        detail: 'Pretty-print a value (lists / dicts shown with formatting).',
        params: ['value — value to pretty-print']
    },
    input: {
        signature: 'input(prompt)',
        detail: 'Display prompt and read a line from stdin. Returns a string.',
        params: ['prompt — text shown before reading']
    },
    read: {
        signature: 'read(filename)',
        detail: 'Read the entire contents of a file and return as string.',
        params: ['filename — path to the file']
    },
    readLine: {
        signature: 'readLine(filename)',
        detail: 'Read a file and return a list of lines.',
        params: ['filename — path to the file']
    },
    write: {
        signature: 'write(filename, data)',
        detail: 'Write data to a file (overwrites existing content).',
        params: ['filename — path to the file', 'data — string data to write']
    },

    // ── Type / Introspection ─────────────────────────────
    len: {
        signature: 'len(collection)',
        detail: 'Return the number of elements in a list, set, dict, or string.',
        params: ['collection — list, set, dict, or string']
    },
    type: {
        signature: 'type(value)',
        detail: 'Return the type name of value as a string (e.g. "str", "int", "list").',
        params: ['value — any value']
    },
    isinstance: {
        signature: 'isinstance(value, typename)',
        detail: 'Return True if value\'s type matches typename string.',
        params: ['value — any value', 'typename — type name string ("str", "int", etc.)']
    },
    repr: {
        signature: 'repr(value)',
        detail: 'Return a string representation of a value (with quotes around strings).',
        params: ['value — any value']
    },

    // ── Type Conversion ──────────────────────────────────
    str: {
        signature: 'str(value)',
        detail: 'Convert value to string.',
        params: ['value — any value']
    },
    int: {
        signature: 'int(value)',
        detail: 'Convert value to integer. NOTE: int("42") is not supported — use double() for string-to-number.',
        params: ['value — numeric value']
    },
    float: {
        signature: 'float(value)',
        detail: 'Convert value to float.',
        params: ['value — numeric value or string']
    },
    double: {
        signature: 'double(value)',
        detail: 'Convert value to double. Works with numeric strings like double("42").',
        params: ['value — numeric value or numeric string']
    },
    long: {
        signature: 'long(value)',
        detail: 'Convert value to long integer.',
        params: ['value — numeric value']
    },
    long_long: {
        signature: 'long_long(value)',
        detail: 'Convert value to long long integer.',
        params: ['value — numeric value']
    },
    long_double: {
        signature: 'long_double(value)',
        detail: 'Convert value to long double.',
        params: ['value — numeric value']
    },
    uint: {
        signature: 'uint(value)',
        detail: 'Convert value to unsigned int.',
        params: ['value — non-negative numeric value']
    },
    ulong: {
        signature: 'ulong(value)',
        detail: 'Convert value to unsigned long.',
        params: ['value — non-negative numeric value']
    },
    ulong_long: {
        signature: 'ulong_long(value)',
        detail: 'Convert value to unsigned long long.',
        params: ['value — non-negative numeric value']
    },
    auto_numeric: {
        signature: 'auto_numeric(value)',
        detail: 'Convert string to the most appropriate numeric type automatically.',
        params: ['value — string containing a number']
    },
    bool: {
        signature: 'bool(value)',
        detail: 'Convert value to boolean.',
        params: ['value — any value']
    },

    // ── Container Constructors ───────────────────────────
    list: {
        signature: 'list(values...)',
        detail: 'Create a new list from arguments.',
        params: ['values... — elements to include']
    },
    set: {
        signature: 'set(values...)',
        detail: 'Create a new set from arguments (unique values only).',
        params: ['values... — elements to include']
    },
    dict: {
        signature: 'dict()',
        detail: 'Create an empty dictionary.',
    },
    range_list: {
        signature: 'range_list(start, end)',
        detail: 'Create a list of integers from start (inclusive) to end (exclusive).',
        params: ['start — starting integer', 'end — ending integer (exclusive)']
    },

    // ── Container Operations ─────────────────────────────
    append: {
        signature: 'append(list, item)',
        detail: 'Return a new list with item appended.',
        params: ['list — the source list', 'item — value to append']
    },
    pop: {
        signature: 'pop(list)',
        detail: 'Return a new list with the last item removed.',
        params: ['list — the source list']
    },
    sum: {
        signature: 'sum(list)',
        detail: 'Return the sum of all elements in a list.',
        params: ['list — a list of numbers']
    },
    sorted: {
        signature: 'sorted(list)',
        detail: 'Return a new list with elements sorted.',
        params: ['list — the source list']
    },
    reversed: {
        signature: 'reversed(list)',
        detail: 'Return a new list with elements in reverse order.',
        params: ['list — the source list']
    },
    all: {
        signature: 'all(list)',
        detail: 'Return True if all elements are truthy.',
        params: ['list — a list of values']
    },
    any: {
        signature: 'any(list)',
        detail: 'Return True if any element is truthy.',
        params: ['list — a list of values']
    },
    enumerate: {
        signature: 'enumerate(list)',
        detail: 'Return a list of [index, value] pairs.',
        params: ['list — the source list']
    },
    zip: {
        signature: 'zip(list1, list2)',
        detail: 'Return a list of [a, b] pairs from two lists.',
        params: ['list1 — first list', 'list2 — second list']
    },
    map: {
        signature: 'map(fn, list)',
        detail: 'Apply function to each element and return results.',
        params: ['fn — function name or lambda', 'list — the source list']
    },

    // ── File I/O ─────────────────────────────────────────
    open: {
        signature: 'open(filename, mode)',
        detail: 'Open a file handle. Use with context manager: with open("f.txt", "r") as fh:',
        params: ['filename — path to file', 'mode — "r", "w", or "a"']
    },
    close: {
        signature: 'close(handle)',
        detail: 'Close an open file handle.',
        params: ['handle — the file handle to close']
    },

    // ── Math Functions ───────────────────────────────────
    sin: {
        signature: 'sin(x)',
        detail: 'Sine of x (radians).',
        params: ['x — angle in radians']
    },
    cos: {
        signature: 'cos(x)',
        detail: 'Cosine of x (radians).',
        params: ['x — angle in radians']
    },
    tan: {
        signature: 'tan(x)',
        detail: 'Tangent of x (radians).',
        params: ['x — angle in radians']
    },
    asin: {
        signature: 'asin(x)',
        detail: 'Arc sine of x. Returns radians.',
        params: ['x — value in [-1, 1]']
    },
    acos: {
        signature: 'acos(x)',
        detail: 'Arc cosine of x. Returns radians.',
        params: ['x — value in [-1, 1]']
    },
    atan: {
        signature: 'atan(x)',
        detail: 'Arc tangent of x. Returns radians.',
        params: ['x — any number']
    },
    log: {
        signature: 'log(x)',
        detail: 'Natural logarithm (base e).',
        params: ['x — positive number']
    },
    log2: {
        signature: 'log2(x)',
        detail: 'Base-2 logarithm.',
        params: ['x — positive number']
    },
    log10: {
        signature: 'log10(x)',
        detail: 'Base-10 logarithm.',
        params: ['x — positive number']
    },
    sqrt: {
        signature: 'sqrt(x)',
        detail: 'Square root of x.',
        params: ['x — non-negative number']
    },
    abs: {
        signature: 'abs(x)',
        detail: 'Absolute value of x.',
        params: ['x — any number']
    },
    ceil: {
        signature: 'ceil(x)',
        detail: 'Smallest integer ≥ x.',
        params: ['x — any number']
    },
    floor: {
        signature: 'floor(x)',
        detail: 'Largest integer ≤ x.',
        params: ['x — any number']
    },
    round: {
        signature: 'round(x)',
        detail: 'Round x to the nearest integer.',
        params: ['x — any number']
    },
    cot: {
        signature: 'cot(x)',
        detail: 'Cotangent of x (1/tan).',
        params: ['x — angle in radians']
    },
    sec: {
        signature: 'sec(x)',
        detail: 'Secant of x (1/cos).',
        params: ['x — angle in radians']
    },
    csc: {
        signature: 'csc(x)',
        detail: 'Cosecant of x (1/sin).',
        params: ['x — angle in radians']
    },
    min: {
        signature: 'min(a, b)',
        detail: 'Return the smaller of two values.',
        params: ['a — first value', 'b — second value']
    },
    max: {
        signature: 'max(a, b)',
        detail: 'Return the larger of two values.',
        params: ['a — first value', 'b — second value']
    },

    // ── Keywords ─────────────────────────────────────────
    fn: {
        signature: 'fn name(params):',
        detail: 'Define a function. Body follows on indented lines, ends with ;',
    },
    give: {
        signature: 'give expression.',
        detail: 'Return a value from a function. Parentheses optional.',
    },
    var: {
        signature: 'var name = value.',
        detail: 'Declare a variable with an initial value.',
    },
    'for': {
        signature: 'for i in range(N): / for i in range(from A to B step S):',
        detail: 'For loop with range or iterable. Body indented, ends with ;',
    },
    'while': {
        signature: 'while condition:',
        detail: 'While loop. Body indented, ends with ;',
    },
    'if': {
        signature: 'if condition:',
        detail: 'Conditional branch. Body indented, elif / else optional.',
    },
    'with': {
        signature: 'with open("file", "r") as handle:',
        detail: 'Context manager — automatically closes resource when block ends.',
    },
    range: {
        signature: 'range(N) / range(from A to B) / range(from A to B step S)',
        detail: 'Range generator for use in for loops.',
        params: ['N — count (simple form)', 'from A — start value', 'to B — end value', 'step S — step size']
    },
    pass: {
        signature: 'pass.',
        detail: 'No-op placeholder. Use in empty blocks.',
    },
};
