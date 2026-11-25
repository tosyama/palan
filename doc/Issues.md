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

Reading through `p` after the block exits reads freed memory — undefined behavior, observed to print a garbage value rather than crash. Detecting this requires a borrow-lifetime/escape analysis, which is out of scope for the address-of/dereference work that introduced general `@`/`@!` — no such analysis is implemented today. A stack variable (`$Point s;`) widens the reach of the same gap: a pointer into it that is returned from the function points into a frame that no longer exists. A returned borrow may likewise point to the function's own storage through a local borrow variable (`@P b = @loc; return b;`) or a named return value; `return @loc` itself is a compile error (v0.2.8).

A variable-length array (v0.2.1) adds a shorter-lived case: `push` and `resize` can move its elements, so a borrow of an element (`@!v[i]`) or the elements given to a `[]T` parameter dangles after the length changes, within the same block. A `for` loop holds its array's storage and length until it ends, and its body may not change them through the name the loop was given (v0.2.2); a borrow made before the loop (`@![+]int64 q = @!v;` and then `q.push(1)` in `for copy a in v { }`) or a pointer stored elsewhere changes the array unnoticed, and the loop then reads freed or moved elements. A dictionary (v0.2.7) is the same: adding or removing a key can move or free the values and the keys, so a borrow of either (`@!d[k]`, a `for`'s `k` kept after its iteration) dangles.

A struct a call returns may hold a borrow of a temporary given to the call (`I x = it(@mk());` where `it` keeps its argument in a `@T` field): the temporary is freed with the statement and the field dangles. A call returning a borrow itself is rejected there, and the call of a `for`'s range has its temporaries held until the loop ends (v0.2.7), but a struct is not followed to what its fields borrow.

A pointer viewed as an owner (`T b <<= p as T;`) stays a borrow, usable while `b` lives, and dangles once `b` is freed.

---

## 2. No Position-Independent Executables

**Summary:** The build manager links with `ld` directly and palan-codegen supplies `_start` and the glue to libc (see SpecAndDesign.md §3.5), producing an executable at a fixed address. A position-independent executable (PIE), which distributions build by default for address-space layout randomization, would need `ld -pie` and code that reaches libc's functions and globals through the PLT and GOT, which palan-codegen does not emit.

---

## 3. Using a Field or Element After It Is Given Away

**Summary:** Giving away a field or an element (`s.p ->> q`, `f(a[i]>>)`) leaves it NULL, and nothing stops it from being used afterwards: `s.p.x` dereferences NULL and the program crashes, and `r -> s.p` copies into a NULL struct. Using a variable after it gives its value away is a compile error (v0.2.6), but that check follows variables only. A field or element is reached by a path whose indices are often known only at run time, and can be filled again through another path or a borrow, so following it needs more than the variables' flow.

---

## 4. Case Mapping and Normalization

**Summary:** `std/utf8` compares and searches by bytes, so `"é"` as one code point and as `e` with a combining accent differ, and it has no conversion to upper or lower case. Both need Unicode's data (and case mapping, in part, the language of the text), as grapheme clusters (v0.2.7) do; the tables would be generated as theirs are.

---

## 5. A Borrow of a Sized Array as an Element or Pointee

**Summary:** `[2]@[3]int32`, `[+]@[3]int32` and `@@[3]int32` are compile errors, while `@[]T`, `@[+]T` and `@[""]T` can be an element or what a pointer points to. palan-sa types a row of a borrowed array (`@[2][3]T`) and a borrow held as an element (`@[2]@[3]T`) the same way, so it would copy the contents where the element should be bound again, and replace the element's own permission by the outer borrow's. Supporting it needs the two told apart in the type, and every place that takes a level of an array for owned rows to be revisited.

---

## 6. A Loop over a Dictionary's Keys Alone

**Summary:** `for` over a dictionary always takes the value, the key being its second variable, so a loop needing only the keys names a value it does not use (`for @v, k in d`; with `copy` it also copies each value). A form for the keys alone, `for k in keys(@d) { }` (also `for k in d -> keys()`), would be the range of a `for` only and allocate nothing: `k`, without `copy` or `@`, would be what the second variable is now, the dictionary's own copy valid for the iteration, and the body could not add or remove a key of `d`.
