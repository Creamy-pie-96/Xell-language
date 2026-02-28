// ═══════════════════════════════════════════════════════════
// ScriptIt VS Code Extension — Client (entry point)
// ═══════════════════════════════════════════════════════════

import * as path from 'path';
import * as vscode from 'vscode';
import {
    LanguageClient,
    LanguageClientOptions,
    ServerOptions,
    TransportKind
} from 'vscode-languageclient/node';
import { ScriptItNotebookSerializer } from './notebookSerializer';
import { ScriptItNotebookController } from './notebookController';

let client: LanguageClient;
let notebookController: ScriptItNotebookController;

// ── ScriptIt Token Colors ────────────────────────────────
// These get injected into editor.tokenColorCustomizations
// so they work with ANY theme the user has selected.
const SCRIPTIT_TOKEN_RULES = [
    { scope: 'keyword.control.flow.scriptit', settings: { foreground: '#ff0000', fontStyle: 'bold' } },
];

/**
 * Inject ScriptIt token colors into the user's settings.
 * configurationDefaults doesn't reliably work for tokenColorCustomizations,
 * so we set them programmatically. Replaces any existing ScriptIt rules
 * with the current SCRIPTIT_TOKEN_RULES (so color customizer changes take effect).
 */
async function applyScriptItColors() {
    try {
        const config = vscode.workspace.getConfiguration('editor');
        const current: Record<string, unknown> = config.get('tokenColorCustomizations') || {};
        const existingRules: Array<{ scope?: string }> = (current as any).textMateRules || [];

        // Remove any existing ScriptIt rules (they'll be replaced with current ones)
        const nonScriptItRules = existingRules.filter(
            (r) => !(typeof r.scope === 'string' && r.scope.endsWith('.scriptit'))
        );

        // Check if rules are identical (avoid unnecessary writes)
        const currentScriptItRules = existingRules.filter(
            (r) => typeof r.scope === 'string' && r.scope.endsWith('.scriptit')
        );
        if (currentScriptItRules.length === SCRIPTIT_TOKEN_RULES.length) {
            const same = SCRIPTIT_TOKEN_RULES.every((newRule, i) => {
                const old = currentScriptItRules[i] as any;
                return old && old.scope === newRule.scope
                    && old.settings?.foreground === newRule.settings?.foreground
                    && (old.settings?.fontStyle || '') === (newRule.settings?.fontStyle || '');
            });
            if (same) return; // No changes needed
        }

        // Merge: user's non-ScriptIt rules + our updated rules
        const merged = {
            ...current,
            textMateRules: [...nonScriptItRules, ...SCRIPTIT_TOKEN_RULES]
        };

        await config.update('tokenColorCustomizations', merged, vscode.ConfigurationTarget.Global);
        console.log('ScriptIt: Token colors applied/updated.');
    } catch (err) {
        console.error('ScriptIt: Failed to apply token colors:', err);
    }
}

export function activate(context: vscode.ExtensionContext) {
    console.log('ScriptIt extension activating...');

    // ── Apply Colors ─────────────────────────────────────
    applyScriptItColors();

    // ── Notebook Support (register FIRST — must not be blocked by LSP) ──
    context.subscriptions.push(
        vscode.workspace.registerNotebookSerializer(
            'scriptit-notebook',
            new ScriptItNotebookSerializer(),
            { transientOutputs: false }
        )
    );

    notebookController = new ScriptItNotebookController();
    context.subscriptions.push({ dispose: () => notebookController.dispose() });

    // ── Language Server (wrapped in try/catch — must not break notebook) ──
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
                { scheme: 'file', language: 'scriptit' },
                { scheme: 'untitled', language: 'scriptit' }
            ],
            synchronize: {
                configurationSection: 'scriptit',
                fileEvents: vscode.workspace.createFileSystemWatcher('**/*.{si,sit}')
            }
        };

        client = new LanguageClient(
            'scriptitLanguageServer',
            'ScriptIt Language Server',
            serverOptions,
            clientOptions
        );

        client.start();
    } catch (err) {
        console.error('ScriptIt: Language server failed to start:', err);
        vscode.window.showWarningMessage(
            'ScriptIt language server failed to start. Syntax highlighting and notebooks still work.'
        );
    }

    // ── Run File Command ─────────────────────────────────
    const runFileCmd = vscode.commands.registerCommand('scriptit.runFile', () => {
        const editor = vscode.window.activeTextEditor;
        if (!editor) {
            vscode.window.showWarningMessage('No active ScriptIt file to run.');
            return;
        }

        const filePath = editor.document.fileName;
        const config = vscode.workspace.getConfiguration('scriptit');
        const scriptitPath = config.get<string>('scriptitPath', 'scriptit');

        // Save before running
        editor.document.save().then(() => {
            const terminal = vscode.window.createTerminal('ScriptIt');
            terminal.show();
            terminal.sendText(`${scriptitPath} --script < "${filePath}"`);
        });
    });

    // ── Run Selection Command ────────────────────────────
    const runSelectionCmd = vscode.commands.registerCommand('scriptit.runSelection', () => {
        const editor = vscode.window.activeTextEditor;
        if (!editor) return;

        const selection = editor.document.getText(editor.selection);
        if (!selection) {
            vscode.window.showWarningMessage('No text selected.');
            return;
        }

        const config = vscode.workspace.getConfiguration('scriptit');
        const scriptitPath = config.get<string>('scriptitPath', 'scriptit');

        const terminal = vscode.window.createTerminal('ScriptIt');
        terminal.show();
        terminal.sendText(`echo '${selection.replace(/'/g, "'\\''")}' | ${scriptitPath} --script`);
    });

    // ── Open Notebook Command ────────────────────────────
    const openNotebookCmd = vscode.commands.registerCommand('scriptit.openNotebook', () => {
        const config = vscode.workspace.getConfiguration('scriptit');
        const scriptitPath = config.get<string>('scriptitPath', 'scriptit');

        // Try to find the notebook server script
        const terminal = vscode.window.createTerminal('ScriptIt Notebook');
        terminal.show();
        terminal.sendText('scriptit-notebook || echo "Notebook server not found. Install ScriptIt system-wide first."');
    });

    context.subscriptions.push(runFileCmd, runSelectionCmd, openNotebookCmd);

    console.log('ScriptIt extension activated.');
}

export function deactivate(): Thenable<void> | undefined {
    if (!client) {
        return undefined;
    }
    return client.stop();
}
