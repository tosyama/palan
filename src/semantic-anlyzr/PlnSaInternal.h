/// Inline helpers shared across PlnSemanticAnalyzer implementation files.
///
/// @file PlnSaInternal.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once
#include <string>
#include <charconv>
#include <boost/assert.hpp>
#include <cstdint>
#include "../../lib/json/single_include/nlohmann/json.hpp"
#include "PlnType.h"
// FieldLayout/StructDef live in PlnSemanticAnalyzer.h, not here; every SA .cpp
// already includes it before this header, but classifySysVStructRet below
// needs it too, and a standalone consumer (sa-unit-tester) would otherwise
// have to know that ordering by convention rather than by include graph.
#include "PlnSemanticAnalyzer.h"

using json = nlohmann::json;
using namespace std;

// A type owned through a pointer and named by its identity: a struct, or an
// opaque type, which is one whose layout is its contents'.
inline bool declaresStructType(const string& stmtType)
{
	return stmtType == "struct-def" || stmtType == "opaque-def";
}
inline bool isTypeDecl(const string& stmtType)
{
	return declaresStructType(stmtType) || stmtType == "enum-def" || stmtType == "type-alias";
}

// LCOV_EXCL_BR_START
inline json wrapConvert(const json& expr, const json& to_type) {
	// Adding or dropping an enum label changes no bits, so no convert node.
	const json& from_type = expr["value-type"];
	if (to_type.value("type-kind", "") == "prim" && from_type.value("type-kind", "") == "prim"
	    && to_type["type-name"] == from_type["type-name"]) {
		json relabeled = expr;
		relabeled["value-type"] = to_type;
		return relabeled;
	}
	return {
		{"expr-type",  "convert"},
		{"value-type", to_type},
		{"from-type",  expr["value-type"]},
		{"src",        expr}
	};
}
// LCOV_EXCL_BR_STOP

inline json strLitType() {
	return {{"type-kind", "pntr"}, {"base-type", {{"type-kind", "prim"}, {"type-name", "uint8"}}}}; // LCOV_EXCL_BR_LINE
} // LCOV_EXCL_BR_LINE

// True if `t` is a non-float primitive type (int8..uint64). Shared by every
// operator that requires integer operands (logical &&/||/!, bitwise &/|/^/~)
// so the "not a float, and not a pointer/struct" check has one definition.
inline bool isFloatPrim(const PlnType* t) {
	if (t == nullptr || t->kind != PlnType::Kind::Prim) return false;
	auto pn = static_cast<const PrimType*>(t)->name;
	return pn == PrimType::Name::Float32 || pn == PrimType::Name::Float64 || pn == PrimType::Name::Float80;
}

inline bool isIntegerPrim(const PlnType* t) {
	if (t->kind != PlnType::Kind::Prim || isFloatPrim(t)) return false;
	return static_cast<const PrimType*>(t)->name != PrimType::Name::Void;
}

inline bool intLiteralFitsBits(const string& value, int bits, bool isSigned)
{
	bool neg = value[0] == '-';
	const char* first = value.data() + (neg ? 1 : 0);
	const char* last = value.data() + value.size();
	uint64_t mag;
	// The lexer only produces digit strings, so the only failure is overflow.
	if (from_chars(first, last, mag).ec != errc()) return false;
	if (isSigned) {
		uint64_t posMax = (uint64_t(1) << (bits - 1)) - 1;
		return mag <= (neg ? posMax + 1 : posMax);
	}
	if (neg) return mag == 0;
	return bits == 64 || mag <= (uint64_t(1) << bits) - 1;
}

inline bool intLiteralFits(const string& value, PrimType::Name pn)
{
	int bits = 64;
	bool isSigned = true;
	switch (pn) {
		case PrimType::Name::Int8:   bits = 8;  break;
		case PrimType::Name::Int16:  bits = 16; break;
		case PrimType::Name::Int32:  bits = 32; break;
		case PrimType::Name::Int64:  bits = 64; break;
		case PrimType::Name::Uint8:  bits = 8;  isSigned = false; break;
		case PrimType::Name::Uint16: bits = 16; isSigned = false; break;
		case PrimType::Name::Uint32: bits = 32; isSigned = false; break;
		case PrimType::Name::Bool:   bits = 1;  isSigned = false; break;
		default:                     bits = 64; isSigned = false; break;
	}
	return intLiteralFitsBits(value, bits, isSigned);
}

inline bool isBoolPrim(const PlnType* t) {
	return t != nullptr && t->kind == PlnType::Kind::Prim
	    && static_cast<const PrimType*>(t)->name == PrimType::Name::Bool;
}

// True if a value may be written through this pointer-typed value-type
// (i.e. it is not a `@T` read-only pointer). A missing "mutable" key means
// writable: it is the default for every pntr value-type SA synthesizes for
// struct/array variables, which carry no read-only concept of their own.
inline bool isWritableThrough(const json& vt) { return vt.value("mutable", true); }

json shapeElem(const json& elem);
json helperLens(const json& elem);
// The root of an array location (the variable `a` of `a[i][j]`) and how many
// levels down it the location lies.
std::pair<const json*, size_t> arrRoot(const json& location);
// Level `level` of the array variable `root`, known only at run time, which the
// variable keeps from its declaration as __<name>_d<level>.
json runtimeLen(const json& root, size_t level);
string allocShapeKey(const AllocShapeRef& shape);
inline AllocShapeRef structShape(const string& name)
{
	return {AllocFamily::Struct, {{"type-kind","struct"},{"type-name",name}}}; // LCOV_EXCL_BR_LINE
} // LCOV_EXCL_BR_LINE
// len, push, resize and pop of the `[+]T` whose header type is varr, as call
// candidates calling impl's helpers in module `allocators`; only their signatures
// without impl.
json varrBuiltinDefs(const json& varr, const AllocShapeRef* impl = nullptr, const string& allocators = "");
// len, has, remove and clear of the `[""]T` whose header type is dict, as varrBuiltinDefs.
json dictBuiltinDefs(const json& dict, const AllocShapeRef* impl = nullptr, const string& allocators = "");

enum class PtrPermission { Ok, Upgrade, InnerNarrowing, FuncSigMismatch };

// Checks binding `from` to `to` level by level down their pointer chains.
// Upgrade: some level would turn read-only (`@T`) into mutable (`@!T`).
// InnerNarrowing: a level narrows `@!` to `@` below a mutable level -- through
// a `@!@int32` made from a `@!@!int32`, a read-only pointer could be stored
// where the original still treats it as mutable (C's `int**` -> `const int**`).
// Non-pointer types, and narrowing while every level above is read-only, are fine.
// FuncSigMismatch: the function types reached differ in a pointer permission.
inline bool funcSigPermissionOk(const json& from, const json& to);
inline bool isOwnedBlock(const json& vt);

