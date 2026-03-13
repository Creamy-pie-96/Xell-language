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

set -euo pipefail

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
RUN_TESTS=false

for arg in "$@"; do
    case "$arg" in
        --system) INSTALL_MODE="system" ;;
        --local)  INSTALL_MODE="local" ;;
        --clean)  CLEAN=true ;;
        --test)   RUN_TESTS=true ;;
        --help|-h)
            echo "Usage: ./install.sh [--local|--system] [--clean] [--test]"
            echo ""
            echo "  --local   Install to ~/.local/bin (default, no sudo)"
            echo "  --system  Install to /usr/local/bin (needs sudo)"
            echo "  --clean   Clean build directory first"
            echo "  --test    Build test targets and run tests"
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
PYTHON_BIN=""

if command -v python3 >/dev/null 2>&1; then
    PYTHON_BIN="python3"
elif command -v python >/dev/null 2>&1; then
    PYTHON_BIN="python"
fi

if [ -n "$PYTHON_BIN" ]; then
    ok "$PYTHON_BIN $($PYTHON_BIN --version 2>&1 | awk '{print $2}')"
else
    warn "Python not found — grammar generation will be skipped"
fi

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

if [ "$CLEAN" = true ]; then
    echo "  Cleaning everything for fresh install..."
    # Build directory
    [ -d "$BUILD_DIR" ] && rm -rf "$BUILD_DIR"
    # Installed binaries
    $SUDO rm -f "$BIN_DIR/xell" "$BIN_DIR/xell-terminal" 2>/dev/null || true
    # Symlinks in ~/.local/bin
    rm -f "$HOME/.local/bin/xell" "$HOME/.local/bin/xell-terminal" 2>/dev/null || true
    # Installed data (both system and local)
    $SUDO rm -rf "/usr/local/share/xell" 2>/dev/null || true
    rm -rf "$HOME/.local/share/xell" 2>/dev/null || true
    # Terminal build
    [ -d "$SCRIPT_DIR/xell-terminal/build" ] && rm -rf "$SCRIPT_DIR/xell-terminal/build"
    ok "Cleaned: build dirs, binaries, data, symlinks"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$BIN_DIR/.." >/dev/null 2>&1

if [ "$RUN_TESTS" = true ]; then
    make -j"$(nproc)" 2>&1
else
    make -j"$(nproc)" xell 2>&1
fi

if [ ! -f "$BUILD_DIR/xell" ]; then
    fail "Build failed — xell binary not found!"
fi
ok "Build successful"

# ---- Step 3: Run tests (only with --test flag) ----

if [ "$RUN_TESTS" = true ]; then
    step "3/6" "Running tests..."

    TEST_OUTPUT=$(ctest --output-on-failure 2>&1)
    if echo "$TEST_OUTPUT" | grep -q "100% tests passed"; then
        TEST_COUNT=$(echo "$TEST_OUTPUT" | grep -o '[0-9]* tests passed' | head -1)
        ok "All $TEST_COUNT"
    else
        warn "Some tests failed:"
        echo "$TEST_OUTPUT" | tail -20
    fi
else
    step "3/6" "Skipping tests (use --test to run)"
    ok "Skipped"
fi

# ---- Step 4: Install binary + data ----

step "4/7" "Installing Xell..."

# When installing system-wide, root may not be able to read the build
# directory (common on mounted/encrypted home).  Stage to /tmp first.
if [ "$INSTALL_MODE" = "system" ]; then
    TMP_STAGE=$(mktemp -d /tmp/xell_install.XXXXXX)
    trap 'rm -rf "$TMP_STAGE"' EXIT

    cp "$BUILD_DIR/xell" "$TMP_STAGE/xell"

    $SUDO mkdir -p "$BIN_DIR"
    $SUDO mv "$TMP_STAGE/xell" "$BIN_DIR/xell"
    $SUDO chmod 755 "$BIN_DIR/xell"
    ok "Binary installed: $BIN_DIR/xell"

    # Also ensure ~/.local/bin/xell points to the system binary so that
    # shells with cached hash tables or PATH ordering pick up the right one.
    LOCAL_BIN="$HOME/.local/bin"
    mkdir -p "$LOCAL_BIN"
    rm -f "$LOCAL_BIN/xell"
    ln -sf "$BIN_DIR/xell" "$LOCAL_BIN/xell"
    ok "Symlinked $LOCAL_BIN/xell → $BIN_DIR/xell"
else
    mkdir -p "$BIN_DIR"
    cp "$BUILD_DIR/xell" "$BIN_DIR/xell"
    chmod 755 "$BIN_DIR/xell"
    ok "Binary installed: $BIN_DIR/xell"
fi

