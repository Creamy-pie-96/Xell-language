#!/usr/bin/env bash
# =============================================================================
# Xell — One-Command Installer
# =============================================================================
#
# Builds Xell from source, installs the binary, customizer, and VS Code
# extension — everything in one shot.
#
# Usage:
#   ./install.sh           # Build + install everything
#   ./install.sh --local   # Install to ~/.local (default, no sudo needed)
#   ./install.sh --system  # Install to /usr/local (needs sudo)
#   ./install.sh --clean   # Clean build before installing
#
# =============================================================================

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
BOLD='\033[1m'
NC='\033[0m'

step() { echo -e "\n${CYAN}${BOLD}[$1]${NC} $2"; }
ok()   { echo -e "  ${GREEN}✓${NC} $1"; }
warn() { echo -e "  ${YELLOW}⚠${NC} $1"; }
fail() { echo -e "  ${RED}✗${NC} $1"; exit 1; }

# ---- Parse args ----

INSTALL_MODE="local"
CLEAN=false

for arg in "$@"; do
    case "$arg" in
        --system) INSTALL_MODE="system" ;;
        --local)  INSTALL_MODE="local" ;;
        --clean)  CLEAN=true ;;
        --help|-h)
            echo "Usage: ./install.sh [--local|--system] [--clean]"
            echo ""
            echo "  --local   Install to ~/.local/bin (default, no sudo)"
            echo "  --system  Install to /usr/local/bin (needs sudo)"
            echo "  --clean   Clean build directory first"
            exit 0
            ;;
    esac
done

# ---- Determine paths ----

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"

if [ "$INSTALL_MODE" = "system" ]; then
    BIN_DIR="/usr/local/bin"
    SHARE_DIR="/usr/local/share/xell"
    SUDO="sudo"
else
    BIN_DIR="$HOME/.local/bin"
    SHARE_DIR="$HOME/.local/share/xell"
    SUDO=""
fi

echo -e "${BOLD}${CYAN}"
echo "╔══════════════════════════════════════════════════╗"
echo "║           Xell Language Installer                ║"
echo "╚══════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "  Mode:      ${BOLD}$INSTALL_MODE${NC}"
echo -e "  Binary:    ${BOLD}$BIN_DIR/xell${NC}"
echo -e "  Data:      ${BOLD}$SHARE_DIR${NC}"
echo ""

# ---- Step 1: Check dependencies ----

step "1/6" "Checking dependencies..."

command -v cmake >/dev/null 2>&1 || fail "cmake not found. Install it: sudo apt install cmake"
command -v g++ >/dev/null 2>&1 || command -v c++ >/dev/null 2>&1 || fail "C++ compiler not found. Install: sudo apt install g++"
ok "cmake $(cmake --version | head -1 | awk '{print $3}')"
ok "$(g++ --version | head -1 2>/dev/null || c++ --version | head -1)"

HAS_NODE=false
HAS_NPM=false
HAS_CODE=false

if command -v node >/dev/null 2>&1; then
    ok "node $(node --version)"
    HAS_NODE=true
fi
if command -v npm >/dev/null 2>&1; then
    ok "npm $(npm --version)"
    HAS_NPM=true
fi
if command -v code >/dev/null 2>&1; then
    ok "VS Code found"
    HAS_CODE=true
elif command -v code-insiders >/dev/null 2>&1; then
    ok "VS Code Insiders found"
    HAS_CODE=true
fi

# ---- Step 2: Build ----

step "2/6" "Building Xell..."

if [ "$CLEAN" = true ] && [ -d "$BUILD_DIR" ]; then
    echo "  Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$BIN_DIR/.." >/dev/null 2>&1
make -j"$(nproc)" 2>&1 | tail -5

if [ ! -f "$BUILD_DIR/xell" ]; then
    fail "Build failed — xell binary not found!"
fi
ok "Build successful"

# ---- Step 3: Run tests ----

step "3/6" "Running tests..."

TEST_OUTPUT=$(ctest --output-on-failure 2>&1)
if echo "$TEST_OUTPUT" | grep -q "100% tests passed"; then
    TEST_COUNT=$(echo "$TEST_OUTPUT" | grep -o '[0-9]* tests passed' | head -1)
    ok "All $TEST_COUNT"
else
    warn "Some tests failed:"
    echo "$TEST_OUTPUT" | tail -20
fi

# ---- Step 4: Install binary + data ----

step "4/6" "Installing Xell..."

$SUDO mkdir -p "$BIN_DIR"
$SUDO cp "$BUILD_DIR/xell" "$BIN_DIR/xell"
$SUDO chmod 755 "$BIN_DIR/xell"
ok "Binary installed: $BIN_DIR/xell"