inline PtrPermission ptrPermission(const json& from, const json& to)
{
	bool aboveAllReadOnly = true, innerNarrowing = false;
	const json* f = &from;
	const json* t = &to;
	while (f->value("type-kind","") == "pntr" && t->value("type-kind","") == "pntr") {
		bool fw = isWritableThrough(*f), tw = isWritableThrough(*t);
		if (!fw && tw) return PtrPermission::Upgrade;
		if (fw && !tw && !aboveAllReadOnly) innerNarrowing = true;
		if (tw) aboveAllReadOnly = false;
		f = &(*f)["base-type"];
		t = &(*t)["base-type"];
	}
	if (!funcSigPermissionOk(*f, *t)) return PtrPermission::FuncSigMismatch;
	return innerNarrowing ? PtrPermission::InnerNarrowing : PtrPermission::Ok;
}

inline bool ptrPermissionOk(const json& from, const json& to)
{
	return ptrPermission(from, to) == PtrPermission::Ok;
}

// A function owning a parameter frees it, so it cannot stand in for one
// borrowing it, nor the reverse.
inline bool sameOwnership(const json& a, const json& b) { return isOwnedBlock(a) == isOwnedBlock(b); }

// A function stored as `to` is called with `to`'s arguments and its result is
// read as `to`'s, so permissions are checked in the direction each value
// flows: reversed for the parameters. Types themselves are compared by the
// registry first, so the parameter counts already agree.
inline bool funcSigPermissionOk(const json& from, const json& to)
{
	if (from.value("type-kind","") != "func" || to.value("type-kind","") != "func") return true;
	const json& fp = from["parameters"];
	const json& tp = to["parameters"];
	BOOST_ASSERT(fp.size() == tp.size());
	for (size_t i = 0; i < fp.size(); i++)
		if (!sameOwnership(tp[i]["var-type"], fp[i]["var-type"])
		    || !ptrPermissionOk(tp[i]["var-type"], fp[i]["var-type"])) return false;
	return sameOwnership(from["ret-type"], to["ret-type"]) && ptrPermissionOk(from["ret-type"], to["ret-type"]);
}

// funcSigPermissionOk's ownership half alone, for a cast: a cast may change
// pointer permissions, but not who frees what a function is given or returns.
inline bool funcSigOwnershipOk(const json& from, const json& to)
{
	const json* f = &from;
	const json* t = &to;
	while (f->value("type-kind","") == "pntr" && t->value("type-kind","") == "pntr") {
		f = &(*f)["base-type"];
		t = &(*t)["base-type"];
	}
	if (f->value("type-kind","") != "func" || t->value("type-kind","") != "func") return true;
	const json& fp = (*f)["parameters"];
	const json& tp = (*t)["parameters"];
	BOOST_ASSERT(fp.size() == tp.size());
	for (size_t i = 0; i < fp.size(); i++)
		if (!sameOwnership(tp[i]["var-type"], fp[i]["var-type"])
		    || !funcSigOwnershipOk(tp[i]["var-type"], fp[i]["var-type"])) return false;
	return sameOwnership((*f)["ret-type"], (*t)["ret-type"]) && funcSigOwnershipOk((*f)["ret-type"], (*t)["ret-type"]);
}

