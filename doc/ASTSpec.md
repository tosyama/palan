Palan Abstract Syntax Tree Json Specification
============================================

ver. 0.2.6

\* - Required

Root
----
- import - import file list
- export - export declaration list
- original - original file path
- c-headers - Path string list of every file the module's `cinclude`s read, the headers they include
  too (may repeat). Omitted when none. The build manager compares their times with the AST's
  (SpecAndDesign.md 5.1)
- module\* - Module identity string (SpecAndDesign.md 3.7), from the file's `library` declaration
  and name (`std.utf8`, `util`)
- target\* - Target name string (`<arch>-<os>-<abi>`), which the C declarations depend on; a `.pai`'s
  `target` statement must name the same
- ast\* - AST model
- libpaths - Absolute directory string list from `libpath` statements; never from a `.pai`
- libs - Link library name string list from `link` statements; only from a `.pai`
- built - Version string from the `built` statement; only from a `.pai`

Import file list
----------------
- path-type\* - Path type string: "src" "inc"
- path\* - Path string. For "src", absolute with symbolic links resolved (SpecAndDesign.md 3.7); for "inc", the library module as written (`std/utf8`)

Export declaration list
-------------------------
Each entry is a Function definition model (see below) for an `export func` or `export syscall`
declaration, with the `block` field omitted (signature only; a `syscall` declaration has none).
An exported type is not listed here: its `struct-def`/`type-alias`/`enum-def` statement carries
`export` instead, since an importer needs the module's other declarations to resolve it.

- name\* - Function name string
- func-type\* - Function type string: "palan" "syscall"
- parameters\* - Parameter list
- ret-type - Return variable type (single-return; omitted for void and multi-return)
- rets - Return value list (multi-return; omitted for single-return and void)
- syscall-number - Expression node (see Function definition model below; func-type "syscall" only)

AST model
---------
- functions - Function definition model list (Palan user-defined functions and `syscall` declarations)
- statements - Statement model list

Function definition model
-------------------------
- name\* - Function name string; an operator function's is its operator: "+" "-" "*" "/" "%" "==" "<"
  "<<"; a type's own copy is "->" (SpecAndDesign.md 3.8)
- func-type\* - Function type string: "palan" "c" "syscall"
- loc\* - Location Array (palan, syscall only)

  1. **palan** - Palan user-defined function
     - export - Boolean, true if declared with `export` keyword (omitted when false)
     - c-export - Boolean, true if declared with `export "C"`, linked by its name (SpecAndDesign.md
       3.7; omitted when false)
     - parameters\* - Parameter list (Palan parameter, see below; empty array when no parameters)
     - ret-type - Return variable type (unnamed single return only; omitted for void and named returns)
     - rets - Named return list (Palan parameter, see below; omitted for an unnamed return and void)
     - release - Boolean, true when marked `release`, as the function releasing an opaque type
       (SpecAndDesign.md 3.7; omitted when false)
     - block\* - Block object (function body; see Block object below). Absent for a function a
       `.pai` declares (SpecAndDesign.md 3.7): an exported one, or a type's `->`, which a `.pai`
       writes unexported

  2. **c** - C function prototype (from `cinclude`)
     - parameters\* - Parameter list (C parameter, see below; empty array when no parameters)
     - ret-type\* - Return variable type
     - symbol - Symbol the function links to (omitted when it is `name`). A header macro that
       names another declared function (`#define sscanf __isoc99_sscanf`) makes `name` a
       declaration with that function's type and symbol, replacing any older one of `name`

  3. **syscall** - Raw syscall declaration (a prototype with no block)
     - export - Boolean, true if declared with `export` keyword (omitted when false)
     - c-export - Boolean, true if declared with `export "C"`, linked by its name (SpecAndDesign.md
       3.7; omitted when false)
     - parameters\* - Parameter list (Palan parameter, see below; empty array when no parameters;
       at most 6, checked by SA)
     - ret-type - Return variable type (omitted for void; never `rets` — gen-ast rejects named and
       multiple returns)
     - syscall-number\* - Expression node for the syscall number; a reference to a cinclude'd
       macro constant is already folded to a `lit-int` by gen-ast (see Constant definition
       model below), so SA only requires the node to be an integer literal expression and folds
       it to a plain integer

