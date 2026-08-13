# LLL - Low Level Lua Compiler

LLL is a Lua-like language that compiles to C for native performance. Zero dependencies, zero bullshit.

## IMPORTANT: Language Status

LLL is currently in EARLY DEVELOPMENT. The language is nearly not done. Many features are missing, buggy, or experimental. Do not use for production projects yet.

## Why LLL?

LLL combines the simplicity of Lua with the power and performance of C.

## Features

- Lua-like syntax with static typing
- Compiles to native C code
- Structs and enums support
- Plugin system (experimental)
- JIT compilation (experimental, uses TCC)
- Zero external dependencies

## Compilation Modes

### AOT (Ahead-of-Time) Compilation - Recommended

AOT converts LLL to C, then GCC compiles to native code. Most reliable mode.

lllc hello.lll
./hello

### JIT (Just-in-Time) Compilation - Experimental

JIT uses TinyCC (TCC) to compile in memory. Fast compilation, experimental.

lllc --jit hello.lll

## Quick Start

### Install

make
sudo make install

### Hello World

print("Hello, World!")

lllc hello.lll
./hello
