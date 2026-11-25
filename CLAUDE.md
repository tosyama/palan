# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Development Principles

- When a gap or bug surfaces mid-implementation, fix it at the layer where it structurally belongs, not at the layer where it happened to surface. If two subsystems feed the same internal structure through different paths (e.g. native syntax vs `cinclude`d C declarations), normalize at the point where each path registers/ingests into that structure, so downstream consumers only ever see one canonical shape. Don't add a second case to a shared consumer (type resolution, codegen, etc.) just to tolerate an un-normalized input — that's a symptom of missing normalization upstream, not a fix.
- Recognize the shapes a shortcut takes. A change is probably routing around the problem rather than fixing it if it: adds a case to a shared consumer so it tolerates an input shape that should never have reached it; adds an entry to a whitelist instead of asking why the whitelist exists; adds a later pass that re-normalizes what an earlier pass left un-normalized (a second normalization point, with pass ordering deciding which one wins); or threads a flag through so one caller can skip a path. Any of these can be the right call — but each needs a reason beyond "it makes the test pass", and the reason belongs in the ticket.
- Prefer the design that keeps exactly one internal representation for a given concept over one that lets two equivalent representations coexist and pushes the burden of reconciling them onto every consumer. This is about representations of the *same* concept: distinctions that carry meaning — a read-only versus a mutable pointer, owned versus borrowed — are different concepts, and collapsing them into one representation drops an invariant instead of unifying anything.
- If the properly-scoped fix looks bigger than the ticket at hand, say so and propose it rather than silently taking the smaller patch that only routes around the symptom. Record the judgment either way, including a deliberate decision to take the smaller patch.
- Don't restate the same fact across a plan, ticket, or doc section. In source, comment only a WHY that isn't derivable from the code (a hidden constraint, a non-obvious workaround, a subtle invariant) — never comment WHAT the code does.

## Build Commands

```bash
# Build all targets and run all tests (build/, -O2 -g with assertions enabled)
make

# Unoptimized build for debugging with gdb (build-debug/, build only)
make debug

# Clean build artifacts (build/ and build-debug/)
make clean

# Generate coverage report (lcov.info in build-cov/)
make coverage           # full — all tests (incremental: build-cov/ persists)
make coverage-codegen   # codegen only (faster)
make coverage-sa        # semantic analyzer only (faster)
make coverage-reset     # full clean rebuild of build-cov/
```

Coverage targets intentionally run only the end-to-end testers, not `sa-unit-tester`/`codegen-unit-tester` — the goal is measuring what's reachable through the real binaries and finding dead code; unit tests would exercise paths a real binary never reaches. `make` (no coverage) runs all eight testers. `build/` is optimized because the end-to-end testers spend most of their time running the tools themselves (an unoptimized build makes the test run about 3x slower); an existing `build/` configured as Debug needs `make clean` first.

**Dependencies:** g++ (C++20), CMake 3.16+, Flex, Bison 3.8+, libboost-dev, libfl-dev, lcov

### Running Individual Test Suites

```bash
cd build && bin/c2ast-tester          # C header → AST parser tests
cd build && bin/gen-ast-tester        # Palan parser tests
cd build && bin/sa-tester             # Semantic analyzer tests
cd build && bin/sa-unit-tester        # Semantic analyzer unit tests
cd build && bin/codegen-tester        # Code generator tests
cd build && bin/codegen-unit-tester   # Code generator unit tests
cd build && bin/build-mgr-tester      # Build manager integration tests
cd build && bin/std-tester            # Standard library tests (std is built into build/ by CMake)
```

### Running a Single Test

```bash
cd build && bin/sa-tester --gtest_filter=sa.helloworld_sa
```

### Running Individual Tools

```bash
cd build && lib/palan/dev/tools/palan-c2ast -s stdio.h         # Parse system header to AST
cd build && lib/palan/dev/tools/palan-gen-ast ../test/testdata/gen-ast/001_basicPattern.pa
cd build && bin/palan ../test/testdata/build-mgr/001_helloworld.pa
```

`build/` has the installation layout (SpecAndDesign §6) with `dev` as the version: `bin/palan` is a symbolic link into `lib/palan/dev/tools/`. Testers are in `bin/` and name the internal tools bare; `test/test-base/testBase.cpp` puts them on `PATH`.

## Architecture

Palan is a multi-stage compiler where independent CLI tools communicate via JSON AST files.

### Compilation Pipeline

```
source.pa
  ↓ palan-gen-ast   (PlnLexer.ll + PlnParser.yy → ast.json)
  ↓ palan-c2ast     (invoked internally for each cinclude; C header → AST nodes embedded in ast.json)
  ↓ palan-sa        (ast.json → sa.json; scope resolution, C function annotation, str-literal collection)
  ↓ palan-codegen   (sa.json → .s; VCodeGen → VProg IR → PlnX86CodeGen → AT&T assembly)
  ↓ as              (.s → .o)
  ↓ ld              (.o → a.out)
```

