"use strict";
// ═══════════════════════════════════════════════════════════
// Xell Language Server
// ═══════════════════════════════════════════════════════════
//
// Provides:
//   • Diagnostics (error squiggles from xell --check)
//   • Completion (keywords, builtins, user-defined names)
//   • Hover information (function signatures)
//   • Signature help (function parameters)
//
// ═══════════════════════════════════════════════════════════
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || (function () {
    var ownKeys = function(o) {
        ownKeys = Object.getOwnPropertyNames || function (o) {
            var ar = [];
            for (var k in o) if (Object.prototype.hasOwnProperty.call(o, k)) ar[ar.length] = k;
            return ar;
        };
        return ownKeys(o);
    };
    return function (mod) {
        if (mod && mod.__esModule) return mod;
        var result = {};
        if (mod != null) for (var k = ownKeys(mod), i = 0; i < k.length; i++) if (k[i] !== "default") __createBinding(result, mod, k[i]);
        __setModuleDefault(result, mod);
        return result;
    };
})();
Object.defineProperty(exports, "__esModule", { value: true });
const node_1 = require("vscode-languageserver/node");
const vscode_languageserver_textdocument_1 = require("vscode-languageserver-textdocument");
const diagnostics_1 = require("./diagnostics");
const completions_1 = require("./completions");
const hover_1 = require("./hover");
const dialectMap_1 = require("./dialectMap");
const fs = __importStar(require("fs"));
const path = __importStar(require("path"));
const os = __importStar(require("os"));
// ── Connection & Document Manager ────────────────────────
const connection = (0, node_1.createConnection)(node_1.ProposedFeatures.all);
const documents = new node_1.TextDocuments(vscode_languageserver_textdocument_1.TextDocument);
let hasConfigurationCapability = false;
let diagnosticsEngine;
const defaultSettings = {
    xellPath: 'xell',
    maxNumberOfProblems: 100,
    enableLinting: true
};
function resolveXellPath(configured) {
    // If it's a full path that exists, use it
    if (configured && configured !== 'xell') {
        return configured;
    }
    const home = os.homedir();
    const candidates = [
        path.join(home, '.local', 'bin', 'xell'),
        '/usr/local/bin/xell',
        '/usr/bin/xell',
    ];
    for (const p of candidates) {
        try {
            if (fs.existsSync(p) && fs.statSync(p).isFile()) {
                return p;
            }
        }
        catch { /* ignore */ }
    }
    return configured;
}
let globalSettings = defaultSettings;
const documentSettings = new Map();
// ── Initialize ───────────────────────────────────────────
connection.onInitialize((params) => {
    const capabilities = params.capabilities;
    hasConfigurationCapability = !!(capabilities.workspace && !!capabilities.workspace.configuration);
    const result = {
        capabilities: {
            textDocumentSync: node_1.TextDocumentSyncKind.Incremental,
            completionProvider: {
                resolveProvider: true,
                triggerCharacters: ['.', '(', '-']
            },
            hoverProvider: true,
            signatureHelpProvider: {
                triggerCharacters: ['(', ',']
            },
            semanticTokensProvider: {
                legend: {
                    tokenTypes: [
                        'keyword', // 0  fallback keyword
                        'function', // 1  builtins
                        'type', // 2  (unused)
                        'variable', // 3  (unused)
                        'namespace', // 4  import keywords
                        'enumMember', // 5  constants (true/false/none)
                        'operator', // 6  logical ops
                        'xellFnDecl', // 7  fn
                        'xellReturn', // 8  give
                        'xellConditional', // 9  if, elif, else, incase
                        'xellLoop', // 10 for, while, loop, in
                        'xellTryCatch', // 11 try, catch, finally
                        'xellBinding', // 12 let, be, immutable
                        'xellModule', // 13 module, export, requires
                        'xellOopDecl', // 14 class, struct, enum, interface, abstract, mixin
                        'xellOopModifier', // 15 inherits, implements, with
                        'xellAccess', // 16 private, protected, public, static
                        'xellGenerator', // 17 yield
                        'xellAsync', // 18 async, await
                        'xellComparison', // 19 is, eq, ne, gt, lt, ge, le
                        'xellSpecial' // 20 of
                    ],
                    tokenModifiers: [
                        'declaration', 'defaultLibrary', 'controlFlow',
                        'async', 'readonly'
                    ]
                },
                full: true
            }
        }
    };
    return result;
});
connection.onInitialized(() => {
    if (hasConfigurationCapability) {
        connection.client.register(node_1.DidChangeConfigurationNotification.type, undefined);
    }
    diagnosticsEngine = new diagnostics_1.XellDiagnostics(connection);
});
// ── Configuration ────────────────────────────────────────
function getDocumentSettings(resource) {
    if (!hasConfigurationCapability) {
        return Promise.resolve(globalSettings);
    }
    let result = documentSettings.get(resource);
    if (!result) {
        result = connection.workspace.getConfiguration({
            scopeUri: resource,
            section: 'xell'
        });
        documentSettings.set(resource, result);
    }
    return result;
}
connection.onDidChangeConfiguration(change => {
    if (hasConfigurationCapability) {
        documentSettings.clear();
    }
    else {
        globalSettings = (change.settings.xell || defaultSettings);
    }
    (0, dialectMap_1.clearDialectCache)();
    documents.all().forEach(validateTextDocument);
});
documents.onDidClose(e => {
    documentSettings.delete(e.document.uri);
    (0, dialectMap_1.invalidateDialect)(e.document.uri);
});
// ── Diagnostics ──────────────────────────────────────────
const pendingValidations = new Map();
documents.onDidChangeContent(change => {
    // Debounce: wait 300ms after last keystroke before validating
    const uri = change.document.uri;
    const prev = pendingValidations.get(uri);
    if (prev)
        clearTimeout(prev);
    pendingValidations.set(uri, setTimeout(() => {
        pendingValidations.delete(uri);
        validateTextDocument(change.document);
    }, 300));
});
async function validateTextDocument(textDocument) {
    const settings = await getDocumentSettings(textDocument.uri);
    if (!settings.enableLinting) {
        connection.sendDiagnostics({ uri: textDocument.uri, diagnostics: [] });
        return;
    }
    const text = textDocument.getText();
    const diagnostics = [];
    // Load dialect mapping for this file (if @convert present)
    const dialect = (0, dialectMap_1.getDialect)(textDocument.uri, text);
    const lines = text.split('\n');
    const bracketStack = [];
    const bracketPairs = { '(': ')', '{': '}', '[': ']' };
    const closingToOpening = { ')': '(', '}': '{', ']': '[' };
    for (let i = 0; i < lines.length; i++) {
        const line = lines[i];
        // Find comment start (skip strings)
        let commentIdx = -1;
        {
            let inStr = false;
            for (let k = 0; k < line.length; k++) {
                const ch = line[k];
                if (inStr) {
                    if (ch === '\\') {
                        k++;
                        continue;
                    }
                    if (ch === '"')
                        inStr = false;
                    continue;
                }
                if (ch === '"') {
                    inStr = true;
                    continue;
                }
                if (ch === '#') {
                    commentIdx = k;
                    break;
                }
            }
        }
        const codePart = commentIdx >= 0 ? line.substring(0, commentIdx) : line;
        // Check brackets
        let inString = false;
        for (let j = 0; j < codePart.length; j++) {
            const ch = codePart[j];
            if (inString) {
                if (ch === '\\') {
                    j++;
                    continue;
                }
                if (ch === '"')
                    inString = false;
                continue;
            }
            if (ch === '"') {
                inString = true;
                continue;
            }
            if (ch === '(' || ch === '{' || ch === '[') {
                bracketStack.push({ char: ch, line: i, col: j });
            }
            else if (ch === ')' || ch === '}' || ch === ']') {
                const expected = closingToOpening[ch];
                if (bracketStack.length === 0) {
                    diagnostics.push({
                        severity: node_1.DiagnosticSeverity.Error,
                        range: { start: { line: i, character: j }, end: { line: i, character: j + 1 } },
                        message: `Unmatched closing bracket '${ch}'`,
                        source: 'xell'
                    });
                }
                else if (bracketStack[bracketStack.length - 1].char !== expected) {
                    const top = bracketStack[bracketStack.length - 1];
                    diagnostics.push({
                        severity: node_1.DiagnosticSeverity.Error,
                        range: { start: { line: i, character: j }, end: { line: i, character: j + 1 } },
                        message: `Mismatched bracket: expected '${bracketPairs[top.char]}' but found '${ch}'`,
                        source: 'xell'
                    });
                    bracketStack.pop();
                }
                else {
                    bracketStack.pop();
                }
            }
        }
        if (inString) {
            diagnostics.push({
                severity: node_1.DiagnosticSeverity.Error,
                range: { start: { line: i, character: 0 }, end: { line: i, character: line.length } },
                message: `Unterminated string literal`,
                source: 'xell'
            });
        }
        const trimmed = codePart.trim();
        // Warn about assignment in condition (dialect-aware)
        const condKeywords = ['if', 'elif', 'while'];
        const allCondWords = [...condKeywords];
        if (dialect) {
            for (const kw of condKeywords) {
                const custom = dialect.canonicalToCustom[kw];
                if (custom)
                    allCondWords.push(custom);
            }
        }
        const condPattern = new RegExp(`^(${allCondWords.join('|')})\\s+`);
        if (condPattern.test(trimmed)) {
            const condMatch = trimmed.match(new RegExp(`^(?:${allCondWords.join('|')})\\s+(.+?):\\s*$`));
            if (condMatch) {
                const condition = condMatch[1];
                if (/(?<![=!<>])=(?!=)/.test(condition)) {
                    const eqIdx = codePart.indexOf('=', codePart.indexOf(condMatch[1]));
                    diagnostics.push({
                        severity: node_1.DiagnosticSeverity.Warning,
                        range: { start: { line: i, character: eqIdx }, end: { line: i, character: eqIdx + 1 } },
                        message: `Possible assignment in condition. Did you mean '==' or 'is'?`,
                        source: 'xell'
                    });
                }
            }
        }
        // Check function definition missing colon (dialect-aware)
        const fnKw = dialect?.canonicalToCustom['fn'] ?? 'fn';
        const fnWords = fnKw === 'fn' ? 'fn' : `fn|${fnKw}`;
        const fnMissingColon = new RegExp(`^\\s*(?:${fnWords})\\s+[a-zA-Z_]\\w*\\s*\\([^)]*\\)\\s*$`);
        if (fnMissingColon.test(line)) {
            diagnostics.push({
                severity: node_1.DiagnosticSeverity.Error,
                range: { start: { line: i, character: 0 }, end: { line: i, character: line.length } },
                message: `Function definition missing ':' after parameters`,
                source: 'xell'
            });
        }
    }
    // Unclosed brackets
    for (const bracket of bracketStack) {
        diagnostics.push({
            severity: node_1.DiagnosticSeverity.Error,
            range: { start: { line: bracket.line, character: bracket.col }, end: { line: bracket.line, character: bracket.col + 1 } },
            message: `Unclosed bracket '${bracket.char}'`,
            source: 'xell'
        });
    }
    // Subprocess diagnostics
    if (diagnosticsEngine) {
        // When a dialect is active, translate custom keywords → canonical
        // before sending to `xell --check` (which reads from stdin and has
        // no file context to resolve @convert directives).
        let lintText = text;
        if (dialect) {
            lintText = (0, dialectMap_1.translateDocument)(text, dialect);
            // Strip the @convert decorator line so the C++ parser doesn't
            // treat it as an unrecognised decorator.
            const convertLineIdx = dialect.decoratorLineIndex;
            if (convertLineIdx >= 0) {
                const lintLines = lintText.split('\n');
                if (convertLineIdx < lintLines.length) {
                    lintLines[convertLineIdx] = '# @convert (stripped by LSP)';
                    lintText = lintLines.join('\n');
                }
            }
        }
        const subprocessDiags = await diagnosticsEngine.validate(lintText, resolveXellPath(settings.xellPath));
        diagnostics.push(...subprocessDiags.slice(0, settings.maxNumberOfProblems - diagnostics.length));
    }
    connection.sendDiagnostics({
        uri: textDocument.uri,
        diagnostics: diagnostics.slice(0, settings.maxNumberOfProblems)
    });
}
// ── Completion ───────────────────────────────────────────
connection.onCompletion((params) => {
    const doc = documents.get(params.textDocument.uri);
    if (!doc)
        return completions_1.ALL_COMPLETIONS;
    const text = doc.getText();
    const offset = doc.offsetAt(params.position);
    // Check for -> access
    const beforeCursor = text.substring(Math.max(0, offset - 50), offset);
    if (beforeCursor.match(/->\s*$/)) {
        return getMapKeyCompletions();
    }
    // Get dialect mapping for this file
    const dialect = (0, dialectMap_1.getDialect)(doc.uri, text);
    // Build completion list: for dialect files, replace mapped keywords/builtins
    let completions;
    if (dialect && Object.keys(dialect.canonicalToCustom).length > 0) {
        completions = completions_1.ALL_COMPLETIONS.map(item => {
            const custom = dialect.canonicalToCustom[item.label];
            if (!custom)
                return item; // not mapped — keep canonical
            return {
                ...item,
                label: custom,
                detail: `${item.detail} (\u2192 ${item.label})`,
                filterText: custom,
                insertText: custom,
                data: item.data
            };
        });
    }
    else {
        completions = [...completions_1.ALL_COMPLETIONS];
    }
    const userCompletions = extractUserIdentifiers(text, dialect);
    return [...completions, ...userCompletions];
});
connection.onCompletionResolve((item) => {
    // Try direct lookup first, then check if it's a dialect alias
    let info = hover_1.HOVER_INFO[item.label];
    if (!info && item.data && typeof item.data === 'string') {
        // item.data may contain the original canonical key
        const canonical = item.data.replace(/^(kw_|fn_)/, '');
        info = hover_1.HOVER_INFO[canonical];
    }
    if (info) {
        item.documentation = { kind: node_1.MarkupKind.Markdown, value: info.detail };
    }
    return item;
});
function getMapKeyCompletions() {
    // Generic suggestions for map/object access
    return [
        { label: 'host', kind: node_1.CompletionItemKind.Property, detail: 'Map key', data: 'key_host' },
        { label: 'port', kind: node_1.CompletionItemKind.Property, detail: 'Map key', data: 'key_port' },
    ];
}
function extractUserIdentifiers(text, dialect = null) {
    const items = [];
    const seen = new Set();
    // User-defined functions: fn name( — or dialect equivalent
    const fnKw = dialect?.canonicalToCustom['fn'] ?? 'fn';
    const fnWords = fnKw === 'fn' ? 'fn' : `fn|${fnKw}`;
    const fnRegex = new RegExp(`\\b(?:${fnWords})\\s+([a-zA-Z_]\\w*)\\s*\\(`, 'g');
    let match;
    while ((match = fnRegex.exec(text)) !== null) {
        const name = match[1];
        if (!seen.has(name)) {
            seen.add(name);
            items.push({ label: name, kind: node_1.CompletionItemKind.Function, detail: 'User-defined function', data: `user_fn_${name}` });
        }
    }
    // User-defined variables: name = value (at start of line or after indent)
    const varRegex = /^\s*([a-zA-Z_]\w*)\s*=/gm;
    while ((match = varRegex.exec(text)) !== null) {
        const name = match[1];
        // Skip canonical keywords AND custom dialect keywords
        const allKeywords = new Set(completions_1.LANG_DATA.allKeywordNames);
        if (dialect) {
            for (const customWord of Object.keys(dialect.customToCanonical)) {
                allKeywords.add(customWord);
            }
        }
        if (!seen.has(name) && !allKeywords.has(name)) {
            seen.add(name);
            items.push({ label: name, kind: node_1.CompletionItemKind.Variable, detail: 'User-defined variable', data: `user_var_${name}` });
        }
    }
    return items;
}
// ── Hover ────────────────────────────────────────────────
connection.onHover((params) => {
    const doc = documents.get(params.textDocument.uri);
    if (!doc)
        return null;
    const text = doc.getText();
    const offset = doc.offsetAt(params.position);
    const word = getWordAtOffset(text, offset);
    if (!word)
        return null;
    // Try direct canonical lookup first
    let info = hover_1.HOVER_INFO[word];
    if (info) {
        return { contents: { kind: node_1.MarkupKind.Markdown, value: `**${info.signature}**\n\n${info.detail}` } };
    }
    // If dialect active, translate custom word → canonical and try again
    const dialect = (0, dialectMap_1.getDialect)(doc.uri, text);
    if (dialect) {
        const canonical = (0, dialectMap_1.translate)(word, dialect);
        if (canonical !== word) {
            info = hover_1.HOVER_INFO[canonical];
            if (info) {
                return {
                    contents: {
                        kind: node_1.MarkupKind.Markdown,
                        value: `**${word}** *(dialect for \`${canonical}\`)*\n\n**${info.signature}**\n\n${info.detail}`
                    }
                };
            }
        }
    }
    return null;
});
function getWordAtOffset(text, offset) {
    let start = offset;
    let end = offset;
    while (start > 0 && /[a-zA-Z_0-9]/.test(text[start - 1]))
        start--;
    while (end < text.length && /[a-zA-Z_0-9]/.test(text[end]))
        end++;
    const word = text.substring(start, end);
    return word.length > 0 ? word : null;
}
// ── Signature Help ───────────────────────────────────────
connection.onSignatureHelp((params) => {
    const doc = documents.get(params.textDocument.uri);
    if (!doc)
        return null;
    const text = doc.getText();
    const offset = doc.offsetAt(params.position);
    const before = text.substring(Math.max(0, offset - 200), offset);
    const match = before.match(/\b([a-zA-Z_]\w*)\s*\([^)]*$/);
    if (!match)
        return null;
    const funcName = match[1];
    // Translate dialect function name to canonical for lookup
    const dialect = (0, dialectMap_1.getDialect)(doc.uri, text);
    const canonicalFunc = (0, dialectMap_1.translate)(funcName, dialect);
    const info = hover_1.HOVER_INFO[canonicalFunc] ?? hover_1.HOVER_INFO[funcName];
    if (!info || !info.params)
        return null;
    const afterParen = match[0].substring(match[0].indexOf('(') + 1);
    const activeParam = (afterParen.match(/,/g) || []).length;
    const parameters = info.params.map(p => {
        const dashIdx = p.indexOf('—');
        const label = dashIdx >= 0 ? p.substring(0, dashIdx).trim() : p;
        const doc = dashIdx >= 0 ? p.substring(dashIdx + 1).trim() : '';
        return { label, documentation: doc || undefined };
    });
    const sig = { label: info.signature, documentation: info.detail, parameters };
    return {
        signatures: [sig],
        activeSignature: 0,
        activeParameter: Math.min(activeParam, parameters.length - 1)
    };
});
// ── Semantic Tokens (for dialect keyword coloring) ───────
// Map canonical keyword categories to semantic token type indices
// These indices match the legend declared in onInitialize
const TOKEN_TYPE_KEYWORD = 0; // 'keyword'
const TOKEN_TYPE_FUNCTION = 1; // 'function'
const TOKEN_TYPE_TYPE = 2; // 'type'
const TOKEN_TYPE_VARIABLE = 3; // 'variable'
const TOKEN_TYPE_NAMESPACE = 4; // 'namespace'
const TOKEN_TYPE_ENUM_MEMBER = 5; // 'enumMember'
const TOKEN_TYPE_OPERATOR = 6; // 'operator'
// Categorize canonical keywords → semantic token type
function getCanonicalTokenType(canonical) {
    // Keywords by category
    const controlFlow = new Set(['if', 'elif', 'else', 'for', 'while', 'in', 'break',
        'continue', 'try', 'catch', 'finally', 'incase', 'loop', 'give']);
    const declarations = new Set(['fn', 'class', 'struct', 'enum', 'module', 'interface',
        'abstract', 'mixin', 'let', 'be', 'immutable']);
    const imports = new Set(['bring', 'from', 'as', 'export', 'requires', 'of']);
    const modifiers = new Set(['private', 'protected', 'public', 'static', 'with',
        'inherits', 'implements', 'async', 'await', 'yield']);
    const operators = new Set(['and', 'or', 'not', 'is', 'eq', 'ne', 'gt', 'lt', 'ge', 'le']);
    const constants = new Set(['true', 'false', 'none']);
    if (controlFlow.has(canonical))
        return TOKEN_TYPE_KEYWORD;
    if (declarations.has(canonical))
        return TOKEN_TYPE_KEYWORD;
    if (imports.has(canonical))
        return TOKEN_TYPE_NAMESPACE;
    if (modifiers.has(canonical))
        return TOKEN_TYPE_KEYWORD;
    if (operators.has(canonical))
        return TOKEN_TYPE_OPERATOR;
    if (constants.has(canonical))
        return TOKEN_TYPE_ENUM_MEMBER;
    // Check if it's a builtin function
    const builtinNames = new Set(completions_1.LANG_DATA.builtins.map(b => b.name));
    if (builtinNames.has(canonical))
        return TOKEN_TYPE_FUNCTION;
    return TOKEN_TYPE_KEYWORD; // default
}
connection.languages.semanticTokens.on((params) => {
    const doc = documents.get(params.textDocument.uri);
    if (!doc)
        return { data: [] };
    const text = doc.getText();
    const dialect = (0, dialectMap_1.getDialect)(doc.uri, text);
    // Only provide semantic tokens for dialect files
    if (!dialect || Object.keys(dialect.customToCanonical).length === 0) {
        return { data: [] };
    }
    const builder = new node_1.SemanticTokensBuilder();
    const lines = text.split('\n');
    for (let li = 0; li < lines.length; li++) {
        const line = lines[li];
        let inString = false;
        let i = 0;
        while (i < line.length) {
            const ch = line[i];
            // Track strings — don't tokenize inside them
            if (ch === '"' && (i === 0 || line[i - 1] !== '\\')) {
                inString = !inString;
                i++;
                continue;
            }
            if (inString) {
                i++;
                continue;
            }
            // Skip comments
            if (ch === '#')
                break;
            // Word boundary: extract identifier
            if (/[a-zA-Z_]/.test(ch)) {
                const wordStart = i;
                let word = '';
                while (i < line.length && /[a-zA-Z_0-9]/.test(line[i])) {
                    word += line[i];
                    i++;
                }
                // Check if this word is a custom dialect keyword
                const canonical = dialect.customToCanonical[word];
                if (canonical) {
                    const tokenType = getCanonicalTokenType(canonical);
                    builder.push(li, wordStart, word.length, tokenType, 0);
                }
                continue;
            }
            i++;
        }
    }
    return builder.build();
});
// ── .xesy File Change Notification ───────────────────────
// When the client notifies about .xesy file changes, invalidate caches
connection.onNotification('xell/xesyFileChanged', (params) => {
    (0, dialectMap_1.invalidateByXesyPath)(params.path);
    // Re-validate all open documents that might use this mapping
    documents.all().forEach(validateTextDocument);
});
// ── Start ────────────────────────────────────────────────
documents.listen(connection);
connection.listen();
//# sourceMappingURL=server.js.map