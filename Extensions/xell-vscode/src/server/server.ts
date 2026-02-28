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

import {
    createConnection,
    TextDocuments,
    Diagnostic,
    DiagnosticSeverity,
    ProposedFeatures,
    InitializeParams,
    InitializeResult,
    TextDocumentSyncKind,
    CompletionItem,
    CompletionItemKind,
    TextDocumentPositionParams,
    DidChangeConfigurationNotification,
    Hover,
    MarkupKind,
    SignatureHelp,
    SignatureInformation,
    ParameterInformation
} from 'vscode-languageserver/node';

import { TextDocument } from 'vscode-languageserver-textdocument';
import { XellDiagnostics } from './diagnostics';
import { XELL_KEYWORDS, XELL_BUILTINS, XELL_OS_BUILTINS, XELL_MATH, ALL_COMPLETIONS } from './completions';
import { HOVER_INFO } from './hover';

// ── Connection & Document Manager ────────────────────────

const connection = createConnection(ProposedFeatures.all);
const documents: TextDocuments<TextDocument> = new TextDocuments(TextDocument);

let hasConfigurationCapability = false;
let diagnosticsEngine: XellDiagnostics;

// ── Settings ─────────────────────────────────────────────

interface XellSettings {
    xellPath: string;
    maxNumberOfProblems: number;
    enableLinting: boolean;
}

const defaultSettings: XellSettings = {
    xellPath: 'xell',
    maxNumberOfProblems: 100,
    enableLinting: true
};

let globalSettings: XellSettings = defaultSettings;
const documentSettings: Map<string, Thenable<XellSettings>> = new Map();

// ── Initialize ───────────────────────────────────────────

connection.onInitialize((params: InitializeParams) => {
    const capabilities = params.capabilities;
    hasConfigurationCapability = !!(
        capabilities.workspace && !!capabilities.workspace.configuration
    );
    const result: InitializeResult = {
        capabilities: {
            textDocumentSync: TextDocumentSyncKind.Incremental,
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
        connection.client.register(DidChangeConfigurationNotification.type, undefined);
    }
    diagnosticsEngine = new XellDiagnostics(connection);
});

// ── Configuration ────────────────────────────────────────

function getDocumentSettings(resource: string): Thenable<XellSettings> {
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
    } else {
        globalSettings = (change.settings.xell || defaultSettings) as XellSettings;
    }
    documents.all().forEach(validateTextDocument);
});

documents.onDidClose(e => {
    documentSettings.delete(e.document.uri);
});

// ── Diagnostics ──────────────────────────────────────────

documents.onDidChangeContent(change => {
    validateTextDocument(change.document);
});

