# Xell Terminal Emulator

A cross-platform terminal emulator built specifically for the **Xell** programming language.
Written in C++17 with SDL2 + SDL_ttf for rendering.

## Features

- **Real PTY** — spawns Xell (or any shell) in a proper pseudo-terminal
- **VT100/ANSI support** — full state-machine parser for escape sequences
- **True color** — supports 16, 256, and 24-bit RGB colors
- **Scrollback buffer** — 5000 lines of history, mouse wheel + Shift+PageUp/Down
- **Cursor blinking** — block cursor with configurable blink rate
- **Window resize** — dynamically resizes the terminal grid and notifies the PTY
- **Cross-platform** — Linux (forkpty), macOS (forkpty), Windows (ConPTY)
- **Bundled font** — ships with JetBrains Mono, no system font dependencies
- **Glyph cache** — efficient texture caching for fast rendering

## Architecture

```
┌─────────────┐     bytes      ┌──────────────┐     update     ┌──────────────┐
│    PTY       │ ─────────────▶ │  VT Parser   │ ─────────────▶ │ ScreenBuffer │
│ (forkpty /   │                │ (state        │                │ (2D cell     │
│  ConPTY)     │                │  machine)     │                │  grid)       │
└──────┬───────┘                └──────────────┘                └──────┬───────┘
       │                                                               │
       │ write                                                         │ read
       │                                                               │
┌──────┴───────┐                                                ┌──────┴───────┐
│    Input     │                                                │   Renderer   │
│   Handler    │◀─── SDL events ────────────────────────────────│   (SDL2 +    │
│              │                                                │    SDL_ttf)  │
└──────────────┘                                                └──────────────┘
```

## Prerequisites

- **CMake** ≥ 3.16
- **SDL2** development libraries
- **SDL2_ttf** development libraries
- **C++17** compiler (GCC ≥ 7, Clang ≥ 5, MSVC ≥ 2017)

### Install dependencies

**Linux (Debian/Ubuntu):**

```bash
sudo apt install cmake libsdl2-dev libsdl2-ttf-dev
```

**Linux (Fedora):**

```bash
sudo dnf install cmake SDL2-devel SDL2_ttf-devel
```

**Linux (Arch):**

```bash
sudo pacman -S cmake sdl2 sdl2_ttf
```

**macOS:**

```bash
brew install cmake sdl2 sdl2_ttf
```

## Build

```bash
# Quick build (Debug mode, auto-downloads font)
./build.sh

# Release build
./build.sh release

# Clean
./build.sh clean
```

Or manually:

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

## Run

```bash
# Launch with Xell shell (auto-detected)
./build/xell-terminal

# Launch with a specific shell
./build/xell-terminal /bin/bash

# Set Xell path explicitly
export XELL_PATH=/usr/local/bin/xell
./build/xell-terminal
```

## Keyboard Shortcuts

| Key            | Action                      |
| -------------- | --------------------------- |
| Ctrl+C         | Send interrupt (SIGINT)     |
| Ctrl+D         | Send EOF                    |
| Ctrl+Z         | Suspend                     |
| Ctrl+L         | Clear screen                |
| Shift+PageUp   | Scroll up through history   |
| Shift+PageDown | Scroll down through history |
| Mouse Wheel    | Scroll through history      |
| Any key        | Return to live view         |

## Project Structure

```
xell-terminal/
├── CMakeLists.txt          # Build configuration
├── build.sh                # One-command build script
├── README.md               # This file
├── assets/
│   └── fonts/
│       └── JetBrainsMono-Regular.ttf
└── src/
    ├── main.cpp            # Entry point, main loop
    ├── pty/
    │   ├── pty.hpp         # Unified PTY interface
    │   ├── pty_unix.cpp    # Linux/macOS: forkpty()
    │   └── pty_win.cpp     # Windows: CreatePseudoConsole()
    ├── terminal/
    │   ├── types.hpp       # Color, Cell structs
    │   ├── screen_buffer.hpp/.cpp  # 2D cell grid + scrollback
    │   ├── vt_parser.hpp/.cpp      # ANSI/VT100 state machine
    │   └── input_handler.hpp/.cpp  # SDL keys → PTY bytes
    └── renderer/
        ├── renderer.hpp
        └── renderer.cpp    # SDL2 + SDL_ttf rendering
```
