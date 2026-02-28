#!/usr/bin/env python3
"""
ScriptIt Notebook Server
Serves the notebook GUI and manages the ScriptIt kernel process.
Usage: python3 notebook_server.py [notebook.nsit] [--port PORT]
"""

import json
import os
import sys
import shutil
import subprocess
import threading
import time
import uuid
import http.server
import socketserver
import urllib.parse
from pathlib import Path
from datetime import datetime

# ─── Configuration ───────────────────────────────────────

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

# Find the scriptit binary — check PATH first, then relative locations
SCRIPTIT_BINARY = shutil.which("scriptit")
if not SCRIPTIT_BINARY:
    # Try relative to this script
    candidates = [
        os.path.join(SCRIPT_DIR, "..", "scriptit"),         # REPL layout
        os.path.join(SCRIPT_DIR, "..", "..", "bin", "scriptit"),  # share -> bin
        "/usr/local/bin/scriptit",
    ]
    for c in candidates:
        if os.path.isfile(c) and os.access(c, os.X_OK):
            SCRIPTIT_BINARY = c
            break
if not SCRIPTIT_BINARY:
    SCRIPTIT_BINARY = "scriptit"  # hope it's in PATH
DEFAULT_PORT = 8888
NOTEBOOK_DIR = SCRIPT_DIR

# ─── Kernel Manager ─────────────────────────────────────

class KernelManager:
    """Manages the ScriptIt kernel subprocess."""

    def __init__(self):
        self.process = None
        self.lock = threading.RLock()  # Reentrant lock for restart()
        self.execution_count = 0

    def start(self):
        """Start the kernel process."""
        with self.lock:
            if self.process and self.process.poll() is None:
                return  # already running
            self.process = subprocess.Popen(
                [SCRIPTIT_BINARY, "--kernel"],
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                bufsize=1  # line-buffered
            )
            # Read the ready message
            ready = self.process.stdout.readline().strip()
            info = json.loads(ready)
            if info.get("status") != "kernel_ready":
                raise RuntimeError(f"Kernel failed to start: {ready}")
            print(f"[Kernel] Started (PID {self.process.pid}, version {info.get('version', '?')})")

    def stop(self):
        """Stop the kernel process."""
        with self.lock:
            if self.process and self.process.poll() is None:
                try:
                    self.process.stdin.write(json.dumps({"action": "shutdown"}) + "\n")
                    self.process.stdin.flush()
                    self.process.wait(timeout=3)
                except:
                    self.process.kill()
                print("[Kernel] Stopped")
            self.process = None

    def restart(self):
        """Restart the kernel."""
        with self.lock:
            # Stop the old process directly (without re-acquiring lock)
            if self.process and self.process.poll() is None:
                try:
                    self.process.stdin.write(json.dumps({"action": "shutdown"}) + "\n")
                    self.process.stdin.flush()
                    self.process.wait(timeout=3)
                except Exception as e:
                    print(f"[Kernel] Shutdown error during restart: {e}")
                    try:
                        self.process.kill()
                        self.process.wait(timeout=2)
                    except:
                        pass
                print("[Kernel] Stopped for restart")
            elif self.process:
                print(f"[Kernel] Process already dead (returncode={self.process.returncode})")
            self.process = None
            self.execution_count = 0

            # Start a new process directly (without re-acquiring lock)
            try:
                self.process = subprocess.Popen(
                    [SCRIPTIT_BINARY, "--kernel"],
                    stdin=subprocess.PIPE,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True,
                    bufsize=1
                )
                ready = self.process.stdout.readline().strip()
                if not ready:
                    raise RuntimeError("Kernel produced no output on startup")
                info = json.loads(ready)
                if info.get("status") != "kernel_ready":
                    raise RuntimeError(f"Kernel failed to start: {ready}")
                print(f"[Kernel] Restarted (PID {self.process.pid})")
            except Exception as e:
                print(f"[Kernel] Failed to restart: {e}")
                self.process = None
                raise

    def execute(self, cell_id, code):
        """Execute code in the kernel and return the result."""
        with self.lock:
            if not self.process or self.process.poll() is not None:
                print("[Kernel] Dead before execute — restarting")
                self.process = None
                self.start()
            try:
                cmd = json.dumps({"action": "execute", "cell_id": cell_id, "code": code})
                self.process.stdin.write(cmd + "\n")
                self.process.stdin.flush()
                response = self.process.stdout.readline().strip()
                if not response:
                    raise RuntimeError("No response from kernel (process may have crashed)")
                return json.loads(response)
            except Exception as e:
                print(f"[Kernel] Execute error: {e}")
                # Try to restart kernel and return error
                try:
                    self.process = None
                    self.start()
                except:
                    pass
                return {
                    "cell_id": cell_id,
                    "status": "error",
                    "stdout": "",
                    "stderr": f"Kernel error: {e}",
                    "result": "",
                    "execution_count": 0
                }

    def reset(self):
        """Reset the kernel state."""
        with self.lock:
            if not self.process or self.process.poll() is not None:
                # Kernel is dead — restart it instead of just resetting
                print("[Kernel] Dead during reset — restarting")
                self.process = None
                self.execution_count = 0
                try:
                    self.process = subprocess.Popen(
                        [SCRIPTIT_BINARY, "--kernel"],
                        stdin=subprocess.PIPE,
                        stdout=subprocess.PIPE,
                        stderr=subprocess.PIPE,
                        text=True,
                        bufsize=1
                    )
                    ready = self.process.stdout.readline().strip()
                    if not ready:
                        raise RuntimeError("Kernel produced no output on startup")
                    info = json.loads(ready)
                    if info.get("status") != "kernel_ready":
                        raise RuntimeError(f"Kernel failed to start: {ready}")
                    print(f"[Kernel] Restarted during reset (PID {self.process.pid})")
                except Exception as e:
                    print(f"[Kernel] Failed to restart during reset: {e}")
                    self.process = None
                    return {"status": "error", "message": str(e)}
                return {"status": "reset_ok"}
            try:
                cmd = json.dumps({"action": "reset"})
                self.process.stdin.write(cmd + "\n")
                self.process.stdin.flush()
                response = self.process.stdout.readline().strip()
                if not response:
                    raise RuntimeError("No response from kernel")
                self.execution_count = 0
                return json.loads(response)
            except Exception as e:
                print(f"[Kernel] Reset failed: {e}")
                return {"status": "error", "message": str(e)}

    def is_alive(self):
        return self.process is not None and self.process.poll() is None


