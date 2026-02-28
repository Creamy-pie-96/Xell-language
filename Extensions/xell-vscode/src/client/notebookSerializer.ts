// ═══════════════════════════════════════════════════════════
// Xell Notebook Serializer
// ═══════════════════════════════════════════════════════════
//
// Reads/writes the .nxel JSON format to/from VS Code's
// internal NotebookData representation.
//
// ═══════════════════════════════════════════════════════════

import * as vscode from 'vscode';

interface NxelCell {
    id: string;
    type: 'code' | 'markdown';
    source: string;
    outputs: NxelOutput[];
    execution_count: number | null;
    metadata: Record<string, unknown>;
}

interface NxelOutput {
    type: 'stdout' | 'stderr' | 'result';
    text: string;
}

interface NxelNotebook {
    nxel_format: string;
    metadata: {
        title: string;
        created: string;
        modified: string;
        kernel: string;
        kernel_version: string;
    };
    cells: NxelCell[];
}

export class XellNotebookSerializer implements vscode.NotebookSerializer {

    async deserializeNotebook(
        content: Uint8Array,
        _token: vscode.CancellationToken
    ): Promise<vscode.NotebookData> {
        const text = new TextDecoder().decode(content);

        let raw: NxelNotebook;
        try {
            raw = JSON.parse(text);
        } catch {
            // Empty or invalid — create a blank notebook
            raw = {
                nxel_format: '1.0',
                metadata: {
                    title: 'Untitled',
                    created: new Date().toISOString(),
                    modified: new Date().toISOString(),
                    kernel: 'xell',
                    kernel_version: '1.0'
                },
                cells: []
            };
        }

        const cells = raw.cells.map(cell => {
            const kind = cell.type === 'markdown'
                ? vscode.NotebookCellKind.Markup
                : vscode.NotebookCellKind.Code;

            const languageId = cell.type === 'markdown' ? 'markdown' : 'xell';
            const cellData = new vscode.NotebookCellData(kind, cell.source, languageId);

            // Convert stored outputs
            if (cell.outputs && cell.outputs.length > 0) {
                cellData.outputs = cell.outputs.map(out => {
                    const mime = out.type === 'stderr'
                        ? 'application/vnd.code.notebook.stderr'
                        : 'text/plain';
                    return new vscode.NotebookCellOutput([
                        vscode.NotebookCellOutputItem.text(out.text, mime)
                    ]);
                });
            }

            cellData.metadata = {
                nxel_id: cell.id,
                execution_count: cell.execution_count,
                ...cell.metadata
            };

            return cellData;
        });

        const notebookData = new vscode.NotebookData(cells);
        notebookData.metadata = {
            nxel_format: raw.nxel_format,
            ...raw.metadata
        };

        return notebookData;
    }

    async serializeNotebook(
        data: vscode.NotebookData,
        _token: vscode.CancellationToken
    ): Promise<Uint8Array> {
        const meta = data.metadata || {};

        const cells: NxelCell[] = data.cells.map((cell, idx) => {
            const isCode = cell.kind === vscode.NotebookCellKind.Code;
            const cellMeta = cell.metadata || {};

            // Convert outputs back to nxel format
            const outputs: NxelOutput[] = [];
            if (cell.outputs) {
                for (const out of cell.outputs) {
                    for (const item of out.items) {
                        const text = new TextDecoder().decode(item.data);
                        if (item.mime === 'application/vnd.code.notebook.stderr') {
                            outputs.push({ type: 'stderr', text });
                        } else {
                            outputs.push({ type: 'stdout', text });
                        }
                    }
                }
            }

            return {
                id: cellMeta.nxel_id || `cell-${idx + 1}`,
                type: isCode ? 'code' : 'markdown',
                source: cell.value,
                outputs,
                execution_count: cellMeta.execution_count ?? null,
                metadata: {}
            };
        });

        const nxel: NxelNotebook = {
            nxel_format: meta.nxel_format || '1.0',
            metadata: {
                title: meta.title || 'Untitled',
                created: meta.created || new Date().toISOString(),
                modified: new Date().toISOString(),
                kernel: 'xell',
                kernel_version: meta.kernel_version || '1.0'
            },
            cells
        };

        const json = JSON.stringify(nxel, null, 2);
        return new TextEncoder().encode(json);
    }
}