// A pntr(struct) with no "mutable" key is a struct's own storage (a struct
// variable, owned field or array element); one with the key is a borrow
// ('@T'/'@!T', or a C pointer whose const was folded in by normalizeCType).
// PlnTypeRegistry interns both alike, so test the raw JSON. A contiguous
// struct array ([n]$T) shares the shape but is an array, not one struct.
inline bool isStructPntr(const json& vt)
{
	return vt.value("type-kind","") == "pntr" && vt.contains("base-type")
	    && vt["base-type"].value("type-kind","") == "struct" && !vt.value("embedded", false);
}
// A '@T'/'@!T' pointer to any T, including one to contiguous rows or structs of
// unknown count (a '[n]@![]$T' slot). A borrowed array level also has
// "mutable", but with "arr-size" or, when its size is known only at run time,
// "unsized"; an owned array level has no "mutable".
inline bool isPtrBorrow(const json& vt)
{
	return vt.value("type-kind","") == "pntr" && vt.contains("mutable")
	    && !vt.contains("arr-size") && !vt.contains("unsized");
}
// NULL, as gen-ast folds the C macro.
inline bool isNullPtrLit(const json& expr)
{
	return expr.value("expr-type", "") == "lit-int" && expr.contains("value-type") // LCOV_EXCL_BR_LINE
	    && expr["value-type"].value("type-kind", "") == "pntr"; // LCOV_EXCL_BR_LINE
}
inline bool isAggregateLit(const json& expr)
{
	string et = expr.value("expr-type", "");
	return et == "arr-lit" || et == "dict-lit";
}
// A type a function pointer's parameter or return may have: a scalar, a
// borrow, or an owned block, passed as a function passes it.
inline bool isFuncSigType(const json& vt)
{
	string tk = vt.value("type-kind","");
	if (vt.value("variadic", false)) return isFuncSigType(vt["base-type"]);
	return (tk == "prim" && vt.value("type-name","") != "void") || (tk == "pntr" && vt.contains("mutable"))
	    || isOwnedBlock(vt);
}
inline bool isFuncPtr(const json& vt)
{
	return vt.value("type-kind","") == "pntr" && vt.contains("base-type")
	    && vt["base-type"].value("type-kind","") == "func";
}
inline bool isStructStorage(const json& vt) { return isStructPntr(vt) && !vt.contains("mutable"); }
// `[+]T` as written, before SA gives it its representation below.
inline bool isVarrAstType(const json& t)
{
	return t.value("type-kind","") == "arr" && t.value("specifier","") == "variable";
}
inline bool isVariadicAstType(const json& t)
{
	return t.value("type-kind","") == "arr" && t.value("specifier","") == "variadic"; // LCOV_EXCL_BR_LINE
}
// The `[...]E` parameter of a Palan signature, which only the last one can be.
inline const json* palanVariadicParam(const json& params)
{
	if (params.empty() || !params.back().contains("var-type")) return nullptr;
	return params.back()["var-type"].value("variadic", false) ? &params.back() : nullptr;
}
// A `[+]T` value: one word pointing at its {len, cap, data} header.
inline bool isVarrPntr(const json& vt)
{
	return vt.value("type-kind","") == "pntr" && vt.contains("base-type")
	    && vt["base-type"].value("type-kind","") == "varr";
}
inline bool isVarrStorage(const json& vt) { return isVarrPntr(vt) && !vt.contains("mutable"); }
inline bool isDictFamily(AllocFamily f) { return f == AllocFamily::Dict || f == AllocFamily::EDict; }
inline bool isDictAstType(const json& t) { return t.value("type-kind","") == "dict"; }
// std/rt's dictionary header: {live, used, cap, ents, tsize, table}, then the
// values' `[+]T` header.
constexpr int DictUsedOffset = 8;
constexpr int DictValuesOffset = 48;
// A `[""]T` value: one word pointing at the header std/rt keeps. Its kind is
// not the written "dict", so that a type already analyzed is not taken for one
// written.
inline bool isDictPntr(const json& vt)
{
	return vt.value("type-kind","") == "pntr" && vt.contains("base-type")
	    && vt["base-type"].value("type-kind","") == "strdict";
}
inline bool isDictStorage(const json& vt) { return isDictPntr(vt) && !vt.contains("mutable"); }
// elem and `embedded` are as a `[+]T`'s (varrPntrType), its values being one.
inline json dictPntrType(const json& elem, bool embedded)
{
	// LCOV_EXCL_BR_START
	json dict = {{"type-kind","strdict"},{"base-type",elem}};
	if (embedded) dict["embedded"] = true;
	return {{"type-kind","pntr"},{"base-type",move(dict)}};
	// LCOV_EXCL_BR_STOP
} // LCOV_EXCL_BR_LINE
// A variable or parameter declared as a borrowed array ('@[n]T'/'@![n]T',
// '@[+]T'/'@![+]T') or dictionary ('@[""]T'/'@![""]T'). Checked on its var-type:
// a row reached through it also carries "mutable" but is part of the borrowed
// storage, not a borrow of its own.
inline bool isArrBorrowVar(const json& vt)
{
	return vt.value("type-kind","") == "pntr" && vt.contains("mutable")
	    && (vt.contains("arr-size") || isVarrPntr(vt) || isDictPntr(vt));
}
// Where a `[+]T` passes its data instead of its header: to C, to an unsized
// `[]T` (whose length the callee is told some other way), or, borrowed, to a
// borrow of elements of no known count (`@[]T`).
inline bool takesVarrData(const json& argVT, const json* paramVT, bool isCFunc)
{
	if (!isVarrPntr(argVT)) return false;
	if (isCFunc) return true;
	if (!paramVT || paramVT->value("type-kind","") != "pntr" || paramVT->contains("arr-size")
	    || isStructStorage(*paramVT) || isVarrPntr(*paramVT)) return false;
	return !paramVT->contains("mutable") || argVT.contains("mutable");
}
// elem is one element as the data holds it: a primitive, an owned struct's
// storage (`[+]T`), or the struct itself (`[+]$T`). `embedded` marks the
// last, since an element type alone cannot tell a row of `[+][n]T` from one
// laid out in place.
inline json varrPntrType(const json& elem, bool embedded)
{
	// LCOV_EXCL_BR_START
	json varr = {{"type-kind","varr"},{"base-type",elem}};
	if (embedded) varr["embedded"] = true;
	return {{"type-kind","pntr"},{"base-type",move(varr)}};
	// LCOV_EXCL_BR_STOP
} // LCOV_EXCL_BR_LINE
// Also a `[""]$T`'s, whose values are a `[+]$T`.
inline bool isVarrEmbedded(const json& vt) { return vt["base-type"].value("embedded", false); }
inline json lenLit(int64_t v)
{
	// LCOV_EXCL_BR_START
	return {{"expr-type","lit-int"},{"value",to_string(v)},{"value-type",{{"type-kind","prim"},{"type-name","int64"}}}};
	// LCOV_EXCL_BR_STOP
} // LCOV_EXCL_BR_LINE
// The header's len, which `len(@v)` reads in place of a call.
inline json varrLen(json varr)
{
	// LCOV_EXCL_BR_START
	json i64 = {{"type-kind","prim"},{"type-name","int64"}};
	return {{"expr-type","field-access"},{"ptr-expr",move(varr)},{"offset",0},{"value-type",move(i64)}};
	// LCOV_EXCL_BR_STOP
} // LCOV_EXCL_BR_LINE
inline bool isStructBorrow(const json& vt)  { return isStructPntr(vt) && isPtrBorrow(vt); }
// Strips a declared variable's `$` and returns where its storage lives.
inline VarStorage takeVarStorage(json& varType)
{
	if (varType.value("type-kind", "") != "embed") return VarStorage::Heap;
	json base = varType["base-type"];
	varType = move(base);
	return VarStorage::Frame;
}
inline bool isArrLevel(const json& t);
// A struct's, a `[+]T`'s or a constant-size array's own block: what a variable or a Palan
// parameter owns and frees, and what a copy into it fills.
inline bool isOwnedBlock(const json& vt)
{
	return isStructStorage(vt) || isVarrStorage(vt) || isDictStorage(vt)
	    || (isArrLevel(vt) && (vt.contains("arr-size") || vt.contains("run-size")) && !vt.contains("mutable"));
}
// An owned element (a struct's, a `[+]T`'s or a row's storage) has no
// permission of its own: it is writable only as far as the array it is
// reached through.
inline bool isWritableElem(const json& saIdx) {
	return !isOwnedBlock(saIdx["value-type"]) || isWritableThrough(saIdx["array"]["value-type"]);
}
// A block returned by a Palan call, owned by whoever receives it.
inline bool isExpiringOwned(const json& v)
{
	return v.value("category", "") == "expiring" && isOwnedBlock(v["value-type"]);
}
// A call returning a struct (or a temp holding one) whose receiver has not yet
// given it the storage to write the struct into.
inline bool isStructRetCall(const json& v)
{
	if (v.value("expr-type", "") == "temp") return isStructRetCall(v["value"]);
	return v.contains("struct-ret") && !v.contains("ret-dest");
}
// An owned array, row or array field named as it is: its value is a pointer to
// its elements, so only the missing permission tells it from `@a`. A `[]T`
// parameter is the same pointer but owns nothing, so it is told by the
// variable having neither a known size nor ownership.
inline bool namesArrStorage(const json& v)
{
	const json& vt = v["value-type"];
	if (!isArrLevel(vt) || vt.contains("mutable") || v.value("category", "") == "expiring")
		return false;
	return vt.contains("arr-size") || v.value("category", "") == "owned"
	    || v.value("expr-type", "") == "arr-index";
}
inline bool returnsBorrow(const json& call)
{
	bool retBorrow = call.contains("value-type") && isPtrBorrow(call["value-type"]);
	if (call.contains("value-types"))
		for (auto& t : call["value-types"])
			retBorrow = retBorrow || isPtrBorrow(t);
	return retBorrow;
}
// A call returning a borrow that may point to a temp struct given as one of its
// arguments, directly or through a nested call of the same kind. A temp holding
// a call (a chained call's return values) may hold the temps it was given.
inline bool holdsCallTemps(const json& v)
{
	string et = v.value("expr-type", "");
	if (et == "temp")
		return v["value"].value("expr-type", "") == "call" && returnsBorrow(v["value"]);
	if (et != "call" || !v.contains("args") || !returnsBorrow(v))
		return false;
	for (auto& a : v["args"])
		if (a.value("expr-type", "") == "temp" || holdsCallTemps(a))
			return true;
	return false;
}
// An assignment node: a statement, or the value of another assignment or of a
// chained call when the assignments chain.
inline bool isStoreStmt(const json& e)
{
	string t = e.value("stmt-type", "");
	return t == "assign" || t == "arr-assign" || t == "field-assign";
}
inline bool chainsFromStore(const json& e)
{
	if (isStoreStmt(e)) return true;
	return e.value("expr-type", "") == "chain-call" && chainsFromStore(e["value"]);
}
inline bool hasCall(const json& e)
{
	if (e.is_object()) {
		string et = e.value("expr-type", "");
		if (et == "call" || et == "member-call" || et == "chain-call" || et == "call-ptr" || et == "append") return true;
		for (auto& [k, v] : e.items())
			if (hasCall(v)) return true;
	} else if (e.is_array()) {
		for (auto& v : e)
			if (hasCall(v)) return true;
	}
	return false;
}
inline bool isLocationExpr(const json& e)
{
	string et = e.value("expr-type", "");
	return et == "id" || et == "arr-index" || et == "field-access";
}
// An arithmetic operator is a call when an operator function defines it.
inline bool isCallBorrow(const json& expr)
{
	if (expr.value("expr-type", "") != "addr-of") return false;
	string et = expr["object"].value("expr-type", "");
	return et == "call" || et == "member-call" || et == "chain-call"
	    || et == "add" || et == "sub" || et == "mul" || et == "div" || et == "mod" || et == "neg";
}

