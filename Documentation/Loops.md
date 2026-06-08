## ⬅️ What's Previous?

Move back to the **[Control Flow](./Control Flow.md)** to review the foundational concepts.

---

# Loops

Loops allow you to execute a block of code repeatedly. Xell provides several types of loops to handle different iteration scenarios.

## 🔄 For Loops

The `for` loop is used to iterate over a collection of items (like a list or map).

### Basic For Loop
```xell
ports = [80, 443, 8080]

for port in ports :
    print "Checking port {port}..."
;
```

### Multi-target Iteration
You can unpack multiple values from each element of the iterable.
```xell
pairs = [[1, "one"], [2, "two"]]

for num, word in pairs :
    print "{num} is {word}"
;
```

### Variadic (Rest) Iteration
If the elements in your collection have varying lengths, use the `...` (ellipsis) to capture the remaining values into a list.
```xell
data = [[1, 2], [3, 4, 5], [6]]

for first, ...rest in data :
    print "First: {first}, Others: {rest}"
;
```

---

## ⏳ While Loops

The `while` loop repeats a block of code as long as a specified condition remains true.

```xell
count = 0

while count lt 5 :
    print "Count is {count}"
    count = count + 1
;
```

---

## ♾️ Infinite Loops (`loop`)

The `loop` statement creates an infinite loop. It is typically used in combination with a `break` statement to exit when a certain condition is met.

```xell
loop :
    input = read_line()
    if input == "exit" :
        break
    ;
    print "You said: {input}"
;
```

---

## 🔁 Do-While Loops

The `do-while` loop ensures that the block of code is executed **at least once** before the condition is checked.

```xell
do :
    print "Enter a positive number: "
    num = read_number()
; while num le 0
```

---

## 🛠️ Loop Control

### Break
The `break` keyword immediately terminates the innermost loop.

```xell
for i in [1, 2, 3, 4, 5] :
    if i == 3 :
        break
    ;
    print i
;
# Output: 1, 2
```

### Continue
The `continue` keyword skips the rest of the current iteration and jumps to the next one.

```xell
for i in [1, 2, 3, 4, 5] :
    if i == 3 :
        continue
    ;
    print i
;
# Output: 1, 2, 4, 5
```

---

## 📖 What's Next?

Now that you have mastered this topic, move on to the **[Arrays & Collections](./Arrays & Collections.md)** guide to further your understanding of Xell.