# ─── Notebook File Manager ───────────────────────────────

def new_notebook(title="Untitled"):
    """Create a new empty notebook."""
    return {
        "nsit_format": "1.0",
        "metadata": {
            "title": title,
            "created": datetime.utcnow().isoformat() + "Z",
            "modified": datetime.utcnow().isoformat() + "Z",
            "kernel": "scriptit",
            "kernel_version": "2.0"
        },
        "cells": [
            {
                "id": str(uuid.uuid4()),
                "type": "code",
                "source": "",
                "outputs": [],
                "execution_count": None,
                "metadata": {}
            }
        ]
    }


def load_notebook(filepath):
    """Load a .nsit notebook file."""
    with open(filepath, "r") as f:
        return json.load(f)


def save_notebook(filepath, notebook):
    """Save a .nsit notebook file."""
    notebook["metadata"]["modified"] = datetime.utcnow().isoformat() + "Z"
    with open(filepath, "w") as f:
        json.dump(notebook, f, indent=2)


# ─── HTTP Server ─────────────────────────────────────────

kernel = KernelManager()
current_notebook_path = None
current_notebook = None


class NotebookHandler(http.server.BaseHTTPRequestHandler):
    """HTTP request handler for the notebook server."""

    def log_message(self, format, *args):
        # Quieter logging
        pass

    def send_json(self, data, status=200):
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(json.dumps(data).encode())

    def send_html(self, html):
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.end_headers()
        self.wfile.write(html.encode())

    def do_OPTIONS(self):
        self.send_response(200)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()

    def do_GET(self):
        global current_notebook

        if self.path == "/" or self.path == "/index.html" or self.path == "/notebook.html":
            gui_path = os.path.join(SCRIPT_DIR, "notebook.html")
            if not os.path.exists(gui_path):
                gui_path = os.path.join(SCRIPT_DIR, "index.html")  # fallback
            if os.path.exists(gui_path):
                with open(gui_path, "r") as f:
                    self.send_html(f.read())
            else:
                self.send_html("<h1>Notebook GUI not found</h1>")
            return

        if self.path == "/api/notebook":
            if current_notebook is None:
                current_notebook = new_notebook()
            self.send_json(current_notebook)
            return

        if self.path == "/api/kernel/status":
            self.send_json({"alive": kernel.is_alive()})
            return

        if self.path == "/api/notebook/list":
            # List all .nsit files in the notebook directory and subdirectories
            files = []
            search_dirs = [NOTEBOOK_DIR]
            # Also search home directory for .nsit files
            home = os.path.expanduser("~")
            if home != NOTEBOOK_DIR:
                search_dirs.append(home)
            for search_dir in search_dirs:
                for root, dirs, filenames in os.walk(search_dir):
                    # Skip hidden directories and common unrelated dirs
                    dirs[:] = [d for d in dirs if not d.startswith('.') and d not in
                              ('node_modules', '__pycache__', 'venv', '.git', 'build')]
                    for fn in sorted(filenames):
                        if fn.endswith(".nsit"):
                            full = os.path.join(root, fn)
                            if full not in files:
                                files.append(full)
                    # Limit depth to avoid scanning too deep
                    if root.count(os.sep) - search_dir.count(os.sep) >= 3:
                        dirs.clear()
            self.send_json({"cwd": NOTEBOOK_DIR, "files": files})
            return

        if self.path.startswith("/api/browse"):
            # Browse a directory on the server filesystem
            parsed = urllib.parse.urlparse(self.path)
            params = urllib.parse.parse_qs(parsed.query)
            browse_path = params.get("path", [NOTEBOOK_DIR])[0]
            if not os.path.isdir(browse_path):
                browse_path = os.path.dirname(browse_path)
            if not os.path.isdir(browse_path):
                browse_path = NOTEBOOK_DIR
            try:
                entries = []
                # Add parent directory entry
                parent = os.path.dirname(browse_path)
                if parent and parent != browse_path:
                    entries.append({"name": "..", "path": parent, "type": "dir"})
                for entry in sorted(os.listdir(browse_path)):
                    if entry.startswith('.'):
                        continue
                    full = os.path.join(browse_path, entry)
                    if os.path.isdir(full):
                        entries.append({"name": entry, "path": full, "type": "dir"})
                    elif entry.endswith(".nsit"):
                        entries.append({"name": entry, "path": full, "type": "file"})
                self.send_json({"current": browse_path, "entries": entries})
            except PermissionError:
                self.send_json({"current": browse_path, "entries": [], "error": "Permission denied"})
            return

        self.send_response(404)
        self.end_headers()

    def do_POST(self):
        global current_notebook, current_notebook_path

        content_length = int(self.headers.get("Content-Length", 0))
        body = self.rfile.read(content_length).decode() if content_length > 0 else "{}"
        data = json.loads(body) if body else {}

        if self.path == "/api/execute":
            cell_id = data.get("cell_id", "")
            code = data.get("code", "")
            start_time = time.time()
            result = kernel.execute(cell_id, code)
            elapsed = time.time() - start_time
            result["exec_time"] = round(elapsed, 4)

            # Update notebook cell outputs
            if current_notebook:
                for cell in current_notebook["cells"]:
                    if cell["id"] == cell_id:
                        cell["source"] = code
                        cell["execution_count"] = result.get("execution_count")
                        cell["outputs"] = []
                        stdout = result.get("stdout", "")
                        stderr = result.get("stderr", "")
                        res = result.get("result", "")
                        if stdout:
                            cell["outputs"].append({"type": "stdout", "text": stdout})
                        if stderr:
                            cell["outputs"].append({"type": "stderr", "text": stderr})
                        if res:
                            cell["outputs"].append({"type": "result", "text": res})
                        break

            self.send_json(result)
            return

        if self.path == "/api/kernel/restart":
            try:
                kernel.restart()
                self.send_json({"status": "restarted"})
            except Exception as e:
                print(f"[Kernel] Restart error: {e}")
                self.send_json({"status": "error", "message": str(e)}, 500)
            return

        if self.path == "/api/kernel/reset":
            result = kernel.reset()
            self.send_json(result)
            return

        if self.path == "/api/notebook/save":
            if current_notebook:
                # Update cells from request if provided
                if "cells" in data:
                    current_notebook["cells"] = data["cells"]
                if "metadata" in data:
                    current_notebook["metadata"].update(data["metadata"])

                filepath = data.get("filepath", current_notebook_path)
                if not filepath:
                    filepath = os.path.join(NOTEBOOK_DIR,
                                          current_notebook["metadata"].get("title", "Untitled") + ".nsit")
                current_notebook_path = filepath
                save_notebook(filepath, current_notebook)
                self.send_json({"status": "saved", "filepath": filepath})
            else:
                self.send_json({"status": "error", "message": "No notebook loaded"}, 400)
            return

        if self.path == "/api/notebook/load":
            filepath = data.get("filepath", "")
            if filepath and os.path.exists(filepath):
                current_notebook = load_notebook(filepath)
                current_notebook_path = filepath
                self.send_json(current_notebook)
            else:
                self.send_json({"status": "error", "message": "File not found"}, 404)
            return

        if self.path == "/api/notebook/new":
            title = data.get("title", "Untitled")
            current_notebook = new_notebook(title)
            current_notebook_path = None
            kernel.reset()
            self.send_json(current_notebook)
            return

        if self.path == "/api/cell/add":
            if current_notebook:
                cell_type = data.get("type", "code")
                after_id = data.get("after_id", None)
                new_cell = {
                    "id": str(uuid.uuid4()),
                    "type": cell_type,
                    "source": "",
                    "outputs": [],
                    "execution_count": None,
                    "metadata": {}
                }
                if after_id:
                    for i, cell in enumerate(current_notebook["cells"]):
                        if cell["id"] == after_id:
                            current_notebook["cells"].insert(i + 1, new_cell)
                            break
                    else:
                        current_notebook["cells"].append(new_cell)
                else:
                    current_notebook["cells"].append(new_cell)
                self.send_json(new_cell)
            return

        if self.path == "/api/cell/delete":
            if current_notebook:
                cell_id = data.get("cell_id", "")
                current_notebook["cells"] = [c for c in current_notebook["cells"] if c["id"] != cell_id]
                if not current_notebook["cells"]:
                    # Always keep at least one cell
                    current_notebook["cells"].append({
                        "id": str(uuid.uuid4()),
                        "type": "code",
                        "source": "",
                        "outputs": [],
                        "execution_count": None,
                        "metadata": {}
                    })
                self.send_json({"status": "deleted"})
            return

        if self.path == "/api/cell/move":
            if current_notebook:
                cell_id = data.get("cell_id", "")
                direction = data.get("direction", "down")  # "up" or "down"
                cells = current_notebook["cells"]
                for i, cell in enumerate(cells):
                    if cell["id"] == cell_id:
                        if direction == "up" and i > 0:
                            cells[i], cells[i - 1] = cells[i - 1], cells[i]
                        elif direction == "down" and i < len(cells) - 1:
                            cells[i], cells[i + 1] = cells[i + 1], cells[i]
                        break
                self.send_json({"status": "moved"})
            return

        if self.path == "/api/cell/type":
            if current_notebook:
                cell_id = data.get("cell_id", "")
                new_type = data.get("type", "code")
                for cell in current_notebook["cells"]:
                    if cell["id"] == cell_id:
                        cell["type"] = new_type
                        cell["outputs"] = []
                        cell["execution_count"] = None
                        break
                self.send_json({"status": "updated"})
            return

        if self.path == "/api/run-all":
            if current_notebook:
                results = []
                for cell in current_notebook["cells"]:
                    if cell["type"] == "code" and cell["source"].strip():
                        result = kernel.execute(cell["id"], cell["source"])
                        cell["execution_count"] = result.get("execution_count")
                        cell["outputs"] = []
                        stdout = result.get("stdout", "")
                        stderr = result.get("stderr", "")
                        res = result.get("result", "")
                        if stdout:
                            cell["outputs"].append({"type": "stdout", "text": stdout})
                        if stderr:
                            cell["outputs"].append({"type": "stderr", "text": stderr})
                        if res:
                            cell["outputs"].append({"type": "result", "text": res})
                        results.append(result)
                self.send_json({"status": "ok", "results": results})
            return

        self.send_response(404)
        self.end_headers()