# Install customizer + data
$SUDO mkdir -p "$SHARE_DIR/color_customizer"
CUSTOMIZER_SRC="$SCRIPT_DIR/Extensions/xell-vscode/color_customizer"
if [ -d "$CUSTOMIZER_SRC" ]; then
    $SUDO cp "$CUSTOMIZER_SRC/customizer_server.py" "$SHARE_DIR/color_customizer/" 2>/dev/null || true
    $SUDO cp "$CUSTOMIZER_SRC/customize.html" "$SHARE_DIR/color_customizer/" 2>/dev/null || true
    $SUDO cp "$CUSTOMIZER_SRC/token_data.json" "$SHARE_DIR/color_customizer/" 2>/dev/null || true
    ok "Customizer installed: $SHARE_DIR/color_customizer/"
fi

# Install stdlib
STDLIB_SRC="$SCRIPT_DIR/stdlib"
if [ -d "$STDLIB_SRC" ]; then
    $SUDO mkdir -p "$SHARE_DIR/stdlib"
    $SUDO cp -r "$STDLIB_SRC"/* "$SHARE_DIR/stdlib/" 2>/dev/null || true
    ok "Stdlib installed: $SHARE_DIR/stdlib/"
fi

# ---- Step 5: Generate grammar + Build VS Code extension ----

step "5/6" "Building VS Code extension..."

EXT_DIR="$SCRIPT_DIR/Extensions/xell-vscode"

if [ "$HAS_NODE" = true ] && [ "$HAS_NPM" = true ]; then
    # Run grammar generator
    if command -v python3 >/dev/null 2>&1; then
        python3 "$SCRIPT_DIR/Extensions/gen_xell_grammar.py" 2>&1 | sed 's/^/  /'
        ok "Grammar generated from C++ sources"
    fi

    cd "$EXT_DIR"
    npm install --silent 2>&1 | tail -1
    npx tsc -b 2>&1 || true

    # Convert icon if needed
    ICON_SVG="$EXT_DIR/images/icon.svg"
    ICON_PNG="$EXT_DIR/images/icon.png"
    if [ -f "$ICON_SVG" ] && command -v convert >/dev/null 2>&1; then
        if [ ! -f "$ICON_PNG" ] || [ "$ICON_SVG" -nt "$ICON_PNG" ]; then
            convert -background none -density 300 "$ICON_SVG" -resize 256x256 "$ICON_PNG" 2>/dev/null
            ok "Icon converted SVG → PNG"
        fi
    fi

    # Package
    rm -f "$EXT_DIR"/*.vsix
    npx @vscode/vsce package --allow-missing-repository 2>&1 | tail -3 | sed 's/^/  /'
    ok "Extension packaged"
else
    warn "Node.js/npm not found — skipping VS Code extension build"
fi

# ---- Step 6: Install VS Code extension ----

step "6/6" "Installing VS Code extension..."

VSIX=$(find "$EXT_DIR" -name "*.vsix" -type f 2>/dev/null | head -1)

if [ -n "$VSIX" ] && [ "$HAS_CODE" = true ]; then
    CODE_CMD="code"
    command -v code >/dev/null 2>&1 || CODE_CMD="code-insiders"
    $CODE_CMD --install-extension "$VSIX" 2>&1 | sed 's/^/  /'
    ok "VS Code extension installed"
elif [ -n "$VSIX" ]; then
    warn "VS Code not found. Install manually: code --install-extension $VSIX"
else
    warn "No .vsix file found — extension not built"
fi

# ---- PATH check ----

if [ "$INSTALL_MODE" = "local" ]; then
    if ! echo "$PATH" | grep -q "$HOME/.local/bin"; then
        echo ""
        warn "Add ~/.local/bin to your PATH:"
        echo -e "    ${BOLD}echo 'export PATH=\"\$HOME/.local/bin:\$PATH\"' >> ~/.bashrc && source ~/.bashrc${NC}"
    fi
fi

# ---- Done! ----

echo ""
echo -e "${GREEN}${BOLD}╔══════════════════════════════════════════════════╗"
echo "║          ✅ Xell installed successfully!          ║"
echo "╚══════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "  Try it:  ${BOLD}xell${NC}                    # Start REPL"
echo -e "           ${BOLD}xell hello.xel${NC}           # Run a script"
echo -e "           ${BOLD}xell --customize${NC}         # Color customizer"
echo -e "           ${BOLD}xell --version${NC}           # Check version"
echo ""
