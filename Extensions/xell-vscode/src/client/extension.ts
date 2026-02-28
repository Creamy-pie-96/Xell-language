// ═══════════════════════════════════════════════════════════
// Xell VS Code Extension — Client (entry point)
// ═══════════════════════════════════════════════════════════

import * as path from 'path';
import * as vscode from 'vscode';
import {
    LanguageClient,
    LanguageClientOptions,
    ServerOptions,
    TransportKind
} from 'vscode-languageclient/node';
import { XellNotebookSerializer } from './notebookSerializer';
import { XellNotebookController } from './notebookController';

let client: LanguageClient;
let notebookController: XellNotebookController;

// ── Xell Token Colors ────────────────────────────────────
// Injected into editor.tokenColorCustomizations so they
// work with ANY theme the user has selected.
const XELL_TOKEN_RULES = [
    // Comments
    { scope: 'comment.block.arrow.xell', settings: { foreground: '#98c379' } },
    { scope: 'comment.line.number-sign.xell', settings: { foreground: '#5c6370', fontStyle: 'italic' } },
    { scope: 'punctuation.definition.comment.begin.xell', settings: { foreground: '#98c379' } },
    { scope: 'punctuation.definition.comment.end.xell', settings: { foreground: '#98c379' } },
    // Literals & Constants
    { scope: 'constant.character.escape.xell', settings: { foreground: '#d19a66' } },
    { scope: 'constant.language.boolean.true.xell', settings: { foreground: '#c678dd' } },
    { scope: 'constant.language.boolean.false.xell', settings: { foreground: '#c678dd' } },
    { scope: 'constant.language.none.xell', settings: { foreground: '#c678dd' } },
    { scope: 'constant.numeric.float.xell', settings: { foreground: '#d19a66' } },
    { scope: 'constant.numeric.integer.xell', settings: { foreground: '#d19a66' } },
    // Strings
    { scope: 'string.quoted.double.xell', settings: { foreground: '#98c379' } },
    { scope: 'string.interpolation.xell', settings: { foreground: '#d19a66' } },
    { scope: 'punctuation.section.interpolation.begin.xell', settings: { foreground: '#c678dd', fontStyle: 'bold' } },
    { scope: 'punctuation.section.interpolation.end.xell', settings: { foreground: '#c678dd', fontStyle: 'bold' } },
    // Functions
    { scope: 'entity.name.function.call.xell', settings: { foreground: '#00ffff' } },
    { scope: 'entity.name.function.definition.xell', settings: { foreground: '#00ffff' } },
    { scope: 'entity.name.function.method.xell', settings: { foreground: '#00ffff' } },
    // Builtins
    { scope: 'support.function.builtin.xell', settings: { foreground: '#00ffff' } },
    { scope: 'support.function.builtin.os.xell', settings: { foreground: '#00ffff' } },
    { scope: 'support.function.math.xell', settings: { foreground: '#00ffff' } },
    { scope: 'support.type.conversion.xell', settings: { foreground: '#008080' } },
    // Keywords
    { scope: 'keyword.control.flow.xell', settings: { foreground: '#e06c75', fontStyle: 'bold' } },
    { scope: 'keyword.control.loop.xell', settings: { foreground: '#e06c75', fontStyle: 'bold' } },
    { scope: 'keyword.control.import.xell', settings: { foreground: '#e06c75', fontStyle: 'bold' } },
    { scope: 'keyword.control.return.xell', settings: { foreground: '#e5c07b', fontStyle: 'bold' } },
    { scope: 'keyword.declaration.function.xell', settings: { foreground: '#e5c07b', fontStyle: 'bold' } },
    { scope: 'keyword.other.special.xell', settings: { foreground: '#e5c07b', fontStyle: 'bold' } },
    // Operators
    { scope: 'keyword.operator.arithmetic.xell', settings: { foreground: '#c678dd' } },
    { scope: 'keyword.operator.assignment.xell', settings: { foreground: '#c678dd' } },
    { scope: 'keyword.operator.comparison.xell', settings: { foreground: '#c678dd' } },
    { scope: 'keyword.operator.comparison.word.xell', settings: { foreground: '#c678dd' } },
    { scope: 'keyword.operator.logical.xell', settings: { foreground: '#c678dd' } },
    { scope: 'keyword.operator.increment.xell', settings: { foreground: '#c678dd' } },
    { scope: 'keyword.operator.access.xell', settings: { foreground: '#61afef', fontStyle: 'bold' } },
    // Punctuation
    { scope: 'punctuation.bracket.round.xell', settings: { foreground: '#abb2bf' } },
    { scope: 'punctuation.bracket.square.xell', settings: { foreground: '#abb2bf' } },
    { scope: 'punctuation.bracket.curly.xell', settings: { foreground: '#abb2bf' } },
    { scope: 'punctuation.separator.colon.xell', settings: { foreground: '#abb2bf' } },
    { scope: 'punctuation.separator.comma.xell', settings: { foreground: '#abb2bf' } },
    { scope: 'punctuation.terminator.block.xell', settings: { foreground: '#abb2bf' } },
    { scope: 'punctuation.terminator.statement.xell', settings: { foreground: '#abb2bf' } },
    // Variables
    { scope: 'variable.other.xell', settings: { foreground: '#eeeeee' } },
    { scope: 'variable.other.loop.xell', settings: { foreground: '#eeeeee' } },
    { scope: 'variable.parameter.xell', settings: { foreground: '#eeeeee' } },
];

