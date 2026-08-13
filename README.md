# LLL - Low Level Lua Compiler

LLL is a Lua-like language that compiles to C for native performance. Zero dependencies, zero bullshit.

## Status

EARLY DEVELOPMENT - The language is nearly not done. Many features are missing, buggy, or experimental. Do not use for production projects yet.

## Features

- Lua-like syntax with static typing
- Compiles to native C code
- Structs and enums support
- Plugin system (experimental)
- JIT compilation using TCC (experimental)
- Zero external dependencies

## Building

make
sudo make install

## Quick Start

Create hello.lll:

print("Hello, World!")

Compile (AOT - recommended):

lllc hello.lll
./hello

Or use JIT (experimental):

lllc --jit hello.lll

## Language Overview

-- Variables with type inference
local name = "LLL"
local version = 1.0

-- Explicit types
local count: int32 = 42

-- Functions
function greet(person: string): string
    return "Hello, " .. person .. "!"
end

-- Structs
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

## Command Line Options

| Command | Description | Status |
|---------|-------------|--------|
| lllc file.lll | AOT compile to executable | Stable |
| lllc -S file.lll | Generate C code only | Stable |
| lllc --cfile file.lll | Keep the generated C file | Stable |
| lllc --jit file.lll | JIT compile using TCC | Experimental |
| lllc -e code | Run code directly | Experimental |
| lllc -c code | Compile code directly | Stable |
| lllc | Interactive mode | Stable |
| lllc --module file.lll | Build as shared library | Stable |
| lllc --x86 file.lll | Compile for 32-bit | Experimental |

## Compilation Modes

### AOT (Ahead-of-Time) - Recommended

AOT converts LLL to C, then GCC compiles to native machine code. Most reliable mode with full optimization.

### JIT (Just-in-Time) - Experimental

JIT uses TinyCC (TCC) to compile in memory. Fast compilation but less runtime optimization. Not fully implemented.

## Project Structure

LLL/
  src/
    main.c
    lexer.c
    lexer.h
    parser.c
    parser.h
    parser_stmt.c
    parser_expr.c
    ast.h
    codegen.c
    codegen.h
    lll.h
    lll_plugin.c
    lll_plugin.h
    platform.c
    platform.h
    utils.c
    utils.h
    keywords/
  docs/
  build/
  CMakeLists.txt
  check.lll

## Available Types

| Type | Size | Description |
|------|------|-------------|
| int32 | 4 bytes | 32-bit signed integer |
| int64 | 8 bytes | 64-bit signed integer |
| float32 | 4 bytes | 32-bit floating point |
| float64 | 8 bytes | 64-bit floating point |
| uint8 | 1 byte | 8-bit unsigned integer |
| uint64 | 8 bytes | 64-bit unsigned integer |
| string | variable | Character string |
| boolean | 1 byte | True or false |
| void | 0 bytes | No type |

## Operators

### Arithmetic
+ Addition
- Subtraction
* Multiplication
/ Division
% Modulo

### Comparison
== Equal
!= Not equal
< Less than
> Greater than
<= Less or equal
>= Greater or equal

### Logical
and - AND
or - OR
not - NOT

### String
.. - Concatenation

### Assignment
= - Assign
+= - Add and assign
-= - Subtract and assign
*= - Multiply and assign
/= - Divide and assign

## Known Issues

- Some language features are incomplete
- Error messages may be confusing
- Plugin system is experimental
- JIT is experimental
- Type system is basic
- No garbage collector yet
- Limited standard library
- No classes or objects
- No modules system (partial)
- No bitwise operators yet
- No switch statements
- No exception handling

## What Works

- Variables and type inference
- Functions with parameters and return types
- If/elseif/else statements
- While loops
- For loops (numeric)
- Repeat until loops
- Structs
- Enums
- Basic operators
- String concatenation
- Tables (basic arrays)
- Recursion
- Early returns
- Nested if/else
- Ternary expressions

## What Does Not Work Yet

- Full standard library
- Garbage collection
- Classes/objects
- Modules system (partial)
- Bitwise operators
- Switch statements
- Foreach loops
- Exception handling
- Default parameter values
- Function overloading
- Closures
- Variadic functions (except built-ins)
- Constants
- Type aliases
- Nullable types
- Hash tables
- Table methods
- Table length operator
- Nested tables (partial)

## Plugin System - EXPERIMENTAL

Plugins are shared libraries (.lllplugin) that extend LLL.

Plugin locations:
./llladdons/
/usr/lib/llladdons/
/usr/local/lib/llladdons/

Plugin structure:
lll_plugin_init(PluginAPI *api, PluginInfo *info)

Example:
int lll_plugin_init(PluginAPI *api, PluginInfo *info) {
    info->name = "My Plugin";
    info->version = "1.0";
    return 0;
}

## License

MIT License - see LICENSE file for details.

## Author

schlonny

## Contributing

This project is in early development. Contributions are welcome but expect breaking changes.
