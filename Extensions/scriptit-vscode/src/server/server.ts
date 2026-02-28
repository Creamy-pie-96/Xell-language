// ═══════════════════════════════════════════════════════════
// ScriptIt Language Server
// ═══════════════════════════════════════════════════════════
//
// Provides:
//   • Diagnostics (error squiggles from scriptit --script)
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
import { ScriptItDiagnostics } from './diagnostics';
import { SCRIPTIT_KEYWORDS, SCRIPTIT_BUILTINS, SCRIPTIT_MATH, SCRIPTIT_TYPES, ALL_COMPLETIONS } from './completions';
import { HOVER_INFO } from './hover';

// ── Connection & Document Manager ────────────────────────

const connection = createConnection(ProposedFeatures.all);
const documents: TextDocuments<TextDocument> = new TextDocuments(TextDocument);

let hasConfigurationCapability = false;
let diagnosticsEngine: ScriptItDiagnostics;

// ── Settings ─────────────────────────────────────────────

interface ScriptItSettings {
    scriptitPath: string;
    maxNumberOfProblems: number;
    enableLinting: boolean;
}

const defaultSettings: ScriptItSettings = {
    scriptitPath: 'scriptit',
    maxNumberOfProblems: 100,
    enableLinting: true
};

let globalSettings: ScriptItSettings = defaultSettings;
const documentSettings: Map<string, Thenable<ScriptItSettings>> = new Map();

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
                triggerCharacters: ['.', '(', '@']
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
    diagnosticsEngine = new ScriptItDiagnostics(connection);
});

// ── Configuration ────────────────────────────────────────

function getDocumentSettings(resource: string): Thenable<ScriptItSettings> {
    if (!hasConfigurationCapability) {
        return Promise.resolve(globalSettings);
    }
    let result = documentSettings.get(resource);
    if (!result) {
        result = connection.workspace.getConfiguration({
            scopeUri: resource,
            section: 'scriptit'
        });
        documentSettings.set(resource, result);
    }
    return result;
}

