# Specification and Design of Palan

## 1. Purpose
This document specifies the goals, scope, architecture, and requirements for the Palan programming language (Palan), covering language design, compiler and toolchain, runtime, standard library, and tooling. It is intended to guide language design decisions, implementation, testing, and documentation.

## 2. Goals
- Palan aims to be a simpler, safer, and more enjoyable programming language alternative to C.

### 2.1 Iteration Goal (2026-11-25)
version: 0.2.8 — tagged unions, and std/json on them

- A type may contain itself through the elements of a `[+]T` or `[""]T` (3.11).
- A tagged union `type U union {...}` holds one of its variants, owned, copied and released by the
  tag the compiler keeps; `is` tests it and binds its contents (3.11).
- Literals of bool, arrays and dictionaries convert to a union, and bool to a struct (3.11).
- `std/json` parses, holds and writes JSON (3.12).
- A type defines `[]` (3.11).
- Returning a borrow of the function's own storage is an error (of doc/Issues.md #1). Hexadecimal
  literals.
- `export import` publishes an imported module's declarations as the importer's own (3.7).

Out of scope: interfaces; an owned field that may be NULL (`node next`); `is {}` on an enum or a
number, guards and nested patterns; `is {}` as an expression.

## 3. Command-line Tools' Responsibilities and Design

### 3.1 Build Manager (palan)

Responsibility:
  The tool is responsible for orchestrating the compilation process.