// LCOV_EXCL_BR_START
inline json fieldPrimLeaf(const FieldLayout& f)
{
	json t = {{"type-kind","prim"},{"type-name",f.typeName}};
	if (!f.enumName.empty()) t["enum"] = f.enumName;
	return t;
}

// The struct a field holds, embedded or owned; empty for any other field.
inline string fieldStructName(const FieldLayout& f)
{
	if (f.typeKind == "embed") return f.typeName;
	if (f.typeKind == "owned" && isStructPntr(f.type)) return f.type["base-type"]["type-name"];
	return "";
}

// What sa_arr_lit_values gives for a dictionary level: {keys, values}, not
// an analyzed expression.
inline bool isDictLitValues(const json& v) { return v.is_object() && v.contains("keys") && !v.contains("expr-type"); }
// An array or a dictionary a struct literal gives the field, embedded or owned.
inline bool isLitArrField(const FieldLayout& f)
{
	return f.typeKind == "embed-arr" || f.typeKind == "embed-varr"
	    || (f.typeKind == "owned" && (isArrLevel(f.type) || isVarrStorage(f.type) || isDictStorage(f.type)));
}

inline json fieldValueType(const FieldLayout& f)
{
	if (f.typeKind == "prim")
		return fieldPrimLeaf(f);
	if (f.typeKind == "embed")
		return {{"type-kind","pntr"},{"base-type",{{"type-kind","struct"},{"type-name",f.typeName}}}};
	return f.type;
}
// LCOV_EXCL_BR_STOP

// Returns element byte size for a primitive type name; -1 if unknown
inline int elemSizeBytes(const string& typeName)
{
	if (typeName == "int8"  || typeName == "uint8" || typeName == "bool") return 1;
	if (typeName == "int16" || typeName == "uint16") return 2;
	if (typeName == "int32" || typeName == "uint32" || typeName == "flo32") return 4;
	if (typeName == "int64" || typeName == "uint64" || typeName == "flo64") return 8;
	if (typeName == "flo80") return 16;
	return -1;
}

// A pointee type name that is primitive rather than a struct name. `void` is
// accepted here but not as a standalone value type (PlnType.h PrimType::Name::Void).
inline bool isPrimPointeeName(const string& name)
{
	return name == "void" || elemSizeBytes(name) >= 0;
}

// The bytes an element takes when it is laid out in place: a primitive or a
// slot, a struct, or a row of them, whose own rows may be laid out in place
// in turn; -1 when a row's length is known only at run time.
inline int64_t inPlaceBytes(const json& elem, const map<string, StructDef>& defs)
{
	string kind = elem.value("type-kind","");
	if (kind == "struct") return defs.at(elem["type-name"].get<string>()).totalSize;
	if (kind == "prim") return elemSizeBytes(elem["type-name"].get<string>());
	if (!isArrLevel(elem)) return 8;
	if (!elem.contains("arr-size")) return -1;
	const json& child = elem["base-type"];
	int64_t childSize = !elem.value("embedded", false) && isArrLevel(child) ? 8 : inPlaceBytes(child, defs);
	return childSize < 0 ? -1 : elem["arr-size"].get<int64_t>() * childSize;
}

// SysV AMD64 ABI (§3.2.3) eightbyte classes for a struct passed or returned by value.
enum class EightbyteClass { Integer, Sse, X87 };
struct Eightbyte {
	EightbyteClass cls;
	int size;            // only the last eightbyte may be <8
	bool flo64 = false;  // taken up by one flo64 (codegen loads it whole)
	bool flo80 = false;  // starts a flo80
};