connection.onDidChangeConfiguration(change => {
    if (hasConfigurationCapability) {
        documentSettings.clear();
    } else {
        globalSettings = (change.settings.scriptit || defaultSettings) as ScriptItSettings;
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

    // ── Static analysis (fast, no subprocess) ──
    const lines = text.split('\n');

    // Track bracket balance
    const bracketStack: { char: string; line: number; col: number }[] = [];
    const bracketPairs: Record<string, string> = { '(': ')', '{': '}', '[': ']' };
    const closingToOpening: Record<string, string> = { ')': '(', '}': '{', ']': '[' };

    for (let i = 0; i < lines.length; i++) {
        const line = lines[i];

        // Skip comments (but respect strings — # inside "..." is NOT a comment)
        let commentIdx = -1;
        {
            let inStr = false;
            let strCh = '';
            for (let k = 0; k < line.length; k++) {
                const ch = line[k];
                if (inStr) {
                    if (ch === '\\') { k++; continue; }
                    if (ch === strCh) inStr = false;
                    continue;
                }
                if (ch === '"' || ch === "'") { inStr = true; strCh = ch; continue; }
                if (ch === '#') { commentIdx = k; break; }
            }
        }
        const codePart = commentIdx >= 0 ? line.substring(0, commentIdx) : line;

        // Track brackets (outside strings)
        let inString = false;
        let stringChar = '';
        for (let j = 0; j < codePart.length; j++) {
            const ch = codePart[j];

            if (inString) {
                if (ch === '\\') { j++; continue; }
                if (ch === stringChar) inString = false;
                continue;
            }

            if (ch === '"' || ch === "'") {
                inString = true;
                stringChar = ch;
                continue;
            }

            if (ch === '(' || ch === '{' || ch === '[') {
                bracketStack.push({ char: ch, line: i, col: j });
            } else if (ch === ')' || ch === '}' || ch === ']') {
                const expected = closingToOpening[ch];
                if (bracketStack.length === 0) {
                    diagnostics.push({
                        severity: DiagnosticSeverity.Error,
                        range: {
                            start: { line: i, character: j },
                            end: { line: i, character: j + 1 }
                        },
                        message: `Unmatched closing bracket '${ch}'`,
                        source: 'scriptit'
                    });
                } else if (bracketStack[bracketStack.length - 1].char !== expected) {
                    const top = bracketStack[bracketStack.length - 1];
                    diagnostics.push({
                        severity: DiagnosticSeverity.Error,
                        range: {
                            start: { line: i, character: j },
                            end: { line: i, character: j + 1 }
                        },
                        message: `Mismatched bracket: expected '${bracketPairs[top.char]}' but found '${ch}'`,
                        source: 'scriptit'
                    });
                    bracketStack.pop();
                } else {
                    bracketStack.pop();
                }
            }
        }

        // Check for unterminated strings
        if (inString) {
            diagnostics.push({
                severity: DiagnosticSeverity.Error,
                range: {
                    start: { line: i, character: 0 },
                    end: { line: i, character: line.length }
                },
                message: `Unterminated string literal`,
                source: 'scriptit'
            });
        }

        // Warn about common mistakes
        const trimmed = codePart.trim();

        // Check for = vs == in if/elif/while conditions
        if (/^(if|elif|while)\s+/.test(trimmed)) {
            // Extract condition part (after keyword, before :)
            const condMatch = trimmed.match(/^(?:if|elif|while)\s+(.+?):\s*$/);
            if (condMatch) {
                const condition = condMatch[1];
                // Look for single = that's not == or != or <= or >=
                if (/(?<![=!<>])=(?!=)/.test(condition)) {
                    const eqIdx = codePart.indexOf('=', codePart.indexOf(condMatch[1]));
                    diagnostics.push({
                        severity: DiagnosticSeverity.Warning,
                        range: {
                            start: { line: i, character: eqIdx },
                            end: { line: i, character: eqIdx + 1 }
                        },
                        message: `Possible assignment in condition. Did you mean '=='?`,
                        source: 'scriptit'
                    });
                }
            }
        }

        // Check fn definition missing colon
        if (/^\s*fn\s+[a-zA-Z_]\w*\s*\([^)]*\)\s*$/.test(line)) {
            diagnostics.push({
                severity: DiagnosticSeverity.Error,
                range: {
                    start: { line: i, character: 0 },
                    end: { line: i, character: line.length }
                },
                message: `Function definition missing ':' or '.' (use ':' for body, '.' for forward declaration)`,
                source: 'scriptit'
            });
        }
    }

    // Report unclosed brackets
    for (const bracket of bracketStack) {
        diagnostics.push({
            severity: DiagnosticSeverity.Error,
            range: {
                start: { line: bracket.line, character: bracket.col },
                end: { line: bracket.line, character: bracket.col + 1 }
            },
            message: `Unclosed bracket '${bracket.char}'`,
            source: 'scriptit'
        });
    }

    // ── Subprocess validation (optional, runs scriptit) ──
    if (diagnosticsEngine) {
        const subprocessDiags = await diagnosticsEngine.validate(text, settings.scriptitPath);
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

        // Check if we're after a dot (method call)
        const beforeCursor = text.substring(Math.max(0, offset - 50), offset);
        if (beforeCursor.match(/\.\s*$/)) {
            return getMethodCompletions();
        }

        // Add user-defined identifiers
        const userCompletions = extractUserIdentifiers(text);

        return [...ALL_COMPLETIONS, ...userCompletions];
    }
);

connection.onCompletionResolve((item: CompletionItem): CompletionItem => {
    // Add documentation to completion items
    const info = HOVER_INFO[item.label];
    if (info) {
        item.documentation = {
            kind: MarkupKind.Markdown,
            value: info.detail
        };
    }
    return item;
});

function getMethodCompletions(): CompletionItem[] {
    const methods = [
        // String methods
        { label: 'upper', detail: 'Convert to uppercase', kind: CompletionItemKind.Method },
        { label: 'lower', detail: 'Convert to lowercase', kind: CompletionItemKind.Method },
        { label: 'strip', detail: 'Remove leading/trailing whitespace', kind: CompletionItemKind.Method },
        { label: 'split', detail: 'Split by delimiter', kind: CompletionItemKind.Method },
        { label: 'replace', detail: 'Replace substring', kind: CompletionItemKind.Method },
        { label: 'find', detail: 'Find index of substring', kind: CompletionItemKind.Method },
        { label: 'startswith', detail: 'Check if starts with prefix', kind: CompletionItemKind.Method },
        { label: 'endswith', detail: 'Check if ends with suffix', kind: CompletionItemKind.Method },
        { label: 'contains', detail: 'Check if contains substring', kind: CompletionItemKind.Method },
        { label: 'count', detail: 'Count occurrences', kind: CompletionItemKind.Method },
        { label: 'reverse', detail: 'Reverse the string/list', kind: CompletionItemKind.Method },
        { label: 'capitalize', detail: 'Capitalize first letter', kind: CompletionItemKind.Method },
        { label: 'title', detail: 'Title case', kind: CompletionItemKind.Method },
        // List methods
        { label: 'append', detail: 'Add item to list', kind: CompletionItemKind.Method },
        { label: 'pop', detail: 'Remove and return last item', kind: CompletionItemKind.Method },
        { label: 'sort', detail: 'Sort in place', kind: CompletionItemKind.Method },
        { label: 'index', detail: 'Find index of item', kind: CompletionItemKind.Method },
        { label: 'insert', detail: 'Insert at index', kind: CompletionItemKind.Method },
        { label: 'remove', detail: 'Remove first occurrence', kind: CompletionItemKind.Method },
        { label: 'extend', detail: 'Extend with another list', kind: CompletionItemKind.Method },
        // Set methods
        { label: 'add', detail: 'Add element to set', kind: CompletionItemKind.Method },
        { label: 'union', detail: 'Union of sets', kind: CompletionItemKind.Method },
        { label: 'intersection', detail: 'Intersection of sets', kind: CompletionItemKind.Method },
        { label: 'difference', detail: 'Difference of sets', kind: CompletionItemKind.Method },
        // File methods
        { label: 'read', detail: 'Read file contents', kind: CompletionItemKind.Method },
        { label: 'write', detail: 'Write to file', kind: CompletionItemKind.Method },
        { label: 'close', detail: 'Close file handle', kind: CompletionItemKind.Method },
        { label: 'readlines', detail: 'Read all lines', kind: CompletionItemKind.Method },
    ];
    return methods.map(m => ({
        label: m.label,
        kind: m.kind,
        detail: m.detail,
        data: `method_${m.label}`
    }));
}

function extractUserIdentifiers(text: string): CompletionItem[] {
    const items: CompletionItem[] = [];
    const seen = new Set<string>();

    // Extract function names: fn name(
    const fnRegex = /\bfn\s+([a-zA-Z_]\w*)\s*\(/g;
    let match;
    while ((match = fnRegex.exec(text)) !== null) {
        const name = match[1];
        if (!seen.has(name)) {
            seen.add(name);
            items.push({
                label: name,
                kind: CompletionItemKind.Function,
                detail: 'User-defined function',
                data: `user_fn_${name}`
            });
        }
    }

    // Extract variable names: var name =
    const varRegex = /\bvar\s+([a-zA-Z_]\w*)\s*=/g;
    while ((match = varRegex.exec(text)) !== null) {
        const name = match[1];
        if (!seen.has(name)) {
            seen.add(name);
            items.push({
                label: name,
                kind: CompletionItemKind.Variable,
                detail: 'User-defined variable',
                data: `user_var_${name}`
            });
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

    // Get the word at cursor
    const word = getWordAtOffset(text, offset);
    if (!word) return null;

    const info = HOVER_INFO[word];
    if (info) {
        return {
            contents: {
                kind: MarkupKind.Markdown,
                value: `**${info.signature}**\n\n${info.detail}`
            }
        };
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

    // Walk backwards to find the function name before '('
    const before = text.substring(Math.max(0, offset - 200), offset);
    const match = before.match(/\b([a-zA-Z_]\w*)\s*\([^)]*$/);
    if (!match) return null;

    const funcName = match[1];
    const info = HOVER_INFO[funcName];
    if (!info || !info.params) return null;

    // Count commas to determine active parameter
    const afterParen = match[0].substring(match[0].indexOf('(') + 1);
    const activeParam = (afterParen.match(/,/g) || []).length;

    const parameters: ParameterInformation[] = info.params.map(p => {
        // Params are strings like "x — description"
        const dashIdx = p.indexOf('—');
        const label = dashIdx >= 0 ? p.substring(0, dashIdx).trim() : p;
        const doc = dashIdx >= 0 ? p.substring(dashIdx + 1).trim() : '';
        return { label, documentation: doc || undefined };
    });

    const sig: SignatureInformation = {
        label: info.signature,
        documentation: info.detail,
        parameters
    };

    return {
        signatures: [sig],
        activeSignature: 0,
        activeParameter: Math.min(activeParam, parameters.length - 1)
    };
});

// ── Start ────────────────────────────────────────────────

documents.listen(connection);
connection.listen();
