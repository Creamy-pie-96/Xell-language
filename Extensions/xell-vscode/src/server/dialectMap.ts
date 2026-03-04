// ═══════════════════════════════════════════════════════════
// Xell Dialect Mapping — Per-file @convert support
// ═══════════════════════════════════════════════════════════
//
// Detects @convert "path.xesy" at the top of .xel files,
// loads the mapping, caches per-file, and provides translation
// helpers used by completions, hover, diagnostics, etc.
//
// Empty values in .xesy mean "keep canonical" — they are ignored.
//
// ═══════════════════════════════════════════════════════════

import * as fs from 'fs';
import * as path from 'path';
import * as url from 'url';

// ── Types ────────────────────────────────────────────────

export interface DialectInfo {
    /** Custom word → canonical keyword.  e.g. "jodi" → "if" */
    customToCanonical: Record<string, string>;
    /** Canonical keyword → custom word.  e.g. "if" → "jodi" */
    canonicalToCustom: Record<string, string>;
    /** Absolute path to the .xesy file */
    mappingPath: string;
    /** mtime of the .xesy file when loaded (for invalidation) */
    mtimeMs: number;
    /** The raw first-line decorator text, for quick change detection */
    decoratorLine: string;
    /** The 0-based line index where the @convert directive sits */
    decoratorLineIndex: number;
}

// ── Module State ─────────────────────────────────────────

/** Per-file dialect cache.  null = checked, no @convert present. */
const dialectCache: Map<string, DialectInfo | null> = new Map();

/** Tracks which .xesy files are used by which file URIs */
const xesyToFiles: Map<string, Set<string>> = new Map();

// ── Decorator Parsing ────────────────────────────────────

/**
 * Scan the first 5 non-blank lines for `@convert "..."`.
 * Returns:
 *   - `{ found: true, path: "..." }` if @convert with explicit path
 *   - `{ found: true, path: "" }` if @convert with no argument
 *   - `{ found: false }` if no @convert
 */
export function extractConvertDirective(text: string): { found: boolean; path: string; line: string; lineIndex: number } {
    const lines = text.split('\n');
    let nonBlankSeen = 0;

    for (let i = 0; i < lines.length && nonBlankSeen < 5; i++) {
        const trimmed = lines[i].trim();
        if (!trimmed) continue;
        nonBlankSeen++;

        if (trimmed.startsWith('@convert')) {
            // Extract quoted path: @convert "path.xesy"
            const match = trimmed.match(/^@convert\s+"([^"]+)"/);
            if (match) {
                return { found: true, path: match[1], line: trimmed, lineIndex: i };
            }
            // @convert with no argument
            return { found: true, path: '', line: trimmed, lineIndex: i };
        }
    }
    return { found: false, path: '', line: '', lineIndex: -1 };
}

// ── Path Resolution ──────────────────────────────────────

/**
 * Resolve a .xesy file path relative to a code file's directory.
 * If mappingPath is empty, searches for any *.xesy in the same directory.
 */
export function resolveXesyPath(fileUri: string, mappingPath: string): string | null {
    let fileFsPath: string;
    try {
        // Convert file:///path URI to filesystem path
        if (fileUri.startsWith('file://')) {
            fileFsPath = url.fileURLToPath(fileUri);
        } else {
            fileFsPath = fileUri;
        }
    } catch {
        return null;
    }

    const fileDir = path.dirname(fileFsPath);

    if (mappingPath) {
        // Explicit path — resolve relative to code file's directory
        const resolved = path.isAbsolute(mappingPath)
            ? mappingPath
            : path.resolve(fileDir, mappingPath);
        return fs.existsSync(resolved) ? resolved : null;
    }

    // Auto-detect: find first *.xesy in the same directory
    try {
        const entries = fs.readdirSync(fileDir);
        for (const entry of entries) {
            if (entry.endsWith('.xesy')) {
                return path.join(fileDir, entry);
            }
        }
    } catch { /* directory not readable */ }

    return null;
}

// ── .xesy File Loading ───────────────────────────────────

/**
 * Load a .xesy JSON file and build forward+inverse maps.
 * Empty values are skipped (canonical stays as-is).
 */
