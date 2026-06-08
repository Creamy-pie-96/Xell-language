## ⬅️ What's Previous?

Move back to the **[Modules & Imports](./Modules & Imports.md)** to review the foundational concepts.

---

# Error Handling

Xell provides a robust mechanism for dealing with runtime errors using `try`, `catch`, `finally`, and `throw`. This allows you to handle exceptions gracefully without crashing your program.

## 🛡️ The Try-Catch-Finally Block

You can wrap code that might fail in a `try` block. If an error occurs, Xell will stop executing the `try` block and look for a matching `catch` block.

### Basic Syntax
```xell
try :
    # Code that might throw an error
    result = 10 / 0
;
catch err :
    print "An error occurred: {err}"
;
```

### Typed Catch Clauses
You can handle different types of errors differently by specifying the error type using the `is` keyword. You can even match multiple error types in a single clause using `or`.

```xell
try :
    # Some risky operation
    data = read_file("config.json")
    parse_json(data)
;
catch err is FileNotFound :
    print "Error: The configuration file was not found."
;
catch err is JsonParseError or TypeError :
    print "Error: The file contains invalid data: {err}"
;
catch err :
    print "An unexpected error occurred: {err}"
;
```

### The Finally Block
The `finally` block is optional and always executes, regardless of whether an error was thrown or caught. It is typically used for cleanup tasks, such as closing files or network connections.

```xell
try :
    file = open_file("data.txt")
    # process file...
;
catch err :
    print "Error processing file: {err}"
;
finally :
    close_file(file)
    print "File connection closed."
;
```

---

## 🚩 Throwing Errors

You can manually trigger an error using the `throw` keyword.

### Throwing a Value
You can throw a string, a number, or a specific error object.
```xell
fn check_age(age) :
    if age lt 18 :
        throw "Access Denied: You must be 18 or older."
    ;
;
```

### Re-throwing Errors
Inside a `catch` block, using `throw` without any value will re-throw the current error, passing it up to the next enclosing `try-catch` block.

```xell
try :
    # ...
;
catch err :
    print "Logging error locally..."
    throw # Re-throw the error
;
```

---

## 📖 What's Next?

Now that you have mastered this topic, move on to the **[Built-ins](./Built-ins.md)** guide to further your understanding of Xell.
