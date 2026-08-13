# Examples

## Hello World

print("Hello, World!")

## Fibonacci

function fibonacci(n: int32): int64
    if n <= 1 then
        return n
    end
    return fibonacci(n - 1) + fibonacci(n - 2)
end

for i = 0, 10 do
    print("F(" .. i .. ") =", fibonacci(i))
end

## Structs

struct Point
    x: float64
    y: float64
end

local p: Point
p.x = 10.5
p.y = 20.3

print(p.x, p.y)

## Enums

enum Color
    RED = 1
    GREEN = 2
    BLUE = 4
end

local c: Color = Color.RED
print(c)

## Tables

local nums = {1, 2, 3, 4, 5}
print(nums[1])

## Control Flow

local score = 85

if score >= 80 then
    print("Good!")
else
    print("Needs work")
end

for i = 1, 5 do
    print(i)
end
