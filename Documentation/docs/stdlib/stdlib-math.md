# Standard Library – Math Functions

> **Previous:** [Process Management](stdlib-process.md)  
> **Next:** [String Functions](stdlib-string.md)

---

Xell provides a comprehensive set of mathematical functions for common operations. These are always available (Tier 1) and cover constants, rounding, absolute values, trigonometry, logarithms, random numbers, and more.

---

## Quick Reference

| Function | What it does | Example |
|----------|--------------|---------|
| `PI` | π ≈ 3.141592653589793 | `PI` → `3.141592653589793` |
| `E` | Euler's number ≈ 2.718281828459045 | `E` → `2.718281828459045` |
| `INF` | Positive infinity | `INF` |
| `floor(x)` | Rounds down | `floor(3.9)` → `3` |
| `ceil(x)` | Rounds up | `ceil(3.1)` → `4` |
| `round(x[, n])` | Rounds to nearest integer or n decimals | `round(3.5)` → `4` |
| `abs(x)` | Absolute value | `abs(-5)` → `5` |
| `mod(a, b)` | Modulo (a % b) | `mod(10, 3)` → `1` |
| `sqrt(x)` | Square root (complex for negative) | `sqrt(16)` → `4` |
| `pow(x, y)` | Exponentiation (x ** y) | `pow(2, 8)` → `256` |
| `sin(x)`, `cos(x)`, `tan(x)` | Trigonometric (radians) | `sin(PI/2)` → `1` |
| `cot(x)`, `sec(x)`, `csc(x)` | Reciprocal trig | `cot(1)` |
| `asin(x)`, `acos(x)`, `atan(x)`, `atan2(y,x)` | Inverse trig | `asin(1)` → `π/2` |
| `acot(x)`, `asec(x)`, `acsc(x)` | Inverse reciprocal trig | `acot(1)` → `π/4` |
| `sinh(x)`, `cosh(x)`, `tanh(x)` | Hyperbolic | `sinh(1)` |
| `coth(x)`, `sech(x)`, `csch(x)` | Reciprocal hyperbolic | `coth(1)` |
| `asinh(x)`, `acosh(x)`, `atanh(x)` | Inverse hyperbolic | `asinh(1)` |
| `acoth(x)`, `asech(x)`, `acsch(x)` | Inverse reciprocal hyperbolic | `acoth(2)` |
| `log(x)` | Natural logarithm | `log(E)` → `1` |
| `log10(x)` | Base‑10 logarithm | `log10(100)` → `2` |
| `log2(x)` | Base‑2 logarithm | `log2(8)` → `3` |
| `is_nan(x)` | Checks if NaN | `is_nan(0/0)` → `true` |
| `is_inf(x)` | Checks if infinite | `is_inf(INF)` → `true` |
| `hex(n)` | Integer → hex string | `hex(255)` → `"ff"` |
| `bin(n)` | Integer → binary string | `bin(10)` → `"1010"` |
| `factorial(n)` | n! (n ≤ 20) | `factorial(5)` → `120` |
| `gcd(a, b)` | Greatest common divisor | `gcd(12, 18)` → `6` |
| `lcm(a, b)` | Least common multiple | `lcm(12, 18)` → `36` |
| `clamp(x, lo, hi)` | Constrains value to [lo, hi] | `clamp(5, 0, 3)` → `3` |
| `random()` | Random float in [0, 1) | `random()` → `0.374...` |
| `random_int(a, b)` | Random integer in [a, b] | `random_int(1, 6)` → `4` |
| `random_choice(list)` | Picks random element | `random_choice(["a","b","c"])` |

---

## Constants

### `PI`

The mathematical constant π (3.141592653589793).

```xell
print(PI)   # 3.141592653589793
```

### `E`

Euler's number (2.718281828459045).

```xell
print(E)   # 2.718281828459045
```

### `INF`

Positive infinity.

```xell
print(INF)          # inf
print(INF > 1000)   # true
```

---

## Basic Arithmetic

### `floor(x)`

Rounds `x` down to the nearest integer. Returns an `int`.

```xell
print(floor(3.9))    # 3
print(floor(-3.1))   # -4
print(floor(42))     # 42 (no change for integers)
```

**Output:**
```
3
-4
42
```

### `ceil(x)`

Rounds `x` up to the nearest integer. Returns an `int`.

```xell
print(ceil(3.1))    # 4
print(ceil(-3.9))   # -3
print(ceil(42))     # 42
```

**Output:**
```
4
-3
42
```

### `round(x[, n])`

Rounds `x` to the nearest integer (ties to even). If `n` is provided, rounds to `n` decimal places.

```xell
print(round(3.5))     # 4
print(round(2.5))     # 2 (ties to even)
print(round(3.14159, 2))   # 3.14
```

**Output:**
```
4
2
3.14
```

### `abs(x)`

Returns the absolute value of a number. Works on `int`, `float`, and `complex`.

```xell
print(abs(-5))        # 5
print(abs(-3.14))     # 3.14
print(abs(2 + 3i))    # 3.605551275463989 (magnitude)
```

