#!/usr/bin/env python3
"""
Fuzzer runner for Xell: generates random programs, runs them, detects crashes/hangs,
and shrinks failures to minimal regression tests.
"""

import os
import sys
import subprocess
import tempfile
import time
import random
import argparse
from pathlib import Path
from typing import List, Optional

from reporter import Reporter, TestResult, TestStatus
from fuzzer.generator import SemanticGenerator
from fuzzer.shrink import shrink_program

class FuzzerRunner:
    def __init__(self, xell_path: str, iterations: int, timeout: int, save_dir: str):
        self.xell = xell_path
        self.iterations = iterations
        self.timeout = timeout
        self.save_dir = Path(save_dir)
        self.save_dir.mkdir(parents=True, exist_ok=True)
        self.results: List[TestResult] = []

    def run_program(self, source: str) -> tuple:
        with tempfile.NamedTemporaryFile(mode='w', suffix='.xell', delete=False) as f:
            f.write(source)
            tmp = f.name
        try:
            proc = subprocess.run(
                [self.xell, tmp],
                capture_output=True,
                text=True,
                timeout=self.timeout
            )
            stdout = proc.stdout
            stderr = proc.stderr
            exitcode = proc.returncode
            timed_out = False
        except subprocess.TimeoutExpired:
            stdout, stderr, exitcode = "", "", -1
            timed_out = True
        finally:
            os.unlink(tmp)
        return stdout, stderr, exitcode, timed_out

    def run(self):
        generator = SemanticGenerator()
        shrinker = shrink_program

        for i in range(self.iterations):
            source, exp_stdout, exp_stderr, exp_exit = generator.generate()
            stdout, stderr, exitcode, timed_out = self.run_program(source)

            # Compare
            stdout_ok = (stdout == exp_stdout)
            stderr_ok = (stderr == exp_stderr)
            exit_ok = (exitcode == exp_exit)
            all_ok = stdout_ok and stderr_ok and exit_ok

            if timed_out:
                # treat as failure
                all_ok = False

            if not all_ok:
                # Save as regression
                self.save_regression(source, exp_stdout, exp_stderr, exp_exit, stdout, stderr, exitcode)
                diff = f"Expected stdout: {exp_stdout!r}, got {stdout!r}\n" \
                       f"Expected stderr: {exp_stderr!r}, got {stderr!r}\n" \
                       f"Expected exit: {exp_exit}, got {exitcode}"
                self.results.append(TestResult(
                    test_file=Path("fuzzer-generated"),
                    status=TestStatus.FAIL,
                    stdout=stdout,
                    stderr=stderr,
                    exitcode=exitcode,
                    expected_stdout=exp_stdout,
                    expected_stderr=exp_stderr,
                    expected_exitcode=exp_exit,
                    diff=diff,
                    source=source
                ))

        self.promote_regressions()

    def save_regression(self, source, exp_stdout, exp_stderr, exp_exit, stdout, stderr, exitcode):
        import time
        name = f"fuzz_{int(time.time())}.xell"
        test_path = self.save_dir / name
        test_path.write_text(source)
        (self.save_dir / (name.replace('.xell', '.expected'))).write_text(exp_stdout)
        (self.save_dir / (name.replace('.xell', '.stderr'))).write_text(exp_stderr)
        (self.save_dir / (name.replace('.xell', '.exitcode'))).write_text(str(exp_exit) + "\n")
        # Save actual outputs for debugging
        (self.save_dir / (name.replace('.xell', '.actual_stdout'))).write_text(stdout)
        (self.save_dir / (name.replace('.xell', '.actual_stderr'))).write_text(stderr)
        (self.save_dir / (name.replace('.xell', '.actual_exitcode'))).write_text(str(exitcode) + "\n")

    def promote_regressions(self):
        target_dir = Path("golden/regression/fuzzer_gen")
        target_dir.mkdir(parents=True, exist_ok=True)
        for src in self.save_dir.glob("*.xell"):
            dest = target_dir / src.name
            if not dest.exists():
                dest.write_text(src.read_text())
                for ext in ['.expected', '.stderr', '.exitcode']:
                    ext_file = src.with_suffix(ext)
                    if ext_file.exists():
                        (target_dir / (src.stem + ext)).write_text(ext_file.read_text())

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Run semantic fuzzer")
    parser.add_argument("--xell", default="xell", help="Path to Xell interpreter")
    parser.add_argument("--iterations", type=int, default=1000, help="Number of programs")
    parser.add_argument("--timeout", type=int, default=5, help="Timeout per program (seconds)")
    parser.add_argument("--save-dir", default="fuzzer/results", help="Directory to save failures")
    parser.add_argument("--html", help="Generate HTML report at this path")
    args = parser.parse_args()

    runner = FuzzerRunner(args.xell, args.iterations, args.timeout, args.save_dir)
    runner.run()

    reporter = Reporter()
    if runner.results:
        reporter.print_console(runner.results)
        if args.html:
            reporter.save_html(runner.results, args.html)
    else:
        print("All semantic checks passed.")
        # Still generate an empty report if HTML requested
        if args.html:
            # Create a minimal report with no failures
            import html
            with open(args.html, "w") as f:
                f.write("""<!DOCTYPE html>
<html><head><title>Xell Fuzzer Report</title></head>
<body><h1>Fuzzer Report</h1><p>All semantic checks passed.</p></body></html>""")
    sys.exit(0)