export function loadXesyFile(xesyPath: string): DialectInfo | null {
    try {
        const raw = fs.readFileSync(xesyPath, 'utf-8');
        const data = JSON.parse(raw);

        const canonicalToCustom: Record<string, string> = {};
        const customToCanonical: Record<string, string> = {};

        for (const [key, value] of Object.entries(data)) {
            // Skip metadata keys and empty values
            if (key.startsWith('_')) continue;
            if (typeof value !== 'string' || !value.trim()) continue;

            const customWord = value.trim();
            canonicalToCustom[key] = customWord;
            customToCanonical[customWord] = key;
        }

        const stat = fs.statSync(xesyPath);

        return {
            customToCanonical,
            canonicalToCustom,
            mappingPath: xesyPath,
            mtimeMs: stat.mtimeMs,
            decoratorLine: '',
            decoratorLineIndex: -1
        };
    } catch (err) {
        console.error(`Xell Dialect: Failed to load ${xesyPath}:`, err);
        return null;
    }
}

// ── Cache Management ─────────────────────────────────────

/**
 * Get or load the dialect mapping for a file.
 * Returns null if the file has no @convert or if loading fails.
 */
export function getDialect(fileUri: string, text: string): DialectInfo | null {
    const directive = extractConvertDirective(text);

    if (!directive.found) {
        // No @convert — cache as null and return
        const prev = dialectCache.get(fileUri);
        if (prev !== undefined && prev === null) return null;
        dialectCache.set(fileUri, null);
        return null;
    }

    // Check if cached and decorator line hasn't changed
    const cached = dialectCache.get(fileUri);
    if (cached && cached.decoratorLine === directive.line) {
        // Check if the .xesy file has been modified since we loaded it
        try {
            const stat = fs.statSync(cached.mappingPath);
            if (stat.mtimeMs <= cached.mtimeMs) {
                return cached;
            }
        } catch {
            // File may have been deleted — reload
        }
    }

    // Resolve and load
    const xesyPath = resolveXesyPath(fileUri, directive.path);
    if (!xesyPath) {
        dialectCache.set(fileUri, null);
        return null;
    }

    const info = loadXesyFile(xesyPath);
    if (!info) {
        dialectCache.set(fileUri, null);
        return null;
    }

    info.decoratorLine = directive.line;
    info.decoratorLineIndex = directive.lineIndex;
    dialectCache.set(fileUri, info);

    // Track xesy→file association for invalidation
    let fileSet = xesyToFiles.get(xesyPath);
    if (!fileSet) {
        fileSet = new Set();
        xesyToFiles.set(xesyPath, fileSet);
    }
    fileSet.add(fileUri);

    return info;
}

/**
 * Remove a file's dialect from the cache (called on document close).
 */
export function invalidateDialect(fileUri: string): void {
    const cached = dialectCache.get(fileUri);
    if (cached) {
        const fileSet = xesyToFiles.get(cached.mappingPath);
        if (fileSet) {
            fileSet.delete(fileUri);
            if (fileSet.size === 0) {
                xesyToFiles.delete(cached.mappingPath);
            }
        }
    }
    dialectCache.delete(fileUri);
}

/**
 * Invalidate all files that use a specific .xesy mapping path.
 * Called when a .xesy file changes on disk.
 */
export function invalidateByXesyPath(xesyAbsPath: string): void {
    const fileSet = xesyToFiles.get(xesyAbsPath);
    if (fileSet) {
        for (const uri of fileSet) {
            dialectCache.delete(uri);
        }
    }
}

/**
 * Clear the entire dialect cache (e.g., on configuration change).
 */
export function clearDialectCache(): void {
    dialectCache.clear();
    xesyToFiles.clear();
}

// ── Translation Helpers ──────────────────────────────────

/**
 * Translate a token from custom dialect to canonical.
 * Returns the token unchanged if not in the mapping or no dialect active.
 */
export function translate(token: string, dialect: DialectInfo | null): string {
    if (!dialect) return token;
    return dialect.customToCanonical[token] ?? token;
}