**Output:**
```
5
3.14
3.605551275463989
```

### `mod(a, b)`

Returns the modulus (remainder) of `a` divided by `b`. Works on `int` and `float`.

```xell
print(mod(10, 3))     # 1
print(mod(10.5, 3))   # 1.5
print(mod(-10, 3))    # -1 (sign follows the dividend)
```

**Throws:** `DivisionByZeroError` if `b == 0`.

### `sqrt(x)`

Returns the square root of `x`. If `x` is negative, returns a complex number.

```xell
print(sqrt(16))        # 4
print(sqrt(2))         # 1.4142135623730951
print(sqrt(-1))        # (0+1i)
```

**Output:**
```
4
1.4142135623730951
(0+1i)
```

### `pow(x, y)`

Returns `x` raised to the power of `y`. Works on `int`, `float`, and `complex`. For integer exponents, returns an `int` if possible.

```xell
print(pow(2, 8))       # 256
print(pow(2.5, 3))     # 15.625
print(pow(2 + 3i, 2))  # (-5+12i)
```

**Output:**
```
256
15.625
(-5+12i)
```

### `clamp(x, lo, hi)`

Constrains `x` to the range `[lo, hi]`. If `x < lo`, returns `lo`; if `x > hi`, returns `hi`; otherwise returns `x`.

```xell
print(clamp(5, 0, 3))    # 3
print(clamp(-2, 0, 3))   # 0
print(clamp(2, 0, 3))    # 2
```

**Output:**
```
3
0
2
```

### `factorial(n)`

Computes `n!` (factorial). `n` must be a non‑negative integer ≤ 20 (to avoid overflow).

```xell
print(factorial(5))      # 120
print(factorial(0))      # 1
```

**Throws:** `ValueError` if `n < 0` or `n > 20`.

### `gcd(a, b)`

Returns the greatest common divisor of two integers.

```xell
print(gcd(12, 18))       # 6
print(gcd(1071, 462))    # 21
```

**Output:**
```
6
21
```

### `lcm(a, b)`

Returns the least common multiple of two integers.

```xell
print(lcm(12, 18))       # 36
print(lcm(4, 6))         # 12
```

**Output:**
```
36
12
```

---

## Trigonometric Functions

### Basic Trigonometry (Radians)

| Function | Description |
|----------|-------------|
| `sin(x)` | Sine |
| `cos(x)` | Cosine |
| `tan(x)` | Tangent |
| `cot(x)` | Cotangent = `1/tan(x)` |
| `sec(x)` | Secant = `1/cos(x)` |
| `csc(x)` | Cosecant = `1/sin(x)` |

```xell
print(sin(PI / 2))      # 1.0
print(cos(0))           # 1.0
print(tan(PI / 4))      # 1.0
print(cot(1))           # 0.6420926159343306
```

**Throws:** `DivisionByZeroError` for `tan(PI/2)`, `sec(PI/2)`, `csc(0)`, etc.

### Inverse Trigonometry

| Function | Description | Domain |
|----------|-------------|--------|
| `asin(x)` | Arc sine | `[-1, 1]` |
| `acos(x)` | Arc cosine | `[-1, 1]` |
| `atan(x)` | Arc tangent | All real |
| `atan2(y, x)` | Two‑argument arc tangent | All real |
| `acot(x)` | Arc cotangent = `atan(1/x)` | `x ≠ 0` |
| `asec(x)` | Arc secant = `acos(1/x)` | `|x| ≥ 1` |
| `acsc(x)` | Arc cosecant = `asin(1/x)` | `|x| ≥ 1` |

```xell
print(asin(1))          # 1.5707963267948966 (π/2)
print(acos(0))          # 1.5707963267948966 (π/2)
print(atan(1))          # 0.7853981633974483 (π/4)
print(atan2(1, 1))      # 0.7853981633974483 (π/4)
```

### Hyperbolic Functions

| Function | Description |
|----------|-------------|
| `sinh(x)` | Hyperbolic sine |
| `cosh(x)` | Hyperbolic cosine |
| `tanh(x)` | Hyperbolic tangent |
| `coth(x)` | Hyperbolic cotangent = `1/tanh(x)` |
| `sech(x)` | Hyperbolic secant = `1/cosh(x)` |
| `csch(x)` | Hyperbolic cosecant = `1/sinh(x)` |

```xell
print(sinh(1))          # 1.1752011936438014
print(cosh(1))          # 1.5430806348152437
print(tanh(0))          # 0.0
```

### Inverse Hyperbolic Functions

| Function | Description | Domain |
|----------|-------------|--------|
| `asinh(x)` | Inverse hyperbolic sine | All real |
| `acosh(x)` | Inverse hyperbolic cosine | `[1, ∞)` |
| `atanh(x)` | Inverse hyperbolic tangent | `(-1, 1)` |
| `acoth(x)` | Inverse hyperbolic cotangent = `atanh(1/x)` | `|x| > 1` |
| `asech(x)` | Inverse hyperbolic secant = `acosh(1/x)` | `(0, 1]` |
| `acsch(x)` | Inverse hyperbolic cosecant = `asinh(1/x)` | `x ≠ 0` |