// Recursively marks the eightbytes covered by one field as INTEGER; SSE is
// each eightbyte's initial class, so an all-float field simply leaves it unmarked
// (mixing within one eightbyte follows the "any INTEGER wins" merge rule).
// A flo64 is 8-byte aligned (or the struct is MEMORY class), so it takes up
// exactly one eightbyte, which is marked as such.
// `base` is the field's struct-relative offset already shifted by every
// enclosing embed/embed-arr, so recursion always marks against the same
// top-level `ebs` vector.
inline void classifySysVWalk(const StructDef& d, int base, int nEightbytes,
                              const map<string, StructDef>& defs,
                              vector<Eightbyte>& ebs)
{
	auto markInt = [&](int offset, int size) {
		for (int b = offset; b < offset + size; b++) {
			int eb = b / 8;
			if (eb < nEightbytes) ebs[eb].cls = EightbyteClass::Integer;
		}
	};
	auto markPrim = [&](int offset, const string& name, int size) {
		if (name == "flo64") ebs[offset / 8].flo64 = true;
		else if (name == "flo80") ebs[offset / 8].flo80 = true;
		else if (name != "flo32") markInt(offset, size);  // Sse is the default
	};
	for (auto& f : d.fields) {
		int off = base + f.offset;
		if (f.typeKind == "prim") {
			markPrim(off, f.typeName, f.size);
		} else if (f.typeKind == "raw-ptr" || f.typeKind == "owned" || f.typeKind == "embed-varr") {
			markInt(off, f.size);
		} else if (f.typeKind == "embed") {
			classifySysVWalk(defs.at(f.typeName), off, nEightbytes, defs, ebs);
		} else if (f.typeKind == "embed-arr") {
			// Each element laid out in place, down to primitives, structs and pointers.
			function<void(const json&, int)> walk = [&](const json& t, int at) {
				string kind = t.value("type-kind", "");
				if (kind == "struct") {
					classifySysVWalk(defs.at(t["type-name"].get<string>()), at, nEightbytes, defs, ebs);
				} else if (kind == "prim") {
					string name = t["type-name"].get<string>();
					markPrim(at, name, elemSizeBytes(name));
				} else if (isArrLevel(t)) {
					const json& elem = t["base-type"];
					int64_t size = inPlaceBytes(elem, defs);
					for (int64_t i = 0; i < t["arr-size"].get<int64_t>(); i++) walk(elem, at + (int)(i * size));
				} else {
					markInt(at, 8);
				}
			};
			walk(f.type, off);
		}
	}
}

// Classifies a struct passed or returned by value per the System V AMD64 ABI
// (§3.2.3): over 16 bytes or with an unaligned field is MEMORY class (an empty
// vector here); otherwise it travels in up to two eightbytes, each INTEGER or SSE.
// A flo80 makes it MEMORY class too, except a returned struct of 16 bytes,
// which is then that one flo80 alone and comes back in %st(0).
inline vector<Eightbyte> classifySysVStruct(const StructDef& def, const map<string, StructDef>& defs,
                                            bool forReturn)
{
	if (def.totalSize > 16 || def.hasUnalignedField) return {};  // MEMORY class

	int nEightbytes = (def.totalSize + 7) / 8;
	int tailWidth = def.totalSize - 8 * (nEightbytes - 1);

	vector<Eightbyte> result;
	for (int i = 0; i < nEightbytes; i++)
		result.push_back({EightbyteClass::Sse, (i == nEightbytes - 1) ? tailWidth : 8});
	classifySysVWalk(def, 0, nEightbytes, defs, result);
	for (auto& eb : result)
		if (eb.flo80)
			return forReturn && def.totalSize == 16 ? vector<Eightbyte>{{EightbyteClass::X87, 16}} : vector<Eightbyte>{};
	return result;
}

// The "eightbytes" list of a "struct-ret" or "struct-arg" descriptor; empty
// for MEMORY class.
inline json eightbytesJson(const StructDef& def, const map<string, StructDef>& defs, bool forReturn)
{
	json ebs = json::array();
	for (auto& eb : classifySysVStruct(def, defs, forReturn)) {
		const char* cls = eb.cls == EightbyteClass::Integer ? "int" : eb.cls == EightbyteClass::Sse ? "sse" : "x87";
		json j = {{"class", cls}, {"size", eb.size}}; // LCOV_EXCL_BR_LINE
		if (eb.flo64) j["flo64"] = true; // LCOV_EXCL_BR_LINE
		ebs.push_back(move(j));
	}
	return ebs;
} // LCOV_EXCL_BR_LINE

// Builds a synthetic free() expression statement for the named pointer variable
// LCOV_EXCL_BR_START
inline json makeFreeStmt(const string& name, const json& pntrType)
{
	return {
		{"stmt-type", "expr"},
		{"body", {
			{"expr-type", "call"}, {"name", "free"}, {"func-type", "c"},
			{"args", json::array({{
				{"expr-type", "id"}, {"name", name},
				{"var-type", pntrType}, {"value-type", pntrType}
			}})}
		}}
	};
}
// LCOV_EXCL_BR_STOP

inline const PlnType* variadicPromote(const PlnType* t, PlnTypeRegistry& reg)
{
	if (t->kind == PlnType::Kind::Enum) return variadicPromote(static_cast<const EnumType*>(t)->base, reg);
	if (t->kind != PlnType::Kind::Prim) return t;
	const auto* p = static_cast<const PrimType*>(t);
	using N = PrimType::Name;
	switch (p->name) {
		case N::Int8:   case N::Int16:  return reg.prim(N::Int32);
		case N::Uint8:  case N::Uint16: return reg.prim(N::Uint32);
		case N::Bool:                   return reg.prim(N::Int32);
		case N::Float32:                return reg.prim(N::Float64);
		default: return t;
	}
}

// Element count of an SA-evaluated array size, or -1 when it is not a
// compile-time literal. sa_arr_size_expr may wrap a literal in a uint64 convert.
inline int64_t constArrSize(const json& sz) {
	const json* e = &sz;
	if (e->value("expr-type","") == "convert") e = &(*e)["src"];
	string et = e->value("expr-type","");
	if (et != "lit-int" && et != "lit-uint") return -1;
	return stoll((*e)["value"].get<string>());
}

// A size known only at run time is kept by the variable owning the array,
// which a `[]T` parameter's level, also without "arr-size", is not.
inline void setArrSize(json& pntrType, const json& saSize) {
	int64_t n = constArrSize(saSize);
	if (n >= 0) pntrType["arr-size"] = n;
	else pntrType["run-size"] = true;
}

// The array levels stop at an element that is a struct's own storage or a
// '@T'/'@!T' pointer.
inline bool isArrLevel(const json& t) {
	return t.value("type-kind","") == "pntr" && !isStructStorage(t) && !isPtrBorrow(t) && !isVarrPntr(t) && !isDictPntr(t);
}
// An array whose elements are blocks of their own (structs, `[+]U`s, rows of
// any length), built and freed through the generated helpers, rather than
// laid out in the array's block.
// Rows laid out in place hold their own elements' slots, so `[2]$[3]T` owns
// what `[3]T` does.
inline bool ownsElems(const json& arrType)
{
	const json* t = &arrType;
	while (t->value("embedded", false) && isArrLevel((*t)["base-type"])) t = &(*t)["base-type"];
	const json& elem = (*t)["base-type"];
	return !t->value("embedded", false)
	    && (isOwnedBlock(elem) || (isArrLevel(elem) && !elem.contains("mutable")));
}

