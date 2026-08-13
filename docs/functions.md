# Functions

Functions in LLL are declared with the function keyword. They support type annotations for parameters and return values.

## Basic Function

function greet(person: string): string
    return "Hello, " .. person .. "!"
end

The person parameter is typed as string, and the function returns a string.

## Parameters

function add(a: int32, b: int32): int32
    return a + b
end

## Return Types

function get_name(): string
    return "LLL"
end

If no return type, the function returns void:

function print_hello()
    print("Hello")
end

## Local Functions

local function helper()
    print("Helper function")
end

## Recursion

function factorial(n: int32): int64
    if n <= 1 then
        return 1
    end
    return n * factorial(n - 1)
end

## Calling Functions

print(greet("World"))
local result = add(5, 3)

## Why Type Annotations?

- Catch errors at compile time
- Make code self-documenting
- Enable better compiler optimizations
- Help other developers understand your code

## Current Limitations

- No default parameter values
- No function overloading
- No closures yet
- No variadic functions (except built-ins)
