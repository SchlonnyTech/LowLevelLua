# LLL - Low Level Lua

**A Lua-inspired language that compiles directly to LLVM IR for native performance.**

[![Status](https://img.shields.io/badge/status-early_development-orange)](https://lowlevellua.sytecc-pages-dev.workers.dev/)
[![License](https://img.shields.io/badge/license-MIT-blue)](LICENSE)

LLL gives you the familiar syntax of Lua with the power of static typing and direct compilation to native code. No C translation, no hidden overhead—just your code, compiled to LLVM IR for speed.

- **Familiar Syntax:** Write in a clean, Lua-like style with `local`, functions, tables, and control flow.
- **Static Typing:** Catch errors early and enable powerful optimizations with explicit types.
- **Low-Level Power:** Use syscalls, inline assembly, and manual memory management when you need it.
- **No Intermediates:** Compile directly to LLVM IR, skipping C entirely for faster, more predictable builds.
- **Multiple Modes:** Run code instantly with the JIT compiler or produce standalone native binaries.

For examples, features, and the latest progress, visit the official website:
**[https://lowlevellua.sytecc-pages-dev.workers.dev/](https://lowlevellua.sytecc-pages-dev.workers.dev/)**

---

## Quick Start

Create `hello.lll`:

```
print("Hello, World!")
```

Compile and run (AOT):
```
./lllc hello.lll -o hello
./hello
```

JIT mode:
```
./lllc hello.lll --jit
```

Generate LLVM IR:
```
./lllc hello.lll -S
```

---

## Building (Linux)

```
mkdir build && cd build
cmake ..
ninja
```

## License

MIT License

## Author

schlonny

## Contributing

This project is in early development. Contributions welcome but expect breaking changes.

## Donating

Want to help found our little company, projects, and more?
Donate at: **[https://coindrop.to/schlonny](https://coindrop.to/schlonny)**
(We currently accept cryptocurrency only.)
