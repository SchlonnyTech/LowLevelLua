# Variables

## Type Inference

local x = 42
local name = "LLL"
local pi = 3.14159
local flag = true

## Explicit Types

local count: int32 = 0
local title: string = "Hello"

## Why Different Integer Types?

- int32: 4 bytes. Use for counters and indexes.
- int64: 8 bytes. Use for timestamps and large numbers.
- uint8: 1 byte. Use for small values.
- uint64: 8 bytes. Use when always positive.

## Available Types

| Type | Size | Description |
|------|------|-------------|
| int32 | 4 bytes | 32-bit signed integer |
| int64 | 8 bytes | 64-bit signed integer |
| float32 | 4 bytes | 32-bit float |
| float64 | 8 bytes | 64-bit float |
| uint8 | 1 byte | 8-bit unsigned |
| uint64 | 8 bytes | 64-bit unsigned |
| string | variable | String |
| boolean | 1 byte | True/false |
| void | 0 bytes | No type |
