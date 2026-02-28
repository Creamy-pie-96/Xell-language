// ═══════════════════════════════════════════════════════════
// Xell Subprocess Diagnostics
// ═══════════════════════════════════════════════════════════
//
// Runs `xell --check` to validate code (parse-only, no execution)
// and parses error messages into LSP Diagnostic objects.
//
// ═══════════════════════════════════════════════════════════

import { Diagnostic, DiagnosticSeverity, Connection } from 'vscode-languageserver/node';
import { spawn } from 'child_process';

export class XellDiagnostics {
    private connection: Connection;
    private pending: Map<string, AbortController> = new Map();

    constructor(connection: Connection) {
        this.connection = connection;
    }

    async validate(text: string, xellPath: string): Promise<Diagnostic[]> {
        const key = 'current';
        const prev = this.pending.get(key);
        if (prev) prev.abort();

        const controller = new AbortController();
        this.pending.set(key, controller);

        try {
            const stderr = await this.runXell(text, xellPath, controller.signal);
            if (controller.signal.aborted) return [];
            return this.parseErrors(stderr, text);
        } catch {
            return [];
        } finally {
            if (this.pending.get(key) === controller) {
                this.pending.delete(key);
            }
        }
    }

    private runXell(text: string, xellPath: string, signal: AbortSignal): Promise<string> {
        return new Promise((resolve, reject) => {
            if (signal.aborted) { reject(new Error('Aborted')); return; }

            const proc = spawn(xellPath, ['--check'], {
                stdio: ['pipe', 'pipe', 'pipe'],
                timeout: 5000,
                windowsHide: true,
            });

            let stderr = '';
            let stdout = '';

            proc.stderr.on('data', (chunk: Buffer) => {
                stderr += chunk.toString();
                if (stderr.length > 65536) { proc.kill(); }
            });

            proc.stdout.on('data', (chunk: Buffer) => {
                stdout += chunk.toString();
                if (stdout.length > 65536) { proc.kill(); }
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

    private parseErrors(stderr: string, sourceText: string): Diagnostic[] {
        if (!stderr.trim()) return [];

        const diagnostics: Diagnostic[] = [];
        const lines = stderr.split('\n');
        const sourceLines = sourceText.split('\n');

        for (const errLine of lines) {
            const trimmed = errLine.trim();
            if (!trimmed) continue;

            // [XELL ERROR] Line 14 — TypeError: cannot add number and list
            // [XELL WARNING] Line 5 — Undefined name 'foo'
            // [XELL HINT] Line 10 — some hint
            let match = trimmed.match(/\[XELL\s+(ERROR|WARNING|HINT)\]\s+Line\s+(\d+)\s*[—-]\s*(?:(\w+):\s*)?(.+)/i);
            if (match) {
                const sevStr = match[1].toUpperCase();
                const lineNum = Math.max(0, parseInt(match[2]) - 1);
                const category = match[3] || '';
                const detail = match[4];
                const message = category ? `${category}: ${detail}` : detail;
                const severity = sevStr === 'WARNING' ? DiagnosticSeverity.Warning :
                                 sevStr === 'HINT' ? DiagnosticSeverity.Hint :
                                 DiagnosticSeverity.Error;
                diagnostics.push(this.createDiagnostic(lineNum, message, sourceLines, severity));
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
                diagnostics.push(this.createDiagnostic(0, trimmed, sourceLines, DiagnosticSeverity.Error));
            }
        }

        return diagnostics;
    }

    private createDiagnostic(
        lineNum: number, message: string, sourceLines: string[], severity: DiagnosticSeverity
    ): Diagnostic {
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

    private getSeverity(message: string): DiagnosticSeverity {
        const lower = message.toLowerCase();
        if (lower.includes('warning')) return DiagnosticSeverity.Warning;
        if (lower.includes('hint') || lower.includes('info')) return DiagnosticSeverity.Information;
        return DiagnosticSeverity.Error;
    }
}
