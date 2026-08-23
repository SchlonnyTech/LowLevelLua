# LLL - Low Level Lua - Language Compiler

LLL is a Lua-like language that compiles directly to LLVM IR for native performance. Zero C translation, zero dependencies beyond LLVM.

## Status

EARLY DEVELOPMENT - Working compiler with core features. LLVM IR generation, JIT compilation, and native binary output all functional.

## Features

Lua-like syntax with static typing. Compiles directly to LLVM IR (no C intermediate).
JIT compilation using LLVM ExecutionEngine.
Native binary output via LLVM TargetMachine.
Structs and enums support.
Syscall support. Inline LLVM assembly. (not fully done)
Custom builtins (min, max, abs).
Math functions (sqrt, pow, sin, cos, floor, ceil).
String operations (strlen, strcat, strcmp).
Memory management (malloc, free, memset, memcpy). (not fully done)
Tables (1-indexed arrays). 
String concatenation with .. 

## Building 
*linux*
  ```bash
mkdir build
cd build
cmake ..
ninja
```
## Quick Start

Create hello.lll:
```
print("Hello, World!")
```
Compile (AOT):
```bash
./lllc hello.lll -o hello
./hello
```
JIT:
```
./lllc hello.lll --jit
``` 
Generate LLVM IR:
```
./lllc hello.lll -S
```
## Language Overview
```lua
-- Variables with type inference
local name = "LLL"
local version = 42

-- Explicit types
local count: int = 42

-- Functions (luau-style)
local function greet(person: string): string
    return "Hello, " .. person .. "!"
end

-- Structs (currently broken? or not?)
struct Point
    x: float64
    y: float64
end

-- Enums
enum Color
    RED = 1
    GREEN = 2
    BLUE = 4
end

-- Control flow
if score >= 90 then
    print("Excellent!")
elseif score >= 80 then
    print("Good!")
else
    print("Needs work")
end

-- Loops
for i = 1, 10 do
    print(i)
end

while count > 0 do
    count = count - 1
end

repeat
    num = num * 2
until num > 100

-- Tables (1-indexed)
local nums = {1, 2, 3, 4, 5}
print(nums[1])
```
## Command Line Options

lllc file.lll - AOT compile to executable
lllc -S file.lll - Generate LLVM IR only
lllc --jit file.lll - JIT compile and execute
lllc -e code - Run code directly
lllc -c code - Compile code directly
lllc - Interactive mode
lllc --module file.lll - Build as shared library
lllc --x86 file.lll - Compile for 32-bit
lllc -v - Verbose mode (show LLVM IR)

## Available Types

int - 64-bit signed integer
int32 - 32-bit signed integer
int64 - 64-bit signed integer
float32 - 32-bit floating point
float64 - 64-bit floating point
uint8 - 8-bit unsigned integer
uint64 - 64-bit unsigned integer
string - Character string
boolean - True or false
void - No type

## Operators

Arithmetic: + - * / %
Comparison: == != < > <= >=
Logical: and or not
String: .. concatenation
Assignment: = += -= *= /=

## Known Issues

No garbage collector yet. No classes or objects. No switch statements. No exception handling. Limited standard library. No bitwise operators yet. No closures. No variadic functions except built-ins. No hash tables. No table methods. No table length operator.

## What Works

Variables and type inference. Functions with parameters and return types. If/elseif/else statements. While loops. For loops numeric. Repeat until loops. Structs. Enums. Basic operators. String concatenation. Tables basic arrays. Recursion. Early returns. Ternary expressions. Syscalls. Inline assembly. Custom builtins.

## What Does Not Work Yet

Full standard library. Garbage collection. Classes/objects. Modules system. Bitwise operators. Switch statements. Foreach loops. Exception handling. Default parameter values. Function overloading. Closures. Constants. Type aliases. Nullable types. Hash tables. Table methods. Nested tables.

## License

MIT License

## Author

schlonny

## Contributing

This project is in early development. Contributions welcome but expect breaking changes.