```xell
print(asinh(1))         # 0.881373587019543
print(acosh(2))         # 1.3169578969248166
print(atanh(0.5))       # 0.5493061443340549
```

---

## Logarithmic Functions

### `log(x)`

Natural logarithm (base `E`). Domain: `x > 0`.

```xell
print(log(E))           # 1.0
print(log(1))           # 0.0
```

**Throws:** `TypeError` if `x ≤ 0`.

### `log10(x)`

Base‑10 logarithm. Domain: `x > 0`.

```xell
print(log10(100))       # 2.0
print(log10(10))        # 1.0
```

### `log2(x)`

Base‑2 logarithm. Domain: `x > 0`.

```xell
print(log2(8))          # 3.0
print(log2(1024))       # 10.0
```

---

## Checks

### `is_nan(x)`

Returns `true` if `x` is NaN (Not a Number). Works on `float` only.

```xell
print(is_nan(0.0 / 0.0))   # true
print(is_nan(42))          # false
```

### `is_inf(x)`

Returns `true` if `x` is positive or negative infinity. Works on `float` only.

```xell
print(is_inf(INF))         # true
print(is_inf(-INF))        # true
print(is_inf(42))          # false
```

---

## Integer Conversions

### `hex(n)`

Converts an integer to a hexadecimal string (no prefix). Returns a lowercase hex string.

```xell
print(hex(255))        # "ff"
print(hex(0x1F))       # "1f"
print(hex(-42))        # "-2a"
```

### `bin(n)`

Converts an integer to a binary string (no prefix).

```xell
print(bin(10))         # "1010"
print(bin(255))        # "11111111"
print(bin(-5))         # "-101"
```

---

## Random Numbers

### `random()`

Returns a random `float` in the range `[0.0, 1.0)`.

```xell
print(random())   # 0.374...
```

### `random_int(a, b)`

Returns a random `int` in the range `[a, b]` (inclusive). The order of `a` and `b` does not matter.

```xell
print(random_int(1, 6))   # between 1 and 6 (inclusive)
print(random_int(-10, 10))
```

### `random_choice(list)`

Returns a random element from a list. Throws `IndexError` if the list is empty.

```xell
let choices = ["rock", "paper", "scissors"]
let result = random_choice(choices)
print(result)   # "paper", "rock", or "scissors"
```

---

## Complete Example: Statistical Simulation

```xell
# Simulate rolling two dice 1000 times and compute statistics
let rolls = []
for i in range(1000):
    let d1 = random_int(1, 6)
    let d2 = random_int(1, 6)
    push(rolls, d1 + d2)
;

let total = sum(rolls)
let avg = avg(rolls)
let max_val = max(rolls)
let min_val = min(rolls)

print("Total: " + str(total))
print("Average: " + str(avg))
print("Min: " + str(min_val))
print("Max: " + str(max_val))

# Check if we got any doubles (1+1, 2+2, etc.)
let doubles = filter(rolls, x => x == 2 or x == 4 or x == 6 or x == 8 or x == 10 or x == 12)
print("Doubles count: " + str(len(doubles)))
```

**Expected output (varies):**
```
Total: 7012
Average: 7.012
Min: 2
Max: 12
Doubles count: 176
```

---

## What NOT to Do

### 1. Using `factorial()` with n > 20

```xell
factorial(21)   # ❌ ValueError – overflow
```

**Why it fails:** `21!` is larger than `2^63-1`; Xell uses 64‑bit integers.

### 2. Using `round()` with Ties to Even (Banker's Rounding)

```xell
round(2.5)   # 2 (not 3)
```

This is intentional (ties to even), following IEEE 754 semantics.

### 3. Using `log()` with Negative Numbers

```xell
log(-1)   # ❌ TypeError – domain error
```

**Why it fails:** Logarithms are only defined for positive numbers.

### 4. Using `sqrt()` with Negative Numbers and Expecting a Real Number

```xell
sqrt(-1)   # (0+1i) (complex)
```

If you need a real number, use `abs` or check the sign first.

### 5. Using `random_int()` with `a == b`

```xell
random_int(5, 5)   # always returns 5
```

This is valid but pointless.

### 6. Using `mod()` with `b == 0`

```xell
mod(10, 0)   # ❌ DivisionByZeroError
```

### 7. Using `hex()` or `bin()` with Non‑Integers

```xell
hex(3.14)   # ❌ TypeError
```

### 8. Expecting `random_choice()` to Work with an Empty List

```xell
random_choice([])   # ❌ IndexError
```

---

## See Also

- [Operators](../core/operators.md) – arithmetic operators (`+`, `-`, `*`, `/`, `%`, `**`)
- [Built‑ins Reference](../core/builtins.md) – complete function list
- [Data Types](../core/data-types.md) – number types

---

**Previous:** [Process Management](stdlib-process.md)  
**Next:** [String Functions](stdlib-string.md)