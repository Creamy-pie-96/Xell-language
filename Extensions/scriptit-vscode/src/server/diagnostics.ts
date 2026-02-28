// ═══════════════════════════════════════════════════════════
// ScriptIt Subprocess Diagnostics
// ═══════════════════════════════════════════════════════════
//
// Runs `scriptit --check` to validate code (parse-only, no execution)
// and parses error messages into LSP Diagnostic objects.
//
// ═══════════════════════════════════════════════════════════

import { Diagnostic, DiagnosticSeverity, Connection } from 'vscode-languageserver/node';
import { spawn } from 'child_process';

export class ScriptItDiagnostics {
    private connection: Connection;
    private pending: Map<string, AbortController> = new Map();

    constructor(connection: Connection) {
        this.connection = connection;
    }

    /**
     * Validate ScriptIt source code by piping it through `scriptit --script`.
     * Returns an array of Diagnostic objects from any error output.
     */
    async validate(text: string, scriptitPath: string): Promise<Diagnostic[]> {
        // Cancel any previous pending validation
        const key = 'current';
        const prev = this.pending.get(key);
        if (prev) prev.abort();

        const controller = new AbortController();
        this.pending.set(key, controller);

        try {
            const stderr = await this.runScriptIt(text, scriptitPath, controller.signal);
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

    /**
     * Spawn the scriptit process, pipe source code to stdin,
     * and collect error output from both stdout and stderr.
     * (ScriptIt may write errors to either stream.)
     */
    private runScriptIt(text: string, scriptitPath: string, signal: AbortSignal): Promise<string> {
        return new Promise((resolve, reject) => {
            if (signal.aborted) { reject(new Error('Aborted')); return; }

            const proc = spawn(scriptitPath, ['--check'], {
                stdio: ['pipe', 'pipe', 'pipe'],
                timeout: 5000,       // 5s timeout
                windowsHide: true,
            });

            let stderr = '';
            let stdout = '';

            proc.stderr.on('data', (chunk: Buffer) => {
                stderr += chunk.toString();
                if (stderr.length > 65536) {
                    proc.kill();
                }
            });

            proc.stdout.on('data', (chunk: Buffer) => {
                stdout += chunk.toString();
                if (stdout.length > 65536) {
                    proc.kill();
                }
            });

            proc.on('close', (_code) => {
                // ScriptIt may write errors to stdout or stderr — check both.
                // Filter stdout to only include lines that look like errors.
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

                // Combine stderr and filtered stdout error lines
                const combined = [stderr.trim(), errorLines.trim()]
                    .filter(s => s.length > 0)
                    .join('\n');
                resolve(combined);
            });

            proc.on('error', (err) => {
                // scriptit not found — that's fine, just no diagnostics
                reject(err);
            });

            const abortHandler = () => {
                proc.kill();
                reject(new Error('Aborted'));
            };
            signal.addEventListener('abort', abortHandler, { once: true });

            // Write source code and close stdin
            proc.stdin.write(text);
            proc.stdin.end();
        });
    }

    /**
     * Parse ScriptIt error output into LSP Diagnostic objects.
     *
     * ScriptIt error messages typically look like:
     *   [Line 5] Error: Undefined variable 'foo'
     *   Error at line 10: unexpected token '+'
     *   Runtime error [Line 12]: division by zero
     *   Error: ...  (no line number)
     */
    private parseErrors(stderr: string, sourceText: string): Diagnostic[] {
        if (!stderr.trim()) return [];

        const diagnostics: Diagnostic[] = [];
        const lines = stderr.split('\n');
        const sourceLines = sourceText.split('\n');

        for (const errLine of lines) {
            const trimmed = errLine.trim();
            if (!trimmed) continue;

            // Try multiple error patterns ScriptIt might produce

            // Pattern 1: [Line N] Error: message
            // Pattern 2: [Line N] message
            let match = trimmed.match(/\[Line\s+(\d+)\]\s*(?:Error:\s*)?(.+)/i);
            if (match) {
                const lineNum = Math.max(0, parseInt(match[1]) - 1); // 0-indexed
                const message = match[2].trim();
                diagnostics.push(this.createDiagnostic(lineNum, message, sourceLines, this.getSeverity(message)));
                continue;
            }

            // Pattern 3: Error at line N: message
            match = trimmed.match(/(?:Error|Warning)\s+(?:at\s+)?line\s+(\d+)\s*:\s*(.+)/i);
            if (match) {
                const lineNum = Math.max(0, parseInt(match[1]) - 1);
                const message = match[2].trim();
                diagnostics.push(this.createDiagnostic(lineNum, message, sourceLines, this.getSeverity(message)));
                continue;
            }

            // Pattern 4: Runtime error [Line N]: message
            match = trimmed.match(/Runtime\s+error\s+\[Line\s+(\d+)\]\s*:\s*(.+)/i);
            if (match) {
                const lineNum = Math.max(0, parseInt(match[1]) - 1);
                const message = match[2].trim();
                diagnostics.push(this.createDiagnostic(lineNum, message, sourceLines, DiagnosticSeverity.Error));
                continue;
            }

            // Pattern 5: message at line N  (ScriptIt format: "Error: ... at line 1")
            match = trimmed.match(/^(.+?)\s+at\s+line\s+(\d+)\s*$/i);
            if (match) {
                const message = match[1].trim();
                const lineNum = Math.max(0, parseInt(match[2]) - 1);
                diagnostics.push(this.createDiagnostic(lineNum, message, sourceLines, this.getSeverity(message)));
                continue;
            }

            // Pattern 6: line N — message (some other format)
            match = trimmed.match(/line\s+(\d+)\s*[-—:]\s*(.+)/i);
            if (match) {
                const lineNum = Math.max(0, parseInt(match[1]) - 1);
                const message = match[2].trim();
                diagnostics.push(this.createDiagnostic(lineNum, message, sourceLines, this.getSeverity(message)));
                continue;
            }

            // Pattern 6: Error: message (no line number — put on line 0)
            if (/^(?:Error|Warning|RuntimeError)\s*:/i.test(trimmed)) {
                const message = trimmed.replace(/^(?:Error|Warning|RuntimeError)\s*:\s*/i, '');
                diagnostics.push(this.createDiagnostic(0, message, sourceLines, this.getSeverity(trimmed)));
                continue;
            }

            // Catch-all: any remaining output that looks like an error
            if (/error|undefined|unexpected|invalid|cannot|failed/i.test(trimmed)) {
                diagnostics.push(this.createDiagnostic(0, trimmed, sourceLines, DiagnosticSeverity.Error));
            }
        }

        return diagnostics;
    }

    /**
     * Create a single Diagnostic for a given line.
     */
    private createDiagnostic(
        lineNum: number,
        message: string,
        sourceLines: string[],
        severity: DiagnosticSeverity
    ): Diagnostic {
        // Clamp line number to valid range
        const safeLine = Math.min(lineNum, Math.max(0, sourceLines.length - 1));
        const lineText = sourceLines[safeLine] || '';

        return {
            severity,
            range: {
                start: { line: safeLine, character: 0 },
                end: { line: safeLine, character: lineText.length }
            },
            message,
            source: 'scriptit'
        };
    }

    /**
     * Determine severity based on error message content.
     */
    private getSeverity(message: string): DiagnosticSeverity {
        const lower = message.toLowerCase();
        if (lower.includes('warning')) return DiagnosticSeverity.Warning;
        if (lower.includes('hint') || lower.includes('info')) return DiagnosticSeverity.Information;
        return DiagnosticSeverity.Error;
    }
}
