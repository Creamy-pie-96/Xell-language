export declare class XellNotebookController {
    readonly controllerId = "xell-kernel";
    readonly notebookType = "xell-notebook";
    readonly label = "Xell Kernel";
    readonly supportedLanguages: string[];
    private controller;
    private kernelProcess;
    private executionOrder;
    private responseBuffer;
    private rl;
    constructor();
    dispose(): void;
    private ensureKernel;
    private readLine;
    private listenForResponses;
    private handleInputRequest;
    private stopKernel;
    private sendToKernel;
    private waitForResponse;
    private execute;
    private executeCell;
    private interrupt;
}
//# sourceMappingURL=notebookController.d.ts.map