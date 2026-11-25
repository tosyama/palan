Palan Semantic Analyzer JSON Specification
==========================================

ver. 0.2.8

Output of palan-sa. Extends the AST JSON format (see ASTSpec.md) with resolved
type information and pre-collected literal tables.

\* - Required

Root
----
- original\* - original source file path
- target\* - Target name string, as in the AST (see ASTSpec.md's Root). palan-codegen rejects a
  missing or unsupported one.
- main\* - The module's top-level function, whose body is `statements`: `name` (its symbol,
  `<module>..main`; see Symbol names) and `parameters` (`__argc` int64 and `__argv`
  `pntr(pntr(uint8))`, in the Function definition model's form), and `globals` as a Palan
  function has (the module's `global` variables and the C globals the top level uses). It is
  exported, and the program's `_start`, built at link time for the entry module, calls it after
  `init`.
- init\* - The symbol of the module's initialization function, `<module>..init`, which `functions`
  defines (see Module initialization below).
- globals - The module's static slots, each emitted by codegen in `.bss`: its `global` variables
  (`label` `<module>.<name>`) and `init`'s flag (`<module>..init.done`), with `var-type`, and
  `export: true` for an `export global`, whose label codegen makes global. Omitted when there are
  none. An imported global's slot is its own module's.
- str-literals\* - String literal table (collected by SA, used by codegen for .rodata)
- functions\* - Processed Palan function list (empty array when no functions defined)
- statements\* - Top-level statement list. When a top-level statement names the implicit
  `args` (a read-only `@[+]@[]uint8` borrow, declared in the top-level scope before anything
  else), the list starts with its header, built from `main`'s parameters and never freed: a
  `var-decl` of `args` (`pntr(varr(pntr(uint8) mutable false))` with `"mutable": false`) whose
  `init` is a 24-byte `frame-alloc`, then `field-assign`s with `"var":"args"` of `__argc` at
  offsets 0 (len) and 8 (cap) and `__argv` at 16 (data). The varr shape and builtins of that
  type are registered then too; a module that does not name `args` has neither.
- libs\* - Link library name list collected from `link` clauses on `cinclude` statements
  anywhere in this source file, including block-scoped and function-body-scoped ones
  (sorted, deduplicated). Empty array when none. Each module's sa.json carries only its
  own libraries; the build manager unions this list across all modules and appends
  `-l<name>` to the `ld` invocation for each entry.
- objs\* - Absolute object file path list from `link` clauses, collected like `libs` (sorted,
  deduplicated; empty array when none). Each file must exist (`E_LinkObjNotFound`). The build
  manager unions it across modules and passes each path to `ld` after the program's own objects.
- alloc-shapes\* - List of shape descriptors for arrays and structs requiring custom allocators.
  Empty array when no qualifying var-decls are present.
  Six entry kinds: struct, bytes, opaque, arr, varr and dict.

  The arr and varr entries describe their element by its layout: the element as `data` (or the
  array) holds it, with each integer type replaced by the signed one of its width (`uint32` and
  an enum become `int32`, `bool` becomes `int8`) and each struct owning nothing by
  `__pln_B<n>`, its `total-size` bytes (see the bytes entry). Arrays whose elements are laid out
  alike thus share their functions; whoever reads an integer from one extends it as its own
  type says. The element:
  - elem\* - `{"kind", "key", "size"}` and the type: `"prim"` with `"type"` (a primitive),
    `"struct"` with `"name"` (an owned slot holding a struct `T`, 8 bytes), `"opaque"` with
    `"name"` and `"release-func"` (an owned slot holding an opaque type, 8 bytes; see the opaque
    entry), `"embed-struct"`
    with `"name"` (the struct itself, `total-size` bytes), or `"varr"` with `"name"` (an owned
    slot holding a `[+]U`, 8 bytes; the name is its header type, `__pln_varr_U`) and `"shape-key"`
    (the `[+]U`'s own entry), `"dict"` with the same for a `[""]U` (see the dict entry), or, for a row `[n]U`, `"row"` (an owned slot holding the row's
    own block, 8 bytes) with `"elem"` (the row's element, described the same way), or
    `"embed-row"` (rows laid out in place; no `"size"`, which depends on the lengths) with `"depth"` (how many consecutive levels are laid
    out in place: `[+]$[3]$[4]int32`'s element is one `"embed-row"` of depth 2) and `"elem"` (the
    first element below them that is not laid out in place). `key` is the element type written
    layout written as in a symbol (see "Symbol names") without its lengths: `int32`,
    `P9__pln_B16` (`[n]Point`, Point 16 bytes owning nothing), `9__pln_B16` (`[+]$Point`),
    `PVint32`, `Pint32` for `[4]int32` and `[k]int32` alike, `C` and the levels below for an
    `"embed-row"` (`CPint32`). Lengths are passed instead: every arr and varr function takes,
    after its own arguments, one `int64` per array level below it, outermost first, through rows
    and `[+]U` elements alike (`[+][3][+][4]int32`'s `__pln_push_varr_PVPint32(h, x, 3, 4)`), so
    arrays differing only in lengths share their functions.
  - Their shape key is `arr_`, `varr_` or `evarr_` followed by the element's `key`, so a key names
    its element's layout. A struct element's struct or bytes entry and a `[+]U` element's varr
    entry precede the entry.

  **Arr entry** (`[n]T` whose element owns storage, a variable or an owned field; a row is such
  an element): build-mgr generates `__pln_alloc_arr_E(n)`, `__pln_init_arr_E(outer, n)`,
  `__pln_free_arr_E(pts, n)`, `__pln_fini_arr_E(pts, n)` and `__pln_copy_arr_E(dst, src, n)`,
  and with an element holding a type with its own `->`, `__pln_assign_arr_E(dst, src, n)`, which
  writes into the existing elements as the struct entry's `assigns` says. Both keep a source
  element moved out (NULL) as NULL and build a NULL destination slot before copying into it whole.
  An array of structs laid out in place (`[n]$T`), copied as bytes, has only
  `__pln_assign_arr_E(@!T dst, @T src, n)`.
  `E` is the element's `key`, each function's arguments followed by the lengths below it
  (`[2][3][4]int32` is `__pln_alloc_arr_PPint32(2, 3, 4)`). Levels laid out in place in the
  array itself are one run of their elements: with `S` owning fields, `[2]$[3]S` is
  `__pln_alloc_arr_P1S(6)`:
  - shape-kind\* - "arr"
  - shape-key\* - Shape key string (e.g. "arr\_P9\_\_pln\_B16", "arr\_Pint32")
  - elem\* - The element, as above

  **Varr entry** (`[+]T` / `[+]$T` variable; `T` below names the shape key's element part): for `[+]T`, build-mgr declares the header as
  `type __pln_varr_T { int64 len; int64 cap; @!T data; }` and generates
  `__pln_alloc_varr_T(len, cap)`, `__pln_init_varr_T(h, len, cap)` (mallocs `cap` elements when
  `cap > 0` and sets `data` to NULL otherwise), `__pln_fini_varr_T(@!hdr h)`
  (frees `data`) and `__pln_free_varr_T(@!hdr h)` (fini, then the header), and the built-ins
  `__pln_push_varr_T(@!hdr h, T x)`,
  `__pln_resize_varr_T(@!hdr h, int64 n)` and `__pln_pop_varr_T(@!hdr h) -> T`, and
  `__pln_copy_varr_T(@!hdr h, @T src, int64 n)` (grows as needed, then sets the length to `n`
  and copies `n` elements from `src`, which may be `h`'s own), and with an element holding a type
  with its own `->`, `__pln_assign_varr_T` taking the same, which writes into the elements `h`
  has and copies those it gains as `copy` does. Growing
  reallocates `data` to max(needed, cap × 2, 4) elements; shrinking keeps the capacity, and
  the elements `resize` adds are undefined. An empty `pop` or a negative `resize` calls `abort()`.
  With a struct `T` (elem kind `"struct"`), `data` is `@!@!T`, an array of owned slots, of
  which exactly the first `len` hold an element: growing (`init`, `resize`, `push`, `copy`)
  builds the new ones as `[n]T` does, shrinking (`resize`, `copy`) and `fini` free them with
  `__pln_free_T` (or `free`), and `pop` returns the last one without freeing it; `push` stores
  the struct it is given as the new slot, `copy` copies into the elements (`__pln_copy_T` or
  memcpy), and `copy`'s source is a run of slots (`[]T src`). `[+]$T` (elem kind
  `"embed-struct"`, shape key `evarr_T`, functions `__pln_*_evarr_T`) keeps the elements in
  `data` like a primitive's, `T` taking its `total-size` bytes and owning nothing; `push` copies
  the struct it is given in and frees it, and `pop` returns a newly allocated copy. `push`'s
  struct (a `[+]U` element's header too, and a row) is the caller's to give, as a struct
  parameter's argument is: the built-in's parameter is the owned `T` (`[n]U`), and the generated
  function takes it as `@!T` (`@![]U`; `W` in its symbol), since it keeps or frees it itself. With a `[+]U`
  element (elem kind `"varr"`) the slots hold the elements' headers the same way: a new element is
  an empty header (`calloc(1, 24)`), a dropped one is freed with `__pln_free_varr_U`, and a copy
  into one is `__pln_copy_varr_U`; `push` stores the header it is given, which the caller hands
  over. With a row `[n]U` (elem kind `"row"`) a slot holds the row's own block: a new one is
  `malloc(n * …)`, its data left undefined and its owned elements built, a dropped one has them dropped (unless the slot
  is NULL, after `->>`) and is freed, `push` stores the row it is given as the slot, and `copy` copies
  the elements from its source (`[][n]U`). `pop` returns the slot's block. Rows laid
  out in place (`[+]$[n]U`, `[+]$[n]$[m]U`, elem kind `"embed-row"`, shape key `evarr_…`) are
  consecutive slots of their element, built, dropped and copied through the element's own helpers; `push`
  moves the row's slots in and frees its block, `copy` takes the run of elements (`[]U` for owned ones, `@U` otherwise), and
  `pop` moves the row into a newly allocated block:
  - shape-kind\* - "varr"
  - shape-key\* - Shape key string (e.g. "varr\_int32", "varr\_P9\_\_pln\_B16", "evarr\_9\_\_pln\_B16")
  - elem\* - The element, as above

  **Dict entry** (`[""]T`; `T` any element of the varr entry): std/rt keeps the keys in a
  72-byte header whose last 24 bytes are the values, a `[+]T` header, one value at each key's
  position. A removed key's value is released at once, leaving its slots NULL (a row laid out
  in place, each of its elements' slots); the positions are compacted later. The values' varr
  entry is listed before this one. build-mgr declares the header as
  `type __pln_dict_T { int64 …; $__pln_varr_T values; }` and generates `__pln_alloc_dict_T()`,
  `__pln_free_dict_T(@!hdr h)` (NULL-safe: `__pln_fini_varr_T` on the values, then
  `__pln_dict_free`) and `__pln_copy_dict_T(@!hdr dst, @hdr src)` (nothing when `dst` is `src`;
  otherwise the keys by `__pln_dict_copy`, which keeps their positions, the removed ones
  included, then the values as one run by `__pln_copy_varr_T`). It also generates, with `k` an
  `@[]uint8`, `__pln_at_dict_T(@hdr h, k)` and `__pln_put_dict_T(@!hdr h, k)` (the address of k's
  value as `@!void`; `at` stops the program for a missing key through `__pln_dict_at`, `put` adds
  the key and a built value), `__pln_has_dict_T(@hdr h, k) -> bool`,
  `__pln_remove_dict_T(@!hdr h, k) -> bool` (releases the value, then `__pln_dict_settle`) and
  `__pln_clear_dict_T(@!hdr h)`. Each also takes the lengths the varr entry's functions do.
  As an element (of a `[+]T` or another dictionary's values), elem kind `"dict"` (`name`
  `__pln_dict_T`, `shape-key`), a slot holds the header: a new one is `__pln_dict_new()`, a dropped
  one is freed with `__pln_free_dict_T`, and a copy into one is `__pln_copy_dict_T`:
  - shape-kind\* - "dict"
  - shape-key\* - `dict_` followed by the element's `key` (e.g. "dict\_int32"); `edict_` for
    `[""]$T`, whose values are an `evarr_` entry
  - values\* - The shape key of the values' varr entry (e.g. "varr\_int32")

  **Bytes entry** (`__pln_B<n>`, the layout of a struct owning nothing that an element or an
  owned field holds): build-mgr only declares it, as `type __pln_B<n> { $[n]uint8 …; }`, for
  the functions of the arrays and structs holding it:
  - shape-kind\* - "bytes"
  - shape-name\* - `__pln_B<n>`
  - total-size\* - `n`

  **Opaque entry** (an opaque type an owned field or element holds): build-mgr declares it as
  a type of `total-size` bytes only to name the slots, and declares its release function in
  `__allocators.h` as `void <symbol>(void *p);`. A slot starts NULL and is dropped by calling
  the release function; nothing holding one is copied (palan-sa rejects it), so its copy
  helpers call `abort()`, and `push` stores the value it is given as the slot:
  - shape-kind\* - "opaque"
  - shape-name\* - The type's identity
  - total-size\* - 8
  - release-func\* - The type's release function (see Symbol names)

  **Struct entry** (a struct owning fields, or holding a type with its own `->`):
  - shape-kind\* - "struct"
  - shape-name\* - Struct name string
  - total-size\* - Total size in bytes (C ABI layout)
  - owned-fields\* - Owned fields (`T field`, `[n]U field`, `[n][m]U field`, `[+]U field`,
    `[n][+]U field`, ...), and arrays laid out in the struct whose elements own storage
    (`$[n]T field`, `$[n][m]U field`, `$[n]$[m]T field`, ...). Each is one element built,
    dropped and copied as an array element of that type is (a `[+]U` field's header is built
    empty with its struct); an array laid out in the struct is a row laid out in place, the run
    of its elements' slots. A `$[+]U field` is its header laid out in the struct, built empty
    with it (`__pln_init_varr_U(h, 0, 0)`), released with `__pln_fini_varr_U` and copied with
    `__pln_copy_varr_U`. A `$T field` of a struct owning fields lists `T`'s owned fields in
    place of itself, at their offsets in the outer struct and named by their path (`name.bytes`),
    unless `T` holds a type with its own `->` (see `runs`):
    - name\* - Field name string, or the path to it through structs laid out in the struct
    - offset\* - Byte offset within the struct
    - elem\* - The element, as in the arr and varr entries (`"struct"` for `T`, `"varr"` for
      `[+]U`, `"dict"` for `[""]U`, `"row"` for an owned array, `"embed-row"` for an array laid out in the struct),
      or `"embed-varr"` for `$[+]U`: the varr element's description with `"size"` 24
    - lens\* - The lengths of the field's levels, outermost first and on through the elements
      of a `[+]U`, which its element's helpers take (`[3]`, `[3, 4]`; empty for `T`)

  - init-func - The symbol of the function storing the struct's field initializers (see Symbol
    names), present when it or a struct laid out in it has one; `__allocators.h` declares it as
    `void <symbol>(void *p);` and `__pln_init_T` calls it after the owned fields are built. A
    struct with initializers has a struct entry even when it owns nothing.
    A struct with initializers that owns nothing also has `__pln_initrun_P(@!P run, int64 from,
    int64 to)`, which builds `run[from]` to `run[to - 1]`. palan-sa calls it after obtaining an
    array laid out in one block (`[n]$P`, `[m]$[n]$P`); the generated helpers call it wherever
    elements laid out in place come into use (`[+]$P`'s resize, a row block of `$P`).

  - runs - Structs laid out in the struct (`$T`, `$[n]$T`) whose type holds one with its own `->`,
    each kept whole and handled by its type's functions (`__pln_init/fini/copy_T`, and its `->`
    or `__pln_assign_T` when written into); omitted when none:
    - offset\* - Byte offset within the struct
    - name\* - The struct
    - count\* - How many of it lie there in a row (1 for `$T`)
  - copy-func - The symbol of the struct's own `->`, which `__allocators.h` declares as
    `void <symbol>(const void *src, void *dst);`
  - assigns - `true` when the struct holds a type with its own `->`, itself or in a field or
    element at any depth. build-mgr then also generates `__pln_assign_T(@!T dst, @T src)` (unless
    it has `copy-func`, which is called instead), which writes into an existing `dst`: as
    `__pln_copy_T`, but writing the parts of such types through their `->`. Such a struct is never
    reduced to a bytes entry.

  `__pln_copy_T` and `__pln_assign_T` treat an owned field that is NULL as the arr entry's do an element.

  Non-owned fields are deliberately not described: build-mgr re-declares the struct in the
  generated allocator module with only the owned fields at their offsets and opaque padding
  elsewhere, because the full layout cannot always be re-expressed as Palan source (C unions,
  synthesized anonymous tags, embedded types not declared in that module).

  Struct entries (and the struct and varr entries their fields' elements need) are ordered
  leaf-first (topological order) so build-mgr can generate and compile allocators in dependency
  order.

String literal table entry
--------------------------
- label\* - Assembly label string (e.g. ".str0")
- value\* - The bytes as escaped text, as the lit-str value of ASTSpec.md

Function model (sa.json)
------------------------
Same structure as the Palan function definition in ASTSpec.md, with `body` replaced by
SA-annotated statements.

- name\* - Symbol name of the function (see "Symbol names" below), not the written name
- func-type\* - "palan"
- export - `true` for an exported function, for a type's `->`, for an opaque type's release
  function and for the module's `..init`, which codegen makes global
- parameters\* - Parameter list (Palan parameter: name, var-type; empty array when no parameters)
- ret-type - Return variable type (single-return functions only)
- rets - Return value list (multi-return functions only)
- ret-slot - Present when the single return value is a struct (not an opaque type, which is
  returned as its owner, a pointer): `{"name":<variable>,"size":<bytes>,
  "eightbytes":[...]}` (`eightbytes` as in a call's `struct-ret`). The body writes the value into
  the struct variable `name` (the named return, or `__ret` for an unnamed one) and returns with a
  bare `return`. With `"eightbytes":[]` (MEMORY class) `name` is bound at entry to the hidden
  first argument; otherwise the body declares it as a `frame-alloc` and codegen returns its
  eightbytes in registers. In `rets`, a struct entry carries `"slot":true`: its variable is bound
  at entry to a hidden argument, one per struct return in order, ahead of the parameters.
- globals - The names the function binds to static slots at its entry: `name`, `label` and
  `var-type` of each Palan global it is permitted to use (`var-type` is how it sees the global;
  see Static variables below) and each C global it uses. Omitted when there are none.
- body\* - SA-annotated statement list (same rules as top-level statements below)

Note: a `syscall` declaration is a prototype with no block, so it never appears in this
`functions` array (local or imported). Its signature lives only in a call node that resolves to
it -- see "call expression" below.

Note: cinclude statements are not present in function bodies (they are top-level only).

### Symbol names

Every Palan function links by a symbol SA builds from its module and its parameter types, so
functions of one name (overloads, or the same function exported by two modules) have distinct
symbols. It is the `name` of the function, of a `call` resolved to it and of a `func-ref` to it.
C functions, syscalls and `cexport` functions are not renamed. A module's top-level function is `<module>..main` and its
initialization function `<module>..init`, which no other function's symbol can be: theirs always
end in a parameter list or `.v`.

`<module>..<name>.<param>.<param>...` — `<name>.v` when there are no parameters.

A function defined inside another one has the enclosing function's symbol in place of
`<module>` (`util..a.v..f.v`). A `{ }` block that defines functions adds `..b<n>`, its order
among such blocks of the enclosing function or of the top level (`util..a.v..b1..f.v`,
`util..b1..f.v`).

A type's `->` (SpecAndDesign.md 3.8) is `__pln_cp_` and the struct as a `<param>` writes it
(`__pln_cp_24__pln_L10std_2eutf8_utf8`): a type has one, so the type alone names it, and the generated allocators
declare it in C, which takes no `.`.

A struct's field initializer function (SpecAndDesign.md 3.6) is `__pln_fi_` and the struct as a
`<param>` writes it, for the same reasons. SA defines it in the struct's module, taking `@!T p`:
a struct laid out in it with initializers is stored by its own function first, then one
`value -> p.field` stores each initializer.

An opaque type's release function (SpecAndDesign.md 3.7) is `__pln_rl_` and the type as a
`<param>` writes it. SA defines it in the type's module, as a function of that name taking the
owned type (`h`): it does nothing when `h` is NULL, and otherwise calls the function marked
`release`, or without one releases the contents. Everything releasing such a value calls it, and
the function marked `release` releases its own parameter as the contents.

- `<name>`: the function's name; an operator function's is a word that starts with a digit, so that
  no written name can be it: `0add` `0sub` `0mul` `0div` `0mod` `0eq` `0lt` `0app` for
  `+ - * / % == < <<` (`vecmath..0sub.R4vec3.R4vec3`).
- `<module>`: the `module` of ast.json: the source file's name without `.pa`, with each byte outside
  `[A-Za-z0-9_]` and each `_` followed by two lowercase hex digits written `_` and two lowercase hex
  digits, and `_` in front of a leading digit (`001_hello.pa` → `_001_hello`), after the name of the
  file's `library` declaration if any (`library std;` in `utf8.pa` gives `std.utf8..len.v`). The
  allocators build-mgr generates are in module `__allocators`, or `<module>.__allocators` for a module
  with a `library` declaration (`std.utf8.__allocators`), which SA names in the calls it makes to them.
- `<param>`: the parameter's normalized `var-type`, each type self-delimiting:
  - prim: its name (`int32`, `flo64`, `bool`, `void`); an enum: `N<len><enum name><base prim>`
  - struct: `<len><type-name>`, the type's identity (the examples here write the name alone)
  - pntr: `E` if `embedded`, `S<stride>_` if `stride`, `A<arr-size>_` if `arr-size`, `U`
    if `unsized`, then `P` (no `mutable`: owned or by value), `R` (`mutable: false`) or `W` (`mutable: true`), then the
    base type
  - func: `F<parameter count>_`, each parameter type, then the return type (a `void` prim when none)
  - varr: `V` (`VC` if `embedded`), then the element type (`@![+]int32` is `WVint32`,
    `[+]Point` is `PVP5Point`, `[+]$Point` is `PVC5Point`, `[+][4]int32` is `PVA4_Pint32`)

The return type is not part of the symbol, since functions differing only by it cannot coexist.
Two parameter types that differ in any key give different symbols. `func show(int32)` in
`util.pa` is `util..show.int32`; `func cmp(@int32, @int32)` is `util..cmp.Rint32.Rint32`.

Note: `[]T` (unsized array type) in `ret-type` or parameter `var-type` is resolved by SA to
`pntr(T)` with no ownership tracking. `[][]T` becomes `pntr(pntr(T))`. `[][m]T` becomes
`pntr` over the same row type a `[n][m]T` variable has (`pntr` with `"arr-size": m`); `m` must be
a compile-time constant. `[]@![]T` becomes `pntr` over the element type a `[n]@![]T` variable
has (`pntr(T)` with `"mutable": true`).
These types are valid only in function signatures; using `[]T` in a `var-decl` is a compile error.

A sized `[n]T` parameter or return type (of any element and nesting a variable may have) is the
type a variable of it has (`pntr` levels with `arr-size` and no `mutable`; `[3]int32` is
`A3_Pint32` in a symbol); every size must be a compile-time constant (E_SigArrSizeNotConst). It
is owned as a `[+]T` parameter or return value is (see `[+]T` below): a parameter takes a call
result as it is and otherwise a copy, of what a copy into it accepts (E_IncompatibleTypes);
`return` copies a value the function doesn't own, and any value whose shape a copy would reject,
an owning variable's included, is E_CopyShapeMismatch; `return [..]` declares `__ret_N` from the
literal, as for a struct. A borrowed array return (`-> @[]T`, `@[n]T`, `@[+]T`) is normalized as a
parameter is (see `@[n]T` below); its call is a borrow, bound and passed by name.

A struct parameter `T` (`pntr(struct)` with no `mutable`) is owned the same way: a call result
is passed as it is, anything else (a struct literal included) is copied into a `temp`
(`__arg_N`), and the function frees it at exit unless it returns it or gives it away with
`->>`. The functions build-mgr generates for structs (`__pln_init/fini/free/copy_T`) take them
as `@!T` (`copy`'s source as `@T`), since they work on storage they do not own.

Statement model
---------------
Same structure as AST statements (see ASTSpec.md) with the following differences:

- cinclude statements are consumed by SA and not emitted; the header's
  `typedefs` list (ASTSpec.md's Typedef definition model) is registered into
  the same type-alias table a native `type X = ...;` uses (see Type Aliases
  in PalanReference.md), unconditionally for every typedef the header
  defines -- independent of whether any C function/global in the header
  actually references that typedef in its signature. A `typedef-name` hint
  carried on a function/global's own var-type node (ASTSpec.md `typedef-name`)
  registers the same alias a second time as a side effect of resolving that
  node; this is a harmless no-op re-registration (same name, same resolved
  type) whose real job is stripping the hint from that node so it never
  reaches sa.json. Object-like-macro constants are no longer an SA concern as
  of v0.1.33: every reference is already folded to a `lit-int` by
  gen-ast (ASTSpec.md's Constant definition model and `lit-int` entries), so
  a `cinclude` statement never carries a `constants` field by the time SA
  sees it, and SA has no macro-name table of its own (a macro c2ast could not
  export reaches SA only as the `c-macro` mark on a reference). A function-like
  macro reaches SA as the statement's `expandables` instead, scoped like its
  `functions`; a call to one is analyzed as its expanded body, so sa.json
  shows only the body's nodes.
  `functions`, `globals`, `structs` and `typedefs` are
  independent sections of the header's AST -- a header exporting only some of
  them (e.g. `stdint.h` with only `typedefs`) still has each present section
  registered; none is gated on another's presence. For a top-level cinclude,
  `structs` and `typedefs` are registered early, before any native type
  declaration is resolved and before Palan function signatures are
  pre-registered, so a native struct field or a Palan function signature
  anywhere at the top level can name a C type. `functions` and
  `globals` are still registered at the statement's own position. A cinclude's `libs` (ASTSpec.md's
  Statement model, from a `link` clause) is handled differently from the
  sections above: instead of being registered into a table and discarded, each
  name is validated (`E_InvalidLinkLibName` on an invalid one) and added to a
  single deduplicated set that is emitted as the top-level `libs` array (see
  Root above) regardless of which scope -- top level, block, or function body
  -- the cinclude appears in, since linking is a whole-program property with
  no lexical scope.
  C global variables (see ASTSpec.md's "Global variable model", the header's
  `globals` list) are registered the same way as typedefs, not
  like C functions: always unqualified, even under `cinclude ... as S;`
  (only `S.func(...)` calls require the alias qualifier). A registered global
  is a static variable (see Static variables below), writable only where a
  `global` statement permits it (`E_GlobalNotWritable`). Visibility follows the same scope rule as C functions
  from cinclude (see the **block** entry below): visible from the cinclude
  point to the end of the enclosing scope. A global whose type SA cannot
  represent (see PlnType.h's `unrepresentableTypeName`, e.g. C's `long
  double`) does not make the cinclude itself fail -- only referencing that
  particular global does, with `E_UnsupportedCGlobalType` (mirrors
  `_unsupported-sig`/`E_UnsupportedCFuncSignature` for C functions).
- import statements are consumed by SA and not emitted; imported functions are
  registered in the current scope and become callable from the point of import.
  Exported types and function signatures are resolved in the exporting module: each
  module has one analyzer for the whole program, which resolves that module's
  declarations as they are needed, and SA copies the results over.
  - Top-level type and const declarations are resolved on first need, by name: a
    module's own declaration, or a type a top-level import brings in. A pre-scan
    resolves all of them (imports first) before function signatures are pre-registered,
    so their order in the source does not matter. A block-scoped import's types are
    registered at the import and writable until the end of the block.
  - Reaching a declaration that is still being resolved is a cycle, reported as
    `E_CircularTypeDecl` at the declaration that reached it, except that a struct
    embedding itself (`$T`, `$[n]T`) is `E_RecursiveStruct`. A struct named through a
    pointer only has to be declared; it is resolved once no declaration is being
    resolved, so a pointer may close a cycle.
  - A struct comes with every struct and enum its fields use, and a signature with every
    type it names. A Palan type source may write is bound to the name it is written as
    (`T`, or `V.T` under an alias); one not selected or not exported is registered but
    unbound. Writing an unbound Palan type name is `E_UnqualifiedAliasType` (imported
    under an alias) or `E_UnknownType`. C types are copied without binding; they are
    global as in C and written by their own name.
  - A struct or enum is named by its identity in sa.json (build-mgr keys struct
    allocators by it): `__pln_L<len><module>_<Name>`, with the module's `.` written `_2e`
    and `<len>` that string's length (`std.utf8`'s `utf8` is `__pln_L10std_2eutf8_utf8`),
    exported or not, and `__pln_L<len><module>_<seq>_<Name>` for one declared in a block.
    Modules and blocks may each declare their own; a message shows the written name. A
    type of a generated allocators module (`__allocators`, `<module>.__allocators`) keeps
    its written name, already an identity. An alias has no identity.
  - A type declaration clashes only with names written in its scope: a type an import
    makes writable, a C type, a const, or another declaration. An imported type that is
    not writable does not clash.
- type-alias statements are consumed by SA and not emitted; the aliased type is
  resolved where the alias is declared, so its names keep meaning the types they
  named there, and inlined at each reference site (see Type Aliases in
  PalanReference.md)
- global-permit statements are consumed by SA and not emitted. A `var-decl` with `global` is
  emitted into the module's `..init` as a top-level variable's would be, its variable given
  `label` (see Static variables below);
  it is `E_GlobalNotTopLevel` anywhere but the top level, as a `global-permit` is. A permit names
  a `global` variable of the module, one imported at the top level (`M.count` under an alias, the
  alias checked by `E_UnknownAlias`) or a C global visible there (`E_GlobalPermitUnknownVar`) and
  top-level Palan functions of the module (`E_GlobalPermitNotFunc`), each once
  (`E_GlobalPermitDuplicate`).
- const-decl statements are consumed by SA and not emitted; the value is
  folded to an untyped literal and inlined at each reference site (see
  Constant Declarations in PalanReference.md)
- a constant expression (literals, consts, `sizeof` and the `len` of a constant-size array under
  the arithmetic, bitwise and shift operators) is folded to one untyped `lit-int`/`lit-uint`/`lit-flo`
  before it is typed, in every expression position; `E_ConstExprOverflow`/`E_ConstExprDivByZero`
  reject an invalid one
- `len(@a)` / `len(@!a)` / `a -> len()` of an array level with `arr-size` (a variable, row, field or
  borrowed array) folds to that `arr-size`, so it is no call and no candidate; like `sizeof`, the
  location's indexes are not evaluated. Of a `run-size` level it is the length kept for it
  (`__<name>_d<k>` of the location's root variable, converted to int64), which is not folded; of
  a level with neither (`[]T` parameter) it is E_ArrLenNotConst; without `@`, E_ArrLenNeedsAddrOf
  unless a user function named `len` exists. A value that is neither an array nor a `[+]T` is
  E_LenNotArray unless a user function named `len` exists
- not-impl statements (ASTSpec.md) are rejected, never emitted: `E_VarTypeInferenceNotImpl` when
  the statement carries `untyped-var`, `E_StmtNotImplemented` otherwise; a not-impl expression in
  value position is rejected as `E_ExprNotImplemented`
- a function signature holding a not-impl parameter or return (ASTSpec.md's Palan Parameter) is
  rejected where the function is registered, whether called or not, local or imported:
  `E_ParamDefaultNotImpl` for a parameter with a default value, `E_ParamNotImplemented` for any
  other parameter, `E_RetNotImplemented` for a return
- var-type resolved on id expressions (see Expression model below)
- assign and return statements are emitted as-is with SA-annotated expressions

- **block** - standalone block statement
  - stmt-type\*: "block"
  - body\*: statement list (cinclude consumed; func-def hoisted to sa["functions"])

  Scope rules:
  - Variables and Palan functions declared inside a block are not visible outside.
  - Shadowing of variables or Palan function names across any scope boundary is
    a compile error (BOOST_ASSERT).
  - C functions from cinclude inside a block are visible only until the end of that
    block. Shadowing of C function names is permitted.
  - Palan function definitions inside a block are pre-registered (pass 1) to
    support forward references within the same block.
  - Struct, enum, type-alias, and const declarations inside a block (a function body
    included) are resolved on entry, before its functions are pre-registered, and are
    not visible outside it; nor are the type names a block's import makes writable.
    A block's struct or enum is named `__pln_L<len><module>_<n>_<Name>` wherever the
    type appears in sa.json (`type-name`, `alloc-shapes`), so blocks may reuse a name.

- **if** - if / if-else statement
  - stmt-type\*: "if"
  - cond\*: SA-annotated condition expression (value-type present; integer or bool expected)
  - then\*: then-block (same structure as block stmt body in ASTSpec.md)
  - else: else-block or nested if statement (omitted when absent)

- **var-decl** (array transformation) - Array variable declarations are transformed by SA:

  Each fixed level's size of the variable's own blocks (the array, and the rows of `[m][n]T`)
  and a `[+n]T`'s capacity are evaluated once per statement. A constant is used as it is; a size
  known only at run time is held in a temp var `__<name>_d<k>` (uint64, `k` counting levels from
  the outermost), declared before the variable, which its allocation, its free and a row index
  without a `stride` read. Each later variable of the statement declares its own, copied from the
  first variable's. Unless palan-sa is given `--release`, the first variable's is followed by
  `if <it as int64> < 0 { __pln_negative_size(); }` (a C call to `std/rt`).

  **`[n]T` (raw array):** A `var-decl` with `arr` type-kind (`specifier: "raw"`, non-null `size-expr`,
  prim `base-type`) is rewritten as a `var-decl` with `pntr` type and a synthesized `__pln_malloc` call
(`std/rt`'s `malloc`, which aborts when it fails) as init:
  - `var-type`: changed from `arr` to `pntr` (pointer to the element type)
  - `init`: `{"expr-type":"call","name":"__pln_malloc","func-type":"c","args":[<size_bytes>],"value-type":<pntr_type>}`
    where `size_bytes` = the size (when elem_size == 1) or `mul(<size>, lit-uint(elem_size))` otherwise
  - The variable is registered in scope with `pntr` type
  - The `size-expr` must evaluate to an integer type; a float size is a compile error

  **`[n]@![]T`, `[n]@[]T` (array of pointer slots):** A `var-decl` with `arr` type-kind where
  `base-type` is `pntr(mutable, base=arr(T))` is also transformed:
  - `var-type`: changed to `pntr(pntr(T, mutable))` (pointer to pointer); like `[n]@T`/`[n]@!T`,
    the element carries the slot's permission, so a slot is a borrow and `->` into it stores the
    pointer. A read-only slot to rows (`[n]@[]$[m]T`) has `"mutable": false` on each row level
    too, as a read-only `@[k][m]T` does
  - `init`: `__pln_malloc(size-expr * 8)` (each slot is a pointer; elem-size is always 8)
  - The outer array is freed at scope exit. What the slots point to is not freed; a slot cannot
    take ownership with `->>` (E_TransferToBorrowSlot).

  **`[m][n]T`, `[m][n]$T` (2D array of owned rows):** An array of `m` owned row blocks, lowered
  like `[n]T` for a struct `T` with the row as the element (an arr entry with a `"row"` element).
  - `var-type`: `pntr(row)`, the row being `pntr(T)` for a primitive `T`, `pntr(pntr(struct(T)))`
    for a struct `T`, and `pntr{embedded, stride, base-type: struct(T)}` for `$T` (which must own
    nothing); `arr-size` is on both levels when constant.
  - `init`: palan call to `__pln_alloc_arr_<E>(<m>, <n>)`, `E` the row's key (`arr_Pint32` for
    `[m][n]int32`); scope-exit free: `__pln_free_arr_<E>(<name>, <m>, <n>)`. A constant length is
    passed as an int64 literal.

  Other 2D struct forms (`[m]$[n]T`, `[m]$[n]$T`) are compile errors.

  **`[n]$[m]T` (contiguous 2D array):** A `var-decl` with outer `arr` type-kind where
  `embedded: true` and `base-type` is an inner `arr(prim T)` is transformed to a single
  contiguous allocation:
  - `var-type`: changed to `pntr(pntr(T [, arr-size:m]), embedded:true [, stride:m*sizeof(T)])`:
    the outer level holds the rows in place, each `stride` bytes; the row is a level of its own.
    - `stride` and the row's `arr-size` are present when `m` is a compile-time constant; absent
      when `m` is a runtime variable
  - When `m` is a constant (e.g. `[n]$[4]int32`):
    - `init`: C call `__pln_malloc(mul(<n-expr>, lit-uint(m * sizeof(T))))`
  - When `m` is a variable (e.g. `[n]$[cols]int32`):
    - `init`: C call `__pln_malloc(mul(<n>, mul(__<name>_d1, lit-uint(sizeof(T)))))`
  - Scope-exit free: C call `free(<name>)` (no custom allocator; no `alloc-shapes` entry needed).
  - `n` (row count) is not stored; only `m` (inner dimension) needs to be retained for row indexing.

  **`[+]T` / `[+n]T` (variable-length array):** The variable is one word pointing
  at a heap header `{int64 len; int64 cap; T* data}` (24 bytes), allocated even when empty so
  that it never needs a NULL test; only `data` waits for a capacity.
  - `var-type`: `pntr(varr(E))`. `varr` (`{"type-kind":"varr","base-type":E}`) appears only as
    this pointer's base; its layout is known to SA alone, and codegen sees a pointer. `E` is one
    element as `data` holds it: a primitive `T`; for a struct `T`, `pntr(struct(T))` (an owned
    slot, as a `[n]T` element) for `[+]T` and `struct(T)` for `[+]$T` (E_EmbedOwningType if
    `T` owns fields), with `"embedded": true` on the `varr`; for a `[+]U` element (`[+][+]U`, any
    depth), the element's own owned `pntr(varr(…))` (`[+]$[+]U` is E_EmbedOwningType, `[+][+n]U`
    E_VarrElemSized); for a row (`[+][n]U`, `[+]$[n]U`), the level a row of `[m][n]U` has
    (`pntr` with `arr-size`, over a primitive, an owned struct slot, a struct laid out in place
    for `[n]$U`, or an owned `[+]V`), with `"embedded": true` on the `varr` for `[+]$[n]U`. The
    row size must be a constant (E_UnsizedArrRowSizeNotConst). Further levels follow the same
    rules at any depth (`[+][m][n]U`, `[+][m]$[n]U`). For a pointer element (`[+]@T`, `[+]@!T`,
    `[+]@[]T`, `[+]@[+]T`, `[+]@[""]T`), `pntr(T, mutable)`, the slot a `[n]@T` element is; it borrows and is laid out as
    an `int64`, so its shape key is `varr_int64`. A row of pointer slots (`[+][n]@T`) is laid out as
    a row of `int64` the same way (`varr_Pint64`). Below, `T` stands for `E` and `varr_T` for the shape key, made from
    `E`'s layout.
  - `init`: `__pln_alloc_varr_T(len, cap)` (see the varr `alloc-shapes` entry): `(0, 0)` for
    `[+]T`, `(0, n)` for `[+n]T` (`n` is converted to uint64 like `[n]T`'s size), and `(n, n)`
    with an array literal, whose size is the length. The literal's size is its element count
    when omitted, and otherwise must be a constant no smaller than the count; the elements past
    the literal's are undefined.
  - Scope-exit free: `__pln_free_varr_T(<name>)`. `$[+]T` places the header in the frame:
    `__pln_init_varr_T(<name>, len, cap)`, released by `__pln_fini_varr_T(<name>)`.
  - `arr[i]` on such a variable indexes its data: SA replaces the `array` operand with
    `{"expr-type":"field-access","ptr-expr":<array>,"offset":16,"value-type":pntr(E)}`, and the
    `arr-index` is then the one for a `pntr(E)`; for `[+]$T` and `[+]$[n]U` the `pntr` carries
    `embedded` and `stride` (the struct's or the row's bytes) as a `[n]$T` or `[m]$[n]U`
    variable's does.
  - A C call argument (fixed or variadic) and a `[]T`-family Palan parameter (an unsized
    elements pointer: no `mutable`, no `arr-size`) are passed the same `field-access` to `data`,
    and so is a borrow (`@v`/`@!v`, or a `@[+]T` variable) bound to a `@[]T`/`@![]T` (an unsized
    elements pointer with `mutable`) parameter, variable, field or return value.
    Through a read-only borrow it is typed `pntr(T)` with `"mutable": false` (and a row element
    has `"mutable": false` on its levels, as a read-only `@[m][n]U`'s row does), so writing an
    element or passing it to `[]T` is rejected as for any `@T`. An owned element (a struct, a
    `[+]U`) reached through a read-only array is read-only too. A sized `@[n]T`/`@![n]T` parameter does
    not take a `[+]T` (E_ArrBorrowShapeMismatch). Growing the array moves `data`, leaving such a
    pointer or a `@!v[i]` dangling; this is not checked.
  - `@v` / `@!v` borrows the array as it is: the value is the header pointer, typed
    `pntr(varr(T))` with `"mutable": false` / `true` (`@[+]T` / `@![+]T`; the owner's type has no
    `mutable`). Given to such a parameter, the array must be written with `@`/`@!`
    (E_VarrBorrowNeedsAddrOf); a chained value takes `@` for `@[+]T` as for other read borrows.
  - `@[+]T` / `@![+]T` written as a parameter or local variable type is normalized to the same
    `pntr(varr(T))` with `mutable` (a size, `@[+n]T`, is E_VarrBorrowSized), and registers `T`'s
    varr shape as a declaration would. Such a variable is bound with `@v`/`@!v` or another borrow,
    can be rebound by `->`, and is never freed.
  - Built-in functions: once `T` is used, the callee lookup also finds `len(@[+]T) -> int64`,
    `push(@![+]T, T)`, `resize(@![+]T, int64)` and `pop(@![+]T) -> T` (a struct element is
    passed and returned as the struct's storage `T`; a row `[n]U` is pushed from a `@[n]U` and
    popped as an owned `[n]U`; `pop`'s result is the caller's), as ordinary Palan
    functions alongside C, local and imported ones (overloading, method form and chaining apply
    unchanged). `len` becomes the header's len, `field-access` at offset 0 of the borrowed
    header pointer, typed `int64`; the others are a `palan` call to
    `__pln_push/resize/pop_varr_T`, whose symbol is built from the generated signature (see the
    varr `alloc-shapes` entry); the built-in keeps `E`'s own types, and only the function it calls
    is shared with every element laid out alike. A Palan function with a built-in's name and parameter types, for
    any `T`, is E_DuplicateFuncDef regardless of declaration order, as is a `len` taking one
    borrowed constant-size array (that `len` is folded).
  - Copying (`[+]T w = v;`, `v -> w`, and an owned parameter, below) takes a `[+]T` or
    `@[+]T` / `@![+]T` (`__pln_copy_varr_T(w, <v's data>, <v's len>)`) or a sized `[n]T` of the
    same element type and layout (`[n]T` into `[+]T`, `[n]$T` into `[+]$T`, `[m][n]U` into
    `[+][n]U`, `[m]$[n]U` into `[+]$[n]U`, also through a borrow;
    `__pln_copy_varr_T(w, a, n)`); the length becomes the source's. A `[+]` source of the other
    layout is E_CopyShapeMismatch too. Into a
    `[n]T` it is E_CopyShapeMismatch. `[..] -> w` (also into an element, `[..] -> m[i]`) is a
    block declaring the literal as a frame array `$[]T __lit_N` (an empty `[]` as a frame
    `$[+]T`) and copying it, so every element is read before `w` changes; its diagnostics name
    `w`.
  - `->>` into such a variable is the transfer of any owned value: `__pln_free_varr_T(w)`, the
    assign, and the source set to NULL. The source is an owning variable (not `$[+]T`) or a call
    result, of the same element type.
  - A return value `-> [+]T` (named or not) is the caller's, as a struct's is: a named one is allocated at entry and can't have an initializer
    (E_NamedRetInitOnStruct); `return v;` of an owning variable returns its header, and a borrow
    or a `$[+]T` variable is returned as a copy (`__ret_N`). The caller's `[+]T w = f();`
    takes the header over, and a call result copied into a variable, given to a borrow, or
    discarded is held in a temp and freed with `__pln_free_varr_T`. A borrowed struct returned
    from a function returning a struct is copied the same way.
  - `pop` of a row returns an owned `[n]U`: a heap `[n]U r = v.pop();` takes the block over
    (freed at scope exit as `[n]U` is), and a result copied into an array, given to a borrow
    (`f(@v.pop())`), or discarded is held in a temp and freed as the variable would be (`free`,
    or `__pln_free_arr_E` for owned elements). `->>` from a row element leaves its slot NULL.
  - An owned parameter `[+]T x` (symbol `PV` + element) takes what the caller passes over and
    frees it when the function exits. A call result is passed as it is; anything else that a
    copy into `[+]T` accepts, or an array literal, is copied into a `temp` (`__arg_N`) whose
    `release` is empty.
  - An element `[+]U` (`[+][+]U`, `[n][+]U`) is owned by the array, as a struct element is:
    `m[i]` is the element's header pointer (typed as the owned `[+]U`), `@m[i]`/`@!m[i]` borrows
    it in place (so `m[i].push(x)` and `m[i] -> len()` apply), and copying, transferring and
    freeing the outer array reach it through the generated helpers. `[n][+]U` is an arr entry of
    the element (`__pln_alloc_arr_PVU(n)` …, or in the frame for `$[n][+]U`), and `@[n][+]U`
    borrows it with each element left as the owned `[+]U`. Not supported: `[n]$[+]U`
    (E_EmbedOwningType), an unsized `[][+]U`
    (E_UnsizedVarrElemArr). An array literal fills `[+]` levels at any depth: each inner row is
    resized to its own count (`__pln_resize_varr_U(<row>, count)`, an empty `[]` included) before
    its elements are stored; a fixed level still needs the exact count, and a fixed size taken from
    an empty `[]` is E_ArrLitEmptySize.
  - A field `[+]U` is an owned field (see the struct entry of alloc-shapes): its header is built
    empty with the struct, and `s.a[i]`, the builtins, borrows, copies and `->>` treat `s.a` as a
    `[+]U` variable. A struct literal resizes it to the literal's count before storing the
    elements, as an inner row. `[+n]U` is E_VarrElemSized.
  - A field `$[+]U` lays the 24-byte header out in the struct. `s.a` is a `field-access` with
    `addr-only: true` and the same `pntr(varr(E))` type as an owned `[+]U` field, so everything
    above applies to it, except that it is neither the source nor the target of `->>`
    (E_TransferFromNonOwner, E_TransferToNonOwnedField), as the header of a `$[+]U` variable.
    `$[n]$[+]U` is E_EmbedOwningType.

  **`[""]T` (dictionary):** The variable is one word pointing at a header std/rt allocates
  (the dict `alloc-shapes` entry). It follows `[+]T` above where nothing else is said.
  - `var-type`: `pntr(strdict(E))`; `strdict` (`{"type-kind":"strdict","base-type":E}`, with
    `embedded` for `[""]$T`) appears only as this pointer's base, its kind other than the written
    `dict` so that an analyzed type is not read as a written one. `E` is any element of a `[+]T`,
    as `varr`'s is (a dictionary's being its `pntr(strdict(…))`); `[""]$T` of a type owning
    storage is E_EmbedOwningType.
  - A dictionary as a field or an element (`[n][""]T`, `[+][""]T`, another dictionary's value) is
    an owned slot of elem kind `"dict"`, starting empty, as a `[+]T`'s is; a struct literal and a
    field initializer give it a dictionary literal. Laid out in place (`$[""]T` as a field,
    `[n]$[""]T`, `[+]$[""]T`, `[""]$[""]T`) it is E_EmbedOwningType, as a variable E_FrameVarType,
    since std/rt allocates the header.
  - `init`: `__pln_alloc_dict_T()`; scope-exit free: `__pln_free_dict_T(<name>)`.
  - Copying takes a `[""]T` or `@[""]T`/`@![""]T` of the same `E` (`__pln_copy_dict_T(w, v)`);
    anything else is E_CopyShapeMismatch. `->>`, returns, owned parameters (symbol `PD` +
    element), `@v`/`@!v` (`pntr(strdict(E))` with `mutable`; without `@`,
    E_DictBorrowNeedsAddrOf) and `@[""]T`/`@![""]T` types are as for `[+]T`.
  - `d[k]` is an `arr-index` by `lit-int` 0 whose `array` is a call of `__pln_at_dict_T(d, k)`,
    typed `pntr(E)` as a `[+]T`'s data pointer is (read-only through `@[""]T`), so the element
    is read, written into, borrowed and given away as a `[+]T`'s is. `k` is checked as an
    argument to `@[]uint8`, but an array is given by name (`d[buf]`).
  - A whole store (`x -> d[k]`, `x ->> d[k]`, a literal included) is emitted as: the value, when
    it has a call, into a `pinned` temp (`__pin_N`); then `__pin_M` initialized to
    `__pln_put_dict_T(d, k)`; then the store into `__pin_M[0]`. A literal is first put into a
    temp (`__lit_N`), which is then stored so. Through `@[""]T` it is E_WriteThroughReadOnlyPtr.
    A constant key (`d[K]`) is its string.
  - A literal `{"a": v, K: w}` initializing `d` is emitted as `d`'s declaration, then for each
    key in order `__pin_N` initialized to `__pln_put_dict_T(d, key)` and the value stored into
    `__pin_N[0]` as an array literal's element is; a value that is itself a dictionary literal is
    written so into the value its key's adding builds, after `__pln_clear_dict_T` on it. The
    values' rows are checked as an array literal's. The key is a `lit-str`, written or a string
    constant's (otherwise E_DictLitKeyNotConst); a repeated key is E_DictLitDupKey. The values
    are analyzed before `d` is declared. Elsewhere (`->`, a return, an argument) the literal
    initializes a temp `__lit_N`/`__ret_N` that is used in its place, as a `[+]T` literal is; an
    argument to `@[""]T` is written `@{..}`. A literal whose keys are all strings or string
    constants fits a dictionary parameter, and one with a string key fits no struct parameter.
  - Builtins, as `[+]T`'s are: `len(@d)` (inline, the header's first word), `has(@d, k)`,
    `remove(@!d, k)` and `clear(@!d)`, calling the generated functions above. A function with
    the parameter types of one is E_DuplicateFuncDef.
  - In a `for` loop's checks (E_ChangeForRange), `d["a"]` and `d["b"]` with constant keys are
    different values, as `v[0]` and `v[1]` are.

  **`[]$[m]T` (unsized embedded array parameter):** Normalized by SA to the same
  `pntr(pntr(T, arr-size:m), embedded:true, stride:m*sizeof(T))` type as the sized form. The inner dimension `m`
  must be a compile-time constant (`E_UnsizedArrRowSizeNotConst`). Without it there is no stride, so
  `[]$[]T` is `E_EmbeddedArrUnsizedInner` wherever the type is written (a parameter, `@[]$[]T`, an
  element `[+]@[]$[]T`); a declaration's initializer literal gives the lengths first.

  **`arr-index` on embedded array (row access):** When the array expression has an embedded
  pntr type (`embedded:true`), `arr[i]` is treated as a row access:
  - `value-type`: the row level, `pntr(T [, arr-size:m])`, with `addr-only: true` (the row is
    laid out in place, not loaded)
  - `elem-size`: the row stride in bytes:
    - Constant `m`: `lit-uint(stride)`
    - Variable m: `mul(__<name>_d1, lit-uint(sizeof(T)))`
  The subsequent `arr[i][j]` operates on the row like any `pntr(T)` level, with elem-size `sizeof(T)`.

  **Function call type compatibility for embedded args:** When an argument is passed to a
  parameter of type `pntr(pntr(T, arr-size:m), embedded:true, stride)`, SA checks (at the raw JSON level):
  - The argument's `value-type` must also have `embedded:true` and `stride`, with a row of `arr-size:k` where `k == m`.
  - Mismatch (or a variable row size in the argument) is a compile error (`E_EmbeddedArrInnerSizeMismatch`).

  **`arr-size`:** Every `pntr` level that stands for an array dimension carries `arr-size`
  (its element count) when that size is a compile-time constant (a literal or a `const`),
  including an array field's value-type. It is absent for a size known only at run time, where
  an owned variable's level (and so its rows') has `"run-size": true` instead, the size being
  kept in `__<name>_d<k>`; a `[]T` parameter's level has neither. Both are only used by SA for
  borrowed-array, ownership, copy and length checks; codegen ignores them. A `for` over a
  `run-size` array declares the lengths again for the variable it copies the range into, as
  `__<copy>_d<k>`. A copy or a free of an array
  takes its lengths from the location receiving or releasing it, and a `run-size` level matches
  any size.

  **`@[n]T` / `@![n]T` (borrowed array):** In a parameter, a return type or a local variable declaration, a
  `pntr` whose `base-type` is an `arr` is normalized to the array's own pntr chain, with
  `arr-size` on every level and `mutable` (`false` for `@`, `true` for `@!`) on every level down
  to the elements — `@[n][m]T` becomes `pntr(pntr(T, mutable, arr-size:m), mutable, arr-size:n)`,
  `@[n]$[m]T` becomes `pntr(pntr(T, mutable, arr-size:m), mutable, embedded:true, stride, arr-size:n)`. For a struct
  `T` in `@[n]T` / `@[n][m]T`, the element is `pntr(struct(T))` without `mutable`, the same
  as in the owned array: it is the struct's storage, and a field write through it is checked
  against the permission of the array level it is indexed from. `@[n]$T` takes the contiguous
  variable's form, `pntr(struct(T), mutable, embedded:true, stride:T.totalSize, arr-size:n)`;
  its `arr[i]` is the same struct storage. Because of `embedded`, that form is an array, not a
  struct pointer. For `@[n]@T` / `@[n]@!T`, the element is `pntr(T, mutable)` without
  `arr-size`, as in the owned `[n]@T`; the borrow's `mutable` stops at the slots. The element
  is any pointer type (`@!@T`, `@func(...)`), each level keeping its own `mutable`; a name
  other than a primitive's under it is `struct(T)`, its layout not needed. A `pntr` with
  `mutable` and none of `arr-size`, `embedded` and `unsized` is a `@T`/`@!T` pointer, never an
  array level. A borrowed level without `arr-size` has `"unsized": true` beside its `mutable`, so
  that it stays an array level: `@name` of an array whose size is known only at run time, and the
  rows of a read-only `@[][]T`.
  Without the outer size, `@[]T` / `@![]T` becomes the element type of a `[n]@[]T` / `[n]@![]T`
  slot, `pntr(T, mutable)` (a row keeping `mutable: false` on each level when read-only), so it
  is the same type as `@T` / `@!T`. Every other size must be constant
  (`E_ArrBorrowSizeNotConst`) and `T` primitive, struct or a pointer to one, with neither a
  struct nor a pointer inside a `$[m]` row (`E_ArrBorrowUnsupportedElem`). An `@[n]T` with a
  size, as an element or what a pointer points to (`[2]@[3]T`, `@@[3]T`), is E_NestedArrBorrow: as
  an element it would be typed as a borrowed array's row is. At a call argument, a
  local's initializer, or an assignment to such a destination, the source must be written as
  `@x`/`@!x` (`E_ArrBorrowNeedsAddrOf`) and its value-type must match depth, `embedded`,
  every `arr-size`, whether the element is a stored struct or a pointer, and a
  pointer element's `mutable`, which a read-only borrow may narrow from `true` to `false`, and the
  shape of what that element points to (`E_ArrBorrowShapeMismatch`). A destination without a size
  (`@[][]T`, `@@T`, a `[k]@[]T` slot) takes a source whose outermost level agrees in `embedded`
  when its elements are rows or pointers, and whose elements match in the same way
  (`E_ArrElemShapeMismatch`). A row read through an
  embedded borrow inherits its `mutable`.

  Scope-exit cleanup:
  - At the end of each block/while/function body containing array var-decls, SA appends
    `free()` expression statements in reverse declaration order for that scope's arrays.
  - Top-level (global scope) arrays are NOT freed (OS reclaims at process exit).

  Early-exit cleanup:
  - `return`: free calls are prepended for all array vars in all active function-level scopes
    (innermost-first, reverse declaration order). When there are frees and the `return` has a
    value, the value is first bound to a temp var `__ret_<N>` (a `var-decl` before the frees)
    and the `return` returns that temp, so the value never reads freed memory.
  - `break`/`continue`: free calls are prepended for arrays in all scopes within the current
    while loop body (including any nested blocks active at that point).
  - Prepended frees before `break`/`continue`/`return` may leave unreachable free calls at the
    normal scope-exit point; this is harmless (dead code, never executed at runtime).

- **while** - while loop statement
  - stmt-type\*: "while"
  - cond\*: SA-annotated condition expression (value-type present; integer or bool expected)
  - body\*: SA-annotated statement list (raw array; variables declared inside are scoped to the loop body)

- **for** - not emitted; SA lowers `for copy a, i in arr { body }` to a `block` holding, in order:
  - a `var-decl` of a hidden variable (`__for_N`) bound to `arr`, so `arr` is evaluated once.
    An array the expression returns is owned by the block and freed when it ends; otherwise the
    variable borrows `arr`.
  - the `int64` index (`i`, or a hidden name when omitted or typed) initialized to 0, and a
    hidden variable holding the length, found as `len` finds it.
  - a `while` on `index < length` whose body is `body`, then `index + 1 -> index`; the step is
    also inserted before each `continue` of this loop. With an index type (`for @a, T i in arr`),
    the body first declares `i` of type `T` from a `convert` of the hidden index; `T` must be an
    integer type (E_ForIndexNotInteger), and for an array of constant size `n`, `n - 1` must fit
    in it (E_ForIndexTooNarrow). For `copy a`, the body next declares `a` from element `index`.
    For `@a`/`@!a`, no variable is declared: each `a` in `body` (outside
    nested functions) is replaced by the element `__for_N[index]` before analysis, and `... -> a`
    becomes an `arr-assign`. The name stays declared, so `body` cannot declare it again.
    `__for_N` is read-only for `@a` (an owned range gets a read-only `__for_ro_N` bound to it);
    `@!a` over a read-only range is E_PtrMutabilityUpgrade.
  The range must be an array `len` accepts (E_ForRangeNotArray, or `len`'s own errors). The
  index is read-only (E_AssignToForIndex). In `body`, an operation that frees, replaces or
  resizes the range or anything containing it is E_ChangeForRange: `->>` into or out of it,
  `->` into it (allowed into a fixed-length range), and a writable borrow of it (`@!v`,
  method-form push/pop/resize, or a `@!` borrow passed on or bound as is). Indices on the path
  are not compared, so any index counts as the iterated one.

  Over a dictionary `d` (`for copy v, k in d`), the block is the same with these differences.
  The hidden index starts at `__pln_dict_next(__for_N, 0)` and steps to
  `__pln_dict_next(__for_N, index + 1)` (std/rt's first live position from there on, or the
  number of positions used); `k`, when written, is a read-only `@uint8` declared from
  `__pln_dict_key(__for_N, index)` and set again after each step, and is read-only as an index
  is. The length is the header's number of positions used (offset 8), and the elements are
  those of a hidden `__for_vals_N`, the values' data pointer (offset 64), indexed as above. A
  type on `k` is E_ForDictKeyTyped. A store `x -> d[k]` into the range is E_ChangeForRange too,
  as it may add a key; one into a value's field is not.

  Over a struct `s` (`for copy c, i in s`), SA lowers it to a `block` holding, in order:
  - the iterator `__pin_N` that the call `iter(@s)` (or `iter(@!s)`) returns. When `s` is a value
    a call returns, it is first held in a `__pin_N` of its own, which the block frees if it owns it.
    The `release` of each `temp` the call was given moves to the end of the block as well, as it does
    for an array a call returns; a `temp` that is the call itself has its `body` laid in the block.
    `iter` and `next` are those visible under any alias whose shape fits
    (`iter(@T s) -> I` or `iter(@!T s) -> I`, with `I` a struct; `next(@!I it) -> bool ok, E`);
    none is E_ForIterShape or E_ForNextShape, both `iter(@T)` and `iter(@!T)` E_ForIterAmbiguous.
  - the `int64` index, as for an array.
  - a `while` on a `bool` literal 1 whose body begins with a `tapple-decl` receiving
    `next(@!__pin_N)` into `__for_ok_N` and the element, then an `if` on `!__for_ok_N` holding
    `break`. A `copy c` of an element `E` returned by value receives it as `c`; otherwise it is
    received into `__for_e_N` (read-only for `@c`), and `copy c` is then declared from
    `__for_e_N[0]` (`__for_e_N` for an array). For `@c`/`@!c`, each `c` in `body` is replaced by
    `__for_e_N[0]` as for an array's element. `@c` over a returned `E`, or `@!c` over `@E`, is
    E_ForElemForm. The typed index and the step are as for an array.
  - the release of the iterator, and of the held `s` when the block owns it.
  When `s` is a location, the same E_ChangeForRange checks apply to it, with every change to `s`
  itself counted, also `->` into it and rebinding it.

- **break** - exit the innermost while loop
  - stmt-type\*: "break"
  - (no additional fields; SA validates that break appears inside a while loop)

- **continue** - skip to next iteration of innermost while loop
  - stmt-type\*: "continue"
  - (no additional fields; SA validates that continue appears inside a while loop)

Additional statement kinds emitted by SA:

- **assign** - assignment statement
  - stmt-type\*: "assign"
  - name\*: target variable name string
  - value\*: SA-annotated source expression (may be wrapped in convert node)

- **arr-assign** - array element assignment (`val -> arr[i]` or `val ->> arr[i]`)
  - stmt-type\*: "arr-assign"
  - target\*: SA-annotated arr-index expression (see Expression model below)
  - value\*: SA-annotated source expression (may be wrapped in convert node to match elem type)

  When `ownership-transfer: true` (`->>` syntax): the target must be an owned struct element or
  row. A `@T`/`@!T` slot is E_TransferToBorrowSlot; a row whose shape differs from the source's
  (as for Copy below, including a row of unknown size) is E_TransferShapeMismatch. SA emits three
  statements: a free of the target's current value (`free`, `__pln_free_T` or
  `__pln_free_arr_T(row, n)`; each accepts NULL), the arr-assign, and an `assign` that sets the
  source variable to NULL. The variable remains in `arrayScopeVars_` and receives `free(NULL)` at
  scope exit (C standard guarantees no-op).

  A `->>` into a struct field (`val ->> obj.field`) is accepted only for an owned field (`T`,
  `[n]T` or `[+]T`; otherwise E_TransferToNonOwnedField), and emits the same three statements with a
  `field-assign`.

  A `->>` into a variable (`val ->> var`, an `assign` with `ownership-transfer`) is accepted only
  for a variable that owns its array or struct (otherwise E_TransferToNonOwnedVar), from a value
  other than the variable itself (E_TransferToSelf) whose shape matches it as for a row
  (E_TransferShapeMismatch). It emits the same three statements with an `assign`; the free is the
  variable's own scope-exit release, which knows its sizes.

  In all three, a value a call computes is first evaluated into a `__pin_N` by a `var-decl`
  (category `expiring`), since it may read what the target holds (`f(v) ->> v`).

- **Chained and once-evaluated stores** - an assignment whose `value` is another assignment
  (`v -> t1 -> t2`, or a `chain-call` starting from one) runs that assignment first and reads its
  target back as the value. Each call is evaluated once, values before targets: a call in a target
  read back or freed by `->>` (an index, or the expression the target starts from), a value with a
  call stored to a target with a call, and a call in the location a `->>` reads and then resets are
  each evaluated into a temp `__pin_N` by a `var-decl`, in source order, and the statements are
  wrapped in a `block`. The temp keeps the value's `category`, so the store treats it as the value
  itself. A value with no call stored to a target with a call is evaluated first too when a call
  could change it: a numeric or pointer value is read into a `__pin_N`, a struct or array copied
  into a `__snap_N` (freed after the store), and a `->>` source read into a `__pin_N` (category
  `expiring`) and reset at once. That is unless all it reads is the storage of local variables
  none of which gives a pointer to its storage away anywhere in the function (by `@`, as a struct,
  `[]T` or C pointer argument, bound to a pointer, or cast). An aggregate literal value is
  evaluated before the target as usual; when the target is read back its address temp is what is
  read.

- **Copy** (`src -> dst` where dst is an array or a struct's storage) - no dedicated statement
  kind. SA lowers the copy to an `expr` statement calling a copy routine with `(dst, src, ...)`,
  where `dst` is the destination's `id`, `arr-index` or `field-access` expression. It applies to
  an `assign` to an owned array or struct variable (a borrowed array variable, with `mutable` on
  its levels, is rebound instead), an `arr-assign` whose element is a struct or an array row, a
  `field-assign` to an owned, embedded or array field, and a `var-decl` of an array or struct
  initialized from another value (the variable is allocated first, then copied into). A struct
  source whose `category` is `expiring` (returned by a Palan call) becomes a `block` of three
  statements: a `var-decl` of a temp initialized by the call, the copy from the temp, and the
  temp's free. The destination's type selects the routine:
  - No owned parts (a primitive, `$T`, `$[m]T` or `@T` element array, a struct without owned
    fields): C `memcpy(dst, src, bytes)` with a `lit-uint` byte count.
  - Struct `T` with owned fields: `__pln_copy_T(dst, src)`.
  - `[n]T` (T a struct): `__pln_copy_arr_T(dst, src, n)`; `[m][n]T`, `[m][n]$T`:
    `__pln_copy_arr_<E>(dst, src, m, n)`, `E` the row's key.

  The routine's `alloc-shapes` entry is recorded as for a declaration, and build-mgr generates
  the copy function next to that shape's allocator. The source's value-type must match the
  destination's level by level: every `arr-size` present and equal, the same `embedded`,
  struct storage versus pointer, and the same element type
  (E_CopyShapeMismatch). A lone struct may also be copied from a `@T`/`@!T`.

- **return** - return statement
  - stmt-type\*: "return"
  - values: SA-annotated return expression list (omitted for bare `return;`)

  Ownership transfer on return: if the returned expression is an `id` whose variable is in
  `arrayScopeVars_`, SA removes it from the free-tracking set before emitting scope-exit
  cleanup. The caller receives ownership of the returned pointer.

  A function with a `ret-slot` returns with a bare `return`, after statements that write the
  value into the slot: a struct-returning call gets the slot as its `ret-dest`; a literal is
  stored into it field by field; an owned variable's bytes are `memcpy`d in and, for a heap
  variable, its block alone is `free`d (what it owns moves with the bytes); anything else is
  copied in as `->` would copy it into a fresh struct.

- **tapple-decl** - tuple-style multiple return value declaration
  - stmt-type\*: "tapple-decl"
  - vars\*: one entry per return value, in order: `var-name` and `var-type` (the function's
    ret type), or for a struct return `ret-dest`, an expression giving the storage it is written
    to (declared by a preceding statement, as for a call's `ret-dest`)
  - value\*: SA-annotated call expression (func-type: "palan"; carries value-types field)

  A declared variable whose type differs from its ret type is received into a temporary and
  declared by a following `var-decl` initialized from it. A struct variable is declared before
  the `tapple-decl` (a raw `malloc`ed block, or a `frame-alloc` for `$T`) and is its own
  `ret-dest`; it owns what it receives and is released at scope exit like a declared struct.
  A `tapple-assign` is emitted as a `block` holding a `tapple-decl` into temporaries followed by
  one `assign`, `arr-assign` or `field-assign` per target, in target order; a struct is copied
  into its target and its temporary freed at the end of the block.

Expression model
----------------
Same structure as AST expressions (see ASTSpec.md) with the following additions:

- category - Present on an expression whose value has, or needs, an owner (absent on anything else):
  - `owned` - an `id` naming a variable that owns its storage and releases it at scope exit (a heap
    struct or array, not a `$` variable)
  - `expiring` - a call returning an owned value that has no owner yet: an array as a heap block, a
    struct as contents written to storage the receiver provides
  - `owner-view` - a pointer cast to an owned type (`p as T`): what it points to, viewed as an owner
    that is no Palan variable
- value-type - Resolved Variable type object (same structure as var-type in ASTSpec.md).
  Present on all expression kinds except bare call with no return type.
  - lit-int: the expected type when used in a typed context (e.g. `int32 x = 10;` → int32);
    defaults to int64 when no expected type is available (int32 for a character literal, one with
    `char`). Exception: an AST `lit-int` that
    already carries a `value-type` (ASTSpec.md's `lit-int` entry — `true`/`false`, or gen-ast's
    in-place substitute for a cinclude'd macro-constant reference) keeps that type as-is; the
    context's expected type never overrides it, since it was fixed by the literal or the C
    declaration, not by where the expression appears
  - lit-uint: adopts the expected uint type when in a uint-typed context (e.g. `uint32 x = 1u;` → uint32);
    defaults to uint64 when no expected uint type is available
  - sizeof: replaced by a `lit-uint` of the type's byte size, typed as above; it never reaches
    sa.json. A struct is its C ABI layout size (incomplete → `E_IncompleteStructType`), an enum its
    base type's, a pointer 8. An array is `n * element size`, where a `$` element is stored in
    place and a struct or row element without `$` is an 8-byte pointer slot; every size must be
    a compile-time constant (`E_SizeofArrSizeNotConst`)
  - Range check (lit-int without its own `value-type`, and lit-uint): once the type is decided, a
    value outside that integer type's range (including a negative value adopting an unsigned type)
    is rejected with `E_IntLiteralOutOfRange`. A literal operand of a binary operator is typed only
    once -- from the other operand's type when that has one, else from the expected type -- so the
    check never runs against a provisional type. For `bool` the range is 0..1.
  - Hexadecimal literal (`hex`): one taking a signed integer type and fitting its width is emitted
    with the negative number those bits are, when its top bit is set. In a comparison it takes the
    other operand's type if that is a float, or an integer it fits the width of; a folded constant
    expression reads it in int64, and its result has `hex` only for `bitand`, `bitor` and `bitxor`
    of two operands that have it. `hex` stays on the node in sa.json; codegen ignores it.
  - Character literal of 128 or more (`char`): an integer type under 32 bits (`int8` `int16`
    `uint8` `uint16` `bool`) is rejected with `E_WideCharLitNarrowType`, both where the literal
    takes the type and where it is compared with an operand of that type (a comparison types no
    literal by the other operand). `char` stays on the node in sa.json; codegen ignores it.
  - lit-flo: adopts the float type of a float-typed context (e.g. `flo32 y = 1.5;` → flo32);
    defaults to flo64 when no expected float type is available
  - lit-str: {"type-kind": "pntr", "base-type": {"type-kind": "prim", "type-name": "uint8"}}
  - id: same object as var-type
  - add: promoted type of left and right operands (see Promotion rules);
    the narrower operand is wrapped in a convert node if types differ
  - sub: same promotion rules as add
  - neg: same type as operand (no promotion)
  - cmp: always `{"type-kind": "prim", "type-name": "bool"}`;
    left and right operands are promoted by the same rules as add
  - logical-and: always `{"type-kind": "prim", "type-name": "bool"}`;
    both operands must be integer types (float operands are a compile error)
  - logical-or: same as logical-and
  - logical-not: always `{"type-kind": "prim", "type-name": "bool"}`; operand must be integer type
  - func-ref: a function name written where a value is expected (an `id` naming no variable or
    C global). Emitted as `{"expr-type":"func-ref","name":<its link name>,"func-type":<"c" or
    "palan">,"value-type":<its function pointer type>}` (plus `loc`); `func-type` tells a cast
    whether it may change the function's ownership (only a C function's). The type is `{"type-kind":"pntr","mutable":false,
    "base-type":{"type-kind":"func","parameters":[{"var-type":T}...],"ret-type":R}}`, built from
    the function's signature; a function returning nothing has a `void` prim `ret-type`, and a
    single named return gives its type. The function must be a Palan function (local or imported)
    or a non-variadic C function, and every parameter and the return must be a primitive, a
    borrowed pointer, or an owned block (`isFuncSigType`; not a C function's by-value struct,
    which is passed in registers); otherwise E_FuncNotValue (multiple returns, variadic, syscall,
    or a type that can't pass through a function pointer). Lowered by codegen to the
    `LeaLabel` of the function's label.
  - call-ptr: a call through a function pointer, `{"expr-type":"call-ptr","callee":<expression of
    a function pointer type>,"args":[...],"value-type":R}` (`value-type` absent for a void
    return). A `call` whose name is a function pointer variable, parameter or C global becomes
    one with an `id` callee, and a `member-call` `s.f(args)` whose object's struct has a function
    pointer field `f` becomes one with a `field-access` callee. If a function of the same name is
    visible (for `s.f(args)`, one whose first parameter is `@!`, i.e. a method-form candidate) the
    call is E_AmbiguousFuncPtrCall. An AST `call-ptr` (`(e)(args)`) keeps its callee, which must
    have a function pointer type (else E_CallNonFuncPtr). Arguments are checked against the
    function type's parameters by the rules for a Palan callee, whatever the pointer's origin, so
    an owned parameter takes a copy or a value given with `>>`; an owned return has category
    `expiring`. Lowered to a C-convention indirect call.
    A function type's parameter and return types take the form a Palan function's do (`sigType`),
    wherever the type is written, so an owned struct is `pntr(struct)` without `mutable`. A
    binding between function pointer types also requires each parameter and the return to agree
    in what the type registry does not tell apart (`sigShapeOk`): an owned block (`isOwnedBlock`)
    has the same type on both sides, and a borrowed array a shape that fits, in the direction the
    value flows.
  - addr-of: SA dispatches on the AST node's `object` (see ASTSpec.md) and emits one of three
    shapes — the `addr-of` expr-type only ever appears for the plain-variable case; a struct-field
    or array-element target lowers to an ordinary `field-access`/`arr-index` node instead:
    - `object.expr-type == "id"` (`@x` / `@!x`): emitted as `{"expr-type":"addr-of","name":<var
      name>,"mutable":<bool>,"value-type":{"type-kind":"pntr","mutable":<bool>,"base-type":<named
      var's type>}}`. `mutable` is `true` for `@!ID`, `false` for `@ID`. The named variable must be
      a local variable in the current scope (not a function parameter), and its own type must be
      `prim` or `pntr`-of-`prim` (a pointer-to-pointer result, matching C's `T **` out-param idiom)
      — a pointer whose base isn't itself `prim` (e.g. a `@T` struct pointer) is rejected
      (E_AddrOfNotLocalVar / E_AddrOfNotPrimitive). A struct variable (a `pntr(struct T)` with no
      `mutable` key: a struct local, named return, or struct-type parameter) is emitted as a plain
      `id` whose `value-type` is the variable's type with `mutable` added — the struct's own
      pointer, not the address of the variable's slot. An array variable (an owned array,
      or a variable of borrowed array type — including a parameter) is the exception: it is
      already a pointer to its elements, so `@x`/`@!x` is emitted as a plain `id` whose
      `value-type` is the variable's type with `mutable` set on every array level (a `@T`/`@!T`
      element keeps its own). Its leaf element must be `prim`, a struct, or a `@T`/`@!T` pointer
      (E_AddrOfNotPrimitive), and `@!x` needs a writable array (E_WriteThroughReadOnlyPtr).
    - `object.expr-type == "call"` or `"member-call"` (`@f()` / `@!f()`): only as a call argument,
      and only for a call returning a struct (`category: "expiring"`); the call gets
      `mutable` added to its `value-type`, as for a struct variable, and is held in a `temp` (see
      Temp arguments below). Elsewhere it is E_ExpiringStructToBorrow; a call
      returning anything else is E_AddrOfNotAddressable.
    - `object.expr-type == "arr-lit"` or `"dict-lit"` (`@[1, 2]` / `@!{x: 1}`): only as a call
      argument (Temp arguments below); elsewhere E_ArrLitContext / E_DictLitContext.
    - `object.expr-type == "field-access"` (`@s.x` / `@!s.in.v`): resolved via the same
      `resolveObjectChain` field-chain machinery as an ordinary field-access read (see the
      field-access section below), then re-emitted as `{"expr-type":"field-access","var"|
      "ptr-expr":…,"offset":<int>,"value-type":<pntr type>,"addr-only":true}` —
      `addr-only:true` tells codegen to compute the field's address (`CalcAddr`) instead of loading
      it. The leaf field must be primitive-typed, an embedded struct (`$T`), an owned field
      (`owned`), or an embedded array — a `raw-ptr` field is rejected
      (E_AddrOfNotPrimitive) — and must exist on the resolved struct (E_UnknownField otherwise). An
      array field is borrowed as an array, as an array variable is (its value-type with the
      requested permission on every level); an owned one is loaded (`addr-only:false`). A
      primitive leaf's `value-type` is `{"type-kind":"pntr","mutable":<bool>,"base-type":<field's
      prim type>}`, same as the `id` case. An embed leaf's own value-type
      (`fieldValueType`) is already `pntr(struct T)` — the field IS the inner struct's storage, the
      same shape a struct-typed local variable has — so its `value-type` here is that same
      `pntr(struct T)` (with `mutable` set to the requested `@`/`@!`), not a further `pntr(...)`
      wrap; wrapping it again would build a pointless `pntr(pntr(struct T))`. An owned struct
      leaf holds the pointer to its struct, so it is emitted with `addr-only:false` (a load of that
      pointer) and the field's `pntr(struct T)` value-type plus `mutable`. Because `@!` requests
      a *mutable* pointer, resolution runs with the same write-permission checks a store-location
      chain would (`resolveObjectChain(obj, forWrite=<mutable>)`): a read-only `@T`-typed base
      variable (E_WriteThroughReadOnlyPtr) or an intermediate read-only raw-ptr field hop
      (E_WriteToImmutablePtrField) rejects `@!`, so a read-only pointer can never be laundered
      into a mutable one by taking a field's address instead of writing the field directly.
    - `object.expr-type == "arr-index"` (`@arr[i]` / `@!arr[i]`): resolved via `sa_expr_arr_index`
      (see the arr-index section below) exactly as an ordinary array-element read would be, then
      the resulting node is reused as-is with `addr-only` forced to `true` and `value-type`
      rewrapped as `{"type-kind":"pntr","mutable":<bool>,"base-type":<original element
      value-type>}` — the node keeps its `expr-type:"arr-index"`, `array`, `index`, `elem-size`,
      and `loc` keys unchanged (unlike the field-access shape above, no new node is built). A
      struct element (value-type `pntr(struct T)` with no `mutable` key, from `[n]T` or `[n]$T`) is
      already a pointer to its storage, so only `mutable` is added to its value-type and
      `addr-only` keeps its value. A row (an array level, from `[n][m]T`, `[n]$[m]T`, `[n][m]$T`
      or `[n][m]T` with struct `T`) is likewise its own storage: its value-type gets `mutable` on
      every array level (as for `@arr` on a whole array) and `addr-only` keeps its value, so it
      binds to a borrowed array `@[m]...`. For any other element, two checks gate this: the resolved element must not already be `addr-only:true` (a struct
      element reached through a struct pointer is already an address) and its
      `value-type.type-kind` must be `"prim"` (a pointer-slot element is not) — either failure is
      E_AddrOfNotPrimitiveElem. When `mutable` is requested, `isWritableThrough` is additionally
      checked against the array's own `value-type` — the same rule `sa_arr_assign_stmt` applies to
      an ordinary `arr[i]` write — so `@!` through a read-only array is E_WriteThroughReadOnlyPtr.

      Example — `@!arr[2]` where `arr` is `[4]int64`:
      ```json
      {
        "addr-only": true,
        "array": {"expr-type":"id","name":"arr","category":"owned",
                   "value-type":{"type-kind":"pntr","base-type":{"type-kind":"prim","type-name":"int64"}}},
        "elem-size": {"expr-type":"lit-uint","value":"8","value-type":{"type-kind":"prim","type-name":"uint64"}},
        "expr-type": "arr-index",
        "index": {"expr-type":"lit-int","value":"2","value-type":{"type-kind":"prim","type-name":"int64"}},
        "value-type": {"type-kind":"pntr","mutable":true,"base-type":{"type-kind":"prim","type-name":"int64"}}
      }
      ```
    - Any other operand shape (a call result, `@(a + b)`, …) is a compile error
      (E_AddrOfNotAddressable) — address-of is not general in this version.
    `mutable` is always present on the resulting `value-type` regardless of which shape was
    emitted.

    **Struct borrow binding:** a `pntr(struct T)` value-type without a `mutable` key is a struct's
    own storage; one with the key is a borrow (`@T`/`@!T`, a C pointer whose `const` was folded by
    cinclude normalization, or a `@x`/`@!x` result). Binding struct storage to a borrow
    destination — a call argument (Palan or C), a local's initializer, an assignment, a field
    store, an array-slot store, or a return — is a compile error (E_StructBorrowNeedsAddrOf); the
    source must be written `@x`/`@!x`. A call result (`category: "expiring"`) has no owner, so binding it to a borrow is
    E_ExpiringStructToBorrow, except as a call argument written `@f()`/`@!f()` (see the addr-of
    call shape above and Temp arguments below). The check reads the raw JSON, since `PlnTypeRegistry` interns a missing
    `mutable` as `true`. Writing through a `false` (read-only) pointer — via `p[0]` deref, a field access, or
    an array-element write — is a compile error (E_WriteThroughReadOnlyPtr); see typeCompat rules
    below for how mutability is enforced separately from type compatibility.
  - call: present when the function has a return type (ret-type in its definition).
    When `ret-type` is `pntr(T)` derived from a `[]T` signature, the caller is responsible
    for freeing the returned pointer (expiring ownership).

    **Temp arguments:** an argument with no owner is emitted as a `temp` (below), since the callee
    may return a borrow of it:
    - a struct returned by a call (`category: "expiring"`, with `mutable` in its `value-type`
      when written `@f()`/`@!f()`): `body` declares `__tmp_N` as a `frame-alloc` and makes the
      call with it as `ret-dest`, `release` is its `__pln_fini_T` (nothing for a struct owning
      nothing). Given to a parameter owning it, the struct is written into a raw `malloc`ed
      block instead, which the callee frees.
    - a literal (`[..]`/`{..}` for a struct parameter `T`, `@[..]`/`@![..]`/`@{..}`/`@!{..}` for
      a `@T`/`@!T` or a borrowed array `@[n]T`/`@![n]T` parameter): `body` declares `__lit_N` of
      the parameter's struct or (owned) array type and initializes it as a variable's initializer
      would; `release` is that variable's scope-end free. `value` is `__lit_N` analyzed as
      written in the literal's place (`@__lit_N`), so the argument is checked as that variable would
      be: a missing `@` is E_StructBorrowNeedsAddrOf / E_ArrBorrowNeedsAddrOf, and `@` for `@!` is a
      permission error. A literal for any other parameter (numeric, a non-struct `@T`/`@!T`,
      `[]T`) is E_ArrLitArgType / E_DictLitArgType, naming the parameter and its type; one for
      a C function's parameter other than a by-value struct is E_LitArgCFunc.

    A struct returned by a call and discarded as an `expr` statement becomes a `block` of the
    temp's `frame-alloc`, the call into it and its `__pln_fini_T` (if any).

    A call returning a `@T`/`@!T` (one of its `value-types` for a `tapple-decl`) with a
    `temp` argument, directly or through a nested call of the same kind, may return a
    pointer to that temp, so binding its result — where E_StructBorrowNeedsAddrOf is checked, or a
    `tapple-decl` — is E_BorrowOfTempEscapes.

SA-only expression kinds (not present in AST JSON):

- convert - Type conversion inserted by SA. Wraps an expression whose value-type
  differs from the required type. Used for both implicit widening and explicit casts
  (narrowing, signed↔unsigned, and the pointer casts below). The `cast` AST node is consumed by SA and replaced
  by `convert` (or removed if Identical); it does not appear in sa.json. A cast to `bool`
  from any other numeric type becomes `convert(cmp(src != 0))` (`0.0` for a float source), so a
  nonzero value yields 1 instead of being truncated.
  - expr-type\*: "convert"
  - value-type\*: target Variable type object
  - from-type\*: source Variable type object
  - src\*: inner expression being converted

  Example: `int64 y = x;` where x is int32 (implicit widening)
  ```json
  {
    "expr-type":  "convert",
    "value-type": {"type-kind": "prim", "type-name": "int64"},
    "from-type":  {"type-kind": "prim", "type-name": "int32"},
    "src": {"expr-type": "id", "name": "x", ...}
  }
  ```

  A cast whose target is a pointer type is not decided by `typeCompat`: from a pointer (not a
  struct value) or an `int64`/`uint64`, any pointer type and permission is accepted, and a
  borrowed array target (`pntr` with `arr-size`) only from a pointer or array. A pointer casts to
  a prim only when that is `int64`/`uint64`. Codegen lowers every such `convert` to a plain
  64-bit move. A borrowed array binding accepts a `convert` as its source, alongside `@name`.

  Example: `int32(x)` where x is int64 (explicit narrowing cast)
  ```json
  {
    "expr-type":  "convert",
    "value-type": {"type-kind": "prim", "type-name": "int32"},
    "from-type":  {"type-kind": "prim", "type-name": "int64"},
    "src": {"expr-type": "id", "name": "x", ...}
  }
  ```

- temp - A call argument held in a temp variable (see Temp arguments above), or a chained call
  given return values (see Chained calls).
  - expr-type\*: "temp"
  - body\*: statements that declare and initialize the temp; the temps of their values are
    released with `release`, after it
  - value\*: the temp as the argument (`id`), or the chained `call` taking the temps
  - release\*: statements that free the temp; codegen runs them once the enclosing full
    expression has been evaluated: at the end of the statement (before a `return`'s `ret`), or,
    in an `if`/`while` condition or a `&&`/`||` operand, when that operand's value is known and
    before it branches
  - reset: statements that set the owner a `transfer` gave away to NULL; codegen runs them once
    all of the arguments of the call the `temp` is given to have been evaluated, just before the
    call
  - value-type: `value`'s (absent for a void `call`); `category` is `value`'s too

  A `temp` holding a call that returns a borrow may point to its temps (E_BorrowOfTempEscapes when
  bound). One discarded as an `expr` statement becomes a `block` of its `body`, the call's
  statement and its `release`.

- static-alloc - The storage of a `$T` or fixed-size array `global` variable, in place of its
  `frame-alloc`: a block in `.bss`, so it outlives the top level. Its value is the block's
  address. Same fields as `frame-alloc`, plus `label` (the variable's label + `.data`), and
  `data` when the variable's array literal is all constants of a `prim` leaf: `type` (the leaf)
  and `values` (each element's `value` text, as its `lit-int`/`lit-uint`/`lit-flo` gives it, rows
  in order). The block then holds those values from the start, in `.data`, and the var-decl is
  followed by no `arr-assign` of them.
- frame-alloc - A block in the function's stack frame; its value is the block's address. Appears
  only as the `init` of a stack variable's var-decl (see Stack variable declarations) or of
  `args`'s (see Root `statements`).
  - expr-type\*: "frame-alloc"
  - size\*: block size in bytes (compile-time constant)
  - align\*: block alignment in bytes (1, 2, 4, 8 or 16)
  - value-type\*: the variable's type

SA-only expression kinds (added to AST nodes):

- **arr-index** - Array element access (`arr[i]`), and pointer dereference/subscript (`p[i]`) --
  the same node covers both, since Palan represents an array as a `pntr(T)` plus attributes and a
  plain pointer as a bare `pntr(T)`. The AST node gains four SA-added fields:
  - value-type\*: element type (the `base-type` of the array's `pntr` value-type), or
    `pntr(struct(T))` when the element is a struct (see below)
  - elem-size\*: `lit-uint` node whose `value` is the element byte size (1, 2, 4, or 8 for a
    scalar; `T.totalSize` for a struct element), or a `mul` expression for a variable inner
    dimension (2D embedded array row access with a runtime column count)
  - addr-only\*: boolean. `true` when the node computes an address (`CalcAddrIdx`) rather than
    loading a value (`DerefLoadIdx`) — struct-array row access, 2D embedded-array row access, and
    struct-pointer dereference all set this; a scalar element or a pointer-slot element sets it
    `false`. Always present; codegen reads it directly and performs no re-derivation.
  - array\*: SA-annotated array expression (value-type must be `pntr`)
  - index\*: SA-annotated index expression (must not be float; integer types only)

  Validation:
  - If the array operand's value-type is not `pntr` → compile error (E_NotArrayType)
  - If the index expression resolves to a float type → compile error (E_ArrayIndexNotInteger)
  - If a non-struct element's byte size cannot be determined → compile error
    (E_UnknownType). Reachable because gen-ast has no symbol table: a name it doesn't
    recognize as a struct in native syntax (e.g. `@!Foo p;` where `Foo` was never declared)
    still parses as `{"type-kind":"prim","type-name":"Foo"}`, so this is the first point SA can
    reject it. This guards the SA/codegen boundary: without it, a name that fails to resolve
    produces a `-1` size sentinel that would otherwise leak into `elem-size` and only surface as
    a `palan-codegen` failure later. (The struct-element branch above has no equivalent guard —
    every `struct`-kind `elem_type` SA constructs already came from a name resolved in its own
    struct registry.)
  - As an `arr-assign` target, a struct element is copied into (see Copy above); a `->>` to a
    node with `addr-only:true` is rejected (E_AssignToWholeStructElem) — there is no storage slot
    at an address-only location to overwrite.

  **Pointer dereference (`p[i]`) on a scalar or pointer element:** when `array`'s `pntr` base-type
  is a primitive or another `pntr`, `arr-index` is exactly C's pointer subscript: `elem-size` is
  the element's byte size (8 for a pointer element), `addr-only` is `false`, and `value-type` is
  the element type. This holds for any index, not just `0` — there is no `i == 0` special case
  anywhere in SA or codegen; `p[i]` for `i != 0` is ordinary pointer arithmetic, scaled by
  `elem-size`, with no bounds check (the same as C). `@T` (read-only) permits this only for a
  read; a write is E_WriteThroughReadOnlyPtr. See PalanReference.md §10.

  **Pointer dereference (`p[i]`) on a struct element:** when `array`'s `pntr` base-type is itself
  `struct(T)` (i.e. `p` is `@T`/`@!T` where `T` is a struct — not an `[n]$T`/`[n]T`/`[n]@T`/
  `[n]@!T` array, which are covered by the table below), Palan has no register-sized
  representation of a struct value: every struct-typed expression is itself a `pntr(struct)`. So
  `p[i]` is an address computation, not a load: `value-type` is `pntr(struct(T))` without
  `mutable` -- the struct's storage, as for an `[n]$T` element -- `elem-size` is `T.totalSize`,
  and `addr-only` is `true`. Writes through it (`p[i].field`, a whole-struct copy `w -> p[i]`)
  are checked against `array`'s value-type `mutable`. `p[0]` is
  the degenerate case of this — reachable via the field-access chain (`resolveObjectChain`),
  not by loading a struct value.

  Example: `a[i]` where `[5]int32 a`:
  ```json
  {
    "expr-type": "arr-index",
    "value-type": {"type-kind": "prim", "type-name": "int32"},
    "array": {"expr-type": "id", "name": "a",
              "var-type":   {"type-kind": "pntr", "base-type": {"type-kind": "prim", "type-name": "int32"}},
              "value-type": {"type-kind": "pntr", "base-type": {"type-kind": "prim", "type-name": "int32"}}},
    "index":    {"expr-type": "id", "name": "i", "value-type": {"type-kind": "prim", "type-name": "int64"}, ...},
    "elem-size": {"expr-type": "lit-uint", "value": "4",
                  "value-type": {"type-kind": "prim", "type-name": "uint64"}}
  }
  ```

  **Struct array element access:** When the array element type is `struct(T)`, `arr-index` yields a
  pointer to the element rather than the element value. The `value-type`, `elem-size`, and
  `addr-only` differ by array form:

  | Declaration | value-type | elem-size | addr-only |
  |---|---|---|---|
  | `[n]$T pts` (contiguous) | `pntr(struct(T))` | `T.totalSize` (e.g. 16 for a two-field int64 struct) | `true` |
  | `[n]T pts` (owned pointers) | `pntr(struct(T))` | `8` (pointer size) | `false` |
  | `[n]@T pts` (read-only) | `pntr(struct(T))` | `8` (pointer size) | `false` |
  | `[n]@!T pts` (mutable) | `pntr(struct(T))` | `8` (pointer size) | `false` |
  | `@T`/`@!T p` (plain pointer to struct) | `pntr(struct(T))` | `T.totalSize` | `true` |

  The `[n]T`/`[n]@T`/`[n]@!T` rows are pointer-*slot* arrays: `pts[i]` loads/stores the 8-byte
  pointer value held in the slot, so `addr-only` is `false` and `pts[i]` can appear as an
  `arr-assign` target (`ptr -> pts[i]`). The `[n]$T` and plain-pointer-to-struct rows have no slot
  to load — the struct data is either inline (`$T`) or the pointer itself has already been
  dereferenced one level (`p[i]`) — so `addr-only` is `true` and only field access
  (`pts[i].field`, `p[i].field`) is valid.

  The returned `pntr(struct(T))` is used as the base for subsequent field access (`pts[i].field`)
  via `resolveObjectChain`.

Additional fields per expression kind:

- lit-str expression: value replaced by label (assembly label string, e.g. ".str0")
- id expression: var-type added (Variable type object from ASTSpec.md)
- call expression:
  - func-type\* added: "c" for C functions, "palan" for Palan user-defined functions, "syscall"
    for a call resolved to a `syscall` declaration (local or imported)
  - name: for func-type "c", the symbol the function links to (its `symbol` in ast.json, else
    the written name); for func-type "palan", its symbol name (see "Symbol names"); likewise for
    a func-ref
  - syscall-number added (integer) when func-type is "syscall" -- the declaration's number,
    folded to a literal by SA; the declaration itself never reaches this or any other module's
    sa.json (see "Function model" above), so this is the only place the number survives
  - value-type added when the function has a single return type (ret-type in its definition)
  - value-types added when the function has multiple return values (rets in its definition);
    array of Variable type objects in declaration order
  - ret-dest and struct-ret added instead of value-type when the function (Palan or C, or a
    function pointer) returns a struct: `ret-dest` is an expression of type `pntr(struct)`
    giving the storage the value is written to, and `struct-ret` is `{"size":<total struct size
    in bytes>,"eightbytes":[{"class":"int"|"sse"|"x87","size":1..16},...]}` (System V AMD64
    classification). Each array entry is one eightbyte of the classified return value in order —
    its byte offset within the struct is implied by its index (`index * 8`), there is no
    explicit `offset` key; `size` on the last entry is the tail width when the struct isn't a
    multiple of 8 bytes (codegen writes a width other than 1/2/4/8 piecewise). An SSE eightbyte
    taken up by one `flo64` also has `"flo64":true`: codegen reads it from memory in one 8-byte
    load, and any other 8-byte SSE eightbyte as two 4-byte halves, since it is usually written as
    two `flo32` stores an 8-byte load could not be forwarded from. A struct holding a `flo80`
    is MEMORY class, except one of 16 bytes, which is that `flo80` alone and is
    `"eightbytes":[{"class":"x87","size":16}]`: it comes back in st(0). A struct over 16
    bytes (the System V AMD64 MEMORY class) is `"eightbytes":[]`: codegen passes `ret-dest` as
    the hidden first argument. Such a call has no value and is always the `body` of an `expr`
    statement; where its value is used, SA emits a `temp` whose `body` declares the storage and
    makes the call, and whose `value` names the storage with `"category":"expiring"`. Where the
    receiver provides storage of its own (see Struct variable declaration and SpecAndDesign.md
    §4), that storage is the `ret-dest`. A C function's by-value struct return type is normalized to `pntr(struct)` when it is registered, as a Palan `-> T` is.
  - struct-arg added on an argument (in `args`) given to a C function's by-value struct
    parameter: `{"size":<total struct size in bytes>,"align":<its alignment>,"eightbytes":[...]}`,
    with `eightbytes` as in `struct-ret` (`[]` for MEMORY class, which a `flo80` always makes it
    here). `align` places a struct copied to the stack, at a 16-byte boundary when it is 16. The argument expression itself is a pointer to the
    struct to copy — a variable, field, array element, or a `temp` holding a literal or a
    returned struct — and codegen reads the eightbytes through it: each goes to a register of
    its class when all of the struct's eightbytes fit in the remaining argument registers,
    otherwise the whole struct is copied to the stack, as a MEMORY-class one always is.

Promotion rules
---------------
Integer types are ranked by bit width:

- Rank 1: int8, uint8
- Rank 2: int16, uint16
- Rank 3: int32, uint32
- Rank 4: int64, uint64

### Usual arithmetic conversions (binary operators and comparisons)

A binary arithmetic operator (`+ - * / % & | ^`) or comparison (`< <= > >= == !=`) applies
`usualArithConv(leftType, rightType)` to determine a common type; the operand(s) that differ
from the common type each get a `convert` node. The rules, in order:

1. Either operand is float → the float side wins (both float → the wider float; float paired
   with an integer → the float type, per the int-to-float widening rule below).
2. Same signedness (both signed or both unsigned) → the higher-ranked type wins.
3. Mixed signedness, and the signed operand's rank is strictly greater than the unsigned
   operand's rank → the signed type wins (e.g. `int64 op uint32` → `int64`).
4. Mixed signedness, otherwise → the higher-ranked **unsigned** type wins (e.g.
   `int32 op uint32` → `uint32`; equal rank always falls here since rule 3 requires *strictly*
   greater).

**No C-style integer promotion to a machine word.** Unlike C, `int8 op int8` stays `int8` — Palan
preserves the declared width's wraparound rather than promoting to `int`. This also means the
common type is always one of the two operand types, never a third type.

An enum operand is first relabeled as its base type (no `convert` node), so `Color.RED + 1` is
the base type.

`bool` is the one exception: before the rules above, SA wraps a `bool` operand of an arithmetic,
bitwise, or comparison operator, `neg`, or `bitnot` in a `convert` to `int32`, so the result is
never a 1-byte value outside 0/1 (`b + 1` is 2). `usualArithConv` itself ranks `bool` below every
other type, so the other side wins; this matters only for call arguments (below).

A shift (`shl`/`shr`) does not take a common type: its `value-type` is the (promoted) left
operand's, and the count is wrapped in a `convert` to that type when it differs. Both operands
must be integers (`E_BitwiseOpNotInteger`). Codegen takes the count modulo the width.

A pointer or struct operand (non-Prim) has no common type: `usualArithConv` returns none, and
an arithmetic operator diagnoses E_ArithOpNotNumeric. A comparison of pointers instead leaves both
operands unconverted (`p == NULL`); the comparison's own result type is always `bool` regardless.

An operator with a struct operand is instead a `call` of the operator function (`0add` .. `0lt`).
A comparison is so unless the other operand is `NULL`, which stays a `cmp`. `!=` `>` `<=` `>=`
call `0eq` or `0lt`: `>` and `<=` with the operands swapped, `!=` `<=` `>=` inside a
`logical-not`. When swapped and either operand has a call, the call is a `temp` whose body
evaluates the left operand into a `__pin_N` first.
An `append` (`a << b`) is always a `call` of `0app`, its first argument the left operand as a
method-form receiver is given (`addr-of` with `mutable`, or as it is when it is a `@!T` borrow or a
call returning one).

Float promotion rules (subsumed by usual arithmetic conversions above, restated for clarity):

- `flo32 op flo64` → flo32 is implicitly widened to flo64; result is flo64
- `flo32 op flo80` / `flo64 op flo80` → the narrower float is implicitly widened to flo80; result is flo80
- `int op flo32` → int is implicitly widened to flo32; result is flo32
- `int op flo64` → int is implicitly widened to flo64; result is flo64
- `flo32 op int` / `flo64 op int` → symmetric with the above

The `%` (mod) operator on float operands is a compile error (SA emits an error and aborts).

typeCompat rules
----------------
`typeCompat(from, to)` returns one of: `Identical`, `ImplicitWiden`, `ExplicitCast`, `Incompatible`.
This is the rule for a **binding site** — a var-decl initializer, assignment, array-assignment,
return, or field-assign — not for a binary operator (see "Usual arithmetic conversions" above)
or a call argument (see below): those accept some cross-signedness conversions a binding site
rejects, because a call argument only needs to fit the callee's ABI width rather than match a
declared variable's exact type.

| from \ to            | same type  | wider, same group | narrower or diff group (prim) | pointer | other |
|----------------------|------------|-------------------|-------------------------------|---------|-------|
| same prim type       | Identical  | —                 | —                             | —       | —     |
| signed int           | Identical  | ImplicitWiden     | ExplicitCast                  | Incompat| Incompat|
| unsigned int         | Identical  | ImplicitWiden     | ExplicitCast                  | Incompat| Incompat|
| float                | Identical  | ImplicitWiden     | ExplicitCast                  | Incompat| Incompat|
| pointer (same base)  | —          | —                 | —                             | Identical | Incompat|
| pointer (same base, mutable diff) | — | —            | —                             | Identical | — |
| pointer (diff base)  | —          | —                 | —                             | Incompatible | — |

Notes:
- Signed and unsigned are different groups; `int32 → uint32` requires `ExplicitCast`.
- `bool` is its own group: `bool →` any integer or float is `ImplicitWiden`, and any integer or
  float `→ bool` is `ExplicitCast`.
- An enum behaves like `bool`: `enum →` a prim is decided as from its base type, with
  `Identical` becoming `ImplicitWiden`; an integer, `bool`, or another enum `→ enum` is
  `ExplicitCast`; a float or pointer `→ enum` is `Incompatible`.
- At a binding site, `ImplicitWiden` inserts a `convert` node; `ExplicitCast` is a compile error
  (E_InvalidNarrowingConv) unless the source expression is an integer literal (`lit-int` /
  `lit-uint`), which instead adopts the destination type. The exemption does not apply to an
  enumerator (its type is fixed) or to an enum destination (`Color c = 5;` needs `Color(5)`). All five binding sites (var-decl
  initializer, assignment, array-assignment, return, field-assign) apply this identically —
  there is exactly one narrowing rule, not one strict (initializer) and four permissive ones.
  Writing `ExplicitCast` at a non-literal binding site requires an explicit `type-name(expr)`
  cast in the source.
- Variadic arguments undergo caller promotion: int8/int16 → int32, uint8/uint16 → uint32,
  bool → int32, an enum → its base type.

### Method-form calls

An AST `member-call` `aa.f(args)` is either a module-alias call or a method-form call.
`normalizeMethodCall` resolves it where SA takes it in (an expression, or the value of a
multiple-return declaration/assignment):

- If `object` is an `id` that is neither a variable nor a C global, it is a module-alias call
  `L.f(args)`, resolved through the alias as before (an unknown name is E_UnknownAlias). A
  variable therefore shadows an alias of the same name, as it does an enum name in `Name.X`.
- Otherwise it is rewritten to the plain `call` `f(recv, args)` and analyzed as any other call
  (callee lookup, argument count/type/permission checks). Only a function `f` whose first
  parameter's `var-type` has `mutable: true` — a Palan `@!T`/`@![n]T`, or a C non-const pointer —
  is a candidate; if no function `f` has one, it is E_MethodNeedsMutableFirstParam.
- `recv` is `addr-of` (`@!aa`) of the object, so the object must be addressable as for `@!` (a
  call returning a struct is, as a call argument). An
  object that is already a borrow (a `@T`/`@!T`/`@[n]T`/`@![n]T`/`@[+]T`/`@![+]T` variable, parameter or field) is
  passed as is; a read-only one is then rejected by the usual permission check.

sa.json carries only the resulting `call`; `member-call` is never emitted. A method-form call
after `->` is rewritten the same way (see Chained calls).

### Overloaded calls

Several functions can share a name: Palan functions whose parameter types differ (the same
parameter types, whatever the return, is E_DuplicateFuncDef), C functions, and functions imported
unqualified (or under one alias) all join the candidates for a call by that name. A function
brought in more than once — a header cincluded again, a module imported again — is one candidate.
A name with one candidate is bound to it directly, with the diagnostics of an argument that does
not fit it. With several, each argument is analyzed once without a parameter type, then:

- A candidate fits when the argument count matches (at least the fixed parameters for a C
  variadic function; from the parameters without defaults to all of them for a function with
  defaults) and every argument passes the parameter's checks; an argument matches
  exactly when its type is the parameter's, including `mutable` on every pointer level (through
  pointers and the elements of a `[+]T` or `[""]T`).
- An argument whose type comes only from its parameter is matched by what it can become: an
  untyped literal (or one the parameter's type reaches through `-`, `~`, a shift or arithmetic)
  fits any primitive and matches exactly its default type (int64, uint64, flo64; for an
  expression of literals and variables, the type it has with no parameter); an aggregate literal
  fits a pointer or struct and never matches exactly; a function name matches a function pointer
  parameter one of its functions fits (`ptrPermission`). It is analyzed with the chosen parameter's type.
- A chained value fits as `@v` where the parameter would take it so, and as `v` otherwise.
- The one fitting candidate is chosen; of several, the one every argument matches exactly
  (and no variadic argument is passed), or else the one every argument matches exactly but for
  a writable outermost pointer given to a read-only parameter. Of several ranking so, the one
  whose parameter types are the arguments' also in what the type registry leaves out (lengths,
  rows, elements' owners, functions' signatures; permissions aside) is chosen. When one of several fitting
  candidates needs a default to fit, none is preferred. Otherwise E_NoMatchingFunc (the argument
  types and the candidates) or E_AmbiguousCall (the candidates that fit, a default shown as
  `int32 = 0`).
- Receiving several values (`(a, b) = f(x)`) considers only the candidates returning that many,
  which must agree on what they return, since the receivers are declared before the arguments
  choose one (E_AmbiguousCall).
- A function name used as a value takes the function fitting the `@func` type it is given to (a
  variable's, field's or parameter's), or of several the one whose own type it is: none is
  E_FuncNameNeedsType (a single one differing only in permission, owner or shape is bound, and
  its mismatch reported), several E_AmbiguousCall.

### Default arguments

A parameter's default (its AST `init`) is analyzed where the function's signature is taken in, by
the module declaring it, so a caller needs none of the names it was written with: after
constants are folded it must be a number, `bool` or enum value typed and converted as an argument
for that parameter would be, or a string literal (E_ParamDefaultNotConst otherwise, including for
a struct parameter). Parameters with defaults come last (E_ParamDefaultNotLast); an operator
function has none (E_OperatorFuncDefault). A call omitting arguments is given the defaults, so a
`call`'s `args` always has one argument per parameter; a string default is analyzed at the call,
so its `str-literals` entry is in the calling module's table and only when used. A function
definition in `functions` keeps no defaults, and a function pointer type has none.

### Variadic arguments

An AST `[...]E` parameter (ASTSpec `specifier: "variadic"`) is normalized where any signature
type is (`sigType`), to `{"type-kind":"pntr","variadic":true,"base-type":<E as a parameter type>}`;
E as an array by value, `$T` or `_` is E_VariadicElemType. A variadic function cannot be a
syscall (E_SyscallVariadic), an operator function (E_OperatorFuncVariadic) or have defaults
(E_VariadicWithDefault). The key travels with the type: a function pointer type's parameter keeps
it, the registry's function type compares it (so a C callback type never matches), it is shown as
`[...]E`, and a symbol encodes the parameter as `X<E>`.

- A call fits a variadic candidate with at least its other parameters' count of arguments, each
  argument past them fitting E as a parameter of that type. Fitting so (with no argument for it
  too) leaves the candidate unranked, as a default does: E_AmbiguousCall when another fits.
- The arguments for it are bound as for a parameter of type E (a struct is copied into a block
  the callee owns; a literal or a call's result is a `temp`), then laid out in the `temp` of a
  frame array `__va_<n>` (`frame-alloc` of the arguments' count times the element's slot, then
  one `arr-assign` each). An argument's own `temp` is opened into the array's, so what it
  releases outlives the call, as the temps an `arr-assign` holds do. The call passes that array (`lit-int` 0 of type `@void` for none) and
  the count as a `uint64` `lit-int`: a call has one argument more than the signature's parameters.
- In `functions` the parameter is two: `xs`, typed as an array sized at run time
  (`{"type-kind":"pntr","run-size":true,"base-type":E}`), and `__xs_d0` (`uint64`), the length
  `len`, `for` and copies read as for a local `[n]E`. `xs` is no owner: its elements owning blocks
  are released at exit by the array helper's `fini`, as a frame array's are, and the array itself
  cannot be moved or taken (E_TransferFromNonOwner).
- A `spread` (AST) is the last argument (E_SpreadPlace, also anywhere outside a call's arguments)
  and the only one past the other parameters (E_SpreadPosition) of a Palan variadic parameter
  (E_SpreadNotVariadic, also for C's `...` and `len`). Its value is analyzed as any argument: an
  array keeping a length (E_SpreadNotArray; a `[]T`, E_SpreadUnsized), not written `@a`
  (E_SpreadBorrow), whose type `copySourceMatch`es `xs`'s (E_SpreadElemMismatch). Among overloads
  only such candidates fit, unranked.
- A given spread (`transfer`, or a call returning an owned array) passes the block it pins, a
  `[+]T`'s data pinned too; any other is copied (`CopyInto::New`) into a heap array `__va_<n>`
  (`____va_<n>_d0` holding a length known at run time). The `temp` frees only the block (and a
  `[+]T`'s header) after the call, since the callee releases the elements; the count is the
  source's length, or the copy's own.

### String literal conversion

A `lit-str` where the expected type is a struct `T` or `@T` (not `@!T`) is analyzed as the call
`T(lit)` or `@T(lit)` written there, when a conversion function exists: one visible by `T`'s
written name, unqualified or under any alias, returning `T` and fitting a call with the literal as
its only argument (its first parameter not a struct). The output has no node of its own: the call
is placed as any call returning a struct is at that position (`ret-dest` into the variable, a
`__cpy_<n>` copied from, a `__tmp_<n>` or `__arg_<n>` `temp`). With no such function the literal
is checked as before. Several such functions make the call E_AmbiguousCall. Borrowing it anywhere
but as an argument or operand (`@T` variable, field, return) is E_ConvertedLiteralToBorrow. In overload
resolution a literal fits a parameter it would convert for only as a last resort: such candidates
are ranked, by the same rules and with the converted argument exact, only when no candidate fits
without a conversion.

### Chained calls

An AST `chain-call` `v -> f(args)` is rewritten where SA takes it in (an expression, or the value
of a multiple-return declaration/assignment) by `normalizeChainCall`, after its `value` (a nested
`chain-call` first):

- A module-alias call is kept. A method-form call is rewritten first (see Method-form calls), and
  the chained values go in after its receiver instead of first.
- If `value` is not a call returning several values, the result is `call` with `value` inserted as
  its first argument, analyzed as any other call. For a read-only borrow parameter (`@T`, `@[n]T`,
  a C const pointer) a struct, numeric value or (for a Palan function) owned array is wrapped in
  `addr-of` (`@v`), so `@` may be omitted; a call returning a struct then becomes a temp as
  `@f()` does. `@!` is never added. When `call` names several functions, whether `@` is added
  depends on the one chosen (see Overloaded calls).
- Otherwise the return values are received into temps `__tap_N` by a `tapple-decl` and placed in
  the arguments the same way; a chain of such calls adds one `tapple-decl` per call.
- A single value given to a method-form call where the value or the receiver has a call is
  evaluated into a temp `__pin_N` by a `var-decl` first, so it comes before the receiver, and the
  temp is placed instead. A constant, an aggregate literal, an overloaded function name and a
  struct or array variable are left in place. `@f()`/`@!f()` is received into an owned temp and
  passed as `@`/`@!` of it; a struct or array field or element is held by its address; a returned
  struct is owned by the temp. A temp that owns a struct is freed after the call.
- A chain with such temps becomes a `temp` whose `body` is their declarations, `value` the last
  `call`, and `release` the frees of returned structs. As a multiple-return
  declaration/assignment's value this is E_ChainMultiToTapple.

sa.json never carries `chain-call`.

### Giving values away (`>>`, `<<=`)

A value is given away by an owner (an owning variable, an owned field or element), as a call
result, or by an `owner-view`; anything else is E_TransferFromNonOwner (naming `>>`, `->>` or
`<<=`). The owner's value is held in a temp (`__pin_N`, typed as the owned value) and the owner is
then set to NULL, as the source of `->>` is (for `x>>`, by the `temp`'s `reset`, after the call's
other arguments); the calls on the owner's path are evaluated once into temps first, so the reset
reaches the location that was read; a call result is given as it is, and an
`owner-view` is pinned but not reset (the pointer is no Palan owner; a read-only one is
E_MoveFromReadOnlyView).

- `transfer` (`x>>`) is analyzed only as a direct call argument; elsewhere it is
  E_TransferNotArg, and as the source of a cast E_TransferCast. Its value is the owned value
  with category `expiring`, passed to a parameter owning it (a Palan function's or a function
  pointer type's `T`, `[n]T`, `[+]T`) as a call result is, without a copy. Any other parameter
  (`@T`/`@!T`, any C function's, a variadic argument) is E_TransferToParam. `x ->> f(args)` is
  `f(x>>, args)` (`op` "->>").
- A later argument of the same call that may change the owner before its `reset` is
  E_GivenChangedByLaterArg, judged from the argument's AST at any depth: a `transfer` of a path
  that is a prefix of the owner's or has it as one; a `@!` borrow (an `addr-of` with `mutable`,
  or the receiver of a `member-call`) of a proper prefix of the owner's path or of a path read in
  one of its indices; or, when one of those paths starts at a global (a `global`, or a C global),
  any call (in the analyzed argument, so an operator function counts).
- `T b <<= v` declares `b` on its own, after `v` is evaluated: `<<=` initializers of one
  declaration run in order. `v` becomes a `pinned` value of `b`'s owned type with category
  `expiring`, which `b` takes over as it does a call result, declared as an array variable is
  (its run-time sizes kept). Another type is E_TransferShapeMismatch, a `$` variable E_TakeIntoFrameVar, and a variable owning no block
  E_TransferToNonOwnedVar.
- A local variable that has given its whole value away is NULL until `->>` fills it, and any
  other use of it is E_UseAfterGiven, at the use (PalanReference.md states which are uses). It
  becomes given when its statement (or `<<=` initializer) ends. Paths merge as given if given on
  any; a use a loop's head reaches before a fill is checked when the loop ends against the end of
  its body and each `continue`.
- A cast whose target is an owned struct or array (`ownedTypeOf`) from a `@!X`/`@X` pointer is a
  `convert` to the owned type with category `owner-view` (and `read-only: true` from `@X`);
  from anything else it is E_OwnerViewNeedsPtr. An `owner-view` is copied where an owned
  variable would be. Sizes known only at run time in the target type are not evaluated.
- A store with `owner-view-type` (`v ->> loc as T`) is the plain store of `v`, given away as
  above and retyped as `loc`'s pointer type, into `loc`, with nothing freed first. `loc` must
  hold a `@!X` and `T` be owned (else E_OwnerViewTargetNeedsPtr), and `v` match `T`
  (else E_TransferShapeMismatch).
- A cast's `target-type` has each `keep` replaced by the source's type at the same place, the
  source being a function pointer (else E_KeepWithoutSource) with as many parameters (else
  E_KeepParamCount, naming the source's function type at that place). A `keep`
  anywhere else is E_KeepOutsideCast.

### Call arguments

A fixed (non-variadic) call argument is checked against its parameter's type by `argConvOk(from,
to)`, not `typeCompat`. It accepts everything `usualArithConv(from, to) == to` would (a genuine
widen with no information loss) **plus** a same-rank signedness reinterpretation in either
direction (e.g. `int32` argument → `uint32` parameter, and `uint32` → `int32`) — the bit pattern
is unchanged, and a binding site's stricter same-width rule doesn't apply here since the
parameter is the callee's declared ABI width, not a variable the caller is naming. Anything else
(a genuine narrowing, or a cross-sign conversion where the destination is narrower) diagnoses
E_InvalidNarrowingConv, the same message a binding site uses. A pointer/struct argument is
unaffected by this rule (`argConvOk` only applies between two Prim types) and keeps its existing
`ImplicitWiden`-only check plus `checkArgPtrPermission`. A `bool` parameter accepts no
reinterpretation: an `int8`/`uint8` argument needs `bool(x)`. An enum argument is checked as
its base type; an enum parameter accepts only the same enum.

A bare integer-literal argument (`lit-int`/`lit-uint`) without its own `value-type` adopts the
parameter's type directly (SA passes the parameter type down as the literal's `expectedType`)
rather than defaulting to int64/uint64 and then being checked for narrowing — this is what lets
`add(1, 2)` bind to `int32` parameters without an explicit cast. A macro-folded `lit-int` (see
above) keeps its own type instead and goes through `argConvOk` like any other typed argument —
this is what lets `mkdir(path, S_IRWXU)` bind `S_IRWXU`'s `int32` to a `uint32`-typed `mode_t`
parameter via the same-rank signedness reinterpretation described above, without an explicit
cast.
- `pntr(T)` and `pntr(T, mutable=true)` are treated as `Identical`; base-type match is sufficient
  for arr-assign target type checking. `mutable` is a write-permission attribute, not part of
  type identity, so `typeCompat` never inspects it.
- Mutability (read-only `@T` vs. mutable `@!T`) is enforced separately from `typeCompat`, at
  every point a pointer value is written through or bound to a typed destination:
  - **Write-through**: `p[0]` deref-write, and field access via a `@T`-typed local variable or
    struct field, are a compile error (E_WriteThroughReadOnlyPtr / E_WriteToImmutablePtrField /
    E_WriteToReadOnlyArrElem) when the base pointer is not `mutable`.
  - **Binding**: assigning, initializing, returning, or passing a `@T` (read-only) value where a
    `@!T` (mutable) type is expected is a compile error — permission can be narrowed
    (mutable → read-only) but never upgraded. Passing to a Palan function parameter or returning
    from a Palan function uses E_PtrMutabilityUpgrade; passing to a cincluded C function's
    parameter (including through an aliased `cinclude ... as X;`, via `X.func(...)`) uses
    E_ReadOnlyPtrToNonConstCParam and names the function and parameter.
  - A missing `mutable` key (SA-synthesized `pntr` types for struct and array variables) means
    writable — there is no read-only concept for those forms in this iteration.
  - A C parameter or return type's own `mutable` comes from its pointee's `const` qualification
    (`base-type.const`), folded in by `normalizeCType` at the point a `cinclude`d function
    signature, global, or struct field is registered (`PlnSaInternal.h`) — `const T *`
    normalizes to `mutable: false`, `T *` to `mutable: true`. This is the same ingestion-boundary treatment struct/array
    variables get elsewhere: SA's own permission checks only ever need to understand `mutable`,
    never c2ast's `const` vocabulary.

C-origin signature admission
-----------------------------
Before a `cinclude`d C function's parameters/return type (or a C global's single type, see the
**cinclude** entry in Statement model above) are usable, `normalizeCFuncSig`/`normalizeCGlobal`
(`PlnSaInternal.h`) apply `normalizeCType` (folding `pntr`'s pointee `const` to `mutable`, and
a named `strct`/`union`→`struct`) and then check every type against `unrepresentableTypeName`, which recognizes:

- A `prim` type-name not resolved to a known Palan primitive (the C spellings c2ast gives
  `__int128` and the `_Complex` types, such as `double _Complex`).
- `arr` (a bare array type-kind — the `[n]@T`/`[n]@!T` pointer-slot-array shape has already
  normalized to `pntr` by this point), a `func` whose parameters or return are not all
  primitives or pointers representable by these same rules (a variadic one, or one taking a
  struct by value), a `strct`/`union`
  with no `type-name` (a genuinely untagged struct reference — c2ast synthesizes a tag from
  the typedef name for a single, non-derived `typedef struct { ... } Name;`, so this is a
  multi-declarator/derived-declarator typedef of such a body, not every anonymous struct),
  and `user` (an identifier c2ast could not resolve to a known type — including a typedef
  that bottoms out in an anonymous union/function-pointer body, or an anonymous struct
  body via one of those two unsynthesized typedef shapes). A `user` naming a typedef an
  earlier cinclude exported in `ast.typedefs` never gets here: `resolveCTypeRefs` replaces it
  with that typedef's type when the cinclude is ingested, as it does for an enum reference.
- A `struct` type-kind carrying a `type-name` is always representable here, whether or not it
  is complete — an incomplete struct's layout is enforced later, only where a layout is actually
  needed (see "Incomplete struct types" below), not at signature-admission time. This applies to
  a struct **return** type, including one that requires the full System V AMD64 by-value return
  classification at the call site (see the call expression's `struct-ret` entry above) — the
  return direction is fully representable. A by-value struct used as a top-level
  **parameter** is rewritten to the shape of a native struct parameter (`pntr` to `struct`
  without `mutable`), so a call checks its argument as for a native function; the call's
  argument then carries `struct-arg` (see the call expression above). Unlike a native struct
  parameter, a read-only `@T` is accepted, since the callee receives a copy.
- `normalizeCType` folds a `pntr` to a `func` to `"mutable":false` whatever C's `const` says,
  drops the parameters' `name`s and the node's `typedef-name`, and recurses into the return type
  and parameters, so a C function pointer has the same shape as a native `@func(...)`. A C
  typedef of a function type (not a pointer, e.g. `cookie_read_function_t`) is expanded by
  c2ast where it is referenced, so `cookie_read_function_t *` arrives as a `pntr` to a `func`.

**Deferred to reference time, not rejected at cinclude time**: a hit does not fail the `cinclude`
statement itself. It is recorded on the entry (`_unsupported-sig` for a function,
`_unsupported-global` for a global) and diagnosed only when the function is called
(`requireSupportedCFuncSig` → E_UnsupportedCFuncSignature) or the global is referenced
(`requireSupportedCGlobal` → E_UnsupportedCGlobalType) — so cincluding a header that happens to
declare one unsupported function or global does not prevent using the header's other, supported
declarations.

Enum types
----------
`type Name enum { ... }` (an `enum-def` node) is consumed by SA and does not appear in sa.json.
An enum type is a `prim` of its base integer type with an `enum` key naming it, e.g.
`{"type-kind":"prim","type-name":"int32","enum":"Color"}`, and this is the only form it takes:
SA registers the name in the same table as a type alias, so every place a type name resolves
(variables, fields, arrays, pointers, parameters, returns, `type X = Name;`) yields this shape.
Storage and codegen read only `type-name`; the `enum` key matters only to type checking.

- Base type: C's enumerator type. `int32` if every value fits, else `uint32` if none is negative
  and all fit, else `int64`/`uint64` (GCC's widening). Its width always equals GCC's choice for
  the enum type itself (which may differ in signedness, e.g. `unsigned int` for a non-negative
  enum), so the ABI is unaffected. Values are limited to the int64 range.
- An enumerator value is the previous one + 1 (0 first) or `= ` an integer literal / const.
- `Name.X` (an AST `field-access` whose object is an `id` naming an enum type or an alias of
  one, not shadowed by a variable) becomes `{"expr-type":"lit-int","value":<decimal>,
  "value-type":<enum type>}`.
- `Name(x)` (an AST `call` naming an enum type, when no function has that name) is a cast.
- Removing or adding only the `enum` key never emits a `convert` node: `wrapConvert` rewrites
  the `value-type` instead.
- A struct field keeps its enum type (`FieldLayout::enumName`), so a field read has the enum
  type and a field store is checked as a binding.

A C enum (a `cinclude` statement's `enums`) registers into the same tables with the same
base-type rule; a cinclude'd enum of a name already taken is skipped when it is itself a C enum
(the same header included again) and E_DuplicateTypeName otherwise. An AST `enum` reference
(`{"type-kind":"enum","type-name":N}`) is resolved to the enum's type where C types enter SA:
function signatures and globals (before `normalizeCFuncSig`/`normalizeCGlobal`), struct fields
(`registerCStruct`), typedefs, and a macro constant's `value-type` on a gen-ast-folded
`lit-int`. A reference to an enum c2ast did not capture stays `enum`, which
`unrepresentableTypeName` reports. A C typedef of an enum is an alias of it, so both `Tag.X` and
`Typedef.X` resolve.

Struct types
------------
`type Name { field_decl... }` defines a struct type. The SA processes `struct-def` nodes and
registers the type in its internal registry; the node is consumed and does not appear in sa.json.
An array field's `size-expr` is evaluated like any array size (a constant expression folds to a
literal); a size that is not a compile-time integer is an error.

A struct captured from a `cinclude`d C header (ASTSpec.md's `cinclude` statement `structs`
field) registers into this exact same internal registry — there is no separate representation
for a C-origin struct. Every sa.json shape documented below (C ABI layout, var-decl,
field-assign, field-access) applies unchanged regardless of whether the struct came from a
native `type Name {...}` or a `cinclude`d header.

A C union (Struct definition model entry with `"union": true`) registers into the same registry
as a struct whose fields all sit at offset 0, with `totalSize` = the largest member size rounded
up to the largest member alignment. sa.json has no union marker: a union appears everywhere as a
`struct` type-kind, and every consumer (allocation, field access, address-of, pointer
compatibility, System V classification) works from offsets and `totalSize` alone. `StructDef`
keeps an `isUnion` flag used only by the layout computation and by diagnostic wording (e.g.
"union 'U' has no field 'c'").

### Incomplete struct types

A registered struct tag may be *incomplete* — its name is known but no field layout is available,
the same distinction C makes for a forward-declared or opaque tag (e.g. `DIR`). `StructDef` has
`bool isComplete` and `string incompleteReason`; when `!isComplete`, `fields` is empty and
`totalSize` is `-1` (there is no partial layout — either the layout is fully known or not known
at all). `incompleteReason` is one of exactly two values:

- `"forward-declared"` — the header only ever mentions the tag: a forward declaration (`struct
  Foo;`) or a bare reference through a field/parameter/return whose pointee struct is never
  defined in this header (ASTSpec.md's Struct definition model — `fields` omitted). No field
  information exists to fail on.
- `"unsupported-field"` — the header gives the tag a field list, but at least one field is
  rejected by "C-origin field admission" (below).

Either way the tag name still resolves — referencing it is not E_UnknownType, the "no such
type" error — but any use that requires a known layout is E_IncompleteStructType instead:

- Field-chain resolution, including a plain field read or an intermediate hop in a longer chain
  (`resolveObjectChain`) and field-assign (write).
- Subscripting a struct-element array (`arr[i]` needs the element stride, i.e. `totalSize`).
- A by-value struct variable declaration of the tag (`T x;`), a `[n]T` owned-pointer-array
  declaration, or a `[n]$T` contiguous embedded-array declaration.
- Embedding the tag as another struct's field — as a `$T` embed leaf, an owned struct-pointer
  leaf, or a struct-element array leaf (`[n]T`/`$[n]$T` field) — checked while the *enclosing*
  struct is being built, so an incomplete struct cannot be smuggled in as a sub-struct either.

An incomplete struct remains fully usable as a pointer: a local variable, function
parameter/return type, or cinclude'd C function argument of type `@T`/`@!T` all work normally,
since none of those require a layout — matching C's own incomplete-type semantics (a `FILE *` is
usable everywhere `FILE` itself is not).

A tag already registered incomplete may later be promoted to complete by a `cinclude` that
supplies a full, fully-supported field list (first *complete* definition wins). The reverse never
happens: a tag already complete keeps its layout regardless of what a later header says.

### C-origin field admission

Before a C-origin field list is registered, each field's `var-type` (ASTSpec.md's c2ast
representation) is first passed through `normalizeCType` (so every pointer level, including a
pointer-slot array's slot and the levels under it, gets `mutable` from its pointee's `const`:
`T *f;` is `@!T`, `const T *f;` is `@T`), then normalized to the same vocabulary a native
`struct-def` field would use, and checked for support; a field the check rejects causes the
*whole* struct to be registered as an incomplete struct (see "Incomplete struct types" below)
rather than being dropped individually or the tag going unregistered — fields up to and including
the failing one are discarded, and no partial layout is kept:

- A by-value struct or union leaf (`type-kind:"struct"` after `normalizeCType`, from
  `struct Foo f;`) is rewritten to `type-kind:"embed"` — same shape a native `$Foo` field
  produces.
- A `prim` field is supported iff `elemSizeBytes` resolves its type-name.
- An `embed` field is supported iff its struct name is already registered and is not the
  enclosing struct itself (no direct self-embedding).
- An `arr` field (a C array declarator, of any number of dimensions) lies in its struct, so it
  is rewritten to the native `$[n]…` shape: wrapped in `embed`, with `embedded` kept only on the
  levels whose elements are laid out in place (rows and by-value struct leaves, the latter
  rewritten to a plain `prim` type-name leaf). `int cells[2][3];` becomes `$[2]$[3]int32`,
  `struct Foo a[4];` `$[4]$Foo` and `T *f[n];` `$[n]@!T`. It is supported only when every
  `size-expr` is a literal (a `T name[];` field has no fixed size and is rejected) and its leaf
  is a pointer, a primitive with a known size, or an already-registered complete struct other
  than the enclosing one.
- A field with `bits` (a C bit-field) must have a folded width (a `null` one is rejected).
  It is laid out as GCC does on x86-64: it moves to the next unit of its declared type only when
  it would straddle one (packed: never), a zero-width one moves to the next unit even when
  packed, and only a named, unpacked one raises the struct's alignment. A named one inside one
  unit of its declared type, with the unit inside the struct, is a field of that type at the
  unit's offset, read and written with `bits` (see field-access below); taking its address is
  E_AddrOfBitField, and an integer literal outside its width E_BitFieldLiteralOutOfRange. One a
  packed struct lays across two units, or past its end, covers the bytes its bits occupy and is
  E_UnsupportedCFieldType on any use. An unnamed one only takes its place.

### C ABI layout

Fields are placed at offsets aligned to their natural size (same as `sizeof`):

```
offset = 0
for each field:
    align = elemSizeBytes(field.type)   // size == align for all primitive types
    offset = alignUp(offset, align)
    field.offset = offset
    offset += align
maxAlign = max of all field alignments
totalSize = alignUp(offset, maxAlign)
```

Example: `type Mixed { int32 a; int64 b; }` → a@0 (4B), 4B padding, b@8 (8B), total=16B.

An array field is typed as an array variable of its type is. Without a leading `$`
(`[n]U`, `[+]U`, `[n]$S`, `[n]@T`) it is an owned field, an 8-byte slot holding its block. With
one (`$[n]U`) the array is laid out in the struct as `$[n]U` lays it out in a frame: it takes
the bytes and alignment of its elements laid out in place, and a field access yields it as the
address of that place (`addr-only`) typed as a `[n]U` variable is. When it holds owned elements
(`$[n]S`, `$[n][m]U`, `$[n][+]U`), their slots are built, freed and copied with the struct (an
`owned-fields` entry), which then owns fields itself; `$` on an element that owns storage is
E_EmbedOwningType, as for a variable.

### struct variable declaration → sa.json

`type Point { int64 x; int64 y; }` followed by `Point p;` produces:

```json
{"stmt-type":"var-decl","vars":[{
  "name":"p",
  "var-type":{"type-kind":"pntr","base-type":{"type-kind":"struct","type-name":"Point"}},
  "init":{"expr-type":"call","name":"__pln_malloc","func-type":"c",
          "args":[{"expr-type":"lit-int","value":"16",
                   "value-type":{"type-kind":"prim","type-name":"uint64"}}],
          "value-type":{"type-kind":"pntr","base-type":{"type-kind":"struct","type-name":"Point"}}}
}]}
```

Struct variables are registered in `arrayScopeVars_` and a `free()` statement is auto-inserted
at scope exit (same mechanism as heap-allocated arrays).

When the initializer is a call returning the same struct (`Point p = makePoint();`,
`div_t d = div(7, 2);`), the var-decl's `init` is a raw `__pln_malloc` of the struct's size even when
the struct owns fields, since the callee builds them, and the call follows the var-decl as an
`expr` statement whose `ret-dest` is the variable. A struct variable with an initializer is
always emitted as its own var-decl, so `Point a = b, c = f();` evaluates left to right.

### Stack variable declarations

A struct or array variable declared with `$` (`$Point p;`, `$[4]int32 a;`) gets the same
`var-type` as without `$`; its `init` is a `frame-alloc` instead of the allocation call, and
what the heap version does through its allocator and free functions is derived from them:

- `__pln_malloc` → nothing (left undefined).
- `__pln_alloc_K(args)` → `__pln_init_K(v, args)` follows the var-decl.
  build-mgr generates `__pln_init_K` (fill an existing block) and `__pln_fini_K` (release what
  the block owns, not the block) for every alloc-shape alongside the alloc/free pair.
- At scope exit, `free` → nothing; `__pln_free_K(v, args)` → `__pln_fini_K(v, args)`.

Only the outermost block is in the frame, so a `[n]T` struct array or a 2D array keeps its heap
elements or rows. `size` must be a compile-time constant (E_FrameArrSizeNotConst). A struct
returned by a call (`$Point p = makePoint();`) is written into `p` directly: the call follows
the var-decl with `p` as its `ret-dest`, and no `__pln_init_K` is emitted, since the callee
builds what the struct owns.

A stack variable is registered in `arrayScopeVars_` with its scope-exit statement (if any) but not
as an owner: an `id` naming it has no `"category":"owned"`, so it cannot be the source of `->>`,
and `return` of a stack struct or `$[+]T` returns a copy. `return` of a stack array is E_ReturnFrameArr;
`$` on a non-aggregate type is E_FrameVarType.

### Static variables

A `global` variable of the module and a C global are static variables: a name bound to a slot at
an assembly label. Expressions and statements refer to one by `name`, as to any variable; what
binds the name is a function's (`main`'s, `..init`'s) `globals` entry, made at its entry, or, in
`..init`, a `global` variable's `var-decl`, whose variable carries `label`. The slot holds what a
frame variable's value would be, a pointer for an array or struct.

- A Palan global's `label` is `<module>.<name>`; its `var-decl` lowers as a top-level variable's
  does, except that a `frame-alloc` becomes a `static-alloc` and what it owns is never freed (a
  value it is reassigned away from still is). `main` binds every Palan global with its own type.
- A function sees a Palan global its permit lists as it would a parameter: an array or struct as
  `storageBorrowType` gives (`@` without `!`, `@!` with it), and the function cannot assign the
  variable itself (`E_GlobalNotReplaceable`); anything else with the variable's type, assignable
  only with `!` (`E_GlobalNotWritable`).
- A C global's `label` is its C name. It is readable anywhere it is visible, and writable at the
  top level after a `global` statement names it, or in a function a permit lists with `!`.
- An imported `export global` keeps its module's label and is bound by `name` as written: `count`,
  or `M.count` under an alias, which SA reads from a field access or a field assignment on the
  alias where no variable hides it. It is visible from the import to the end of the enclosing
  scope. An import at the top level binds it in `main` and `..init` with its own type, and lets
  permits name it; one in a function binds it there with its own type, readable and writable. Its
  type comes from its declaration alone, the sizes an array literal gives filled in, so a source and
  its interface give one type; a size known only at run time is `E_ExportedGlobalSizeNotConst`. A
  name already declared in the scope, by the module or by another import, is `E_DuplicateVarDecl`.

### Module initialization

`<module>..init` (in `functions`, exported, no parameters) is generated for every module, since
each importer calls it. Its body, when the module imports something or declares a `global`:

1. `if` its flag `__init_done` (bound in its `globals` to `<module>..init.done`, `uint8`) is not
   0, `return`; then `1 -> __init_done`, so a circle of imports reaching it again returns at once.
2. An `expr` `call` with no arguments of the `..init` of each module the source imports, anywhere
   in it, in order of first import, the module itself excepted.
3. The `global` variables' declarations, in source order.

An initializer is analyzed in a scope of its own holding the globals declared before it, so it
cannot use an ordinary top-level variable (`E_GlobalInitUsesTopVar`); temporaries and C globals it
uses belong to `..init`.

### field-assign statement → sa.json

`10 -> p.x` (inline or `$T` chain — no pointer indirection) produces:

```json
{"stmt-type":"field-assign",
 "var":"p",
 "offset":0,
 "value-type":{"type-kind":"prim","type-name":"int64"},
 "value":{"expr-type":"lit-int","value":"10",
          "value-type":{"type-kind":"prim","type-name":"int64"}}}
```

`10 -> r.tl.x` (where `tl` is an owned `Point` — pointer indirection required) uses `ptr-expr`:

```json
{"stmt-type":"field-assign",
 "ptr-expr":{"expr-type":"field-access","var":"r","offset":0,
             "value-type":{"type-kind":"pntr","base-type":{"type-kind":"struct","type-name":"Point"}}},
 "offset":0,
 "value-type":{"type-kind":"prim","type-name":"int64"},
 "value":{"expr-type":"lit-int","value":"10",
          "value-type":{"type-kind":"prim","type-name":"int64"}}}
```

`ptr-expr` is present when the field chain passes through an owned (`T`) or non-owning (`@T`/`@!T`)
struct pointer field, or starts from an expression other than a variable (a call returning
`@T`/`@!T`, a cast). It is an expression that gives the intermediate pointer; `offset` is then applied
relative to that pointer. When `ptr-expr` is present, `var` is absent; when `var` is present, `ptr-expr`
is absent.

### field-access expression → sa.json

`p.x` (inline or `$T` chain — no pointer indirection) produces:

```json
{"expr-type":"field-access",
 "var":"p",
 "offset":0,
 "value-type":{"type-kind":"prim","type-name":"int64"}}
```

`r.tl.x` (where `tl` is an owned `Point`) uses `ptr-expr`:

```json
{"expr-type":"field-access",
 "ptr-expr":{"expr-type":"field-access","var":"r","offset":0,
             "value-type":{"type-kind":"pntr","base-type":{"type-kind":"struct","type-name":"Point"}}},
 "offset":0,
 "value-type":{"type-kind":"prim","type-name":"int64"}}
```

`n1.next.val` (where `next` is a `@Node` non-owning pointer) produces the same `ptr-expr` structure:

```json
{"expr-type":"field-access",
 "ptr-expr":{"expr-type":"field-access","var":"n1","offset":8,
             "value-type":{"type-kind":"pntr","base-type":{"type-kind":"struct","type-name":"Node"}}},
 "offset":0,
 "value-type":{"type-kind":"prim","type-name":"int64"}}
```

`@!p.x` (address-of on a struct field) produces the same node shape, plus `addr-only:true` and
a `pntr`-typed `value-type` in place of the field's own type — see the addr-of section above:

```json
{"expr-type":"field-access",
 "var":"p",
 "offset":0,
 "value-type":{"type-kind":"pntr","mutable":true,"base-type":{"type-kind":"prim","type-name":"int64"}},
 "addr-only":true}
```

A C bit-field's `field-access` and `field-assign` add `"bits":{"shift":<int>,"width":<int>}`:
the field is `width` bits from bit `shift` of the unit of its `value-type` at `offset`. A read
extends the sign of a signed type; a write keeps the unit's other bits and the value's low
`width` bits.

`resolveObjectChain(object, forWrite)` is the single field-chain resolver used by field-access
reads, field-assign writes, and the addr-of field-access shape alike — `field-assign`'s
`forWrite` is always `true`; a plain read passes `false`; addr-of passes its own `mutable` flag.
Besides a plain `id` base, the chain root may also be an `arr-index` node whose element is
itself a struct pointer (`pts[i].x` where `pts` is `[n]$T`/`[n]T`/`[n]@T`, or `p[i].x` where `p`
is `@T`/`@!T`) — the index expression is evaluated via `sa_expr_arr_index` first, and the
write-permission check (E_WriteToReadOnlyArrElem) applies to the array's own mutability exactly
as it would for a plain `arr[i]` write. Any other chain root — a call result, a parenthesized
tuple, or anything else that isn't `id`/`arr-index`/`field-access` — normalizes to `"not-impl"`
at the AST layer (see ASTSpec.md's `not-impl` object shape) and is rejected as
E_FieldAccessOnNonStruct: a field chain can only be rooted in something that names storage, never
in the result of evaluating an expression.


