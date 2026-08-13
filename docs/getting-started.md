# Getting Started

## Installation

git clone https://github.com/yourrepo/lll
cd lll
make
sudo make install

Verify:

lllc --about

## Your First Program

Create hello.lll:

print("Hello, World!")

Compile:

lllc hello.lll

Run:

./hello

JIT mode:

lllc --jit hello.lll

## Command Line Options

| Command | Description | Status |
|---------|-------------|--------|
| lllc file.lll | AOT compile | Stable |
| lllc -S file.lll | Generate C only | Stable |
| lllc --cfile file.lll | Keep C file | Stable |
| lllc --jit file.lll | JIT compile | Experimental |
| lllc -e code | Run code | Experimental |
| lllc -c code | Compile code | Stable |
| lllc | Interactive | Stable |
| lllc --module file.lll | Shared library | Stable |
| lllc --x86 file.lll | 32-bit | Experimental |

## Basic Syntax

### Variables

local x = 42
local name = "LLL"

### Functions

function greet(person: string): string
    return "Hello, " .. person .. "!"
end

### Control Flow

if score >= 80 then
    print("Good!")
else
    print("Needs work")
end
