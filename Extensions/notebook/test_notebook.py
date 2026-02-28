#!/usr/bin/env python3
"""
ScriptIt Notebook System - End-to-End Test Suite
Tests the kernel, server API, and notebook file operations.
"""

import json
import os
import sys
import time
import signal
import subprocess
import urllib.request
import urllib.error
import tempfile
import shutil

# ─── Config ──────────────────────────────────────────────

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SERVER_SCRIPT = os.path.join(SCRIPT_DIR, "notebook_server.py")
SCRIPTIT_BINARY = os.path.join(SCRIPT_DIR, "..", "scriptit")
PORT = 18899  # Use unusual port to avoid conflicts
BASE_URL = f"http://localhost:{PORT}"
SAMPLE_NOTEBOOK = os.path.join(SCRIPT_DIR, "getting_started.nsit")

server_proc = None
passed = 0
failed = 0
errors = []

# ─── Helpers ─────────────────────────────────────────────

def api_get(path):
    url = BASE_URL + "/api" + path
    req = urllib.request.Request(url, method="GET")
    with urllib.request.urlopen(req, timeout=10) as resp:
        return json.loads(resp.read().decode())

def api_post(path, data=None):
    url = BASE_URL + "/api" + path
    body = json.dumps(data or {}).encode()
    req = urllib.request.Request(url, data=body, method="POST",
                                 headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=10) as resp:
        return json.loads(resp.read().decode())

def check(test_name, condition, detail=""):
    global passed, failed, errors
    if condition:
        passed += 1
        print(f"  ✅ {test_name}")
    else:
        failed += 1
        msg = f"  ❌ {test_name}" + (f" — {detail}" if detail else "")
        print(msg)
        errors.append(msg)

def section(name):
    print(f"\n{'─'*60}")
    print(f"  {name}")
    print(f"{'─'*60}")

# ─── Test: Kernel Protocol (Direct) ─────────────────────

def test_kernel_direct():
    section("Kernel Protocol (direct stdin/stdout)")

    proc = subprocess.Popen(
        [SCRIPTIT_BINARY, "--kernel"],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        text=True, bufsize=1
    )

    # Read ready message
    ready = proc.stdout.readline().strip()
    info = json.loads(ready)
    check("kernel_ready signal", info.get("status") == "kernel_ready", ready)
    check("kernel version", info.get("version") == "2.0")

    # Execute simple assignment
    proc.stdin.write(json.dumps({"action": "execute", "cell_id": "c1", "code": "var x = 42."}) + "\n")
    proc.stdin.flush()
    resp = json.loads(proc.stdout.readline().strip())
    check("execute: status ok", resp["status"] == "ok", json.dumps(resp))
    check("execute: cell_id matches", resp["cell_id"] == "c1")
    check("execute: execution_count=1", resp["execution_count"] == 1)

    # Execute print
    proc.stdin.write(json.dumps({"action": "execute", "cell_id": "c2", "code": "print(x)."}) + "\n")
    proc.stdin.flush()
    resp = json.loads(proc.stdout.readline().strip())
    check("print: stdout has 42", "42" in resp.get("stdout", ""), resp.get("stdout", ""))
    check("print: execution_count=2", resp["execution_count"] == 2)

    # Scope persistence: x should still be 42
    proc.stdin.write(json.dumps({"action": "execute", "cell_id": "c3", "code": "var y = x + 8. print(y)."}) + "\n")
    proc.stdin.flush()
    resp = json.loads(proc.stdout.readline().strip())
    check("scope persistence: y=50", "50" in resp.get("stdout", ""), resp.get("stdout", ""))

    # String operations
    proc.stdin.write(json.dumps({"action": "execute", "cell_id": "c4", "code": 'var s = "hello". print(s.upper()).'}) + "\n")
    proc.stdin.flush()
    resp = json.loads(proc.stdout.readline().strip())
    check("string method: upper()", "HELLO" in resp.get("stdout", ""), resp.get("stdout", ""))

    # Reset
    proc.stdin.write(json.dumps({"action": "reset"}) + "\n")
    proc.stdin.flush()
    resp = json.loads(proc.stdout.readline().strip())
    check("reset: status ok", resp["status"] == "reset_ok")

    # After reset, x should be gone (None in v2)
    proc.stdin.write(json.dumps({"action": "execute", "cell_id": "c5", "code": "print(x)."}) + "\n")
    proc.stdin.flush()
    resp = json.loads(proc.stdout.readline().strip())
    check("after reset: x is None", "None" in resp.get("stdout", ""), resp.get("stdout", ""))

    # Error handling
    proc.stdin.write(json.dumps({"action": "execute", "cell_id": "c6", "code": "var z = 1 / 0."}) + "\n")
    proc.stdin.flush()
    resp = json.loads(proc.stdout.readline().strip())
    # Could be error or could execute (depends on implementation)
    check("division by zero: handled", resp["status"] in ("ok", "error"), json.dumps(resp))

    # Shutdown
    proc.stdin.write(json.dumps({"action": "shutdown"}) + "\n")
    proc.stdin.flush()
    resp = json.loads(proc.stdout.readline().strip())
    check("shutdown: status ok", resp["status"] == "shutdown_ok")

    proc.wait(timeout=3)
    check("kernel process exited", proc.returncode is not None)


