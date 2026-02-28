#!/usr/bin/env python3
"""
Xell Color Customizer — Backend Server
Serves the color customizer HTML and handles save requests.

Two save modes (auto-detected):
  DEV MODE  — extension.ts found nearby → write rules there + rebuild
  USER MODE — system install            → write to VS Code settings.json

Flags:
  --port N          Listen on port N (default 7890)
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
from urllib.parse import urlparse, parse_qs
import webbrowser
import threading


class ThreadingHTTPServer(ThreadingMixIn, HTTPServer):
    daemon_threads = True

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

# ── Find extension.ts (dev tree) ──
def _find_extension_ts():
    """Locate the extension directory and extension.ts."""
    d = os.path.dirname(SCRIPT_DIR)
    for _ in range(4):
        candidate = os.path.join(d, "src", "client", "extension.ts")
        if os.path.exists(candidate):
            return d, candidate
        d = os.path.dirname(d)
    return None, None

EXT_DIR, EXTENSION_TS = _find_extension_ts()
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
    """Serves customize.html and handles /api/save, /api/status."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=SCRIPT_DIR, **kwargs)

    def log_message(self, format, *args):
        pass

    # ── GET ──
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
            })
        elif parsed.path == "/api/current-rules":
            self._handle_current_rules()
        else:
            super().do_GET()

    def _handle_current_rules(self):
        """Read current XELL_TOKEN_RULES from extension.ts and return them."""
        if not EXTENSION_TS or not os.path.exists(EXTENSION_TS):
            self._send_json({"rules": [], "source": "none"})
            return
        try:
            with open(EXTENSION_TS, "r") as f:
                content = f.read()
            pattern = r"const XELL_TOKEN_RULES = \[(.*?)\];"
            match = re.search(pattern, content, re.DOTALL)
            if not match:
                self._send_json({"rules": [], "source": "not-found"})
                return
            block = match.group(1)
            rules = []
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

    # ── OPTIONS (CORS) ──
    def do_OPTIONS(self):
        self.send_response(200)
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
        self.send_header("Access-Control-Allow-Headers", "Content-Type")
        self.end_headers()

    # ── POST ──
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

    # ── Save logic ──
    def _handle_save(self, install=False):
        try:
            length = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(length).decode("utf-8")
            data = json.loads(body)
            rules = data.get("rules", [])
            if not rules:
                self._send_json({"status": "error", "message": "No rules provided"})
                return
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
            if DEV_MODE:
                result = self._save_to_extension_ts(flat_rules, install)
            else:
                result = self._save_to_vscode_settings(flat_rules)
            self._send_json(result)
        except Exception as e:
            self._send_json({"status": "error", "message": str(e)})

    # ── Streaming Save & Install (SSE) ──
    def _handle_save_stream(self):
        try:
            length = int(self.headers.get("Content-Length", 0))
            body = self.rfile.read(length).decode("utf-8")
            data = json.loads(body)
            rules = data.get("rules", [])
            if not rules:
                self._send_sse_error("No rules provided")
                return
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
                result = self._save_to_vscode_settings(flat_rules)
                send_event("log", result.get("message", "Done"))
                send_event("done", json.dumps(result))
                return

            send_event("log", "📝 Saving colors to extension.ts...")
            if not EXTENSION_TS or not os.path.exists(EXTENSION_TS):
                send_event("error", "extension.ts not found")
                return

            with open(EXTENSION_TS, "r") as f:
                content = f.read()
            new_block = self._build_ts_rules_block(flat_rules)
            pattern = r"const XELL_TOKEN_RULES = \[.*?\];"
            match = re.search(pattern, content, re.DOTALL)
            if not match:
                send_event("error", "Could not find XELL_TOKEN_RULES in extension.ts")
                return
            new_content = content[:match.start()] + new_block + content[match.end():]
            with open(EXTENSION_TS, "w") as f:
                f.write(new_content)
            send_event("log", "✅ Colors saved to extension.ts")
            send_event("log", "🔄 Rebuild the extension to see changes")
            send_event("done", json.dumps({"status": "ok", "message": "Colors saved to extension.ts! Rebuild to apply."}))
        except Exception as e:
            try:
                self._send_sse_error(str(e))
            except:
                pass

    def _send_sse_error(self, message):
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(f"event: error\ndata: {message}\n\n".encode("utf-8"))
        self.wfile.flush()

    # ── DEV MODE: patch extension.ts ──
    def _save_to_extension_ts(self, rules, install):
        with open(EXTENSION_TS, "r") as f:
            content = f.read()
        new_block = self._build_ts_rules_block(rules)
        pattern = r"const XELL_TOKEN_RULES = \[.*?\];"
        match = re.search(pattern, content, re.DOTALL)
        if not match:
            return {"status": "error", "message": "Could not find XELL_TOKEN_RULES in extension.ts"}
        new_content = content[:match.start()] + new_block + content[match.end():]
        with open(EXTENSION_TS, "w") as f:
            f.write(new_content)
        return {"status": "ok", "message": "Colors saved to extension.ts! Rebuild to apply."}

    # ── USER MODE: patch VS Code settings.json ──
    def _save_to_vscode_settings(self, rules):
        if not VSCODE_SETTINGS:
            return {"status": "error", "message": "Could not locate VS Code settings.json on this OS."}
        tm_rules = []
        for r in rules:
            entry = {"scope": r["scope"], "settings": {"foreground": r.get("foreground", "#ffffff")}}
            fs = r.get("fontStyle", "")
            if fs:
                entry["settings"]["fontStyle"] = fs
            tm_rules.append(entry)

        new_block = json.dumps({"textMateRules": tm_rules}, indent=6)
        new_block_inner = new_block.strip()[1:-1].strip()

        if not os.path.exists(VSCODE_SETTINGS):
            os.makedirs(os.path.dirname(VSCODE_SETTINGS), exist_ok=True)
            content = '{\n    "editor.tokenColorCustomizations": {\n'
            content += '      ' + new_block_inner + '\n'
            content += '    }\n}\n'
            with open(VSCODE_SETTINGS, "w") as f:
                f.write(content)
            return {"status": "ok",
                    "message": f"Colors saved to VS Code settings!\n{VSCODE_SETTINGS}\nReload VS Code to see changes."}

        with open(VSCODE_SETTINGS, "r") as f:
            content = f.read()

        pattern = r'"editor\.tokenColorCustomizations"\s*:\s*'
        match = re.search(pattern, content)
        if match:
            start = content.index("{", match.end())
            depth = 0
            end = start
            for i in range(start, len(content)):
                if content[i] == "{": depth += 1
                elif content[i] == "}":
                    depth -= 1
                    if depth == 0:
                        end = i + 1
                        break
            replacement = "{\n      " + new_block_inner + "\n    }"
            content = content[:match.end()] + replacement + content[end:]
        else:
            last_brace = content.rstrip().rfind("}")
            if last_brace == -1:
                return {"status": "error", "message": "settings.json appears malformed"}
            before = content[:last_brace].rstrip()
            needs_comma = before and before[-1] not in (",", "{")
            comma = "," if needs_comma else ""
            inject = f'{comma}\n    "editor.tokenColorCustomizations": {{\n      {new_block_inner}\n    }}\n'
            content = content[:last_brace] + inject + content[last_brace:]

        with open(VSCODE_SETTINGS, "w") as f:
            f.write(content)
        return {"status": "ok",
                "message": f"Colors saved to VS Code settings!\n{VSCODE_SETTINGS}\nReload VS Code to see changes."}

    # ── Helpers ──
    def _build_ts_rules_block(self, rules):
        lines = ["const XELL_TOKEN_RULES = ["]
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
        mode_str = "DEV MODE  → saves to extension.ts"
        save_target = EXTENSION_TS
    else:
        mode_str = "USER MODE → saves to VS Code settings.json"
        save_target = VSCODE_SETTINGS or "(not found)"

    server = ThreadingHTTPServer(("127.0.0.1", port), CustomizerHandler)
    print(f"""
\033[1m\033[96m╔══════════════════════════════════════════════════╗
║  Xell Color Customizer                            ║
╚══════════════════════════════════════════════════╝\033[0m

  \033[96mURL:\033[0m      http://localhost:{port}
  \033[96mMode:\033[0m     {mode_str}
  \033[96mSaves to:\033[0m {save_target}
  \033[96mPress\033[0m     Ctrl+C to stop
""")

    def open_browser():
        import time; time.sleep(1)
        webbrowser.open(f"http://localhost:{port}")

    threading.Thread(target=open_browser, daemon=True).start()

    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n[Server] Shutting down...")
        server.shutdown()


if __name__ == "__main__":
    main()