Design:
 The build manager generates the AST of the given source file by invoking palan-gen-ast, then
 follows its `import`s of other Palan source files and generates their ASTs in turn. It then unions
 every module's `libpath`s and finds each `import <...>`'s interface (3.7).
 For each source module it invokes palan-sa and palan-codegen and assembles the result with `as`,
 skipping every step whose result is still valid (5.1). A module compiles the same whichever module
 is the entry. The allocators the modules need are generated as one more module (see 3.6). The entry
 is chosen only at link time: `palan-codegen --entry` emits a `_start` calling the given module's
 initialization function and then its top-level function, and all object files and the archives of the libraries used are linked with
 `ld` to create the final executable. Two modules with one identity are reported here, before `ld`.
 The target is decided once, here. It is passed to palan-gen-ast, which records it in ast.json for
 the rest of the pipeline, and to `palan-codegen --entry`, which has no such file to read.
 Options are read only before the source file; every argument after it goes to the program.
 With `-o`, the executable is written to the given path and not run, and an argument after the
 source file is an error. Without it, the executable is linked in the entry's directory under the
 working directory (see 5.1) and run with those arguments, and left there like the other work
 files, which `--clean` removes. Only `-o` writes into the current directory, so palan can be used
 as a script runner without leaving build artifacts, without touching an `a.out` already there,
 and with several sources in one directory run at the same time.
 What `ld` is given beyond the objects is the union of every module's `libs` and `objs` (SASpec.md's
 Root) and every used library's `link` statements (3.7). Palan archives are linked in one
 `--start-group`/`--end-group`, since the union keeps no order.
 A build is in debug or release mode (PalanReference.md's Command-Line Usage), the default following
 `-o` since a program run directly is being tried and one written out is being delivered. The
 checks of the writer's errors are left out of a release build by `palan-sa --release` and by the
 generated allocators; debug information and optimization do not exist yet. A failed allocation
 is checked in either mode, as no debug run finds it.

 Code palan-sa and the allocators generate allocates through `std/rt` (`__pln_malloc`, ...) and
 fails a check through it. It is a Palan library built with std and given to every `ld`, so a
 program compiles nothing more for it, and it is written in Palan, for any target, not emitted by
 palan-codegen as `_start` is.

### 3.2 AST Generator (palan-gen-ast)
Responsibility:
  The tool generates the Abstract Syntax Tree (AST) from Palan source code.

Design:
 For each `cinclude` the AST generator runs palan-c2ast and embeds its output in the cinclude
 statement node. It passes each run the `c-state` the previous run output (`--state`, through a temporary file),
 so every header is preprocessed and parsed with the macros, typedefs, struct/union tags and
 enums in effect at its `cinclude`, as if the headers cincluded so far in the enclosing scopes were
 included into one C translation unit. The state follows block scope: at a block's `}` it returns to
 what it was at the `{`, so a header read inside the block is read again by a later `cinclude`
 outside it, whose scope would otherwise get none of its functions. The macro constants a `cinclude`
 registers likewise fold only up to the end of its scope. The state is restored from a history of
 (location, state) at the block's reduction, not saved at the `{`: a mid-rule action there would add
 GLR conflicts.
 To parse the Palan source code, the AST generator uses a parser generated by Flex and Bison.
 The parser is a GLR parser (Bison's `glr2.cc`), which passes `$N` to grammar actions as `const`
 references because split stacks may share a value, so actions never move from or modify `$N`.
 A growing list is therefore a persistent list (`PList`) that shares its prefix, and statements
 hold nested blocks by handle (`LazyNode`) and are converted to JSON once at the end; appending or
 nesting never copies what is already built.
 The grammar is kept GLR rather than converted to LALR(1): its conflicts (`.` after a term, a
 declaration list's `ID = expr`, `[expr]` as a type or a literal) are resolved by reading further,
 which keeps the syntax free of rules made for the parser. The cost is a nesting depth limit
 (`YYMAXDEPTH`), since Bison's `glr2.cc` frees a value twice when it grows its stack.
 The grammar holds the language and the forms reserved for a planned feature, which it parses
 for palan-sa to reject as not implemented (`name = expr`, for type inference). A form without a
 plan is not parsed: a feature is designed with the language of its time, and a parsed form nothing
 uses has let code be silently ignored.
 A file named `.pai` is read as an interface (3.7): it may declare a function without a body and
 hold `link` statements, and must hold `built` and `target` statements of the tools' version and the target. `--interface`
 writes the module's own declarations in that form; merging a library's modules is `--lib`'s.
 `libpath "dir";` is recorded in ast.json as an absolute path, and is rejected in a `.pai`.

### 3.3 C-to-AST Translator (palan-c2ast)
Responsibility:
  The tool translates C header files into AST nodes compatible with Palan.

Design:
 The output's top-level `c-state` is where the header leaves off, for the next run to start from:
 `macros`, the macro table written as `#define` lines (each body re-spelled token by token) plus a
 `#pragma palan_once_included "<path>"` line per file read under `#pragma once`; and `typedefs`,
 `structs`, `enums` and `enumerators`, the parser's tables, including entries the AST leaves out
 (an enum whose values can't be computed). `--state` loads it in place of predefined.h, so
 predefined macros an earlier header `#undef`ed stay undefined, and a header whose include guard
 skips it still has its types for `sizeof` and its enumerators for constant expressions.
 Nothing loaded this way is exported to the AST again unless the header changes it: a redefined
 macro, or a struct the earlier header only referenced and this one defines. The macros the
 header `#undef`s are listed in `ast.undefined-macros`.
 `__GNUC__` is not predefined, so headers take their non-GNU paths, but `__attribute__` is a
 keyword: `packed` and `aligned` reach the struct layout (ASTSpec.md, Struct definition model).
 The C expression parser builds an expression tree in the AST's form, and a constant (a macro, an
 enumerator, an array size) is folded by evaluating that tree, so one parser serves both. A
 function-like macro becomes an expandable definition (3.4): the preprocessor expands the macros in
 its body with each parameter's place marked, and the tree is its body. Type names and enumerators
 are resolved here, where C's tables are; a function or global the body names is left to palan-sa,
 marked as a C name. A body with a form Palan has no expression for (`?:`, an assignment, a comma,
 a string, `sizeof` of an expression, `#`/`##`, `__VA_ARGS__`) is left unexported with that form.

### 3.4 Semantic Analyzer (palan-sa)
Responsibility:
  The tool performs semantic analysis on the generated AST and checks type correctness and scope rules.

Design:
 The semantic analyzer reads the ast.json file and builds a new sa.json independently, carrying
 over its target. An imported module's AST is read from the same work tree as its own: the root is
 the AST file's path without its `original` and `.ast.json`, since work files mirror their sources'
 absolute paths (5.1). An AST outside that layout can import no Palan module. An `import <...>` is
 looked up in the `--library` table instead, for the importing module and for the `.pai`s it reaches;
 one that is not there is an error, since finding it is the build manager's.
 An AST made from a `.pai` can only be imported, not analyzed itself: it has functions without bodies
 and so no code to compile.
 A const whose value names a top-level variable is evaluated from that variable's declared shape
 alone (the written type and an array literal's element count), so it is known before the
 variable's initializer is analyzed in statement order.
 A static variable (SASpec.md's Static variables) follows the rule a parameter of its type follows.
 The storage of a `$T` or fixed-size array `global` is static rather than in the top level's frame,
 so a function C calls after the top level returns (an `atexit` handler) still finds it. An array
 of numbers whose literal is all constants starts with its values in `.data` instead of being
 stored element by element in `..init`, so a table such as std's grapheme properties costs
 its data alone; with any element not constant, every element is stored as before, since storing
 the constant ones first would change what a function called by the initializer sees.
 A C bit-field is a field of its declared type at the unit of that type holding it, with its
 shift and width (SASpec.md), and codegen reads and writes the unit by shifts and masks. Turning
 it into shift and mask expressions in palan-sa was not taken: a write reads, changes and writes
 back the unit, so the unit's address must be evaluated once, and Palan has no arithmetic shift to
 extend the sign. A bit-field a packed struct lays across two units is left unsupported, since
 none in the C library or Linux headers is laid so.
 A float constant expression is folded in the widest float, flo80, before its context gives it a
 type, so a flo80 constant keeps its precision; a flo32 or flo64 context rounds the folded value
 again, as a C compiler evaluating in x87 does.
 An expandable definition (ASTSpec.md) is a name, parameters, a body expression and a binding,
 and a call to it is analyzed as its body at the call: the body is copied, its parameters bound to
 the arguments, and the copy analyzed in the caller's context, so the result is ordinary sa.json
 and codegen knows nothing of it. A function-like C macro binds by substitution: each place a
 parameter appears takes the argument expression, untyped and evaluated as often as it appears, as
 the preprocessor does. An inline function would bind by evaluating each argument once into a
 typed parameter; the copying, the binding by place and the name resolution are shared. Expanding
 in palan-gen-ast, as macro constants are folded, was not taken: an inline function needs
 overloads, types and temporaries, which only palan-sa has. Inlining in codegen was not taken
 either, since an untyped macro body is no function. The body's C names resolve only among the C
 declarations visible at the call, never a Palan name there, as an inline function's names would
 resolve in its own module. A macro is chosen over a C function of its name, as in C, and needs an
 aliased cinclude's qualifier as a function does; one that cannot be expanded (a form left
 unexported, a C name not declared) calls that function, or is an error naming the macro when
 there is none.

### 3.5 Code Generator (palan-codegen)
Responsibility:
  The tool generates x86-64 assembly from the analyzed AST (sa.json).

Design:
 The code generator reads sa.json and emits x86-64 AT&T syntax assembly for use with `as`. An sa.json
 whose target is missing or unsupported is an error.
 A module's top-level statements are its exported function `<module>..main`, which takes argc and
 argv. The entry point `_start` calls each symbol `--entry` names, in the order given (the
 module's `..init` (3.7), then its `..main`), each with argc and argv from the initial stack,
 then `exit(0)`.

 Calling convention:
 - C and Palan function calls share the x86-64 System V argument assignment: integer/pointer
   arguments take rdi, rsi, rdx, rcx, r8, r9 and float arguments take xmm0-xmm7, each counted
   by its own class index; arguments beyond either sequence go to the stack in order.
 - Before a C call, al is set to the number of XMM argument registers used (required by
   variadic functions such as printf).
 - A single return value is returned in rax, or xmm0 for a float (System V compatible).
 - Two or more return values take each class's argument register sequence in order
   (integers in rdi/rsi/rdx/..., floats in xmm0/xmm1/...; caller-saved, read immediately after call).
 - A flo80 (C's `long double`) follows System V: an argument goes to the stack at a 16-byte
   boundary, as a struct of that alignment does, and a single return value, or a returned struct
   that is one flo80 alone, comes back in st(0). Among two or more return values it takes the next
   float register whole instead, so a call whose values are dropped leaves the x87 stack empty.
 - A struct is returned as System V returns it, by Palan and C functions and through function
   pointers alike, into storage the caller provides: one of up to 16 bytes comes back in rax/rdx
   and xmm0/xmm1 by its eightbytes' classes and the caller stores it; a larger one is written
   through a pointer the caller passes as a hidden first argument (rdi), which the callee
   returns in rax. Each struct among two or more return values is written through such a
   pointer, one per struct in order, ahead of the arguments. palan-sa only says where the value
   goes (`ret-dest`, SASpec.md); the hidden arguments and the stores are codegen's. An array is
   still returned as a pointer to a heap block.
 - Normal Palan functions use standard frame setup (pushq %rbp / movq %rsp, %rbp / subq $N, %rsp)
   with frameSize rounded to a multiple of 16, and epilogue `leave; ret`.
 - A module's top-level function returns like any Palan function; `return` statements are
   rejected there by palan-sa.
 - Symbol visibility: `_start`, each module's top-level function and `export func` functions are
   emitted with `.globl`. All other Palan
   functions are local symbols (no `.globl`), equivalent to `static` in C.

 Register allocation (PlnRegAlloc, per function, on live ranges in instruction order; a value
 defined before a loop and used in it lives until the loop's backward jump):
 - An integer or pointer value used only as a call's argument, with no call in between, takes
   that argument register; one living across a call takes a callee-saved register (rbx, r12-r15)
   while they last. The rest, and locals initialized by a literal, take stack slots.
 - A float value takes one of xmm10-xmm15 (xmm0-xmm7 carry arguments and return values, xmm8 and
   xmm9 are the emitter's scratch). Every XMM register is caller-saved, so a value living across
   a call also gets a home stack slot: it is stored there after each write and reloaded after
   each call it lives across. A value goes to a register only when that is estimated to be
   cheaper: on the stack it costs its reads and writes; in a register, those stores and reloads
   (nothing if it crosses no call). Each count is weighted by 10 for every loop around it. When
   registers run short, the values saving the most keep them.
 - A local whose address is taken stays on the stack.
 - A flo80 value always takes a 16-byte stack slot. The x87 unit computes it from memory and
   leaves nothing on its own stack between instructions, and a copy moves its 16 bytes through an
   XMM register.

 Static variables (SASpec.md) are local `.bss` slots and blocks (`.data` for a block with initial
 values), a C global the C library's own symbol. Every access goes to memory, since a call may
 change the variable.

 A string literal may hold any bytes, but JSON holds only valid UTF-8 text, so the literal's
 value travels through ast.json, sa.json and a module's interface as one escaped text
 (`src/common/PlnStrLit.h`): printable ASCII and valid UTF-8 characters as they are, `\"`, `\\`,
 `\n`, `\t`, `\r`, and every other byte as `\xHH`. That text is itself the literal's Palan
 spelling. palan-gen-ast writes it however the source spelled the bytes (an escape or the byte
 itself), and only palan-codegen turns it back into bytes.

 Palan links no crt startup objects (no crt1.o/crti.o/crtbegin.o -- the build manager
 links `ld <objs> -lc` directly), so the code generator supplies the ELF/libc glue those
 objects would otherwise provide: the `--entry` object defines `__dso_handle` (needed by
 glibc's `atexit`/`at_quick_exit`/`pthread_atfork`, which forward to `__cxa_atexit`-family
 calls taking it), and every object emits `.note.GNU-stack` (otherwise `ld` marks the
 whole binary's stack executable as soon as any note-carrying libc object joins the link).
 Linking with `ld` rather than through `cc` is the design, so the toolchain needs no C compiler: what
 the CRT objects would initialize is supplied here when a program first needs it. Only a
 position-independent executable is open (doc/Issues.md #2).


### 3.6 Allocator Generation

An owned aggregate made of more than one heap block — a struct that owns fields, an array of
structs, a `[m][n]T` array — is allocated, released and copied by generated functions instead of
inline loops. So is a struct with field initializers, which must be built wherever one comes to
exist. A struct that owns nothing and has no initializers, and a `[n]T` array of primitives, use
`malloc` and `free` directly.

palan-sa emits only calls to these functions and records each shape it needs in sa.json's
`alloc-shapes` (SASpec.md's Root, which lists the functions of each shape). The build manager
collects the shapes of all modules, deduplicates them by key (each module defining its own copy
would collide at link time), and writes the functions as Palan source `__allocators.pa` in the
entry's directory under the working directory (5.1), compiled as one more module. A struct is
declared there with only its owned fields, so a C struct or a type of another module needs no
Palan declaration of its other fields, and a struct owning nothing is only bytes to allocate,
copy and free (`__pln_B<n>`), whatever struct it is.

A key names how elements are stored, moved and passed, not their type, so elements laid out alike
share their functions: a narrow integer passes as its width and whoever reads it extends it as its
own type says; a float, passed in another register, has a key of its own. Lengths are arguments
rather than part of the key for the same reason. The arrays of any element are generated from one
description of the element (`ElemOps` in the build manager).

Field initializers are stored by a function of the module defining the struct
(`__pln_fi_<T>(@!T p)`, one `value -> p.field` each), which the generated `__pln_init_T` calls once
the owned fields are built, declaring it in C as it does a type's `->` (3.8). The generated module knows only the
owned fields, and the values go into them too (`[n]P`'s elements, `[m][n]T`'s rows): stores there
would need their layout in the generated module and a second way of writing a literal. A struct
laid out in another with initializers of its own is stored by its own function, which the outer one
calls, unless the generated functions build it whole (`runs`).

A struct with initializers is never reduced to `__pln_B<n>`, since structs of one size no longer
build alike, nor is one holding a type with its own `->` (3.8), since writing into it is no longer
copying bytes. A struct of the latter kind laid out in another is kept whole (`runs`) rather than
merged into it as other structs laid out in place are, so that copying the bytes around it leaves
the parts its `->` keeps alone. A value taken over (`<<=`, `->>`) or written by a C function is not
built again.

A module declaring `library` has allocators of its own, from its own shapes: `std.utf8` calls
`std.utf8.__allocators`. They come from its source alone, so its object is the same whichever
program or archive holds it, and they are kept with its work files, or in its library's archive,
which publishes nothing about the private types they serve. Allocators of two modules may hold
functions of one key, under different symbols; they agree, since a key names a layout and all of
them release through `free`. They are generated in `<source>.<mode>.alloc/` under the working directory
(5.1), declaring the identity through `library std.utf8;`. A library module compiled from source
into a program uses them too, in place of the program's.

### 3.7 Modules and Libraries

**Module identity.** A module's identity is the module part of its symbols (SASpec.md's Symbol
names). It comes from its source alone, never from the entry or the source's location:

| Source | Identity |
|--------|----------|
| `utf8.pa` declaring `library std;` | `std.utf8` |
| `io/file.pa` declaring `library std.io;` | `std.io.file` |
| `util.pa` declaring nothing | `util` |

PalanReference.md's Module Names states the rules. palan-gen-ast reads the declaration and records the
identity in ast.json; later tools only receive it. The file name is escaped as SASpec.md's Symbol
names says; a leading digit gets a `_` in front rather than being escaped, which `as` needs and
which reads better, so `1x.pa` and `_1x.pa` meet.

**Type identity.** A Palan struct or enum is identified by its module and its name, exported or not:
`__pln_L<len><module>_<Name>`, with the identity's `.` written `_2e` (`std.utf8`'s `Point` is
`__pln_L10std_2eutf8_Point`) and a block's type adding its order (`__pln_L4util_2_Point`). This name
is what sa.json, symbols (a type's `->` is `__pln_cp_<len><that name>`) and the allocators' keys
use; a message shows the written name. A C type keeps its C name, global as in C. So does a type
declared in a generated allocators module, whose names are already the types' identities.

- The identity itself rather than a hash of it: it reads in assembly and symbol tables, and two modules
  never meet. Escaping `.` as `_2e` cannot meet an identity's own `_2e`, which is already `_5f2e`;
  the length marks where the identity ends. The written name cannot be used, since the generated
  allocators are C and take no `.`.
- Not taken: identifying a type by its package (`library std;`), which would leave two modules of
  one package colliding.
- A library's own type names still take a prefix (`utf8mode`): imported without an alias, a type's
  name is written bare in the importer, where `mode` would collide more often.

**Finding a library.** PalanReference.md's Using a Library states the rules. A library path has no
`.`, since a module is named and not a file. A package found under two `libpath`s is an error rather
than decided by order, since the union of `libpath`s has none. `std` always comes from the tools'
own version and target (6.1). A `.pai` holds no `libpath`: the program's must find the libraries it
imports. Only the build manager sees every module, so only it looks a library up; it generates the
`.pai`'s AST and tells palan-sa where that is with `--library`. An `import "..."` is made absolute by
palan-gen-ast, relative to the importing file and with symbolic links resolved, so the build manager
and palan-sa do not each resolve it, and one file reached by two spellings is one module.

**Interface (`.pai`).** What a library publishes for one main source, in Palan source, which a
person reads to learn how to use it and palan-gen-ast reads like any source:

```
library std;
built "0.2.4";
target "x86_64-linux-gnu";
link "uv";
cinclude <uv.h> export uv_loop_t;
export type Writer { @uv_loop_t loop; };
export func open([]uint8 path) -> @!Writer;
```

A library's is written by `--lib`, and `palan-gen-ast --interface` writes one module's own
declarations in the same form (3.2), keeping every private one and each `import "..."` as an absolute
path. An ordinary `import "..."` needs none: palan-sa reads the imported module's AST, and a
library's AST comes from its `.pai`, so palan-sa has one kind of input. An error in an imported
source module is reported at its own line, and one in a library at its `.pai`'s.

A library's `.pai` declares types only of its main source, so how the library's source is divided is
never published:

- PalanReference.md's Building a Library states what is public and what a public declaration may name.
- The consts and aliases of internal modules the public declarations need are written as the main's,
  renamed (`__m<n>_<Name>`) when two share a name: they identify nothing, since a symbol holds what
  they stand for.
- A type of another library stays its own: the `.pai` keeps the `import <...>` (whether public
  declarations need it or not, since its archive is linked). A C type comes through
  `cinclude ... export`. An `import "..."` of an internal module disappears. Another main of the
  same library is imported as its users do (std's json re-exports `<std/utf8>`), so its types stay
  its own; imported as a source, it would be an internal module, whose types cannot be published.
- A struct or enum of an internal module is not written in the main's `.pai`: its reader would identify
  it by the main's module, unlike the objects. Not taken: stating each such type's module in the
  `.pai` (`export type Kept from std.kept {...}`, or a statement switching the module of the
  declarations after it), which would publish the library's internal modules.
- `link "m";` lists the libraries of every module's `link` clauses. The statement is accepted
  only in a `.pai`. An object file named by a `link` clause goes into the archive instead.
- `built "<version>";` records the tools' version, and `target "<name>";` the target. Reading a `.pai`
  of another version or target is an error, so a path holds one target's libraries, and libraries for
  several targets take a `libpath` of their own for each, which a way to name the target in it could
  simplify when a second target exists.

A `.pai` is written in the order `library`, `built`, `target`, `link`s, the imports, `cinclude`s, types,
consts and variables in source order, then the functions. There are no bodies or executable
statements, and a macro constant is written as its value. A function without a body is accepted only
in a `.pai`. A top-level variable appears only if a const uses it, as its type with the sizes filled
in (`[]int32 a = [1, 2, 3];` is `[3]int32 a;`), so changing a value leaves the interface as it was. A
const naming anything else that cannot be written, such as a variable whose value only running gives,
is left out with it: no declaration could use it either (`E_ConstUsedBeforeKnown`). An exported
`global` is written without its initializer (`export global int64 count;`).

`cinclude <stdio.h> export FILE, fpos_t;` (PalanReference.md's Exporting C Types) only puts the
`cinclude` in the `.pai`, so the clause matters only to `--lib`. palan-gen-ast rejects it on a block's
`cinclude` rather than leave a type silently unpublished. A local header's types are not exported:
installing the header beside the `.pai` would need rules for where it and the headers it includes go.

**Opaque types.** PalanReference.md's Opaque Types states the rules. A type such as
`export type Handle = opaque K.Inner;` (`export type Handle = opaque;` in a `.pai`) lets a library
publish a type defined in an internal module, or hide a type of its main, without the reader learning
its layout or its module.

- It is a type of its own, not an alias: an alias stands for its type in a function's symbol, which
  would name the internal module (`get(Handle)` would link as `get` of `K.Inner`).
- The contents are owned through one pointer, so a user holds the value as a struct without knowing
  its size.
- An owned field or element starts empty wherever it is created, so a type holding one is allocated
  the same in every module. Copying a value holding one is left out until wanted: the allocators
  that would copy it know the opaque type by its name only.
- The release function, or without one the contents' release, is reached through a function
  generated in the type's module, whose symbol is made of the type's identity alone
  (`__pln_rl_<len><identity>`), carried with the type as a type's `->` is (3.8), so the `.pai`
  needs nothing more and the user's allocators can call it. It does nothing for an empty value, so
  neither the release function nor the user checks for one.
- Not taken: rejecting a value not explicitly closed (a linear type), which the flow analysis of
  given-away variables (4) could find for a variable but not for fields and elements (doc/Issues.md
  #3); a borrowed handle only, released explicitly as C's `DIR`, which leaves the user to free.

**Module initialization.** Every module has a hidden function `<module>..init`, which no written
function's symbol can be (theirs end in a parameter list or `.v`), not written in a `.pai`. It calls
the `..init` of each module it imports, then initializes its `global` variables in declaration
order, once (a flag in the module). The entry's `_start` calls the entry's `..init`, then its
`..main` (the top-level statements, which still run only for the entry). A library's internal
modules are initialized through its main's, so a program needs to know only what it imports.

- The entry follows the rule that an initializer cannot name an ordinary top-level variable, which does
  not exist for an imported module, so a module compiles the same whichever is the entry.
- Not taken: initializing on first use (a check at every access), or an import deferring a module's
  initialization, which another import of the same module would defeat and a library's internal
  modules would not receive.

**Exported globals.** An exported `global` is used in an importer as in its own module, through
the importer's own `global` statements, so a function's access to module state is stated where the
function is. Its symbol is its label, `<module>.<name>`.

- An `import` in a function lets that function use the module's globals without a `global`
  statement: the scope is the function's, so nothing else reaches them.
- An importer's type comes from the declaration alone, with the sizes an array literal gives, so
  a source and its `.pai` give one type. A size only running gives is an error: the lengths live in
  the module's `..init`, which no importer reads.
- `M.count` reaches palan-sa as a field access on `M`; it becomes the id `M.count` as each
  statement is entered, not in palan-gen-ast, since whether `M` is an alias depends on the scope (a
  variable `M` hides it).

**Re-exporting (`export import`).** `export import <std/utf8>;` publishes what the imported module
publishes as the importing module's own, and `export import cstr, utf8 from <std/utf8>;` the names
listed, so std/json's users need not import std/utf8 too. The library's author decides it: finding a
type's functions through the type, wherever it came from, was not taken, as a reader could no
longer tell where a function comes from.

- An importer sees what is re-exported as it sees the module's own exports: bare, as `J.cstr` under an
  alias, and listable in a selective import. A declaration keeps its identity and symbols; nothing is
  copied. A chain of `export import`s passes it on.
- The exports of a module are its own and what its `export import`s give, gathered once where palan-sa
  reads an imported module, so taking its types, functions and globals sees one list. The same
  declaration reached twice (std/json and std/utf8 both imported) is one, not a clash.
- An alias is the importing file's own name, so `export import ... as U;` is an error, as is an
  `export import` in a block, where `export` cannot be written.
- A `.pai` keeps the `export import <...>`. A library's main cannot re-export one of its internal
  modules (`export import "x.pa"`): that would publish types the `.pai` cannot state; another main is
  re-exported as `<...>`.

**C-named functions.** `cexport func add(...)` gives the function the symbol `add`. There is one
symbol, not an alias beside the usual one: Palan calls it by `add` too, and a `.pai` writes
`cexport` so an importer gives it the same. It is written like `cinclude` rather than as C++'s
`extern "C"`, since there is no other linkage to name.

- A C declaration of the same name is not checked against it: a library reading its own C header,
  which declares the function for its C code, is the expected use. Two definitions fail at link
  time; one replacing a libc function does not, and is the writer's to avoid.
- A call that sees both the C declaration and the Palan function is to one function, and takes the
  Palan one's types, which carry ownership the C declaration cannot.

**Building a library.** `palan --lib -o <dir> main.pa` compiles the modules `main.pa` reaches, which
must all declare the same first name, and writes under `<dir>`, as given, the main's identity as a
path: `<dir>/std/utf8.pai` and `<dir>/std/utf8.a`. Each public module is built this way, one archive each, so a
program links only what it imports. A module two mains reach is in both archives, under one symbol,
so after changing it, every main reaching it must be rebuilt: `--lib` builds one main and does not
detect an older copy left in another's archive.
The source tree's layout does not matter to the result. `--lib` then reads the interface back
through palan-gen-ast and `palan-sa --interface`, and compares the symbols it gives the public
functions with those the main source compiled to. So an interface that lacks something, such as a C
type no `export` clause names, or that names a function differently from its object, fails here and
not at the library's first user. Nothing
is written to `<dir>` until this passes. Every module's top level remains in its object (a test
written there is linked when the object is), but runs only for an entry.

An archive's members are named by module identity (`std.utf8.o`, `std.utf8.__allocators.o`), since
modules may share a file name and every module's allocators do.

### 3.8 Type-Defined Behavior, Default and Variadic Arguments

PalanReference.md (§2, §7, §8, §9, §11) states the rules and SASpec.md how palan-sa represents them;
this section keeps why they are so.

Two rules run through all of it:

- A new value is made whole; an existing value is written by its type. Initialization, a move
  (`->>`, `>>`, `<<=`), a `T` argument and a returned value copy or move the value as a whole and
  never call a function of the type; only `x -> v` into an existing value calls the type's `->`.
  This is C++'s copy constructor versus assignment: a value keeps what it was made with (for
  `utf8`, its mode) unless written into.
- An operand is evaluated in the order it is written, also when the function takes it in another
  position (a derived comparison).

**Literal conversion.** The function named like the type is the conversion, so no syntax is
added.

- Only a value that folds to a literal converts implicitly (a literal, a const, a constant
  expression), so an allocation and a copy are never hidden behind a byte array that was not meant
  to become text. A constant expression is a literal wherever it is written, so it is folded in its
  default type first (`big n = 12 * 3;` is `big(36)`). A literal with a type of its own (an enum
  value, a character, `NULL`) does not convert. `true` and `false` convert only by a function taking
  `bool`: as `bool` widens to any number, `big(int64)` would otherwise make `true` a 1.
- The conversion is chosen among the functions callable with the literal alone as any overloaded
  call is: one taking the literal's default type exactly wins.
- The function is chosen by visibility, as an operator function is: a module may add one for a type
  it imports, and an alias does not hide it, since a conversion is never written qualified.
- A candidate needing a conversion is considered only when none fits without one, so adding a
  conversion function never changes a call that resolves without one. A call that converts may
  change, as any overloaded call may when a function is added.
- An array or dictionary literal fits only a parameter it can be made into (a struct, a sized array,
  a block the callee owns), its elements not looked at, so `json([1, 2])` does not compete with the
  `json(@[]uint8)` taking text. Ranking by the elements was not taken, as it would pick the bytes,
  nor were priorities written on functions, which would need a rule between modules adding
  conversions.
- Not for `@!T`, since writing into a temporary is lost. `as` never calls a function.

**Comparison functions.** Only `==` and `<` are defined; the other four are derived, so they agree
by construction. A three-way comparison was not taken: for text, `==` is answered by the lengths
alone when they differ, which a three-way comparison cannot use.

- Structs are not compared by address, a C struct included, nor is a field-by-field `==` given by
  default: how deep owned fields compare, whether a borrow compares the pointer, and what `NaN` is
  differ by type, so the type states them.
- A comparison with `NULL` stays a pointer comparison, since the generated allocators (3.6) and C
  interop code test borrows against it. Whether two borrows point to the same value is
  `int64(a) == int64(b)`; no operator is added for it.

**Append operator `<<`.** The function must return `@!T` of its own `T`, so `s << a << b` always
chains; that it returns `s` itself is a contract the compiler cannot check (doc/Issues.md #1). The
left operand is what a method-form receiver may be, or a call returning `@!T`: appending to a
temporary would be lost. `[+]T` gets none for now: `[+]uint8 << "abc"` would not say whether it
adds one element or the bytes.

**Copy function `->`.** It is part of the type, not an operator function. An operator function is
found among the functions visible where it is written, but `->` is also called where nothing is
written (a field of a struct copied into), and a module can hold a `T` while seeing none of `T`'s
module's functions (`T` a field of a type it imports). Choosing it by visibility would make one copy
behave differently by module, while the generated allocators share one function per shape (3.6).
Carrying every operator function with its type was not taken either: a function of two struct
operands would need a rule for which type it goes with, and an operator would reach a module by two
paths.

- So it is defined only in `T`'s module, not exported, and the StructDef carries its symbol
  (`copyFunc`) to every module `T` reaches. The symbol is named by the type's identity alone
  (`__pln_cp_24__pln_L10std_2eutf8_utf8`, SASpec.md's Symbol names) and is a global C identifier, since the generated
  allocators call it.
- The source is the same type only: a `->` from another type would be an implicit conversion under
  another name.
- palan-sa tells each copy as into a new value or an existing one (the rule above). Into an
  existing value of a type holding one, nested or as elements, it calls the generated
  `__pln_assign_K` (SASpec.md's alloc-shapes), so `p -> q` and `p.name -> q.name` agree.

**Default arguments.** A default is a constant, not an expression evaluated per call, which in a
library's interface could name what the library does not publish. palan-sa fills it in at the call,
so a function's symbol does not change and a changed default reaches callers when they are compiled
again, which a changed import already causes (5.1). When several candidates fit and one needs a
default, the call is ambiguous rather than ranked, so the reader follows no further ranking rule.
An operator function has none (its operands are fixed), nor a function pointer type, so functions
with different defaults share a pointer type.

**Variadic arguments.** `[...]E` is written as an array because the body sees one.

- The caller lays the arguments out in a temporary array in its frame and passes its address and
  its count (NULL and 0 for none). A header like a `[+]T`'s was not taken: the address and count
  serve an array spread into the call (`...a`) as well.
- A spread array follows a by-value array parameter: `...a` passes a copy, so the callee writing
  an element does not reach `a`, and `...a>>` passes `a` itself. Passing primitives without a copy
  was not taken, since the callee may write them. The callee owns the elements either way, so the
  caller frees only the array's block after the call. A spread is the only argument for the
  parameter: mixed with single ones it would need a new array even when given. Its elements must
  match exactly, as for `->`; converting them is left out while the copy could. A `[]T` has no
  count to pass.
- In the body it has the representation of a local array sized at run time (4), the count being
  its length, rather than one of its own. Struct elements are the callee's, as a `T` parameter is; the array stays the
  caller's.
- Defaults and a variadic parameter together are an error: in `f(1, 2)` for
  `f(int32 a, int32 b = 0, [...]int32 xs)`, `2` could be either. A candidate fitting through its
  variadic parameter is ambiguous with another, as for defaults.
- It is unrelated to C's `...`, and a Palan variadic function cannot be a C callback.

**Iteration with `for`.** `iter` gives an iterator and `next` advances it. An iterator type, rather
than an `int64` position, lets the state be anything (a tree's path, a file's buffer); an
index-based protocol was not taken, since indexing `utf8` by character costs a scan each time. The
iterator is a struct returned by value and `next` is chosen by it, so the iterations of unrelated
types do not compete for one `next(@!int64)`. `iter` and `next` are found as operator functions
are, an alias not hiding them, since `for` never writes their names. What the range's call was given
is held until the loop ends, not only what it returned: an error, as for a call returning a
borrow, would also reject a struct that borrows nothing.

**Hexadecimal literals.** One is the bits of the type it adopts, as hexadecimal is written for a bit
pattern and the type gives the width. A comparison types it by the other operand for the same
reason, though it does not type a decimal one.

**Character literals.** A literal is its code point. One of 128 or more adopts only an integer type
of 32 bits or more: UTF-8 text never holds it as a byte, so `buf[i] == 'é'` on a `[]uint8` would be
silently false. A character of several code points is an error suggesting a string literal; the
characters of `std/utf8` are code points.

**Hidden and read-only fields.** The boundary is the module, as for `export`. Unmarked fields stay
public, since most exported structs are plain data (`samples/raytracer/vecmath.pa`'s `vec3`).
`private` and `readonly` are not reserved words, so no existing name breaks. `readonly` covers a
borrowed pointee too: it is not part of the value, but leaving it writable would let any module
change what the type's module chose to expose read only; a module wanting it changed from outside
provides a function. A `.pai` keeps `private` fields, since the layout and the allocators need them,
and generated modules (the allocators) are exempt.

### 3.9 std/utf8

PalanStdLibReference.md states what it does; this section keeps why.

`utf8` stays an ordinary library type, built only on the features of 3.8. Positions count characters
and are reached by a scan, since that is what a text type means by a position, rather than bytes.

- No mode empties the whole text for one bad part: it is rarely wanted, and whoever wants it checks
  `hadInvalid`.
- `x -> s` takes `x` in by `s`'s mode (3.8's first rule keeps a new value's own), so no function making
  a text valid is needed (`utf8 r; s -> r;`).
- `hadInvalid` is kept rather than judged from the bytes: through a mode other than `KEEP_INVALID` no
  invalid bytes remain, so that one was replaced or deleted would be lost. A function checking the
  whole text again can be added when wanted.
- `+`, `<<`, `append` and the comparisons also take a literal or `@[]uint8` as it is, the others only
  `@utf8`: converting made `s == "abc"` about 15 times slower.
- The functions changing `s` return `@!utf8` because each was decided to chain; it is not a rule of
  the library. A character is not appended with `<<`, since `s << n` for an integer would do it.
- Left out: functions doing what a shorter form does (`isEmpty`, `clear`), `trimStart`/`trimEnd`, a
  copying or counted `replace`, case mapping and normalization (Unicode data, doc/Issues.md #4),
  conversion with numbers (a formatting module's, such as `std/fmt`), compound assignment, `[]`
  indexing.

**Grapheme clusters.**

- `graphemes` returns an iterator type for `for`'s `iter`/`next` (3.8) rather than a `[+]utf8`, so a
  long text is not held twice. Positions still count code points: making the clusters `utf8`'s
  characters (as Swift does) would scan every position for boundaries. Functions taking cluster
  positions are left until wanted.
- The properties the rules read (Grapheme_Cluster_Break, Extended_Pictographic,
  Indic_Conjunct_Break) are tables of code point ranges searched by bisection, generated from the
  Unicode Character Database of one pinned version by a script in `tools/` and committed, so a
  build needs neither the network nor the script. The script runs only to move to another
  version. The Unicode License, under which the data is redistributed, is kept in `src/std/` and
  goes into a release.
- Unicode's `GraphemeBreakTest.txt` of the same version is committed as test data, and a Palan
  test program checks every case of it; `utf8.pa`'s own tests hold a few readable ones.

### 3.10 Dictionaries

PalanReference.md states what a dictionary does, and SASpec.md the functions generated for it. It is a
built-in type rather than a library one, as `[+]T` is, because Palan has no generic types: like
`[+]T`, it is made of functions generated for each shape of its values (3.6), and follows `[+]T`'s
rules wherever a type may appear.

- The type is written `[""]` rather than with a key type, since a key is neither the `@[]uint8`
  borrow given nor a `[]uint8` the caller owns. Other key types are left until wanted; `[K:]T` was
  not taken for integer keys, as it reads like a slice.
- Creating an element on a read was not taken: a read through `@d` could not, and a mistyped key
  would pass silently. Finding the key tells whether it is there, so the check costs nothing and
  stays in a release build. The value is evaluated before its key is added, as Python does.
- In a literal, taking a bare name as the key's text, as JavaScript does, was not taken: a constant
  meant as the key would pass silently. gen-ast cannot tell `{x: 1}` for a struct from one for a
  dictionary, so it emits one braced literal and palan-sa decides by the type it is given to.

Layout: an owned slot pointing to a header that `std/rt` allocates, holding the live count, the
number of positions used, the entries `{hash, key}` in the order of addition, an index table, and
at its end the values as a `[+]T` (or `[+]$T`) header, one value at each entry's position. The index
table, of a power-of-two size, holds positions, found by the key's hash (FNV-1a) and linear probing,
and grows when three quarters full, removed positions counted. A removed element leaves its
key NULL and its table slot in place, which lookups probe past; the entries and values are compacted
when the removed outnumber the live, never during a `for` over the dictionary, which cannot remove.
Everything on keys (hashing, finding, adding, removing, compacting, copying the keys) does not
depend on `T`, so it is written once in `std/rt` rather than generated, as `__pln_dict_*` C
functions: compacting moves the values as bytes of the element's size, as the removed ones are
already released. The values' header is an ordinary `[+]T` one, so the generated functions build,
release and copy the values with `[+]T`'s.

### 3.11 Tagged Unions

PalanReference.md states what a union does; this section keeps why.

- **Why a union.** A struct holding every kind's fields allocates them all for each value. An
  interface suits kinds that are open and operations that are fixed; JSON is the reverse. C's
  untagged union cannot own anything, as nothing tells what to release. A C union stays the separate
  thing it is, its layout being C's.
- **Variants** are written as fields are, so any field type can be one; names, not types, tell them
  apart. Exported variants are all public: branching on them is what a union is for, and `opaque`
  hides one.
- **Recursion.** Only a `[+]T` or `[""]T` closes a cycle, across types too: it is empty when declared,
  so allocation ends. A cycle of owned or embedded values stays an error.
- **`j.s`** on another variant is the writer's error, checked in a debug build as `pop`'s is. Unlike
  `d[k]`'s, the check is not free, and a test guards it. Allowing it only where a test precedes was
  not taken: whether the value changed in between cannot be followed through an index or a borrow.
- **Whole assignment** changes the variant, as `x -> d[k]` adds a key. `j.s ->> v` leaves `j` its first
  variant, so no variant is left with NULL contents.
- **`is`** binds only where the test is known true. A binding moving the contents out was not taken:
  most uses read them, and `j.s ->> v` after a test moves them. In `if j is { }`, no keyword but `is`
  is taken: `match` is a name libraries want, and `switch` promises C's fall-through and `break`.
- **Making one.** Literals convert by functions named as the type (3.8), extended to bool, and to array
  and dictionary literals where a union is expected, since a struct already gives those literals a
  meaning. The author of the type, not the compiler, decides which variant `1` is. Variables do not
  convert.
- **Layout.** The tag, then space for the largest contents; an owned value is a pointer, as in a struct,
  unless written `$T`. Allocation, release and copy are functions generated per type (3.6) that branch
  on the tag and treat each variant as they treat a struct's field. Comparisons are not generated.

**Type-defined `[]`.** For structs and unions only, with one key. A type with a `[]` is indexed only
through it, also through `@T p`: falling back to the pointer's subscript where no key fits would read
past `p` silently. A type with a `->[]` stores only through it; without one, `x -> t[k]` stores into
`[](@!T, K)`'s element, as an array-like type only replaces. Choosing per call whether a `->[]` fits
was not taken: it depends on `x`, which would be analyzed twice.

### 3.12 std/json

PalanStdLibReference.md states what it does; this section keeps why.

- `json` has no accessor functions, as `is`, `j.i` and `j[k]` are them. `parseJson` returns a union of
  the value and the error rather than two values: a failure has no value to be mistaken for `null`.
- `maxDepth` bounds parsing, since parsing, copying and releasing recurse; a tree a program builds is
  not bounded.
- `\u0000` is an error: `utf8` and dictionary keys hold no NUL, and replacing it would silently change
  the text and could merge keys.
- NaN and infinity are a runtime error when written: writing `null` would lose the value silently.
- JSONTestSuite is run as test data, the `y_` cases holding `\u0000` listed as exceptions.

## 4. Expression Value Categories

palan-sa marks an expression whose value has, or needs, an owner with a `category` (SASpec.md's
Expression model); PalanReference.md and SASpec.md state the ownership rules. Why some are as they
are:

- A returned struct is written into storage its receiver provides (a variable it initializes, an
  owning parameter it is given, or the function's own return storage for `return f();`), so it is
  not copied; anywhere else it goes to a frame temp.
- `v>>` sets `v` to NULL only once all of the call's arguments have been evaluated, so a later
  argument can still borrow from it (`uv_write(req>>, s, @req.buf, ...)`, as C passes a request
  and a part of it).
- A use after a give is found in the one pass that analyzes the code, so a loop is not analyzed
  twice: a use is recorded for each loop it reaches from the head without a fill, and checked when
  the loop ends against what its iterations leave. A give takes effect when its statement ends,
  since a store may be analyzed twice (`pending-alias`) and the arguments after `v>>` may borrow
  `v`; a use later in the same statement is not found. Only variables are followed, not a field
  or element given away (doc/Issues.md #3).
- Ownership is part of a function pointer type, and only a C function named in a cast may change it,
  as C's types say nothing about it: ownership leaves or enters Palan only where the call says so.
- An array does not carry its lengths: a variable keeps those its type leaves to run time, and a
  row shares its parent's. A variable given an array takes its lengths over. A row, a field, a
  parameter or a constant size cannot, so a move there needs sizes known at compile time on both
  sides, or a cast (`a as [m]T`) saying they agree, which is not checked. The cast is allowed only
  on what is given away: read through, it would read past a shorter array.
- A copy goes as far as the shorter of the two at each level, which needs no agreement. Rows laid
  out in place whose lengths may differ have different strides, so they are copied one by one.

## 5. Working Directory and Output Files
### 5.1 Working Directory
The working directory for all command-line tools is `<work>/<target>/`, where `<work>` is
`$PALAN_WORK` if set and not empty, else `~/.palan/work` (an error if `HOME` is not set either).
`palan --clean` removes only `<work>/<target>/`, as `PALAN_WORK` may name a directory holding
other files. Work files are of two kinds:

```
~/.palan/work/<target>/
  home/user/proj/util.pa.ast.json                            a module's AST, for both modes
  home/user/proj/util.pa.<mode>.{sa.json,s,o}                a module's, reused by every entry
  home/user/proj/main.pa.<mode>.d/{__allocators.*,entry.*,out}  an entry's own
  home/user/proj/utf8.pa.<mode>.alloc/__allocators.*         a library module's allocators (3.6)
  home/user/proj/utf8.pa.<mode>.lib/{utf8.pai*,*.o}          what `--lib` builds from a main source
  home/user/libs/foo/bar.pai.ast.json                        the AST of a library's interface
```

`<mode>` is `debug` or `release` (3.1), so running a source and writing it with `-o` keep their
results apart. The AST does not depend on the mode, and is the slowest step for a source with
`cinclude`s, so it is shared. Both modes are named, since which one is the default depends on `-o`.

A module's files mirror the absolute path of its source and do not depend on the entry, so `palan
util.pa` (its top-level tests) and `palan main.pa` (importing it) share them. As with `make`, a step
is skipped if its result is newer than everything it depends on; an equal time counts as older, so
an edit right after a build is not missed:

- generating the AST (`.ast.json`): the source or `.pai`, the C headers it read (`c-headers` in ast.json), the tools;
- compiling (`.o`, with `.sa.json` present): its `.ast.json`, the `.ast.json` of every module and
  `.pai` its imports reach, the tools.

The tools are `palan-gen-ast`, `palan-c2ast`, `palan-sa`, `palan-codegen` and `predefined.h`. Any
change to an imported source compiles its importers again, a change to a function's body too. A step
that fails leaves its result older, so an error is reported on every run. The generated allocators
follow the same rule, written only when their content changed. The entry stub and `ld` always run.

Not detected, and cleared by `palan --clean`: a file whose time went back (`cp -p`), a header newly
placed earlier on the search path, a symbolic link pointed elsewhere, a switch to an older version's
tools, a module shared by programs whose `libpath`s find its `import <...>` in different places.

Files are written under a temporary name and renamed: two builds racing produce the same
content, at worst compiled twice. What differs per program, the generated allocators and the
executable, is kept per entry, so programs in one directory can run at the same time.

### 5.2 Error Output Format

All tools write error messages to **stderr** and exit with code **1** on error.

| Tool | Error format |
|------|-------------|
| palan-gen-ast | `<source_file>:<line>:<col>: error: <message>` (parse errors); `<message>` (CLI errors) |
| palan-sa | `<source_file>:<line>:<col>: error: <message>` |
| palan-codegen | `<message>` |
| palan (build-mgr) | `<message>` |
| palan-c2ast | `<source_file>:<line>:<col>: error: <message>` (preprocessor errors); `<message>` (CLI errors) |

## 6. Layout

### 6.1 One Layout
```
<root>/
  bin/palan -> ../lib/palan/<version>/tools/palan
  lib/palan/<version>/
    tools/palan, palan-gen-ast, palan-c2ast, palan-sa, palan-codegen
    <target>/c2ast/predefined.h
    <target>/std/utf8.pai, utf8.a
    <target>/std/rt.pai, rt.a         linked into every program (3.1)
    src/std/*.pa, LICENSE-Unicode.txt  for reference; not read when compiling
    uninstall
  share/doc/palan/<version>/
```
Every tool finds the others in its own directory, the symbolic link resolved, as they do now, so
versions can be installed side by side and the root can move. `bin/palan` selects one version: the
last installed, or whichever it is pointed at. `lib/palan/<version>/<target>` is also where `std` is found,
in the form a `libpath` has.
A target is named `<arch>-<os>-<abi>`; `x86_64-linux-gnu` is the only one supported now, and any
other is an error. ast.json and sa.json record it, since C declarations and the ABI
information in them (`eightbytes`) depend on it. `predefined.h` defines the architecture and OS, so
it is per target.

### 6.2 Build Tree, Installation and Release
- Build tree: `build/` has this layout with `dev` in place of `<version>`, since a tool never reads
  the name and the path stays the same when the version changes. `uninstall`, the documents and
  `src/std/` are left out, since only an installation needs them, and the testers are in `build/bin/`, since the
  layout holds only what is installed. CMake builds the tools, then builds `std` from `src/std/`
  with them, then the testers. `std-tester` runs it. CMake lists the public mains and runs `--lib`
  for each in one step, again when a tool or any source in `src/std/` changes, since `--lib` does not
  detect an older copy of a shared module (3.7). That step keeps its work files in the build tree
  through `PALAN_WORK`, not in the user's `~/.palan/work/`.
- Installation: `cmake --install` writes the layout to the prefix, or under `DESTDIR` as a package
  build does. Every destination is relative to the prefix, and `uninstall` and the documents are
  separate components, so that a package can leave them out. A test installs to a temporary
  directory and compiles and runs a program using `std` from there.
- Uninstallation: `lib/palan/<version>/uninstall`, a shell script in the layout, so it works however
  the version was installed. It removes the version's directories, and any left empty. If `bin/palan`
  points at that version, it is pointed at the newest one left, or removed if none is. Each user's
  `~/.palan/work/` stays, for `palan --clean`.
- Release: a tag `v<version>` (`v<version>-<suffix>` for a trial) starts a CI job that checks it
  against `PALAN_VERSION`, runs the tests, installs with `--strip` and packs the result as
  `palan-<version>-<target>.tar.gz`, then unpacks it elsewhere and runs the same test
  (`.github/scripts/release.sh`). The archive's top level is the layout, so it unpacks into a
  prefix with the same result as `cmake --install`. It is kept as the run's artifact; the job run
  by hand for a tag with `publish` builds it again and attaches it to a GitHub Release of that tag,
  so a release can be published at any time after the artifact has expired. The system must
  provide `as`, `ld`, C headers, and a libc and libstdc++ at least as new as the ones the release
  was built with (README.md names them).
