/// cinclude macro-constant table and reference folding pass.
///
/// @file PlnGenAstMacroFold.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once
#include <map>
#include <string>
#include <vector>
#include "../../lib/json/single_include/nlohmann/json.hpp"

using json = nlohmann::json;
using std::string;
using std::map;
using std::vector;

// One registered `#define NAME value` macro constant: its literal value and
// value-type (null when C left it untyped) as c2ast reported them, and the
// `loc` of the cinclude statement that registered it, used to decide whether
// a given reference appears textually after this registration.
// A macro c2ast could not export has no value; `unexported` names why
// ("function-like" or "not-constant") and `form` what in a function-like
// macro's body Palan has no expression for, if c2ast found one. An `undefined`
// entry records a later header's #undef, or a function-like macro palan-sa
// expands: from there on the name is nothing to fold or mark here. `end` is the
// `}` of the block holding the cinclude (null at top level): after it the
// entry is out of scope and an earlier registration applies.
struct MacroEntry {
	json value;
	json valueType;
	json loc;
	string unexported;
	string form;
	bool undefined = false;
	bool expandable = false;
	json end;
};

// Each name's entries in registration order, which is source order: each
// cinclude's c2ast starts from the previous one's macros, so a later entry
// is a redefinition (or #undef) seen from that cinclude on.
using MacroTable = map<string, vector<MacroEntry>>;

// Register `constants` (c2ast's per-cinclude array of {name, value, value-type})
// into `macros`, tagging each entry with `cincludeLoc`.
void registerMacroConstants(MacroTable& macros, const json& constants, const json& cincludeLoc);

// Register `unexported` (c2ast's {name, function-like} array) the same way, so
// a reference can be told apart from an undefined name.
void registerUnexportedMacros(MacroTable& macros, const json& unexported, const json& cincludeLoc);

// Register `undefined` (c2ast's array of names #undef'd by this header).
void registerUndefinedMacros(MacroTable& macros, const json& undefined, const json& cincludeLoc);

// Register `expandables` (c2ast's expandable definitions) as undefined entries.
void registerExpandableMacros(MacroTable& macros, const json& expandables, const json& cincludeLoc);

// The names an expandable definition from an earlier cinclude ends at, for
// palan-sa: those `cAst` (c2ast's `ast`) #undefs or defines as anything else
// that `macros` has had an expandable definition of.
json endedMacroNames(const MacroTable& macros, const json& cAst);

// Close the block from `open` to `close` (locs of its braces): the entries
// registered inside it go out of scope at `close`.
void closeMacroScope(MacroTable& macros, const json& open, const json& close);

// Recursively replace `id` expression nodes in `node` whose name matches a
// registered macro constant, and whose reference `loc` is textually after
// that macro's cinclude `loc` and not after its `end` (the last such
// registration applies), with a `lit-int` node carrying the macro's
// value, plus its value-type only when it has one. Does not descend into
// `cinclude` statement subtrees (their `functions`/`structs`/etc. are C
// declarations, not Palan references). Leaves earlier (pre-cinclude)
// references as `id` untouched. A later `id` or call naming an unexported
// macro keeps its form and gets `c-macro` set to the entry's reason, and
// `c-macro-form` to its form if it has one.
void foldMacroConstants(json& node, const MacroTable& macros);