Constant definition model
--------------------------
Emitted by **palan-c2ast** in its own per-header `ast.json` (the `ast.constants` list — same
"lifted from the header's own top-level list" relationship as the Struct definition model
below), never by palan-gen-ast: gen-ast consumes this list into an internal macro table when it
processes each `cinclude` statement and folds every later textual reference to a macro name into
a `lit-int` node in place (see Expression model's `lit-int` below), so `constants` itself
never reaches gen-ast's own `ast.json`.

An object-like `#define` macro whose body, after fully expanding any references to other
object-like macros (e.g. `#define S_IFDIR __S_IFDIR`), folds down to a single compile-time
integer value is exported here. The body may be a bare integer literal (e.g. `#define MAGIC
42`), a unary `+`/`-`/`~` of one, or a binary expression built from `| & ^ << >> + - * / %` over such
forms (e.g. `#define S_IRWXU (S_IREAD|S_IWRITE|S_IEXEC)`) — evaluated left-to-right within each
precedence level. Literals may carry C's `U`/`L`/`LL` suffixes, and any operand may be an integer
cast (e.g. ncurses' `((chtype)(1U) << 18)`) or `sizeof(type)`, typed `uint64` (`size_t`). `sizeof`
folds for a primitive, pointer, typedef, array of a folded size, enum, or a struct/union whose
body is in the same header, laid out by the x86-64 C rules including `packed`/`aligned` (see
Struct definition model); `_Alignof`/`__alignof__` fold the same way. For anything else (an incomplete struct, `void`, a typedef from an earlier
`cinclude`) it doesn't fold. An operation with no suffixed or cast operand uses
exact signed 64-bit arithmetic; otherwise it follows C's integer promotions and usual arithmetic
conversions (LP64), with unsigned results wrapping to their width. An operation that would
overflow a signed type, divide/mod by zero, or shift by a negative or out-of-range count does not
fold (same "not exported" outcome as an unsupported form). The body may also be a pointer-cast of
any of these folded forms (e.g. `#define NULL ((void *)0)`). A form that doesn't fold this way (a
function-like macro referenced without a call, a string or floating literal, a relational/
equality/logical/ternary expression, unary `!`, or a reference to an unresolved identifier)
is not exported here; it goes to the `ast.unexported-macros` list instead, as `{name}` or, for a
function-like macro, `{name, "function-like": true}`. gen-ast registers these into the same macro
table and, instead of folding, sets `c-macro`
on a later `id` or `call` that names one (see Expression model's `id`), so SA reports the macro
rather than an undefined name.

Each c2ast run starts from the previous `cinclude`'s macros and types (SpecAndDesign.md §3.3),
so these lists hold only the macros the header itself (re)defines, and `ast.undefined-macros`
names the ones it `#undef`s. Likewise a `cinclude`'s `structs`, `enums` and `typedefs` hold only
what an earlier `cinclude` didn't already give, such as a struct it only referenced. gen-ast keeps each name's registrations in source order, and a reference uses
the last one before it; an `#undef` registration leaves the reference unfolded.

An enumerator (see Enum definition model below) is an integer operand in these expressions,
untyped like an unsuffixed literal. A body that is exactly one enumerator of a named enum
(`#define JDCT_DEFAULT JDCT_ISLOW`, glibc's `#define SOCK_STREAM SOCK_STREAM`) stands for that
enumerator and keeps the enum's type. The list also holds the enumerators of an anonymous enum,
which C code references unqualified, as untyped constants; a `#define X X` over one of them is
not exported a second time.

- name\* - Macro name string
- value\* - Decimal string (e.g. "10")
- value-type - Variable type (see below). Present only when C fixes the type: for a body with a
  suffixed literal or an integer cast, its C type (e.g. `uint32` for `1U`; a cast's own target
  type, including `typedef-name`, when the cast is outermost); for a pointer-cast body, the
  cast's own target type (e.g. `pntr` for `NULL`). Omitted otherwise (e.g. `#define MAGIC 42`,
  `#define ERR (-1)`), so the constant is an untyped literal that takes its type from the
  context, same as a Palan source literal. A body that is one named enum's enumerator has an
  `enum` reference here (`{"type-kind":"enum","type-name":<enum name>}`)

Enum definition model
---------------------
Captured from every enum body palan-c2ast sees in a `cinclude`d C header, into its own
top-level `ast.enums` list (lifted onto the `cinclude` statement's `enums` field by gen-ast,
like `structs`). c2ast computes each enumerator's value; an enum with a value it cannot compute
(e.g. `= sizeof(struct Opaque)` for a forward-declared struct) is not captured, and references to it stay unrepresentable in SA. The base
integer type is not given here; SA chooses it by the same rule as for a Palan enum (SASpec.md).

- name\* - Enum name string: the tag, or for `typedef enum { ... } Name;` (a single, non-derived
  declarator) the typedef name, as for a struct. An anonymous body otherwise gets a synthesized
  `anon@<file>:<line>:<col>` name (see Struct definition model's `name`).
- enumerators\* - Enumerator list, in source order
  - name\* - Enumerator name string
  - value\* - Decimal string, within the int64 range

Struct definition model
------------------------
Captured from every struct or union tag palan-c2ast sees while parsing a `cinclude`d C header
(C struct and union tags share one namespace, so both go into this one list) —
a full `struct Name { field_decl... }` definition, a forward declaration with no body
(`struct Missing;`), or a bare reference through a field/parameter/return type (e.g. a
pointer field whose pointee is never defined in this header) — one entry per tag name,
keyed by first appearance. palan-c2ast collects every struct it captures from a header into
its own top-level `ast.structs` list; gen-ast then lifts that list onto the enclosing
`cinclude` statement's `structs` field (see Statement model below) when merging the header's
AST in. Same field-list shape as the native `struct-def` statement.

- name\* - Struct tag name string. An anonymous struct/union body used as a member
  (`struct { ... } name;` or `struct { ... };` inside another struct/union) is given a synthesized tag
  `anon@<file>:<line>:<col>` (the body's source location, with `#2`, `#3`, ... appended when several
  bodies share one location, as every expansion of one macro body does). Named by location rather than a counter so the
  same body gets the same tag in every cinclude's separate c2ast run. The name contains
  characters no C identifier can, so it never collides with a real tag, and Palan source cannot
  spell it.
- union - `true` when the tag is a C union; omitted for a struct.
- packed - `true` for GCC's `__attribute__((packed))` on the struct/union itself: every field
  is aligned to 1. Omitted otherwise.
- aligned - Integer from `__attribute__((aligned(N)))` on the struct/union itself (16 for a bare
  `aligned`): its alignment is at least N. Omitted otherwise.
- typedef-name - The first typedef declared directly over the tag (`typedef struct a_s a_t;`,
  not `typedef a_t b_t;` or a pointer typedef), which SA's messages name the struct by. A C
  reserved name (`__FILE`) gives way to a later one that is not (`FILE`). Omitted when there is
  none, or when the typedef name is the tag itself.
- fields - Field list; omitted when this header never gives the tag a body (forward
  declaration or bare reference only). SA registers such a tag as an incomplete struct
  (usable only through a pointer) rather than leaving the name unresolved — see SASpec.md.
  - name - Field name string; omitted for a C11 anonymous member (`union { ... };` with no
    member name), whose `var-type` names the body's synthesized tag. SA promotes that body's
    fields into the enclosing struct at the member's offset.
  - var-type\* - Field type (same Variable type object format; a C array declarator, e.g.
    `char name[16];`, produces an `arr` type-kind field — see `embedded` above and SASpec.md's
    C-origin field admission rules for which array shapes SA accepts)
  - packed / aligned - The same attributes on the field alone: `packed` aligns it to 1,
    `aligned` raises its alignment to at least N. Omitted otherwise.
  - bits - A bit-field's width (`unsigned a:4;`), an integer; `null` when the width does not
    fold to a constant. An unnamed bit-field (`int :3;`) has `bits` and no `name`. Omitted for
    any other field.

c2ast reads GCC's `__attribute__((...))` itself (it is a keyword, so the empty
`#define __attribute__(xyz)` that `sys/cdefs.h` issues for a non-GNU compiler has no effect).
Only `packed` and `aligned` are recorded; other attributes (`noreturn`, `format`, ...) are
ignored. `vector_size` and `mode`, which change the type itself, are rejected: a typedef
carrying them is not captured, and on any other declaration c2ast reports an error, as it does
for `packed`/`aligned` on an enum. A typedef carrying `packed`/`aligned` on its own declarator
is not captured either: GCC gives that alignment to the typedef name only, which Palan has no
way to express.

Global variable model
----------------------
Captured from file-scope `extern` object declarations in a `cinclude`d C header (e.g.
`extern FILE *stdout;`) whose type is `prim`, `pntr` or `enum` — the only shapes Palan can
represent without heap or embedded-array semantics. `static` declarations, block-scope
declarations, and `extern` declarations of array or by-value struct/union type are
not captured. palan-c2ast collects these into its own top-level `ast.globals` list;
gen-ast then lifts that list onto the enclosing `cinclude` statement's `globals` field
(see Statement model below) when merging the header's AST in.

- name\* - Global variable name string
- var-type\* - Variable type (same Variable type object format)

Typedef definition model
-------------------------
Captured from every `typedef` declaration in a `cinclude`d C header whose resolved
underlying type is `prim`, `pntr` (e.g. `typedef void *timer_t;`), `user`, or a
`strct`/`union`/`enum` that carries a `type-name` (including a name synthesized for a single,
non-derived typedef of an otherwise tagless struct/union/enum body — see Variable type's `strct`
case below).
`typedef`s are captured regardless of whether any C function or global in the header
references them, so a header of pure typedefs and no functions (e.g. `stdint.h`) still
exports its types. palan-c2ast collects these into its own top-level `ast.typedefs` list,
keyed by name (a name typedef'd more than once collapses to a single entry holding its
last-resolved shape) and sorted by typedef name; the list is omitted entirely when the
header defines no capturable typedef.

- name\* - Typedef name string
- var-type\* - The typedef's fully resolved underlying type (same Variable type object
  format; a chain of typedefs is collapsed to its bottom type). Never carries a
  `typedef-name` field itself — that field is a reference-site annotation (see the Note
  below), not part of the typedef's own registered shape.

Palan Parameter
---------------
Used in Palan function `parameters` and `rets`.

- name\* - Variable name string
- var-type\* - Variable type
- init - Expression node for a parameter's default value, or a named return's initial value
  (omitted when not written)

A parameter or return the grammar parses but the compiler does not implement (`T >> name`, an
untyped `name = expr`, a type with no declarable form such as `[#]T`) stays in the list as
`{"not-impl": true, "loc": ...}`, plus `untyped-var` (the name) for `name = expr`; SA rejects
the function at its definition. An unimplemented unnamed return type is a `ret-type` of the
same shape.

C Parameter
-----------
Used in C function `parameters`.

- name\* - Parameter name string (or "..." for variadic sentinel)
- var-type - Variable type (omitted for variadic sentinel "..."). An array-declared C parameter
  (`T name[n]`) decays to a plain `pntr` whose `base-type` is the array's own `base-type` — same
  as C's own array-to-pointer decay, and only the outermost dimension decays: `T name[2][3]`
  becomes `pntr(arr(base-type: T, size-expr: 3, ...))`, not `pntr(pntr(T))`.
- A prototype declared with the single unnamed keyword `void` (`f(void)`) normalizes to an empty
  `parameters` array — the same shape as a genuinely empty `()` parameter list — rather than a
  one-entry list carrying a `void`-typed parameter. This applies wherever a C parameter list
  appears, including a function pointer's own inner parameter list.

Return value (rets entry)
-------------------------
Same structure as Palan Parameter.

- name\* - Return value name string
- var-type\* - Variable type

Variable type
-------------
- type-kind\* - Type kind string: "prim" "pntr" "arr" "embed" "strct" "union" "enum" "func" "user" "keep"
- const - Boolean, true if const qualified (omitted when false). c2ast/cinclude-only: a C
  pointer's own qualifier (`T * const`) is emitted on the `pntr` node itself; the pointee's
  qualifier (`const T *`) is emitted on `pntr`'s `base-type`. SA folds pointee `const` into
  `mutable` (below) on any `pntr` reached from a `cinclude`d function signature, so only
  `mutable` reaches sa.json for those pointers — see SASpec.md.
  1. prim - Primitive type
    - type-name\* - Type name string
      - Integer: "int8" "int16" "int32" "int64" "uint8" "uint16" "uint32" "uint64"
      - Float: "flo32" "flo64"
      - Boolean: "bool" — also what c2ast emits for C's `_Bool`
      - Other: "void" — pointee-only (`prim(void)` appears only as a `pntr`'s `base-type`, never
        as a variable's own top-level type). Previously only reachable from a c2ast-derived
        signature (a C `void *` parameter/return/field); native syntax `@void`/`@!void` now
        produces the same shape directly (`'@' KW_VOID` / `AT_EXCL KW_VOID` in the grammar), so
        `pntr(prim(void))` no longer has a single origin.
      - A type name the parser can't classify (a struct, alias, or enum) also arrives as `prim`;
        SA resolves it. A type imported under a module alias is written `V.T` and arrives as the
        single string `"V.T"`.
  2. pntr - Pointer type
    - base-type\* - Base variable type
    - mutable - Boolean, true for a writable pointer (`@!` syntax), false for a read-only
      pointer (`@` syntax). Native `@`/`@!` syntax always emits this key explicitly (including
      `false` for `@`); a c2ast-derived `pntr` (a C pointer parameter/return/field) and an
      SA-synthesized `pntr` (a struct or array variable's own type) never emit it. A missing
      `mutable` key means writable — see SASpec.md's typeCompat rules for how sa.json settles
      this for every origin.
  3. arr - Array type
    - base-type\* - Base variable type
    - specifier\* - Array kind string:
      - "raw"      — `[expr]type` or `[]type`    raw heap array (C-array equivalent, no count stored)
      - "fixed"    — `[#expr]type` or `[#]type`  heap array with element count in memory header
      - "variable" — `[+expr]type` or `[+]type`  variable-size array (size-expr = initial capacity)
      - "variadic" — `[...]type`  a variadic parameter, only as the outermost type of the last
        parameter of a function or a function pointer type (size-expr is always null); gen-ast
        rejects one followed by another parameter
    - size-expr - Array size expression model
      - non-null: size/capacity determined by this expression
      - null semantics differ by specifier:
        - "raw"      (`[]type`)  — initialization required; element count determined by SA from initializer
        - "fixed"    (`[#]type`) — initialization required; element count determined by SA from initializer
        - "variable" (`[+]type`) — no initial capacity: the header is allocated, its data is not
          (no initialization required)
    - embedded - Boolean, true when the array's storage is laid out inline/contiguous rather than
      as a separately-allocated block; omitted when false. Native syntax sets this for the
      `$`-prefixed inline-storage forms, both 1D (`[n]$T`) and a contiguous 2D array (`[n]$[m]T`,
      or `[]$[m]T` with a null outer size-expr);
      c2ast sets it on every C array declarator it captures (a struct field or parameter written
      `T name[n]` in the header), 1D included, since a C array is always inline storage.
    Note: For `[m][n]T` (2D array via pointer-of-pointers), the outer arr's `base-type` is itself an
    `arr` type (`specifier: "raw"`, leaf `base-type` is a prim type). SA transforms this nested arr
    into a `pntr(pntr(T))` var-decl with auto-generated allocator calls (see SASpec.md).
    Note: For `[n]$[m]T` (contiguous 2D array), the outer arr has `embedded: true` and its `base-type`
    is an inner `arr`. SA allocates the entire grid as a single malloc of n*m elements.
    Note: The `$` token in `[n]$[m]T` produces an `arr` with `embedded:true`; `$T` standalone (outside
    an array context) produces the `embed` type-kind below.
    Note: A c2ast-captured C array (`specifier:"raw"`, `embedded:true`) nests the same way a native
    multi-dimensional array does — `T name[2][3]` produces an outer `arr` (size-expr 2) whose
    `base-type` is an inner `arr` (size-expr 3). See SASpec.md's C-origin field admission rules for
    which shapes SA accepts as a struct field (2D+ is not supported there) and the C Parameter
    section below for how an array-typed parameter decays.
    Note: c2ast gives a C array a null `size-expr` only for `[]`. A size it can't fold to an
    integer is `{"expr-type": "not-const"}`, so it never reads as a flexible array member; SA
    rejects it in a struct field like any other non-literal size.
    Note: c2ast's declarator parser follows normal C precedence — the postfix `[n]` binds tighter
    than the prefix `*` — so `T *name[n];` (array of `n` pointers to T) captures as an `arr` whose
    `base-type` is `pntr(T)`, while `T (*name)[n];` (pointer to an array of `n` T) captures as a
    `pntr` whose `base-type` is `arr(T)`; these are not interchangeable. See SASpec.md's C-origin
    field admission rules for how the `arr(pntr(T))` form is normalized to Palan's `[n]@T`/`[n]@!T`
    pointer-slot-array shape at the cinclude ingestion boundary.
  4. embed - In-place storage (`$T` syntax)
    - base-type\* - The type placed in place: a struct name (type-kind "prim") in a field, or any
      declarable variable type in a variable declaration
    Note: In a `struct-def` field list, SA resolves the sub-struct layout and folds the embedded
    fields' offsets into the parent struct (no separate pointer). At the top of a local `var-decl`
    var-type (`$P p;`, `$[4]int32 a;`), it places the variable's storage in the stack frame. A
    parameter or named return with it is emitted as a not-impl entry (`{"not-impl": true}`).
  5. strct - Struct type reference (by name), from a C struct-typed parameter/return in a
     `cinclude`d function signature
    - type-name - Struct name string; omitted only for a reference to a genuinely untagged
      (anonymous `struct { ... }`) type, which SA cannot register under any name and treats
      as unrepresentable (see SASpec.md's C-origin signature admission). A *named* tag is
      always given a `type-name` here — including a single, non-derived typedef of an
      otherwise tagless struct body (`typedef struct { ... } Name;`), for which c2ast
      synthesizes `Name` itself as the tag (a multi-declarator or derived-declarator typedef
      of such a body, e.g. `typedef struct {...} X, *PX;` or `typedef struct {...} *PX;`,
      is not synthesized and stays `user`/untagged respectively) — even when the header only
      forward-declares it or the reference is a bare mention with no field list ever seen —
      SA registers such a tag as
      an incomplete struct (usable only through a pointer) rather than leaving the name
      unresolved; see SASpec.md's Incomplete struct types.
  6. union - Union type reference (by name), from a C union-typed field/parameter/return.
     Same shape and `type-name` rules as `strct` above (including tag synthesis for a single,
     non-derived `typedef union { ... } Name;` and for an anonymous member body); the union's
     fields are in its Struct definition model entry (`"union": true`). SA folds it to
     `struct` at ingestion, the same as `strct`.
    - type-name - Union tag name string; omitted only for a genuinely untagged reference
  7. enum - Enum type reference (by name), from a C enum-typed field/parameter/return/global.
     Its enumerators are in its Enum definition model entry. SA resolves it to the enum's own
     type at ingestion; a reference to an enum not captured there stays unrepresentable (see
     SASpec.md's C-origin signature admission).
    - type-name\* - Enum name string (see Enum definition model's `name`)
  8. func - Function type, only as a `pntr`'s `base-type` (a function pointer)
    - parameters\* - Parameter list; a native `@func(...)` entry has only `var-type`
    - ret-type\* - Return variable type; `prim(void)` when nothing is returned
    - Native `@func(T1, T2) -> R` is `pntr` (`mutable: false`) to this node, the same shape
      c2ast gives a C function pointer; `@!func(...)` arrives with `mutable: true` for SA to
      reject. A function's return type is taken greedily, so `e as @func(T) -> X` is a cast
      to a function returning `X`. A C typedef of a function type is expanded where it is
      referenced (not listed in `typedefs`).
    - A parameter or return type written `_` is `keep` (below).
  9. user - An identifier used as a type that c2ast could not resolve to a recognized keyword
     or a previously-registered typedef — including an anonymous struct/union body via a
     multi-declarator or derived-declarator typedef (see type-kind "strct" above for the one
     struct/union-body shape c2ast does register), none of which c2ast registers. Each
     cinclude is translated separately, so it is also a typedef from an earlier cinclude
     (`size_t` in a header that relies on `stdio.h` being included first); SA replaces such a
     reference with that typedef's type at ingestion. Otherwise unrepresentable in SA this
     version (see SASpec.md's C-origin signature admission).
    - type-name\* - The unresolved identifier string
  10. keep - `_` as a function type's parameter or return type: the type a cast's source has at
      that place (`free as @func(Q)`, `f as @func(_, P) -> _`). SA accepts it only in a cast's
      `target-type`. No other fields.

Note: C `restrict` qualifier is not represented in the AST (optimization hint only).

Note: A var-type node originating from a C typedef that resolves to a known type (type-kind "prim",
or "pntr" for a pointer-bottomed typedef chain, e.g. `typedef void *timer_t;`) may carry an additional
`typedef-name` field — the original C typedef identifier (e.g. `"size_t"`). This is a
c2ast/cinclude-only annotation. For type-kind "prim", SA registers it as a native type alias (see
PalanReference.md §4 Type Aliases) and strips the field before emitting to sa.json (see SASpec.md).
For type-kind "pntr", the field is left in place and never reaches sa.json (C function signatures
themselves aren't serialized there); the name itself is registered through `ast.typedefs`.
This annotation is specific to the var-type node at the reference site that looked the typedef
name up (a function parameter/return, a global, or a further typedef's underlying type as parsed)
— it is never present on an `ast.typedefs` entry's own `var-type` (see Typedef definition model
above), even when that entry's resolved type came from a chain of several typedef names.

Block object
------------
Used in `func-def` bodies and standalone block statements.

- functions\* - Palan function definition list local to this block (may be empty array)
- body\* - Statement model list (does not contain func-def entries)

Statement model
---------------
- stmt-type\* - Statement type: "import" "cinclude" "expr" "var-decl" "assign" "arr-assign" "struct-def" "type-alias" "const-decl" "field-assign" "return" "tapple-decl" "tapple-assign" "block" "if" "while" "for" "break" "continue" "not-impl" "enum-def" "global-permit" "opaque-def"
- loc\* - Location Array
  1. import - import module statement
    - path-type\* - Path type string: "src" "inc"
    - path\* - Path string, as in the import file list
    - targets - Import target name string list (a function name, a type name or an operator), null - all
    - alias - Alias name string
  2. cinclude - C header include statement (expands in place, scope-aware)
    - path-type\* - Path type string: "src" "inc"
    - path\* - Path string
    - functions - Function definition model list (C prototypes from the header)
    - enums - Enum definition model list (see Enum definition model above); omitted when the
      header defines no capturable enum
    - structs - Struct definition model list (see Struct definition model above); omitted
      when the header defines no capturable structs
    - globals - Global variable model list (see Global variable model above); omitted
      when the header defines no capturable extern objects
    - typedefs - Typedef definition model list (see Typedef definition model above);
      omitted when the header defines no capturable typedef
    - export-types - C type name string list from the `export` clause (SpecAndDesign.md 3.7);
      omitted when the statement has no such clause
    - libs - Link library name string list, in source order (from the `link` clause);
      omitted when the statement has no `link` clause. See SASpec.md's Root `libs` for
      how these are aggregated (sorted, deduplicated, and unioned across scopes and
      modules) once SA processes them. An entry ending in `.o` goes to `objs` instead.
    - objs - Object file path string list from the `link` clause, each made absolute against
      the source file's directory; omitted when the clause names none. Aggregated like `libs`
      (SASpec.md's Root `objs`).
  3. expr - expression statement
    - body\* - Expression model
  4. var-decl - variable declaration statement
    - global - Boolean, true for a `global` declaration (omitted when false); `vars` then holds
      exactly one typed variable; any other `global` declaration (a type-omitted or tuple one) is
      a `not-impl` statement instead.
    - export - Boolean, true for an `export global` declaration (omitted when false)
    - vars\* - Variable declaration list
      - name\* - Variable name string
      - var-type\* - Variable type
      - init - Initializer expression model (omitted if no initializer)
      - ownership-transfer - Boolean, true if the initializer is given with `<<=` (`T b <<= v`),
        taking over what `v` owns; omitted when false
  5. assign - assignment statement (`expr -> var` or `expr ->> var`)
    - name\* - Target variable name string
    - value\* - Source expression model
    - ownership-transfer - Boolean, true if `->>` ownership-transfer syntax; omitted when false
    - owner-view-type - With `ownership-transfer`, the type the target is viewed as the owner of
      (`expr ->> loc as T`); omitted otherwise
  6. arr-assign - array element assignment statement (`expr -> arr[i]` or `expr ->> arr[i]`)
    - target\* - arr-index expression model (see Expression model §12)
    - value\* - Source expression model
    - ownership-transfer - Boolean, true if `->>` ownership-transfer syntax; omitted when false
    - owner-view-type - With `ownership-transfer`, the type the target is viewed as the owner of
      (`expr ->> loc as T`); omitted otherwise
  7. struct-def - struct type definition (`type Name { field_decl... }`; consumed by SA, not emitted to sa.json)
    - name\* - Struct name string
    - export - Boolean, true if declared with `export` (omitted when false)
    - fields\* - Field list (each entry: `name`, `var-type`, `init`, `access`)
      - name\* - Field name string
      - var-type\* - Field type (same Variable type object format)
      - init - The field's initializer (`int32 y = 7;`), an expression model as written
      - access - `"private"` or `"readonly"` (`private int32 y;`); omitted for a public field
  8. type-alias - native type alias declaration (`type Name = type_expr;`; consumed by SA, not emitted to sa.json)
    - name\* - Alias name string
    - export - Boolean, true if declared with `export` (omitted when false)
    - type\* - Aliased type (same Variable type object format)
  9. const-decl - native constant declaration (`const Name = <literal>;`; consumed by SA, not emitted to sa.json)
    - name\* - Constant name string
    - value\* - Literal expression model (lit-int, lit-uint, lit-flo, or lit-str; see Expression model)
  10. field-assign - struct field assignment (`value -> obj.field` or `value ->> obj.field`)
    - object\* - Base object expression model: `id` for a plain variable, `arr-index` or
      `field-access` for a nested chain (e.g. `s.f[0].sub`, `pts[0].x`), or any other expression
      the chain starts from (a call, a cast in parentheses)
    - field\*  - Field name string
    - value\*  - Source expression model
    - ownership-transfer - Boolean, true if `->>` ownership-transfer syntax; omitted when false
    - owner-view-type - With `ownership-transfer`, the type the target is viewed as the owner of
      (`expr ->> loc as T`); omitted otherwise
  11. return - return statement
    - values - Return expression list (omitted for bare `return;`)
  12. tapple-decl - tuple-style multiple return value declaration (`(type name, ...) = call(...)`)
    - vars\* - Variable declaration list (var-name, var-type per entry)
    - value\* - Call expression model (must be a call to a multi-return Palan function), or a
      `chain-call` whose last call is one
  13. tapple-assign - multiple return values assigned to existing targets (`call(...) -> (target, ...)`)
    - targets\* - Target expression model list: `id`, `arr-index` or `field-access` per entry
    - value\* - Call expression model (must be a call to a multi-return Palan function), or a
      `chain-call` whose last call is one
  - In a chained assignment `v -> t1 -> t2`, the `value` of the `t2` assign / arr-assign /
    field-assign is the whole `v -> t1` statement node, whose target SA reads back. An
    assignment feeding a chained call (`x -> v -> f()`) is likewise the `chain-call`'s `value`.
    Elsewhere in an expression such a node is rejected by SA. A chain with a tapple-assign in
    it is a single not-impl statement (a not-impl expression when a call follows).
  14. block - standalone block statement (`{ ... }`)
    - functions\* - Palan function definition list local to this block (may be empty array)
    - body\* - Statement model list (does not contain func-def entries)
  15. if - if / if-else statement
    - cond\* - Condition expression model
    - then\* - Then-block object (block statement body)
    - else - Else-block object or nested if statement (omitted when absent)
  16. while - while loop statement
    - cond\* - Condition expression model
    - body\* - Statement model list (raw array, no block wrapper)
  17. for - loop over an array's elements, or over a struct by its type's `iter` and `next`
      (`for copy a, i in arr { ... }`; SA lowers it to a block holding a while)
    - elem\* - The loop variable
      - name\* - Variable name string
      - borrow\* - Boolean, true for `@a`/`@!a` (the element itself, read-only or writable) and false for `copy a` (a copy)
      - mutable - Boolean, true for `@!a`; present only when borrow is true
      - loc\* - Location Array
    - index - Name string of the index variable (omitted when absent)
    - index-type - Type model of the index written before its name (`for @a, int32 i in arr`);
      omitted when not written, and the index is then `int64`
    - range\* - The expression model of the array or struct iterated over
    - body\* - Statement model list (raw array, no block wrapper)
  18. break - exit the innermost while or for loop (no additional fields)
  19. continue - skip to next iteration of innermost while or for loop (no additional fields)
  20. not-impl - a statement the grammar parses but the compiler does not implement (e.g.
      `construct`, `interface`, `x++`, a type-omitted declaration). Rejected by SA.
    - untyped-var - Name of the first type-omitted variable; present only for a `name = expr;`
      declaration (reserved for type inference; an assignment is `expr -> name`)
  21. enum-def - enum type definition (`type Name enum { X, Y = expr, ... }`; consumed by SA, not
      emitted to sa.json)
    - name\* - Enum name string
    - export - Boolean, true if declared with `export` (omitted when false)
    - enumerators\* - Enumerator list, in source order
      - name\* - Enumerator name string
      - value - Value expression model (omitted when the value is implicit)
      - loc\* - Location Array
  22. global-permit - the functions allowed to use a global variable: the `for` list of a
      `global` declaration (emitted right after its `var-decl`) or a later `global name for ...;`,
      or, with no list, `global name;`
    - name\* - Variable name string; an imported global under an alias is one name `M.count`
    - funcs\* - Permitted function list, in source order (empty for `global name;`)
      - name\* - Function name string (an operator for an operator function)
      - writable\* - Boolean, true when written with a trailing `!`
      - loc\* - Location Array
  23. opaque-def - opaque type definition (`type Name = opaque type_expr;`; consumed by SA, not
      emitted to sa.json)
    - name\* - Type name string
    - export - Boolean, true if declared with `export` (omitted when false)
    - contents - The type it holds (same Variable type object format); omitted in a `.pai`, which
      writes `type Name = opaque;`

Expression model
----------------
- expr-type\* - Expression type string: "lit-str" "lit-int" "lit-uint" "lit-flo" "id" "add" "sub" "mul" "div" "mod" "neg" "cmp" "call" "cast" "arr-index" "field-access" "member-call" "logical-and" "logical-or" "logical-not" "addr-of" "not-impl" "bitand" "bitor" "bitxor" "shl" "shr" "append" "bitnot" "arr-lit" "dict-lit" "sizeof" "chain-call" "transfer" "call-ptr"
- loc\* - Location Array
  1. lit-str - String literal
    - value\* - The bytes as escaped text: printable ASCII and valid UTF-8 characters as they
      are, `\"`, `\\`, `\n`, `\t`, `\r`, and every other byte as `\xHH` (upper-case hex), so
      `"x\xffy"` and the same bytes written raw are both `x\xFFy`
  2. lit-int - Signed integer literal (corresponds to INT token). A character literal (`'a'`)
    is also emitted as a `lit-int` holding its Unicode code point, with no `value-type`;
    `true`/`false` are emitted as a `bool`-typed `lit-int`
    - char - `true` for a character literal (omitted otherwise): one of 128 or more takes only
      an integer type of 32 bits or more, and one with no type to take is an `int32`
    - value\* - Decimal string, optionally with a leading `-` (e.g. "10", "-128"); a macro
      substitute's value may exceed int64 when its `value-type` is `uint64`
    - value-type - Variable type; present only for `true`/`false` (value `"1"`/`"0"`, value-type
      `bool`) or when this node is gen-ast's in-place substitute
      for a reference to a cinclude'd macro constant that has a `value-type` (see Constant
      definition model above) — a numeric or character literal never carries one. Holds the
      macro's own `value-type` (e.g. `pntr` for `NULL`), unchanged by the substitution; a macro
      without one substitutes an untyped `lit-int`, identical to a source literal. Substitution
      happens for every `id` reference whose name matches a macro registered by a `cinclude`
      earlier in the same textual scope (comparing `loc`; a reference before the cinclude, or to
      a name no cinclude registered, is left as `id`) and applies uniformly wherever an `id` node
      can appear (binary operands, `size-expr`, a `syscall-number`, etc.) — but never inside a
      `cinclude` statement's own subtree. A name registered by more than one `cinclude` resolves
      to whichever registered it first.
  3. lit-uint - Unsigned integer literal (corresponds to UINT token)
    - value\* - Decimal string (e.g. "10")
  4. lit-flo - Floating-point literal (corresponds to FLO token; format: digits.digits with an optional exponent, or digits with an exponent; the value string is kept as written, e.g. "1e10")
    - value\* - Decimal string (e.g. "3.14")
  5. id - Identifier (variable reference)
    - name\* - Identifier name string
    - c-macro - Set by gen-ast when the name is a cinclude'd macro c2ast could not export (see
      Constant definition model): "function-like" or "not-constant". SA reads it only to word the
      error when the name resolves to nothing else. A `call` carries it the same way.
  6. add - Binary addition
    - left\*  - Left operand expression model
    - right\* - Right operand expression model
  7. sub - Binary subtraction
    - left\*  - Left operand expression model
    - right\* - Right operand expression model
  8. mul - Binary multiplication
    - left\*  - Left operand expression model
    - right\* - Right operand expression model
  9. div - Binary division
    - left\*  - Left operand expression model
    - right\* - Right operand expression model
  10. mod - Remainder (`a % b`; integer operands only)
    - left\*  - Left operand expression model
    - right\* - Right operand expression model
  11. neg - Unary negation
    - operand\* - Operand expression model
  12. cmp - Comparison expression (result: 0 or 1 as int32)
    - op\*    - Operator string: "<" "<=" ">" ">=" "==" "!="
    - left\*  - Left operand expression model
    - right\* - Right operand expression model
  13. call - Function call expression
    - name\* - Function name string
    - args - Argument expression list
  14. cast - Explicit type cast expression (`expr as type` or `type-name(expr)`). `as` takes any
    type a declaration accepts; `type-name(expr)` produces a `cast` only for a built-in type
    keyword — an alias or enum cast `Name(expr)` is emitted as a `call` and SA treats it as a cast.
    - target-type\* - Target Variable type object
    - src\* - Source expression model
  15. arr-index - Array element access (`arr[i]`)
    - array\* - Array expression model
    - index\* - Index expression model
    (elem-size is added by SA; not present in AST)
  16. logical-and - Short-circuit logical AND (`a && b`; result: int32, 0 or 1)
    - left\*  - Left operand expression model
    - right\* - Right operand expression model
  17. logical-or - Short-circuit logical OR (`a || b`; result: int32, 0 or 1)
    - left\*  - Left operand expression model
    - right\* - Right operand expression model
  18. logical-not - Logical NOT (`!a`; result: int32, 0 or 1)
    - operand\* - Operand expression model
  19. field-access - Struct field read (`obj.field` rvalue). An enumerator reference `Name.X` has
    the same shape (`object` is the `id` `Name`); SA tells them apart.
    - object\* - Base object expression model — same vocabulary as field-assign's `object` (see
      Statement model above)
    - field\*  - Field name string
  20. member-call - `x.f(args)`: a module-alias call `L.f(args)` or a method-form call
    `aa.f(args)` (`f(@!aa, args)`); gen-ast does not tell them apart (consumed by SA, not
    emitted to sa.json)
    - object\* - Object expression: an `id` naming a module alias, or the receiver expression
    - method\* - Method/function name string
    - args - Argument expression list (without the receiver)
    Note: SA resolves `member-call` and emits a regular `call` node in sa.json (see SASpec.md
    "Method-form calls").
  21. addr-of - Address-of a local variable, a struct field reached from one, or an array
      element reached from one (`@` read-only, `@!`/`AT_EXCL` mutable). The operand grammar is
      `store_loc` — the same vocabulary the right side of `->` accepts (a bare identifier, a
      `.`-chain of field accesses, a `[expr]` index, a parenthesized tuple, or a call `f(...)` /
      `x.f(...)` whose object is itself one of these, or a parenthesized expression) — so `@s.x`,
      `@!s.in.v`, and `@arr[i]` all parse — or an array literal or `{name: value}` literal
      (`@[1, 2]`, `@![1, 2][3, 4]`, `@{x: 1}`); SA decides in sa.json whether the target is
      addressable (see SASpec.md). `@` binds looser than `.`, so `@x.f()` is `@(x.f())`, never
      `(@x).f()`.
    - object\* - Operand expression: `arr-lit` or `dict-lit` for a literal, otherwise the shape
      the `store_loc` alternative gives: `id` for a plain variable, `field-access` for a `.`-chain (nested
      chains produce nested `field-access` objects), `arr-index` for a `[expr]`-indexed target,
      `call` for a call `f(...)` (`@f()`), `member-call` for a method-form or module-qualified
      call `x.f(...)` (`@v.f()`, `@M.f()`, `@f().g()`), the inner expression for a parenthesized
      one (`@(x)`), or `not-impl` for a tuple grouping or a type-name cast `@int32(x)`. A call or
      a parenthesized expression may also be the base of the chain (`@f().x`, `@(p as @!P).x`).
    - mutable\* - `true` for `@!`, `false` for `@`
  22. not-impl - An expression the grammar parses but the compiler does not implement (e.g. a
      tuple `(a, b)` used as a value, `sizeof` of a `[#n]` type, a call argument `x>>`), or a `store_loc` shape that
      carries no addressable location (a parenthesized tuple, or a type-name cast `int32(x)`, reached via
      `@`/`@!` or as a chain root). Carries no
      fields beyond `expr-type` and `loc`; SA rejects any expression of this shape.
  23. bitand - Binary bitwise AND (`a & b`; integer operands only)
    - left\*  - Left operand expression model
    - right\* - Right operand expression model
  24. bitor - Binary bitwise OR (`a | b`; integer operands only)
    - left\*  - Left operand expression model
    - right\* - Right operand expression model
  25. bitxor - Binary bitwise XOR (`a ^ b`; integer operands only)
    - left\*  - Left operand expression model
    - right\* - Right operand expression model
  26. bitnot - Unary bitwise NOT (`~a`; integer operand only)
    - operand\* - Operand expression model
  27. shl - Left shift (`a <<< b`; integer operands only)
    - left\*  - Value expression model
    - right\* - Shift count expression model
  28. shr - Logical right shift (`a >>> b`; integer operands only)
    - left\*  - Value expression model
    - right\* - Shift count expression model
  29. arr-lit - Array literal (`[a, b, c]`)
    - items\* - Element expression model list. A 2D literal is an `arr-lit` of row `arr-lit`s;
      the concatenated form `[a,b][c,d]` is normalized to the same shape as the nested form
      `[[a,b],[c,d]]` (only `loc` differs)
  30. dict-lit - Named-value literal (`{x: a, y: b}`); a trailing comma is allowed
    - items\* - List of named values in source order
      - name\*  - Name string
      - value\* - Value expression model
      - loc\*   - Location Array of `name: value`
  31. sizeof - Byte size of a type (`sizeof(T)`). A type with a `[#n]`/`[+n]` level or a
      template argument is emitted as `not-impl` instead
    - type\* - Type model
  32. chain-call - A chained call `value -> f(args)` (`f(value, args)`), `value -> L.f(args)`, or
      a method-form call `value -> s.f(args)` (`f(@!s, value, args)`);
      consumed by SA, not emitted to sa.json (see SASpec.md "Chained calls")
    - value\* - Expression model whose value (or return values) come first in the arguments; a
      nested `chain-call` for `x -> f() -> g()`, or an assignment statement node for `x -> v -> f()`
    - call\* - `call` or `member-call` expression model, its `args` without the chained value
    `value ->> f(args)` is a `chain-call` whose `value` is a `transfer` of the value (`op` "->>").
  33. transfer - A value given away (`v>>`), written after it and binding tighter than `as` and
      the binary operators (`v>> as T` is a cast of it, which SA rejects). SA accepts it only as a
      call argument (see SASpec.md "Giving values away")
    - value\* - Expression model whose owned value is given away
    - op - Operator string for diagnostics: "->>" when it comes from `value ->> f(args)`;
      omitted for `>>`
  34. call-ptr - A call through a parenthesized expression, `(callee)(args)`. A parenthesized
      list of several expressions called this way is `not-impl`
    - callee\* - Expression model of a function pointer
    - args\* - Argument expression list
  35. append - `a << b`, a call of the operator function `<<` (precedence of the shifts);
      consumed by SA, which makes it a `call` (see SASpec.md "Usual arithmetic conversions")
    - left\*  - Expression model appended to
    - right\* - Expression model appended

Note: Unary minus on a `lit-int` without `value-type` is folded into a single `lit-int` with a negative
`value` (`-42` → `"value":"-42"`), so SA range-checks it as one value. Any other operand (including a
macro-folded `lit-int` and a `lit-uint`) is wrapped in a `neg` expression.
Note: sa.json extends this format with additional fields and expression kinds. See SASpec.md.

Location Array
-----------------
- 0\* - Begin line integer
- 1\* - Begin column integer
- 2\* - End line integer
- 3\* - End column integer