inline string PlnSemanticAnalyzer::structDisplayName(const string& structName) const {
	auto it = cStructDisplayNames_.find(structName);
	return it != cStructDisplayNames_.end() ? it->second : structName;
}

// Renders an SA type as source-like text for diagnostics: a struct's own
// storage as "T", a borrow as "@T"/"@!T", an array as "[20][10]int32" or
// "[3]$[?]int32" (an unknown size shows as "?"), with a borrowed array's
// "@"/"@!" once in front although each of its levels carries "mutable".
inline string PlnSemanticAnalyzer::typeDisplayName(const json& t) const {
	if (!t.is_object() || !t.contains("type-kind")) // LCOV_EXCL_BR_LINE -- no real producer emits this
		return "malformed type"; // LCOV_EXCL_LINE
	const string kind = t["type-kind"].get<string>();
	if (kind == "prim") {
		if (t.contains("enum")) return t["enum"].get<string>();
		return structDisplayName(t.value("type-name", "malformed type")); // LCOV_EXCL_BR_LINE -- no real producer emits this
	}
	if (kind == "struct")
		return structDisplayName(t.value("type-name", "anonymous struct"));
	if (kind == "func") {
		string out = "func(";
		for (auto& p : t["parameters"]) {
			if (out.back() != '(') out += ", ";
			out += p.contains("var-type") ? typeDisplayName(p["var-type"]) : "...";
		}
		out += ")";
		if (t["ret-type"].value("type-name", "") != "void") out += " -> " + typeDisplayName(t["ret-type"]);
		return out;
	}
	if (kind != "pntr")
		return unrepresentableTypeName(t);
	if (!t.contains("base-type")) return "malformed type"; // LCOV_EXCL_LINE -- no real producer emits this
	auto perm = [](const json& p) { return string(isWritableThrough(p) ? "@!" : "@"); };
	if (t.value("variadic", false))
		return "[...]" + typeDisplayName(t["base-type"]);
	if (isVarrPntr(t))
		return (isVarrStorage(t) ? "" : perm(t)) + "[+]" + (isVarrEmbedded(t) ? "$" : "")
		     + typeDisplayName(t["base-type"]["base-type"]);
	if (isDictPntr(t))
		return (isDictStorage(t) ? "" : perm(t)) + "[\"\"]" + (isVarrEmbedded(t) ? "$" : "")
		     + typeDisplayName(t["base-type"]["base-type"]);
	if (isStructStorage(t))
		return typeDisplayName(t["base-type"]);
	if (isPtrBorrow(t))
		return perm(t) + typeDisplayName(t["base-type"]);
	string out = t.contains("mutable") ? perm(t) : "";
	const json* cur = &t;
	while (isArrLevel(*cur)) {
		out += "[" + (cur->contains("arr-size") ? to_string((*cur)["arr-size"].get<int64_t>()) : string("?")) + "]";
		if (cur->value("embedded", false))
			out += "$";
		cur = &(*cur)["base-type"];
	}
	return out + typeDisplayName(*cur);
} // LCOV_EXCL_BR_LINE

// Shape equality for binding an array to a borrowed array type: same depth,
// same embedded layout, every size `to` names known and equal (a borrowed
// array type names them all), owned elements (a struct's storage or a `[+]T`)
// in both or neither, and pointer elements in both or neither. The element type is left
// to the usual type compatibility check, which cannot tell a stored struct
// from a pointer to one and ignores pointer permissions. A pointer element
// keeps its permission, except that read-only slots may narrow '@!T' to '@T':
// with writable slots the callee could store a '@T' where the caller expects
// a '@!T'. An owned element (a row, a struct's storage or a `[+]T`) may be lent to a
// read-only '@T' slot when the slots are read-only too: the callee can neither
// replace it nor free a borrow.
inline bool arrShapeMatch(const json& from, const json& to, bool toSlotsMutable = true) {
	bool fp = isArrLevel(from), tp = isArrLevel(to);
	if (!toSlotsMutable && isPtrBorrow(to) && !isWritableThrough(to)) {
		if (fp) return arrShapeMatch(from["base-type"], to["base-type"], false);
		if (isOwnedBlock(from)) return true;
	}
	if (!fp || !tp) {
		if (fp != tp || isOwnedBlock(from) != isOwnedBlock(to) || isPtrBorrow(from) != isPtrBorrow(to))
			return false;
		return !isPtrBorrow(to) || isWritableThrough(from) == isWritableThrough(to)
		    || (!toSlotsMutable && !isWritableThrough(to));
	}
	return (!to.contains("arr-size") || (from.contains("arr-size") && from["arr-size"] == to["arr-size"]))
	    && from.value("embedded", false) == to.value("embedded", false)
	    && arrShapeMatch(from["base-type"], to["base-type"], isWritableThrough(to));
}

// Binding an array to a pointer that has no size of its own ('[]T', '[][m]T',
// a '[k]@![]T' slot, a C 'T*'). The type registry ignores sizes and cannot
// tell an owned row or struct from a '@T'/'@!T' pointer, so only this check
// keeps rows of another size out and keeps the elements' owner unchanged:
// either side would free, or overwrite, what the other one owns. A 'void*'
// on either side (NULL, a C 'void*') says nothing about the elements.
// A '[+]T' borrow must name the same rows, laid out the same way.
// Pointer elements follow the slots of a borrowed `[n]@T`.
inline bool elemShapeMatch(const json& from, const json& to) {
	if (isVarrPntr(from) && isVarrPntr(to)) {
		const json& fe = from["base-type"]["base-type"];
		const json& te = to["base-type"]["base-type"];
		if (isArrLevel(te) || isArrLevel(fe)) return from["base-type"] == to["base-type"];
		if (isPtrBorrow(te)) return isPtrBorrow(fe) && arrShapeMatch(fe, te, isWritableThrough(to));
	}
	if (to.value("type-kind","") != "pntr" || to.contains("arr-size") || !isArrLevel(from))
		return true;
	auto isVoid = [](const json& t) {
		return t.value("type-kind","") == "prim" && t.value("type-name","") == "void";
	};
	if (isVoid(from["base-type"]) || isVoid(to["base-type"]))
		return true;
	return arrShapeMatch(from["base-type"], to["base-type"], isWritableThrough(to));
}

// '->' into a value of this type copies its contents: a struct's own storage
// or an array level. Rebinding would alias the source and free it twice.
inline bool isCopiedByValue(const json& t) { return isOwnedBlock(t) || isArrLevel(t); }

