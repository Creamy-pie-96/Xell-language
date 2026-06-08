## ⬅️ What's Previous?

Move back to the **[Classes & Objects](./Classes & Objects.md)** to review the foundational concepts.

---

# Modules & Imports

Xell uses a modular system to organize code, promote reuse, and manage dependencies. You can group related functions, classes, and variables into **modules** and import them where needed using the `bring` keyword.

## 📦 Creating a Module

A module is defined using the `module` keyword. All code within the module block is encapsulated.

```xell
module MathUtils :
    # Dependencies for this module
    requires os, network
    
    # Private function (not exported)
    fn internal_calc(x) :
        give x * 2
    ;

    # Exported function
    export fn add(a, b) :
        give a + b
    ;
;
```

### Exporting Members
By default, members of a module are private. Use the `export` keyword to make a function, class, or variable available to other modules that import it.

```xell
export name = "XellCore"
export fn initialize() :
    print "System initialized"
;
```

### Module Dependencies (`requires`)
Modules can declare their own dependencies using the `requires` keyword inside the module body. This ensures that required modules are loaded before the current module is initialized.

```xell
module MyModule :
    requires json, filesystem
    # ...
;
```

---

## 📥 Importing with `bring`

To use members from another module, use the `bring` statement. Xell offers several flexible ways to import.

### 1. Importing Specific Items
You can import one or more specific members by name.
```xell
bring add, subtract
```

### 2. Importing Everything
Use the asterisk `*` to import all exported members of a module.
```xell
bring *
```

### 3. Module-Based Resolution (`of`)
If you want to specify exactly which module a member comes from, use the `of` keyword. This is useful for resolving naming conflicts.
```xell
bring add of math->basic
```

### 4. File-Based Resolution (`from`)
You can import members directly from a specific file path.
```xell
bring config from "settings/config.xel"
```

### 5. Directory-Based Resolution
You can specify a directory first, then bring items from it.
```xell
from "utils" bring helper, logger
```

### 6. Aliasing (`as`)
You can rename imported items to avoid conflicts or provide shorter names using the `as` keyword.
```xell
bring add, subtract as plus, minus
# Now you can use plus() instead of add()
```

### 7. Chaining Imports (`and`)
You can combine multiple import sources in a single statement using `and`.
```xell
bring add of math and logger from "utils/log.xel"
```

---

## 📖 What's Next?

Now that you have mastered this topic, move on to the **[Error Handling](./Error Handling.md)** guide to further your understanding of Xell.
