## ⬅️ What's Previous?

Move back to the **[Lambdas](./Lambdas.md)** to review the foundational concepts.

---

# Control Flow

Control flow structures allow you to execute different blocks of code based on conditions or patterns.

## 🚦 If / Elif / Else

The `if` statement is used for basic conditional logic. In Xell, blocks are opened with `:` and closed with `;`.

### Basic If
```xell
if score ge 90 :
    print "Grade: A"
;
```

### If / Elif / Else
You can chain multiple conditions using `elif` and provide a fallback with `else`.

```xell
if score ge 90 :
    print "Grade: A"
;
elif score ge 75 :
    print "Grade: B"
;
elif score ge 60 :
    print "Grade: C"
;
else :
    print "Grade: F"
;
```

---

## 🧩 Pattern Matching with `incase`

Xell features a powerful pattern matching construct called `incase`. It is similar to `switch` in other languages but more expressive, allowing you to match values, types, or bind values to variables.

### Basic Syntax
```xell
incase subject :
    # Clause 1
    is value :
        # body
    ;
    # Clause 2
    else :
        # fallback body
    ;
;
```

### Matching Types of Clauses

#### 1. Value Matching (`is`)
Matches the subject against one or more specific values. You can use `or` to match multiple possibilities.

```xell
incase status :
    is "success" or "ok" :
        print "Everything is fine!"
    ;
    is "error" :
        print "Something went wrong."
    ;
;
```

#### 2. Type Matching (`belong`)
Matches if the subject is of a specific type (e.g., `string`, `number`, `list`, `none`).

```xell
incase input :
    belong number :
        print "You entered a number: {input}"
    ;
    belong string :
        print "You entered text: {input}"
    ;
    belong none :
        print "Input was empty."
    ;
;
```

#### 3. Value Binding (`bind`)
Captures the subject value into a new variable for use within the clause body.

```xell
incase response :
    bind data :
        print "Processing received data: {data}"
    ;
;
```

### Guard Clauses (`if`)
Every `incase` clause can have an optional `if` guard. The clause only executes if both the pattern matches **and** the guard expression is true.

```xell
incase age :
    is 18 if is_registered :
        print "You can vote!"
    ;
    is 18 :
        print "You are 18, but not registered to vote."
    ;
;
```

### The `else` Clause
The `else` clause acts as the default handler if no other clauses match.

```xell
incase color :
    is "red" :
        print "Stop!"
    ;
    else :
        print "Proceed with caution."
    ;
;
```

---

## 📖 What's Next?

Now that you have mastered this topic, move on to the **[Loops](./Loops.md)** guide to further your understanding of Xell.
