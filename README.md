# Palan

Palan is a programming language designed as a simpler, safer, and more enjoyable alternative to C.
It compiles to native x86-64 Linux binaries and calls C libraries directly through their headers.

```palan
cinclude <stdio.h>;

func divmod(int32 a, int32 b) -> int32 q, int32 r {
    a / b -> q;
    a % b -> r;
}

(int32 q, r) = divmod(17, 5);
printf("%d %d\n", q, r);    // 3 2
q * 2 -> q;                 // values flow left to right
printf("%d\n", q);          // 6
```

## Features

**Ownership without a garbage collector.** Arrays and structs are freed automatically at the end
of their scope. Initializing from a value copies it, `->>` moves ownership, and a function
borrows with `@` (read-only) or `@!` (writable). Using a variable after it gave its value away is
a compile error:

```palan
cinclude <stdio.h>;

type Team { [+]int32 scores; };

func total(@Team t) -> int32 s {
    for copy x in t.scores { s + x -> s }
}

Team a;
a.scores.push(70);          // method form of push(@!a.scores, 70)
a.scores.push(85);
Team b = a;                 // copies, the scores too
b.scores.push(90);
printf("%d %d\n", total(@a), total(@b));   // 155 245

Team c;
a ->> c;                    // moves: nothing is copied, and a is left empty
printf("%d\n", total(@c));  // 155
total(@a);                  // error: 'a' was given away and is NULL here
```

**C libraries as they are.** `cinclude` reads a C header and makes its functions, types, structs,
enums, macro constants and function-like macros usable from Palan; `link "m"` names a library to
link. `$` places a struct or array in the stack frame instead of the heap:

```palan
cinclude <stdio.h>;
cinclude <sys/stat.h>;

$stat st;                   // a C struct, in the stack frame
for copy path, i in args {
    if i == 0 { continue }
    if stat(path, @!st) != 0 { printf("%s: not found\n", path); continue }
    if S_ISDIR(st.st_mode) { printf("%s: directory\n", path) }    // a C macro
    else { printf("%s: %ld bytes\n", path, st.st_size) }
}
```

Other features include variable-length arrays (`[+]T` with `len`/`push`/`resize`/`pop`),
dictionaries keyed by C strings (`[""]T`, `{"a": 1}`, `for copy v, k in d`), tagged unions
(`type U union { ... }`, tested and bound with `if u is @s { }`),
function overloading, default and variadic arguments, multiple named return values, function
pointers, enums, field initializers, Unicode character literals, modules (`import`/`export`,
`export import` passing a module on, with module state in `global` variables), libraries (`palan --lib`, `import <...>`) publishing `opaque`
types, functions C calls by name (`cexport`), run-time checks left out of a release build, and raw
Linux syscalls. A library
type can be used like a built-in one: it defines operators (`+`, `==`, `<`, `<<`, `[]`), its own copy
`->`, iteration with `for`, conversion from a string literal, and fields hidden or read only outside
its module, as the standard library's `std/utf8` does:

```palan
cinclude <stdio.h>;
import <std/utf8>;

utf8 s = "h\u{E9}llo";
s << ", " << "w\u{F6}rld";
for copy c in s { printf("U+%04X ", c) }
if s -> startsWith("h\u{E9}", "H\u{E9}") { printf("\n%s\n", s -> cstr()) }
```
See the [Palan Language Reference](doc/PalanReference.md) for the whole language, and the
[Palan Standard Library Reference](doc/PalanStdLibReference.md) for `std` (`std/utf8`, `std/json`).

## Quick Start

