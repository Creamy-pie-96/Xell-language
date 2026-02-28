#!/usr/bin/env python3
"""
ScriptIt Color Customizer — Backend Server
Serves the color customizer HTML and handles save requests.

Two save modes (auto-detected):
  DEV MODE  — extension.ts found nearby → write rules there + rebuild
  USER MODE — system install            → write to VS Code settings.json

Flags:
  --port N          Listen on port N (default 7890)
  --install-py PATH Use this install.py (and derive extension.ts from it)
"""

import os
import sys
import re
import json
import subprocess
import platform
import shutil
from http.server import HTTPServer, SimpleHTTPRequestHandler
from socketserver import ThreadingMixIn
from urllib.parse import urlparse
import webbrowser
import threading


class ThreadingHTTPServer(ThreadingMixIn, HTTPServer):
    daemon_threads = True

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

# ── CLI: --install-py lets the user point at install.py explicitly ──
def _get_cli_arg(flag):
    if flag in sys.argv:
        idx = sys.argv.index(flag)
        if idx + 1 < len(sys.argv):
            return sys.argv[idx + 1]
    return None

CLI_INSTALL_PY = _get_cli_arg("--install-py")

# ── Find extension.ts (dev tree, system install, or via --install-py) ──
def _find_extension_ts():
    """Locate the extension directory and extension.ts."""
    # 1. If user gave --install-py, derive everything from it
    if CLI_INSTALL_PY and os.path.exists(CLI_INSTALL_PY):
        ext_dir = os.path.dirname(os.path.abspath(CLI_INSTALL_PY))
        ext_ts  = os.path.join(ext_dir, "src", "client", "extension.ts")
        if os.path.exists(ext_ts):
            return ext_dir, ext_ts, os.path.abspath(CLI_INSTALL_PY)

    # 2. Walk upward from SCRIPT_DIR looking for src/client/extension.ts
    d = os.path.dirname(SCRIPT_DIR)
    for _ in range(4):
        candidate = os.path.join(d, "src", "client", "extension.ts")
        if os.path.exists(candidate):
            install = os.path.join(d, "install.py")
            return d, candidate, install if os.path.exists(install) else None
        d = os.path.dirname(d)

    # 3. Check system install location
    sys_ext = "/usr/local/share/scriptit/extension"
    ext_ts  = os.path.join(sys_ext, "src", "client", "extension.ts")
    if os.path.exists(ext_ts):
        install = os.path.join(sys_ext, "install.py")
        return sys_ext, ext_ts, install if os.path.exists(install) else None

    return None, None, None

EXT_DIR, EXTENSION_TS, INSTALL_PY = _find_extension_ts()
DEV_MODE = EXTENSION_TS is not None

# ── VS Code user settings.json (cross-platform) ──
def _find_vscode_settings():
    home = os.path.expanduser("~")
    system = platform.system()
    if system == "Linux":
        return os.path.join(home, ".config", "Code", "User", "settings.json")
    elif system == "Darwin":
        return os.path.join(home, "Library", "Application Support", "Code", "User", "settings.json")
    elif system == "Windows":
        appdata = os.environ.get("APPDATA", os.path.join(home, "AppData", "Roaming"))
        return os.path.join(appdata, "Code", "User", "settings.json")
    return None

VSCODE_SETTINGS = _find_vscode_settings()

PORT = 7890