// The source type that declares an owned value of saType: an array of its
// levels (a borrowed array parameter's), a `[+]T`, a struct or a primitive;
// null when an element is not one a literal gives.
inline json astTypeOf(const json& saType)
{
	// LCOV_EXCL_BR_START
	json out, elem;
	if (isVarrPntr(saType)) {
		out = {{"type-kind","arr"},{"specifier","variable"},{"size-expr",nullptr}};
		if (isVarrEmbedded(saType)) out["embedded"] = true;
		elem = astTypeOf(saType["base-type"]["base-type"]);
	} else if (isDictPntr(saType)) {
		out = {{"type-kind","dict"}};
		if (isVarrEmbedded(saType)) out["embedded"] = true;
		elem = astTypeOf(saType["base-type"]["base-type"]);
	} else if (isArrLevel(saType) && saType.contains("arr-size")) {
		out = {{"type-kind","arr"},{"specifier","raw"},
			{"size-expr",{{"expr-type","lit-uint"},{"value",to_string(saType["arr-size"].get<int64_t>())}}}};
		if (saType.value("embedded", false)) out["embedded"] = true;
		elem = astTypeOf(saType["base-type"]);
	} else if (saType.value("type-kind","") == "struct") {
		return {{"type-kind","prim"},{"type-name",saType["type-name"]}};
	} else if (isStructStorage(saType)) {
		return {{"type-kind","prim"},{"type-name",saType["base-type"]["type-name"]}};
	} else if (saType.value("type-kind","") == "prim") {
		return saType;
	} else if (isPtrBorrow(saType)) {
		out = {{"type-kind","pntr"},{"mutable",saType["mutable"]}};
		elem = astTypeOf(saType["base-type"]);
	}
	if (elem.is_null())
		return nullptr;
	out["base-type"] = move(elem);
	return out;
	// LCOV_EXCL_BR_STOP
} // LCOV_EXCL_BR_LINE


// Shape equality for such a copy, which is a plain byte copy level by level:
// the same layout and the exact same element type, since no conversion can
// apply, and equal sizes where both are known. A size known only at run time
// matches any: the lengths are the receiving owner's, and that they agree is
// the programmer's responsibility.
inline bool copyShapeMatch(const json& from, const json& to) {
	if (isArrLevel(to)) {
		if (!isArrLevel(from) || from.value("embedded", false) != to.value("embedded", false))
			return false;
		bool fromSized = from.contains("arr-size"), toSized = to.contains("arr-size");
		if ((!fromSized && !from.contains("run-size")) || (!toSized && !to.contains("run-size")))
			return false;
		if (fromSized && toSized && from["arr-size"] != to["arr-size"])
			return false;
		return copyShapeMatch(from["base-type"], to["base-type"]);
	}
	if (isStructStorage(from) != isStructStorage(to) || isPtrBorrow(from) != isPtrBorrow(to))
		return false;
	if (isPtrBorrow(to))
		return ptrPermissionOk(from, to) && from["base-type"] == to["base-type"];
	if (isStructStorage(to))
		return from["base-type"] == to["base-type"];
	if (isVarrPntr(from) || isVarrPntr(to))
		return isVarrPntr(from) && isVarrPntr(to) && from["base-type"] == to["base-type"];
	if (isDictPntr(from) || isDictPntr(to))
		return isDictPntr(from) && isDictPntr(to) && from["base-type"] == to["base-type"];
	return from.value("type-kind","") == to.value("type-kind","")
	    && from.value("type-name","") == to.value("type-name","");
}

// Whether a level of the array vt has a length known only at run time.
inline bool hasRunSize(const json& vt)
{
	for (const json* t = &vt; isArrLevel(*t); t = &(*t)["base-type"])
		if (t->value("run-size", false)) return true;
	return false;
}

// A move hands a block over with the lengths it has. A level the receiving
// variable keeps a length for at run time (`inherits`) takes them over; any
// other level's length is fixed, so a length known only at run time on either
// side cannot be shown to agree with it, unless a cast (`resized`) sets it.
enum class MoveFit { Match, Mismatch, RunSize };
inline MoveFit moveShapeFit(const json& from, const json& to, bool inherits, bool resized)
{
	if (!isArrLevel(to) || !isArrLevel(from))
		return copyShapeMatch(from, to) ? MoveFit::Match : MoveFit::Mismatch;
	if (!copyShapeMatch(from, to))
		return MoveFit::Mismatch;
	MoveFit fit = moveShapeFit(from["base-type"], to["base-type"], inherits, resized);
	bool fixed = !(inherits && to.value("run-size", false));
	if (fit == MoveFit::Match && fixed && !resized && (!from.contains("arr-size") || !to.contains("arr-size")))
		return MoveFit::RunSize;
	return fit;
}

// An array type as its owner holds it, without a borrow's permissions.
inline json withoutArrPermission(json t)
{
	json* cur = &t;
	while (isArrLevel(*cur)) {
		cur->erase("mutable");
		cur->erase("unsized");
		cur = &(*cur)["base-type"];
	}
	return t;
}
// What `[+]T` vt can be copied from: another `[+]T` (owned or borrowed) or a
// sized array, of exactly its element type and layout.
inline bool varrCopySourceMatch(const json& from, const json& vt)
{
	const json& elem = vt["base-type"]["base-type"];
	// A writable pointer slot may be copied into a read-only one, as between sized arrays.
	bool sized = from.contains("arr-size") || from.value("run-size", false);
	if (isPtrBorrow(elem)) {
		const json* fe = isVarrPntr(from) ? &from["base-type"]["base-type"]
		               : isArrLevel(from) && sized && !from.value("embedded", false)
		               ? &from["base-type"] : nullptr;
		return fe && copyShapeMatch(*fe, elem);
	}
	// The layouts must agree too: `[+][n]T` and `[+]$[n]T` share an element type.
	if (isVarrPntr(from))
		return from["base-type"] == vt["base-type"];
	// A contiguous struct array's type has the same shape as a lone struct's
	// storage, so only its stride tells it from one.
	if (isVarrEmbedded(vt))
		return from.contains("stride") && sized && withoutArrPermission(from["base-type"]) == elem;
	return isArrLevel(from) && sized && !from.value("embedded", false)
	    && withoutArrPermission(from["base-type"]) == elem;
}
// What a copy into vt, other than a lone struct, can take: a `[+]T` any run
// of its elements, a sized array the elements of a `[+]T` laid out the same
// way, anything else the same shape.
inline bool copySourceMatch(const json& from, const json& vt)
{
	if (isVarrStorage(vt)) return varrCopySourceMatch(from, vt);
	if (isDictStorage(vt)) return isDictPntr(from) && from["base-type"] == vt["base-type"];
	if (isVarrPntr(from) && isArrLevel(vt) && (vt.contains("arr-size") || vt.value("run-size", false))) {
		const json& fe = from["base-type"]["base-type"];
		if (isPtrBorrow(fe) || isPtrBorrow(vt["base-type"]))
			return isPtrBorrow(fe) && !vt.value("embedded", false) && copyShapeMatch(fe, vt["base-type"]);
		json asVarr = {{"type-kind","pntr"},{"base-type",from["base-type"]}}; // LCOV_EXCL_BR_LINE
		json run = withoutArrPermission(vt);
		run.erase("arr-size");
		run["run-size"] = true;
		return varrCopySourceMatch(run, asVarr);
	}
	return copyShapeMatch(from, vt);
}

