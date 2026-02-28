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
Object.defineProperty(exports, "__esModule", { value: true });
const node_1 = require("vscode-languageserver/node");
const vscode_languageserver_textdocument_1 = require("vscode-languageserver-textdocument");
const diagnostics_1 = require("./diagnostics");
const completions_1 = require("./completions");
const hover_1 = require("./hover");
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
    documents.all().forEach(validateTextDocument);
});
documents.onDidClose(e => {
    documentSettings.delete(e.document.uri);
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
        // Warn about assignment in condition
        if (/^(if|elif|while)\s+/.test(trimmed)) {
            const condMatch = trimmed.match(/^(?:if|elif|while)\s+(.+?):\s*$/);
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
        // Check function definition missing colon
        if (/^\s*fn\s+[a-zA-Z_]\w*\s*\([^)]*\)\s*$/.test(line)) {
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
        const subprocessDiags = await diagnosticsEngine.validate(text, settings.xellPath);
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
    const userCompletions = extractUserIdentifiers(text);
    return [...completions_1.ALL_COMPLETIONS, ...userCompletions];
});
connection.onCompletionResolve((item) => {
    const info = hover_1.HOVER_INFO[item.label];
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
function extractUserIdentifiers(text) {
    const items = [];
    const seen = new Set();
    // User-defined functions: fn name(
    const fnRegex = /\bfn\s+([a-zA-Z_]\w*)\s*\(/g;
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
        // Skip keywords
        const keywords = new Set(['fn', 'if', 'elif', 'else', 'for', 'while', 'give', 'bring', 'true', 'false', 'none']);
        if (!seen.has(name) && !keywords.has(name)) {
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
    const info = hover_1.HOVER_INFO[word];
    if (info) {
        return { contents: { kind: node_1.MarkupKind.Markdown, value: `**${info.signature}**\n\n${info.detail}` } };
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
    const info = hover_1.HOVER_INFO[funcName];
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
// ── Start ────────────────────────────────────────────────
documents.listen(connection);
connection.listen();
//# sourceMappingURL=server.js.map