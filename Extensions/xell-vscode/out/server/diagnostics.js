"use strict";
// ═══════════════════════════════════════════════════════════
// Xell Subprocess Diagnostics
// ═══════════════════════════════════════════════════════════
//
// Runs `xell --check` to validate code (parse-only, no execution)
// and parses error messages into LSP Diagnostic objects.
//
// ═══════════════════════════════════════════════════════════
Object.defineProperty(exports, "__esModule", { value: true });
exports.XellDiagnostics = void 0;
const node_1 = require("vscode-languageserver/node");
const child_process_1 = require("child_process");
class XellDiagnostics {
    constructor(connection) {
        this.pending = new Map();
        this.connection = connection;
    }
    async validate(text, xellPath) {
        const key = 'current';
        const prev = this.pending.get(key);
        if (prev)
            prev.abort();
        const controller = new AbortController();
        this.pending.set(key, controller);
        try {
            const stderr = await this.runXell(text, xellPath, controller.signal);
            if (controller.signal.aborted)
                return [];
            return this.parseErrors(stderr, text);
        }
        catch {
            return [];
        }
        finally {
            if (this.pending.get(key) === controller) {
                this.pending.delete(key);
            }
        }
    }
    runXell(text, xellPath, signal) {
        return new Promise((resolve, reject) => {
            if (signal.aborted) {
                reject(new Error('Aborted'));
                return;
            }
            const proc = (0, child_process_1.spawn)(xellPath, ['--check'], {
                stdio: ['pipe', 'pipe', 'pipe'],
                timeout: 5000,
                windowsHide: true,
            });
            let stderr = '';
            let stdout = '';
            proc.stderr.on('data', (chunk) => {
                stderr += chunk.toString();
                if (stderr.length > 65536) {
                    proc.kill();
                }
            });
            proc.stdout.on('data', (chunk) => {
                stdout += chunk.toString();
                if (stdout.length > 65536) {
                    proc.kill();
                }
            });
            proc.on('close', (_code) => {
                const errorLines = stdout.split('\n').filter(line => {
                    const t = line.trim().toLowerCase();
                    return t.startsWith('error') ||
                        t.includes('at line') ||
                        t.includes('[line') ||
                        t.startsWith('runtime error') ||
                        t.startsWith('warning') ||
                        t.includes('undefined') ||
                        t.includes('unexpected') ||
                        t.includes('invalid') ||
                        t.includes('cannot') ||
                        t.includes('unknown');
                }).join('\n');
                const combined = [stderr.trim(), errorLines.trim()]
                    .filter(s => s.length > 0)
                    .join('\n');
                resolve(combined);
            });
            proc.on('error', (err) => { reject(err); });
            const abortHandler = () => {
                proc.kill();
                reject(new Error('Aborted'));
            };
            signal.addEventListener('abort', abortHandler, { once: true });
            proc.stdin.write(text);
            proc.stdin.end();
        });
    }
    parseErrors(stderr, sourceText) {
        if (!stderr.trim())
            return [];
        const diagnostics = [];
        const lines = stderr.split('\n');
        const sourceLines = sourceText.split('\n');
        for (const errLine of lines) {
            const trimmed = errLine.trim();
            if (!trimmed)
                continue;
            // [XELL ERROR] Line 14 — TypeError: cannot add number and list
            let match = trimmed.match(/\[XELL\s+ERROR\]\s+Line\s+(\d+)\s*[—-]\s*(\w+):\s*(.+)/i);
            if (match) {
                const lineNum = Math.max(0, parseInt(match[1]) - 1);
                const message = `${match[2]}: ${match[3]}`;
                diagnostics.push(this.createDiagnostic(lineNum, message, sourceLines, node_1.DiagnosticSeverity.Error));
                continue;
            }
            // [Line N] Error: message
            match = trimmed.match(/\[Line\s+(\d+)\]\s*(?:Error:\s*)?(.+)/i);
            if (match) {
                const lineNum = Math.max(0, parseInt(match[1]) - 1);
                const message = match[2].trim();
                diagnostics.push(this.createDiagnostic(lineNum, message, sourceLines, this.getSeverity(message)));
                continue;
            }
            // Error at line N: message
            match = trimmed.match(/(?:Error|Warning)\s+(?:at\s+)?line\s+(\d+)\s*:\s*(.+)/i);
            if (match) {
                const lineNum = Math.max(0, parseInt(match[1]) - 1);
                const message = match[2].trim();
                diagnostics.push(this.createDiagnostic(lineNum, message, sourceLines, this.getSeverity(message)));
                continue;
            }
            // General error line
            if (/error|undefined|unexpected|invalid|cannot|failed/i.test(trimmed)) {
                diagnostics.push(this.createDiagnostic(0, trimmed, sourceLines, node_1.DiagnosticSeverity.Error));
            }
        }
        return diagnostics;
    }
    createDiagnostic(lineNum, message, sourceLines, severity) {
        const safeLine = Math.min(lineNum, Math.max(0, sourceLines.length - 1));
        const lineText = sourceLines[safeLine] || '';
        return {
            severity,
            range: {
                start: { line: safeLine, character: 0 },
                end: { line: safeLine, character: lineText.length }
            },
            message,
            source: 'xell'
        };
    }
    getSeverity(message) {
        const lower = message.toLowerCase();
        if (lower.includes('warning'))
            return node_1.DiagnosticSeverity.Warning;
        if (lower.includes('hint') || lower.includes('info'))
            return node_1.DiagnosticSeverity.Information;
        return node_1.DiagnosticSeverity.Error;
    }
}
exports.XellDiagnostics = XellDiagnostics;
//# sourceMappingURL=diagnostics.js.map