class ThreadedHTTPServer(socketserver.ThreadingMixIn, http.server.HTTPServer):
    allow_reuse_address = True
    daemon_threads = True


# ─── Main ────────────────────────────────────────────────

def main():
    global current_notebook, current_notebook_path

    port = DEFAULT_PORT
    notebook_file = None

    args = sys.argv[1:]
    i = 0
    while i < len(args):
        if args[i] == "--port" and i + 1 < len(args):
            port = int(args[i + 1])
            i += 2
        elif args[i].endswith(".nsit"):
            notebook_file = os.path.abspath(args[i])
            i += 1
        else:
            i += 1

    # Check binary exists
    if not os.path.exists(SCRIPTIT_BINARY):
        print(f"Error: ScriptIt binary not found at {SCRIPTIT_BINARY}")
        print("Build it first: g++ -std=c++20 -I<path>/include -o scriptit ScriptIt.cpp")
        sys.exit(1)

    # Load or create notebook
    if notebook_file and os.path.exists(notebook_file):
        current_notebook = load_notebook(notebook_file)
        current_notebook_path = notebook_file
        print(f"[Notebook] Loaded: {notebook_file}")
    else:
        title = Path(notebook_file).stem if notebook_file else "Untitled"
        current_notebook = new_notebook(title)
        current_notebook_path = notebook_file
        print(f"[Notebook] Created new: {title}")

    # Start kernel
    kernel.start()

    # Start server
    server = ThreadedHTTPServer(("", port), NotebookHandler)
    print(f"\n╔══════════════════════════════════════════════════╗")
    print(f"║  ScriptIt Notebook                               ║")
    print(f"║  http://localhost:{port:<6}                        ║")
    print(f"╚══════════════════════════════════════════════════╝\n")

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n[Server] Shutting down...")
        kernel.stop()
        server.shutdown()


if __name__ == "__main__":
    main()
