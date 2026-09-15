import sys
from enum import Enum
from dataclasses import dataclass, field
from pathlib import Path
from typing import List, Optional

class TestStatus(Enum):
    PASS = "PASS"
    FAIL = "FAIL"
    ERROR = "ERROR"
    TIMEOUT = "TIMEOUT"

@dataclass
class TestResult:
    test_file: Path
    status: TestStatus
    stdout: str
    stderr: str
    exitcode: int
    expected_stdout: str
    expected_stderr: str
    expected_exitcode: int
    diff: str
    source: str = ""  # added

class Reporter:
    def print_console(self, results: List[TestResult]):
        # Count
        pass_count = sum(1 for r in results if r.status == TestStatus.PASS)
        fail_count = sum(1 for r in results if r.status != TestStatus.PASS)
        total = len(results)

        # Header
        print("\n" + "="*60)
        print(f"TEST RESULTS: {pass_count} passed, {fail_count} failed, {total} total")
        print("="*60)

        # Per-test details
        for r in results:
            if r.status == TestStatus.PASS:
                symbol = "\033[92m✔\033[0m"  # green
            else:
                symbol = "\033[91m✘\033[0m"  # red
            print(f"{symbol} {r.test_file}  [{r.status.value}]")
            if r.status != TestStatus.PASS:
                # Print source code snippet
                if r.source:
                    # Show first few lines (max 10 lines) with line numbers
                    lines = r.source.splitlines()
                    if len(lines) > 10:
                        lines = lines[:10] + ["... (truncated)"]
                    print("  Code:")
                    for idx, line in enumerate(lines, 1):
                        print(f"    {idx:2d} | {line}")
                # Print diff
                if r.diff:
                    print("    Diff:")
                    # Indent diff lines
                    for dline in r.diff.splitlines():
                        print(f"      {dline}")
                else:
                    print("    (no diff available)")
        # Footer with summary
        if fail_count == 0:
            print("\n\033[92mAll tests passed!\033[0m")
        else:
            print(f"\n\033[91m{fail_count} test(s) failed.\033[0m")

    def save_html(self, results: List[TestResult], filename: str):
        import html
        rows = []
        for r in results:
            status_color = "#4CAF50" if r.status == TestStatus.PASS else "#f44336"
            code_snippet = ""
            if r.source:
                lines = r.source.splitlines()
                if len(lines) > 10:
                    lines = lines[:10] + ["..."]
                code_snippet = "<br>".join(html.escape(l) for l in lines)
            rows.append(f"""
            <tr>
                <td>{r.test_file}</td>
                <td style="color:{status_color}">{r.status.value}</td>
                <td><pre>{code_snippet}</pre></td>
                <td><pre>{html.escape(r.diff[:300])}</pre></td>
            </tr>
            """)
        html_content = f"""
        <!DOCTYPE html>
        <html><head><title>Xell Test Report</title>
        <style>body {{ font-family: sans-serif; }} table {{ border-collapse: collapse; }} th, td {{ border: 1px solid #ddd; padding: 8px; }} th {{ background: #f2f2f2; }}</style>
        </head><body>
        <h1>Test Report</h1>
        <table>
            <tr><th>Test</th><th>Status</th><th>Code Snippet</th><th>Diff</th></tr>
            {''.join(rows)}
        </table>
        </body></html>
        """
        with open(filename, "w") as f:
            f.write(html_content)