class CustomizerHandler(SimpleHTTPRequestHandler):
    """Serves customize.html and handles /api/save, /api/save-install, /api/status."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=SCRIPT_DIR, **kwargs)

    def log_message(self, format, *args):
        pass  # suppress noisy logs

    # ── GET ──────────────────────────────────────────────────────────
    def do_GET(self):
        parsed = urlparse(self.path)
        if parsed.path in ("/", "/index.html", "/customize.html"):
            html = os.path.join(SCRIPT_DIR, "customize.html")
            if os.path.exists(html):
                with open(html, "r") as f:
                    content = f.read().encode("utf-8")
                self.send_response(200)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.send_header("Content-Length", str(len(content)))
                self.end_headers()
                self.wfile.write(content)
            else:
                self.send_error(404, "customize.html not found")
        elif parsed.path == "/api/status":
            self._send_json({
                "dev_mode": DEV_MODE,
                "ext_dir": EXT_DIR or "",
                "settings_path": VSCODE_SETTINGS or "",
                "install_py": INSTALL_PY or "",
            })
        elif parsed.path == "/api/browse":
            self._handle_browse()
        elif parsed.path == "/api/current-rules":
            self._handle_current_rules()
        else:
            super().do_GET()

    def _handle_browse(self):
        """List directory contents for the file browser."""
        from urllib.parse import parse_qs
        parsed = urlparse(self.path)
        params = parse_qs(parsed.query)
        dir_path = params.get("dir", [os.path.expanduser("~")])[0]

        if not os.path.isdir(dir_path):
            self._send_json({"error": f"Not a directory: {dir_path}"})
            return

        try:
            entries = []
            # Add parent directory link
            parent = os.path.dirname(dir_path)
            if parent != dir_path:
                entries.append({"name": "..", "path": parent, "type": "dir"})

            items = sorted(os.listdir(dir_path), key=lambda x: (not os.path.isdir(os.path.join(dir_path, x)), x.lower()))
            for name in items:
                if name.startswith("."):
                    continue
                full = os.path.join(dir_path, name)
                is_dir = os.path.isdir(full)
                # Only show directories and .py files
                if is_dir or name.endswith(".py"):
                    entries.append({
                        "name": name,
                        "path": full,
                        "type": "dir" if is_dir else "file",
                    })

            self._send_json({"dir": dir_path, "entries": entries})
        except PermissionError:
            self._send_json({"error": f"Permission denied: {dir_path}"})

    def _handle_current_rules(self):
        """Read current SCRIPTIT_TOKEN_RULES from extension.ts and return them."""
        if not EXTENSION_TS or not os.path.exists(EXTENSION_TS):
            self._send_json({"rules": [], "source": "none"})
            return

        try:
            with open(EXTENSION_TS, "r") as f:
                content = f.read()

            pattern = r"const SCRIPTIT_TOKEN_RULES = \[(.*?)\];"
            match = re.search(pattern, content, re.DOTALL)
            if not match:
                self._send_json({"rules": [], "source": "not-found"})
                return

            # Parse the TS array into rules
            block = match.group(1)
            rules = []
            # Match each { scope: '...', settings: { foreground: '...', fontStyle: '...' } }
            entry_pattern = r"\{\s*scope:\s*'([^']+)',\s*settings:\s*\{([^}]+)\}\s*\}"
            for m in re.finditer(entry_pattern, block):
                scope = m.group(1)
                settings_str = m.group(2)
                fg_match = re.search(r"foreground:\s*'([^']+)'", settings_str)
                fs_match = re.search(r"fontStyle:\s*'([^']+)'", settings_str)
                rules.append({
                    "scope": scope,
                    "foreground": fg_match.group(1) if fg_match else "#ffffff",
                    "fontStyle": fs_match.group(1) if fs_match else "",
                })

            self._send_json({"rules": rules, "source": EXTENSION_TS})
        except Exception as e:
            self._send_json({"rules": [], "source": "error", "error": str(e)})

    # ── OPTIONS (CORS) ──────────────────────────────────────────────
    def do_OPTIONS(self):
        self.send_response(200)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()

    # ── POST ─────────────────────────────────────────────────────────
    def do_POST(self):
        path = urlparse(self.path).path
        if path == "/api/save":
            self._handle_save(install=False)
        elif path == "/api/save-install":
            self._handle_save(install=True)
        elif path == "/api/save-install-stream":
            self._handle_save_stream()
        else:
            self.send_error(404, "Not found")

    # ── Save logic ──────────────────────────────────────────────────
    def _handle_save(self, install=False):
        try:
            length = int(self.headers.get("Content-Length", 0))
            body   = self.rfile.read(length).decode("utf-8")
            data   = json.loads(body)
            rules  = data.get("rules", [])

            if not rules:
                self._send_json({"status": "error", "message": "No rules provided"})
                return

            # Normalize: frontend sends {scope, settings:{foreground, fontStyle}}
            # Flatten to {scope, foreground, fontStyle} for internal use
            flat_rules = []
            for r in rules:
                if "settings" in r and isinstance(r["settings"], dict):
                    flat_rules.append({
                        "scope": r["scope"],
                        "foreground": r["settings"].get("foreground", "#ffffff"),
                        "fontStyle": r["settings"].get("fontStyle", ""),
                    })
                else:
                    flat_rules.append(r)

            # Also check if --install-py was passed via the request body
            req_install_py = data.get("install_py", "")
            if req_install_py:
                self._override_install_py(req_install_py)

            if DEV_MODE:
                result = self._save_to_extension_ts(flat_rules, install)
            else:
                result = self._save_to_vscode_settings(flat_rules)

            self._send_json(result)

        except Exception as e:
            self._send_json({"status": "error", "message": str(e)})

    # ── Streaming Save & Install (SSE) ──────────────────────────────
    def _handle_save_stream(self):
        """Save rules to extension.ts, then stream install.py --ext-only output as SSE."""
        try:
            length = int(self.headers.get("Content-Length", 0))
            body   = self.rfile.read(length).decode("utf-8")
            data   = json.loads(body)
            rules  = data.get("rules", [])

            if not rules:
                self._send_sse_error("No rules provided")
                return

            # Normalize nested format to flat
            flat_rules = []
            for r in rules:
                if "settings" in r and isinstance(r["settings"], dict):
                    flat_rules.append({
                        "scope": r["scope"],
                        "foreground": r["settings"].get("foreground", "#ffffff"),
                        "fontStyle": r["settings"].get("fontStyle", ""),
                    })
                else:
                    flat_rules.append(r)

            # Check for install_py override
            req_install_py = data.get("install_py", "")
            if req_install_py:
                self._override_install_py(req_install_py)

            # Start SSE response
            self.send_response(200)
            self.send_header("Content-Type", "text/event-stream")
            self.send_header("Cache-Control", "no-cache")
            self.send_header("Connection", "keep-alive")
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()

            def send_event(event, data_str):
                self.wfile.write(f"event: {event}\ndata: {data_str}\n\n".encode("utf-8"))
                self.wfile.flush()

            if not DEV_MODE:
                # User mode — no streaming, just save to settings
                result = self._save_to_vscode_settings(flat_rules)
                send_event("log", result.get("message", "Done"))
                send_event("done", json.dumps(result))
                return

            # ── Step 1: Save to extension.ts ──
            send_event("log", "📝 Saving colors to extension.ts...")
            if not EXTENSION_TS or not os.path.exists(EXTENSION_TS):
                send_event("error", "extension.ts not found")
                return

            with open(EXTENSION_TS, "r") as f:
                content = f.read()

            new_block = self._build_ts_rules_block(flat_rules)
            pattern   = r"const SCRIPTIT_TOKEN_RULES = \[.*?\];"
            match     = re.search(pattern, content, re.DOTALL)
            if not match:
                send_event("error", "Could not find SCRIPTIT_TOKEN_RULES in extension.ts")
                return

            new_content = content[:match.start()] + new_block + content[match.end():]
            with open(EXTENSION_TS, "w") as f:
                f.write(new_content)
            send_event("log", "✅ Colors saved to extension.ts")

            # ── Step 2: Rebuild extension ──
            if not INSTALL_PY or not os.path.exists(INSTALL_PY):
                send_event("log", "⚠ install.py not found — colors saved but extension not rebuilt")
                send_event("done", json.dumps({"status": "ok", "message": "Saved (no rebuild)"}))
                return

            send_event("log", "🔨 Rebuilding extension (this may take a moment)...")
            send_event("log", f"   Running: install.py --ext-only")
            send_event("log", f"   Directory: {EXT_DIR}")

            try:
                env = os.environ.copy()
                env["PYTHONUNBUFFERED"] = "1"
                proc = subprocess.Popen(
                    [sys.executable, INSTALL_PY, "--ext-only"],
                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                    text=True, cwd=EXT_DIR,
                    env=env,
                    bufsize=1  # line-buffered
                )

                for line in iter(proc.stdout.readline, ""):
                    line = line.rstrip()
                    if line:
                        # Clean ANSI codes for cleaner display
                        clean = re.sub(r"\033\[[0-9;]*m", "", line)
                        send_event("log", clean)

                proc.wait(timeout=180)

                if proc.returncode == 0:
                    send_event("log", "")
                    send_event("log", "✅ Extension rebuilt & installed!")
                    send_event("log", "🔄 Restart VS Code (Ctrl+Shift+P → Reload Window) to see changes")
                    send_event("done", json.dumps({"status": "ok",
                        "message": "Colors saved, extension rebuilt & installed! Restart VS Code."}))
                else:
                    send_event("log", f"❌ Build failed (exit code {proc.returncode})")
                    send_event("done", json.dumps({"status": "error",
                        "message": f"Build failed with exit code {proc.returncode}"}))

            except subprocess.TimeoutExpired:
                proc.kill()
                send_event("log", "❌ Build timed out (>180s)")
                send_event("done", json.dumps({"status": "error", "message": "Build timed out"}))
            except Exception as e:
                send_event("log", f"❌ Build error: {e}")
                send_event("done", json.dumps({"status": "error", "message": str(e)}))

        except Exception as e:
            try:
                self._send_sse_error(str(e))
            except:
                pass

    def _send_sse_error(self, message):
        """Send an error via SSE format."""
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(f"event: error\ndata: {message}\n\n".encode("utf-8"))
        self.wfile.flush()

    def _override_install_py(self, path):
        """If user provides install.py path via the UI, override globals."""
        global DEV_MODE, EXT_DIR, EXTENSION_TS, INSTALL_PY
        path = os.path.abspath(path)
        if not os.path.exists(path):
            return
        ext_dir = os.path.dirname(path)
        ext_ts  = os.path.join(ext_dir, "src", "client", "extension.ts")
        if os.path.exists(ext_ts):
            EXT_DIR = ext_dir
            EXTENSION_TS = ext_ts
            INSTALL_PY = path
            DEV_MODE = True

    # ── DEV MODE: patch extension.ts ─────────────────────────────────
    def _save_to_extension_ts(self, rules, install):
        with open(EXTENSION_TS, "r") as f:
            content = f.read()

        new_block = self._build_ts_rules_block(rules)
        pattern   = r"const SCRIPTIT_TOKEN_RULES = \[.*?\];"
        match     = re.search(pattern, content, re.DOTALL)
        if not match:
            return {"status": "error",
                    "message": "Could not find SCRIPTIT_TOKEN_RULES in extension.ts"}

        new_content = content[:match.start()] + new_block + content[match.end():]
        with open(EXTENSION_TS, "w") as f:
            f.write(new_content)

        msg = "Colors saved to extension.ts!"

        if install and INSTALL_PY and os.path.exists(INSTALL_PY):
            msg += " Rebuilding extension..."
            try:
                r = subprocess.run(
                    [sys.executable, INSTALL_PY, "--ext-only"],
                    capture_output=True, text=True, timeout=120,
                    cwd=EXT_DIR
                )
                if r.returncode == 0:
                    msg = ("Colors saved, extension rebuilt & installed! "
                           "Restart VS Code to see changes.")
                else:
                    msg += f"\n{r.stderr or r.stdout}"
            except subprocess.TimeoutExpired:
                msg += " Build timed out."
            except Exception as e:
                msg += f" Build error: {e}"

        return {"status": "ok", "message": msg}

    # ── USER MODE: patch VS Code settings.json ───────────────────────
    def _save_to_vscode_settings(self, rules):
        if not VSCODE_SETTINGS:
            return {"status": "error",
                    "message": "Could not locate VS Code settings.json on this OS."}

        # Build the tokenColorCustomizations JSON block
        tm_rules = []
        for r in rules:
            entry = {"scope": r["scope"],
                     "settings": {"foreground": r.get("foreground", "#ffffff")}}
            fs = r.get("fontStyle", "")
            if fs:
                entry["settings"]["fontStyle"] = fs
            tm_rules.append(entry)

        new_block = json.dumps({"textMateRules": tm_rules}, indent=6)
        # Remove outer braces so we can nest it
        new_block_inner = new_block.strip()[1:-1].strip()

        if not os.path.exists(VSCODE_SETTINGS):
            # Create a fresh settings file
            os.makedirs(os.path.dirname(VSCODE_SETTINGS), exist_ok=True)
            content = '{\n    "editor.tokenColorCustomizations": {\n'
            content += '      ' + new_block_inner + '\n'
            content += '    }\n}\n'
            with open(VSCODE_SETTINGS, "w") as f:
                f.write(content)
            return {"status": "ok",
                    "message": f"Colors saved to VS Code settings!\n{VSCODE_SETTINGS}\nReload VS Code to see changes."}

        # Read existing file as raw text (JSONC-safe — no json.load)
        with open(VSCODE_SETTINGS, "r") as f:
            content = f.read()

        # Try to find existing "editor.tokenColorCustomizations" block and replace it
        # Match: "editor.tokenColorCustomizations": { ... }  (with balanced braces)
        pattern = r'"editor\.tokenColorCustomizations"\s*:\s*'
        match = re.search(pattern, content)

        if match:
            # Find the balanced { } block after the key
            start = content.index("{", match.end())
            depth = 0
            end = start
            for i in range(start, len(content)):
                if content[i] == "{":
                    depth += 1
                elif content[i] == "}":
                    depth -= 1
                    if depth == 0:
                        end = i + 1
                        break

            # Replace the value block
            replacement = "{\n      " + new_block_inner + "\n    }"
            content = content[:match.end()] + replacement + content[end:]
        else:
            # No existing block — inject before the last }
            last_brace = content.rstrip().rfind("}")
            if last_brace == -1:
                return {"status": "error", "message": "settings.json appears malformed (no closing brace)"}

            # Check if we need a comma before our new entry
            before = content[:last_brace].rstrip()
            needs_comma = before and before[-1] not in (",", "{")
            comma = "," if needs_comma else ""

            inject = f'{comma}\n    "editor.tokenColorCustomizations": {{\n      {new_block_inner}\n    }}\n'
            content = content[:last_brace] + inject + content[last_brace:]

        # Write back
        with open(VSCODE_SETTINGS, "w") as f:
            f.write(content)

        return {"status": "ok",
                "message": (f"Colors saved to VS Code settings!\n"
                            f"{VSCODE_SETTINGS}\n"
                            f"Reload VS Code (Ctrl+Shift+P → Reload Window) to see changes.")}

    # ── Helpers ──────────────────────────────────────────────────────
    def _build_ts_rules_block(self, rules):
        lines = ["const SCRIPTIT_TOKEN_RULES = ["]
        for r in rules:
            parts = [f"foreground: '{r.get('foreground', '#ffffff')}'"]
            if r.get("fontStyle"):
                parts.append(f"fontStyle: '{r['fontStyle']}'")
            lines.append(f"    {{ scope: '{r['scope']}', settings: {{ {', '.join(parts)} }} }},")
        lines.append("];")
        return "\n".join(lines)

    def _send_json(self, data):
        body = json.dumps(data).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


def main():
    port = PORT
    if "--port" in sys.argv:
        idx = sys.argv.index("--port")
        if idx + 1 < len(sys.argv):
            port = int(sys.argv[idx + 1])

    if DEV_MODE:
        mode_str   = "DEV MODE  → saves to extension.ts"
        save_target = EXTENSION_TS
    else:
        mode_str   = "USER MODE → saves to VS Code settings.json"
        save_target = VSCODE_SETTINGS or "(not found)"

    server = ThreadingHTTPServer(("127.0.0.1", port), CustomizerHandler)
    print(f"""
\033[1m\033[96m╔══════════════════════════════════════════════════╗
║  ScriptIt Color Customizer                        ║
╚══════════════════════════════════════════════════╝\033[0m

  \033[96mURL:\033[0m      http://localhost:{port}
  \033[96mMode:\033[0m     {mode_str}
  \033[96mSaves to:\033[0m {save_target}
  \033[96mPress\033[0m     Ctrl+C to stop
""")

    def open_browser():
        import time; time.sleep(1)
        url = f"http://localhost:{port}"
        # If running as root (sudo), try to open browser as the real user
        real_user = os.environ.get("SUDO_USER", "")
        if real_user and os.geteuid() == 0:
            try:
                subprocess.Popen(
                    ["su", real_user, "-c", f"xdg-open '{url}' 2>/dev/null || open '{url}' 2>/dev/null"],
                    stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
                )
                return
            except:
                pass
        webbrowser.open(url)

    threading.Thread(target=open_browser, daemon=True).start()

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n[Server] Shutting down...")
        server.shutdown()


if __name__ == "__main__":
    main()
