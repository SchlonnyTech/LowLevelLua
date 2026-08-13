# Control Flow

## If Statements

if score >= 90 then
    print("Excellent!")
elseif score >= 80 then
    print("Good!")
else
    print("Needs work")
end

## While Loops

while count > 0 do
    print(count)
    count = count - 1
end

## For Loops

for i = 1, 10 do
    print(i)
end

With step:

for i = 0, 10, 2 do
    print(i)
end

## Repeat Until

repeat
    num = num * 2
until num > 100

## Ternary

local status = if age >= 18 then "adult" else "minor"

## Break and Continue

for i = 1, 10 do
    if i == 5 then
        break
    end
    if i == 3 then
        continue
    end
end
