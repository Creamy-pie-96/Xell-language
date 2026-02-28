"use strict";
// ═══════════════════════════════════════════════════════════
// Xell Notebook Controller (Kernel)
// ═══════════════════════════════════════════════════════════
//
// Manages the Xell kernel subprocess and executes
// notebook cells via the --kernel JSON protocol.
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
exports.XellNotebookController = void 0;
const vscode = __importStar(require("vscode"));
const child_process_1 = require("child_process");
const readline = __importStar(require("readline"));
class XellNotebookController {
    constructor() {
        this.controllerId = 'xell-kernel';
        this.notebookType = 'xell-notebook';
        this.label = 'Xell Kernel';
        this.supportedLanguages = ['xell'];
        this.kernelProcess = null;
        this.executionOrder = 0;
        this.responseBuffer = new Map();
        this.rl = null;
        this.controller = vscode.notebooks.createNotebookController(this.controllerId, this.notebookType, this.label);
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
    async ensureKernel() {
        if (this.kernelProcess && this.kernelProcess.exitCode === null) {
            return true; // already running
        }
        const config = vscode.workspace.getConfiguration('xell');
        const xellPath = config.get('xellPath', 'xell');
        try {
            this.kernelProcess = (0, child_process_1.spawn)(xellPath, ['--kernel'], {
                stdio: ['pipe', 'pipe', 'pipe'],
                windowsHide: true,
            });
            // Read line-by-line from stdout
            this.rl = readline.createInterface({
                input: this.kernelProcess.stdout,
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
        }
        catch (err) {
            const msg = err instanceof Error ? err.message : String(err);
            vscode.window.showErrorMessage(`Failed to start Xell kernel: ${msg}\n\nMake sure '${xellPath}' is installed.`);
            return false;
        }
    }
    readLine() {
        return new Promise((resolve) => {
            if (!this.rl) {
                resolve('');
                return;
            }
            this.rl.once('line', resolve);
        });
    }
    listenForResponses() {
        if (!this.rl)
            return;
        this.rl.on('line', (line) => {
            try {
                const resp = JSON.parse(line);
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
            }
            catch {
                // ignore malformed lines
            }
        });
    }
    async handleInputRequest(req) {
        const prompt = req.prompt || 'Input:';
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
    stopKernel() {
        if (this.kernelProcess) {
            try {
                this.sendToKernel({ action: 'shutdown' });
            }
            catch { /* ignore */ }
            this.kernelProcess.kill();
            this.kernelProcess = null;
        }
        if (this.rl) {
            this.rl.close();
            this.rl = null;
        }
        this.responseBuffer.clear();
    }
    sendToKernel(msg) {
        if (this.kernelProcess?.stdin) {
            this.kernelProcess.stdin.write(JSON.stringify(msg) + '\n');
        }
    }
    waitForResponse(cellId) {
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
    async execute(cells, _notebook, _controller) {
        const kernelReady = await this.ensureKernel();
        if (!kernelReady)
            return;
        for (const cell of cells) {
            await this.executeCell(cell);
        }
    }
    async executeCell(cell) {
        const execution = this.controller.createNotebookCellExecution(cell);
        execution.executionOrder = ++this.executionOrder;
        execution.start(Date.now());
        const cellId = `cell-exec-${this.executionOrder}`;
        const code = cell.document.getText();
        const waitPromise = this.waitForResponse(cellId);
        this.sendToKernel({ action: 'execute', cell_id: cellId, code });
        const response = await waitPromise;
        const outputItems = [];
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
    interrupt(_notebook) {
        this.stopKernel();
        this.executionOrder = 0;
        vscode.window.showInformationMessage('Xell kernel interrupted and reset.');
    }
}
exports.XellNotebookController = XellNotebookController;
//# sourceMappingURL=notebookController.js.map