import { Diagnostic, Connection } from 'vscode-languageserver/node';
export declare class XellDiagnostics {
    private connection;
    private pending;
    constructor(connection: Connection);
    validate(text: string, xellPath: string): Promise<Diagnostic[]>;
    private runXell;
    private parseErrors;
    private createDiagnostic;
    private getSeverity;
}
//# sourceMappingURL=diagnostics.d.ts.map