// An array is borrowed with one permission for every level down to its
// elements, so a read-only 2D borrow also makes its rows read-only. A struct
// element stays the struct's storage; writes to it are checked against the
// array it is reached through. A pointer element keeps its own permission.
inline json withArrPermission(json t, bool isMutable) {
	json* cur = &t;
	while (isArrLevel(*cur)) {
		if (!cur->contains("arr-size")) (*cur)["unsized"] = true;
		(*cur)["mutable"] = isMutable;
		cur = &(*cur)["base-type"];
	}
	return t;
}

// A named C struct or union reference. Both are laid out through structDefs_
// (a union is a StructDef with every field at offset 0), so SA folds them to
// one canonical "struct" shape; a nameless one has nothing to register under
// and stays unrepresentable.
inline bool isCRecordTag(const json& t) {
	string tk = t.value("type-kind", "");
	return (tk == "strct" || tk == "union") && t.contains("type-name");
}

inline json normalizeCType(const json& type);

// Rewrites a named C struct/union tag to SA's canonical "struct", and folds a
// pointee's "const" into SA's own "mutable" pointer-permission flag
// (recursing into `func` nodes so callback pointer parameters get it too, and
// into `arr` nodes for a struct field's pointer-slot array).
inline json normalizeCType(const json& type) {
	if (type.value("type-kind","") == "pntr") {
		json t = type;
		json base = normalizeCType(type["base-type"]);
		// Code is never written through a function pointer, so it is `@func`
		// whatever C says.
		t["mutable"] = base.value("type-kind","") != "func" && !base.value("const", false);
		t["base-type"] = move(base);
		return t;
	}
	if (isCRecordTag(type)) {
		json t = type;
		t["type-kind"] = "struct";
		return t;
	}
	if (type.value("type-kind","") == "arr" && type.contains("base-type")) {
		json t = type;
		t["base-type"] = normalizeCType(type["base-type"]);
		return t;
	}
	if (type.value("type-kind","") == "func") {
		json t = type;
		t.erase("typedef-name");
		t["ret-type"] = normalizeCType(type["ret-type"]);
		for (auto& p : t["parameters"]) {
			if (!p.contains("var-type")) continue; // "..." variadic marker
			p.erase("name");
			p["var-type"] = normalizeCType(p["var-type"]);
		}
		return t;
	}
	return type;
} // LCOV_EXCL_BR_LINE

// typeAliases_ holds types in native syntax's shape (what `type A = ...;`
// writes), so a normalizeCType'd C typedef is written back into it: a struct
// tag as prim(Tag), and the pointee's "const" dropped once "mutable" carries it.
inline json cTypeAsAlias(const json& type) {
	string tk = type.value("type-kind", "");
	if (tk == "pntr")
		return {{"type-kind", "pntr"}, {"mutable", type["mutable"]}, // LCOV_EXCL_BR_LINE
		        {"base-type", cTypeAsAlias(type["base-type"])}}; // LCOV_EXCL_BR_LINE
	if (tk == "struct")
		return {{"type-kind", "prim"}, {"type-name", type["type-name"]}}; // LCOV_EXCL_BR_LINE
	json t = type;
	t.erase("const");
	t.erase("typedef-name");
	return t;
} // LCOV_EXCL_BR_LINE

// The symbol a C call or function pointer links to: a header can declare a
// name as an alias of another function (`#define sscanf __isoc99_sscanf`).
inline const json& cLinkName(const json& funcDef) {
	return funcDef.contains("symbol") ? funcDef["symbol"] : funcDef["name"];
}

// An unrepresentable parameter/return type is recorded as "_unsupported-sig"
// rather than rejected here, so cinclude'ing the header is not itself an
// error -- only calling the function is. A by-value struct parameter or return
// takes the shape of a native `T` one, so a call checks its argument and is
// received the same way; the call then passes a copy (saCallArgs marks it
// "struct-arg") and returns the struct as a Palan function does.
inline void normalizeCFuncSig(json& funcDef) {
	string bad;
	if (funcDef.contains("parameters"))
		for (auto& p : funcDef["parameters"])
			if (p.contains("var-type")) {
				json vt = normalizeCType(p["var-type"]);
				if (bad.empty()) bad = unrepresentableTypeName(vt);
				if (vt.value("type-kind", "") == "struct")
					vt = {{"type-kind","pntr"}, // LCOV_EXCL_BR_LINE
					      {"base-type",{{"type-kind","struct"},{"type-name",vt["type-name"]}}}}; // LCOV_EXCL_BR_LINE
				p["var-type"] = move(vt);
			}
	if (funcDef.contains("ret-type")) {
		json rt = normalizeCType(funcDef["ret-type"]);
		if (bad.empty()) bad = unrepresentableTypeName(rt);
		if (rt.value("type-kind", "") == "struct")
			rt = {{"type-kind","pntr"}, // LCOV_EXCL_BR_LINE
			      {"base-type",{{"type-kind","struct"},{"type-name",rt["type-name"]}}}}; // LCOV_EXCL_BR_LINE
		funcDef["ret-type"] = move(rt);
	}
	if (!bad.empty()) funcDef["_unsupported-sig"] = bad;
} // LCOV_EXCL_BR_LINE

// Symmetric counterpart to normalizeCFuncSig for a single cinclude'd C global
// object declaration (see the "Global variable model" in ASTSpec.md). Same
// deferral policy: an unrepresentable type is recorded as "_unsupported-global"
// rather than rejected here, so cinclude'ing a header with one unsupported
// global is not itself an error -- only referencing that global is (checked
// at reference time by requireSupportedCGlobal).
inline void normalizeCGlobal(json& g) {
	g["var-type"] = normalizeCType(g["var-type"]);
	string bad = unrepresentableTypeName(g["var-type"]);
	if (!bad.empty()) g["_unsupported-global"] = bad;
}