/**
 * Inject Xell token colors into the user's settings.
 */
async function applyXellColors() {
    try {
        const config = vscode.workspace.getConfiguration('editor');
        const current: Record<string, unknown> = config.get('tokenColorCustomizations') || {};
        const existingRules: Array<{ scope?: string }> = (current as any).textMateRules || [];

        // Remove existing Xell rules (they'll be replaced)
        const nonXellRules = existingRules.filter(
            (r) => !(typeof r.scope === 'string' && r.scope.endsWith('.xell'))
        );

        // Check if rules are identical (avoid unnecessary writes)
        const currentXellRules = existingRules.filter(
            (r) => typeof r.scope === 'string' && r.scope.endsWith('.xell')
        );
        if (currentXellRules.length === XELL_TOKEN_RULES.length) {
            const same = XELL_TOKEN_RULES.every((newRule, i) => {
                const old = currentXellRules[i] as any;
                return old && old.scope === newRule.scope
                    && old.settings?.foreground === newRule.settings?.foreground
                    && (old.settings?.fontStyle || '') === (newRule.settings?.fontStyle || '');
            });
            if (same) return;
        }

        const merged = {
            ...current,
            textMateRules: [...nonXellRules, ...XELL_TOKEN_RULES]
        };

        await config.update('tokenColorCustomizations', merged, vscode.ConfigurationTarget.Global);
        console.log('Xell: Token colors applied/updated.');
    } catch (err) {
        console.error('Xell: Failed to apply token colors:', err);
    }
}

export function activate(context: vscode.ExtensionContext) {
    console.log('Xell extension activating...');

    // ── Apply Colors ─────────────────────────────────────
    applyXellColors();

    // ── Notebook Support (register FIRST) ────────────────
    context.subscriptions.push(
        vscode.workspace.registerNotebookSerializer(
            'xell-notebook',
            new XellNotebookSerializer(),
            { transientOutputs: false }
        )
    );

    notebookController = new XellNotebookController();
    context.subscriptions.push({ dispose: () => notebookController.dispose() });

    // ── Language Server ──────────────────────────────────
    try {
        const serverModule = context.asAbsolutePath(
            path.join('out', 'server', 'server.js')
        );

        const debugOptions = { execArgv: ['--nolazy', '--inspect=6009'] };

        const serverOptions: ServerOptions = {
            run: { module: serverModule, transport: TransportKind.ipc },
            debug: {
                module: serverModule,
                transport: TransportKind.ipc,
                options: debugOptions
            }
        };

        const clientOptions: LanguageClientOptions = {
            documentSelector: [
                { scheme: 'file', language: 'xell' },
                { scheme: 'untitled', language: 'xell' }
            ],
            synchronize: {
                configurationSection: 'xell',
                fileEvents: vscode.workspace.createFileSystemWatcher('**/*.xel')
            }
        };

        client = new LanguageClient(
            'xellLanguageServer',
            'Xell Language Server',
            serverOptions,
            clientOptions
        );

        client.start();
    } catch (err) {
        console.error('Xell: Language server failed to start:', err);
        vscode.window.showWarningMessage(
            'Xell language server failed to start. Syntax highlighting and notebooks still work.'
        );
    }

    // ── Run File Command ─────────────────────────────────
    const runFileCmd = vscode.commands.registerCommand('xell.runFile', () => {
        const editor = vscode.window.activeTextEditor;
        if (!editor) {
            vscode.window.showWarningMessage('No active Xell file to run.');
            return;
        }

        const filePath = editor.document.fileName;
        const config = vscode.workspace.getConfiguration('xell');
        const xellPath = config.get<string>('xellPath', 'xell');

        editor.document.save().then(() => {
            const terminal = vscode.window.createTerminal('Xell');
            terminal.show();
            terminal.sendText(`${xellPath} --script < "${filePath}"`);
        });
    });

    // ── Run Selection Command ────────────────────────────
    const runSelectionCmd = vscode.commands.registerCommand('xell.runSelection', () => {
        const editor = vscode.window.activeTextEditor;
        if (!editor) return;

        const selection = editor.document.getText(editor.selection);
        if (!selection) {
            vscode.window.showWarningMessage('No text selected.');
            return;
        }

        const config = vscode.workspace.getConfiguration('xell');
        const xellPath = config.get<string>('xellPath', 'xell');

        const terminal = vscode.window.createTerminal('Xell');
        terminal.show();
        terminal.sendText(`echo '${selection.replace(/'/g, "'\\''")}' | ${xellPath} --script`);
    });

    // ── Open Notebook Command ────────────────────────────
    const openNotebookCmd = vscode.commands.registerCommand('xell.openNotebook', () => {
        const terminal = vscode.window.createTerminal('Xell Notebook');
        terminal.show();
        terminal.sendText('xell-notebook || echo "Notebook server not found. Install Xell system-wide first."');
    });

    context.subscriptions.push(runFileCmd, runSelectionCmd, openNotebookCmd);

    console.log('Xell extension activated.');
}

export function deactivate(): Thenable<void> | undefined {
    if (!client) {
        return undefined;
    }
    return client.stop();
}
