// ═══════════════════════════════════════════════════════════
// Xell Notebook Controller (Kernel)
// ═══════════════════════════════════════════════════════════
//
// Manages the Xell kernel subprocess and executes
// notebook cells via the --kernel JSON protocol.
//
// ═══════════════════════════════════════════════════════════

import * as vscode from 'vscode';
import { spawn, ChildProcess } from 'child_process';
import * as readline from 'readline';

export class XellNotebookController {
    readonly controllerId = 'xell-kernel';
    readonly notebookType = 'xell-notebook';
    readonly label = 'Xell Kernel';
    readonly supportedLanguages = ['xell'];

    private controller: vscode.NotebookController;
    private kernelProcess: ChildProcess | null = null;
    private executionOrder = 0;
    private responseBuffer: Map<string, (data: KernelResponse) => void> = new Map();
    private rl: readline.Interface | null = null;

    constructor() {
        this.controller = vscode.notebooks.createNotebookController(
            this.controllerId,
            this.notebookType,
            this.label
        );

        this.controller.supportedLanguages = this.supportedLanguages;
        this.controller.supportsExecutionOrder = true;
        this.controller.executeHandler = this.execute.bind(this);
        this.controller.interruptHandler = this.interrupt.bind(this);
    }

    dispose() {
        this.stopKernel();
        this.controller.dispose();
    }

    // ── Kernel Lifecycle ─────────────────────────────────

    private async ensureKernel(): Promise<boolean> {
        if (this.kernelProcess && this.kernelProcess.exitCode === null) {
            return true; // already running
        }

        const config = vscode.workspace.getConfiguration('xell');
        const xellPath = config.get<string>('xellPath', 'xell');

        try {
            this.kernelProcess = spawn(xellPath, ['--kernel'], {
                stdio: ['pipe', 'pipe', 'pipe'],
                windowsHide: true,
            });

            // Read line-by-line from stdout
            this.rl = readline.createInterface({
                input: this.kernelProcess.stdout!,
                crlfDelay: Infinity
            });

            // Wait for kernel_ready
            const readyLine = await this.readLine();
            const readyMsg = JSON.parse(readyLine);
            if (readyMsg.status !== 'kernel_ready') {
                vscode.window.showErrorMessage(`Xell kernel failed to start: ${readyLine}`);
                return false;
            }

            // Set up continuous response reading
            this.listenForResponses();

            console.log(`[Xell Kernel] Started (PID ${this.kernelProcess.pid})`);
            return true;
        } catch (err: unknown) {
            const msg = err instanceof Error ? err.message : String(err);
            vscode.window.showErrorMessage(
                `Failed to start Xell kernel: ${msg}\n\nMake sure '${xellPath}' is installed.`
            );
            return false;
        }
    }

    private readLine(): Promise<string> {
        return new Promise((resolve) => {
            if (!this.rl) { resolve(''); return; }
            this.rl.once('line', resolve);
        });
    }

    private listenForResponses() {
        if (!this.rl) return;
        this.rl.on('line', (line) => {
            try {
                const resp: KernelResponse = JSON.parse(line);

                // Handle input_request: kernel is asking for user input
                if (resp.status === 'input_request') {
                    this.handleInputRequest(resp);
                    return;
                }

                const key = resp.cell_id || resp.status || '';
                const cb = this.responseBuffer.get(key);
                if (cb) {
                    this.responseBuffer.delete(key);
                    cb(resp);
                }
            } catch {
                // ignore malformed lines
            }
        });
    }

    private async handleInputRequest(req: KernelResponse) {
        const prompt = (req as any).prompt || 'Input:';
        const cellId = req.cell_id || '';

        const userInput = await vscode.window.showInputBox({
            prompt: prompt,
            placeHolder: 'Enter value...',
            title: `Xell Input (${cellId})`
        });

        const reply = JSON.stringify({
            action: 'input_reply',
            text: userInput ?? ''
        });
        this.sendToKernel(JSON.parse(reply));
    }

    private stopKernel() {
        if (this.kernelProcess) {
            try {
                this.sendToKernel({ action: 'shutdown' });
            } catch { /* ignore */ }
            this.kernelProcess.kill();
            this.kernelProcess = null;
        }
        if (this.rl) {
            this.rl.close();
            this.rl = null;
        }
        this.responseBuffer.clear();
    }

    private sendToKernel(msg: KernelRequest) {
        if (this.kernelProcess?.stdin) {
            this.kernelProcess.stdin.write(JSON.stringify(msg) + '\n');
        }
    }

    private waitForResponse(cellId: string): Promise<KernelResponse> {
        return new Promise((resolve) => {
            const timer = setTimeout(() => {
                this.responseBuffer.delete(cellId);
                resolve({
                    cell_id: cellId,
                    status: 'error',
                    stdout: '',
                    stderr: 'Execution timed out after 30 seconds',
                    result: '',
                    execution_count: 0
                });
            }, 30000);

            this.responseBuffer.set(cellId, (data) => {
                clearTimeout(timer);
                resolve(data);
            });
        });
    }

    // ── Execution ────────────────────────────────────────

    private async execute(
        cells: vscode.NotebookCell[],
        _notebook: vscode.NotebookDocument,
        _controller: vscode.NotebookController
    ): Promise<void> {
        const kernelReady = await this.ensureKernel();
        if (!kernelReady) return;

        for (const cell of cells) {
            await this.executeCell(cell);
        }
    }

    private async executeCell(cell: vscode.NotebookCell): Promise<void> {
        const execution = this.controller.createNotebookCellExecution(cell);
        execution.executionOrder = ++this.executionOrder;
        execution.start(Date.now());

        const cellId = `cell-exec-${this.executionOrder}`;
        const code = cell.document.getText();

        const waitPromise = this.waitForResponse(cellId);
        this.sendToKernel({ action: 'execute', cell_id: cellId, code });

        const response = await waitPromise;

        const outputItems: vscode.NotebookCellOutput[] = [];

        if (response.stdout) {
            outputItems.push(new vscode.NotebookCellOutput([
                vscode.NotebookCellOutputItem.text(response.stdout, 'text/plain')
            ]));
        }

        if (response.stderr) {
            outputItems.push(new vscode.NotebookCellOutput([
                vscode.NotebookCellOutputItem.stderr(response.stderr)
            ]));
        }

        if (response.result && response.result !== response.stdout.trim()) {
            outputItems.push(new vscode.NotebookCellOutput([
                vscode.NotebookCellOutputItem.text(response.result, 'text/plain')
            ]));
        }

        execution.replaceOutput(outputItems);

        const success = response.status === 'ok';
        execution.end(success, Date.now());
    }

    // ── Interrupt ────────────────────────────────────────

    private interrupt(_notebook: vscode.NotebookDocument): void {
        this.stopKernel();
        this.executionOrder = 0;
        vscode.window.showInformationMessage('Xell kernel interrupted and reset.');
    }
}

// ── Kernel Protocol Types ────────────────────────────────

interface KernelRequest {
    action: 'execute' | 'reset' | 'shutdown' | 'complete';
    cell_id?: string;
    code?: string;
    cursor?: number;
}

interface KernelResponse {
    cell_id?: string;
    status: string;
    stdout: string;
    stderr: string;
    result: string;
    execution_count: number;
}