Build the toolchain (see [Building](#building)), then write `hello.pa`:

```palan
cinclude <stdio.h>;
printf("Hello World!\n");
```

```bash
build/bin/palan hello.pa
# Hello World!
```

Without `-o`, `palan` builds the binary among its work files and runs it right after linking;
arguments after the source file go to the program (`build/bin/palan hello.pa a b`). To write the
binary to the current directory instead:

```bash
build/bin/palan -o hello hello.pa
./hello
```

## Samples

- [samples/tetris.pa](samples/tetris.pa) — Tetris in the terminal with ncurses (needs
  `libncurses-dev`). Arrow keys move and drop, space rotates, `q` quits.

  ```bash
  build/bin/palan samples/tetris.pa
  ```

- [samples/raytracer/](samples/raytracer/) — a port of
  [tinyraytracer](https://github.com/ssloy/tinyraytracer) that writes `ray.jpg` with libjpeg
  (needs `libjpeg-dev`).

  ```bash
  cd samples/raytracer && ../../build/bin/palan raytracer.pa
  ```

## Building

**Dependencies:** g++ (C++20), CMake 3.16+, Flex, Bison 3.8+, libboost-dev, libfl-dev, lcov

```bash
make            # build (build/, -O2 -g) and run all tests
make debug      # unoptimized build for gdb (build-debug/)
make clean      # remove build/ and build-debug/
make coverage   # coverage report (build-cov/lcov.info)
```

`build/bin/palan` runs the toolchain; the tools it uses are in `build/lib/palan/dev/tools/`.

To install, with `--strip` leaving out the debug information:

```bash
cmake --install build --prefix ~/.local --strip
```

`bin/palan` then points at the version installed last, and versions can stay side by side. To remove
one, run `lib/palan/<version>/uninstall` under the prefix; `bin/palan` moves to the newest one left.

## Installing a Release

A tag `v<version>` makes CI build `palan-<version>-x86_64-linux-gnu.tar.gz`, kept as an artifact
of the `release` workflow's run (and attached to the GitHub Release when one is published). Its top
level is the installed layout, so it unpacks into a prefix as `cmake --install` would:

```bash
tar -xzf palan-0.2.8-x86_64-linux-gnu.tar.gz -C ~/.local
~/.local/bin/palan hello.pa
```

It needs `as` and `ld` (binutils), the C headers (libc6-dev), glibc 2.38 or newer and the
libstdc++ of GCC 13.2 or newer, as on Ubuntu 24.04. `lib/palan/<version>/uninstall` removes it,
as for a version built and installed from the sources.

## Toolchain

Palan compiles through a pipeline of independent tools that pass JSON between them:

| Tool | Input | Output | Description |
|---|---|---|---|
| `palan-gen-ast` | `.pa` source | `.ast.json` | Parse Palan source into an AST |
| `palan-c2ast` | C header | AST JSON | Translate a C header (run by `palan-gen-ast` for each `cinclude`) |
| `palan-sa` | `.ast.json` | `.sa.json` | Semantic analysis |
| `palan-codegen` | `.sa.json` | `.s` assembly | x86-64 code generation |
| `palan` | `.pa` source | executable | Build manager (runs the tools above, `as` and `ld`) |

## Documentation

- [doc/PalanReference.md](doc/PalanReference.md) — language reference
- [doc/PalanStdLibReference.md](doc/PalanStdLibReference.md) — standard library reference
- [doc/SpecAndDesign.md](doc/SpecAndDesign.md) — toolchain design
- [doc/ASTSpec.md](doc/ASTSpec.md), [doc/SASpec.md](doc/SASpec.md) — JSON formats between the tools
- [doc/Issues.md](doc/Issues.md) — known limitations and open issues

## License

MIT License — see [LICENSE](LICENSE). The bundled [nlohmann/json](lib/json) is distributed under its own MIT license (`lib/json/LICENSE.MIT`). The grapheme property table and the conformance test data made from the Unicode Character Database (`src/std/graphemeprops.pa`, `test/testdata/std/GraphemeBreakTest.txt`) are under the [Unicode License v3](src/std/LICENSE-Unicode.txt). The JSONTestSuite cases in `test/testdata/std/JSONTestSuite` are under their own MIT license (`test/testdata/std/JSONTestSuite/LICENSE`).
