#!/usr/bin/env python3
"""
Golden test runner for Xell.
Discovers all .xell files, executes them, compares output.
"""

import os
import sys
import subprocess
import difflib
import argparse
from pathlib import Path
from typing import List, Tuple, Optional, Dict

from reporter import Reporter, TestResult, TestStatus

class TestRunner:
    def __init__(self, xell_path: str, golden_dir: str, expected_dir: str):
        self.xell = xell_path
        self.golden_dir = Path(golden_dir)
        self.expected_dir = Path(expected_dir) if expected_dir else None
        self.results: List[TestResult] = []

    def discover_tests(self) -> List[Path]:
        """Find all .xell files in golden_dir recursively."""
        return list(self.golden_dir.rglob("*.xell"))

    def read_source(self, test_file: Path) -> str:
        """Read the source code of the test file."""
        try:
            return test_file.read_text()
        except Exception:
            return ""

    def run_test(self, test_file: Path) -> TestResult:
        """Execute one test file and compare with expected."""
        rel_path = test_file.relative_to(self.golden_dir)

        # Locate expected files
        expected_base = None
        if self.expected_dir:
            expected_base = self.expected_dir / rel_path.with_suffix("")
        if not expected_base or not expected_base.exists():
            expected_base = test_file.with_suffix("")

        expected_stdout_file = expected_base.with_suffix(".expected")
        expected_stderr_file = expected_base.with_suffix(".stderr")
        expected_exit_file = expected_base.with_suffix(".exitcode")

        # Read source
        source = self.read_source(test_file)

        # Run the interpreter
        try:
            proc = subprocess.run(
                [self.xell, str(test_file)],
                capture_output=True,
                text=True,
                timeout=30
            )
            stdout = proc.stdout
            stderr = proc.stderr
            exitcode = proc.returncode
        except subprocess.TimeoutExpired:
            return TestResult(
                test_file=test_file,
                status=TestStatus.TIMEOUT,
                stdout="",
                stderr="",
                exitcode=-1,
                expected_stdout="",
                expected_stderr="",
                expected_exitcode=0,
                diff="",
                source=source
            )
        except Exception as e:
            return TestResult(
                test_file=test_file,
                status=TestStatus.ERROR,
                stdout="",
                stderr=str(e),
                exitcode=-1,
                expected_stdout="",
                expected_stderr="",
                expected_exitcode=0,
                diff="",
                source=source
            )

        # Read expected outputs
        exp_stdout = expected_stdout_file.read_text() if expected_stdout_file.exists() else ""
        exp_stderr = expected_stderr_file.read_text() if expected_stderr_file.exists() else ""
        exp_exit = int(expected_exit_file.read_text().strip()) if expected_exit_file.exists() else 0

        # Compare
        stdout_ok = stdout == exp_stdout
        stderr_ok = stderr == exp_stderr
        exit_ok = exitcode == exp_exit
        all_ok = stdout_ok and stderr_ok and exit_ok

        diff = ""
        if not stdout_ok:
            diff += "--- stdout mismatch ---\n" + "\n".join(
                difflib.unified_diff(exp_stdout.splitlines(keepends=True),
                                     stdout.splitlines(keepends=True),
                                     fromfile="expected", tofile="actual")
            )
        if not stderr_ok:
            diff += "--- stderr mismatch ---\n" + "\n".join(
                difflib.unified_diff(exp_stderr.splitlines(keepends=True),
                                     stderr.splitlines(keepends=True),
                                     fromfile="expected", tofile="actual")
            )
        if not exit_ok:
            diff += f"--- exit code mismatch: expected {exp_exit}, got {exitcode}\n"

        status = TestStatus.PASS if all_ok else TestStatus.FAIL
        return TestResult(
            test_file=test_file,
            status=status,
            stdout=stdout,
            stderr=stderr,
            exitcode=exitcode,
            expected_stdout=exp_stdout,
            expected_stderr=exp_stderr,
            expected_exitcode=exp_exit,
            diff=diff,
            source=source
        )

    def run_all(self) -> List[TestResult]:
        tests = self.discover_tests()
        self.results = [self.run_test(t) for t in tests]
        return self.results

def main():
    parser = argparse.ArgumentParser(description="Run Xell golden tests")
    parser.add_argument("--xell", default="xell", help="Path to the Xell interpreter")
    parser.add_argument("--golden-dir", default="golden", help="Root of golden tests")
    parser.add_argument("--expected-dir", default="expected", help="Root of expected outputs")
    parser.add_argument("--html", help="Generate HTML report")
    args = parser.parse_args()

    runner = TestRunner(args.xell, args.golden_dir, args.expected_dir)
    results = runner.run_all()

    reporter = Reporter()
    reporter.print_console(results)
    if args.html:
        reporter.save_html(results, args.html)

    # Exit code: non‑zero if any test failed
    sys.exit(0 if all(r.status == TestStatus.PASS for r in results) else 1)

if __name__ == "__main__":
    main()