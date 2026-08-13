# Structs

Structs group related data together, similar to C structs.

## Declaration

struct Point
    x: float64
    y: float64
end

## Usage

local p: Point
p.x = 10.5
p.y = 20.3

print(p.x, p.y)

## Nested Structs

struct Person
    name: string
    position: Point
end

local person: Person
person.name = "Alice"
person.position.x = 10.0

## Why Use Structs?

- Group related data together
- Type safety
- Better code organization
- Memory efficiency (stored contiguously)
- Performance (no pointer indirection)

## Current Limitations

- No methods on structs yet
- No inheritance
- No constructors