# Install customizer + data
if [ "$INSTALL_MODE" = "system" ]; then
    TMP_DATA=$(mktemp -d /tmp/xell_data.XXXXXX)
    trap 'rm -rf "$TMP_STAGE" "$TMP_DATA"' EXIT

    CUSTOMIZER_SRC="$SCRIPT_DIR/Extensions/xell-vscode/color_customizer"
    if [ -d "$CUSTOMIZER_SRC" ]; then
        mkdir -p "$TMP_DATA/color_customizer"
        cp "$CUSTOMIZER_SRC/customizer_server.py" "$TMP_DATA/color_customizer/" 2>/dev/null || true
        cp "$CUSTOMIZER_SRC/customize.html" "$TMP_DATA/color_customizer/" 2>/dev/null || true
        cp "$CUSTOMIZER_SRC/token_data.json" "$TMP_DATA/color_customizer/" 2>/dev/null || true
    fi

    STDLIB_SRC="$SCRIPT_DIR/stdlib"
    if [ -d "$STDLIB_SRC" ]; then
        mkdir -p "$TMP_DATA/stdlib"
        cp -r "$STDLIB_SRC"/* "$TMP_DATA/stdlib/" 2>/dev/null || true
    fi

    $SUDO mkdir -p "$SHARE_DIR"
    $SUDO cp -r "$TMP_DATA"/* "$SHARE_DIR/" 2>/dev/null || true
    ok "Data installed: $SHARE_DIR"
else
    CUSTOMIZER_SRC="$SCRIPT_DIR/Extensions/xell-vscode/color_customizer"
    if [ -d "$CUSTOMIZER_SRC" ]; then
        mkdir -p "$SHARE_DIR/color_customizer"
        cp "$CUSTOMIZER_SRC/customizer_server.py" "$SHARE_DIR/color_customizer/" 2>/dev/null || true
        cp "$CUSTOMIZER_SRC/customize.html" "$SHARE_DIR/color_customizer/" 2>/dev/null || true
        cp "$CUSTOMIZER_SRC/token_data.json" "$SHARE_DIR/color_customizer/" 2>/dev/null || true
        ok "Customizer installed: $SHARE_DIR/color_customizer/"
    fi

    STDLIB_SRC="$SCRIPT_DIR/stdlib"
    if [ -d "$STDLIB_SRC" ]; then
        mkdir -p "$SHARE_DIR/stdlib"
        cp -r "$STDLIB_SRC"/* "$SHARE_DIR/stdlib/" 2>/dev/null || true
        ok "Stdlib installed: $SHARE_DIR/stdlib/"
    fi
fi

# ---- Step 5: Build + Install xell-terminal (if SDL2 available) ----

step "5/7" "Building xell-terminal..."

TERMINAL_SRC="$SCRIPT_DIR/xell-terminal"
HAS_SDL2=false

if [ -d "$TERMINAL_SRC" ]; then
    if pkg-config --exists sdl2 sdl2_ttf 2>/dev/null; then
        HAS_SDL2=true
    elif [ -f /usr/include/SDL2/SDL.h ] 2>/dev/null; then
        HAS_SDL2=true
    fi

    if [ "$HAS_SDL2" = true ]; then
        TERMINAL_BUILD="$TERMINAL_SRC/build"
        mkdir -p "$TERMINAL_BUILD"
        cd "$TERMINAL_BUILD"
        cmake .. -DCMAKE_BUILD_TYPE=Release >/dev/null 2>&1
        make -j"$(nproc)" 2>&1 | tail -3

        if [ -f "$TERMINAL_BUILD/xell-terminal" ]; then
            ok "xell-terminal built"

            # Determine font destination
            TERM_SHARE_DIR="${SHARE_DIR%/xell}/xell-terminal"   # e.g. /usr/local/share/xell-terminal

            if [ "$INSTALL_MODE" = "system" ]; then
                # Stage to /tmp first (encrypted mount can't copy directly to root)
                TMP_TERM=$(mktemp -d /tmp/xell_term.XXXXXX)

                # Copy binary
                cp "$TERMINAL_BUILD/xell-terminal" "$TMP_TERM/xell-terminal"

                # Copy font assets
                mkdir -p "$TMP_TERM/fonts"
                cp "$TERMINAL_SRC"/assets/fonts/*.ttf "$TMP_TERM/fonts/" 2>/dev/null || true

                # Copy terminal_colors.json (theme data)
                COLORS_SRC="$SCRIPT_DIR/Extensions/xell-vscode/color_customizer/terminal_colors.json"
                [ -f "$COLORS_SRC" ] && cp "$COLORS_SRC" "$TMP_TERM/terminal_colors.json"

                # Copy autocomplete data (language_data.json + snippets)
                LANG_DATA_SRC="$TERMINAL_BUILD/assets/language_data.json"
                SNIPPETS_SRC="$TERMINAL_BUILD/assets/xell_snippets.json"
                [ -f "$LANG_DATA_SRC" ] && cp "$LANG_DATA_SRC" "$TMP_TERM/language_data.json"
                [ -f "$SNIPPETS_SRC" ] && cp "$SNIPPETS_SRC" "$TMP_TERM/xell_snippets.json"

                # Install binary
                $SUDO mkdir -p "$BIN_DIR"
                $SUDO mv "$TMP_TERM/xell-terminal" "$BIN_DIR/xell-terminal"
                $SUDO chmod 755 "$BIN_DIR/xell-terminal"

                # Install fonts
                $SUDO mkdir -p "$TERM_SHARE_DIR/fonts"
                $SUDO cp "$TMP_TERM"/fonts/*.ttf "$TERM_SHARE_DIR/fonts/" 2>/dev/null || true

                # Install theme data
                [ -f "$TMP_TERM/terminal_colors.json" ] && \
                    $SUDO cp "$TMP_TERM/terminal_colors.json" "$TERM_SHARE_DIR/"

                # Install autocomplete data
                [ -f "$TMP_TERM/language_data.json" ] && \
                    $SUDO cp "$TMP_TERM/language_data.json" "$TERM_SHARE_DIR/"
                [ -f "$TMP_TERM/xell_snippets.json" ] && \
                    $SUDO cp "$TMP_TERM/xell_snippets.json" "$TERM_SHARE_DIR/"

                rm -rf "$TMP_TERM"

                # Symlink to ~/.local/bin too
                rm -f "$HOME/.local/bin/xell-terminal"
                ln -sf "$BIN_DIR/xell-terminal" "$HOME/.local/bin/xell-terminal"
            else
                # Local install — no sudo needed
                mkdir -p "$BIN_DIR"
                cp "$TERMINAL_BUILD/xell-terminal" "$BIN_DIR/xell-terminal"
                chmod 755 "$BIN_DIR/xell-terminal"

                mkdir -p "$TERM_SHARE_DIR/fonts"
                cp "$TERMINAL_SRC"/assets/fonts/*.ttf "$TERM_SHARE_DIR/fonts/" 2>/dev/null || true

                # Install theme data
                COLORS_SRC="$SCRIPT_DIR/Extensions/xell-vscode/color_customizer/terminal_colors.json"
                [ -f "$COLORS_SRC" ] && cp "$COLORS_SRC" "$TERM_SHARE_DIR/"

                # Install autocomplete data
                LANG_DATA_SRC="$TERMINAL_BUILD/assets/language_data.json"
                SNIPPETS_SRC="$TERMINAL_BUILD/assets/xell_snippets.json"
                [ -f "$LANG_DATA_SRC" ] && cp "$LANG_DATA_SRC" "$TERM_SHARE_DIR/"
                [ -f "$SNIPPETS_SRC" ] && cp "$SNIPPETS_SRC" "$TERM_SHARE_DIR/"
            fi
            ok "Terminal installed: $BIN_DIR/xell-terminal"
            ok "Fonts installed: $TERM_SHARE_DIR/fonts/"
        else
            warn "xell-terminal build failed"
        fi
    else
        warn "SDL2/SDL2_ttf not found — skipping xell-terminal (install: sudo apt install libsdl2-dev libsdl2-ttf-dev)"
    fi
else
    warn "xell-terminal source not found — skipping"
fi

# ---- Step 6: Generate grammar + Build VS Code extension ----

step "6/7" "Building VS Code extension..."

EXT_DIR="$SCRIPT_DIR/Extensions/xell-vscode"

if [ "$HAS_NODE" = true ] && [ "$HAS_NPM" = true ]; then
    # Run grammar generator
    if [ -n "$PYTHON_BIN" ]; then
        if "$PYTHON_BIN" "$SCRIPT_DIR/Extensions/gen_xell_grammar.py" 2>&1 | sed 's/^/  /'; then
            ok "Grammar generated from C++ sources"
        else
            fail "Grammar generation failed"
        fi
    else
        warn "Skipping grammar generation (python not found)"
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

# ---- Step 7: Install VS Code extension ----

step "7/7" "Installing VS Code extension..."

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

# ---- Verify installation ----

WHICH_XELL=$(which xell 2>/dev/null || true)
if [ -n "$WHICH_XELL" ]; then
    # Resolve symlinks to get the real path
    REAL_XELL=$(readlink -f "$WHICH_XELL" 2>/dev/null || echo "$WHICH_XELL")
    REAL_BIN=$(readlink -f "$BIN_DIR/xell" 2>/dev/null || echo "$BIN_DIR/xell")
    if [ "$REAL_XELL" = "$REAL_BIN" ]; then
        ok "which xell → $WHICH_XELL ✓"
    else
        warn "which xell → $WHICH_XELL (expected $BIN_DIR/xell)"
        warn "Your PATH may have a stale xell. Remove it: rm $WHICH_XELL"
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
echo -e "           ${BOLD}xell --terminal${NC}          # Launch Xell Terminal"
echo -e "           ${BOLD}xell --customize${NC}         # Color customizer"
echo -e "           ${BOLD}xell --gen_xesy${NC}          # Generate dialect template"
echo -e "           ${BOLD}xell --convert file.xel${NC}  # Convert dialect → canonical"
echo -e "           ${BOLD}xell --revert file.xel${NC}   # Revert canonical → dialect"
echo -e "           ${BOLD}xell --version${NC}           # Check version"
echo ""
