#!/usr/bin/env bash
# ╔══════════════════════════════════════════════════╗
# ║  Xell Notebook Launcher                          ║
# ╚══════════════════════════════════════════════════╝
#
# Usage:
#   ./notebook.sh                      # New empty notebook
#   ./notebook.sh my_notebook.nxel     # Open existing notebook
#   ./notebook.sh --port 9999          # Custom port
#   ./notebook.sh my.nxel --port 9999  # Both

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Try multiple locations for notebook files
if [ -f "$SCRIPT_DIR/notebook/xell_notebook_server.py" ]; then
    NOTEBOOK_DIR="$SCRIPT_DIR/notebook"
elif [ -f "$SCRIPT_DIR/../share/xell/notebook/xell_notebook_server.py" ]; then
    NOTEBOOK_DIR="$(cd "$SCRIPT_DIR/../share/xell/notebook" && pwd)"
elif [ -f "/usr/local/share/xell/notebook/xell_notebook_server.py" ]; then
    NOTEBOOK_DIR="/usr/local/share/xell/notebook"
else
    echo -e "\033[0;31mError: Cannot find xell_notebook_server.py\033[0m"
    echo "  Tried: $SCRIPT_DIR/notebook/"
    echo "  Tried: $SCRIPT_DIR/../share/xell/notebook/"
    echo "  Tried: /usr/local/share/xell/notebook/"
    exit 1
fi

SERVER="$NOTEBOOK_DIR/xell_notebook_server.py"

# Find the xell binary
BINARY="$(command -v xell 2>/dev/null || echo "")"
if [ -z "$BINARY" ] || [ ! -f "$BINARY" ]; then
    # Try relative build locations
    for candidate in \
        "$SCRIPT_DIR/../build/xell" \
        "$SCRIPT_DIR/../build/bin/xell" \
        "$SCRIPT_DIR/xell" \
        "/usr/local/bin/xell"; do
        if [ -f "$candidate" ] && [ -x "$candidate" ]; then
            BINARY="$candidate"
            break
        fi
    done
fi

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

# ─── Check / Build binary ────────────────────────────────

if [ -z "$BINARY" ] || { ! command -v "$BINARY" &>/dev/null && [ ! -f "$BINARY" ]; }; then
    # Try building from source if we're in the dev tree
    BUILD_DIR="$SCRIPT_DIR/../build"
    CMAKELISTS="$SCRIPT_DIR/../CMakeLists.txt"
    if [ -f "$CMAKELISTS" ]; then
        echo -e "${CYAN}Building Xell...${NC}"
        mkdir -p "$BUILD_DIR"
        cd "$BUILD_DIR"
        cmake .. -DCMAKE_BUILD_TYPE=Release 2>/dev/null
        make -j$(nproc) 2>/dev/null
        BINARY="$BUILD_DIR/xell"
        cd "$SCRIPT_DIR"
        if [ -f "$BINARY" ]; then
            echo -e "${GREEN}✓ Built successfully${NC}"
        else
            echo -e "${RED}Error: Build failed. xell binary not produced.${NC}"
            exit 1
        fi
    else
        echo -e "${RED}Error: xell binary not found. Build it first:${NC}"
        echo "  cd build && cmake .. && make"
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
            echo "Usage: $0 [notebook.nxel] [--port PORT]"
            echo ""
            echo "Options:"
            echo "  --port, -p PORT    Server port (default: 8888)"
            echo "  --help, -h         Show this help"
            exit 0
            ;;
        *)
            if [[ "$1" == *.nxel ]]; then
                NOTEBOOK_FILE="$1"
            fi
            shift
            ;;
    esac
done

# ─── Launch ──────────────────────────────────────────────

echo -e ""
echo -e "${BOLD}${BLUE}╔══════════════════════════════════════════════════╗${NC}"
echo -e "${BOLD}${BLUE}║${NC}  ${BOLD}Xell Notebook${NC}                                    ${BOLD}${BLUE}║${NC}"
echo -e "${BOLD}${BLUE}╚══════════════════════════════════════════════════╝${NC}"
echo -e ""
echo -e "  ${CYAN}URL:${NC}      http://localhost:$PORT"
if [ -n "$NOTEBOOK_FILE" ]; then
    echo -e "  ${CYAN}File:${NC}     $NOTEBOOK_FILE"
fi
echo -e "  ${CYAN}Binary:${NC}   $BINARY"
echo -e "  ${CYAN}Press${NC}     Ctrl+C to stop"
echo -e ""

# Ensure xell binary is in PATH for the notebook server
export PATH="$(dirname "$BINARY"):$PATH"

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
