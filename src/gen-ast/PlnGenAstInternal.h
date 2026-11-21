/// Inline helpers shared across PlnParser.yy grammar actions.
///
/// @file PlnGenAstInternal.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once
#include <cctype>
#include <filesystem>
#include <vector>
#include "../../lib/json/single_include/nlohmann/json.hpp"

using json = nlohmann::json;
using std::string;

inline bool isInterfaceFile(const string& path)
{
	return path.ends_with(".pai");
}

// A module's identity comes from the source alone, so a symbol is the same
// wherever the source is checked out and whichever module is the entry. An
// escape is `_` and two lowercase hex digits, so a `_` that looks like one is
// escaped too; a leading digit would make `as` read the symbol as a number.
inline string moduleIdentity(const string& library, const string& sourcePath)
{
	string stem = std::filesystem::path(sourcePath).stem().string();
	auto isHex = [](unsigned char c) { return isdigit(c) || (c >= 'a' && c <= 'f'); };
	static const char hex[] = "0123456789abcdef";
	string out = library.empty() ? "" : library + ".";
	if (isdigit((unsigned char)stem[0])) out += '_';
	for (size_t i = 0; i < stem.size(); i++) {
		unsigned char c = stem[i];
		bool looksEscaped = c == '_' && i + 2 < stem.size() && isHex(stem[i + 1]) && isHex(stem[i + 2]);
		if (isalnum(c) || (c == '_' && !looksEscaped)) { out += c; continue; }
		out += '_';
		out += hex[c >> 4];
		out += hex[c & 15];
	}
	return out;
} // LCOV_EXCL_BR_LINE

// Convert a `kind`-tagged store_loc intermediate node into an `expr-type`-tagged
// expression node, recursively (needed since a store_loc base can itself be a
// field access, e.g. `s.f[0]`).
inline json storeLocToExpr(const json& loc)
{
	string kind = loc.value("kind", "");
	if (kind == "call") return loc["call"];
	if (kind == "expr") return loc["expr"];
	json e;
	if (kind == "var") {
		e = {{"expr-type", "id"}, {"name", loc["name"]}}; // LCOV_EXCL_BR_LINE
	} else if (kind == "arr-index") {
		e = {{"expr-type", "arr-index"},
		     {"array", loc["array"]}, {"index", loc["index"]}}; // LCOV_EXCL_BR_LINE
	} else {
		e = {{"expr-type", "field-access"},
		     {"object", storeLocToExpr(loc["base"])}, {"field", loc["field"]}}; // LCOV_EXCL_BR_LINE
	}
	if (loc.contains("loc")) e["loc"] = loc["loc"];
	return e;
} // LCOV_EXCL_BR_LINE

inline bool isStoreStmt(const json& e)
{
	return e.contains("stmt-type");
}

// `value -> loc` (`->>` when transfer) as its statement. In a chain the value
// is itself a store statement, whose target SA reads back.
inline json storeStmt(json loc, json value, bool transfer)
{
	string kind = loc["kind"];
	json s;
	if (kind == "var") {
		s = {{"stmt-type", "assign"}, {"name", move(loc["name"])}}; // LCOV_EXCL_BR_LINE
	} else if (kind == "arr-index") {
		json target = {{"expr-type", "arr-index"}, {"array", move(loc["array"])}, {"index", move(loc["index"])}}; // LCOV_EXCL_BR_LINE
		target["loc"] = loc["loc"];
		s = {{"stmt-type", "arr-assign"}, {"target", move(target)}}; // LCOV_EXCL_BR_LINE
	} else if (kind == "field") {
		s = {{"stmt-type", "field-assign"}, {"object", storeLocToExpr(loc["base"])}, {"field", move(loc["field"])}}; // LCOV_EXCL_BR_LINE
	} else {
		return {{"expr-type", "not-impl"}}; // LCOV_EXCL_BR_LINE
	}
	s["value"] = move(value);
	if (transfer) s["ownership-transfer"] = true;
	return s;
} // LCOV_EXCL_BR_LINE

// Build a pointer type node. `mut` distinguishes @T (false) from @!T (true).
inline json pntrTypeExpr(json base, bool mut)
{
	return {{"type-kind","pntr"},{"mutable",mut},{"base-type",std::move(base)}}; // LCOV_EXCL_BR_LINE
} // LCOV_EXCL_BR_LINE

// The base-type node for @void/@!void -- C's void*, spelled directly instead
// of via a primitive pointee like @!int8 (pntr(void) is bidirectionally
// compatible with any pntr(T), see PlnType.cpp typeCompat).
inline json voidTypeExpr()
{
	return json{{"type-kind","prim"},{"type-name","void"}}; // LCOV_EXCL_BR_LINE
} // LCOV_EXCL_BR_LINE

// The pointee of @func(...): the same shape c2ast gives a C function type, so
// a native function pointer and a cinclude'd one reach SA identically.
inline json funcTypeExpr(std::vector<json> params, json ret)
{
	return {{"type-kind","func"},{"parameters",std::move(params)},{"ret-type",std::move(ret)}}; // LCOV_EXCL_BR_LINE
} // LCOV_EXCL_BR_LINE

// `_` in a function type keeps the type a cast's source has at that place.
inline json fptrSigType(json t)
{
	if (t.value("type-kind","") == "prim" && t.value("type-name","") == "_")
		return {{"type-kind","keep"}}; // LCOV_EXCL_BR_LINE
	return t;
} // LCOV_EXCL_BR_LINE

