# Introduction to Xell

Welcome to **Xell**, a modern, cross-platform scripting language designed to replace traditional shells like Bash, PowerShell, and Zsh. 

Xell provides a consistent, readable, and powerful environment for automation, file operations, and process management, eliminating the "platform quirks" typically associated with shell scripting.

## 🎯 Philosophy

The core philosophy of Xell is **"Write Once, Run Anywhere."** Whether you are on Linux, macOS, or Windows, your Xell scripts will behave identically. 

Xell focuses on:
- **Readability**: A clean syntax that avoids the arcane symbols of traditional shells.
- **Predictability**: Consistent behavior across different operating systems.
- **Power**: Full access to system processes, networking, and filesystem operations.

---

## 🛠️ Installation & Building

### Prerequisites
To build Xell from source, you will need:
- **CMake** (Version 3.16+)
- **C++ Compiler** (G++ or Clang supporting C++17)

### Option 1: One-Command Installer (Recommended)
The easiest way to install Xell is using the provided installation script:

```bash
# Install to ~/.local/bin (No sudo required)
./install.sh --local

# Install to /usr/local/bin (Requires sudo)
./install.sh --system
```

### Option 2: Building from Source
If you prefer to build it manually:

```bash
mkdir build && cd build
cmake ..
cmake --build .
```
The resulting binary `xell` will be located in the `build` directory.

---

## 🚀 Your First Xell Script

Creating a Xell script is simple. Files use the `.xel` extension.

### 1. Create a file named `hello.xel`
```xell
# This is a comment
name = "World"
print "Hello, {name}!"
```

### 2. Run the script
Using the `xell` executable:

```bash
xell hello.xel
```

**Output:**
`Hello, World!`

---

## 📖 What's Next?

Now that you have Xell running, move on to the **[Grammar](./Grammar.md)** guide to learn about syntax rules, whitespace, and how to structure your code.
