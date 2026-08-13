# Enums

Enums define named constants.

## Declaration

enum Color
    RED = 1
    GREEN = 2
    BLUE = 4
end

## Usage

local my_color: Color = Color.RED
print(Color.GREEN)

## Why Use Enums?

- Named constants instead of magic numbers
- Type safety
- Better code readability
- Compiler can catch invalid values