// Whether a type_expr result has no array level other than a plain '[n]'/'[]'
// one ('[+n]' is not implemented).
inline bool isImplementedTypeExpr(const json& t)
{
	if (t.value("type-kind", "") == "arr" && t.value("specifier", "") != "raw")
		return false;
	return !t.contains("base-type") || isImplementedTypeExpr(t["base-type"]);
}

// `[+]$T` keeps its elements in place, marked like `[n]$T`.
inline json varrTypeExpr(json sizeExpr, json base)
{
	json t = {{"type-kind","arr"},{"specifier","variable"},{"size-expr",std::move(sizeExpr)}}; // LCOV_EXCL_BR_LINE
	if (base.value("type-kind", "") == "embed") {
		t["embedded"] = true;
		t["base-type"] = std::move(base["base-type"]);
	} else {
		t["base-type"] = std::move(base);
	}
	return t;
} // LCOV_EXCL_BR_LINE

// A variable-length array.
inline bool isVarrType(const json& t)
{
	return t.value("type-kind", "") == "arr" && t.value("specifier", "") == "variable";
}

// Whether a type_expr result is one gen-ast currently knows how to carry as
// a declared variable's type (a bare, uninitialized var_declaration). Shared
// by var_declaration and return_def (ARROW type_expr) so a bare-return-type
// function stays representable in exactly the cases a same-typed local
// variable already is -- a named return (ARROW var_declarations) gets this
// for free by going through var_declaration itself.
inline bool isDeclarableVarType(const json& t)
{
	string tk = t.value("type-kind", "");
	if (tk == "prim" || tk == "pntr")
		return true;
	if (isVarrType(t))
		return t["base-type"].value("type-kind", "") == "prim" || isDeclarableVarType(t["base-type"]);
	if (tk != "arr" || t.value("specifier", "") != "raw")
		return false;
	if (t["size-expr"].is_null())
		return true;  // unsized raw array
	const json& bt = t["base-type"];
	string btk = bt.value("type-kind", "");
	if (btk == "prim")
		return true;  // fixed-size array of prim
	if (isVarrType(bt))
		return isDeclarableVarType(bt);  // fixed-size array of [+]T
	if (btk == "pntr" && bt.contains("base-type")) {
		const json& ibt = bt["base-type"];
		if (ibt.value("type-kind", "") == "arr" && ibt.value("specifier", "") == "raw"
			&& ibt.contains("size-expr") && ibt["size-expr"].is_null())
			return true;  // array of @/@! slots into an unsized array
	}
	if (btk == "pntr") {
		const json* pointee = &bt["base-type"];
		while (pointee->value("type-kind", "") == "pntr") pointee = &(*pointee)["base-type"];
		string pk = pointee->value("type-kind", "");
		if (pk == "prim" || pk == "func")
			return true;  // array of pointers (@T, @!@T, @func(...) elements)
	}
	if (btk == "arr" && bt.value("specifier", "") == "raw" && !bt["size-expr"].is_null())
		return isDeclarableVarType(bt);  // fixed-size multidim array (also covers embedded-struct arrays)
	return false;
}

// `$T v;` / `$[n]T a;` put a local's storage in the stack frame. Whether the
// base is a struct or an array is only known in SA, so any declarable base is
// carried here and SA rejects the rest with a real message.
inline bool isFrameVarType(const json& t)
{
	return t.value("type-kind", "") == "embed" && isDeclarableVarType(t["base-type"]);
}

// A typed lit-int, like a macro-folded constant: SA keeps its bool type
// instead of retyping it from context, so no separate literal kind is needed.
inline json boolLiteral(const char* value)
{
	return {{"expr-type", "lit-int"}, {"value", value},
	        {"value-type", {{"type-kind", "prim"}, {"type-name", "bool"}}}}; // LCOV_EXCL_BR_LINE
} // LCOV_EXCL_BR_LINE

// Unary minus on an untyped integer literal becomes one negative literal, so
// SA's range check sees `-128` as a single value (an int8 `neg(128)` operand
// would be out of range). A typed lit-int (macro-folded) keeps its `neg`.
inline json negateExpr(json operand)
{
	if (operand.value("expr-type", "") == "lit-int" && !operand.contains("value-type")) {
		string v = operand["value"];
		operand["value"] = (v[0] == '-') ? v.substr(1) : "-" + v;
		return operand;
	}
	return {{"expr-type", "neg"}, {"operand", std::move(operand)}}; // LCOV_EXCL_BR_LINE
} // LCOV_EXCL_BR_LINE

// A `link` clause entry ending in ".o" names an object file rather than a -l
// library. Resolved here, against the source file's directory as a local
// cinclude header is, so everything downstream sees only absolute paths.
inline bool isLinkObjName(const string& name)
{
	return name.size() > 2 && name.compare(name.size() - 2, 2, ".o") == 0;
}

inline string resolveLinkObjPath(const string& name, const string& inputFile)
{
	std::filesystem::path p = name;
	if (!p.is_absolute())
		p = std::filesystem::absolute(std::filesystem::path(inputFile).parent_path() / p);
	return p.lexically_normal().string();
}

// Links are resolved, unlike a `link` object's path: a module is identified by
// its file, so two spellings of one file must come out the same.
inline json resolveImportPath(json pathInf, const string& inputFile)
{
	if (pathInf["path-type"] == "src") {
		std::filesystem::path p = std::filesystem::path(inputFile).parent_path() / pathInf["path"].get<string>();
		pathInf["path"] = std::filesystem::weakly_canonical(p).string();
	}
	return pathInf;
}

inline bool isLibraryImportPath(const string& path)
{
	if (path.find('.') != string::npos || path.find('/') == string::npos) return false;
	return path.front() != '/' && path.back() != '/' && path.find("//") == string::npos;
}
