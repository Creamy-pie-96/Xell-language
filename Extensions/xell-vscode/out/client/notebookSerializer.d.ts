import * as vscode from 'vscode';
export declare class XellNotebookSerializer implements vscode.NotebookSerializer {
    deserializeNotebook(content: Uint8Array, _token: vscode.CancellationToken): Promise<vscode.NotebookData>;
    serializeNotebook(data: vscode.NotebookData, _token: vscode.CancellationToken): Promise<Uint8Array>;
}
//# sourceMappingURL=notebookSerializer.d.ts.map