#!/usr/bin/env bash
# =============================================================================
# build.sh — Build the Xell Terminal Emulator
# =============================================================================
# Usage:
#   ./build.sh            Build in Debug mode
#   ./build.sh release    Build in Release mode
#   ./build.sh clean      Remove build directory
#   ./build.sh install    Build + install to system
# =============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

BUILD_TYPE="${1:-Debug}"
BUILD_DIR="build"

if [ "$1" = "release" ]; then
    BUILD_TYPE="Release"
    BUILD_DIR="build_release"
fi

if [ "$1" = "clean" ]; then
    echo "Cleaning build directories..."
    rm -rf build build_release
    echo "Done."
    exit 0
fi

# --- Check dependencies ---
echo "=== Xell Terminal Build ==="
echo ""

check_dep() {
    if ! command -v "$1" &>/dev/null; then
        echo "ERROR: '$1' is required but not found."
        echo "  Install it with: $2"
        exit 1
    fi
}

check_dep cmake "sudo apt install cmake  OR  brew install cmake"
check_dep pkg-config "sudo apt install pkg-config  OR  brew install pkg-config"

# Check for SDL2
if ! pkg-config --exists sdl2 2>/dev/null; then
    echo "ERROR: SDL2 development libraries not found."
    echo ""
    echo "  Linux (Debian/Ubuntu):  sudo apt install libsdl2-dev libsdl2-ttf-dev"
    echo "  Linux (Fedora):         sudo dnf install SDL2-devel SDL2_ttf-devel"
    echo "  Linux (Arch):           sudo pacman -S sdl2 sdl2_ttf"
    echo "  macOS:                  brew install sdl2 sdl2_ttf"
    echo ""
    exit 1
fi

if ! pkg-config --exists SDL2_ttf 2>/dev/null; then
    echo "ERROR: SDL2_ttf development libraries not found."
    echo ""
    echo "  Linux (Debian/Ubuntu):  sudo apt install libsdl2-ttf-dev"
    echo "  macOS:                  brew install sdl2_ttf"
    echo ""
    exit 1
fi

# --- Download font if not present ---
FONT_FILE="assets/fonts/JetBrainsMono-Regular.ttf"
if [ ! -f "$FONT_FILE" ]; then
    echo "Downloading JetBrains Mono font..."
    mkdir -p assets/fonts

    FONT_URL="https://github.com/JetBrains/JetBrainsMono/releases/download/v2.304/JetBrainsMono-2.304.zip"
    TEMP_ZIP="/tmp/jetbrains_mono.zip"

    if command -v wget &>/dev/null; then
        wget -q -O "$TEMP_ZIP" "$FONT_URL"
    elif command -v curl &>/dev/null; then
        curl -sL -o "$TEMP_ZIP" "$FONT_URL"
    else
        echo "ERROR: wget or curl required to download font."
        echo "  Or manually place a .ttf monospace font at: $FONT_FILE"
        exit 1
    fi

    # Extract just the Regular weight
    if command -v unzip &>/dev/null; then
        unzip -o -j "$TEMP_ZIP" "fonts/ttf/JetBrainsMono-Regular.ttf" -d assets/fonts/ 2>/dev/null || true
    else
        echo "ERROR: unzip required to extract font."
        exit 1
    fi

    rm -f "$TEMP_ZIP"

    if [ ! -f "$FONT_FILE" ]; then
        echo "WARNING: Font download/extraction failed."
        echo "  Please manually place a monospace .ttf font at: $FONT_FILE"
        echo "  Continuing build anyway..."
    else
        echo "Font downloaded successfully."
    fi
fi

# --- Build ---
echo ""
echo "Building Xell Terminal ($BUILD_TYPE)..."
echo ""

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake .. \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

cmake --build . -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"

echo ""
echo "=== Build complete ==="
echo "  Executable: $SCRIPT_DIR/$BUILD_DIR/xell-terminal"
echo ""
echo "  Run with:   ./$BUILD_DIR/xell-terminal"
echo "  Or specify a shell:  ./$BUILD_DIR/xell-terminal /bin/bash"
echo ""

# --- Install ---
if [ "$1" = "install" ]; then
    echo "Installing..."
    sudo cmake --install .
    echo "Installed to system."
fi
