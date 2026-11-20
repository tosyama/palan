# Palan Issues

Open issues and topics for future consideration.

---

## 1. Borrowed Pointer Lifetimes Are Unchecked

**Summary:** The address-of operators (`@`/`@!`) produce a borrowed pointer that is never allocated and never freed on its own — the object it points into keeps its own ownership and free timing unchanged. There is currently no check that the pointer does not outlive that object. A pointer taken from a struct field or array element inside a block can be assigned to a variable declared outside the block; once the block ends and the owning variable is freed, the outer pointer is left dangling with no diagnostic, e.g.:

```
type Point { int64 x; int64 y; };

@!int64 p;
{
    Point s;
    5 -> s.x;
    @!s.x -> p;
}
printf("%ld\n", p[0]);
```

Reading through `p` after the block exits reads freed memory — undefined behavior, observed to print a garbage value rather than crash. Detecting this requires a borrow-lifetime/escape analysis, which is out of scope for the address-of/dereference work that introduced general `@`/`@!` — no such analysis is implemented today. A stack variable (`$Point s;`) widens the reach of the same gap: a pointer into it that is returned from the function points into a frame that no longer exists. A returned borrow (`-> @T`, and a borrowed array `-> @[]T` since v0.2.4) may likewise point to the function's own storage (`P loc; return @loc;`). Checking only where a returned borrow comes from — a borrowed parameter, or what is reached from one — would catch that case without a full analysis, but not one that goes through a local borrow variable.

A variable-length array (v0.2.1) adds a shorter-lived case: `push` and `resize` can move its elements, so a borrow of an element (`@!v[i]`) or the elements given to a `[]T` parameter dangles after the length changes, within the same block. A `for` loop holds its array's storage and length until it ends, and its body may not change them through the name the loop was given (v0.2.2); a borrow made before the loop (`@![+]int64 q = @!v;` and then `q.push(1)` in `for copy a in v { }`) or a pointer stored elsewhere changes the array unnoticed, and the loop then reads freed or moved elements.

A pointer viewed as an owner (`T b <<= p as T;`) is not reset as a variable given away is, so `p` dangles once `b` is freed.

---

## 2. No Position-Independent Executables

**Summary:** The build manager links with `ld` directly and palan-codegen supplies `_start` and the glue to libc (see SpecAndDesign.md §3.5), producing an executable at a fixed address. A position-independent executable (PIE), which distributions build by default for address-space layout randomization, would need `ld -pie` and code that reaches libc's functions and globals through the PLT and GOT, which palan-codegen does not emit.

---

## 3. Using a Field or Element After It Is Given Away

**Summary:** Giving away a field or an element (`s.p ->> q`, `f(a[i]>>)`) leaves it NULL, and nothing stops it from being used afterwards: `s.p.x` dereferences NULL and the program crashes, and `r -> s.p` copies into a NULL struct. Using a variable after it gives its value away is a compile error (v0.2.6), but that check follows variables only. A field or element is reached by a path whose indices are often known only at run time, and can be filled again through another path or a borrow, so following it needs more than the variables' flow.

---

## 4. Grapheme Clusters

**Summary:** `std/utf8` counts and iterates by code point, so `"🇯🇵"` has 2 characters and `e` with a combining accent has 2. Counting or iterating what a reader sees as one character (a grapheme cluster) needs Unicode's segmentation data, and its elements would be `utf8` values rather than code points. Case mapping and normalization need such data too.