# ─── Test: Server API ───────────────────────────────────

def test_server_api():
    section("Server API (/api endpoints)")

    # GET /api/notebook
    nb = api_get("/notebook")
    check("GET /notebook: has cells", "cells" in nb, str(nb.keys()))
    check("GET /notebook: has metadata", "metadata" in nb)
    check("GET /notebook: has nsit_format", "nsit_format" in nb)
    check("GET /notebook: cells non-empty", len(nb.get("cells", [])) > 0)

    # GET /api/kernel/status
    status = api_get("/kernel/status")
    check("GET /kernel/status: alive", status.get("alive") == True, str(status))

    # POST /api/execute
    first_code_cell = None
    for cell in nb["cells"]:
        if cell["type"] == "code":
            first_code_cell = cell
            break
    check("found code cell", first_code_cell is not None)

    if first_code_cell:
        result = api_post("/execute", {
            "cell_id": first_code_cell["id"],
            "code": 'print("hello from test").'
        })
        check("POST /execute: status ok", result.get("status") == "ok", json.dumps(result))
        check("POST /execute: has stdout", "hello from test" in result.get("stdout", ""), result.get("stdout", ""))
        check("POST /execute: has execution_count", result.get("execution_count", 0) > 0)

    # POST /api/kernel/reset
    result = api_post("/kernel/reset")
    check("POST /kernel/reset: ok", result.get("status") == "reset_ok", str(result))

    # POST /api/execute after reset - scope should be clean
    result = api_post("/execute", {
        "cell_id": "test-after-reset",
        "code": "var fresh = 999. print(fresh)."
    })
    check("execute after reset: ok", "999" in result.get("stdout", ""), result.get("stdout", ""))


def test_cell_operations():
    section("Cell Operations (add, delete, move, type)")

    # Add a code cell
    result = api_post("/cell/add", {"type": "code", "after_id": None})
    check("add cell: has id", "id" in result, str(result))
    new_cell_id = result.get("id", "")
    check("add cell: type is code", result.get("type") == "code")

    # Add a markdown cell after it
    result2 = api_post("/cell/add", {"type": "markdown", "after_id": new_cell_id})
    md_cell_id = result2.get("id", "")
    check("add markdown cell: ok", result2.get("type") == "markdown")

    # Toggle type
    result = api_post("/cell/type", {"cell_id": md_cell_id, "type": "code"})
    check("toggle type: ok", result.get("status") == "updated")

    # Move cell
    result = api_post("/cell/move", {"cell_id": new_cell_id, "direction": "down"})
    check("move cell: ok", result.get("status") == "moved")

    # Delete cell
    result = api_post("/cell/delete", {"cell_id": new_cell_id})
    check("delete cell: ok", result.get("status") == "deleted")

    # Delete the markdown cell too
    result = api_post("/cell/delete", {"cell_id": md_cell_id})
    check("delete md cell: ok", result.get("status") == "deleted")


