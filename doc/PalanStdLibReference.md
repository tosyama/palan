# Palan Standard Library Reference

**Version:** v0.2.8

The standard library is the library `std`, found with the compiler: `import <std/utf8>;` needs no
`libpath` (see [Using a Library](PalanReference.md#using-a-library) in the
[Palan Language Reference](PalanReference.md)). Its modules are ordinary Palan; nothing in it is
known to the compiler.

---

## Modules

- [std/utf8](#stdutf8) — UTF-8 text
- [std/json](#stdjson) — JSON values, parsed and written

---

## std/utf8

```palan
cinclude <stdio.h>;
import <std/utf8>;

utf8 s = "h\u{E9}llo";
s << ", " << "w\u{F6}rld";
s.appendChar('!');
printf("%s %ld %ld\n", s -> cstr(), s -> byteLen(), s -> charCount());   // héllo, wörld! 15 13

for copy c, i in s { printf("%ld:U+%04X ", i, c); }    // -1 for an invalid part

[+]utf8 words = s -> split(", ");
utf8 t = words -> join(" & ");                // héllo & wörld!
if t -> startsWith("h\u{E9}", "H\u{E9}") && t != s { t.replace("&", "+").trim(); }
```

`utf8` is UTF-8 text held with a NUL at the end, so `cstr` gives it to C as it is. A declared
`utf8` is empty. A string literal converts to one wherever a `utf8` is expected
(`utf8 s = "abc";`, `"" -> s;`, `s == "abc"`); other bytes are converted by calling `utf8`.
Bytes given as a literal or a `@[]uint8` are read up to their first NUL, so they must end with one:
`"ab\0cd"` is the text `"ab"`. A `utf8` never holds a NUL.

A character is a Unicode code point: `"🇯🇵"` has 2, though one grapheme cluster, what a reader
sees as one character (see [Grapheme clusters](#grapheme-clusters)). Positions (`find`, `slice`, `insert`,
`remove`) count characters, an invalid part counting as one, so reaching one scans the text from
the start (O(n)). A position out of range is brought within it: a negative one is 0, beyond the
end is the end, and a start at or after the end gives nothing.

### Modes

`mode` says what a `utf8` does with bytes that are not valid UTF-8 when text is made or added to
it:

| `utf8mode` | Invalid part |
|------------|--------------|
| `KEEP_INVALID` | kept as it is |
| `REPLACE_INVALID` (the default) | replaced with U+FFFD |
| `DELETE_INVALID` | dropped |

An invalid part is the longest start of a valid sequence, or else one byte, as Unicode
recommends: `"A\xE2\x82B"` has one, which `REPLACE_INVALID` makes `"A\u{FFFD}B"`. Only a
`KEEP_INVALID` text holds invalid bytes. Adding to text takes the added bytes in one call at a
time: a sequence cut at the end of one `<<` is an invalid part even if the next completes it.

`hadInvalid` tells that an invalid part was met in making the text, or in making the texts it was
made from: adding `t` to `s` sets it if `t`'s is set. It is never cleared by adding; `x -> s`
replaces it with `x`'s. A part made by `slice` or `split` has `s`'s, whether it holds an invalid part
or not.

| Text | Mode |
|------|------|
| `utf8 s;`, `utf8 s = "abc";` | `REPLACE_INVALID` |
| `utf8 s = t;`, a move, an argument, a returned value | `t`'s, with `hadInvalid` (the value is copied whole) |
| `t -> s` | `s`'s: `t`'s text is taken in by it. So `utf8 r; s -> r;` is a copy of `s` with each invalid part replaced |
| `s << x`, and every function changing `s` | `s`'s |
| `a + b`, `join` | the loosest of the `utf8` operands' (`KEEP_INVALID`, then `REPLACE_INVALID`, then `DELETE_INVALID`); a string literal or `@[]uint8` operand has none and is taken in by it |
| `slice`, `split` | `s`'s |

A literal given where a function takes `@utf8` (`find`, `replace` and the others) is converted
in `REPLACE_INVALID` mode, so `k -> find("\xFF")` does not find those bytes in a `KEEP_INVALID`
`k`; write `utf8("\xFF", utf8mode.KEEP_INVALID)` to find them. A comparison compares a literal
or `@[]uint8` operand as its bytes.

`s.hadInvalid` and `s.mode` can be read; the bytes are reached only through `cstr`.

### Functions

| Function | Meaning |
|----------|---------|
| `utf8(@[]uint8 cs, utf8mode m = utf8mode.REPLACE_INVALID) -> utf8` | the NUL-terminated bytes `cs`, made by the rules of `m` |
| `cstr(@utf8 s) -> @[]uint8` | the NUL-terminated bytes, until `s` is changed |
| `byteLen(@utf8 s) -> int64` | the number of bytes, without the NUL |
| `charCount(@utf8 s) -> int64` | the number of characters |
| `for copy c, i in s` | each character `c` as an `int32` code point, `-1` for an invalid part; `i` counts the characters from 0 |
| `graphemes(@utf8 s) -> utf8graphemes` | the grapheme clusters of `s`, for `for` (see [Grapheme clusters](#grapheme-clusters)) |
| `graphemeCount(@utf8 s) -> int64` | the number of grapheme clusters |
| `a == b`, `a < b` (and `!=`, `>`, `<=`, `>=`) | by bytes, which is code point order; either one may be a literal or `@[]uint8` |
| `a + b` | `a` then `b`, either one a literal or `@[]uint8` |
| `s << x` | adds `x` (`@utf8`, a literal or `@[]uint8`) and gives `s` back: `s << a << b` |
| `append(@!utf8 s, x) -> @!utf8` | `s << x` |
| `appendChar(@!utf8 s, int32 c) -> @!utf8` | adds the code point `c`; 0, a surrogate, a value beyond U+10FFFF or a negative one is an invalid part, which `KEEP_INVALID` has no bytes to keep for, so it drops it |
| `startsWith(@utf8 s, ...)`, `endsWith`, `contains` | whether `s` starts with, ends with or contains any of the texts given (`s -> startsWith("http:", "https:")`); none given is false, `""` is true |
| `find(@utf8 s, @utf8 x, int64 from = 0) -> int64` | the position of the first `x` at or after `from`, or `-1`; `""` is found at `from`, and nothing is found from beyond the end |
| `replace(@!utf8 s, @utf8 from, @utf8 to) -> @!utf8` | replaces every `from`, from the left and without overlapping, not searching what replaced it (`"aaaa"` with `"aa"` to `"a"` gives `"aa"`); an empty `from` changes nothing |
| `insert(@!utf8 s, int64 pos, @utf8 x) -> @!utf8` | inserts `x` before the character at `pos` |
| `remove(@!utf8 s, int64 start, int64 end) -> @!utf8` | removes the characters from `start` up to `end` |
| `trim(@!utf8 s) -> @!utf8` | removes the 25 characters of Unicode's White_Space at both ends |
| `slice(@utf8 s, int64 start, int64 end) -> utf8` | the characters from `start` up to `end` |
| `split(@utf8 s, @utf8 sep) -> [+]utf8` | the parts between each `sep`, empty ones included (`"a,,b"` gives `"a"`, `""`, `"b"`); an empty `sep` gives each character (none for an empty text), and an empty text with a non-empty `sep` one empty part |
| `join(@[+]utf8 parts, @utf8 sep) -> utf8` | the parts with `sep` between them (`parts -> join(", ")`); no parts give empty text |

The functions changing `s` return it, so they chain: `s.replace("\t", " ").trim();`. `s -> s`
and `s << s` work.

To clear a text write `"" -> s;`; to test for empty text, `s -> byteLen() == 0` or `s == ""`.

### Grapheme clusters

```palan
utf8 s = "e\u{301}\u{1F1EF}\u{1F1F5}!";
for copy g, i in s -> graphemes() { printf("%ld:%s ", i, g -> cstr()); }   // 0:é 1:🇯🇵 2:!
printf("%ld %ld\n", s -> charCount(), s -> graphemeCount());               // 5 3
```

The clusters are the extended grapheme clusters of UAX #29 in Unicode 18.0.0. Each `g` is a `utf8` of
its own, with `s`'s mode and `hadInvalid` as `slice` gives; `i` counts the clusters from 0. An invalid
part is a cluster of its own. `graphemes` borrows `s`, so what it returns must not outlive `s` or be
used after `s` is changed. Positions (`find`, `slice` and the others) count code points, not clusters.

---

## std/json

```palan
cinclude <stdio.h>;
import <std/json>;
import <std/utf8>;

jsonresult r = parseJson("{\"name\": \"palan\", \"tags\": [1, 2.5]}");
if r is @err = error { printf("%s at %ld:%ld\n", err.message -> cstr(), err.line, err.col); }
json j;
if r is value { r.value ->> j; }

if j["name"] is @s { printf("%s\n", s -> cstr()); }     // palan
printf("%g\n", num(@j["tags"][1]));                       // 2.5
true -> j["ok"];
[json.Null, {"k": "v"}] -> j["more"];
"lang" -> j["tags"][0];
printf("%s\n", toUtf8(@j) -> cstr());
// {"name":"palan","tags":["lang",2.5],"ok":true,"more":[null,{"k":"v"}]}
```

`json` is a [union](PalanReference.md#tagged-unions) of a JSON value's kinds. A declared `json` is `Null`.

```palan
export type json union { Null; bool b; int64 i; flo64 f; utf8 s; [+]json items; [""]json members; };
```

It is read with `is` and `j.i` as any union is (`if j is copy i { }`, `if j is { ... }`), and indexed:
`j["k"]` is a value of `members`, `j[n]` of `items` (a borrow of it, as an array element is). The key
is what a dictionary's is.

| `j[k]` | Does |
|--------|------|
| read, `@!j[k]`, `x -> j[k].i` | the value; another kind, a missing key or a position out of range stops the program |
| `x -> j["k"]` (and `->>`) | adds `k` or replaces its value in `members`; a `Null` becomes `members` first |
| `x -> j[n]` (and `->>`) | replaces an element of `items`; it does not add one, which `j.items.push(x)` does |

The checks are made in a release build too. As a dictionary's, a borrow of a value (`@!j["k"]`)
must not be kept across adding a key.

A literal converts to a `json` wherever one is expected: `true`, a number, a string, an array or
dictionary literal (`json j = [1, "a", {"b": false}];`), its elements converted the same way.
`json.Null` is null. A variable converts by calling `json` (`json(n)`), or by naming the kind
(`json.i(n)`, `json.items(v>>)`).

| Function | Meaning |
|----------|---------|
| `json(x)` | `x` as a `json`: `bool`, `int64`, `flo64`, `@[]uint8` and `@utf8` copied, `[+]json` and `[""]json` moved in |
| `parseJson(@utf8 s, int64 maxDepth = 512) -> jsonresult` | `s` parsed, or why it is not JSON |
| `toUtf8(@json j, int32 indent = 0) -> utf8` | `j` as JSON text: on one line, or with each element on its own line indented by `indent` spaces |
| `s << j` | adds `toUtf8(j)` to the `utf8` `s` and gives `s` back |
| `num(@json j) -> flo64` | an `i` or an `f` as a `flo64`; another kind stops the program |
| `p == q`, `p != q` | the same kind and contents; an `i` and an `f` are equal when they are the same number exactly, `members` in any order |

```palan
export type jsonresult union { json value; jsonerror error; };
export type jsonerror { utf8 message; int64 pos; int64 line; int64 col; };
```

`pos` counts characters from 0, `line` and `col` from 1.

### Parsing

The text is JSON as RFC 8259 defines it, with nothing added but a leading BOM, which is skipped.

- A number without a fraction or exponent that fits `int64` is an `i`; any other number, and `-0`,
  an `f`. A number beyond `flo64`'s range is an error.
- `\u0000`, a surrogate without its pair, and bytes that are not UTF-8 are errors.
- A repeated key takes the last value, kept at the first's place.
- Arrays and objects nested deeper than `maxDepth` are an error.

### Writing

Keys are written in the order they were added, and a string escapes only `"`, `\` and the control
characters; bytes that are not UTF-8 are written as U+FFFD. An `f` is written in the fewest digits
that read back as the same value, as JavaScript writes numbers, with `.0` when it is whole
(`100.0`, `1e+21`, `1.5e-7`). An `f` that is NaN or infinite stops the program. Neither parsing nor
writing depends on the C locale.
