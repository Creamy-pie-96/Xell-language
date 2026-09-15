"""
Shrink a failing program to a minimal test case.
"""

import subprocess
import tempfile
import os

def shrink_program(source: str, xell_path: str, timeout: int) -> str:
    """Attempt to reduce the source code while preserving the failure."""
    # Greedy reduction: try removing statements, blocks, etc.
    # This is a simple version; more advanced would parse AST.
    lines = source.splitlines()
    # Try removing each line
    for i in range(len(lines)):
        reduced = lines[:i] + lines[i+1:]
        if reduced and test_program("\n".join(reduced), xell_path, timeout):
            # Failure still occurs, keep reducing
            return shrink_program("\n".join(reduced), xell_path, timeout)
    # Also try reducing numbers, removing comments, etc.
    return source

def test_program(source: str, xell_path: str, timeout: int) -> bool:
    """Run program and return True if it still fails (crashes or times out)."""
    with tempfile.NamedTemporaryFile(mode='w', suffix='.xell', delete=False) as f:
        f.write(source)
        tmp = f.name
    try:
        proc = subprocess.run(
            [xell_path, tmp],
            capture_output=True,
            text=True,
            timeout=timeout
        )
        # Check for crash indicators
        stderr = proc.stderr
        if ("Segmentation fault" in stderr or "core dumped" in stderr or
            "Aborted" in stderr or "Illegal instruction" in stderr):
            return True
        if proc.returncode < 0:  # signal termination
            return True
        return False
    except subprocess.TimeoutExpired:
        return True
    finally:
        os.unlink(tmp)