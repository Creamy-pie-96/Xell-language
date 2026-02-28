#!/usr/bin/env bash
# ╔══════════════════════════════════════════════════╗
# ║  ScriptIt Notebook Launcher                      ║
# ╚══════════════════════════════════════════════════╝
#
# Usage:
#   ./notebook.sh                      # New empty notebook
#   ./notebook.sh my_notebook.nsit     # Open existing notebook
#   ./notebook.sh --port 9999          # Custom port
#   ./notebook.sh my.nsit --port 9999  # Both

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Try multiple locations for notebook files
# 1. Local REPL layout: $SCRIPT_DIR/notebook/
# 2. System install: /usr/local/share/scriptit/notebook/
# 3. Prefix-based: $SCRIPT_DIR/../share/scriptit/notebook/
if [ -f "$SCRIPT_DIR/notebook/notebook_server.py" ]; then
    NOTEBOOK_DIR="$SCRIPT_DIR/notebook"
elif [ -f "$SCRIPT_DIR/../share/scriptit/notebook/notebook_server.py" ]; then
    NOTEBOOK_DIR="$(cd "$SCRIPT_DIR/../share/scriptit/notebook" && pwd)"
elif [ -f "/usr/local/share/scriptit/notebook/notebook_server.py" ]; then
    NOTEBOOK_DIR="/usr/local/share/scriptit/notebook"
else
    echo -e "\033[0;31mError: Cannot find notebook_server.py\033[0m"
    echo "  Tried: $SCRIPT_DIR/notebook/"
    echo "  Tried: $SCRIPT_DIR/../share/scriptit/notebook/"
    echo "  Tried: /usr/local/share/scriptit/notebook/"
    exit 1
fi

SERVER="$NOTEBOOK_DIR/notebook_server.py"
BINARY="$(command -v scriptit 2>/dev/null || echo "$SCRIPT_DIR/scriptit")"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

# ─── Check / Build binary ────────────────────────────────

if ! command -v "$BINARY" &>/dev/null && [ ! -f "$BINARY" ]; then
    # Try building from source if we're in the dev tree
    CPP_SOURCE="$SCRIPT_DIR/ScriptIt.cpp"
    INCLUDE_DIR="$(cd "$SCRIPT_DIR/../../.." 2>/dev/null && pwd)/include"
    STUBS_DIR="$(cd "$SCRIPT_DIR/../../.." 2>/dev/null && pwd)/src/pythonicDispatchStubs.cpp"
    if [ -f "$CPP_SOURCE" ] && [ -f "$STUBS_DIR" ]; then
        echo -e "${CYAN}Building ScriptIt...${NC}"
        g++ -std=c++20 -I"$INCLUDE_DIR" -O2 -o "$SCRIPT_DIR/scriptit" "$CPP_SOURCE" "$STUBS_DIR"
        BINARY="$SCRIPT_DIR/scriptit"
        echo -e "${GREEN}✓ Built successfully${NC}"
    else
        echo -e "${RED}Error: scriptit binary not found. Install ScriptIt first.${NC}"
        exit 1
    fi
fi

# ─── Check dependencies ──────────────────────────────────

if ! command -v python3 &>/dev/null; then
    echo -e "${RED}Error: python3 not found${NC}"
    exit 1
fi

if [ ! -f "$SERVER" ]; then
    echo -e "${RED}Error: Server not found at $SERVER${NC}"
    exit 1
fi

# ─── Parse args ──────────────────────────────────────────

PORT=8888
NOTEBOOK_FILE=""

while [[ $# -gt 0 ]]; do
    case "$1" in
        --port|-p)
            PORT="$2"
            shift 2
            ;;
        --help|-h)
            echo "Usage: $0 [notebook.nsit] [--port PORT]"
            echo ""
            echo "Options:"
            echo "  --port, -p PORT    Server port (default: 8888)"
            echo "  --help, -h         Show this help"
            exit 0
            ;;
        *)
            if [[ "$1" == *.nsit ]]; then
                NOTEBOOK_FILE="$1"
            fi
            shift
            ;;
    esac
done

# ─── Launch ──────────────────────────────────────────────

echo -e ""
echo -e "${BOLD}${BLUE}╔══════════════════════════════════════════════════╗${NC}"
echo -e "${BOLD}${BLUE}║${NC}  ${BOLD}ScriptIt Notebook${NC}                               ${BOLD}${BLUE}║${NC}"
echo -e "${BOLD}${BLUE}╚══════════════════════════════════════════════════╝${NC}"
echo -e ""
echo -e "  ${CYAN}URL:${NC}      http://localhost:$PORT"
if [ -n "$NOTEBOOK_FILE" ]; then
    echo -e "  ${CYAN}File:${NC}     $NOTEBOOK_FILE"
fi
echo -e "  ${CYAN}Binary:${NC}   $BINARY"
echo -e "  ${CYAN}Press${NC}     Ctrl+C to stop"
echo -e ""

# Build command
CMD=(python3 "$SERVER" --port "$PORT")
if [ -n "$NOTEBOOK_FILE" ]; then
    CMD+=("$NOTEBOOK_FILE")
fi

# Open browser after short delay
(sleep 1.5 && {
    if command -v xdg-open &>/dev/null; then
        xdg-open "http://localhost:$PORT" 2>/dev/null
    elif command -v open &>/dev/null; then
        open "http://localhost:$PORT"
    fi
}) &

# Run server (foreground, Ctrl+C will stop it)
exec "${CMD[@]}"
