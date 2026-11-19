# Palan Language Reference

**Version:** v0.2.6

Palan is a compiled systems programming language designed as a simpler, safer, and more enjoyable alternative to C. It targets developers who want low-level control and direct access to C libraries, without the sharp edges of C syntax. Palan code compiles to native x86-64 binaries via AT&T assembly, with no runtime overhead.

---

## Hello World

```palan
cinclude <stdio.h>;
printf("Hello World!\n");
```

---

## Quick Look

```palan
cinclude <stdio.h>;

// Define a function with multiple named return values
func sumsOf(int64 a, int64 b, int64 c) -> int64 ab, int64 bc {
    a + b -> ab;
    b + c -> bc;
    return;
}

// Top-level statements are the program entry point
int64 x = 1, y = 2, z = 3;
(int64 ab, bc) = sumsOf(x, y, z);
printf("%ld %ld\n", ab, bc);   // 3 5
```

---

## Table of Contents

1. [Command-Line Usage](#1-command-line-usage)
2. [Lexical Rules](#2-lexical-rules)
3. [Type System](#3-type-system)
4. [Variables, Constants and Type Aliases](#4-variables-constants-and-type-aliases)
5. [Expressions](#5-expressions)
6. [Statements](#6-statements)
7. [Function Definitions](#7-function-definitions)
8. [Arrays](#8-arrays)
9. [Struct Types](#9-struct-types)
10. [Address-Of Operator](#10-address-of-operator)
11. [Modules and Program Structure](#11-modules-and-program-structure)
12. [C Library Integration](#12-c-library-integration)
13. [Raw Syscalls](#13-raw-syscalls)

---

## 1. Command-Line Usage

```
palan source.pa                  # compile, link and run
palan source.pa a -b             # run with the arguments "a" and "-b"
palan -o myapp source.pa         # compile & link -> ./myapp
palan --lib -o dir main.pa        # build the library main.pa is the main source of
palan --clean                    # clean intermediate files
```

The build manager (`palan`) orchestrates the full pipeline: parse → semantic analysis → code generation → assemble → link.
Options go before the source file; everything after it is passed to the program (see
[Program Structure](#program-structure) for `args`). Without `-o`, the executable is built among
the intermediate files under `~/.palan/work/`, not in the current directory.

The environment variable `PALAN_WORK` names another directory for the intermediate files, in place
of `~/.palan/work/`. `--clean` removes only what `palan` put there.

---

## 2. Lexical Rules

**Identifiers:** `[A-Za-z_][A-Za-z0-9_]*`

Names starting with `__` belong to the toolchain, which uses them in generated code and in a
library's interface. Do not declare one, nor name a source file so; this is not checked. A name
from a cincluded C header may start with `__` and is used as it is.

**Integer literals:**
- Signed: `[0-9]+` (e.g., `42`, `1000`)
- Unsigned: `[0-9]+u` (e.g., `42u`)

A literal must fit the type it adopts from its context (the declared variable, parameter, or the
other operand); otherwise it is a compile error — `int8 x = 300;` and `uint8 y = -1;` are both
rejected. A minus sign directly on an integer literal is part of the value, so `int8 x = -128;`
is accepted. A constant expression (see [Constant Declarations](#constant-declarations)) is
folded first, so its result is what must fit: `int8 x = 100 + 100;` is rejected.

**Character literals:** `'a'` is an integer literal whose value is the character's Unicode code
point (`'a'` is 97, `'é'` is 233, `'😀'` is 128512), written as is or as `\u{X}` (`'\u{20AC}'` is
`'€'`). The escapes `\n` `\t` `\r` `\0` `\\` `\'` are also allowed.

- An ASCII character literal follows the rules of any other integer literal (`int8 c = 'a';`).
- One of 128 or more takes only an integer type of 32 bits or more: `uint8 b = 'é';` and
  `buf[i] == 'é'` for a `[]uint8 buf` are compile errors, since no byte of UTF-8 text holds it.
- With no type to take (a C variadic argument, a comparison of two literals), it is an `int32`.
- A literal holds one code point. An empty one, one of several code points (`e` followed by a
  combining accent, a flag), or one that is not valid UTF-8 is a compile error; write a string
  literal instead.

**String literals:** `"..."` with the following escape sequences:

| Escape | Meaning |
|--------|---------|
| `\n`   | newline |
| `\t`   | tab |
| `\r`   | carriage return |
| `\0`   | null byte |
| `\\`   | backslash |
| `\"`   | double quote |
| `\xHH` | hex byte |
| `\u{X}` | the UTF-8 bytes of code point `X` (1 to 6 hex digits; `"\u{20AC}"` is `E2 82 AC`) |

A string literal holds bytes, not checked as UTF-8: `\xHH` may give any byte, and a byte written
in the source is taken as it is. `\u{X}` (in a character literal too) must name a code point up to
`10FFFF` that is not a surrogate (`D800` to `DFFF`).

**Comments:** `// line comment`

---

## 3. Type System

### Built-in Types

| Type    | Size    | Signedness |
|---------|---------|------------|
| `int8`  | 8-bit   | signed     |
| `int16` | 16-bit  | signed     |
| `int32` | 32-bit  | signed     |
| `int64` | 64-bit  | signed     |
| `uint8` | 8-bit   | unsigned   |
| `uint16`| 16-bit  | unsigned   |
| `uint32`| 32-bit  | unsigned   |
| `uint64`| 64-bit  | unsigned   |
| `flo32` | 32-bit  | float      |
| `flo64` | 64-bit  | float      |
| `bool`  | 8-bit   | holds 0 or 1 |

### bool

- `true` and `false` are the `bool` literals (reserved words), with the values 1 and 0. They are
  always `bool`, so `int32 x = true;` is a widening and `true + 1` is 2.
- An integer literal converts to `bool` implicitly only when it is `0` or `1`. Any other integer
  or float value needs `bool(x)`, which yields 1 for every nonzero value rather than truncating.
- `bool` widens implicitly to every integer and float type.
- An operator promotes a `bool` operand to `int32`, as C does, so `b + 1` is 2. Storing such a
  result back into a `bool` needs `bool(...)`.
- A `bool` can be an `if`/`while` condition. Comparisons and logical operators yield `bool`.
- A C `_Bool` parameter or field (e.g. ncurses' `keypad`) is a `bool`.

### enum

```palan
type Color enum {
    RED,          // 0
    GREEN = 5,
    BLUE          // 6
};

Color c = Color.GREEN;
int32 n = c;            // an enum widens implicitly
Color d = Color(n + 1); // an integer needs Name(x)
```

- An enumerator without `=` is the previous value + 1, starting at 0. `=` takes an integer
  constant expression (`= A + 1`).
- Enumerators are referenced as `Name.X`. A type alias of the enum works the same way
  (`type Shade = Color;` then `Shade.RED`).
- An enum's base type is the type C gives its enumerators: `int32` when every value fits, which
  covers almost every enum. Otherwise it is `uint32` (no negative value, all fit), else `int64` or
  `uint64`. An enum is passed to and from C as its base type.
- An enum is its own type, like `bool`. It widens implicitly to its base type and to anything its
  base type widens to. An integer, a `bool`, or a different enum becomes an enum only through
  `Name(x)`; an integer literal is no exception (`Color c = 5;` is an error).
- An operator computes an enum operand as its base type, so `Color.RED + 1` is an `int32`.
  Storing the result back into a `Color` needs `Color(...)`.

### Implicit Widening

Within the same signedness group, narrower types widen automatically to wider types:

- `int8` → `int16` → `int32` → `int64`
- `uint8` → `uint16` → `uint32` → `uint64`

### Explicit Cast

An explicit cast is written `expr as T`, where `T` is any type as a declaration writes it. A
type with a name — a primitive, an enum, or a type alias — can also be written `T(expr)`; the
two forms mean the same thing:

```palan
int64 x = 100;
printf("%d\n", x as int32);   // explicit narrowing cast
printf("%d\n", int32(x));     // the same cast
```

A cast can convert:

- Between primitive and enum types — narrowing, a signed↔unsigned change, float↔integer, or
  into `bool` (any nonzero value becomes 1) or an enum.
- From a pointer to another pointer type, with any pointee type and either permission —
  `@T as @!T` drops the read-only restriction, for a C function whose parameter is not
  `const` but which doesn't write through it. No bits change; reading or writing through the
  result as the new type is the programmer's responsibility, as in C.
- From a function pointer to another function pointer type. Writing `_` for a parameter or
  return type keeps the one the source has there, so `free as @func(Q)` and
  `memmove as @func([4]int8, _, _) -> _` change only what they name. This is how a C function
  takes or returns ownership (see [Giving Values Away](#giving-values-away--and-taking-them-over-)).
- Between a pointer and `int64`/`uint64`, in either direction. A narrower integer is rejected
  rather than dropping the upper half of the address.
- From a pointer or an array to a borrowed array (`@[n]T`/`@![n]T`, including `@[n]$[m]T`),
  which views the memory with sizes known at compile time — to pass it where a sized borrowed
  array is expected, copy it into an array with `->`, or index it in two dimensions. As with
  `p[i]`, nothing checks that the memory really holds that many elements.

```palan
cinclude <stdlib.h>;

@!uint8 pix = calloc(uint64(640 * 480), uint64(1));
@![480]$[640]uint8 img = pix as @![480]$[640]uint8;
255 -> img[10][20];
```

- From a pointer (`@!X`) to an owned struct or array (`T`, `[n]T`, `[+]T`), which views what it
  points to as that type's owner, as C hands such a thing back. It is copied like an owned
  variable when assigned or passed, and moved by `<<=`, `->>` or `>>`; a move leaves the pointer
  as it is. A `@X` pointer can be viewed this way only to copy from it. A `@!X` location is
  viewed this way to be given a value: `v ->> loc as T` (see
  [Ownership Across C Functions](#ownership-across-c-functions)).
- From an owned array to the same array of other sizes (`a as [m]T`), only where it is given
  away (see [Sizes Known Only at Run Time](#sizes-known-only-at-run-time)).

A value is never cast into an owned type — a cast never creates ownership — and a struct value
can't be cast into a pointer: take its address first (`@!s as @!uint8`).

`as` binds tighter than every binary operator and looser than a unary one: `p as @!int32 == NULL`
compares the cast pointer, `a + b as int64` casts only `b`, and `-x as uint32` casts `-x`.
Indexing binds tighter than `as`, so index a cast pointer as `(p as @!int32)[0]`. A cast result
can be indexed, have its fields accessed, and be written through like a variable of that type:
`5 -> (p as @!int32)[0]`, `(v as @P).a`.

### Usual Arithmetic Conversions (Binary Operators and Comparisons)

A binary arithmetic operator (`+ - * / % & | ^`) or a comparison (`< <= > >= == !=`) first
converts its two operands to one common type, then evaluates. The common type is chosen by
these rules, in order:

1. Either operand is a floating-point type → the float side wins (both float → the wider of
   the two; float paired with an integer → the float type — an integer is always widened to a
   float type it's compared or combined with, regardless of the integer's own width).
2. Both operands have the same signedness (both signed or both unsigned) → the wider
   (higher-ranked) type wins.
3. Signedness differs, and the signed operand is strictly wider than the unsigned operand →
   the signed type wins (e.g. `int64 & uint32` → `int64`).
4. Signedness differs, otherwise (including equal width) → the **unsigned** type wins (e.g.
   `int32 & uint32` → `uint32`).

Unlike C, there is **no integer promotion to a machine word**: `int8 + int8` stays `int8`,
preserving that width's own wraparound rather than promoting to a wider type first.

This rule governs binary arithmetic, bitwise operators (§5), and comparisons only — it is
**more permissive** than the rule for a binding site (a variable initializer, a plain or array
assignment, `return`, or a struct field assignment): all five of those always require an
explicit cast for a narrowing or a signedness change, with no exception for same-width
sign reinterpretation (see Explicit Cast above). A function call argument sits in between: it
accepts everything a binding site would, plus a same-width signedness reinterpretation (e.g. an
`int32` value passed where a `uint32` parameter is declared, or vice versa) — an argument only
needs to fit the callee's declared width, not match a variable's exact declared type. An integer
*literal* argument or initializer is the one further exception at any of these sites: it simply
adopts the destination's type instead of being checked for narrowing (this is what lets
`add(1, 2)` bind to `int32` parameters without a cast).

A pointer or struct operand has no common type with anything under this rule: using one with
`+ - * / % & | ^` is a compile error (Palan has no pointer arithmetic). A comparison of pointers
is valid and leaves both operands unconverted (`p == NULL`); a comparison with a struct operand is
a call of the type's comparison function (see [Operator Functions](#operator-functions)). A
comparison's result is always `bool` regardless of operand type.

### Variadic Argument Promotion

When passing to variadic C functions (e.g., `printf`), small integer types are promoted:
- `int8`, `int16` → `int32`
- `uint8`, `uint16` → `uint32`
- `bool` → `int32`

### Floating-Point Types

Palan supports 32-bit and 64-bit IEEE 754 floating-point types:

| Type    | Width  | C equivalent |
|---------|--------|--------------|
| `flo32` | 32-bit | `float`      |
| `flo64` | 64-bit | `double`     |

#### Declaration and Initialization

Float variables are declared like integer variables. A float literal has digits on both sides of the decimal point, an exponent, or both. A number followed directly by other letters (`1.5f`, `100px`) is an error.

```palan
flo64 pi = 3.14159;
flo32 half = 0.5;
flo64 big = 1e10;
flo64 small = 2.5e-3;
flo64 f;              // uninitialized
```

Float literals adopt the declared variable's type. A `flo32` variable initialized with `1.5` stores the value as a 32-bit float; a `flo64` variable stores it as a 64-bit double.

Integer literals can also initialize float variables:

```palan
flo64 n = 5;    // stored as 5.0
```

#### Arithmetic Operators

`+`, `-`, `*`, `/` work on `flo32` and `flo64` operands and produce a float result.

```palan
flo64 a = 3.0, b = 2.0;
flo64 sum = a + b;    // 5.0
flo64 diff = a - b;   // 1.0
flo64 prod = a * b;   // 6.0
flo64 quot = a / b;   // 1.5
```

The `%` (modulo) operator is **not** supported on float types; using it is a compile error.

#### Comparison Operators

All six comparison operators (`<`, `<=`, `>`, `>=`, `==`, `!=`) work on float operands and produce `bool` (1 if true, 0 if false), the same as integer comparisons.

```palan
flo64 x = 1.5;
if x > 1.0 { printf("big\n"); }
```

#### Unary Minus

`-expr` negates a float value.

```palan
flo64 v = 3.14;
-v -> v;   // v is now -3.14
```

#### Type Promotion

Float operand conversion for binary operators and comparisons follows the same Usual Arithmetic
Conversions rule as integer operands — see [Type System](#3-type-system): the wider float wins
between two floats, and a float always wins over a paired integer regardless of the integer's
width (`flo32 op flo64` → `flo64`; `int op flo32` → `flo32`; `int op flo64` → `flo64`).

#### Explicit Cast to Integer

Use `int64(x)` or `int32(x)` to convert a float to an integer (truncation toward zero):

```palan
cinclude <stdio.h>;
flo64 pi = 3.14159;
printf("%ld\n", int64(pi));   // prints 3
```

#### Float Parameters and Return Values

Palan functions take and return `flo32`/`flo64` values like any other type, and may mix them with
integer parameters and return values:

```palan
func scale(flo64 x, int32 n) -> flo64 {
    return x * n;
}
flo64 r = scale(1.5, 4);   // 6.0
```

#### Example — Newton's Method

```palan
cinclude <stdio.h>;

flo64 x = 2.0;
flo64 guess = 1.0;
flo64 eps = 0.000001;
flo64 diff = guess * guess - x;
if diff < 0.0 { -diff -> diff; }
while diff > eps {
    (guess + x / guess) / 2.0 -> guess;
    guess * guess - x -> diff;
    if diff < 0.0 { -diff -> diff; }
}
printf("sqrt(2) = %f\n", guess);
```

Expected output: `sqrt(2) = 1.414214`

---

## 4. Variables, Constants and Type Aliases

```palan
int64 x = 10;
int32 a = 5, b = 10;   // type inheritance: b is also int32
```

**Type inheritance:** In a comma-separated declaration, all variables after the first inherit the type of the first variable.

**Type is required:** `x = 10;` is a declaration without a type, reserved for future type inference,
and is currently a compile error. It is not an assignment — assignment is written `10 -> x;`.

**Stack storage:** A struct or array variable is allocated on the heap; with `$` in front of its
type (`$Point p;`, `$[4]int32 a;`) it is placed in the function's stack frame instead. See
[Stack Arrays](#stack-arrays-nt) and [Stack Structs](#stack-structs-t).

**Initial contents:** A new struct or array starts undefined, as memory from C's `malloc` does:
its primitive fields and elements, and its pointers (`@T`), hold whatever was there before, so
write them before reading them. Only what Palan itself releases is set up: an owned field or
element (`T`, `[n]U`) is allocated with it.

### Constant Declarations

Declare a compile-time constant with `const`:

```palan
const MaxLen = 256;

[MaxLen]uint8 buf;
type Msg { $[MaxLen]uint8 body; };
printf("%ld\n", MaxLen);   // 256
```

- The value is a literal or a constant expression: literals, consts and `sizeof(type)` combined
  with `+ - * / %`, `& | ^`, `<<< >>>`, unary `-` and `~` (`const W = N * 2 + 1;`). A cast, a variable, an
  enumerator or a call is not constant.
- A constant expression is folded wherever it is written, not only in a const: an array size
  (`[N + 1]int32 a;`, a struct field `[N * 2]$int32 v;`), an enumerator value, or an ordinary
  expression (`x * (N + 1)` folds `N + 1`).
- Integers are computed in `int64`, or in `uint64` once a `u` literal or `sizeof` is an operand. A
  value outside that range, a negative value in `uint64`, or a division by zero is a compile
  error. Division truncates toward zero, and `%` takes the sign of the dividend. A shift is
  computed at 64 bits: bits shifted out are not an error, and the count is taken modulo 64.
- A float operand makes the whole operation `flo64`; `%`, `&`, `|`, `^` and `~` on a float are errors.
- The result is untyped like a literal and must fit the type of the place it is used.
- A `cinclude`d macro constant inside a constant expression drops its C type and is computed like
  any other literal.
- Every reference to the constant is inlined with the literal value.
- A const may reference another const (`const B = A;`); this chains naturally through inlining.
- A const cannot share its name with another const, a type, or a variable (including a parameter)
  in the same or an enclosing scope. A const is visible from the top of its block, so a variable
  declared before it in the same block clashes too.
- Like the literal it names, a const takes its type from where it is used: `const N = 20;` can be
  passed to an `int32` parameter, used in `N + 2` for an `int16` variable, or assigned to `flo64`.

#### Restrictions

- Only a top-level const can size a borrowed array parameter (`@[N]T`). Its value may use the
  length of a top-level array declared before it (`[]Light lights = [...]; const LN =
  lights->len();`) when the array's declaration fixes its size, but not other variables: such a
  const is known only from its own statement on, so it cannot size a parameter or a top-level
  type.

### Type Aliases

Declare an alias for a type with `type`:

```palan
type MyInt = int64;

MyInt x = 42;

func addOne(MyInt n) -> MyInt result {
    n + 1 -> result;
}
```

- The target may be a primitive, struct, or array type (`type PT = Point;`, `type Row = [3]int32;`).
- The alias may be used wherever a type is written: variable declarations, function signatures,
  struct field types, and array element types (`[n]MyInt`).

---

## 5. Expressions

| Expression | Syntax | Example |
|-----------|--------|---------|
| Integer literal | `[0-9]+` | `42` |
| String literal | `"..."` | `"hello\n"` |
| Identifier | `name` | `x` |
| Addition | `expr + expr` | `a + b` |
| Subtraction | `expr - expr` | `a - b` |
| Multiplication | `expr * expr` | `a * b` |
| Division | `expr / expr` | `a / b` |
| Modulo | `expr % expr` | `a % b` |
| Unary minus | `-expr` | `-x` |
| Bitwise AND | `expr & expr` | `a & b` |
| Bitwise OR | `expr \| expr` | `a \| b` |
| Bitwise XOR | `expr ^ expr` | `a ^ b` |
| Bitwise NOT | `~expr` | `~a` |
| Left shift | `expr <<< expr` | `a <<< 3` |
| Logical right shift | `expr >>> expr` | `a >>> 3` |
| Append | `expr << expr` (a struct defines it, see [Operator Functions](#operator-functions)) | `s << "abc" << t` |
| Grouping | `(expr)` | `-(a + b)` |
| Comparison | `expr < expr`, `<=`, `>`, `>=`, `==`, `!=` | `x < 10` |
| Function call | `name(args)` | `add(3, 4)` |
| Explicit cast | `expr as type`, `type(expr)` | `p as @!int32`, `int32(x)`, `Color(n)` |
| Enumerator | `Name.X` | `Color.RED` |
| Type size | `sizeof(type)` | `sizeof(Vec3f)` |
| Logical AND | `expr && expr` | `a && b` |
| Logical OR | `expr \|\| expr` | `a \|\| b` |
| Logical NOT | `!expr` | `!x` |
| Assignment expression | `expr -> var` | `x + 1 -> x` |

Operator precedence (high to low): unary minus / `!` / `~`, `as`, `* / % & | ^`, `+ -`,
`<<< >>> <<`, `->` / `->>`, comparisons, `&&`, `||`. All binary operators are left-associative.

`->` sits below arithmetic, so `x + 1 -> x` stores the sum, and above comparisons, so a chained
call can be compared: `i < a -> len()` is `i < (a -> len())`. To store a comparison or a logical
result, parenthesize it: `(x < y) -> b`. Without the parentheses, `x < y -> b` puts an assignment
inside the comparison, which is a compile error.

Note the bitwise operators `&`/`|`/`^` share a precedence class with `*`/`/`/`%` — one level
**above** `+`/`-` — rather than C's much lower placement below comparisons. This is a deliberate
simplification, not an oversight, and it changes how mixed expressions parse relative to C:

- `a & b == c` parses as `(a & b) == c` — this actually *avoids* a well-known C pitfall, where
  the same expression parses as `a & (b == c)` because C's `&` sits below `==`.
- `a + b | c` parses as `a + (b | c)` — this is **different from C**, where `|`'s low precedence
  would instead parse it as `(a + b) | c`. Parenthesize explicitly when porting C expressions
  that mix `+`/`-` with `&`/`|`/`^`.

Bitwise operators require integer operands; using one with a float operand is a compile error.

The shift operators are `<<<` (left) and `>>>` (logical right); `>>` stays the mark that moves
ownership into a call. As in C, a shift sits between `+`/`-` and the comparisons, so
`a + 1 <<< 2` is `(a + 1) <<< 2`; since `|` binds tighter than a shift in Palan, `x <<< 3 | y`
is `x <<< (3 | y)`.

- The result has the left operand's type, and the count may be any integer type.
- `>>>` fills with zeros even for a signed operand: for `int32 a = -8;`, `a >>> 1` is
  `2147483644`.
- The count is taken modulo the left operand's width in bits, so `int32` shifted by `33` is
  shifted by `1`, as in Java and JavaScript.
- There is no arithmetic (sign-filling) right shift; divide by a power of two instead. A
  multiplication, division or `%` by a power-of-two constant compiles to shifts.

Comparison operators produce `bool` (1 if true, 0 if false). Both operands are converted to a
common type first — see [Usual Arithmetic Conversions](#3-type-system) in Type System.

Logical operators `&&` and `||` use **short-circuit evaluation**: the right operand is not
evaluated if the result is already determined by the left operand. Both operands must be
integer types (float operands are a compile error). The result is always `bool` (1 if true,
0 if false).

The assignment expression `expr -> var` evaluates `expr`, stores it in `var`, and the result is the stored value.

Assignments chain as a statement: `v -> a -> b;` is `v -> a; a -> b;`, and the same holds
for `->>` (`x ->> s.p ->> t.p;` moves ownership from `x` to `s.p`, then from `s.p` to `t.p`).
A target followed by another `->` is read back as the next value from the same location
(`v -> arr[f()] -> x` calls `f` once). Evaluation goes left to right: the value first, then
each target as it is stored, so in `g() -> arr[f()]` `g` is called before `f`, and in
`n -> i -> arr[i]` `arr[i]` uses the `i` just stored. A call in the target that changes the
value through a pointer it was given does not change what is stored: `q[0] -> q[bump(@!q)]`
stores `q[0]` as it was before `bump`. A `->>` gives its value away when the value is
evaluated, so such a call already sees the source empty. An assignment used inside any other
expression is not supported.
A call can follow `->` too; see [Chained Calls](#chained-calls).

`sizeof(type)` is the byte size of a type, as C's `sizeof` gives it, and is typed like an
unsigned integer literal (`uint64` unless the context is another unsigned type). Its operand is
a type, not an expression. A struct, including a `cinclude`d one, has its C layout size; an
incomplete struct such as `DIR` is an error. An array is the total size of its elements, and
every size must be a compile-time constant. Only a `$` element is stored in the array itself:
a struct or row element without `$` is a pointer, so `sizeof([3]Vec3f)` is `3 * 8` while
`sizeof([3]$Vec3f)` is `3 * sizeof(Vec3f)`.

```palan
jpeg_compress_struct cinfo;
jpeg_CreateCompress(@!cinfo, JPEG_LIB_VERSION, sizeof(jpeg_compress_struct));
```

---

## 6. Statements

```palan
int64 x = 10;              // variable declaration
x + 1 -> x;               // assignment statement
printf("%ld\n", x);        // expression statement (function call)
return;                    // return from function (no value)
return expr;               // return with single value
(int64 a, b) = foo();      // tapple declaration (receive multiple return values)
foo() -> (a, b);           // assign multiple return values to existing variables
import "lib.pa";           // import Palan source file (see §11)
if expr { ... }            // conditional (see §6)
if expr { ... } else { ... }  // conditional with else (see §6)
while expr { ... }         // loop (see §6)
for copy a, i in arr { ... }  // loop over an array's elements, or a struct's (see §6)
```

### If / If-Else Statements

An `if` statement conditionally executes a block. The condition expression must be an integer type; zero is false, non-zero is true.

```palan
if x < 0 {
    printf("negative\n");
}
```

An optional `else` branch executes when the condition is false:

```palan
if x < 0 {
    printf("negative\n");
} else {
    printf("non-negative\n");
}
```

`else if` chains are supported:

```palan
if x < 0 {
    printf("negative\n");
} else if x == 0 {
    printf("zero\n");
} else {
    printf("positive\n");
}
```

- The condition can be any expression that produces an integer value (comparison, variable, call, etc.).
- The `then` and `else` bodies are block statements and create their own scope.

### While Loops

`while cond { body }` repeats `body` as long as `cond` is non-zero.

```palan
cinclude <stdio.h>;

int64 i = 1;
while i <= 5 {
    printf("%ld\n", i);
    i + 1 -> i;
}
```

The condition is evaluated before each iteration. The loop exits when the condition is zero (false).

```palan
cinclude <stdio.h>;

func fizzbuzz(int64 n) {
    int64 i = 1;
    while i <= n {
        if i % 15 == 0 { printf("FizzBuzz\n"); }
        else if i % 3 == 0 { printf("Fizz\n"); }
        else if i % 5 == 0 { printf("Buzz\n"); }
        else { printf("%ld\n", i); }
        i + 1 -> i;
    }
}

fizzbuzz(20);
```

### For Loops

`for copy a in arr { body }` runs `body` once for each element of `arr`, in order, with `a` a
copy of the element. `for copy a, i in arr` also gives the element's index as `i`, an `int64`.

```palan
cinclude <stdio.h>;

[+]int64 v = [10, 20, 30];
int64 s = 0;
for copy a in v { s + a -> s; }
printf("%ld\n", s);                    // 60

for @!a in v { a * 2 -> a; }           // write the element itself
for copy a, i in v { printf("%ld:%ld ", i, a); }
printf("\n");                          // 0:20 1:40 2:60
```

- `arr` can be any array `len` gives the length of (see [Variable-Length
  Arrays](#variable-length-arrays-t)): a fixed-size or variable-length array, a borrowed one, a
  struct's array field or a row. It is evaluated once, before the first iteration; an array
  returned by a function call is freed after the loop.
- The loop variable has no type written, but one of `copy`, `@` or `@!` is required. `copy a` is
  a copy of the element (an owned struct or array is copied deeply and freed at the end of each
  iteration). `@a` and `@!a` are the element itself, read-only and writable: `a` stands for
  `arr[i]` wherever it is written (`0 -> a`, `a.x`, `a[j]`, `a.push(1)`), so it is given to a
  function as an element is (`f(@!a)`, `len(@a)`). `copy` is not a reserved word; it can still
  name a variable or field.
- `i` cannot be assigned to.
- A type can be written before `i`: `for @c, int32 x in row` gives `x` as an `int32`, so it is
  passed to an `int32` parameter without a cast. It must be an integer type, and for an array of
  constant size every index must fit in it. For an array whose length is known only at run time
  it is not checked: an index too large for the type wraps as `int32(i)` would, while `a` is
  still each element in order.
- The body cannot change `arr`'s length or replace it, directly or through what contains it:
  `push`, `pop`, `resize`, `->>` into or out of it, `->` into a variable-length one, and passing
  `@!arr` are compile errors. Writing elements, changing an inner array (`m[0].push(1)` in
  `for copy r in m`) and `@arr` are fine. So is changing an element at another constant index
  (`vv[1].push(1)` in `for copy a in vv[0]`); one at a variable index may be the same element and
  is rejected. A change made through another borrow of `arr` is not detected.

A struct can be iterated over when its type defines two functions, `iter` and `next`:

```palan
cinclude <stdio.h>;

type bag { [+]int32 v; };
type bagit { @bag b; int64 pos; };     // the iterator: where the loop is
func iter(@bag b) -> bagit { bagit r; b -> r.b; return r; }
func next(@!bagit it) -> bool ok, int32 c
{
    false -> ok;
    if it.pos >= len(@it.b.v) { return; }
    it.b.v[it.pos] -> c;
    true -> ok;
    it.pos + 1 -> it.pos;
}

bag g;
g.v.push(10); g.v.push(20);
for copy c, i in g { printf("%ld:%d ", i, c); }    // 0:10 1:20
```

- `for copy c in s` calls `iter(@s)` once to get an iterator, then calls `next` with it before
  each iteration, running the body while `ok` is true. When `ok` is false the element is not used.
- `iter` is `func iter(@T s) -> I` or `func iter(@!T s) -> I`, one of the two, where `I` is a
  struct type returned by value; with `@!T`, `s` must be writable. `next` is
  `func next(@!I it) -> bool ok, E`. Another shape, or a type without them, is a compile error
  naming the shapes expected. They are found also in a module imported under an alias, since
  `for` never writes their names.
- What `next` returns decides the loop variable: an element `E` by value allows `copy` only (a
  struct is received as returned, without a copy, and freed at the end of each iteration); `@E`
  allows `copy` and `@`; `@!E` allows all three. With `@c` and `@!c`, `c` stands for the element
  itself, as for an array.
- `i` counts the iterations from 0. A type may be written for it, which is not checked to fit,
  as for an array whose length is known only at run time.
- `s` is evaluated once; a struct returned by a call is freed after the loop. The body cannot
  change `s` itself: `->` or `->>` into or out of it, and passing `@!s` (also `s.f()` and
  `s << x`), are compile errors. Reading `s` and writing its fields are fine.

### break / continue

`break` exits the innermost `while` or `for` loop immediately. `continue` skips the rest of the loop body and goes on to the next iteration. Both are only valid inside a loop; using them outside a loop is a compile error.

```palan
cinclude <stdio.h>;

func print_primes(int64 limit) {
    int64 n = 2;
    while n <= limit {
        int64 i = 2;
        int64 is_prime = 1;
        while i * i <= n {
            if n % i == 0 {
                0 -> is_prime;
                break
            }
            i + 1 -> i;
        }
        if is_prime == 1 { printf("%ld\n", n); }
        n + 1 -> n;
    }
}

func print_nonmult3(int64 limit) {
    int64 i = 0;
    while i < limit {
        i + 1 -> i;
        if i % 3 == 0 { continue }
        printf("%ld\n", i);
    }
}

print_primes(20);
printf("---\n");
print_nonmult3(10);
```

### Block Statements

A block `{ ... }` creates a new scope. Variables and functions declared inside are not visible after the closing brace.

```palan
int64 x = 10;
{
    int64 y = 20;      // y visible only inside this block
    printf("%ld\n", y);
}
// y is no longer visible here
printf("%ld\n", x);   // x is still visible
```

Shadowing — declaring a variable with the same name as one in an outer scope, or a Palan function with the same name and parameter types as one in an outer scope — is a compile error (functions of one name with different parameter types are [overloads](#overloading)). A variable (including a parameter) and a const cannot share a name either way.

Palan function definitions inside a block are block-scoped and support forward references within the same block.

Types and consts declared inside a block follow the same rules (see [Declaration order](#declaration-order)).

`cinclude` inside a block makes the imported C functions available only within that block. A C function brought in again is the same function.

### Optional Semicolons

The semicolon at the end of the last statement in a block (or at top level) may be omitted. Statements ending with `}` (`if`, `while`, standalone block) never require a trailing semicolon. Other statements require a semicolon when followed by another statement.

```palan
int64 x = 5;
int64 y = x + 1     // semicolon optional here (last statement)
```

```palan
cinclude <stdio.h>;
int64 i = 0;
while i < 3 {
    i + 1 -> i;
    printf("%ld\n", i)   // semicolon optional at end of block body
}
```

---

## 7. Function Definitions

Functions are defined with the `func` keyword. Return values are declared after `->`.
The optional `export` keyword makes the function callable from other Palan files that import this file (see §11).
Parameters and return values may be float types as well as integer types (see [Floating-Point Types](#floating-point-types)).

### No Return Value

```palan
cinclude <stdio.h>;

func greet() {
    printf("hello\n");
}
```

### Single Return Value (unnamed)

```palan
cinclude <stdio.h>;

func add(int32 a, int32 b) -> int32 {
    return a + b;
}

printf("%d\n", add(3, 4));
```

### Single Named Return Value

Named return variables are pre-declared and can be assigned via `->`. An implicit `return` at the end of the function is valid.

```palan
func passthrough(int32 x) -> int32 y {
    x -> y;
    return;
}
```

### Multiple Named Return Values

```palan
func sumsOf(int64 a, int64 b, int64 c) -> int64 ab, int64 bc {
    a + b -> ab;
    b + c -> bc;
    return;
}
```

A named return starts at `0` unless it has an initializer. Initializers are evaluated in order at function entry and can use the parameters:

```palan
func range(int32 from) -> int32 lo = from, int32 hi = from + 10 { }
```

A struct-type named return is allocated at function entry and can't have an initializer. Declaring it again in the body is an error.

### Default Arguments

A parameter may have a default value, used when a call leaves the argument out. Parameters
with defaults come last:

```palan
cinclude <stdio.h>;

type Align enum { Left, Right };
const WIDTH = 8;

func pad(int32 n, int32 width = WIDTH, Align a = Align.Right, @[]uint8 mark = "|") {
    int32 w = width;
    if a == Align.Left { -width -> w; }
    printf("%s%*d%s\n", mark, w, n, mark);
}

pad(42);                   // pad(42, 8, Align.Right, "|")
pad(42, 4);                // pad(42, 4, Align.Right, "|")
42 -> pad(3, Align.Left);  // chained and method-form calls use defaults too
```

- A default is a constant: a number, `true` or `false`, an enum value, a `const` (or an
  expression of them, such as `WIDTH * 2`), or a string literal for a parameter taking one. A
  variable, a function call, or a value for a struct parameter is a compile error.
- The default is filled in where the function is called. When a module or library changes a
  default, the programs calling the function are compiled again with the new value; the
  function's own code does not change.
- Leaving out an argument that has no default is a compile error.
- An operator function (§7 [Operator Functions](#operator-functions)) cannot have defaults.
- A function pointer type has no defaults: a call through a pointer gives every argument.
- With [overloading](#overloading), see there for how a call that could use a default is chosen.

### Variadic Arguments

`[...]T` as the last parameter takes any number of arguments of type `T`, none included. In the
function the parameter is an array whose length is the number of arguments:

```palan
cinclude <stdio.h>;

func sum(int32 base, [...]int64 xs) -> int64 t {
    base -> t;
    for copy x in xs { t + x -> t; }
}

func say(@[]uint8 sep, [...]@[]uint8 words) {
    for copy w, i in words {
        if i > 0 { printf("%s", sep); }
        printf("%s", w);
    }
    printf(" (%ld)\n", len(@words));
}

printf("%ld\n", sum(1, 2, 3));   // 6
printf("%ld\n", sum(1));         // 1
say(", ", "a", "b", "c");         // a, b, c (3)
say(", ");                        //  (0)
```

- Each argument is given as it would be to a parameter of type `T`: converted the same way, a
  struct `T` copied (or taken over from a call returning it), a borrow written `@x`. `T` may be a
  primitive, an enum, a struct, a borrow (`@T`, `@!T`, `@[]uint8`), but not an array by value
  (`[n]T`, `[]T`, `[+]T`) or `$T`.
- In the function the parameter works as a local array sized at run time
  (see [Sizes Known Only at Run Time](#sizes-known-only-at-run-time)): `len(@xs)`, `xs[i]`,
  `for`, and `->` copying it to or from another array. Its elements may be written. The array
  itself belongs to the call, so it cannot be moved out (`->>`, `<<=`) or given away.
- Struct elements are the function's: those still there when it returns are freed, and one can
  be taken out with `xs[i] ->> v`, which leaves NULL in its place.
- A function cannot have both [default arguments](#default-arguments) and a variadic parameter.
  An operator function and a `syscall` cannot be variadic.
- A function pointer type can end with `[...]T` (`@func(int32, [...]int64) -> int64`) and is
  called the same way. It is a different type from one taking `@[]T`, and a variadic function
  cannot be given to a C function as a callback.
- `...a` passes the array `a` as all of the arguments (see below).

`...a` passes an existing array as all of the arguments, as a `[n]T` parameter takes an array:

```palan
func avg([...]int64 xs) -> int64 { return sum(0, ...xs) / len(@xs); }

[3]int64 a = [1, 2, 3];
printf("%ld\n", sum(1, ...a));     // 7: the callee gets a copy
printf("%ld\n", avg(...a));        // 2
printf("%ld\n", sum(0, ...a>>));   // 6: a itself is passed, and a is now NULL
```

- It is the last argument and the only one for the variadic parameter: `sum(1, 2, ...a)` is an
  error. It is written only to a Palan variadic parameter, not to C's `...`.
- `...a` passes a copy: what the function writes into its elements does not reach `a`.
  `...a>>` passes `a` itself without a copy, and `a` becomes NULL (see
  [Giving Values Away](#giving-values-away--and-taking-them-over-)). A call returning an array is
  passed without a copy too (`...mk()`). Either way the struct elements are the function's.
- `a` is any array with a length: a `[n]T` (also sized at run time), a `[+]T`, a row, a field,
  a borrowed `@[n]T` or `@[+]T`, or the function's own variadic parameter. A `[]T` keeps no
  length and cannot be spread.
- The elements must be exactly `T`, as for [copying](#copying--); they are not converted:
  `[3]int32` cannot be spread into `[...]int64`, nor `[n]$T` into `[...]T`.

### Method-Form Calls

A function whose first parameter is a writable borrow (`@!T`, `@![n]T`, or a C function's
non-const pointer) can also be called as `x.f(args)`, which is the same as `f(@!x, args)`:

```palan
type Counter { int32 n; };
func add(@!Counter c, int32 d) { c.n + d -> c.n; }

Counter c;
c.add(1);               // add(@!c, 1)

jpeg_compress_struct cinfo;  // cinclude <jpeglib.h>
cinfo.jpeg_set_defaults();  // jpeg_set_defaults(@!cinfo)
```

- `x` must be something `@!` can be applied to: a variable, a struct field, or an array element.
- If `x` is already a borrow (a `@!T` variable, parameter or field), it is passed as it is, so a
  function taking `@!Counter c` can write `c.add(1)`. A read-only `@T` borrow is a compile error.
- A function whose first parameter is not `@!` (a value, `@T`, or no parameters) cannot be called
  this way.
- If `x` is both a variable and a module alias (§11), the variable is used.
- A method-form call can follow `->`; see [Chained Calls](#chained-calls).

### Chained Calls

`v -> f(args)` calls `f` with `v` in front of the arguments, `f(v, args)`, and can be continued
like an assignment chain:

```palan
x -> add(4) -> y;                              // add(x, 4) -> y
reflect(d, @n) -> normalized() -> dir;         // normalized(@vec3 v)
divmod(17, 5) -> add3(100);                    // add3(q, r, 100)
```

- `v` is any value: a variable, a field, a call, an arithmetic expression. A call returning
  several values gives all of them, in order.
- The arguments are then checked as in a plain call: the count must match, and each value must
  fit its parameter.
- For a read-only borrow parameter (`@T`, `@[n]T`, or a C function's const pointer), `@` may be
  omitted: `v -> len()` is `len(@v)`. `@!` must be written: `@!v -> scale(2)`.
- As a statement, `x -> v -> f()` stores `x` in `v`, then calls `f(v)`. A call is evaluated once.
- A method-form call takes the chained values after `x`: `v -> x.f(args)` is `f(@!x, v, args)`,
  and `divmod(7, 2) -> acc.add3(9)` is `add3(@!acc, q, r, 9)`. `v` is evaluated before `x`, so in
  `next(@!i) -> a[i].f()` `a[i]` uses the `i` `next` changed, and in `n -> a[bump(@!n)].f()` the
  `n` passed is the one before `bump`.
- `v ->> f()` gives `v` away to `f`, as `f(v>>)` does (see
  [Giving Values Away](#giving-values-away--and-taking-them-over-)).
- Not supported in this version: receiving with `(a, b) = ...` the return
  values of a chain whose value is evaluated first: a call given several chained values, or a
  method-form call where the value or `x` has a call (`(a, b) = g() -> acc.two()`).

### Receiving Multiple Return Values

A **tapple declaration** `(type name, ...) = call(...)` receives multiple return values into newly declared variables in one statement:

```palan
(int64 ab, int64 bc) = sumsOf(1, 2, 3);
printf("%ld %ld\n", ab, bc);
```

**Type inheritance** applies here as well — variables after the first can omit the type:

```palan
(int64 ab, bc) = sumsOf(1, 2, 3);   // bc is also int64
```

Each variable is initialized from its return value with the same type rules as a single declaration `int64 ab = ...;` (widening is implicit, narrowing is an error). A returned struct is owned by the variable that receives it; a `$T` variable receives it into the stack frame as `$T p = f();` does (see [Stack Structs](#stack-structs-t)).

To store the values into existing variables, array elements or fields instead, assign the call to a parenthesized target list:

```palan
sumsOf(1, 2, 3) -> (ab, arr[1]);
```

All return values are received first, then assigned to the targets from left to right, with the same type rules as a single `->` (a struct is copied into its target). So in `f() -> (i, arr[i])`, `arr[i]` uses the newly assigned `i`.

### Function Pointers

`@func(T1, T2) -> R` is a pointer to a function taking `T1`, `T2` and returning `R`. Without
`-> R` the function returns nothing. A function's name is a value of its function pointer type,
and a function pointer is called like a function:

```palan
cinclude <stdio.h>;

func twice(int32 x) -> int32 { return x * 2; }
func plus1(int32 x) -> int32 { return x + 1; }
func apply(@func(int32) -> int32 f, int32 x) -> int32 { return f(x); }

@func(int32) -> int32 g = twice;
printf("%d\n", g(5));              // 10
plus1 -> g;
printf("%d\n", g(5));              // 6
printf("%d\n", apply(twice, 21));  // 42

type Ops { @func(int32) -> int32 op; };
Ops o;
twice -> o.op;
printf("%d\n", o.op(4));           // 8
```

- A function pointer can be a variable, a parameter, a return type, a struct field, or an array
  element (`[4]@func(int32) -> int32`). Parameter names are not written.
- It is read-only like `@T`: code is never written through it, so `@!func(...)` is an error.
  The variable itself can be given another function with `->`.
- A parameter or return type is a primitive, a borrowed pointer (`@T`/`@!T`, including another
  function pointer), or an owned struct or array (`T`, `[n]T` with a constant `n`, `[+]T`), which
  is passed and returned as a function passes it: the called function owns an owned argument and
  frees it, and the caller owns an owned return value. A `$T` or `[]T` is not allowed. The last
  parameter can be [variadic](#variadic-arguments) (`[...]T`).
- A function becomes a function pointer by writing its name where a value is expected. This works
  for a Palan function returning nothing or one value (a single named return counts as one), an
  imported function, and a C function that is not variadic. A function with several return
  values, a variadic C function, a C function taking or returning a struct by value, and a
  `syscall` cannot be used this way. A variable of the same
  name is used in preference to the function.
- A function fits a function pointer type when the parameter count is the same, each parameter
  and the return type are the same type (no widening: `int32` does not fit `int64`), and pointer
  permissions flow the right way: a function taking `@!T` does not fit a pointer that passes
  `@T`, while a function taking `@T` fits one that passes `@!T`. Ownership must match too: a
  function owning a parameter (`T`) does not fit a pointer that lends it (`@!T`), nor the
  reverse. A `@void` parameter or return matches any pointer, as it does elsewhere.
- A function pointer is a pointer otherwise: it can be `NULL` and compared with `==`/`!=`, a
  `@void` converts to it, and `as` converts it to `@void` or `int64`/`uint64` and back. Calling a
  `NULL` function pointer is not checked.
- `f(x)` when both a function pointer `f` and a function `f` are visible is an error, and so is
  `s.f(x)` when `s` has a function pointer field `f` and a function `f` could take `s` as a method
  (its first parameter is `@!`).
- A parenthesized expression is called through: `(tbl[0])(3)`, `(pick(true))(3)`,
  `(f as @func(@!int8))(p)`. Without the parentheses (`tbl[0](3)`) it is not supported.

### Overloading

Functions may share a name when their parameter types differ. A call takes the one its
arguments fit:

```palan
cinclude <stdio.h>;

func show(int32 x) { printf("int %d\n", x); }
func show(flo64 x) { printf("float %.1f\n", x); }

int32 n = 3;
show(n);            // int 3
show(2.5);          // float 2.5
```

- Two functions with the same parameter types are a compile error, even if they return
  different types.
- C functions and imported functions of the name are candidates too: after
  `cinclude <stdlib.h>;`, `func abs(flo64 x) -> flo64` adds a float version of C's `abs(int)`.
- If one function fits the arguments, it is called. If several fit, the one every argument
  matches exactly (same type, and for a pointer the same `@`/`@!`, down to the elements of a
  borrowed array) is called. Failing that, the one that differs only by taking a `@!` argument as
  `@`, at its outermost level, is called: given `@!s` of a `[2]@!P s`, `f(@[2]@!P)` is chosen
  over `f(@[2]@P)`. Otherwise the call is ambiguous and a compile error. With `f(int32)` and `f(int64)`, an `int16` argument is
  ambiguous: write `f(int32(x))`.
- A literal matches exactly only its default type: `int64` for an integer, `uint64` with `u`,
  `flo64` for a decimal. With `f(int32)` and `f(int64)`, `f(1)` calls `f(int64)`.
- An arithmetic expression with a literal in it is taken as a literal is, unless an operand is
  known to be a struct, which makes the expression an [operator function](#operator-functions)
  call. Known to be a struct are a variable, a field reached from one, and a call whose functions
  all return one struct type or whose arguments, all of these kinds, choose a function returning
  one. Given `h(int32) -> vec2` and `h(flo64) -> flo64`, `g(h(n) * 2.0)` with an `int32 n` passes
  a `vec2`, while `g(h(2.5) * 2.0)` is taken as a number: a literal argument is typed only once
  `g` is chosen.
- A method-form call `x.f()` considers only the functions whose first parameter is `@!`. In a
  chained call `v -> f()`, `v` fits a `@T` parameter as `@v`.
- A function name used as a value takes the function of the pointer type it is given to:
  `@func(int32) -> int32 g = show;` is an error here (no `show` returns `int32`), and one
  written where no function pointer type is expected is a compile error.
- Receiving several return values (`(a, b) = f(x)`) needs the functions returning that many to
  return the same types.
- If one of several functions fitting the arguments fits only by using its
  [defaults](#default-arguments) or its [variadic parameter](#variadic-arguments), the call is
  ambiguous: with `f(int32)` and
  `f(int32, int32 = 0)`, `f(1)` is a compile error, and `f(1, 0)` calls the second. A function
  using defaults or a variadic parameter is called when it is the only one that fits.
- A literal [converts](#conversion-from-a-literal) to a struct parameter only when no function
  fits without the conversion, so `f(@[]uint8)` is chosen over `f(@name)` for `f("Ann")`, and
  `f(int64)` over `f(big)` for `f(12)`.
  Among those needing it, the rules above apply, a converted argument counting as exact.

### Operator Functions

A function named by an operator defines binary `+`, `-`, `*`, `/`, `%` or unary `-` where an
operand is a struct (a struct variable, field, borrow or returned struct):

```palan
cinclude <stdio.h>;

type vec3 { flo32 x; flo32 y; flo32 z; };

func +(@vec3 a, @vec3 b) -> vec3 { return [a.x + b.x, a.y + b.y, a.z + b.z]; }
func *(@vec3 v, flo32 f) -> vec3 { return [v.x * f, v.y * f, v.z * f]; }
func *(flo32 f, @vec3 v) -> vec3 { return [v.x * f, v.y * f, v.z * f]; }
func -(vec3 v) -> vec3 {
    -v.x -> v.x; -v.y -> v.y; -v.z -> v.z;
    return v;
}

vec3 a = [1, 2, 3];
vec3 b = -(a + a * 2.0);           // -3 -6 -9
vec3 c = 2.0 * b;                  // -6 -12 -18
printf("%g %g %g\n", c.x, c.y, c.z);
```

- `a + b` calls the `+` functions as `+(a, b)` would, chosen as an [overloaded](#overloading)
  call among those taking two parameters; `-a` among the `-` functions taking one. With no
  function for the operator and the operands' types, it is a compile error.
- An operand is passed as a value chained into a call (`a -> f()`) is: a `@T` parameter borrows
  it, a `T` parameter receives a copy, and a struct an operator or a call returns is handed over
  without one. When both a `@T` and a `T` function fit, the operator is ambiguous.
- `+`, `*`, `/` and `%` take two parameters, `-` one or two. At least one parameter must be a
  struct (`T` or `@T`), and none can be `@!`.
- An operator over numbers only is the built-in one and cannot be defined.
- A struct an operator returns can be borrowed as a call argument: `f(@(a + b))`.
- `func ->` is not an operator function but part of a type: see
  [A Type's Own Copy](#a-types-own-copy-func--).

#### Comparison Functions

A type defines `==` and `<`; `!=`, `>`, `<=` and `>=` are derived from them and cannot be
defined:

```palan
cinclude <stdio.h>;

type ver { int32 major; int32 minor; };

func ==(@ver a, @ver b) -> bool { return a.major == b.major && a.minor == b.minor; }
func <(@ver a, @ver b) -> bool {
    if (a.major != b.major) { return a.major < b.major; }
    return a.minor < b.minor;
}

ver a = [1, 2];
ver b = [1, 3];
printf("%d %d %d\n", a == b, a != b, a >= b);   // 0 1 0
```

- `a != b` is `!(a == b)`, `a > b` is `b < a`, `a <= b` is `!(b < a)` and `a >= b` is
  `!(a < b)`. `a` is still evaluated before `b`. A type without an order defines only `==`.
- A comparison function takes two parameters, at least one a struct (`T` or `@T`), none `@!`, and
  returns `bool`. The two types may differ: `a == b` looks for `==(A, B)` only, never `==(B, A)`,
  and `5 > s` looks for `<(S, int32)`.
- A comparison with a struct operand — a struct variable, field, borrow or returned struct, of a
  Palan type or a C one — calls these functions; with no function for the operands' types it is
  a compile error. Two structs are never compared by address.
- A comparison with `NULL` stays a pointer comparison: `p == NULL` for a borrow `@T p`. Whether
  two borrows point to the same value is written `int64(a) == int64(b)`.

#### Append

A type defines `<<` to append to a value of it in place:

```palan
cinclude <stdio.h>;

type tally { int64 sum; int32 count; };

func <<(@!tally t, int64 x) -> @!tally {
    t.sum + x -> t.sum;
    t.count + 1 -> t.count;
    return t;
}

tally t;
t << 3 << 4 + 5;
printf("%ld %d\n", t.sum, t.count);   // 12 2
```

- The function is `<<(@!T s, X x) -> @!T`: two parameters, the first a `@!` borrow of a struct, the
  second anything but `@!`, and it returns `@!T` of the same `T`. By contract it returns `s`
  itself, so that `s << a << b` appends both to `s`; the compiler cannot check that.
- The left operand is what a method-form call's receiver may be (`s.f()`, see
  [Method-Form Calls](#method-form-calls)), passed as `@!s` without writing `@!`: a variable, a field or an
  element, or a `@!T` borrow; also a call returning `@!T`, which is how `s << a << b` continues.
  A `@T` borrow is an error, and so is a struct a call or an operator returns: what is appended to
  a temporary would be lost.
- The right operand is passed as an operand of `+` is, and chooses among overloads the same way.
- `<<` binds as the shifts do: `s << a + b` appends `a + b`, and `s << x -> f()` is `f(s << x)`.
- `[+]T` has no `<<`; it uses `push`. For a `[+]uint8`, `v << "abc"` would not say whether it
  adds the bytes or one element.

---

## 8. Arrays

A 1D array is declared with `[size-expr]type name`. The element count is given as an integer expression before the type.

```palan
[64]uint8 buf;
```

### Heap Allocation and Automatic Cleanup

Array variables are heap-allocated (via `malloc`) when the declaration is reached. At the end of the enclosing scope, the array is automatically freed (via `free`). No manual memory management is required.

```palan
cinclude <stdio.h>;

{
    [64]uint8 buf;            // malloc(64) called here
    sprintf(buf, "hello\n");
    printf("%s", buf);
}                             // free(buf) called here automatically
```

### Sizes Known Only at Run Time

An array variable whose size is known only at run time (`[n]int32 a;` for a variable `n`) keeps
that size from its declaration, and the rows of `[3][m]int32 mm;` share the one `mm` keeps. An
array does not carry its size with it:

```palan
int64 n = 2;
int64 m = 4;
[n]int32 a;
[m]int32 b;
a ->> b;                    // b takes a's size: len(@b) is 2, and a is now NULL
[m]int32 c;
b -> c;                     // copies 2 elements, the shorter of the two
[3][m]int32 mm;
[n]int32 d;
d as [m]int32 ->> mm[1];    // a row's size cannot change: the cast says d has it
```

- A variable given an array (`->>`, `<<=`) takes over its size: `[m]int32 e <<= a;` has `a`'s.
- A row, a field, a parameter, a return value and a variable of constant size keep theirs. An
  array is given to one only if both sizes are known at compile time and are equal, or through
  a cast to the receiver's size, which is not checked.
- `a as [m]T` gives the array `a` owns other sizes, with the same element type and layout. It is
  written only where the array is given away (`->>`, `<<=`, `>>`, `return`), and `a` becomes
  NULL as usual.
- `->` copies each level as far as the shorter of the two goes; the receiving array keeps its
  size. Sizes known at compile time on both sides must still be equal.
- `len` and `for` read the size kept, as for a row of the variable (`len(@m[0])`,
  `for copy x in m[0]`).
- A `[]T` parameter keeps no size, so it cannot be copied from into an array.
- A parameter's or return value's sizes are known at compile time.

### Stack Arrays (`$[n]T`)

`$` in front of an array type places the array in the function's stack frame: nothing is
allocated or freed for it, and its elements are not initialized, as with `malloc`. The size must
be a compile-time constant (a literal, a `const`, or an expression of them).

```palan
const N = 4;
$[N]int32 a;          // 16 bytes in the frame
$[3]$[4]int16 g;      // 3 rows of 4 in one block
$[2]$Point pts;       // structs laid out in place ($T: Point must own no fields)
$[]int32 b = [1, 2, 3];
```

Only the outermost block goes into the frame; the rest of the type keeps its meaning. In `$[n]T`
with `T` a struct, `$[m][n]T`, and `$[m][n]$T`, the frame holds the array of pointers, and the
elements or rows are allocated on the heap and freed at scope exit as for the heap array. They
are owned by the array like the heap array's, so they can be moved with `->>`.

A stack array is used like a heap one — indexed, copied with `->`, borrowed with `@`/`@!`, and
passed to a `[]T` parameter or a C function — with these differences:

- It does not own its storage, so it cannot be the source of `->>`.
- It cannot be returned: its storage ends with the function.
- `$` is not allowed on a parameter or a named return.

A large array can overflow the stack (typically 8 MiB for the main thread); a pointer borrowed
from a stack array must not outlive it (see [Address-Of](#10-address-of-operator)).

### Passing to C Functions

An array variable is passed to C functions as a pointer to its first element, matching the C `uint8 *` / `char *` convention.

```palan
cinclude <stdio.h>;

[64]uint8 buf;
sprintf(buf, "Hello, array! %d\n", 2025);
printf("%s", buf);
```

Expected output:
```
Hello, array! 2025
```

### Element Access

Individual elements are read with `arr[i]` and written with `val -> arr[i]`. The index must be an integer type.

```palan
cinclude <stdio.h>;

[10]int64 fib;
1 -> fib[0];
1 -> fib[1];
int64 i = 2;
while i < 10 {
    fib[i-1] + fib[i-2] -> fib[i];
    i + 1 -> i;
}
0 -> i;
while i < 10 {
    printf("%lld\n", fib[i]);
    i + 1 -> i;
}
```

Expected output:
```
1
1
2
3
5
8
13
21
34
55
```

### Unsized Array Types in Function Signatures

`[]T` and `[][]T` can be used as parameter types and return types in function declarations.
The semantic analyzer resolves them to plain pointer types with no ownership tracking —
`[]T` becomes a pointer to `T`, and `[][]T` becomes a pointer to a pointer to `T`. The caller
is responsible for managing the lifetime of the returned pointer. A struct element is laid out
as in a sized array: `[]T` takes the elements of a `[n]T` array, and `[]$T` those of a `[n]$T`
array. `[][m]T` takes the rows of a `[n][m]T` array (and `[][m]$T` those of a `[n][m]$T` struct
array); `m` must be a constant, and an array whose rows have another size is a compile error.
`[]@![]T` takes the slots of a `[n]@![]T` array.

Only the outermost size may be left out: the elements must match exactly. An element owned by
the array (a row, or a struct of `[n]T`) cannot be given as a `@!T` pointer slot, or the
other way around, since one side would free or overwrite what the other one owns. For example,
a `[n]@![]int32` array is returned as `[]@![]int32`, not as `[][]int32`, and a `[n]P` array
cannot be passed as `[]@!P`. The same holds for storing an array into a `[n]@![]T` slot.
This does not depend on whether the sizes are known at compile time.

An owned element can be lent as a read-only `@T` slot when the slots are read-only too
(`@[n]@P`, or a C `const T * const *`): the callee can neither replace it nor free a borrow.
A C function that declares `T **` without `const` but does not write the slots takes the owned
rows through a cast, which states that the C declaration is looser than its use:

```palan
[1][width*3]JSAMPLE row;
cinfo.jpeg_write_scanlines(@!row as JSAMPARRAY, 1);   // JSAMPARRAY is JSAMPLE **
```

```palan
func sum_arr([]int32 a, int64 n) -> int64 {
    int64 s = 0;
    int64 i = 0;
    while i < n {
        s + a[i] -> s;
        i + 1 -> i;
    }
    return s;
}
```

Using `[]T` in a variable declaration is a compile error.

### Sized Arrays in Function Signatures

A `[n]T` parameter receives a copy of the argument, which it owns. A `[n]T` return, named or
not, gives the caller an array it owns, as a struct return does; returning a borrow returns a
copy. `n` must be a constant.

```palan
func bumped([3]int32 a) -> [3]int32 {
    a[0] + 1 -> a[0];        // a is the caller's array copied in
    return a;
}
func same(@[3]int32 a) -> [3]int32 { return a; }   // returns a copy

[3]int32 x = [1, 2, 3];
[3]int32 y = bumped(x);
[3]int32 z = same(@x);
printf("%d %d %d\n", x[0], y[0], z[0]);   // 1 2 1
```

### Borrowing Arrays

`@[n]T` borrows an array read-only and `@![n]T` borrows it writable. Pass the array with
`@arr` / `@!arr`: the callee asks for a borrow, so the caller writes one. Nothing is copied or
freed; the array keeps its owner.

```palan
const H = 3;
const W = 4;

[H][W]int32 grid;
fill(@!grid, 7);
printf("%d\n", sum(@grid));

func sum(@[H][W]int32 g) -> int32 {
    return g[0][0] + g[H-1][W-1];
}

func fill(@![H][W]int32 g, int32 x) {
    x -> g[0][0];
    x -> g[H-1][W-1];
}
```

- The forms are `@[n]T`, `@[n][m]T`, `@[n]$[m]T`, `@[n]$T`, and `@[n][m]$T`, each with the `@!` variant. `T`
  must be a primitive type, or a struct type in `@[n]T` and `@[n][m]T` (an array of owned structs,
  `[n]T` / `[n][m]T`) and in `@[n]$T` / `@[n][m]$T` (contiguous struct arrays, `[n]$T` / `[n][m]$T`). Every size must be a constant: an integer literal or a `const`.
- An array of pointer slots (`[n]@T` / `[n]@!T`) is borrowed as `@[n]@T`, `@![n]@!T`, and so on.
  The borrow's `@`/`@!` controls writing the slots (`@q -> g[i]`); the element's `@T`/`@!T`
  controls writing through a pointer (`1 -> g[i].x`). The element permission must match the
  array's, except that a `@` borrow may take `[n]@!T` as `@[n]@T`.
- The array's shape must match exactly: the number of dimensions, contiguous (`$`) or not, and
  every size. An array whose size is only known at run time cannot be borrowed as `@[n]T`.
- `@[]T` / `@![]T` leaves out the outer size and borrows the elements of an array of any length
  (`func greet(@[]uint8 name)`, `greet(args[0])`, `greet(@buf)`). It is the same type as a
  `@T` / `@!T` pointer: its elements are read with `p[i]` unchecked, and `len` does not apply.
- Through `@`, no element can be written, including elements reached through a row
  (`g[i][j]`) and the fields of a struct element (`g[i].x`). A `@` borrow cannot be passed
  where `@!` is expected.
- A struct element is still passed to a `@T`/`@!T` parameter as `@g[i]` / `@!g[i]`.
- A row of a multi-dimensional array is borrowed the same way: `@g[i]` / `@!g[i]` gives the row of
  `[n][m]T`, `[n]$[m]T`, `[n][m]$T` or `[n][m]T` (struct `T`) as `@[m]T`, `@[m]$T` and so on, also
  when `g` is itself a borrowed array.
- An array field is borrowed the same way: `@s.v` / `@!s.v` gives a `[n]T`, `$[n]$T` or `$[n]@T`
  field as `@[n]T`, `@[n]$T` or `@[n]@T`.
- A borrowed array can also be a local variable (`@[4]int64 p = @v;`).
- A borrowed array is already a borrow, like a `@T`/`@!T` pointer: pass it on, bind it, and
  rebind it by name (`sum(g)`, `len(g)`, `@[4]int64 q = p;`, `q -> p`). `@g` on it is a compile
  error. A `@!` borrow may be given where `@` is expected. A row (`g[i]`) is part of the borrowed
  storage, not a borrow of its own, and cannot be given by name.
- An array given to a `@T`/`@!T` pointer (a parameter, a variable, a pointer field or slot) is
  written with `@`/`@!` too (`f(@!v)`, `@!v -> slots[0]`), and so is a row (`@!m[i]`) or an array
  field (`@!s.v`). Only a `[]T` parameter and a C function take an array by name, as in C, and a
  `[]T` parameter, which owns nothing, is passed on by name.
- `@!arr` also works where a C function takes a pointer to the elements (`memset(@!v, 0, 32)`).
- A function may return a borrowed array (`-> @[]T`, `@[n]T`, `@[+]T`, or `@!`), such as one
  reached through a borrowed parameter. The call is itself a borrow, passed and bound by name
  (`@[]uint8 p = name(@s);`). As with a returned `@T`, nothing checks that it does not outlive
  what it borrows (see [Address-Of Operator](#10-address-of-operator)).

  ```palan
  type Text { $[+]uint8 bytes = [0]; };
  func cstr(@Text t) -> @[]uint8 { return @t.bytes; }

  Text t;
  printf("[%s]\n", t -> cstr());     // []
  ```

### Array of Pointer Slots (`[n]@![]T`)

`[n]@![]T` declares an array of `n` writable pointer slots, each capable of holding a `[]T`
pointer. The outer array is heap-allocated (`malloc(n * 8)`) and automatically freed at scope
exit. A slot borrows what it points to and never owns it: store into it with `->` (for example
memory from C `malloc`, or an array as `@!arr`) and free that memory yourself. `->>` into a slot is a compile error.
`[n]@![]$[m]T` and `[n]@![]$T` are slots the same way, pointing at contiguous rows or structs.
`[n]@[]T` declares read-only slots the same way: they take `@arr` as well as `@!arr`, and
nothing can be written through them. Slots can be the elements of rows as other elements can
(`[n][m]@T`, `[n]$[m]@T`, `[+][m]@T`, a `[n][m]@T` field, a `[][m]@T` parameter).

```palan
int64 rows = 4;
[rows]@![]int32 ptrs;   // malloc(rows * 8) — outer array
malloc(3 * 4) -> ptrs[0];
// ...
free(ptrs[0]);
// free(ptrs) emitted automatically at scope exit
```

### Copying (`->`)

`src -> dst` where `dst` is an array or a struct copies the contents of `src` into the storage
`dst` already has; the two stay independent. The destination can be a variable, an array
element or row (`a[i]`, `m[i]`), or a field. Initializing a declaration from another array or
struct (`[4]$Point p = minos[1];`, `Point q = p;`) copies the same way.

```palan
[2][4]$Point minos = [[0,0][0,1][0,2][0,3], [0,0][0,1][1,1][2,1]];
[4]$Point mino = minos[1];   // copy row 1
minos[0][3] -> mino[0];      // copy one struct element
2 -> mino[1].x;              // minos is unchanged
```

- The source must have the same element type and layout as the destination, and the same sizes
  where both are known at compile time; otherwise it is a compile error. A source may be borrowed
  (`@[n]T`, `@T`). A size known only at run time is copied
  [up to the shorter one](#sizes-known-only-at-run-time).
- What the destination owns is copied too: owned struct fields and arrays, and the structs of a
  `[n]T` array, get copies of the source's. A `@T`/`@!T` pointer is copied as a pointer.
- An element taken out with `->>` is NULL, and stays NULL in the copy. An element copied into
  a slot that is NULL is made there first, as a new value.
- A struct returned by a function is copied the same way, and then freed. A `[]T` return has
  no size known at compile time, so it cannot be copied into an array.

#### A Type's Own Copy (`func ->`)

A struct type can define what copying into an existing value of it does:

```palan
cinclude <stdio.h>;

type Text { [+]uint8 bytes; int32 mode; };
// Keeps dst's mode: only the bytes are copied.
func ->(@Text src, @!Text dst) { src.bytes -> dst.bytes; }

Text a; 1 -> a.mode;
Text b; 2 -> b.mode;
a -> b;            // calls ->(@a, @!b): b.mode stays 2
Text c = a;        // a new value: copied whole, c.mode is 1
```

- The function is `func ->(@T src, @!T dst)`: two parameters of one struct type, no defaults and
  no return value. It is part of the type: it is defined at the top level of the module defining
  `T`, is not exported, and comes with `T` into every module `T` reaches, also one reaching `T`
  only as a field of another type. A C struct has none.
- It is called wherever a value of `T` that already exists is copied into, nested or not: `x -> v`,
  a field of `T` when the struct holding it is copied into (`p -> q` calls it for `q.name` when
  `name` is a `T`, `$T` or `[n]$T` field), each element of a `[n]T`, and each element a `[+]T`
  already has. So `p -> q` and `p.name -> q.name` agree. The rest of a copy is as without it.
- A new value is copied whole and never calls it: initializing a declaration (`T c = a;`), the
  elements a `[+]T` gains by a copy, `->>` and `<<=`, an argument for a `T` parameter, and a
  returned value.
- An array or struct literal written into an existing value (`[[1, 2], 0] -> a`) is made as a new
  value first, which is then copied in as above.
- `src` and `dst` may be the same value (`s -> s`); the function must handle that.

### Ownership Transfer (`->>`)

`val ->> arr[i]` transfers ownership of `val` into an owned element or row: a struct of a
`[n]T` array, or a row of an `[m][n]T` array. What the element or row held is freed first. `val`
is then set to NULL, so that the automatic `free(val)` at scope exit becomes `free(NULL)` — a
no-op by C standard.

```palan
[3]int32 inner;
[2][3]int32 outer;

inner ->> outer[0];      // outer's original row 0 is freed; inner is set to NULL
// free(inner) at scope exit → free(NULL) = no-op
```

A row must have the same element type as `val`, and the same size where both are known at
compile time.
A `@T`/`@!T` pointer slot (including `[n]@![]T`) cannot take ownership; `->>` into one is a
compile error.

`val ->> obj.field` transfers `val` into an owned struct field (`T` or `[n]T`). What the field
held is freed first, and `val` is set to NULL as above. Other fields are a compile error.

```palan
type W { Point pt; };
W w;
Point p;
p ->> w.pt;              // w's original Point is freed; p is set to NULL
```

`val ->> var` transfers `val` into a variable that owns its array or struct. What the variable
held is freed first, and `val` is set to NULL as above. An array must have the same element type
as the variable, and the same sizes where both are known at compile time; a struct must be of
the same type. A stack (`$`) variable, a borrow, or `val` naming the variable itself is
a compile error.

```palan
[4]int32 a = [1, 2, 3, 4];
[4]int32 c;
a ->> c;                 // c's original array is freed; a is set to NULL
```

The source must own what it gives away: a variable that owns its array or struct, an owned
field or element (`v.pt ->> w.pt`, `pts[1] ->> w.pt`, `m[1] ->> w.arr`), or a value returned by
a function. A field or element source is set to NULL like a variable. A borrow (`@T`/`@!T`,
`@[n]T`), a stack (`$`) variable, or a `@T` field is a compile error.

`return` on a tracked array variable also transfers ownership: the variable is removed from
free-tracking and the caller receives the pointer.

### Giving Values Away (`>>`) and Taking Them Over (`<<=`)

`v>>` gives what `v` owns away as a call argument, and `v` is set to NULL as by `->>`. Given to a
parameter that owns its argument (a struct `T`, `[n]T`, `[+]T`), it is passed without a copy:

```palan
func total([+]int32 v) -> int64 t {   // v is freed when total returns
    0 -> t;
    for copy e in v { t + e -> t; }
}

[+]int32 big = [1, 2, 3];
total(big>>);                // no copy; big is now NULL
big ->> total();             // the same, chained
```

- The source is an owner, as for `->>`: a variable that owns its array or struct, an owned field
  or element, a value a call returns, or a pointer viewed as an owner (`p as T`).
- `v>>` is written only as a call argument to a parameter that owns it, and cannot be cast.
  Given to a `@T`/`@!T` parameter, a C function's pointer parameter, or a variadic argument, it
  is a compile error, since nothing would own it. A whole array is given to a variadic parameter
  as `...a>>` (see [Variadic Arguments](#variadic-arguments)).
- The call's arguments are evaluated left to right, and `v>>` reads `v` where it is written, but
  `v` becomes NULL only after all of them, just before the call. Another argument can still
  borrow from it, as a C function taking a request and a part of it expects
  (`(uv_write as @func(write_req_t, _, _, _, _))(req>>, client, @req.buf, 1, on_write)`).
- An argument after `v>>` that could change `v` before that is a compile error: giving `v`, a
  part of it or what contains it away again; borrowing with `@!` (or calling a method on) what
  contains `v`, or a variable its index reads (`f(a[i]>>, g(@!i))`); or, when `v` or its index
  is reached from a global, calling any function.

`T b <<= v;` declares `b` taking over what `v` owns, without a copy. `v` is an owner (set to
NULL) or a call result:

```palan
[4]int32 a = [1, 2, 3, 4];
[4]int32 c <<= a;                     // a is now NULL
```

- `b` must own its array or struct: not a `$` variable, a borrow or a primitive. `v` must have
  `b`'s type (for an array, the same sizes where both are known at compile time; a size `b`
  leaves to run time is [`v`'s](#sizes-known-only-at-run-time)).

#### Using a Variable After It Is Given Away

A local variable or parameter that has given its whole value away (`->>`, `>>`, `<<=`) holds
NULL, and using it is a compile error until `->>` fills it again. Any appearance counts as a use
(`len(@a)` too, `@a`, `-> a`, giving it away again, `return a`), except comparing the variable
itself with `NULL` (`a == NULL`, `a != NULL`) and the release at scope exit.

```palan
[3]int32 a;
[3]int32 b;
a ->> b;
len(@a);                     // error: 'a' was given away
[3]int32 t;
t ->> a;                     // a holds t's array
len(@a);                     // 3
```

- A use is judged along each path to it: given on any path that reaches it (one branch of an
  `if`, a `break` reaching past the loop) is an error, and a path ending in `return` reaches
  nothing. In a loop, a use reached from the loop's start before `->>` fills the variable is an
  error if the end of an iteration (or a `continue`) leaves it given.
- A variable becomes given when its statement ends (for `<<=`, when its initializer does): the
  rest of the same statement is not checked, as the arguments after `v>>` may still borrow it.
- A field or element given away, and a global, are not checked.

#### Ownership Across C Functions

A C function's pointer parameter only borrows, as C's types say nothing about who frees what. A
C function that keeps what it is given, or hands back what the caller is to free, is called
through a [cast](#explicit-cast) to a function pointer type owning that parameter or return
value, and `>>` gives the value to it. Nothing frees it in Palan from then on:

```palan
uv_tcp_t client;
...
(uv_close as @func(uv_tcp_t, @func(uv_tcp_t)))(client>>, on_close);

func on_close(uv_tcp_t client) { }    // libuv hands client back; it is freed here
```

Only a C function named in the cast can be read this way. A Palan function frees what its own
parameters own, so casting it, or any function pointer value, to a type that owns or borrows
differently is an error: `on_close as @func(@!uv_handle_t)` would have the caller keep what
`on_close` frees.

A pointer C hands back inside a struct is viewed as its owner with `as` and taken over:

```palan
func on_read(@!uv_stream_t s, int64 nread, @uv_buf_t buf)
{
    [65536]int8 data <<= buf.base as [65536]int8;
}   // data is freed here; buf.base is left as it is
```

A pointer C is to hold is given the same way, by viewing the location it goes to as an owner.
What the location held is not freed, as it is no Palan owner:

```palan
func alloc_buffer(@!uv_handle_t handle, uint64 size, @!uv_buf_t buf)
{
    [size]int8 data;
    data ->> buf.base as [size]int8;     // data is now NULL; buf.base holds the block
    size -> buf.len;
}
```

- Whether a pointer really points to a `T` that Palan can free is not checked, as in C.
- A Palan function owning a parameter frees what it owns inside it; C's `free` does not, so give
  C's `free` only a struct or array that owns nothing further.

### Two-Dimensional Arrays (`[m][n]T`)

`[m][n]T` declares a two-dimensional array with `m` rows and `n` columns. The outer array is
heap-allocated; each row is independently heap-allocated by the auto-generated allocator.
`T` can also be a struct type; see [Struct Arrays](#struct-arrays).

```palan
int64 rows = 2;
int64 cols = 3;
[rows][cols]int32 mat;
```

**Element access:**
- Read:  `mat[i][j]`
- Write: `val -> mat[i][j]`

Both row and column indices must be integer types.

**Memory management:** The compiler allocates the outer array and each row at declaration and
frees them at scope exit. No manual memory management is required.

### Contiguous Two-Dimensional Arrays (`[n]$[m]T`)

`[n]$[m]T` declares a contiguous 2D array where all `n × m` elements occupy a single
heap-allocated block (`malloc(n * m * sizeof(T))`). Unlike `[n][n]T`, only one allocation and
one free are needed. The inner dimension `m` can be a compile-time constant or a runtime
variable expression.

```palan
int64 rows = 3;
[rows]$[4]int32 mat;   // malloc(rows * 4 * sizeof(int32)) = malloc(rows * 16)
```

**Element access:**
- Read:  `mat[i][j]`
- Write: `val -> mat[i][j]`

Both indices must be integer types.

**Row access:** `mat[i]` yields a transient `[]T` pointer to the start of row `i`. This
pointer is non-owning and must not be freed.

```palan
func row_sum([]$[4]int32 mat, int64 r) -> int32 {
    return mat[r][0] + mat[r][1] + mat[r][2] + mat[r][3];
}
```

**Function parameters:** Declare as `[]$[m]T` with a fixed inner dimension `m`. The compiler
rejects calls where the argument's inner dimension is variable or does not match `m`.

**Memory management:** A single `malloc` is called at declaration and a single `free` is
inserted at scope exit. No helper functions are generated.

### Arrays of More Levels

The levels combine in any order and number: `[2][3][4]int32` has rows of rows, each in its own
block; `[2]$[3]$[4]int32` lays all 24 elements out in one block; `[2]$[3][4]int32` lays out
the 6 rows' slots in one block. They are indexed, copied, moved and borrowed like 2D arrays, and
any level may have a length known only at run time. A function parameter or a borrow needs the
lengths of the levels laid out in place to be constants.

```palan
[2][3][4]int32 a;
5 -> a[1][2][3];
[2][3][4]int32 b = a;   // copies every level
int64 k = 3;
[2]$[k]$[4]int32 c;     // one block of 2 * k * 4 elements
```

### Variable-Length Arrays (`[+]T`)

`[+]T` is an array whose length changes at run time. It starts empty; `[+n]T` starts empty
with room for `n` elements, and an array literal sets its length. `v[i]` reads and writes an
element as in other arrays, without bounds checking. Four built-in functions work on it:

| Function | Meaning |
|----------|---------|
| `len(@v)` | the number of elements, an `int64` |
| `push(@!v, x)` | appends `x`, a copy of it unless it is moved (`x>>`) |
| `resize(@!v, n)` | sets the length to `n` (an `int64`); new elements are undefined, as an array variable's are |
| `pop(@!v)` | removes the last element and returns it |

They are ordinary functions, so they can be called in method form (`v.push(x)`) or after `->`
(`x -> v.push()`, `v -> len()`; see [Chained Calls](#chained-calls)).

```palan
cinclude <stdio.h>;

[+]int32 v;                  // empty
v.push(10);
20 -> v.push();
push(@!v, 30);
printf("%ld %d\n", len(@v), v[2]);      // 3 30
v.resize(4);                 // v[3] is undefined until written
40 -> v[3];
int32 last = v.pop();
printf("%ld %d\n", v -> len(), last);   // 3 40

[+16]int32 w;                // room for 16, length 0
[+]int32 u = [1, 2, 3];      // length 3
printf("%ld %ld\n", len(@w), len(@u));  // 0 3

int64 i = 0;
while i < u -> len() {
    printf("%d ", u[i]);
    i + 1 -> i;
}
printf("\n");                // 1 2 3
```

- Growing past the room it has reallocates the elements, at least doubling the room, so adding
  one element at a time reallocates only now and then. Shrinking keeps the room.
- `pop` on an empty array and `resize` to a negative length abort the program.
- `[+n]T v = [...]` takes `n` elements, the literal's first and the rest undefined; `n` must be
  a constant no less than the literal's length.
- `$[+]T v` keeps the array's header in the stack frame; its elements are on the heap either way.
- `len` also takes an array of constant size (`len(@a)` for `[4]int32 a` is 4), and is then a
  constant: `const N = len(@a);`. For an array whose size is known only at run time it reads the
  size the variable keeps (see [Sizes Known Only at Run Time](#sizes-known-only-at-run-time)),
  which is not a constant. A `[]T` has no length to give and is a compile error.
- A function with the same parameter types as a built-in one is a compile error.

**Borrowing and passing.** `@[+]T` and `@![+]T` borrow the array, as for other arrays (see
[Borrowing Arrays](#borrowing-arrays)); only `@!` can change its length. Passed to a `[]T`
parameter, such as a C function's pointer, the array gives its elements, and so does `@v`/`@!v`
(or a `@[+]T` borrow) given to a `@[]T`/`@![]T`. A `[+]T` parameter, a
`[+]T` return and copying follow the rules of other arrays and structs:

```palan
cinclude <stdio.h>;

func total([]int32 a, int64 n) -> int64 s {
    int64 i = 0;
    while i < n { s + a[i] -> s; i + 1 -> i; }
}
func sum(@[+]int32 v) -> int64 s {
    int64 i = 0;
    while i < v -> len() { s + v[i] -> s; i + 1 -> i; }
}
func addTo(@![+]int32 v, int32 x) { x -> v.push(); }
func evens(int64 n) -> [+]int32 r {
    int64 i = 0;
    while i < n { r.push(int32(i * 2)); i + 1 -> i; }
}
func grown([+]int32 v) -> [+]int32 {
    v.push(99);
    return v;
}

[+]int32 v = evens(4);       // 0 2 4 6
addTo(@!v, 8);
printf("%ld %ld\n", sum(@v), total(v, len(@v)));   // 20 20
[+]int32 c = v;              // a copy
1 -> c[0];
[3]int32 a = [7, 8, 9];
[+]int32 d;
a -> d;                      // d has 3 elements
[+]int32 g = grown(v);       // v is copied in, so it keeps its length
printf("%d %d %ld %ld %ld\n", v[0], c[0], len(@d), len(@v), len(@g));  // 0 1 3 5 6
[+]int32 m;
v ->> m;                     // m takes v's array; v is left NULL
printf("%ld\n", len(@m));    // 5
```

- A `[+]T` parameter receives a copy of the argument. Returning a borrow (`return v;` for a
  `@[+]T v`) returns a copy, as for a struct.
- Copying into a `[+]T` takes any number of elements and sets the length to it. Copying a `[+]T`
  into a `[n]T` copies as many as both have; the `[n]T` keeps its size.
- Changing the length can move the elements, so a borrow of an element (`@!v[i]`) or the
  elements given to a `[]T` must not be kept across a `push` or `resize`. This is not checked.

**Elements.** An element can be a struct, a row or another variable-length array, nested to
any depth:

```palan
cinclude <stdio.h>;

type Point { int64 x; int64 y; };

[+]Point ps;                 // each element is its own block
Point p;
3 -> p.x;
ps.push(p);                  // copies p in
ps.push({x: 5, y: 6});
ps.resize(3);                // ps[2] is a new Point
9 -> ps[2].y;
Point q = ps.pop();          // q owns the last element
printf("%ld %ld %ld\n", len(@ps), ps[1].x, q.y);   // 2 5 9

[+]$Point qs;                // elements laid out in the array's data
qs.push(p);
printf("%ld\n", qs[0].x);    // 3

[+][+]int32 m;               // a row of its own length per element
m.resize(2);
7 -> m[1].push();
printf("%ld %ld\n", len(@m[0]), len(@m[1]));      // 0 1

[+][3]int32 rows;            // rows of 3
[3]int32 r = [1, 2, 3];
rows.push(r);                // copies r in
rows.push([4, 5, 6]);
rows.resize(3);
printf("%ld %d\n", len(@rows), rows[1][2]);       // 3 6

[2][+]int32 two;             // two variable-length arrays
4 -> two[1].push();
printf("%d\n", two[1][0]);   // 4
```

- `push` takes a struct or a row as a parameter of its type does: a copy, or the value itself
  when it is moved (`rows.push(r>>)`); `pop` returns an element the caller then owns.
- `$` cannot lay out something that owns storage: `[+]$[+]T` and `$[+]T` as an element are
  compile errors.
- A `[+]T` struct field is described in [Array Fields](#array-fields).
- `[+]@T` / `[+]@!T` holds pointer slots, as `[n]@T` does (see [Array of Pointer
  Slots](#array-of-pointer-slots-nt)): each element borrows what it points to, so it takes
  `@x` / `@!x` (`ps.push(@!p)`, `@!q -> ps[0]`) and nothing it points to is freed. `[+]@[]T`
  holds pointers to arrays of any length; the program's arguments, `args`, are a
  `@[+]@[]uint8` (see [Program Structure](#program-structure)).

### Array Literals

An array variable can be initialized with an array literal `[a, b, c]` in its declaration.
When the size is omitted (`[]T`), it is taken from the number of elements.

```palan
cinclude <stdio.h>;
int32 n = 10;
[3]int32 a = [1, -2, 3];
[]flo64 f = [1.5, 2, n];
printf("%d %d %d\n", a[0], a[1], a[2]);
printf("%.1f %.1f %.1f\n", f[0], f[1], f[2]);
```

Expected output:
```
1 -2 3
1.5 2.0 10.0
```

A two-dimensional literal can be written either as a sequence of rows (`[1,2,3][4,5,6]`) or as
nested rows (`[[1,2,3],[4,5,6]]`); both mean the same. It initializes both `[m][n]T` and
`[n]$[m]T` arrays, and omitted dimensions (`[][]T`, `[]$[]T`) are taken from the literal.

```palan
cinclude <stdio.h>;
[2][3]int32 a = [1,2,3][4,5,-6];
[]$[3]int32 d = [[1,2,3],[4,5,6]];
printf("%d %d %d %d\n", a[0][0], a[0][2], a[1][1], a[1][2]);
printf("%d %d %d\n", d[0][2], d[1][0], d[1][2]);
```

Expected output:
```
1 3 5 -6
3 4 6
```

Elements may be any expressions, converted to the element type by the same rules as a scalar
variable's initializer: implicit narrowing is an error (use an explicit cast such as `uint8(n)`),
and integer literals are range-checked against the element type. The elements are evaluated
before the array variable is declared, so they cannot refer to the variable itself.

A trailing comma after the last element of a row is allowed (`[1, 2, 3,]`), which is convenient
when a literal is written over several lines.

An array of structs (`[n]T`, `[n]$T`, `[m][n]T`, `[m][n]$T`) is initialized the same way, with each element
written either as its field values in declaration order (`[1, 2]`) or by field name
(`{y: 2, x: 1}`). Every field must be given exactly once. A struct field (`T` or `$T`) is written
as a nested struct value; pointer and array fields cannot be initialized by a literal. Since a
struct element is itself written in brackets, `[0,1][2,3]` is one row of two `Point`s.

```palan
cinclude <stdio.h>;
type Point { int32 x; int32 y; };
type Line { Point a; $Point b; };
[2][2]Point m = [[0,1][2,3], [4,5][6,7]];
[]$Point e = [{y: 5, x: 6}, [7, 8]];
[1]Line l = [[{x: 1, y: 2}, [3, 4]]];
printf("%d %d %d %d\n", m[1][0].y, e[0].x, l[0].a.y, l[0].b.x);
```

Expected output:
```
5 6 2 3
```

In a comma-separated declaration, each initializer belongs to its own variable:
`[2]int16 r, s = [7, 8];` initializes only `s`.

**Restrictions:**
- An array literal can only be used as an array variable's initializer, the source of `->` to
  an array (a variable, a field, or a row), or an argument for a [borrowed array](#borrowing-arrays)
  parameter, written `@[..]` / `@![..]` as for an array variable (`sum(@[1, 2, 3])`). The argument
  is a temporary array of the parameter's type, freed as a struct returned by a call and given as
  an argument is (see [Struct types in function signatures](#struct-types-in-function-signatures)).
  A struct value can also be used where a struct is expected; see [Struct literals](#struct-literals). A struct element
  may also be a struct expression, which is copied (`[os[1], os[0]] -> os` swaps).
- As with a struct literal, `[a[1], a[0]] -> a` reads every value before writing any element, and
  the target of `->` must be the array itself: giving a literal to a borrowed array variable is an
  error, since `->` to a borrow rebinds it.
- A given dimension, or every size of the target of `->`, must be a compile-time constant that
  matches the literal's element or row count, and every row of a 2D literal must have the same length.
- Only numeric (integer and float) and struct element types are supported. Arrays of pointers
  and arrays of three or more dimensions cannot be initialized with a literal.
- A `{name: value}` literal can only be used as a struct value (an element of an array literal, or
  a [struct literal](#struct-literals)).
- Omitting only the inner dimension (`[2][]T`, `[2]$[]T`) is not supported.

### Struct Arrays

Palan supports four forms of struct array declarations. All are heap-allocated and automatically freed at scope exit,
or placed in the stack frame with `$` (see [Stack Arrays](#stack-arrays-nt)).

| Form | Memory | Element type |
|---|---|---|
| `[n]$T pts` | `malloc(n * T.totalSize)` / `free(pts)` | contiguous inline elements |
| `[n]T pts` | `__pln_alloc_arr_T(n)` / `__pln_free_arr_T(pts, n)` | owned pointers, each element allocated separately |
| `[n]@T pts` | `malloc(n * 8)` / `free(pts)` | non-owning read-only pointers |
| `[n]@!T pts` | `malloc(n * 8)` / `free(pts)` | non-owning mutable pointers |

`pts[i]` yields a `T` pointer for all four forms. Fields are accessed with `pts[i].field`.

The pointer-slot forms also take a primitive element type: `[n]@int32` / `[n]@!int32` hold
`@int32` / `@!int32` pointers, and `pts[i][0]` reads or writes the value one points to. The
element may be any pointer type: `[n]@!@int32` holds pointers to `@int32` pointers, and
`[n]@func(...)` holds function pointers.

```palan
cinclude <stdio.h>;
type Point { int64 x; int64 y; };

// [n]$T — contiguous inline struct array
[3]$Point pts;
10 -> pts[0].x;  20 -> pts[0].y;
printf("%ld %ld\n", pts[0].x, pts[0].y);  // 10 20

// [n]T — owned pointer array (each element separately allocated)
[2]Point pts2;
5 -> pts2[0].x;
printf("%ld\n", pts2[0].x);  // 5

// [n]@T / [n]@!T — non-owning pointer arrays
Point p;
99 -> p.x;
[4]@Point rpts;    // read-only slots
@p -> rpts[0];
printf("%ld\n", rpts[0].x);   // 99

[4]@!Point wpts;   // writable slots
@!p -> wpts[0];
42 -> wpts[0].x;
printf("%ld\n", p.x);         // 42 (write-through via pointer)
```

A two-dimensional struct array is declared as `[m][n]T`: each of the `m` rows is an `[n]T` owned
pointer array. `pts[i][j]` yields a `T` pointer, and `pts[i][j].field` accesses a field.

```palan
cinclude <stdio.h>;
type Point { int64 x; int64 y; };
[2][3]Point grid;
7 -> grid[1][2].y;
printf("%ld\n", grid[1][2].y);  // 7
```

`[m][n]$T` instead makes each row an `[n]$T` contiguous array, so the structs sit in the rows
themselves. It is accessed the same way.

```palan
[2][3]$Point tiles;
7 -> tiles[1][2].y;
printf("%ld\n", tiles[1][2].y);  // 7
```

**Restrictions:**
- `[n]$T` and `[m][n]$T` require that `T` owns no fields (no `U field` or `[k]U field`). Use `[n]T` / `[m][n]T` instead when it does.
- `[m]$[n]T` lays out the `m × n` struct slots in one block; `[m]$[n]$T`, the structs themselves (`T` owning no fields).

Array **fields** (declared inside `type { ... }`, see Section 19) use the same four forms but
require `n` to be a compile-time integer literal, since struct layout must be statically known.
Array **variables** (this section) allow non-constant `n`, except a stack array (`$[n]T`).

### Limitations (current version)

- An array declared at the top level (outside any block) is not freed when the top-level statements end; the OS reclaims the memory at process exit.
- Boundary checking is not performed.
- `[n]$T` is not supported when `T` owns fields.
- A `[][+]T` parameter is not supported.

---

## 9. Struct Types

Define a struct type with `type`:

```palan
type Point { int64 x; int64 y; };
```

Declare a variable, assign fields, and read fields:

```palan
Point p;
10 -> p.x;
20 -> p.y;
printf("%ld %ld\n", p.x, p.y);   // 10 20
```

A field access or an index can start from any expression that gives a struct or array reference,
not only a variable: a cast, a parenthesized expression, or a call returning a `@T`/`@!T` (a C
function returning a pointer, too). Writing through it follows that reference's permission.

```palan
3 -> view(@!p).x;                       // view returns @!Point
printf("%d\n", gmtime(@t).tm_year);     // gmtime returns @!tm
```

A call returning an owned struct (`T`) or a C struct by value has nothing that would free the
result, so `mk().x` and `div(7, 2).quot` are compile errors: receive the result into a variable
with `->` first.

- **Memory management**: heap-allocated, with undefined contents but for fields with
  [initializers](#field-initializers) (see
  [Initial contents](#4-variables-constants-and-type-aliases)); automatically freed at scope exit.
  With `$` (`$Point p;`) the struct is placed in the stack frame instead; see
  [Stack Structs](#stack-structs-t).
- **Layout**: C ABI-compatible (natural alignment, padding, total size rounded up to max-field alignment). Matches System V AMD64 ABI struct layout.
- **Visibility**: every field is public; an exported struct may hide fields from other files
  (see [Hidden and Read-only Fields](#hidden-and-read-only-fields)).

### Stack Structs (`$T`)

`$Point p;` places the struct in the function's stack frame, its contents undefined like a heap struct's.
What it owns (`U field`, `[k]U field`) is still allocated when the declaration is reached and freed
at scope exit; only the struct's own storage is in the frame. Initializers work as for a heap
struct, copying the value: `$Point p = q;`, `$Point p = {x: 1, y: 2};`, and
`$Point p = makePoint(1, 2);`, where the returned struct's contents, owned fields included, move
into `p` and the returned storage is freed. A struct returned by value from a C function is
received directly into the frame.

A stack struct is used like a heap one, including `@p`/`@!p` and passing it by value to a C
function, with these differences:

- It does not own its storage, so it cannot be the source of `->>`.
- Returning it returns a copy, as for a struct the function doesn't own.
- `$` is not allowed on a parameter or a named return.

### Struct Literals

A struct value written as in an array literal's struct element (`[1, 2]` or `{y: 2, x: 1}`) can
initialize a struct variable, be the source of `->` to a struct, be returned from a function
returning a struct, and be passed as an argument:

```palan
type Point { int64 x; int64 y; };
Point p = {x: 10, y: 20};
[p.y, p.x] -> p;              // every value is read before any field is written: swaps
func reset(@!Point q) { [0, 0] -> q[0]; }
func sub(@Point a, @Point b) -> Point { return [a.x - b.x, a.y - b.y]; }
reset(@![1, 2]);
```

- As an argument, the literal is a temporary struct, written as a struct variable would be:
  `@[..]` / `@![..]` (`@{..}` / `@!{..}`) for a `@T`/`@!T` parameter, and without `@` for a `T`
  parameter. It is freed as a struct returned by a call and given as an argument is (see
  [Struct types in function signatures](#struct-types-in-function-signatures)). Of a C function's
  parameters, only one taking a struct by value can take a literal (see
  [Structs Passed By Value](#structs-passed-by-value)).

- The target of `->` must be the struct itself: a struct variable, a `$T`/`T` field, a struct
  array element, or `q[0]` for what a borrow `q` points to. Giving a literal to the borrow `q`
  itself is an error, since `->` to a borrow rebinds it.
- The field rules are those of an array literal's struct element: every field exactly once,
  except that `{name: value}` may leave out a field with an [initializer](#field-initializers),
  which then takes it; struct fields as nested values, array fields as array literals (`[[1, 2, 3], 4]`), no pointer
  fields or arrays of pointers.
- A struct field's value may also be a struct expression of that type — a struct variable, a
  `T` field, an array element, a `@T`/`@!T`, or a call result: `Q q = [p, mk(3)];` copies it. With `->`
  it is read before any write too, so `[q.b, @q.a] -> q` swaps.

### Field Initializers

A field can be given an initial value in the type, written as a variable's initializer is:

```palan
const N = 4;
type Counter { int32 step = N / 2; bool on = true; $[+]uint8 name = [0]; int32 n; };

Counter c;                    // c.step is 2, c.on is true, c.name holds one 0; c.n is undefined
Counter d = {n: 5};           // the fields left out take their initializers
printf("%d %d %ld\n", c.step, d.on, len(@c.name));   // 2 1 1
```

- The value must be a compile-time constant: literals, [consts](#constant-declarations) and
  operators on them, an enum member, or an array literal of such values.
- A primitive field (an integer, a float, `bool` or an enum) and a one-dimensional array of
  primitives (`[n]T`, `$[n]T`, `[+]T`, `$[+]T`) can have one. Others, such as a struct field given
  a struct value, cannot in this version.
- The initializers are in place wherever a struct comes to exist: a variable, heap or `$`; a
  struct field, whose own type's initializers apply; an element of an array, laid out in place
  (`[n]$T`, `[+]$T`) or not, including those `resize` adds. A struct taken over with `<<=` or
  `->>`, or written by a C function, keeps what it holds.
- A `{name: value}` literal may leave out a field with an initializer; a `[...]` literal still gives
  every field.

### Field Types

Integer and float primitives (`int8`–`int64`, `uint8`–`uint64`, `flo32`, `flo64`) are declared without a prefix:

```palan
type Point { int64 x; int64 y; };
```

Struct-type fields are written with a prefix that controls ownership and memory layout:

| Syntax     | Meaning                              | Memory          |
|------------|--------------------------------------|-----------------|
| `$T field` | Inline embedding — T's bytes are part of the parent struct's allocation; what T owns is allocated, copied and freed with the parent | parent's block  |
| `T field`  | Owned pointer — 8-byte pointer; T is auto-allocated/freed with parent | `__pln_alloc_T` |
| `@T field` | Non-owning read-only pointer — 8-byte pointer; lifecycle is user-managed; pointer value may be set but field write-through is not allowed | none            |
| `@!T field`| Non-owning mutable pointer — same as `@T` but field write-through is also allowed | none            |

The pointee of `@T`/`@!T` may itself be a pointer, each level with its own permission
(`@!@int32 pp;` is a mutable pointer to a read-only pointer to `int32`).

`@T`/`@!T` are general pointer-type expressions, not exclusive to struct fields — they may
also be used as a plain local variable's declared type, as a non-owning alias into existing
storage (no allocation, no automatic freeing at scope exit). Field access on such a variable
works the same as on a struct-field pointer of the same kind:

```palan
type Point { int64 x; int64 y; };

Point original;
5 -> original.x;  10 -> original.y;

@!Point view = @!original;   // non-owning alias — view and original share the same storage
20 -> view.x;               // write-through
printf("%ld %ld\n", original.x, original.y);   // 20 10
```

### Array Fields

An array field takes the type an array variable can, with the same meaning. Where a variable's
`$` puts the array in the stack frame, a field's `$` puts it in the struct's own block.

| Syntax        | Meaning                                  | Memory                          |
|---------------|-------------------------------------------|----------------------------------|
| `[n]T field`, `[n]$T field`, `[n][m]T field`, `[n][+]T field`, `[+]T field`, ... | Owned array, as an array variable of that type | cascaded alloc/free with parent |
| `$[n]T field` (primitive `T`), `$[n]$T field` (struct `T`), `$[n]$[m]T field`, ... | Array laid out in the struct | parent's block |
| `$[n]T field` (struct `T`), `$[n][m]T field`, `$[n][+]T field`, ... | Array laid out in the struct, holding elements that own storage | parent's block (slots); elements cascade with parent's alloc/free |
| `$[+]T field` | Variable-length array whose header is laid out in the struct | parent's block (header); elements cascade with parent's alloc/free |
| `$[n]@T field` / `$[n]@!T field` | Non-owning pointer slots laid out in the struct | parent's block (slots only) |

`n` (and each `m`) must be a compile-time integer: a literal, a [const](#constant-declarations),
or `sizeof(T)`. An owned array field is allocated, copied and freed with its struct; its
primitive elements start undefined, as an array variable's do. An array laid out in the struct
is copied with it. When its elements own storage (`$[n]T` with a struct `T`, `$[n][m]T`,
`$[n][+]T`), their slots lie in the struct and the elements are allocated, copied and freed
with it, as those of an array variable in the stack frame are with the variable; a struct
with such a field owns storage itself. `$` on an element that owns storage is rejected as for
a variable. A `$[+]T` field starts empty and is used as a `[+]T` field is, but, like a `$[+]T`
variable, it cannot be given away or taken over with `->>`.
`field[i]` accesses an element; for struct-leaf forms, `field[i].sub` continues the field
chain.

```palan
type Point { int64 x; int64 y; };

// $[n]$T -- structs laid out in the struct
type Polygon { $[4]$Point pts; };
Polygon poly;
10 -> poly.pts[0].x;  20 -> poly.pts[0].y;
printf("%ld %ld\n", poly.pts[0].x, poly.pts[0].y);  // 10 20

// [n]T -- owned array of owned structs (cascades with parent's alloc/free)
type Cluster { [4]Point pts; };
Cluster c;
5 -> c.pts[0].x;
printf("%ld\n", c.pts[0].x);  // 5

// $[n]@!T -- non-owning pointer slots laid out in the struct
type Ring { $[4]@!Point nodes; };
Point p;  99 -> p.x;
Ring r;
@!p -> r.nodes[0];
printf("%ld\n", r.nodes[0].x);   // 99
42 -> r.nodes[0].x;              // write-through (mutable only)
printf("%ld\n", p.x);            // 42
```

**`$T` — inline embedding**

```palan
type Point { int64 x; int64 y; };
type Line  { $Point a; $Point b; };

Line l;
10 -> l.a.x;  20 -> l.a.y;
printf("%ld %ld\n", l.a.x, l.a.y);   // 10 20
```

`$Point` fields are stored directly inside `Line`'s memory block. No separate allocation.
Because nothing allocates on its behalf, a `$T` field (and a `$[n]$T` field) requires that `T`
owns no fields (no `U field` or `[k]U field`); use `T field` instead.
A `$T` field can be read as a struct value (`l.a -> p;`, `Point p = l.a;`), which copies it.

**`T` — owned struct pointer**

```palan
type Point { int64 x; int64 y; };
type Rect  { Point tl; Point br; };

Rect r;
10 -> r.tl.x;  20 -> r.tl.y;
printf("%ld %ld\n", r.tl.x, r.tl.y);   // 10 20
```

`r` and its `tl`/`br` sub-structs are all automatically freed at scope exit.

**`@T` — non-owning read-only pointer**

```palan
type Node { int64 val; @Node next; };

Node n1;  Node n2;
42 -> n1.val;  100 -> n2.val;
@n2 -> n1.next;                         // set pointer value: OK
printf("%ld %ld\n", n1.val, n1.next.val);   // 42 100
```

`@T` allows reading through the pointer but not writing. Use `@!T` for write-through:

```palan
type Node { int64 val; @!Node next; };

Node n1;  Node n2;
@!n2 -> n1.next;
42 -> n1.next.val;                      // write through mutable pointer: OK
printf("%ld\n", n2.val);               // 42
```

### Struct Types in Function Signatures

A struct-type parameter owns its argument, as a `[n]T` parameter does: the caller passes a copy
(a struct a call returns is handed over without one), and the function frees it when it returns,
unless it returns it or gives it away with `->>`. Writing to it does not change the caller's
struct:

```palan
func shifted(Point p) -> Point {
    p.x + 1 -> p.x;
    return p;
}

Point pt;
Point q = shifted(pt);       // pt.x stays 0; q.x is 1
```

A `@T`/`@!T` parameter borrows a struct, and the caller writes `@s` (read-only) or `@!s`
(mutable), as with a [borrowed array](#borrowing-arrays):

```palan
func moveX(@!Point p, int64 dx) {
    p.x + dx -> p.x;
}

Point pt;
moveX(@!pt, 3);
```

A struct given by name where a `@T`/`@!T` is expected — a parameter, a variable's initializer, an
assignment, or a pointer field or slot — is a compile error. `@`/`@!` works on a struct variable
(including a struct-type parameter), an owned (`T`) or embedded (`$T`) struct field, and a struct
array element (`@!pts[1]`). A `@T`/`@!T` pointer itself is already a borrow and is passed by name; `@` on it is a compile error.

`q -> p` copies struct `q` into `p`, including what `q` owns; see [Copying](#copying--).

A named return of struct type transfers ownership to the caller:

```palan
func makePoint(int64 x, int64 y) -> Point p {
    x -> p.x;  y -> p.y;
}
```

The caller receives the returned struct with `->` (`Point pt; makePoint(1, 2) -> pt;`), or as a
variable's initializer (`Point pt = makePoint(1, 2);`), where the variable takes it over without a
copy. Passed
directly as an argument (`getX(makePoint(1, 2))`, or `moveX(@!makePoint(1, 2), 3)` for a `@T`/`@!T`
parameter, written with `@`/`@!` as for a variable), it is freed when the statement has been
evaluated — in an `if`/`while` condition or a `&&`/`||` operand, when that operand has; called as a
statement on its own, it is freed at once. Binding it to a `@T`/`@!T` — a variable's initializer,
an assignment, a pointer field or slot, or a return — is a compile error, since nothing would
own it. So is binding a pointer returned by a call that was given such a struct
(`@vec3 p = scale(@!sub(@a, @b), 2);`), since that pointer may point to it.

Returning a struct the function doesn't own — a parameter, a field, or an array element — returns
a copy of it (including what it owns), which the caller owns.

### Conversion from a Literal

A function named like a struct type, returning it and callable with a literal alone (its other
parameters have [defaults](#default-arguments) or are [variadic](#variadic-arguments)), converts a
string, integer or floating-point literal to that type wherever a value of the type, or a `@T` of
it, is expected:

```palan
type name { [+]uint8 bytes; };
func name(@[]uint8 cs) -> name n { ... }
func greet(@name who) { ... }

name a = "Ann";          // name a = name("Ann");
"Bob" -> a;              // name("Bob") -> a;
greet("Cy");             // greet(@name("Cy"));

type big { [+]uint32 digits; };
func big(int64 n) -> big b { ... }
func big(flo64 f) -> big b { ... }

big x = 12;              // big x = big(12);
big y = 1.5;             // big y = big(1.5);
```

- It happens in a variable's initializer, the source of `->`, an argument (a variadic one too),
  `return`, an element of an array literal, a field of a struct literal, and an operand of an
  [operator function](#operator-functions) (`"Ann" == a`). It is the call written there: `T`
  receives the returned struct as from any call, and a `@T` argument or operand borrows it until
  the statement has been evaluated. Binding it to a `@T` variable, field or return is a compile
  error, since nothing would own it: declare a `T` variable initialized with the literal and
  borrow that.
- A [const](#constant-declarations) or a constant expression converts as the literal it folds to,
  computed in the literal's default type (`int64`, `uint64` or `flo64`): `big x = 12 * 3;` is
  `big(36)`. Any other value is converted by calling the function (`name(getenv("HOME"))`,
  `big(n * 3)`), and so is `true`, an enum value or a character literal, which have types of their
  own. Nothing converts for a `@!T`, and `"Ann" as name` is a compile error.
- The functions are those of the name visible at the literal, including those of a module
  imported with an alias (`import "names.pa" as N;` converts for `N.name`). Among those callable
  with the literal alone, one is chosen as for an [overloaded](#overloading) call: `big(int64)` for
  `12` and `big(flo64)` for `1.5` above. If none ranks first, it is a compile error.
- In an [overloaded](#overloading) call, a candidate needing a conversion is considered only when
  no other fits.

### Declaration Order

A struct, enum, type alias, or const may be used anywhere in the scope that declares it, before
its declaration too, and so may the C types of a top-level `cinclude`. One declared in a block or
function body is not visible outside it; neither are the types an `import` inside a block brings
in.

Declaring a type or const with the name of one visible from an outer scope, or twice in the same
block, is a compile error; separate blocks may reuse a name. So is declaring a type with the name
of a type a cincluded header introduces (a struct, union, or enum tag, or a typedef name).

```palan
type Shape { $Pos at; @Shape next; };   // Pos is declared below
type Pos { flo64 x; flo64 y; };
```

A type must not contain itself. Embedding or owning in a cycle (`type A { $B b; };` with
`type B { $A a; };`, possibly across files that import each other) or an alias cycle is a
compile error. A pointer field (`@T`/`@!T`) may close a cycle.

### Restrictions

- An unsized array (`[]T field`) is not supported as a field.
- Recursive embedding (`type A { $A a; }`) is a compile error.
- An [incomplete struct type](#incomplete-struct-types-opaque-handles) — a tag whose layout
  isn't known, e.g. a cincluded `DIR` — cannot be used as a by-value variable (`DIR d;`), an
  owned or embedded array (`[n]DIR`/`[n]$DIR`), an embedded field (`$DIR field;`), or an
  array-of-struct element (`arr[i]`/`p[i]` subscripting). It is usable only through a pointer
  (`@T`/`@!T`) — as a local variable, function parameter, or return type.

---

## 10. Address-Of Operator

Take the address of a local primitive-typed variable with `@` (read-only) or `@!` (mutable):

```palan
int64 x = 42;
@!int64 p = @!x;   // p now holds the address of x
```

- `@ID` yields a read-only pointer to `ID`'s storage; `@!ID` yields a mutable pointer. The
  compiler enforces this: writing through a `@ID` pointer, or assigning/passing one where a
  `@!`-typed (mutable) destination is expected, is a compile error.
- `ID` may itself already be a pointer to a primitive (`@T`/`@!T`); `@`/`@!` on it then yields a
  pointer to that pointer's own storage slot, matching the C `T **` out-param idiom used by
  functions like `strtol`:

  ```palan
  cinclude <stdlib.h>;

  @!int8 end;
  int64 v = strtol("42abc", @!end, 10);   // strtol writes end's own storage: end now points at "abc"
  printf("%ld %s\n", v, end);             // 42 abc
  ```
- `@` also takes the address of a primitive-typed struct field reached from a local variable
  (`@s.x`, including through a chain of fields — `@!s.inner.x` — and through pointer-typed
  fields — `@!p.next.val`):

  ```palan
  type Point { int64 x; int64 y; };
  Point s;
  memcpy(@!s.x, @src, 8);   // out-param write into s.x, same as memcpy(@!x, ...) on a plain variable
  ```

  The same read-only/mutable rule applies at every step of the chain: `@!` on a field reached
  through a read-only pointer (a `@T`-typed base variable, or a `@T`-typed pointer field along
  the way) is a compile error, not silently downgraded to a read-only address.
- `@` also takes the address of an embedded-struct field (`$T`) reached from a local variable
  (`@!s.inner`, C's `&st.st_atim` idiom) — the result is a plain pointer to the inner struct
  (`@!Inner`), the same shape a struct-typed local variable already has, not a pointer to a
  pointer:

  ```palan
  cinclude <sys/stat.h>;

  stat st;
  stat("/", @!st);
  @!timespec atim = @!st.st_atim;   // pointer to the embedded timespec field
  printf("%ld\n", atim.tv_sec);
  ```
- `@` also takes the address of a primitive-typed array element (`@arr[2]`, `@!arr[2]`),
  including an element of a fixed-size array field (`@!s.data[2]`) and the innermost element of a
  multi-dimensional array (`@!mat[0][1]`). A row (`@!mat[0]`) and an array field (`@!s.data`) are
  borrowed as an array (see Borrowing Arrays), which also works where a C function takes a pointer to its elements:

  ```palan
  [4]int64 arr;
  memcpy(@!arr[2], @src, 8);   // out-param write into arr[2]
  ```

  The same write-permission rule applies: `@!arr[i]` requires write permission on `arr` itself, so
  taking a mutable address through a read-only array (or a read-only array field reached through a
  `@T`-typed pointer) is a compile error.
- The resulting pointer can be handed to a cincluded C function that expects a pointer parameter
  — as an input the function reads through (`const T*`), or as an out-param it writes into:

  ```palan
  cinclude <time.h>;
  cinclude <stdio.h>;

  time_t t = int64(0);
  printf("%s", ctime(@t));                      // read-only: ctime takes const time_t*

  int32 clk_id = 0;
  @!int32 p = @!clk_id;
  int32 rc = clock_getcpuclockid(int32(0), p);   // mutable out-param
  printf("%d\n", rc == int32(0));
  printf("%d\n", p[0]);                          // read the C-written value back — see below
  ```

  The read-only/mutable rule extends to C parameters too: a `@T` (read-only) value may only be
  passed where the C parameter is `const`-qualified (as `ctime`'s `const time_t*` is above);
  passing `@T` to a non-`const` parameter — an out-param like `clock_getcpuclockid`'s second
  argument — is a compile error, same as passing `@T` where a Palan function expects `@!T`.
- For a pointer to a pointer, the rule applies at every level, and a level may narrow `@!` to
  `@` only while every level above it is `@`: `@!@!int32` binds to `@@int32` but not to
  `@!@int32` — through the latter, a read-only pointer could be stored where the original still
  treats it as mutable. This matches C, where `int **` converts to `const int * const *` but not
  to `const int **`.
- It can also be dereferenced from Palan code itself, using `p[i]` — subscript notation, not a
  separate operator, since Palan already represents an array as a pointer plus attributes and a
  plain pointer as a bare pointer. `p[0]` reads or writes the pointee; `@T` (read-only) allows only
  the read, `@!T` (mutable) allows both — the same rule as writing a struct field through a
  pointer. `p[i]` for `i != 0` is ordinary C-style pointer arithmetic (the element at `i` element-
  widths past `p`) with no bounds check — as in C, staying within the bounds of what `p` actually
  points to is the programmer's responsibility, not something the compiler verifies.

  ```palan
  int64 x = 42;
  @!int64 p = @!x;
  99 -> p[0];        // writes through p — x is now 99
  int64 y = p[0];     // reads through p — y is 99
  ```

  When the pointee is itself a struct (`@T`/`@!T` where `T` is a struct type), Palan has no
  register-sized representation of a struct value — every struct-typed expression is already a
  pointer — so `p[i]` computes an address rather than loading a value. Access its fields with
  `p[i].field`, exactly as with `p.field`:

  ```palan
  cinclude <time.h>;

  time_t t = int64(0);
  @!tm p = gmtime(@t);
  1972 -> p[0].tm_year;
  printf("%d\n", p.tm_year);   // p[0] and p name the same pointee — prints 1972
  ```

  `p[i]` names the struct itself, like an element of an `[n]$T` array: `w -> p[0]` copies the
  struct `w` into the pointee (only through `@!T`), and `@p[i]`/`@!p[i]` borrows the element.
- The pointer returned by `@`/`@!` is a borrowed reference: taking it never allocates or frees
  anything, and the storage it points into keeps its own ownership and free timing unchanged.
  Nothing checks that the pointer does not outlive that storage — if it escapes the scope that
  owns the storage (e.g. assigned to a variable declared in an outer block), reading through it
  after the storage is freed is undefined behavior, not a compile error. See `doc/Issues.md`
  for a worked example. For a stack (`$`) variable this includes returning such a pointer
  from the function: the frame it points into is gone once the function returns.

### Restrictions

`@`/`@!` produces a pointer to one storage slot: a primitive value, or a pointer to a primitive
(pointer-to-pointer). A struct or array variable is the exception: it is already its own pointer
to its storage, so `@s`/`@!s` and `@arr`/`@!arr` borrow it as it is (see
[Struct types in function signatures](#struct-types-in-function-signatures) and
[Borrowing Arrays](#borrowing-arrays)).

- Not usable on function parameters, except a struct-type parameter.
- Not usable on a borrow: a struct pointer (`@T`/`@!T` where `T` is a struct) or a borrowed array
  (`@[n]T`, `@[+]T`) is passed by name.
- On a local variable, usable only when the variable is primitive-typed, itself a pointer to a
  primitive (`@T`/`@!T`), a struct, or an array with primitive, struct, or `@T`/`@!T` pointer
  elements.
- On a struct field reached from a local variable, usable only when the leaf field is
  primitive-typed, an embedded struct (`$T`), or an owned struct (`T`) — a pointer-typed field
  (`@T`/`@!T`) or an array field is rejected.
- Not usable on a 2D array row (`@mat[i]`) or a pointer-slot array element (`[n]@T`/`[n]@!T`) —
  only a primitive-typed or struct array element.
- Not usable on a general expression (a parenthesized tuple, etc.) — only a local variable, a
  field reached from one, or an array element reached from one. A call returning a struct is the
  exception, as a call argument only (`f(@g())`); see
  [Struct types in function signatures](#struct-types-in-function-signatures).

---

## 11. Modules and Program Structure

Palan supports multi-file compilation. Functions and types declared with `export` are visible to other files that `import` the declaring file.

### Exporting a Function

```palan
// lib_add.pa
export func add(int32 a, int32 b) -> int32 {
    return a + b;
}
```

Functions without `export` are file-private and not accessible from other files.

### Exporting a Function to C

`export "C"` also lets C code linked into the program (see [Link Libraries](#link-libraries))
call the function by its name:

```palan
// scale.pa
cinclude "scale.h" link "scale.o";   // scale.h: int32_t scaled(int32_t v); int32_t twice_scaled(int32_t v);
export "C" func scaled(int32 v) -> int32 { return v * 10; }
```

```c
// scale.c
#include "scale.h"
int32_t twice_scaled(int32_t v) { return 2 * scaled(v); }
```

- The C name is the function's name, so it cannot be overloaded in its file. Its signature follows
  the rules of a [callback](#passing-a-palan-function-as-a-callback): no several return values,
  no variadic parameter, no default arguments, and it cannot be an operator function.
- Other Palan files import it as any exported function, and a library's interface writes it as
  `export "C" func scaled(int32 v) -> int32;`.
- A C declaration of the same name, such as one in the header the C code includes, is the same
  function: a call takes the Palan function's types. It is not checked against them.
- A name a C library already defines, such as `puts`, is not reported, and may replace that
  function for the whole program.

### Importing a File

```palan
// main.pa
cinclude <stdio.h>;
import "lib_add.pa";

printf("%d\n", add(3, 4));   // 7
```

- The path in `import` is relative to the importing file.
- Imported functions are visible from the `import` statement to the end of the enclosing scope.
- Block-scoped `import` (inside `{ }`) makes the imported functions visible only within that block.
- Circular imports (A imports B and B imports A) are supported.

### Selective Import

Import only named functions from a file:

```palan
cinclude <stdio.h>;
import square from "lib_math.pa";

printf("%ld\n", square(4));   // 16  (cube is not imported)
```

Multiple names can be listed with commas: `import square, cube from "lib_math.pa"`. An
[operator function](#operator-functions) is listed by its operator: `import vec3, +, - from "vec3.pa"`.

### Alias Import

Import all exports under a namespace prefix to avoid name conflicts:

```palan
cinclude <stdio.h>;
import "lib_math.pa" as L;

printf("%ld\n", L.square(3));  // 9
printf("%ld\n", L.cube(2));    // 8
```

- All calls must use the `L.f()` qualified form. Unqualified `square()` is a compile error.
- Operator functions apply unqualified (`a + b`), since an operator cannot be written with `L.`.
- A variable named `L` hides the alias: `L.f()` is then a method-form call on the variable (see §7).

### Selective Alias Import

Import specific functions under a namespace prefix:

```palan
cinclude <stdio.h>;
import square from "lib_math.pa" as L;

printf("%ld\n", L.square(5));  // 25
```

### Exporting Types

A struct, enum, or type alias declared with `export type` can be imported like a function:

```palan
// vec3.pa
export type vec3 { flo32 x; flo32 y; flo32 z; };
export type Color enum { Red, Green };
export type Row = [3]vec3;
export func len2(@vec3 v) -> flo32 {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}
```

```palan
// main.pa
import "vec3.pa";
vec3 p;
Color c = Color.Green;
flo32 l = len2(@p);
```

- An exported type is visible in the whole importing file, like a type declared in it. A
  type imported inside a block is visible from the `import` on.
- Under an alias import, write the type with the alias: `V.vec3`, `V.Color.Green`,
  `V.Color(1)`. The unqualified name is a compile error.
- A selective import lists types alongside functions: `import vec3, len2 from "vec3.pa";`.
- An exported type may use the declaring file's non-exported types, consts, and cincluded
  headers; those are not made nameable in the importing file.
- Two files may export types of the same name; a file importing both writes at least one of
  them with its alias (`import "b.pa" as B;` and `B.Point`).
- A file may declare a type with the name of an imported type it cannot write unqualified
  (one imported as `V.T`, or only brought along by another imported type). Declaring a type
  with a name an import makes writable is a compile error.
- Two files may import each other and use each other's types anywhere, including in type
  declarations, as long as no type contains itself (see [Declaration order](#declaration-order)).

### Hidden and Read-only Fields

A field of an exported struct may start with `private` (not visible outside the file declaring the
struct) or `readonly` (read only outside it). Unmarked fields are public. Inside the file, every
field is used as usual.

```palan
// counter.pa
export type Counter {
    private [+]int64 log;
    readonly int64 count = 0;
};
export func add(@!Counter c, int64 v) { c.log.push(v); c.count + 1 -> c.count; }
```

```palan
// main.pa
import "counter.pa";
Counter c;
c.add(3);
int64 n = c.count;     // reading is fine
3 -> c.count;          // compile error: read-only outside counter.pa
int64 x = c.log[0];    // compile error: private
```

- Outside the file, a `readonly` field and whatever is reached through it are read only: `->`
  into it, `@!c.count`, a method-form call or `push` on it, `<<=` out of it, and writing an
  element, a field or a pointee through it are compile errors. A `readonly` pointer field is read
  as a read-only pointer (`@T`). To let other files change it, the declaring file provides a function.
- Operations on the whole value are allowed outside: copying, moving, `->`, passing `c` or `@!c`.
- A struct literal written outside sets only public fields, by name: naming a hidden field, or a
  positional literal of a struct having one, is a compile error. A hidden field without an
  [initializer](#field-initializers) thus makes such a literal impossible outside.
- `private` and `readonly` are not reserved words: they are names everywhere else. On a field of a
  struct that is not exported, they are a compile error.
- A library's interface lists hidden fields with their marks, since the struct's layout needs them.

### Opaque Types

`type Name = opaque T;` declares a type of its own holding a value of `T`, without letting other
files see what `T` is. Only a file that can write `T` reaches the contents:

```palan
// file.pa
cinclude <stdio.h>;
type Conn { int32 fd; [+]uint8 buf; };
export type Handle = opaque Conn;

export func open(int32 fd) -> Handle {
    Conn c;
    fd -> c.fd;
    return c as Handle;           // the owned value moves into the Handle
}
export func fd(@Handle h) -> int32 { return (h as @Conn).fd; }
export func close(Handle h) release { printf("closing %d\n", fd(@h)); }
```

```palan
// main.pa
import "file.pa";
Handle h = open(3);
int32 n = fd(@h);
Handle g <<= h;                   // moved, not copied
close(g>>);                       // released now, and not again
// h is released when its scope ends; close prints for it
```

- `T` is anything a variable owns: a struct (Palan's or C's), an array of constant sizes or a
  `[+]T`. A pointer, a `$T`, a primitive or an [incomplete C struct](#incomplete-struct-types-opaque-handles) is a compile error.
- A value is owned, borrowed and moved as a struct is (`Handle`, `@Handle`, `@!Handle`, `<<=`,
  `->>`, `>>`), passed and returned, and released when its owner is.
- Where `T` cannot be written, what needs its layout is a compile error: a declaration without an
  initializer, a copy (`Handle b = a;`, `a -> b`, passing `a` to a `Handle` parameter), `sizeof`.
  Where it can, a declaration without an initializer allocates the contents and a copy copies
  them. A field access and `$Handle` are compile errors everywhere.
- `x as Handle` and `h as T` convert an owned value between the type and its contents, in a file
  that can write `T`, and only as a move: `<<=`, `->>`, `>>` or `return` (`T c <<= h as T;`). A
  borrow is cast as any pointer is (`@h as @T`).
- A field or an element of an opaque type (`type W { Handle h; };`, `[4]Handle`, `[+]Handle`)
  starts empty, as a variable is after `>>`, and is filled by `->>` (`open(3) ->> w.h;`).
  Releasing an empty one does nothing. Copying a value holding one is not supported in this
  version, nor is `push` of one that is not moved (`v.push(h>>)`, `v.push(open(3))`).
- A function marked `release`, in the file declaring the type, taking the owned type alone and
  returning nothing, is called whenever a value of the type is released. Called as an ordinary
  function, it takes the value over (`close(g>>)`), so it is not released twice. Its own
  parameter is released as the contents when it ends. A type has at most one; without one, the
  contents are released as they would be by themselves.
- A [library](#building-a-library) may publish an opaque type whose contents are a type of another
  of its files. Its interface writes `export type Handle = opaque;`.
- `opaque` and `release` are reserved words.

### Module Names (`library`)

A module's name comes from its file alone, not from where it is or what imports it. It is the file
name without `.pa`, unless the file starts with a `library` declaration, which puts it in a
library:

```palan
// utf8.pa
library std;       // the module std.utf8
```

- `library std.io;` in `file.pa` names the module `std.io.file`.
- The declaration comes first in the file, at most once; `library` is a reserved word.
- Two modules of one name in a program are a compile error. Two `util.pa` in different
  directories meet unless they declare different libraries (`library proj.a;` and
  `library proj.b;`). A few different file names also give one name, such as `1x.pa` and `_1x.pa`.

### Building a Library

`palan --lib -o <dir> main.pa` builds a library from `main.pa` and the files it imports, each of
which must declare `library` with the same first name:

```palan
// utf8.pa
library std;
export func length([]uint8 s) -> int64 { ... }
```

This writes `<dir>/std/utf8.pai`, the interface a person reads to learn the library, and
`<dir>/std/utf8.a`, the code, for the target being built for.

- The interface holds the main file's `export`s and whatever private types and constants those
  need, including the constants and type aliases of other files. Any `export` of an imported file
  is visible only inside the library.
- A type the library publishes is declared in the main file, which the other files may import. A
  public declaration using a struct or enum of another file, directly, through a type alias or
  through a field, is a compile error; an [opaque type](#opaque-types) of the main file may hold
  one.
- The global variables of every file are initialized when a program using the library starts,
  as an imported file's are (see [Global Variables](#global-variables)). The main file's
  [exported global variables](#exporting-global-variables) are published, without their
  initializers (`export global [3]int32 primes;`).
- The interface may rename a constant or type alias of another file to a name starting with `__`
  (see [Lexical Rules](#2-lexical-rules)).

### Using a Library

`import <package/module>` imports a library's module, with the same forms as importing a file.
`libpath` says where to look, relative to the file writing it:

```palan
libpath "../libs";
import <geo/vec>;          // reads ../libs/geo/vec.pai, links ../libs/geo/vec.a

Vec a = vec(3.0, 4.0);
```

- The library's sources are not needed, only its `.pai` and `.a`.
- A `libpath` may be written in any file and adds a place to look for the whole program.
- `std` is found with the compiler, and a `libpath` cannot replace it. Its modules are described
  in the [Palan Standard Library Reference](PalanStdLibReference.md).
- A package found under two `libpath`s is an error, as is a library built by another version of
  Palan or for another target.
- A library that imports another needs that one found too, through the program's `libpath`s.

### Program Structure

```palan
cinclude <stdio.h>;

int64 x = 10;
printf("%lld\n", x);
```

- Top-level statements run when the program starts, only in the file being built, after the
  [global variables](#global-variables) of every file are initialized. The top-level statements
  of an imported file are not executed, so a module can hold its own tests or usage examples.
- The top-level statements see the program's command-line arguments as `args`, a read-only
  borrowed variable-length array of C strings (`@[+]@[]uint8`). `args[0]` is the program's
  path, and `len(args)` counts it. `args` is a borrow, so it is used by name: `len(args)`,
  `args[i]`, `for copy a in args`, `f(args)` for a `@[+]@[]uint8` parameter. Functions do not see it
  unless it is passed, and declaring another `args` at the top level is a compile error.
  Every module has its own, so an imported module's top-level demo can read the arguments when
  it is the file being built.

  ```palan
  cinclude <stdio.h>;

  for copy a, i in args {
      if i > 0 { printf("%s\n", a); }
  }
  ```

  `palan echo.pa x y` prints `x` and `y`.
- A top-level variable is visible to the top-level statements after it, but not inside
  functions, unless it is a [global variable](#global-variables).
- User-defined functions can be placed before or after top-level statements.
- `return` is not valid at top-level. To exit early, call `exit()` from `<stdlib.h>`.

### Global Variables

```palan
cinclude <stdlib.h>;

global int32 calls = 0 for cmp!, report;
global int32 dir = 1;
global dir for cmp;

func cmp(@int32 a, @int32 b) -> int32 r {
    calls + 1 -> calls;
    (a[0] - b[0]) * dir -> r;
}
```

- A top-level variable declared with `global` can be used by the functions its `for` list
  names, an [operator function](#operator-functions) by its operator (`for +`). A function named
  alone may only read the variable; with `!` after its name it may also write it. A `global` statement without a type adds functions to the list later
  (`global dir for cmp;`). Functions not listed do not see the variable, so they may use its
  name for their own parameters and variables; a listed function may not.
- This is how a function C calls with fixed arguments, such as a `qsort` comparator or a libuv
  callback, gets at what it needs.
- `global` is a reserved word. It is written only at the top level of a file, declares one
  variable, and lists only functions defined at the top level of the same file (a name covers all
  its overloads).
- A listed function sees an array or struct variable as it would a `@` parameter (`@!` with `!`):
  it reads (or writes) the elements and fields, but cannot replace the variable itself. A
  number or pointer variable it can assign only with `!`; what a pointer points to keeps the
  pointer's own type (`@!T` stays writable through).
- Global variables are initialized when the program starts, before any top-level statement
  runs, in every file of the program: each file once, the files it imports first, then its own
  in declaration order. So a listed function works wherever it is called, even from a top-level
  statement before the declaration.
- An initializer cannot use an ordinary top-level variable or `args`, which do not exist yet
  (a compile error). It can use constants, functions and the global variables declared before it.
- When files import each other in a circle, they are initialized in import order, and a function
  reached from one may see another's global variables not yet initialized (zero, or an empty owner).
- To build the state when the user chooses, declare the variable without an initializer and
  export a function that sets it:

  ```palan
  type Config { int32 level; };
  global Config config for configure!;
  export func configure(int32 level) { level -> config.level; }
  ```

- What a global variable owns lives until the program ends; it is not freed at the end of the
  top level. A value it is reassigned away from at the top level is freed as usual.

### Exporting Global Variables

`export global` lets the files importing this one use the variable as their own global
variable:

```palan
// counter.pa
export global int64 count = 0 for inc!;
export func inc() { count + 1 -> count; }
```

```palan
// main.pa
cinclude <stdio.h>;
import "counter.pa";
global count for report;

func report() { printf("%ld\n", count); }

inc();
count + 10 -> count;   // the top-level statements read and write it freely
report();              // 11
```

- In the importing file, the top-level statements use it freely, and a `global` statement there
  lets its functions read it, or with `!` write it, as for its own global variables.
- Under an alias import, write it with the alias: `C.count`, and `global C.count for report;`.
  A variable named `C` hides the alias.
- A selective import names it like a function: `import count from "counter.pa";`.
- An `import` inside a function lets that function read and write it, without a `global`
  statement. Inside a block, it is visible only in the block.
- An imported variable whose name the file already declares, or which two unaliased imports both
  export, is a compile error; import one of them with an alias.
- The sizes of an exported array must be constants or come from its array literal
  (`export global []int32 primes = [2, 3, 5];`); a size known only at run time is a compile error.
- To keep a variable from being written by other files, do not export it; export functions that
  read or change it.

---

## 12. C Library Integration

```palan
cinclude <stdio.h>;          // system header — functions visible as unqualified calls
cinclude "myheader.h";       // local header
printf("%ld\n", x);          // call C function directly

cinclude <stdio.h> as S;     // alias — functions accessible only as S.xxx()
S.printf("%d\n", 42);        // qualified call

cinclude <math.h> link "m";  // link against libm (-lm) when building
```

### Function Calls

- `cinclude` makes C functions visible from the declaration point to the end of the enclosing scope.
- A C function whose signature uses a C type Palan cannot represent (`long double`, `va_list`,
  `__int128`, a `_Complex` type) can be cincluded, but calling it is a compile error. `tgmath.h`
  cannot be cincluded: it requires a GNU compiler.
- With an alias, functions are accessible only via the qualified form `alias.funcName(...)`.
- The number of arguments must match the number of parameters; a variadic C function takes at least
  its fixed parameters. A C function declared with empty parentheses `f()` takes no arguments, the
  same as `f(void)`.
- A macro that names another declared function (glibc's `#define sscanf __isoc99_sscanf`) makes
  its name call that function, as in C. So `sscanf` has the C99 behavior, and `strerror_r` and
  `libgen.h`'s `basename` are the XPG versions.

### Link Libraries

- Some C headers declare functions that live outside libc, in a separate shared library — `math.h`'s
  `sqrt`/`pow`/`sin`/etc, for instance, are only in libm. A `link` clause on `cinclude` names the
  library to add to the final link step: `cinclude <math.h> link "m";` appends `-lm` to the `ld`
  invocation, the same as passing `-lm` on a C compiler's command line. Palan does not keep a
  built-in header-to-library lookup table — every third-party library must be named explicitly with
  `link`, since a lookup table covering every header a program might ever cinclude would itself need
  ongoing maintenance.
- Multiple libraries in one clause are comma-separated: `cinclude <stdio.h> link "rt", "m";`. The
  `link` clause comes last, after an optional `as` alias.
- A library name may contain only letters, digits, `_`, `.`, `+` and `-`, and may not be empty — it
  is appended directly after `-l` when invoking the linker, so this is checked at compile time
  rather than left to fail at the `ld` step.
- Library names are collected across the whole program, not scoped to the file or block where
  `link` appears: a `link` clause inside a function body or `{ }` block, or one that reaches the
  final binary only through an `import`ed module, still ends up on the final `ld` command line. The
  same library named more than once (in one `link` clause, in different clauses, or across
  different modules) is deduplicated.
- A header may be cincluded with `link` in one place and without it elsewhere in the same program —
  the library is added to the link step regardless of which cincluding site names it.
- A name ending in `.o` is an object file to link instead of a library, such as one compiled from
  your own C source: `cinclude "util.h" link "util.o";`. A relative path is relative to the
  directory of the source file, as a local `cinclude` header is. A missing file is a compile error.
  Object files and libraries can be mixed in one clause (`link "m", "lib/util.o"`), and object
  files are collected and deduplicated across the whole program in the same way.
- `link` is not a reserved word — it is only recognized as introducing this clause immediately after
  a `cinclude` path (and optional `as` alias). `unistd.h`'s `link()` function, for example, remains
  callable as an ordinary C function.

### Exporting C Types

`cinclude <stdio.h> export FILE, fpos_t;` names the C types a module's public declarations use, for
building it as a library (see [Building a Library](#building-a-library)).

- The module still uses the whole header, so `printf` works as it does without the clause.
- A program using the library needs the header too. A `cinclude` without the clause, used only in
  function bodies, asks nothing of it.
- The program can write `FILE` only after a `cinclude` of its own.
- The clause comes before `link`: `cinclude <uv.h> export uv_loop_t link "uv";`.
- It is accepted only on a top-level `cinclude`; in a block or a function it is a compile error.
- An ordinary `import` of a source file does not read the clause.
- A local header (`cinclude "x.h" export T;`) is not supported yet when the library's public
  declarations use the type: its reader would not find the header, so `--lib` reports an error.

### Typedefs

- A `typedef` a cincluded C header defines is registered as soon as the header is cincluded,
  whether or not any C function or global in that same header references it. For example,
  `cinclude <stdint.h>; int32_t x = 5;` works even though `stdint.h` declares no C functions at
  all — every typedef is registered, not only those a function signature uses. `size_t n = strlen(s);` after `cinclude <string.h>;` works the same way, just less
  visibly, since `string.h` also declares functions.
- Typedefs that resolve to a primitive type are registered as a Palan type alias (see
  [Type Aliases](#type-aliases)). Typedefs that resolve to a struct (e.g. `FILE`, `typedef struct
  _IO_FILE FILE;`, including a multi-level chain) are registered and usable the same way as a
  native struct type — see [Incomplete Struct Types](#incomplete-struct-types-opaque-handles) below
  for the case where the struct's own layout isn't known. This includes a struct body
  with no tag of its own (`typedef struct { int quot; int rem; } div_t;`) — the typedef's own name
  is used as the struct's tag, so `div_t`/`ldiv_t`/`lldiv_t` (from `stdlib.h`'s `div`/`ldiv`/`lldiv`)
  are usable struct types exactly like a tagged one.
- Typedefs that bottom out in a pointer type are usable as a pointer type alias: `@!T` when the
  pointee is not `const`, `@T` when it is (`typedef void *timer_t;` is `@!void`, `typedef const
  char *CS;` is `@int8`). A typedef of a function pointer is a [function pointer
  type](#function-pointers) (`__compar_fn_t` is `@func(@void, @void) -> int32`).
- Typedefs that bottom out in a union are usable the same way as struct typedefs (see
  [Union Types](#union-types) below). A typedef of an enum is another name for it (see
  [C Enum Types](#c-enum-types) below).
- A header may use a typedef from a header cincluded before it, as in C where both are included
  into one translation unit: `cinclude <stdio.h>; cinclude <jpeglib.h>;` lets `jpeglib.h`'s
  declarations use `size_t` and `FILE`. Typedefs that bottom out in a pointer type are not carried
  over this way.
- Macros carry over the same way: a header sees every macro the headers cincluded before it in the
  same or an enclosing scope defined or `#undef`ed, so its `#ifdef`s and macro uses work as in C.
  A header that such a cinclude already read is skipped by its include guard, as in C, and its
  declarations come from that earlier cinclude. What a block's cincludes carry over ends with the
  block: a later `cinclude` of the same header outside it reads the header again.
- If multiple cincluded headers introduce the same typedef name resolving to the *same* underlying
  type, the first registration silently wins. If they resolve to *different* underlying types, this
  is a compile error.
- Aliased cinclude (`cinclude <x.h> as X;`) does not namespace imported typedefs — they are
  always registered globally. Only C function calls require the `X.` qualifier.

### Macro Constants

- An object-like `#define` macro whose body folds down to a single compile-time integer value is
  usable by name from Palan the moment the header is cincluded — not as an imported symbol, but
  as a compile-time text substitution: every later reference to the name is replaced with the
  folded literal value, the same way the C preprocessor itself would substitute it. This covers a
  bare integer literal, a pointer-cast of one (e.g. `#define NULL ((void *)0)`), and an
  arithmetic or bitwise expression built from `| & ^ << >> + - * / %` over such forms (e.g.
  `sys/stat.h`'s `S_IRWXU`, defined as `(S_IREAD|S_IWRITE|S_IEXEC)`) — including one that
  references another already-defined macro (`#define S_IFDIR __S_IFDIR`). An operand may be
  C's `sizeof(type)`, which folds to the type's size as `size_t` when its layout is known (e.g.
  `sys/select.h`'s `__NFDBITS`, `(8 * (int) sizeof (__fd_mask))`). The same folding sizes a C
  struct's array fields, so `fd_set` is a complete struct and `select` can be called with it.
- A macro whose body doesn't fold this way — a function-like macro referenced without a call
  (e.g. `S_ISDIR`, which takes an argument), a string literal, a relational/equality/logical/
  ternary expression, or a reference to an identifier that never resolves — is left untouched.
  Referencing such a name from Palan is a compile error naming the macro:

  ```palan
  cinclude <sys/stat.h>;
  stat st;
  stat("/etc", @!st);
  if (S_ISDIR(st.st_mode)) { ... }   // error: C macro 'S_ISDIR' is function-like; Palan cannot use it.
  ```

  `S_IFMT`/`S_IFDIR` and similar bare constants *do* fold, so the same check can be written
  directly with a bitwise AND (see [Expressions](#5-expressions)):

  ```palan
  if ((st.st_mode & S_IFMT) == S_IFDIR) { ... }   // works
  ```

  ```palan
  cinclude <string.h>;
  if (strchr(s, 'x') == NULL) {
      // not found
  }
  ```

  `NULL` is a generic pointer value: it is compatible with and can be compared (`==`/`!=`)
  against any pointer-typed value, including C function return values, `[]T` array
  pointers, and struct pointer fields.
- A macro constant whose type C leaves open — a plain number such as `#define COLOR_RED 1` or
  `#define ERR (-1)`, or arithmetic over plain numbers like `S_IRWXU` — behaves exactly like an
  integer literal written in Palan: it takes the type its context expects and must fit in it. So
  `st.st_mode & S_IFMT` is `uint32` because `S_IFMT` takes `st_mode`'s type, and ncurses'
  `init_pair(1, COLOR_RED, COLOR_BLACK)` passes the constants to `short` parameters without a
  cast.
- A macro constant whose type C fixes — an unsigned or long suffix (`1U`, `5L`), `sizeof`, or an
  integer cast, as in ncurses' `A_REVERSE` (`unsigned int`) — keeps that type, and meets other operands
  through the [usual arithmetic conversions](#3-type-system) like any typed value.
- A reference resolves against the last cincluded header that defines the name, textually
  before that reference, up to the end of the block the `cinclude` appears in. A reference that
  textually precedes every cincluding `cinclude` is left unresolved (`Undefined function`/`Undefined
  variable`), even if a later `cinclude` in the same file would otherwise define it.
- A later cincluded header may redefine or `#undef` a macro, as in C. References after that
  `cinclude` see the new value, or no macro at all.
- A macro constant silently takes priority over any same-named variable, parameter, or `const` in
  scope at the reference site, undiagnosed.
- Aliased cinclude (`cinclude <x.h> as X;`) does not namespace macro constants — they fold the
  same regardless of alias. Only C function calls require the `X.` qualifier.
- A macro constant is not visible across an `import`/`export` boundary — it is purely a
  same-file, parse-time substitution. A value built from one (e.g. an `export syscall`'s number,
  see [Raw Syscalls](#13-raw-syscalls)) is already a plain literal by the time it could be
  exported, so the folded *value* does cross that boundary even though the macro *name* never
  does.

### C Global Variables

- A file-scope `extern` object declared in a cincluded header, whose type is a primitive, an enum
  or a pointer (e.g. `extern FILE *stdout;`), is captured as a readable Palan variable of the same
  name, visible from the cinclude point to the end of the enclosing scope — the same rule as a
  cincluded function. `static` declarations, block-scope declarations, and arrays or by-value
  struct/union globals are not captured.

  ```palan
  cinclude <stdio.h>;
  fprintf(stderr, "starting up\n");   // stderr resolves to the C global
  ```

- **Written only with permission**: a C global can be read anywhere it is visible, but writing
  it needs a `global` statement, the same one that lists the functions of a
  [global variable](#global-variables): `global optind;` lets the top-level statements after it
  write `optind`, and `global optind for parse!;` lets `parse` write it too. Without one,
  assigning to it (`x -> stdout`) or taking a writable address (`@!optind`) is a compile error.
  This restricts the variable itself, not what it points to — the *pointee* keeps its own
  C-declared mutability, so `fwrite(data, 1, n, stdout)` (writing through the `FILE *` value
  `stdout` holds) works normally.
- Aliased cinclude (`cinclude <x.h> as X;`) does not namespace C globals either — like typedefs
  and constants, they are always registered globally; only a function call needs the `X.`
  qualifier.

### Struct Types

- A C `struct Name { ... }` defined (with a full field list, not just forward-declared) in a
  cincluded header becomes a usable [struct type](#9-struct-types) under that same name — field
  access, embedded struct-typed fields, and struct-typed function parameters all work identically
  to a struct declared natively with `type Name { ... }`. A tag that is only forward-declared, or
  whose full definition can't be captured, is still usable under its name — see [Incomplete
  Struct Types](#incomplete-struct-types-opaque-handles) below.
- A C pointer field (`T *f;`, or a pointer array `T *f[n];`) is `@!T` when its pointee is not
  `const` and `@T` when it is (`const T *f;`), the same rule as a C function's pointer parameters.
- A function pointer field (or an array of them) is a [function pointer](#function-pointers)
  field, and `s.f(args)` calls through it. One whose signature can't be expressed as `@func(...)`
  (a variadic one, for example) keeps its place in the layout but cannot be read, written, or have
  its address taken.
- A struct pointer *returned* by a C function should be bound to a non-owning `@T`/`@!T`-typed
  local variable, not a named return — the pointer may be owned by C (e.g. a static internal
  buffer), so no automatic freeing is registered for it:

  ```palan
  cinclude <time.h>;
  cinclude <stdio.h>;

  time_t t = int64(0);
  @!tm p = gmtime(@t);                        // non-owning: gmtime owns the storage, not Palan
  printf("%d\n", int32(p.tm_year) + 1900);    // 1970
  ```

  A struct passed *into* a C call's struct pointer parameter is borrowed the same way as for a
  native `@T`/`@!T` parameter: write `@s` for a `const` pointer parameter and `@!s` otherwise
  (`mktime(@!t)`, `asctime(@t)`; see
  [Struct types in function signatures](#struct-types-in-function-signatures)).

### Structs Returned By Value

A C function that returns a struct **by value** (not by pointer) can be received directly into a
struct-typed local variable's initializer, the same syntax as a native call:

```palan
cinclude <stdlib.h>;
cinclude <stdio.h>;

div_t d = div(7, 2);
printf("%d %d\n", d.quot, d.rem);   // 3 1
```

- The variable's own storage is what the C call writes into — this is an ordinary owning struct
  local, freed automatically at scope exit like any other, not a `@T`/`@!T`-typed handle to
  something C owns (contrast with the pointer-returning case above).
- This is implemented as a general System V AMD64 return-value classification, not a
  `div_t`-specific special case: a struct of 16 bytes or less comes back in registers (`%rax`/
  `%rdx` for integer-classified eightbytes, `%xmm0`/`%xmm1` for floating-point ones, mixed if the
  struct has both), and a struct larger than 16 bytes comes back through a hidden destination
  pointer, the same ABI a C caller would use.
- Writing a call that returns a struct by value anywhere other than a struct variable's
  initializer — as a bare statement, discarding the result — is a compile error: the call still
  has to happen, but there is nowhere to write the result, so the compiler rejects it instead of
  silently dropping the call.
- Native Palan functions do not gain a struct-by-value return syntax from this — this feature is
  about receiving a C function's by-value return, not a Palan-side language addition.

### Structs Passed By Value

A C function that takes a struct **by value** is called with the struct written by name, as for a
native struct-type parameter; the function receives a copy:

```palan
cinclude <stdio.h>;

func cwrite(@!void cookie, @int8 buf, uint64 size) -> int64 n {
    printf("[%.*s]", int32(size), buf);
    int64(size) -> n;
}

cookie_io_functions_t io;
cwrite -> io.write;
@!FILE f = fopencookie(NULL, "w", io);
fprintf(f, "hello");                 // [hello]
fclose(f);
```

- The argument can be a struct variable, a struct field or array element, a struct returned by a
  call, a struct literal (`{x: 1, y: 2}`), or a `@T`/`@!T` pointer to the struct — a read-only
  `@T` is fine, since only a copy is passed. Changes the C function makes to its copy are not
  seen by the caller.
- The struct is passed the way a C caller would pass it under the System V AMD64 ABI: in
  registers when it is 16 bytes or less and its parts fit in the remaining argument registers,
  on the stack otherwise. A union is passed the same way.
- A C function that takes a struct by value cannot be used as a function pointer.

### Passing a Palan Function as a Callback

A bare Palan function name, written in a C function's argument position, is passed as that
function's address — the common C idiom of handing a callback to a library function like
`qsort`:

```palan
cinclude <stdlib.h>;
cinclude <stdio.h>;

func cmp(@int32 a, @int32 b) -> int32 { return a[0] - b[0]; }

[4]int32 arr;
5 -> arr[0];  3 -> arr[1];  1 -> arr[2];  4 -> arr[3];
qsort(arr, uint64(4), uint64(4), cmp);
printf("%d %d %d %d\n", arr[0], arr[1], arr[2], arr[3]);   // 1 3 4 5
```

- The C parameter is a [function pointer](#function-pointers) (`int (*)(const void *, const
  void *)` is `@func(@void, @void) -> int32`), and the function name is a value of that type. The
  rules for which functions match are in that section; in short, the parameter and return types
  must be the same, with no widening, since the C library calls the Palan function directly.
- `@void`/`@!void` — a pointer whose pointee type is `void` — exists to write the callback
  signatures C itself uses generically, such as `qsort`'s and `bsearch`'s `const void *`
  comparator arguments:

  ```palan
  cinclude <stdlib.h>;
  cinclude <stdio.h>;

  func cmp(@void a, @void b) -> int32 {
      @int32 x = a;   // give the void pointer a concrete type before reading through it
      @int32 y = b;
      return x[0] - y[0];
  }

  [4]int32 arr;
  1 -> arr[0];  3 -> arr[1];  4 -> arr[2];  5 -> arr[3];
  int32 key = 4;
  @!int32 found = bsearch(@key, arr, uint64(4), uint64(4), cmp);
  printf("%d\n", found[0]);   // 4

  int32 miss = 2;
  @!int32 none = bsearch(@miss, arr, uint64(4), uint64(4), cmp);
  printf("%d\n", none == NULL);   // 1 -- not found
  ```

  `void` can only appear as a pointer's pointee (`@void`/`@!void`) — a bare `void x;` variable is
  still rejected. A `@void`/`@!void` value is compatible with any other pointer type in both
  directions, so assigning it to a concretely-typed pointer variable (as `@int32 x = a;` does
  above) gives it a type to read or write through, as does a cast (`(a as @int32)[0]`). Indexing
  or dereferencing a void pointer directly (`a[0]`) is a compile error.
- A function registered with `atexit`/`at_quick_exit` must not read or write any of the calling
  scope's Palan-owned local variables: program exit frees all still-live owned locals before
  invoking the registered handlers, so by the time the handler runs, that storage is gone.

  ```palan
  cinclude <stdlib.h>;
  cinclude <stdio.h>;

  func say_bye()  { printf("bye\n"); }
  func say_last() { printf("last\n"); }

  atexit(say_bye);
  atexit(say_last);
  printf("hello\n");
  // output: hello / last / bye -- exit handlers run in LIFO order, same as C's atexit
  ```

### C Array Fields

- A fixed-size C array field (`char name[16];`) becomes an [array field](#array-fields) laid out
  in the struct, exactly like a native `$[16]int8 name;` field — element access and taking the
  address of an element both work. A field with more dimensions (`int cells[2][3];`) is the
  same as `$[2]$[3]int32 cells;`, an array of structs (`struct pt p[4];`) as `$[4]$pt p;`, and an
  array of pointers (`char *argv[4];`) as `$[4]@!int8 argv;`. A C function parameter declared
  as an array (`void take(char buf[32])`) decays to a plain pointer, same as in C — only the
  outermost dimension decays, so `char buf[2][3]` becomes a pointer to a 3-element row.

### Union Types

A C `union Name { ... }` (or `typedef union { ... } Name;`) from a cincluded header is usable under
its name everywhere a C struct is: a by-value variable declaration, `@`/`@!` address-of, field
read/write, a `$Name` field of a native struct, and passing it to a C function by pointer. All
members start at the same address, so writing one member changes what the others read, as in C;
the size is that of the largest member, rounded up to the largest member alignment.

`pthread.h`'s `pthread_mutex_t`, `pthread_cond_t` and `pthread_attr_t` are unions:

```palan
cinclude <pthread.h>;
cinclude <stdio.h>;

type Shared { int64 count; $pthread_mutex_t m; };

func worker(@!void arg) -> @!void {
    @!Shared sh = arg;
    pthread_mutex_lock(@!sh.m);
    sh.count + 1 -> sh.count;
    pthread_mutex_unlock(@!sh.m);
    return NULL;
}

Shared s;
pthread_mutex_init(@!s.m, NULL);
pthread_t t1;
pthread_t t2;
pthread_create(@!t1, NULL, worker, s);
pthread_create(@!t2, NULL, worker, s);
pthread_join(t1, NULL);
pthread_join(t2, NULL);
pthread_mutex_destroy(@!s.m);
printf("%ld\n", s.count);    // 2
```

`PTHREAD_MUTEX_INITIALIZER` and the other `*_INITIALIZER` macros expand to a brace initializer,
which Palan has no syntax for; call the matching `_init` function instead.

The fields of a C11 anonymous member, a struct or union body with no member name
(`struct S { int k; union { int a; float b; }; };`), are fields of the enclosing type:
`s.a` and `s.b` read and write the union directly. An anonymous body that has a member name
(`union { ... } u;`) is reached through that name (`s.u.a`).

### Packed and Aligned C Structs

A C struct or union declared with GCC's `__attribute__((packed))` or `__attribute__((aligned(N)))`,
on the type or on individual fields, has the same layout in Palan as in C: field offsets, size,
and the stride of an array of it all match. `sys/epoll.h`'s `struct epoll_event` is packed
(12 bytes, `data` at offset 4), so an array of it can be handed to `epoll_wait` directly:

```palan
cinclude <stdio.h>;
cinclude <unistd.h>;
cinclude <sys/epoll.h>;

[2]int32 fds;
pipe(@!fds[0]);
int32 ep = epoll_create1(0);
epoll_event ev;
EPOLLIN -> ev.events;
42 -> ev.data.u64;
epoll_ctl(ep, EPOLL_CTL_ADD, fds[0], @!ev);
write(fds[1], "x", 1);
[2]$epoll_event evs;
int32 n = epoll_wait(ep, @!evs[0], 2, 1000);
printf("%d %lu\n", n, evs[0].data.u64);    // 1 42
```

Attributes that change the type itself (`vector_size`, `mode`) are not supported: a typedef that
carries one is not imported, and on a struct field the header is rejected. A typedef that carries
`packed`/`aligned` after its own name (`typedef struct { ... } T __attribute__((aligned(16)));`)
is not imported either; the attribute on the struct itself (`typedef struct { ... }
__attribute__((aligned(16))) T;`) is fine.

### Bit-Fields

A C struct with bit-fields (`unsigned th_off:4;`) has the same layout in Palan as in C, so a
header such as `netinet/tcp.h` can be included and the struct passed to and from C. A bit-field
member itself cannot be read, written, have its address taken or be given in a struct literal;
each of these is a compile error. The struct's other fields work as usual.

### C Enum Types

A C enum from a cincluded header is a Palan [enum](#enum): its enumerators are referenced as
`Name.X`, and it follows the same type rules. `Name` is the tag (`enum Tag { ... }`) or the
typedef name (`typedef enum { ... } Name;`); when the enum has both, either name works. It can be
the type of a variable, a struct field, a C function's parameter or return value, and a C global.

```palan
cinclude <jpeglib.h>;

jpeg_compress_struct cinfo;
J_COLOR_SPACE.JCS_RGB -> cinfo.in_color_space;
JDCT_DEFAULT -> cinfo.dct_method;    // #define JDCT_DEFAULT JDCT_ISLOW
int32 cs = cinfo.in_color_space;     // widens to its base type
```

- A macro that is just one enumerator (`#define JDCT_DEFAULT JDCT_ISLOW`, or glibc's
  `#define SOCK_STREAM SOCK_STREAM`) has the enum's type. A macro computing with enumerators
  (`#define X (A + 1)`) is a plain integer constant, like any other [macro constant](#macro-constants).
- The enumerators of an enum with neither a tag nor a typedef name (`enum { A, B };`) are
  referenced without qualification (`A`), as plain integer constants.
- An enum with an enumerator whose value can't be computed at compile time (e.g. `= sizeof(struct
  Opaque)` for a forward-declared struct) is not available; a function or struct using it is treated as unsupported.

### Incomplete Struct Types (Opaque Handles)

A struct tag whose full layout isn't known to Palan — either because the header only
forward-declares it, or because one of its fields uses a shape [C Array Fields](#c-array-fields)
above doesn't support — is registered as an **incomplete struct**: its name resolves and it can
be used, but only through a pointer (`@T`/`@!T`), the same restriction C itself places on an
incomplete type. `DIR` (from `dirent.h`) is the running example: its underlying `struct
__dirstream` is only forward-declared, so `DIR` is incomplete, and `opendir`/`readdir`/`closedir`
all work with it purely as an opaque handle:

```palan
cinclude <dirent.h>;
@!DIR d = opendir(".");
closedir(d);
```

`@!DIR d = opendir(...)` is **not** automatically freed at scope exit — unlike an owning struct
pointer variable, the pointee here belongs to C (closed by `closedir`, not Palan's own allocator).

An incomplete struct cannot be used anywhere a full layout is required: a by-value variable
declaration (`DIR d;`), an owned or embedded array of it (`[n]DIR`/`[n]$DIR`), a field access
through it, subscripting an array of it, or embedding it in another struct. Each of these is a
compile error naming the struct and the reason (forward-declared in the header, or a field type
this version can't represent) — never a compiler crash.

---

## 13. Raw Syscalls

A `syscall` declaration binds a Linux x86-64 syscall number to a name, callable afterward like
any other function — bypassing libc entirely:

```palan
syscall sys_write(int32 fd, @void buf, uint64 count) -> int64 = 1;

sys_write(1, "hello, syscall\n", 15u);
```

A cinclude'd macro constant works here too, since it folds to a literal before this point is
checked (see [Macro Constants](#macro-constants)):

```palan
cinclude <sys/syscall.h>;

syscall sys_write(int32 fd, @void buf, uint64 count) -> int64 = SYS_write;
```

- Syntax: `[export] syscall name(parameters) [-> type] = number;`, where `number` must be an
  integer literal (0 to 2^32-1) — a bare literal or a cinclude'd macro constant that folds to
  one; any other expression is not allowed.
- Once declared, the name is called exactly like a normal function, including from other
  functions or across an `import`/`export` boundary (see §11).
- A block-scoped `syscall` declaration is visible only within that block, same as a nested
  `func`.
- At most 6 parameters. The return, if any, must be a single unnamed type (`-> type`); named or
  multiple return values are not allowed.
- Parameter and return types must be representable as a full 8-byte register: `flo32`/`flo64`
  and the 8-bit/16-bit integer types (including `bool`) are rejected.
- The return value is the kernel's raw result in `rax`: a negative value conventionally means
  `-errno` (e.g. `-9` for `EBADF`), which the caller must interpret itself — libc's `errno`
  variable is never touched.
- `syscall` is a reserved word, so a cinclude'd `<unistd.h>`'s `syscall()` wrapper function
  cannot be called from a file that uses this feature.

