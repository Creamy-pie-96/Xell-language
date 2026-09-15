# Getting Started with Xell

> **Previous:** [Documentation Home](README.md)  
> **Next:** [Overview](core/overview.md)

---

## Installation

Xell is distributed as a single C++ executable. The recommended way to install is using the provided `install.sh` script, which builds Xell from source and installs everything in one go.

### Quick Install (Recommended)

Clone the repository and run the installer:

```bash
git clone https://github.com/Creamy-pie-96/Xell-language.git
cd Xell-language
./install.sh
```

The script will:
- Check for dependencies (CMake, C++ compiler, Python, Node.js, etc.)
- Build Xell from source
- Install the `xell` binary to `~/.local/bin`
- Install the color customizer and standard library to `~/.local/share/xell`
- Build and install the `xell-terminal` GUI (if SDL2 is available)
- Build and install the VS Code extension (if Node.js is available)

### Install Options

| Option | Description |
|--------|-------------|
| `./install.sh` | Default – install to `~/.local` (no `sudo` needed) |
| `./install.sh --system` | Install to `/usr/local` (requires `sudo`) |
| `./install.sh --clean` | Clean build directories before installing |
| `./install.sh --test` | Build and run tests |
| `./install.sh --help` | Show usage help |

**Example:**

```bash
# Install locally with a clean build
./install.sh --clean

# Install system‑wide and run tests
./install.sh --system --test
```

### Manual Build (Alternative)

If you prefer to build manually:

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt install cmake g++ libsdl2-dev libsdl2-ttf-dev

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Install
sudo make install   # installs to /usr/local
```

### From a Release (Pre‑built Binary)

If you prefer a pre‑built binary, download the latest release from the [GitHub releases page](https://github.com/Creamy-pie-96/Xell-language.git/releases):

```bash
curl -L https://github.com/Creamy-pie-96/Xell-language.git/releases/latest/download/xell -o xell
chmod +x xell
sudo mv xell /usr/local/bin/
```

---

## The REPL

Start the interactive Read‑Eval‑Print Loop with no arguments:

```bash
xell
```

You’ll see a prompt:
```
Xell v0.1.0
>
```

Type expressions and statements directly:

```
> print("Hello, world!")
Hello, world!

> let answer = 42
> answer
42

> if answer == 42 : print("The meaning of life") ;
The meaning of life
```

- End a statement with `;` (semicolon).  
- For multi‑line input, use `\` at the end of a line to continue.

---

## Your First Program

Create a file named `hello.xel`:

```xell
print("Hello, Xell!")

let name = "world"
print(f"Hello, {name}!")
```

Run it:

```bash
xell hello.xel
```

Output:
```
Hello, Xell!
Hello, world!
```

---

## Basic Syntax in 30 Seconds

- **Comments**  
  ```xell
  # This is a single‑line comment
  --> This is a
      multi‑line comment <--
  ```

- **Variables**  
  ```xell
  let x = 10          # mutable
  immutable y = 20    # immutable (cannot be reassigned)
  ```

- **Functions**  
  ```xell
  fn greet(name): print(f"Hello, {name}") ;
  ```

- **Conditionals**  
  ```xell
  if x > 0 : print("positive")
  elif x == 0 : print("zero")
  else : print("negative") ;
  ```

- **Loops**  
  ```xell
  for i in range(5): print(i) ;
  ```

- **Data structures**  
  ```xell
  let list = [1, 2, 3]
  let map = { "key": "value", "answer": 42 }
  ```

---

## Next Steps

Continue with the **[Overview](core/overview.md)** to understand Xell’s design, or dive directly into any topic from the sidebar.

---

**Previous:** [Documentation Home](README.md)  
**Next:** [Overview](core/overview.md)