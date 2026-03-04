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
/**
 * Scan the first 5 non-blank lines for `@convert "..."`.
 * Returns:
 *   - `{ found: true, path: "..." }` if @convert with explicit path
 *   - `{ found: true, path: "" }` if @convert with no argument
 *   - `{ found: false }` if no @convert
 */
export declare function extractConvertDirective(text: string): {
    found: boolean;
    path: string;
    line: string;
    lineIndex: number;
};
/**
 * Resolve a .xesy file path relative to a code file's directory.
 * If mappingPath is empty, searches for any *.xesy in the same directory.
 */
export declare function resolveXesyPath(fileUri: string, mappingPath: string): string | null;
/**
 * Load a .xesy JSON file and build forward+inverse maps.
 * Empty values are skipped (canonical stays as-is).
 */
export declare function loadXesyFile(xesyPath: string): DialectInfo | null;
/**
 * Get or load the dialect mapping for a file.
 * Returns null if the file has no @convert or if loading fails.
 */
export declare function getDialect(fileUri: string, text: string): DialectInfo | null;
/**
 * Remove a file's dialect from the cache (called on document close).
 */
export declare function invalidateDialect(fileUri: string): void;
/**
 * Invalidate all files that use a specific .xesy mapping path.
 * Called when a .xesy file changes on disk.
 */
export declare function invalidateByXesyPath(xesyAbsPath: string): void;
/**
 * Clear the entire dialect cache (e.g., on configuration change).
 */
export declare function clearDialectCache(): void;
/**
 * Translate a token from custom dialect to canonical.
 * Returns the token unchanged if not in the mapping or no dialect active.
 */
export declare function translate(token: string, dialect: DialectInfo | null): string;
/**
 * Translate an entire document from custom dialect to canonical keywords.
 * Replaces whole words only, skipping content inside strings and comments.
 * Returns the translated text with the same line count.
 */
export declare function translateDocument(text: string, dialect: DialectInfo): string;
/**
 * Translate a token from canonical to custom dialect.
 * Returns the token unchanged if not in the mapping or no dialect active.
 */
export declare function toCustom(token: string, dialect: DialectInfo | null): string;
/**
 * Get all custom keyword names from a dialect.
 */
export declare function getCustomKeywords(dialect: DialectInfo | null): string[];
/**
 * Check if a word is a custom dialect keyword.
 */
export declare function isCustomKeyword(word: string, dialect: DialectInfo | null): boolean;
//# sourceMappingURL=dialectMap.d.ts.map