/**
 * Translate an entire document from custom dialect to canonical keywords.
 * Replaces whole words only, skipping content inside strings and comments.
 * Returns the translated text with the same line count.
 */
export function translateDocument(text: string, dialect: DialectInfo): string {
    if (Object.keys(dialect.customToCanonical).length === 0) return text;

    // Build a regex matching any custom keyword (whole word, longest first)
    const customWords = Object.keys(dialect.customToCanonical)
        .sort((a, b) => b.length - a.length || a.localeCompare(b));
    const pattern = new RegExp(`\\b(${customWords.join('|')})\\b`, 'g');

    const lines = text.split('\n');
    const result: string[] = [];

    let inBlockComment = false;

    for (const line of lines) {
        if (inBlockComment) {
            // Inside --> ... <-- block comment — pass through
            if (line.includes('<--')) {
                inBlockComment = false;
            }
            result.push(line);
            continue;
        }

        // Check for block comment start
        if (line.includes('-->')) {
            // If also has <-- on same line, handle inline block comment
            if (line.includes('<--')) {
                // Has both — only replace outside the comment
                result.push(replaceOutsideStringsAndComments(line, pattern, dialect.customToCanonical));
            } else {
                inBlockComment = true;
                result.push(replaceOutsideStringsAndComments(line, pattern, dialect.customToCanonical));
            }
            continue;
        }

        result.push(replaceOutsideStringsAndComments(line, pattern, dialect.customToCanonical));
    }

    return result.join('\n');
}

/**
 * Replace words in a line, skipping content inside strings and after #.
 */
function replaceOutsideStringsAndComments(
    line: string,
    pattern: RegExp,
    mapping: Record<string, string>
): string {
    // Find ranges that are inside strings or after #
    const skipRanges: [number, number][] = [];
    let inStr = false;
    let commentStart = -1;

    for (let i = 0; i < line.length; i++) {
        const ch = line[i];
        if (inStr) {
            if (ch === '\\') { i++; continue; }
            if (ch === '"') {
                skipRanges.push([skipRanges[skipRanges.length - 1]?.[0] ?? i, i + 1]);
                inStr = false;
            }
            continue;
        }
        if (ch === '"') {
            inStr = true;
            skipRanges.push([i, -1]); // -1 = open
            continue;
        }
        if (ch === '#') {
            commentStart = i;
            break;
        }
    }

    // Fix any unclosed string range
    for (let i = 0; i < skipRanges.length; i++) {
        if (skipRanges[i][1] === -1) {
            skipRanges[i][1] = line.length;
        }
    }

    // Add comment range
    if (commentStart >= 0) {
        skipRanges.push([commentStart, line.length]);
    }

    // Do the replacement, checking if each match falls in a skip range
    pattern.lastIndex = 0;
    let result = '';
    let lastIdx = 0;
    let match: RegExpExecArray | null;

    while ((match = pattern.exec(line)) !== null) {
        const matchStart = match.index;
        const matchEnd = matchStart + match[0].length;

        // Check if this match is inside a skip range
        let skip = false;
        for (const [start, end] of skipRanges) {
            if (matchStart >= start && matchEnd <= end) {
                skip = true;
                break;
            }
        }

        result += line.slice(lastIdx, matchStart);
        if (skip) {
            result += match[0]; // keep original
        } else {
            result += mapping[match[0]] ?? match[0];
        }
        lastIdx = matchEnd;
    }
    result += line.slice(lastIdx);

    return result;
}

/**
 * Translate a token from canonical to custom dialect.
 * Returns the token unchanged if not in the mapping or no dialect active.
 */
export function toCustom(token: string, dialect: DialectInfo | null): string {
    if (!dialect) return token;
    return dialect.canonicalToCustom[token] ?? token;
}

/**
 * Get all custom keyword names from a dialect.
 */
export function getCustomKeywords(dialect: DialectInfo | null): string[] {
    if (!dialect) return [];
    return Object.keys(dialect.customToCanonical);
}

/**
 * Check if a word is a custom dialect keyword.
 */
export function isCustomKeyword(word: string, dialect: DialectInfo | null): boolean {
    if (!dialect) return false;
    return word in dialect.customToCanonical;
}
