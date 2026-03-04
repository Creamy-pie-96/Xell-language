import { CompletionItem } from 'vscode-languageserver/node';
interface LangKeyword {
    name: string;
    kind: string;
    detail: string;
    hover?: {
        sig: string;
        detail: string;
        params?: string[];
    };
}
interface LangBuiltin {
    name: string;
    category: string;
    kind: string;
    hover?: {
        sig: string;
        detail: string;
        params?: string[];
    };
}
interface LanguageData {
    keywords: LangKeyword[];
    builtins: LangBuiltin[];
    blockKeywords: string[];
    allKeywordNames: string[];
}
declare const LANG_DATA: LanguageData;
export declare const XELL_KEYWORDS: CompletionItem[];
export declare const XELL_BUILTINS: CompletionItem[];
export declare const ALL_COMPLETIONS: CompletionItem[];
export { LANG_DATA };
export type { LanguageData, LangKeyword, LangBuiltin };
//# sourceMappingURL=completions.d.ts.map