"use strict";
// ═══════════════════════════════════════════════════════════
// Xell Notebook Serializer
// ═══════════════════════════════════════════════════════════
//
// Reads/writes the .nxel JSON format to/from VS Code's
// internal NotebookData representation.
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
exports.XellNotebookSerializer = void 0;
const vscode = __importStar(require("vscode"));
class XellNotebookSerializer {
    async deserializeNotebook(content, _token) {
        const text = new TextDecoder().decode(content);
        let raw;
        try {
            raw = JSON.parse(text);
        }
        catch {
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
    async serializeNotebook(data, _token) {
        const meta = data.metadata || {};
        const cells = data.cells.map((cell, idx) => {
            const isCode = cell.kind === vscode.NotebookCellKind.Code;
            const cellMeta = cell.metadata || {};
            // Convert outputs back to nxel format
            const outputs = [];
            if (cell.outputs) {
                for (const out of cell.outputs) {
                    for (const item of out.items) {
                        const text = new TextDecoder().decode(item.data);
                        if (item.mime === 'application/vnd.code.notebook.stderr') {
                            outputs.push({ type: 'stderr', text });
                        }
                        else {
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
        const nxel = {
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
exports.XellNotebookSerializer = XellNotebookSerializer;
//# sourceMappingURL=notebookSerializer.js.map