async function validateTextDocument(textDocument: TextDocument): Promise<void> {
    const settings = await getDocumentSettings(textDocument.uri);

    if (!settings.enableLinting) {
        connection.sendDiagnostics({ uri: textDocument.uri, diagnostics: [] });
        return;
    }

    const text = textDocument.getText();
    const diagnostics: Diagnostic[] = [];

    const lines = text.split('\n');
    const bracketStack: { char: string; line: number; col: number }[] = [];
    const bracketPairs: Record<string, string> = { '(': ')', '{': '}', '[': ']' };
    const closingToOpening: Record<string, string> = { ')': '(', '}': '{', ']': '[' };

    for (let i = 0; i < lines.length; i++) {
        const line = lines[i];

        // Find comment start (skip strings)
        let commentIdx = -1;
        {
            let inStr = false;
            for (let k = 0; k < line.length; k++) {
                const ch = line[k];
                if (inStr) {
                    if (ch === '\\') { k++; continue; }
                    if (ch === '"') inStr = false;
                    continue;
                }
                if (ch === '"') { inStr = true; continue; }
                if (ch === '#') { commentIdx = k; break; }
            }
        }
        const codePart = commentIdx >= 0 ? line.substring(0, commentIdx) : line;

        // Check brackets
        let inString = false;
        for (let j = 0; j < codePart.length; j++) {
            const ch = codePart[j];
            if (inString) {
                if (ch === '\\') { j++; continue; }
                if (ch === '"') inString = false;
                continue;
            }
            if (ch === '"') { inString = true; continue; }
            if (ch === '(' || ch === '{' || ch === '[') {
                bracketStack.push({ char: ch, line: i, col: j });
            } else if (ch === ')' || ch === '}' || ch === ']') {
                const expected = closingToOpening[ch];
                if (bracketStack.length === 0) {
                    diagnostics.push({
                        severity: DiagnosticSeverity.Error,
                        range: { start: { line: i, character: j }, end: { line: i, character: j + 1 } },
                        message: `Unmatched closing bracket '${ch}'`,
                        source: 'xell'
                    });
                } else if (bracketStack[bracketStack.length - 1].char !== expected) {
                    const top = bracketStack[bracketStack.length - 1];
                    diagnostics.push({
                        severity: DiagnosticSeverity.Error,
                        range: { start: { line: i, character: j }, end: { line: i, character: j + 1 } },
                        message: `Mismatched bracket: expected '${bracketPairs[top.char]}' but found '${ch}'`,
                        source: 'xell'
                    });
                    bracketStack.pop();
                } else {
                    bracketStack.pop();
                }
            }
        }

        if (inString) {
            diagnostics.push({
                severity: DiagnosticSeverity.Error,
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
                        severity: DiagnosticSeverity.Warning,
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
                severity: DiagnosticSeverity.Error,
                range: { start: { line: i, character: 0 }, end: { line: i, character: line.length } },
                message: `Function definition missing ':' after parameters`,
                source: 'xell'
            });
        }
    }

    // Unclosed brackets
    for (const bracket of bracketStack) {
        diagnostics.push({
            severity: DiagnosticSeverity.Error,
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

connection.onCompletion(
    (params: TextDocumentPositionParams): CompletionItem[] => {
        const doc = documents.get(params.textDocument.uri);
        if (!doc) return ALL_COMPLETIONS;

        const text = doc.getText();
        const offset = doc.offsetAt(params.position);

        // Check for -> access
        const beforeCursor = text.substring(Math.max(0, offset - 50), offset);
        if (beforeCursor.match(/->\s*$/)) {
            return getMapKeyCompletions();
        }

        const userCompletions = extractUserIdentifiers(text);
        return [...ALL_COMPLETIONS, ...userCompletions];
    }
);

connection.onCompletionResolve((item: CompletionItem): CompletionItem => {
    const info = HOVER_INFO[item.label];
    if (info) {
        item.documentation = { kind: MarkupKind.Markdown, value: info.detail };
    }
    return item;
});

function getMapKeyCompletions(): CompletionItem[] {
    // Generic suggestions for map/object access
    return [
        { label: 'host', kind: CompletionItemKind.Property, detail: 'Map key', data: 'key_host' },
        { label: 'port', kind: CompletionItemKind.Property, detail: 'Map key', data: 'key_port' },
    ];
}

function extractUserIdentifiers(text: string): CompletionItem[] {
    const items: CompletionItem[] = [];
    const seen = new Set<string>();

    // User-defined functions: fn name(
    const fnRegex = /\bfn\s+([a-zA-Z_]\w*)\s*\(/g;
    let match;
    while ((match = fnRegex.exec(text)) !== null) {
        const name = match[1];
        if (!seen.has(name)) {
            seen.add(name);
            items.push({ label: name, kind: CompletionItemKind.Function, detail: 'User-defined function', data: `user_fn_${name}` });
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
            items.push({ label: name, kind: CompletionItemKind.Variable, detail: 'User-defined variable', data: `user_var_${name}` });
        }
    }

    return items;
}

// ── Hover ────────────────────────────────────────────────

connection.onHover((params: TextDocumentPositionParams): Hover | null => {
    const doc = documents.get(params.textDocument.uri);
    if (!doc) return null;

    const text = doc.getText();
    const offset = doc.offsetAt(params.position);
    const word = getWordAtOffset(text, offset);
    if (!word) return null;

    const info = HOVER_INFO[word];
    if (info) {
        return { contents: { kind: MarkupKind.Markdown, value: `**${info.signature}**\n\n${info.detail}` } };
    }
    return null;
});

function getWordAtOffset(text: string, offset: number): string | null {
    let start = offset;
    let end = offset;
    while (start > 0 && /[a-zA-Z_0-9]/.test(text[start - 1])) start--;
    while (end < text.length && /[a-zA-Z_0-9]/.test(text[end])) end++;
    const word = text.substring(start, end);
    return word.length > 0 ? word : null;
}

// ── Signature Help ───────────────────────────────────────

connection.onSignatureHelp((params: TextDocumentPositionParams): SignatureHelp | null => {
    const doc = documents.get(params.textDocument.uri);
    if (!doc) return null;

    const text = doc.getText();
    const offset = doc.offsetAt(params.position);
    const before = text.substring(Math.max(0, offset - 200), offset);
    const match = before.match(/\b([a-zA-Z_]\w*)\s*\([^)]*$/);
    if (!match) return null;

    const funcName = match[1];
    const info = HOVER_INFO[funcName];
    if (!info || !info.params) return null;

    const afterParen = match[0].substring(match[0].indexOf('(') + 1);
    const activeParam = (afterParen.match(/,/g) || []).length;

    const parameters: ParameterInformation[] = info.params.map(p => {
        const dashIdx = p.indexOf('—');
        const label = dashIdx >= 0 ? p.substring(0, dashIdx).trim() : p;
        const doc = dashIdx >= 0 ? p.substring(dashIdx + 1).trim() : '';
        return { label, documentation: doc || undefined };
    });

    const sig: SignatureInformation = { label: info.signature, documentation: info.detail, parameters };

    return {
        signatures: [sig],
        activeSignature: 0,
        activeParameter: Math.min(activeParam, parameters.length - 1)
    };
});

// ── Start ────────────────────────────────────────────────

documents.listen(connection);
connection.listen();