def test_notebook_io():
    section("Notebook I/O (new, save, load)")

    # Create new notebook
    result = api_post("/notebook/new", {"title": "Test Notebook"})
    check("new notebook: has cells", "cells" in result, str(result.keys()))
    check("new notebook: title set", result.get("metadata", {}).get("title") == "Test Notebook")

    # Execute something so we have state
    api_post("/execute", {"cell_id": result["cells"][0]["id"], "code": 'var msg = "saved state". print(msg).'})

    # Save
    tmpfile = os.path.join(tempfile.gettempdir(), "test_notebook_save.nsit")
    result = api_post("/notebook/save", {
        "cells": result["cells"],
        "metadata": result["metadata"],
        "filepath": tmpfile
    })
    check("save: status saved", result.get("status") == "saved")
    check("save: file exists", os.path.exists(tmpfile))

    # Load the saved file
    result = api_post("/notebook/load", {"filepath": tmpfile})
    check("load: has cells", "cells" in result, str(result.keys()))
    check("load: title preserved", result.get("metadata", {}).get("title") == "Test Notebook")

    # Cleanup
    if os.path.exists(tmpfile):
        os.remove(tmpfile)


def test_run_all():
    section("Run All Cells")

    # Create a fresh notebook with sequential cells
    nb = api_post("/notebook/new", {"title": "Run All Test"})
    cells = nb["cells"]

    # Add more cells with code that depends on previous cells
    cell1_id = cells[0]["id"]

    # We need to set code for cell1 through execute
    api_post("/execute", {"cell_id": cell1_id, "code": 'var a = 10. print(a).'})

    # Add second cell
    cell2 = api_post("/cell/add", {"type": "code", "after_id": cell1_id})
    cell2_id = cell2["id"]

    # Reset and run-all to test sequential execution
    api_post("/kernel/reset")
    result = api_post("/run-all", {})
    check("run-all: status ok", result.get("status") == "ok", str(result))
    check("run-all: has results", len(result.get("results", [])) >= 1)

    # Clean up
    api_post("/cell/delete", {"cell_id": cell2_id})


def test_kernel_restart():
    section("Kernel Restart")

    # Execute something
    api_post("/execute", {"cell_id": "pre-restart", "code": "var before = 123."})

    # Restart
    result = api_post("/kernel/restart")
    check("restart: status", result.get("status") == "restarted")

    # After restart, scope should be clean
    result = api_post("/execute", {"cell_id": "post-restart", "code": "print(before)."})
    check("after restart: var is None", "None" in result.get("stdout", ""), result.get("stdout", ""))


def test_multi_cell_scope():
    section("Multi-Cell Scope Persistence")

    api_post("/kernel/reset")

    # Cell 1: define variables
    r1 = api_post("/execute", {"cell_id": "scope1", "code": 'var name = "Alice". var age = 30.'})
    check("cell1: ok", r1["status"] == "ok")

    # Cell 2: use variables from cell 1
    r2 = api_post("/execute", {"cell_id": "scope2", "code": 'print(name + " is " + str(age) + " years old").'})
    check("cell2: scope persists", "Alice is 30 years old" in r2.get("stdout", ""), r2.get("stdout", ""))

    # Cell 3: modify and use
    r3 = api_post("/execute", {"cell_id": "scope3", "code": 'age = age + 1. print("Next year: " + str(age)).'})
    check("cell3: mutation persists", "Next year: 31" in r3.get("stdout", ""), r3.get("stdout", ""))

    # Cell 4: function defined in one cell, called in another
    r4 = api_post("/execute", {"cell_id": "scope4", "code": 'fn greet @(n): print("Hi " + n). ;'})
    check("cell4: fn definition ok", r4["status"] == "ok")

    r5 = api_post("/execute", {"cell_id": "scope5", "code": 'greet("Bob").'})
    check("cell5: fn call across cells", "Hi Bob" in r5.get("stdout", ""), r5.get("stdout", ""))