`palan` (build manager) orchestrates the full pipeline. Intermediate files are stored under `~/.palan/work/<absolute-source-path>/`.

### Key Design Points

**codegen IR layer** (`src/codegen/`): The code generator uses a virtual register + virtual instruction set (VProg) as an intermediate representation between the AST and x86-64 assembly. This allows future register allocation strategies to be plugged in via `PlnRegAlloc`.

- `PlnVCodeGen`: AST (Module) → VProg
- `PlnRegAlloc`: VProg → RegMap (VReg → physical register)
- `PlnX86CodeGen`: VProg + RegMap → AT&T assembly

**cinclude scoping**: C function declarations from `cinclude` are scope-aware in palan-sa. Functions are visible from the `cinclude` point to the end of the enclosing scope. `cinclude` nodes are consumed and not emitted to sa.json.

**str-literals**: String literals are collected by palan-sa (not codegen) into a `str-literals` table in sa.json. Codegen uses this table to emit `.rodata` entries.

### File Naming Convention

- Palan modules: `Pln<Name>.h/cpp` (e.g., `PlnSemanticAnalyzer`, `PlnX86CodeGen`)
- C2ast modules: `C<Name>.h/cpp` (e.g., `CLexer`, `CParser`, `CPreprocessor`)
- Entry points: `main.cpp` in each tool's directory

### Source Layout

```
src/
├── build-mgr/       palan — build manager (orchestrates pipeline)
│   └── uninstall.sh     Installed as lib/palan/<version>/uninstall
├── c2ast/           palan-c2ast — C header parser
│   ├── CLexer.*         Hand-written C lexer
│   ├── CParser.*        Recursive descent C parser
│   ├── CPreprocessor.*  Macro expansion and #include handling
│   ├── CToken.*         Token definitions
│   └── predefined.h     Built-in macro definitions (copied to <target>/c2ast/ in the layout)
├── gen-ast/         palan-gen-ast — Palan parser
│   ├── PlnLexer.ll      Flex lexer rules
│   ├── PlnParser.yy     Bison grammar rules
│   └── PlnGenAstC2Ast.* Invokes palan-c2ast for each cinclude
├── semantic-anlyzr/ palan-sa — semantic analyzer
│   ├── PlnSemanticAnalyzer.*
│   ├── PlnSaDecl.cpp
│   ├── PlnSaStmt.cpp
│   ├── PlnSaExpr.cpp
│   ├── PlnSaCall.cpp    Calls, callee lookup, method-form and chained calls
│   ├── PlnSaStorage.cpp Storage types of structs and arrays; their allocation, release and copy
│   ├── PlnSaMessage.*
│   ├── PlnSaInternal.h
│   └── PlnType.*
├── codegen/         palan-codegen — x86-64 code generator
│   ├── PlnNode.h        AST node types (Module, Stmt, Expr variants)
│   ├── PlnVProg.h       VReg / VInstr / VProg IR definitions
│   ├── PlnVCodeGen.*    AST → VProg (functions, statements, control flow)
│   ├── PlnVCodeGenExpr.cpp  Expressions, array elements, bit-fields
│   ├── PlnVCodeGenCall.cpp  Calls, arguments, struct values by value, returns
│   ├── PlnRegAlloc.*    VProg → RegMap (register allocator plug-in point)
│   ├── PlnX86CodeGen.*  VProg + RegMap → AT&T assembly (dispatch)
│   ├── PlnX86Arith.cpp  Arithmetic instruction emission
│   ├── PlnX86Call.cpp   Call instruction emission (C calls, Palan calls, syscalls)
│   ├── PlnX86Mem.cpp    Memory/address instruction emission
│   ├── PlnDeserialize.* sa.json → Module
│   └── PlnCodeGen.h     Abstract base class
├── common/          Shared utilities (PlnProcess — argv-based child process spawning; PlnInterface — writes a module's interface; PlnStrLit — the text a string literal's value travels as)
└── std/             The standard library, in Palan (CMakeLists.txt lists its public mains)
tools/
└── gen-grapheme-data.py  Generates std's grapheme property table and its conformance test data from the Unicode data
test/
├── testdata/        Test input files (numbered sequentially per tool)
├── test-base/       Shared test helpers (execTestCommand, cleanTestEnv)
└── *-tester/        Eight gtest binaries, one per suite listed above
doc/
├── SpecAndDesign.md  Language spec and toolchain design
├── ASTSpec.md        JSON AST format specification (palan-gen-ast output)
├── SASpec.md         SA JSON format specification (palan-sa output)
├── PalanReference.md Palan language reference manual
├── PalanStdLibReference.md Palan standard library reference
├── Issues.md         Open issues and topics for future consideration
localtickets/        Local task management (Markdown, not tracked by git)
```

## Testing

- Framework: Google Test (fetched via CMake FetchContent, v1.12.0)
- Generated files (`PlnLexer.cpp`, `PlnParser.cpp/h`) are excluded from coverage
- `execTestCommand` returns stdout + `:` + stderr; empty string means success
- Test data files are numbered sequentially (e.g., `001_helloworld.pa`)
