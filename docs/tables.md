# Tables

Tables are ordered collections of values. They use 1-based indexing.

## What are Tables?

Tables are similar to arrays. They can hold multiple values of different types.

## Declaration

local numbers = {1, 2, 3, 4, 5}
local mixed = {1, "two", 3.0, true}
local empty = {}

## Access

print(numbers[1])
print(numbers[3])
print(mixed[2])

## 1-Indexed (Important!)

LLL tables use 1-based indexing like Lua:
- First element is at index 1, NOT 0
- Last element is at index equal to table length
- Index 0 is invalid

local nums = {10, 20, 30}
print(nums[1])  -- 10 (first)
print(nums[2])  -- 20 (second)
print(nums[3])  -- 30 (third)

## Why 1-Indexed?

- More intuitive for humans
- Matches how people count naturally
- Consistent with Lua's design

## Modifying Tables

local t = {10, 20, 30}
t[1] = 100
t[4] = 40
print(t[1])  -- 100
print(t[4])  -- 40

## Current Limitations

- Only array-style tables (no hash tables)
- No table methods
- No table length operator yet
- No nested table support fully