def test_gui_served():
    section("GUI Served")

    req = urllib.request.Request(BASE_URL + "/")
    with urllib.request.urlopen(req, timeout=10) as resp:
        html = resp.read().decode()
    check("GET /: serves HTML", "<!DOCTYPE html>" in html)
    check("GET /: has ScriptIt title", "ScriptIt" in html)
    check("GET /: has toolbar", "toolbar" in html)
    check("GET /: has JS app", "function render()" in html or "async function init()" in html)


# ─── Test: NSIT File Format ─────────────────────────────

def test_nsit_format():
    section("NSIT File Format")

    if os.path.exists(SAMPLE_NOTEBOOK):
        with open(SAMPLE_NOTEBOOK) as f:
            nb = json.load(f)
        check("nsit: valid JSON", True)
        check("nsit: has nsit_format", nb.get("nsit_format") == "1.0")
        check("nsit: has metadata", "metadata" in nb)
        check("nsit: has cells array", isinstance(nb.get("cells"), list))
        check("nsit: has code cells", any(c["type"] == "code" for c in nb["cells"]))
        check("nsit: has markdown cells", any(c["type"] == "markdown" for c in nb["cells"]))
        check("nsit: cells have id", all("id" in c for c in nb["cells"]))
        check("nsit: cells have source", all("source" in c for c in nb["cells"]))
    else:
        check("nsit: sample file exists", False, SAMPLE_NOTEBOOK)


# ─── Main ────────────────────────────────────────────────

def start_server():
    global server_proc
    server_proc = subprocess.Popen(
        [sys.executable, SERVER_SCRIPT, "--port", str(PORT)],
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        text=True
    )
    # Wait for server to be ready
    for i in range(30):
        time.sleep(0.3)
        try:
            req = urllib.request.Request(BASE_URL + "/api/kernel/status")
            urllib.request.urlopen(req, timeout=2)
            return True
        except:
            pass
    return False


def stop_server():
    global server_proc
    if server_proc:
        server_proc.terminate()
        try:
            server_proc.wait(timeout=5)
        except:
            server_proc.kill()
        server_proc = None


def main():
    global passed, failed

    print("╔════════════════════════════════════════════════════╗")
    print("║  ScriptIt Notebook — Test Suite                    ║")
    print("╚════════════════════════════════════════════════════╝")

    # ── Phase 1: Direct kernel tests (no server) ──
    try:
        test_kernel_direct()
    except Exception as e:
        print(f"  💥 Kernel test crashed: {e}")
        failed += 1

    # ── Phase 2: Check NSIT format ──
    try:
        test_nsit_format()
    except Exception as e:
        print(f"  💥 Format test crashed: {e}")
        failed += 1

    # ── Phase 3: Server tests ──
    print(f"\n{'═'*60}")
    print("  Starting notebook server...")
    if not start_server():
        print("  ❌ Failed to start server!")
        failed += 1
        stop_server()
        print_summary()
        return

    print("  Server started ✅")

    try:
        test_gui_served()
        test_server_api()
        test_cell_operations()
        test_notebook_io()
        test_run_all()
        test_kernel_restart()
        test_multi_cell_scope()
    except Exception as e:
        print(f"\n  💥 Server test crashed: {e}")
        import traceback
        traceback.print_exc()
        failed += 1
    finally:
        stop_server()

    print_summary()


def print_summary():
    total = passed + failed
    print(f"\n{'═'*60}")
    print(f"  Results: {passed}/{total} passed", end="")
    if failed > 0:
        print(f", {failed} FAILED")
        for e in errors:
            print(f"    {e}")
    else:
        print(" — ALL PASSED ✅")
    print(f"{'═'*60}\n")
    sys.exit(0 if failed == 0 else 1)


if __name__ == "__main__":
    main()
