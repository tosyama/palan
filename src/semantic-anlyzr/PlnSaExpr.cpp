/// Palan Semantic Analyzer — expression analysis
///
/// @file PlnSaExpr.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <iostream>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdint>
#include "PlnSemanticAnalyzer.h"
#include "PlnSaMessage.h"
#include "PlnSaInternal.h"

FieldChain PlnSemanticAnalyzer::resolveObjectChain(const json& obj, bool forWrite)
{
	if (obj.value("expr-type","") == "id") {
		string varName = obj["name"].get<string>();
		const json* vt = lookupVar(varName, obj);
		if (!vt)
			undefinedNameError(obj, obj, E_UndefinedVariable);
		if (!isStructPntr(*vt)) {
			cerr << locPrefix(obj) << PlnSaMessage::getMessage(E_FieldAccessOnNonStruct) << endl;
			exit(1);
		}
		if (forWrite && !isWritableThrough(*vt)) {
			cerr << locPrefix(obj) << PlnSaMessage::getMessage(E_WriteThroughReadOnlyPtr) << endl;
			exit(1);
		}
		return {false, varName, 0, {}, (*vt)["base-type"]["type-name"].get<string>()};
	}
	if (obj.value("expr-type","") == "arr-index") {
		json sa_idx = sa_expr_arr_index(obj, forWrite);
		const json& vt = sa_idx["value-type"];
		if (vt.value("type-kind","") != "pntr" || vt["base-type"].value("type-kind","") != "struct") {
			cerr << locPrefix(obj) << PlnSaMessage::getMessage(E_FieldAccessOnNonStruct) << endl;
			exit(1);
		}
		if (forWrite && !isWritableElem(sa_idx)) {
			cerr << locPrefix(obj) << PlnSaMessage::getMessage(E_WriteThroughReadOnlyPtr) << endl;
			exit(1);
		}
		if (forWrite && !isWritableThrough(vt)) {
			cerr << locPrefix(obj) << PlnSaMessage::getMessage(E_WriteToReadOnlyArrElem) << endl;
			exit(1);
		}
		string struct_name = vt["base-type"]["type-name"].get<string>();
		return {true, "", 0, move(sa_idx), struct_name};
	}
	if (obj.value("expr-type","") != "field-access") {
		json saBase = sa_expression(obj);
		requireOwnedBase(saBase, obj);
		json vt = saBase.value("value-type", json::object());
		if (!isStructPntr(vt)) {
			cerr << locPrefix(obj) << PlnSaMessage::getMessage(E_FieldAccessOnNonStruct) << endl;
			exit(1);
		}
		if (forWrite && !isWritableThrough(vt)) {
			cerr << locPrefix(obj) << PlnSaMessage::getMessage(E_WriteThroughReadOnlyPtr) << endl;
			exit(1);
		}
		string structName = vt["base-type"]["type-name"].get<string>();
		return {true, "", 0, move(saBase), structName};
	}
	FieldChain base = resolveObjectChain(obj["object"], forWrite);
	string fn = obj["field"].get<string>();
	const FieldLayout* it = &findFieldOrExit(base.structName, fn, obj, forWrite);
	if (it->typeKind == "prim" || it->typeKind == "embed-varr" || (it->typeKind == "owned" && !isStructPntr(it->type))) {
		cerr << locPrefix(obj) << PlnSaMessage::getMessage(E_FieldAccessOnNonStruct) << endl;
		exit(1);
	}
	if (forWrite && it->typeKind == "raw-ptr" && !isWritableThrough(it->type)) {
		cerr << locPrefix(obj) << PlnSaMessage::getMessage(E_WriteToImmutablePtrField) << endl;
		exit(1);
	}
	if (it->typeKind == "raw-ptr" && !isStructPntr(it->type)) {
		// A pointer-to-primitive (e.g. `@!int64 p;`) or pointer-to-pointer field
		// has nothing further to chain into -- this hop must be the chain's
		// terminal, typed by the caller via fieldValueType, not
		// resolveObjectChain's own pntr(struct(...)).
		cerr << locPrefix(obj) << PlnSaMessage::getMessage(E_FieldAccessOnNonStruct) << endl;
		exit(1);
	}
	if (it->typeKind == "embed") {
		base.offset += it->offset;
		base.structName = it->typeName;
		return base;
	}
	// LCOV_EXCL_BR_START
	json pntr_type = fieldValueType(*it);
	string structName = pntr_type["base-type"]["type-name"];
	json ptrNode;
	if (!base.isPointerBased)
		ptrNode = {{"expr-type","field-access"},{"var",base.varName},
		           {"offset",base.offset+it->offset},{"value-type",pntr_type}};
	else
		ptrNode = {{"expr-type","field-access"},{"ptr-expr",base.ptrExpr},
		           {"offset",base.offset+it->offset},{"value-type",pntr_type}};
	return {true, "", 0, move(ptrNode), structName};
	// LCOV_EXCL_BR_STOP
} // LCOV_EXCL_BR_LINE

// A call result can be indexed or have its fields accessed only when something
// else owns what it points to: nothing would release an owned struct or array
// a call returns, and a borrow of a temp given to the call outlives that temp.
void PlnSemanticAnalyzer::requireOwnedBase(const json& saBase, const json& locNode)
{
	if (holdsCallTemps(saBase)) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_BorrowOfTempEscapes) << endl;
		exit(1);
	}
	json vt = saBase.value("value-type", json::object());
	if (saBase.value("category", "") == "expiring" && !isPtrBorrow(vt)) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_UnownedCallResult) << endl;
		exit(1);
	}
}

bool PlnSemanticAnalyzer::outsideTypeModule(const string& structName) const
{
	auto origin = typeOrigins_.find(structName);
	return origin != typeOrigins_.end() && origin->second != moduleId_;
}

const FieldLayout& PlnSemanticAnalyzer::findFieldOrExit(const string& structName, const string& fieldName, const json& locNode,
                                                        bool forWrite)
{
	if (structDefs_.at(structName).isOpaque) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_OpaqueField,
			typeDisplayName({{"type-kind", "prim"}, {"type-name", structName}})) << endl; // LCOV_EXCL_BR_LINE
		exit(1);
	}
	const StructDef& def = requireCompleteStruct(structName, locNode);
	auto it = find_if(def.fields.begin(), def.fields.end(), [&](const FieldLayout& f){ return f.name == fieldName; });
	if (it == def.fields.end()) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_UnknownField, structDisplayName(structName), fieldName, def.keyword()) << endl;
		exit(1);
	}
	if (!it->unsupported.empty()) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_UnsupportedCFieldType, structDisplayName(structName), fieldName, it->unsupported) << endl;
		exit(1);
	}
	if ((it->access == "private" || (it->access == "readonly" && forWrite)) && outsideTypeModule(structName)) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(it->access == "private" ? E_PrivateField : E_ReadonlyField,
			PlnSaMessage::writtenTypeName(structName), fieldName) << endl;
		exit(1);
	}
	return *it;
} // LCOV_EXCL_BR_LINE

// `name` must already be a key in structDefs_ (checked by the caller, e.g. via
// isKnownTypeName/count(), or because it's a chain hop whose base type was
// normalized by a producer that already required it -- see resolveObjectChain).
// This only distinguishes "layout not yet known" (an incomplete/opaque struct,
// e.g. C's FILE) from "fully laid out": callers needing the former distinguished
// from "no such struct" must check that separately before calling this.
const StructDef& PlnSemanticAnalyzer::requireCompleteStruct(const string& structName, const json& locNode,
                                                            const string& use)
{
	const StructDef& def = structDefs_[structName];
	if (def.isOpaque) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_OpaqueNeedsLayout,
			typeDisplayName({{"type-kind", "prim"}, {"type-name", structName}}), use) << endl; // LCOV_EXCL_BR_LINE
		exit(1);
	}
	if (!def.isComplete) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_IncompleteStructType, structDisplayName(structName),
		                                                        def.incompleteReason, def.keyword()) << endl;
		exit(1);
	}
	return def;
} // LCOV_EXCL_BR_LINE

const StructDef* PlnSemanticAnalyzer::opaqueDefOf(const json& vt) const
{
	if (!isStructPntr(vt) && !isStructBorrow(vt)) return nullptr;
	auto it = structDefs_.find(vt["base-type"].value("type-name", ""));
	return it != structDefs_.end() && it->second.isOpaque ? &it->second : nullptr;
}

// An owned value between an opaque type and its contents keeps its pointer and
// its owner, which a move then resets; null when neither side is opaque.
json PlnSemanticAnalyzer::opaqueConversion(const json& expr, const json& src, const json& written)
{
	const json& from = src["value-type"];
	json resolved = resolveKnownType(expr, written);
	const StructDef* fromOpaque = isStructStorage(from) ? opaqueDefOf(from) : nullptr;
	const StructDef* toOpaque = nullptr;
	if (isStructType(resolved) && structDefs_.at(resolved["type-name"].get<string>()).isOpaque)
		toOpaque = &structDefs_.at(resolved["type-name"].get<string>());
	// A borrow is cast as any pointer is.
	if ((!fromOpaque && !toOpaque) || resolved.value("type-kind", "") == "pntr") return json();
	bool owned = isOwnedBlock(from);
	const StructDef* def = nullptr;
	json target;
	auto moves = [](const json& f, const json& t) { return moveShapeFit(f, t, false, false) == MoveFit::Match; };
	if (toOpaque && owned && opaqueContentsVisible(*toOpaque) && moves(from, toOpaque->contentsOwned)) {
		def = toOpaque;
		target = toStructPntrType(resolved);
	} else if (fromOpaque && owned && opaqueContentsVisible(*fromOpaque)) {
		json out = ownedTypeOf(expr, resolved);
		if (!out.is_null() && moves(fromOpaque->contentsOwned, out)) {
			def = fromOpaque;
			target = move(out);
		}
	}
	if (!def) {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_OpaqueConvert,
			typeDisplayName({{"type-kind", "prim"}, {"type-name", (toOpaque ? toOpaque : fromOpaque)->name}})) << endl;
		exit(1);
	}
	json conv = wrapConvert(src, target);
	conv["category"] = "opaque-convert";
	return conv;
} // LCOV_EXCL_BR_LINE

bool PlnSemanticAnalyzer::isStructValue(const json& vt) const
{
	return isStructStorage(vt) && !opaqueDefOf(vt);
}

bool PlnSemanticAnalyzer::opaqueContentsVisible(const StructDef& def) const
{
	if (def.contents.is_null()) return false;
	bool visible = true;
	function<void(const json&)> walk = [&](const json& t) {
		if (!t.is_object()) return;
		if (t.value("type-kind", "") == "struct") {
			string name = t["type-name"].get<string>();
			// A C struct is written by its own name wherever its header is.
			bool written = structDefs_.count(name) && !typeOrigins_.count(name);
			for (auto& [w, bound] : typeAliases_)
				written = written || (bound.value("type-kind", "") == "prim" && bound.value("type-name", "") == name);
			visible = visible && written;
		}
		if (t.contains("base-type")) walk(t["base-type"]);
	};
	walk(def.contentsOwned);
	return visible;
}

bool PlnSemanticAnalyzer::holdsOpaque(const json& vt) const
{
	if (!vt.is_object()) return false;
	if (vt.value("type-kind", "") == "struct") {
		auto it = structDefs_.find(vt["type-name"].get<string>());
		if (it == structDefs_.end()) return false;
		if (it->second.isOpaque) return true;
		for (auto& f : it->second.fields) {
			if (f.typeKind == "embed" && holdsOpaque({{"type-kind", "struct"}, {"type-name", f.typeName}}))
				return true;
			if (f.typeKind != "raw-ptr" && holdsOpaque(f.type)) return true;
		}
		return false;
	}
	// A pointer to what it does not own holds nothing.
	if (isPtrBorrow(vt) && !vt.contains("arr-size")) return false;
	return vt.contains("base-type") && holdsOpaque(vt["base-type"]);
} // LCOV_EXCL_BR_LINE

// The same pointer, seen as the contents' owner or, from a borrow, a borrow of them.
json PlnSemanticAnalyzer::asOpaqueContents(json value, const StructDef& def) const
{
	json vt = def.contentsOwned;
	if (value["value-type"].contains("mutable")) vt["mutable"] = value["value-type"]["mutable"];
	return wrapConvert(value, vt);
}

void PlnSemanticAnalyzer::requireSupportedCFuncSig(const json& entry, const string& funcName, const json& locNode)
{
	auto it = entry.find("_unsupported-sig");
	if (it == entry.end()) return;
	cerr << locPrefix(locNode)
	     << PlnSaMessage::getMessage(E_UnsupportedCFuncSignature, funcName, it->get<string>()) << endl;
	exit(1);
}

void PlnSemanticAnalyzer::requireSupportedCGlobal(const json& entry, const string& globalName, const json& locNode)
{
	auto it = entry.find("_unsupported-global");
	if (it == entry.end()) return;
	cerr << locPrefix(locNode)
	     << PlnSaMessage::getMessage(E_UnsupportedCGlobalType, globalName, it->get<string>()) << endl;
	exit(1);
}

// What '@x' / '@!x' borrows a struct or array variable as: the variable is
// already a pointer to its storage, so it is borrowed as it is rather than
// taking the address of its slot. Null for any other variable.
json PlnSemanticAnalyzer::storageBorrowType(const string& name, const json& varType, bool isMutable) const
{
	if (isStructStorage(varType) || isVarrStorage(varType) || isDictStorage(varType)) {
		json vt = varType;
		vt["mutable"] = isMutable;
		return vt;
	}
	// A variadic parameter is sized at run time but owned by no one in the function.
	if (isOwnedVar(name) || varType.contains("arr-size") || varType.value("run-size", false))
		return withArrPermission(varType, isMutable);
	return nullptr;
}

json PlnSemanticAnalyzer::sa_expr_addr_of(const json& expr)
{
	bool isMutable = expr.value("mutable", false);
	const json& obj = expr["object"];
	string obj_type = obj.value("expr-type", "");
	if (isMutable)
		checkRangeChange(expr, obj, RangeChange::Borrow);

	// A literal operand is accepted only as a call argument (sa_lit_arg).
	if (isAggregateLit(obj))
		sa_expression(obj);

	if (obj_type == "id") {
		string name = obj["name"].get<string>();
		const json* varType = lookupVar(name, expr);
		if (varType == nullptr)
			undefinedNameError(expr, obj, E_UndefinedVariable);
		// '@' on a borrow would name its variable's slot; only a primitive
		// pointer has a use for that (a C 'T **' out-param).
		if (isArrBorrowVar(*varType) || isStructBorrow(*varType)) {
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_AddrOfBorrowed, name) << endl;
			exit(1);
		}
		json borrowed = storageBorrowType(name, *varType, isMutable);
		if (!borrowed.is_null()) {
			json out = {{"expr-type","id"},{"name",name},{"var-type",*varType},{"value-type",move(borrowed)}}; // LCOV_EXCL_BR_LINE
			if (expr.contains("loc")) out["loc"] = expr["loc"];
			return out;
		}
		if (isMutable)
			requireAssignable(name, expr);
		if (!isLocalVar(name) && !isStaticBound(name)) {
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_AddrOfNotLocalVar, name) << endl;
			exit(1);
		}
		string vtk = varType->value("type-kind", "");
		bool isPtrToPrim = vtk == "pntr" && (*varType)["base-type"].value("type-kind","") == "prim";
		if (vtk != "prim" && !isPtrToPrim) {
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_AddrOfNotPrimitive, name) << endl;
			exit(1);
		}
		json pntr_type = {{"type-kind", "pntr"}, {"mutable", isMutable}, {"base-type", *varType}}; // LCOV_EXCL_BR_LINE
		json out = {{"expr-type", "addr-of"}, {"name", name}, {"mutable", isMutable}, {"value-type", pntr_type}}; // LCOV_EXCL_BR_LINE
		if (expr.contains("loc")) out["loc"] = expr["loc"];
		return out;
	}

	if (obj_type == "field-access") {
		FieldChain chain = resolveObjectChain(obj["object"], /*forWrite=*/isMutable);
		string fn = obj["field"].get<string>();
		const FieldLayout& fld = findFieldOrExit(chain.structName, fn, obj, isMutable);
		if (fld.bitWidth) {
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_AddrOfBitField, fn) << endl;
			exit(1);
		}
		bool isOwned = fld.typeKind == "owned";
		bool isArrField = (isOwned && isArrLevel(fld.type)) || fld.typeKind == "embed-arr";
		if (fld.typeKind != "prim" && fld.typeKind != "embed" && fld.typeKind != "embed-varr" && !isOwned && !isArrField) {
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_AddrOfNotPrimitive, fn) << endl;
			exit(1);
		}
		// An embed field's own value-type (fieldValueType) is already
		// pntr(struct T) -- the field IS the struct's storage, same as a
		// struct-typed local variable -- so wrapping it again here would
		// produce pntr(pntr(struct T)), a double indirection nothing needs.
		// An owned struct field holds the pointer to its struct, so '@' loads
		// that pointer rather than computing the field's address.
		// A prim field's value-type is the bare prim, so it still needs the
		// usual pntr(...) wrap to become "address of this prim slot".
		// An array field is borrowed as an array, like an array variable: an
		// owned one ([n]T) holds the pointer to its elements, an embedded one
		// is the elements.
		bool addrOnly = !isOwned;
		json pntr_type;
		if (isArrField) {
			pntr_type = withArrPermission(fieldValueType(fld), isMutable);
		} else {
			pntr_type = (fld.typeKind == "prim")
				? json{{"type-kind","pntr"},{"base-type",fieldValueType(fld)}}
				: fieldValueType(fld);
			pntr_type["mutable"] = isMutable;
		}
		int off = chain.offset + fld.offset;
		json out = chain.isPointerBased
			? json{{"expr-type","field-access"},{"ptr-expr",chain.ptrExpr},{"offset",off},{"value-type",pntr_type},{"addr-only",addrOnly}}
			: json{{"expr-type","field-access"},{"var",chain.varName},{"offset",off},{"value-type",pntr_type},{"addr-only",addrOnly}}; // LCOV_EXCL_BR_LINE
		if (expr.contains("loc")) out["loc"] = expr["loc"];
		return out;
	}

	if (obj_type == "arr-index") {
		json sa_idx = sa_expr_arr_index(obj, isMutable);
		// A struct or `[+]T` element or a row is already a pointer to its storage,
		// so '@' only marks it as a borrow, as for a whole array variable. Any other
		// element that is already an address computation or a pointer
		// (pointer-slot element) is rejected -- '@' keeps a single meaning
		// ("make a pointer to a storage slot"), and wrapping those would double
		// the indirection.
		bool isRow = isArrLevel(sa_idx["value-type"]);
		bool isStructElem = !isRow && isOwnedBlock(sa_idx["value-type"]);
		if (!isStructElem && !isRow && (sa_idx.value("addr-only", false)
		                      || sa_idx["value-type"].value("type-kind","") != "prim")) {
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_AddrOfNotPrimitiveElem) << endl;
			exit(1);
		}
		if (isMutable && !isWritableThrough(sa_idx["array"]["value-type"])) {
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_WriteThroughReadOnlyPtr) << endl;
			exit(1);
		}
		if (isStructElem) {
			sa_idx["value-type"]["mutable"] = isMutable;
		} else if (isRow) {
			sa_idx["value-type"] = withArrPermission(sa_idx["value-type"], isMutable);
		} else {
			sa_idx["addr-only"]  = true;
			sa_idx["value-type"] = {{"type-kind","pntr"},{"mutable",isMutable},{"base-type",sa_idx["value-type"]}}; // LCOV_EXCL_BR_LINE
		}
		if (expr.contains("loc")) sa_idx["loc"] = expr["loc"];
		return sa_idx;
	}

	// A call operand is accepted only as a call argument (sa_call_borrow_arg):
	// elsewhere nothing would release the returned struct.
	if (obj.contains("converts-to")) {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_ConvertedLiteralToBorrow, obj["name"]) << endl;
		exit(1);
	}
	if (isCallBorrow(expr) && isExpiringOwned(sa_expression(obj))) {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_ExpiringStructToBorrow) << endl;
		exit(1);
	}
	cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_AddrOfNotAddressable) << endl;
	exit(1);
} // LCOV_EXCL_BR_LINE

void PlnSemanticAnalyzer::checkIntLiteralRange(const json& lit)
{
	const PlnType* t = registry_.fromJson(lit["value-type"]);
	if (!isIntegerPrim(t)) return;
	rejectWideCharLit(lit, lit["value-type"]);
	string value = lit["value"];
	if (intLiteralFits(value, static_cast<const PrimType*>(t)->name)) return;
	cerr << locPrefix(lit) << PlnSaMessage::getMessage(E_IntLiteralOutOfRange,
		value, typeDisplayName(lit["value-type"])) << endl;
	exit(1);
}

// Below the bit-field's own range, C would keep only the low bits of a
// constant, which a literal written for the field almost never means.
void PlnSemanticAnalyzer::checkBitFieldLiteral(const FieldLayout& field, const json& value)
{
	if (value.value("expr-type", "") != "lit-int" && value.value("expr-type", "") != "lit-uint") return;
	string v = value["value"];
	if (intLiteralFitsBits(v, field.bitWidth, field.typeName.rfind("int", 0) == 0)) return;
	cerr << locPrefix(value) << PlnSaMessage::getMessage(E_BitFieldLiteralOutOfRange,
		v, field.name, to_string(field.bitWidth)) << endl;
	exit(1);
}

// A character of 128 or more is never a byte of UTF-8 text, so a narrower
// type would silently hold, or compare against, something no text has.
void PlnSemanticAnalyzer::rejectWideCharLit(const json& lit, const json& type)
{
	if (!lit.value("char", false) || stol(lit["value"].get<string>()) < 128) return; // LCOV_EXCL_BR_LINE
	const PlnType* t = registry_.fromJson(type);
	if (!isIntegerPrim(t)) return;
	auto pn = static_cast<const PrimType*>(t)->name;
	if (pn != PrimType::Name::Int8 && pn != PrimType::Name::Int16 && pn != PrimType::Name::Uint8
	    && pn != PrimType::Name::Uint16 && pn != PrimType::Name::Bool)
		return;
	char cp[16];
	snprintf(cp, sizeof(cp), "U+%04lX", stol(lit["value"].get<string>()));
	cerr << locPrefix(lit) << PlnSaMessage::getMessage(E_WideCharLitNarrowType, cp, typeDisplayName(type)) << endl;
	exit(1);
}

// C's integer promotion, applied to bool and enum only: an operator computes
// in at least int32 so a bool operand never yields a 1-byte result outside
// 0/1 (`b + 1` is 2), and an enum computes as its base type. Other narrow
// types keep their declared width.
// LCOV_EXCL_BR_START
static json promoteOperand(json operand)
{
	const json& vt = operand["value-type"];
	if (vt.value("type-kind", "") != "prim") return operand;
	if (vt.contains("enum")) {
		json base = vt;
		base.erase("enum");
		return wrapConvert(operand, base);
	}
	if (vt.value("type-name", "") != "bool") return operand;
	return wrapConvert(operand, {{"type-kind", "prim"}, {"type-name", "int32"}});
}
// LCOV_EXCL_BR_STOP

static const set<string> foldableOps = {
	"add", "sub", "mul", "div", "mod", "bitand", "bitor", "bitxor", "shl", "shr", "neg", "bitnot", "sizeof"
};

// A folded result is untyped like a source literal, so the context types it
// and checkIntLiteralRange rejects what does not fit there.
json PlnSemanticAnalyzer::foldConstExpr(const json& expr)
{
	string et = expr.value("expr-type", "");
	json lit;
	if (et == "id") {
		string name = expr["name"].get<string>();
		auto cit = constDecls_.find(name);
		if (cit == constDecls_.end()) return expr;
		lit = cit->second;
	} else if (foldableOps.count(et) || et == "call" || et == "chain-call") {
		optional<ConstVal> v = evalConst(expr);
		if (!v) return expr;
		switch (v->kind) {
			case ConstVal::Kind::Int:  lit = {{"expr-type", "lit-int"},  {"value", to_string(v->i)}}; break; // LCOV_EXCL_BR_LINE
			case ConstVal::Kind::Uint: lit = {{"expr-type", "lit-uint"}, {"value", to_string(v->u)}}; break; // LCOV_EXCL_BR_LINE
			case ConstVal::Kind::Flo: {
				char buf[64];
				auto r = to_chars(buf, buf + sizeof(buf), v->f);
				string s(buf, r.ptr);
				if (s.find_first_of(".e") == string::npos) s += ".0";
				lit = {{"expr-type", "lit-flo"}, {"value", s}}; // LCOV_EXCL_BR_LINE
				break;
			}
		}
	} else {
		return expr;
	}
	if (expr.contains("loc")) lit["loc"] = expr["loc"];
	return lit;
} // LCOV_EXCL_BR_LINE

// The array of `len(@a)`, `len(h)` of a borrow h, or `a -> len()`, when it is a
// location. Like sizeof, its indexes are not evaluated: the length depends on
// the type alone, or on the root variable for a length kept at run time.
const json* PlnSemanticAnalyzer::lenOperand(const json& expr)
{
	auto lenCall = [&](const json& call, size_t argc) {
		return call.value("expr-type", "") == "call" && call["name"] == "len"
		    && call["args"].size() == argc && !findVar("len");
	};
	// A borrowed array variable is passed by name, as to a function's parameter.
	auto namesArrBorrow = [&](const json& e) {
		if (e.value("expr-type", "") == "field-access") {
			json vt = knownValueType(e);
			return vt.is_object() && isArrBorrowVar(vt);
		}
		if (e.value("expr-type", "") != "id") return false;
		const json* vt = findVar(e["name"].get<string>());
		return vt && isArrBorrowVar(*vt);
	};
	const json* arr;
	bool borrowed = true;
	if (expr["expr-type"] == "call") {
		if (!lenCall(expr, 1)) return nullptr;
		arr = &expr["args"][0];
		borrowed = arr->value("expr-type", "") == "addr-of" || namesArrBorrow(*arr);
	} else {
		if (!lenCall(expr["call"], 0)) return nullptr;
		arr = &expr["value"];
	}
	const json& given = *arr;
	if (arr->value("expr-type", "") == "addr-of") {
		const json& addrOf = *arr;
		arr = &addrOf["object"];
		if (namesArrBorrow(*arr)) {
			cerr << locPrefix(addrOf) << PlnSaMessage::getMessage(E_AddrOfBorrowed, (*arr)[arr->contains("field") ? "field" : "name"].get<string>()) << endl;
			exit(1);
		}
	}
	auto userLenExists = [&]() {
		vector<Callee> cands = findCallees("len");
		return any_of(cands.begin(), cands.end(), [](const Callee& c) { return !(*c.def).value("inline", false); });
	};
	// A chain's value was judged in its `a -> len()` form already.
	if (arr->value("expr-type", "") == "chained-value") return nullptr;
	if (arr->value("expr-type", "") == "spread") {
		cerr << locPrefix(*arr) << PlnSaMessage::getMessage(E_SpreadNotVariadic, "len") << endl;
		exit(1);
	}
	bool isLocation = isLocationExpr(*arr);
	if (!isLocation && (isAggregateLit(*arr) || userLenExists())) return nullptr;
	json vt = operandValueType(*arr);
	if (isArrLevel(vt) && isLocation) {
		// Without '@' it is a call; one of the user's functions may take the array.
		if (!borrowed) {
			if (userLenExists()) return nullptr;
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_ArrLenNeedsAddrOf) << endl;
			exit(1);
		}
		// A constant length reads nothing, but the variable holds no array.
		checkRootNotGiven(*arr);
		return arr;
	}
	if (isArrLevel(vt) || isVarrPntr(vt) || isDictPntr(vt) || userLenExists()) return nullptr;
	cerr << locPrefix(given) << PlnSaMessage::getMessage(E_LenNotArray, typeDisplayName(vt)) << endl;
	exit(1);
}

// Like sizeof, a constant length needs only the array's type.
optional<int64_t> PlnSemanticAnalyzer::constArrLen(const json& expr)
{
	const json* arr = lenOperand(expr);
	if (!arr) return nullopt;
	json vt = operandValueType(*arr);
	if (vt.value("run-size", false)) return nullopt;
	return constArrSizeOrExit(vt, expr);
}

// A length known only at run time is read where the array keeps it.
json PlnSemanticAnalyzer::runtimeArrLen(const json& expr)
{
	const json* arr = lenOperand(expr);
	if (!arr || !operandValueType(*arr).value("run-size", false)) return json(); // LCOV_EXCL_BR_LINE
	return arrLenOf(sa_expression(*arr), expr);
}

int64_t PlnSemanticAnalyzer::constArrSizeOrExit(const json& arrType, const json& locNode)
{
	if (!arrType.contains("arr-size")) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_ArrLenNotConst) << endl;
		exit(1);
	}
	return arrType["arr-size"].get<int64_t>();
}

// The length `len` gives for an array value.
json PlnSemanticAnalyzer::arrLenOf(const json& saArr, const json& locNode)
{
	const json& vt = saArr["value-type"];
	// A dictionary's header starts with its live count, as a `[+]T`'s with its len.
	if (isVarrPntr(vt) || isDictPntr(vt)) return varrLen(saArr);
	if (vt.value("run-size", false)) {
		const json& location = saArr["expr-type"] == "addr-of" ? saArr["object"] : saArr;
		auto [root, depth] = arrRoot(location);
		return wrapConvert(runtimeLen(*root, depth), {{"type-kind","prim"},{"type-name","int64"}}); // LCOV_EXCL_BR_LINE
	}
	return lenLit(constArrSizeOrExit(vt, locNode));
} // LCOV_EXCL_BR_LINE

// Evaluates in int64, or uint64 once a 'u' operand joins, like the usual
// arithmetic conversions; a C macro constant's own type is dropped here.
// The right operand goes first: a left-associative chain over a variable
// (`a + b + c`) then fails at each level without walking the whole chain.
optional<ConstVal> PlnSemanticAnalyzer::evalConst(const json& expr)
{
	using K = ConstVal::Kind;
	string et = expr.value("expr-type", "");
	auto overflow = [&]() __attribute__((noreturn)) {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_ConstExprOverflow) << endl;
		exit(1);
	};
	if (et == "lit-int" || et == "lit-uint") {
		string text = expr["value"].get<string>();
		ConstVal v{et == "lit-int" ? K::Int : K::Uint};
		const char* first = text.data();
		const char* last = text.data() + text.size();
		bool ok = v.kind == K::Int ? from_chars(first, last, v.i).ec == errc()
		                           : from_chars(first, last, v.u).ec == errc();
		if (!ok) overflow();
		return v;
	}
	if (et == "lit-flo") {
		ConstVal v{K::Flo};
		string text = expr["value"].get<string>();
		from_chars(text.data(), text.data() + text.size(), v.f);
		return v;
	}
	if (et == "sizeof") {
		return ConstVal{K::Uint, 0, (uint64_t)typeByteSize(expr, resolveKnownType(expr, expr["type"]))};
	}
	if (et == "call" || et == "chain-call") {
		optional<int64_t> n = constArrLen(expr);
		if (!n) return nullopt;
		return ConstVal{K::Int, *n};
	}
	if (et == "id") {
		json lit = foldConstExpr(expr);
		if (lit.value("expr-type", "") == "id" || lit.value("expr-type", "") == "lit-str") return nullopt;
		return evalConst(lit);
	}
	if (et == "neg" || et == "bitnot") {
		optional<ConstVal> v = evalConst(expr["operand"]);
		if (!v) return nullopt;
		if (et == "bitnot") {
			if (v->kind == K::Flo) {
				cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_BitwiseOpNotInteger) << endl;
				exit(1);
			}
			v->i = ~v->i;
			v->u = ~v->u;
		} else if (v->kind == K::Flo) {
			v->f = -v->f;
		} else if (v->kind == K::Uint) {
			if (v->u != 0) overflow();
		} else {
			if (__builtin_sub_overflow((int64_t)0, v->i, &v->i)) overflow();
		}
		return v;
	}
	if (!foldableOps.count(et)) return nullopt;

	optional<ConstVal> r = evalConst(expr["right"]);
	if (!r) return nullopt;
	optional<ConstVal> l = evalConst(expr["left"]);
	if (!l) return nullopt;

	bool isShift = et == "shl" || et == "shr";
	bool isBit = et == "bitand" || et == "bitor" || et == "bitxor" || isShift;
	if (isShift && l->kind != K::Flo && r->kind != K::Flo) {
		// Bits shifted out are not an overflow; the count is taken modulo the
		// 64-bit width an untyped constant is computed in.
		int count = (r->kind == K::Int ? (uint64_t)r->i : r->u) & 63;
		uint64_t bits = l->kind == K::Int ? (uint64_t)l->i : l->u;
		bits = et == "shl" ? bits << count : bits >> count;
		l->i = (int64_t)bits;
		l->u = bits;
		return l;
	}
	if (l->kind == K::Flo || r->kind == K::Flo) {
		if (isBit) {
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_BitwiseOpNotInteger) << endl;
			exit(1);
		}
		if (et == "mod") {
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_FloatModulo) << endl;
			exit(1);
		}
		auto toF = [](const ConstVal& v) {
			return v.kind == K::Flo ? v.f : v.kind == K::Uint ? (long double)v.u : (long double)v.i;
		};
		long double a = toF(*l), b = toF(*r);
		ConstVal v{K::Flo};
		v.f = et == "add" ? a + b : et == "sub" ? a - b : et == "mul" ? a * b : a / b;
		if (!isfinite(v.f)) {
			if (et == "div" && b == 0) {
				cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_ConstExprDivByZero) << endl;
				exit(1);
			}
			overflow();
		}
		return v;
	}

	if ((et == "div" || et == "mod") && (r->kind == K::Int ? r->i == 0 : r->u == 0)) {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_ConstExprDivByZero) << endl;
		exit(1);
	}
	if (l->kind == K::Uint || r->kind == K::Uint) {
		for (ConstVal* v : {&*l, &*r}) {
			if (v->kind != K::Int) continue;
			if (v->i < 0) overflow();
			v->u = v->i;
		}
		uint64_t a = l->u, b = r->u;
		ConstVal v{K::Uint};
		if      (et == "add") { if (__builtin_add_overflow(a, b, &v.u)) overflow(); }
		else if (et == "sub") { if (__builtin_sub_overflow(a, b, &v.u)) overflow(); }
		else if (et == "mul") { if (__builtin_mul_overflow(a, b, &v.u)) overflow(); }
		else if (et == "div") v.u = a / b;
		else if (et == "mod") v.u = a % b;
		else if (et == "bitand") v.u = a & b;
		else if (et == "bitor")  v.u = a | b;
		else                     v.u = a ^ b;
		return v;
	}
	int64_t a = l->i, b = r->i;
	ConstVal v{K::Int};
	if      (et == "add") { if (__builtin_add_overflow(a, b, &v.i)) overflow(); }
	else if (et == "sub") { if (__builtin_sub_overflow(a, b, &v.i)) overflow(); }
	else if (et == "mul") { if (__builtin_mul_overflow(a, b, &v.i)) overflow(); }
	else if (et == "div") { if (a == INT64_MIN && b == -1) overflow(); v.i = a / b; }
	else if (et == "mod") v.i = b == -1 ? 0 : a % b;
	else if (et == "bitand") v.i = a & b;
	else if (et == "bitor")  v.i = a | b;
	else                     v.i = a ^ b;
	return v;
}

json PlnSemanticAnalyzer::sa_expression(const json &rawExpr, const PlnType* expectedType)
{
	json sa = sa_expr_maybe_void(rawExpr, expectedType);
	if (!sa.contains("value-type")) {
		cerr << locPrefix(rawExpr) << PlnSaMessage::getMessage(E_VoidCallUsedAsValue) << endl;
		exit(1);
	}
	return sa;
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

// '->' binds tighter than a comparison or a logical operator, so code written
// to store such a result (`x < y -> b`) now nests an assignment in it.
void PlnSemanticAnalyzer::rejectArrowOperand(const json& expr, const string& op) const
{
	if (!expr["right"].contains("stmt-type")) return;
	cerr << locPrefix(expr["right"]) << PlnSaMessage::getMessage(E_ArrowBindsTighter, op) << endl;
	exit(1);
}

json PlnSemanticAnalyzer::sa_expr_maybe_void(const json &rawExpr, const PlnType* expectedType)
{
	if (rawExpr.contains("stmt-type")) {
		cerr << locPrefix(rawExpr) << PlnSaMessage::getMessage(E_AssignInExpr) << endl;
		exit(1);
	}
	if (rawExpr.value("expr-type", "") == "pinned")
		return rawExpr["sa"];
	const json expr = foldConstExpr(rawExpr);
	json sa_expr = expr;
	string expr_type = expr["expr-type"];
	// A literal bound to an enum takes the base type, so the binding still
	// rejects it as needing Name(x) instead of silently labeling it.
	if (expectedType && expectedType->kind == PlnType::Kind::Enum)
		expectedType = static_cast<const EnumType*>(expectedType)->base;

	if (expr_type == "arr-lit") {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_ArrLitContext) << endl;
		exit(1);
	}
	if (expr_type == "dict-lit") {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_DictLitContext) << endl;
		exit(1);
	}
	if (expr_type == "not-impl") {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_ExprNotImplemented) << endl;
		exit(1);
	}

	if (expr_type == "lit-int") {
		// A lit-int already carrying a value-type is a macro constant folded
		// in by gen-ast (a plain source literal never has one) -- its type
		// was fixed by the C declaration, not by this expression's context,
		// so expectedType must not override it.
		if (expr.contains("value-type")) {
			resolveCTypeRefs(sa_expr["value-type"]);
		} else {
			// A character literal without a type to take is an int, as in C.
			if (expectedType && expectedType->kind == PlnType::Kind::Prim)
				sa_expr["value-type"] = registry_.toJson(expectedType);
			else
				sa_expr["value-type"] = registry_.toJson(registry_.prim(
					expr.value("char", false) ? PrimType::Name::Int32 : PrimType::Name::Int64));
			// A float constant has exactly one form, lit-flo, so codegen never
			// sees an integer literal that must be materialized as a float.
			if (isFloatPrim(expectedType))
				sa_expr["expr-type"] = "lit-flo";
			else
				checkIntLiteralRange(sa_expr);
		}

	} else if (expr_type == "lit-uint") {
		if (expectedType && expectedType->kind == PlnType::Kind::Prim) {
			auto pn = static_cast<const PrimType*>(expectedType)->name;
			if (pn == PrimType::Name::Uint8  || pn == PrimType::Name::Uint16 ||
			    pn == PrimType::Name::Uint32 || pn == PrimType::Name::Uint64 ||
			    pn == PrimType::Name::Bool)
				sa_expr["value-type"] = registry_.toJson(expectedType);
			else
				sa_expr["value-type"] = registry_.toJson(registry_.prim(PrimType::Name::Uint64));
		} else {
			sa_expr["value-type"] = registry_.toJson(registry_.prim(PrimType::Name::Uint64));
		}
		checkIntLiteralRange(sa_expr);

	} else if (expr_type == "lit-flo") {
		if (isFloatPrim(expectedType)) {
			sa_expr["value-type"] = registry_.toJson(expectedType);
		} else {
			sa_expr["value-type"] = registry_.toJson(registry_.prim(PrimType::Name::Float64));
		}

	} else if (expr_type == "lit-str") {
		string value = expr["value"];
		if (!strLiteralLabels.count(value)) {
			string label = ".str" + to_string(strLiteralLabels.size());
			strLiteralLabels[value] = label;
			sa["str-literals"].push_back({{"label", label}, {"value", value}}); // LCOV_EXCL_BR_LINE
		}
		sa_expr["label"] = strLiteralLabels[value];
		sa_expr.erase("value");
		sa_expr["value-type"] = strLitType();

	} else if (expr_type == "id") {
		const string& name = expr["name"].get_ref<const string&>();
		const json* varType = expr.contains("c-name") ? lookupCGlobal(name, expr) : lookupVar(name, expr);
		if (varType != nullptr) {
			sa_expr["var-type"]   = *varType;
			sa_expr["value-type"] = *varType;
			if (isOwnedVar(expr["name"].get<string>()))
				sa_expr["category"] = "owned";
		} else {
			json ref = sa_func_ref(expr, expectedType);
			if (!ref.is_null()) return ref;
			undefinedNameError(expr, expr, E_UndefinedVariable);
		}

	} else if (expr_type == "addr-of") {
		return sa_expr_addr_of(expr);

	} else if (expr_type == "add" || expr_type == "sub"
	        || expr_type == "mul" || expr_type == "div" || expr_type == "mod"
	        || expr_type == "bitand" || expr_type == "bitor" || expr_type == "bitxor") {
		return sa_expr_arith(expr, expectedType);

	} else if (expr_type == "shl" || expr_type == "shr") {
		return sa_expr_shift(expr, expectedType);

	} else if (expr_type == "append") {
		return sa_expr_append(expr);

	} else if (expr_type == "neg") {
		json operand = sa_expression(expr["operand"], expectedType);
		if (isStructPntr(operand["value-type"]))
			return sa_operator_call(expr, "-", {&expr["operand"]}, {move(operand)}); // LCOV_EXCL_BR_LINE
		operand = promoteOperand(move(operand));
		sa_expr["operand"]    = operand;
		sa_expr["value-type"] = operand["value-type"];

	} else if (expr_type == "bitnot") {
		json operand = promoteOperand(sa_expression(expr["operand"], expectedType));
		const PlnType* t = registry_.fromJson(operand["value-type"]);
		if (!isIntegerPrim(t)) {
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_BitwiseOpNotInteger) << endl;
			exit(1);
		}
		sa_expr["operand"]    = operand;
		sa_expr["value-type"] = operand["value-type"];

	} else if (expr_type == "cmp") {
		return sa_expr_cmp(expr);

	} else if (expr_type == "logical-and" || expr_type == "logical-or") {
		rejectArrowOperand(expr, expr_type == "logical-and" ? "&&" : "||");
		json left  = sa_expression(expr["left"]);
		json right = sa_expression(expr["right"]);
		for (const json* op : {&left, &right}) {
			const PlnType* t = registry_.fromJson((*op)["value-type"]);
			if (!isIntegerPrim(t)) {
				cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_LogicalOpNotInteger) << endl;
				exit(1);
			}
		}
		sa_expr["left"]       = left;
		sa_expr["right"]      = right;
		sa_expr["value-type"] = {{"type-kind", "prim"}, {"type-name", "bool"}}; // LCOV_EXCL_BR_LINE

	} else if (expr_type == "logical-not") {
		json operand = sa_expression(expr["operand"]);
		const PlnType* t = registry_.fromJson(operand["value-type"]);
		if (!isIntegerPrim(t)) {
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_LogicalOpNotInteger) << endl;
			exit(1);
		}
		sa_expr["operand"]    = operand;
		sa_expr["value-type"] = {{"type-kind", "prim"}, {"type-name", "bool"}}; // LCOV_EXCL_BR_LINE

	} else if (expr_type == "cast") {
		return sa_expr_cast(expr, false);

	} else if (expr_type == "call") {
		if (expr.contains("converts-to")) return sa_expr_call(expr);
		const string& name = expr["name"].get_ref<const string&>();
		if (expr.contains("c-name")) {
			json call = expr;
			call.erase("c-name");
			return makeCall(call, usableCallees(call, findCCallees(name)), name);
		}
		if (!expr.contains("iterates")) {
			json expanded = expandedCall(expr, "", name);
			if (!expanded.is_null()) return sa_expr_maybe_void(expanded, expectedType);
		}
		json len = runtimeArrLen(expr);
		if (!len.is_null()) return len;
		const string& callName = expr["name"].get<string>();
		if (findCallees(callName).empty()) {
			json cast = aliasCast(expr, callName);
			if (!cast.is_null()) return cast;
		}
		return sa_expr_call(expr);

	} else if (expr_type == "member-call") {
		string qualified = typeNameWritten(expr["object"]);
		if (!qualified.empty()) {
			json cast = aliasCast(expr, qualified + "." + expr["method"].get<string>());
			if (!cast.is_null()) return cast;
		}
		json field = funcPtrField(expr);
		if (!field.is_null())
			return sa_expr_call_ptr(expr, sa_expression(field), expr["method"].get<string>());
		if (!isMethodForm(expr)) {
			json expanded = expandedCall(expr, expr["object"]["name"].get<string>(), expr["method"].get<string>());
			if (!expanded.is_null()) return sa_expr_maybe_void(expanded, expectedType);
		}
		json call = normalizeMethodCall(expr);
		return call["expr-type"] == "call" ? sa_expr_call(call) : sa_expr_member_call(expr);

	} else if (expr_type == "call-ptr") {
		json callee = sa_expression(expr["callee"]);
		if (!isFuncPtr(callee["value-type"])) {
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_CallNonFuncPtr,
				typeDisplayName(callee["value-type"])) << endl;
			exit(1);
		}
		const json* named = &expr["callee"];
		while ((*named)["expr-type"] == "cast") named = &(*named)["src"];
		string funcName = (*named)["expr-type"] == "id" ? (*named)["name"].get<string>() : "function pointer";
		return sa_expr_call_ptr(expr, move(callee), funcName);

	} else if (expr_type == "chain-call") {
		json len = runtimeArrLen(expr);
		if (!len.is_null()) return len;
		return sa_expr_chain_call(expr);

	} else if (expr_type == "transfer") {
		return sa_expr_transfer(expr);

	} else if (expr_type == "spread") {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_SpreadPlace) << endl;
		exit(1);

	} else if (expr_type == "field-access") {
		json enumerator = resolveEnumerator(expr);
		if (!enumerator.is_null()) return enumerator;
		return sa_expr_field_access(expr, /*forWrite=*/false);

	} else if (expr_type == "arr-index") {
		return sa_expr_arr_index(expr);
	}
	return sa_expr;
} // LCOV_EXCL_BR_LINE

// The count does not join the usual arithmetic conversions, so the result
// keeps the left operand's width. Converted to that type, the count keeps the
// low bits that a count modulo the width needs.
json PlnSemanticAnalyzer::sa_expr_shift(const json& expr, const PlnType* expectedType)
{
	if (isBoolPrim(expectedType)) expectedType = registry_.prim(PrimType::Name::Int32);
	json left  = promoteOperand(sa_expression(expr["left"], expectedType));
	json right = promoteOperand(sa_expression(expr["right"]));
	const PlnType* leftType  = registry_.fromJson(left["value-type"]);
	const PlnType* rightType = registry_.fromJson(right["value-type"]);
	if (!isIntegerPrim(leftType) || !isIntegerPrim(rightType)) {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_BitwiseOpNotInteger) << endl;
		exit(1);
	}
	if (rightType != leftType) right = wrapConvert(right, left["value-type"]);
	json sa_expr = expr;
	sa_expr["left"]       = left;
	sa_expr["right"]      = right;
	sa_expr["value-type"] = left["value-type"];
	return sa_expr;
} // LCOV_EXCL_BR_LINE

static const set<string> arithOps = {"add", "sub", "mul", "div", "mod", "bitand", "bitor", "bitxor"};

// Whether expectedType changes what sa_expression makes of `rawExpr`: it must
// follow how sa_expr_maybe_void, sa_expr_arith and sa_expr_shift pass it down
// to a literal.
bool PlnSemanticAnalyzer::takesExpectedType(const json& rawExpr)
{
	const json expr = foldConstExpr(rawExpr);
	string et = expr.value("expr-type", "");
	if (et == "lit-int") return !expr.contains("value-type");
	if (et == "lit-uint" || et == "lit-flo") return true;
	// Over a struct, an operator is an operator function's call, whose
	// parameters type its literals.
	auto structOperand = [&](const json& e) {
		json t = knownValueType(e);
		return t.is_object() && isStructPntr(t);
	};
	if (et == "neg") return !structOperand(expr["operand"]) && takesExpectedType(expr["operand"]);
	if (et == "bitnot") return takesExpectedType(expr["operand"]);
	if (et == "shl" || et == "shr") return takesExpectedType(expr["left"]);
	if (arithOps.count(et)) {
		if (structOperand(expr["left"]) || structOperand(expr["right"])) return false;
		// A lit-int operand takes the other operand's type instead.
		if (foldConstExpr(expr["left"])["expr-type"] == "lit-int") return takesExpectedType(expr["right"]);
		if (foldConstExpr(expr["right"])["expr-type"] == "lit-int") return takesExpectedType(expr["left"]);
		return takesExpectedType(expr["left"]) || takesExpectedType(expr["right"]);
	}
	return false;
}

// The type of a variable, of a field reached from one, or of a call whose
// functions all return one type or whose arguments are all of these, told
// without analyzing the expression; null for anything else, or where
// analyzing it would report an error. A call with a literal argument stays
// unknown: the literal's type waits for the enclosing call's choice.
json PlnSemanticAnalyzer::knownValueType(const json& expr)
{
	string et = expr.value("expr-type", "");
	if (et == "id") {
		const json* vt = findVar(expr["name"].get<string>());
		return vt ? *vt : json();
	}
	if (et == "field-access") {
		json objType = knownValueType(expr["object"]);
		if (!objType.is_object() || !isStructPntr(objType)) return json();
		auto def = structDefs_.find(objType["base-type"]["type-name"].get<string>());
		if (def == structDefs_.end() || !def->second.isComplete) return json();
		for (auto& f : def->second.fields)
			if (f.name == expr["field"].get<string>() && f.unsupported.empty()) return fieldValueType(f);
		return json();
	}
	if (et == "call") {
		const string& name = expr["name"].get<string>();
		vector<Callee> cands = findCallees(name);
		json ret;
		bool oneType = true;
		for (auto& c : cands) {
			const json* r = c.def->contains("ret-type") ? &(*c.def)["ret-type"] : nullptr;
			if (!r || (!ret.is_null() && ret != *r)) { oneType = false; break; }
			ret = *r;
		} // LCOV_EXCL_BR_LINE
		if (oneType) return ret;
		vector<CallArg> args;
		for (auto& a : expr.value("args", json::array())) {
			string aet = a.value("expr-type", "");
			if (aet == "chained-value" || isAggregateLit(a) || namesOverloadedFunc(a) || takesExpectedType(a))
				return json();
			json vt = knownValueType(a);
			if (vt.is_null()) return json();
			// What callArgMismatch reads of an analyzed argument.
			json peeked = {{"expr-type", aet}, {"value-type", vt}}; // LCOV_EXCL_BR_LINE
			if (aet == "id") peeked["var-type"] = vt;
			args.push_back({a, false, move(peeked)});
		} // LCOV_EXCL_BR_LINE
		CalleeChoice choice = chooseCallee(name, cands, args);
		if (!choice.chosen) return json();
		return choice.chosen->def->value("ret-type", json());
	}
	return json();
} // LCOV_EXCL_BR_LINE

// Literals and variables under the operators takesExpectedType follows:
// analyzing it has no effect but its type.
bool PlnSemanticAnalyzer::isPureOperand(const json& rawExpr)
{
	const json expr = foldConstExpr(rawExpr);
	string et = expr.value("expr-type", "");
	if (et == "lit-int" || et == "lit-uint" || et == "lit-flo") return true;
	if (et == "id") return findVar(expr["name"].get<string>()) != nullptr;
	if (et == "neg" || et == "bitnot") return isPureOperand(expr["operand"]);
	if (et == "shl" || et == "shr" || arithOps.count(et))
		return isPureOperand(expr["left"]) && isPureOperand(expr["right"]);
	return false;
}

// The type an argument takesExpectedType accepts has with no parameter to give
// it one, which is the type it matches exactly; null when finding it would
// take evaluating more than literals and variables.
json PlnSemanticAnalyzer::literalDefaultType(const json& rawExpr)
{
	const json expr = foldConstExpr(rawExpr);
	string et = expr.value("expr-type", "");
	if (et == "lit-int") return registry_.toJson(registry_.prim(PrimType::Name::Int64));
	if (et == "lit-uint") return registry_.toJson(registry_.prim(PrimType::Name::Uint64));
	if (et == "lit-flo") return registry_.toJson(registry_.prim(PrimType::Name::Float64));
	if (!isPureOperand(expr)) return json();
	return sa_expression(expr)["value-type"];
}

// A struct operand makes a comparison a call of the type's '==' or '<'; a
// comparison with NULL stays a pointer comparison, which the generated
// allocators and C interop code rely on.
json PlnSemanticAnalyzer::sa_expr_cmp(const json& expr)
{
	rejectArrowOperand(expr, expr["op"].get<string>());
	json probes[2];
	json folded[2] = {foldConstExpr(expr["left"]), foldConstExpr(expr["right"])}; // LCOV_EXCL_BR_LINE
	bool structOperand = false, nullOperand = isNullPtrLit(folded[0]) || isNullPtrLit(folded[1]);
	string op = expr["op"];
	for (int i : {0, 1}) {
		const json& operand = folded[i];
		if (isAggregateLit(operand) || operand.value("expr-type", "") == "lit-int") continue; // LCOV_EXCL_BR_LINE
		// Whether a variable is NULL may be asked after it gave its value away.
		bool saved = nullCompare_;
		nullCompare_ = nullOperand && (op == "==" || op == "!=") && operand.value("expr-type", "") == "id";
		probes[i] = sa_expression(expr[i ? "right" : "left"]);
		nullCompare_ = saved;
		structOperand = structOperand || isStructPntr(probes[i]["value-type"]);
	}
	if (structOperand && !nullOperand)
		return sa_compare_call(expr, probes);

	json left  = probes[0].is_null() ? sa_expression(expr["left"]) : move(probes[0]);
	json right = probes[1].is_null() ? sa_expression(expr["right"]) : move(probes[1]);
	// A literal here takes no type from the other operand, so a narrow one
	// is checked against the literal directly.
	rejectWideCharLit(left, right["value-type"]);
	rejectWideCharLit(right, left["value-type"]);
	left  = promoteOperand(move(left));
	right = promoteOperand(move(right));
	const PlnType* leftType  = registry_.fromJson(left["value-type"]);
	const PlnType* rightType = registry_.fromJson(right["value-type"]);
	// A pointer pair (usualArithConv returns nullptr) is left unconverted --
	// pointer comparison (`p == NULL`, `p == q`) has no common numeric type
	// to convert to.
	const PlnType* promoted = usualArithConv(leftType, rightType);
	if (promoted) {
		if (leftType != promoted)  left  = wrapConvert(left,  registry_.toJson(promoted));
		if (rightType != promoted) right = wrapConvert(right, registry_.toJson(promoted));
	}
	json sa_expr = expr;
	sa_expr["left"]       = left;
	sa_expr["right"]      = right;
	sa_expr["value-type"] = {{"type-kind", "prim"}, {"type-name", "bool"}}; // LCOV_EXCL_BR_LINE
	return sa_expr;
} // LCOV_EXCL_BR_LINE

json PlnSemanticAnalyzer::sa_expr_arith(const json& expr, const PlnType* expectedType)
{
	json sa_expr = expr;
	string expr_type = expr["expr-type"];

	// A lit-int operand takes the other operand's type, which takes
	// precedence over expectedType (e.g. `int64_var + 4` keeps int64); only
	// when both are literals does expectedType decide (`int32 x = (4+4)`).
	// Each literal is typed exactly once so its range check never fires on
	// a provisional type.
	if (isBoolPrim(expectedType)) expectedType = registry_.prim(PrimType::Name::Int32);

	// Every operand but a lit-int is analyzed with expectedType as below, so
	// it is analyzed first, to learn whether a struct makes this an operator
	// function call. There, an operand that is no struct and takes the type
	// it is given is typed by the parameter instead.
	static const map<string, string> operators = {
		{"add", "+"}, {"sub", "-"}, {"mul", "*"}, {"div", "/"}, {"mod", "%"},
	}; // LCOV_EXCL_BR_LINE
	json probes[2];
	auto op = operators.find(expr_type);
	if (op != operators.end()) {
		bool structOperand = false;
		for (int i : {0, 1}) {
			const json& operand = expr[i ? "right" : "left"];
			if (isAggregateLit(operand) || foldConstExpr(operand)["expr-type"] == "lit-int") continue;
			probes[i] = sa_expression(operand, expectedType);
			structOperand = structOperand || isStructPntr(probes[i]["value-type"]);
		}
		if (structOperand) {
			for (int i : {0, 1})
				if (!probes[i].is_null() && !isStructPntr(probes[i]["value-type"])
				    && takesExpectedType(expr[i ? "right" : "left"]))
					probes[i] = json();
			return sa_operator_call(expr, op->second, {&expr["left"], &expr["right"]},
			                        {move(probes[0]), move(probes[1])}); // LCOV_EXCL_BR_LINE
		}
	}
	auto operand = [&](int i, const PlnType* type) {
		return promoteOperand(probes[i].is_null() ? sa_expression(expr[i ? "right" : "left"], type)
		                                          : move(probes[i]));
	};

	json left, right;
	auto typeOf = [&](const json& other) {
		return other.contains("value-type") ? registry_.fromJson(other["value-type"]) : expectedType;
	};
	if (foldConstExpr(expr["left"])["expr-type"] == "lit-int") {
		right = operand(1, expectedType);
		left  = operand(0, typeOf(right));
	} else if (foldConstExpr(expr["right"])["expr-type"] == "lit-int") {
		left  = operand(0, expectedType);
		right = operand(1, typeOf(left));
	} else {
		left  = operand(0, expectedType);
		right = operand(1, expectedType);
	}
	const PlnType* leftType  = registry_.fromJson(left["value-type"]);
	const PlnType* rightType = registry_.fromJson(right["value-type"]);
	if (expr_type == "bitand" || expr_type == "bitor" || expr_type == "bitxor") {
		// Checked on the operands themselves, not the post-promotion type: a
		// pointer/struct pair is Incompatible and would otherwise reach the
		// generic E_ArithOpNotNumeric below with a less specific message.
		for (const PlnType* t : {leftType, rightType}) {
			if (!isIntegerPrim(t)) {
				cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_BitwiseOpNotInteger) << endl;
				exit(1);
			}
		}
	}
	// Usual arithmetic conversions: both operands convert to one common type.
	// A pointer/struct operand (usualArithConv returns nullptr) is rejected --
	// Palan has no pointer arithmetic syntax, so `p + 1` has no meaning here.
	const PlnType* promoted = usualArithConv(leftType, rightType);
	if (!promoted) {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_ArithOpNotNumeric) << endl;
		exit(1);
	}
	if (leftType != promoted)  left  = wrapConvert(left,  registry_.toJson(promoted));
	if (rightType != promoted) right = wrapConvert(right, registry_.toJson(promoted));
	if (expr_type == "mod" && isFloatPrim(promoted)) {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_FloatModulo) << endl;
		exit(1);
	}
	sa_expr["left"]       = left;
	sa_expr["right"]      = right;
	sa_expr["value-type"] = registry_.toJson(promoted);
	return sa_expr;
} // LCOV_EXCL_BR_LINE

// Shared narrowing rule for every binding site. An integer/uint literal is
// exempt from the ExplicitCast rejection below, since its width/sign was
// never fixed by source syntax; pointer mutability is checked separately by
// each call site via checkPtrPermission.
json PlnSemanticAnalyzer::convertForBinding(const json& locNode, json value,
		const PlnType* toType, const json& toTypeJson)
{
	noteEscape(value);
	if (takesVarrData(value["value-type"], &toTypeJson, false)) value = varrData(move(value));
	if (value["value-type"] == toTypeJson) return value;
	const PlnType* fromType = registry_.fromJson(value["value-type"]);
	TypeCompat compat = typeCompat(fromType, toType, registry_);
	if (compat == TypeCompat::ImplicitWiden) {
		value = wrapConvert(value, toTypeJson);
	} else if (compat == TypeCompat::ExplicitCast) {
		string et = value["expr-type"];
		bool untypedLit = (et == "lit-int" || et == "lit-uint") && fromType->kind != PlnType::Kind::Enum;
		if (!untypedLit || toType->kind == PlnType::Kind::Enum) {
			cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_InvalidNarrowingConv,
				typeDisplayName(value["value-type"]), typeDisplayName(toTypeJson)) << endl;
			exit(1);
		}
	} else if (compat == TypeCompat::Incompatible) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_IncompatibleTypes,
			typeDisplayName(value["value-type"]), typeDisplayName(toTypeJson)) << endl;
		exit(1);
	}
	return value;
}

optional<SaDiag> PlnSemanticAnalyzer::ptrPermissionMismatch(const json& from, const json& to)
{
	switch (ptrPermission(from, to)) {
	case PtrPermission::Ok:
		break;
	case PtrPermission::Upgrade:
		return SaDiag{E_PtrMutabilityUpgrade}; // LCOV_EXCL_BR_LINE
	case PtrPermission::InnerNarrowing:
		return SaDiag{E_PtrInnerPermissionNarrowing, typeDisplayName(from), typeDisplayName(to)}; // LCOV_EXCL_BR_LINE
	case PtrPermission::FuncSigMismatch:
		return SaDiag{E_IncompatibleTypes, typeDisplayName(from), typeDisplayName(to)}; // LCOV_EXCL_BR_LINE
	}
	return nullopt;
}

void PlnSemanticAnalyzer::checkPtrPermission(const json& locNode, const json& from, const json& to)
{
	if (auto d = ptrPermissionMismatch(from, to)) reportDiag(locNode, *d);
}

// A struct returned by a call is owned by nobody, and a borrow never frees
// what it points to.
void PlnSemanticAnalyzer::checkBorrowSource(const json& locNode, const json& saValue,
		const json& dstType)
{
	if (holdsCallTemps(saValue)) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_BorrowOfTempEscapes) << endl;
		exit(1);
	}
	if (isVarrPntr(dstType) && isPtrBorrow(dstType) && isVarrStorage(saValue["value-type"])) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_VarrBorrowNeedsAddrOf) << endl;
		exit(1);
	}
	if (isDictPntr(dstType) && isPtrBorrow(dstType) && isDictStorage(saValue["value-type"])) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_DictBorrowNeedsAddrOf) << endl;
		exit(1);
	}
	if (isPtrBorrow(dstType) && namesArrStorage(saValue)) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_ArrBorrowNeedsAddrOf) << endl;
		exit(1);
	}
	if (!isStructBorrow(dstType) || !isStructStorage(saValue["value-type"]))
		return;
	if (isExpiringOwned(saValue)) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_ExpiringStructToBorrow) << endl;
		exit(1);
	}
	cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_StructBorrowNeedsAddrOf) << endl;
	exit(1);
}

json PlnSemanticAnalyzer::enumTypeNamed(const string& name) const
{
	const json* t = findTypeBinding(name);
	return t && t->contains("enum") ? *t : json();
} // LCOV_EXCL_BR_LINE

// C's sizeof of the type as Palan lays it out: an array holds its elements
// inline, but only a '$' element is stored in place -- a struct or row
// element without '$' is a pointer slot.
int64_t PlnSemanticAnalyzer::typeByteSize(const json& locNode, json t)
{
	string tk = t.value("type-kind", "");
	if (tk == "pntr")
		return 8;
	if (tk == "embed") {
		json base = t["base-type"];
		if (base.value("type-kind", "") == "prim" && !isKnownTypeName(base.value("type-name", ""))) {
			cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_UnknownType, base.value("type-name", "")) << endl;
			exit(1);
		}
		if (!isStructType(base)) {
			cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_EmbedElemNotStruct, typeDisplayName(base)) << endl;
			exit(1);
		}
		return typeByteSize(locNode, base);
	}
	if (tk == "arr") {
		int64_t n = t["size-expr"].is_null() ? -1 : constArrSize(sa_arr_size_expr(locNode, t["size-expr"]));
		if (n < 0) {
			cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_SizeofArrSizeNotConst) << endl;
			exit(1);
		}
		json elem = t["base-type"];
		bool inPlace = t.value("embedded", false)
		            || (elem.value("type-kind", "") == "prim" && !isStructType(elem));
		return n * (inPlace ? typeByteSize(locNode, elem) : 8);
	}
	string name = t["type-name"].get<string>();
	if (structDefs_.count(name))
		return requireCompleteStruct(name, locNode, "'sizeof'").totalSize;
	return elemSizeBytes(name);
}

// `Name.X` where Name is an enum type (or an alias of one) not shadowed by a
// variable; null when the field access is an ordinary one.
// The type name `T` or `V.T` an expression spells, if it names no variable;
// empty otherwise.
string PlnSemanticAnalyzer::typeNameWritten(const json& obj) const
{
	string et = obj.value("expr-type", "");
	if (et == "field-access") {
		string qualifier = typeNameWritten(obj["object"]);
		return qualifier.empty() || qualifier.find('.') != string::npos
		       ? "" : qualifier + "." + obj["field"].get<string>();
	}
	if (et != "id") return "";
	string name = obj["name"].get<string>();
	return findVar(name) || findCGlobal(name) ? "" : name;
}

// gen-ast only knows the prim keywords as cast targets; an alias (an enum
// included) is a type only SA can see, so its cast arrives shaped as a call.
// Null when `typeName` is no alias.
json PlnSemanticAnalyzer::aliasCast(const json& expr, const string& typeName)
{
	json written = {{"type-kind", "prim"}, {"type-name", typeName}}; // LCOV_EXCL_BR_LINE
	requireWritableTypeName(expr, typeName);
	const json* target = findTypeBinding(typeName);
	if (!target || *target == written) return json();
	if (expr["args"].size() != 1) {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_CastArgCount, typeName) << endl;
		exit(1);
	}
	json cast = {{"expr-type", "cast"}, {"target-type", written}, {"src", expr["args"][0]}}; // LCOV_EXCL_BR_LINE
	if (expr.contains("loc")) cast["loc"] = expr["loc"];
	return sa_expression(cast);
} // LCOV_EXCL_BR_LINE

// `_` in a function type keeps the type the cast's source has at that place.
static bool hasKeptType(const json& t)
{
	if (t.value("type-kind", "") == "keep") return true;
	if (t.contains("base-type") && t["base-type"].is_object()) return hasKeptType(t["base-type"]);
	if (t.value("type-kind", "") != "func") return false;
	for (auto& p : t["parameters"])
		if (hasKeptType(p["var-type"])) return true;
	return hasKeptType(t["ret-type"]);
}

json PlnSemanticAnalyzer::fillKeptTypes(const json& locNode, json target, const json& from, const json& srcType)
{
	if (target.value("type-kind", "") == "keep") return from;
	if (!hasKeptType(target)) return target;
	string tk = target.value("type-kind", "");
	if (tk == "pntr" && from.value("type-kind", "") == "pntr") {
		target["base-type"] = fillKeptTypes(locNode, move(target["base-type"]), from["base-type"], from);
		return target;
	}
	if (tk != "func" || from.value("type-kind", "") != "func") {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_KeepWithoutSource, typeDisplayName(srcType)) << endl;
		exit(1);
	}
	if (target["parameters"].size() != from["parameters"].size()) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_KeepParamCount,
			to_string(target["parameters"].size()), typeDisplayName(srcType),
			to_string(from["parameters"].size())) << endl;
		exit(1);
	}
	for (size_t i = 0; i < target["parameters"].size(); i++) {
		const json& fp = from["parameters"][i]["var-type"];
		target["parameters"][i]["var-type"] = fillKeptTypes(locNode,
			move(target["parameters"][i]["var-type"]), fp, fp);
	}
	target["ret-type"] = fillKeptTypes(locNode, move(target["ret-type"]), from["ret-type"], from["ret-type"]);
	return target;
} // LCOV_EXCL_BR_LINE

// The owned type `p as T` views a pointer as, or null when `written` is not one.
json PlnSemanticAnalyzer::ownedCastType(const json& locNode, const json& written)
{
	json resolved = resolveKnownType(locNode, written);
	if (isStructType(resolved))
		requireCompleteStruct(resolved["type-name"].get<string>(), locNode);
	else if (resolved.value("type-kind", "") != "arr")
		return json();
	json owned = ownedTypeOf(locNode, resolved);
	if (owned.is_null()) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_InvalidCastTarget, typeDisplayName(written)) << endl;
		exit(1);
	}
	return owned;
} // LCOV_EXCL_BR_LINE

// An array type with its levels' sizes left out, which are all a cast to
// another array type may change.
static json withoutArrSizes(json t)
{
	for (json* cur = &t; isArrLevel(*cur); cur = &(*cur)["base-type"]) {
		cur->erase("arr-size");
		cur->erase("run-size");
		if (isArrLevel((*cur)["base-type"])) cur->erase("stride");
	}
	return t;
} // LCOV_EXCL_BR_LINE

// The lengths a cast to an array type sets on the levels it gives a length
// known only at run time, kept for a variable taking the array over.
json PlnSemanticAnalyzer::castLens(const json& expr, const json& written, json view)
{
	bool runSized = false;
	for (const json* t = &view["value-type"]; isArrLevel(*t); t = &(*t)["base-type"])
		runSized = runSized || t->value("run-size", false);
	if (runSized) {
		json lens = json::array();
		for (auto& len : arrVarStorage(expr, resolveKnownType(expr, written)).lens) lens.push_back(move(len));
		view["lens"] = move(lens);
	}
	return view;
} // LCOV_EXCL_BR_LINE

json PlnSemanticAnalyzer::sa_expr_cast(const json& expr, bool moving)
{
	if (expr["src"].value("expr-type", "") == "transfer") {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_TransferCast) << endl;
		exit(1);
	}
	json src = sa_expression(expr["src"]);
	// An address cast to an integer can be cast back.
	noteEscape(src);
	const json& from = src["value-type"];
	json written = fillKeptTypes(expr, expr["target-type"], from, from);
	json opaqueConv = opaqueConversion(expr, src, written);
	if (!opaqueConv.is_null()) return opaqueConv;

	// A pointer viewed as the owner of what it points to: no bits change, and
	// whether it really holds that is the programmer's responsibility.
	json owned = ownedCastType(expr, written);
	// An owned array given away at other sizes, which only a move may do, as
	// whether it has them is the programmer's responsibility.
	if (!owned.is_null() && isArrLevel(owned) && isArrLevel(from) && isOwnedBlock(from)) {
		if (!moving || withoutArrSizes(from) != withoutArrSizes(owned)) {
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(moving ? E_TransferShapeMismatch : E_ResizeNotMoved,
				typeDisplayName(owned), typeDisplayName(from)) << endl;
			exit(1);
		}
		json view = wrapConvert(src, owned);
		view["category"] = "resized";
		view["resized"] = true;
		return castLens(expr, written, move(view));
	}
	if (!owned.is_null()) {
		if (from.value("type-kind", "") != "pntr" || !from.contains("mutable")) {
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_OwnerViewNeedsPtr, typeDisplayName(written)) << endl;
			exit(1);
		}
		json view = wrapConvert(src, owned);
		view["category"] = "owner-view";
		view["resized"] = true;
		if (!from["mutable"].get<bool>()) view["read-only"] = true;
		return castLens(expr, written, move(view));
	}

	json resolved = resolveKnownType(expr, written);
	string tk = resolved.value("type-kind", "");
	if (tk == "embed") {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_InvalidCastTarget, typeDisplayName(written)) << endl;
		exit(1);
	}
	// A struct value is held as a pointer to its storage, but it is not one.
	bool fromPntr = from.value("type-kind", "") == "pntr" && !isStructStorage(from);
	auto incompatible = [&](const json& target) __attribute__((noreturn)) {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_IncompatibleTypeCast,
			typeDisplayName(from), typeDisplayName(target)) << endl;
		exit(1);
	};

	// A pointer cast changes no bits: it may retarget the pointee, change its
	// permission, or view the memory as a borrowed array, unchecked as `p[i]` is.
	// A pointer only fits a 64-bit integer, so narrower integers are rejected
	// rather than silently dropping the upper half.
	auto isWord = [](const json& t) {
		string n = t.value("type-kind", "") == "prim" && !t.contains("enum") ? t.value("type-name", "") : "";
		return n == "int64" || n == "uint64";
	};
	if (tk == "pntr") {
		json target = deepNormalizePrimToStruct(expr, normalizeArrBorrowType(expr, resolved));
		if (from == target) return src;
		// C's types say nothing of ownership, so only a cast of a C function
		// itself tells it; any other function already frees by its signature.
		bool fromCFunc = src.value("expr-type", "") == "func-ref" && src.value("func-type", "") == "c";
		if (!fromCFunc && !funcSigOwnershipOk(from, target)) {
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_FuncCastOwnership,
				typeDisplayName(from), typeDisplayName(target)) << endl;
			exit(1);
		}
		bool toArr = target.contains("arr-size");
		if (!fromPntr && (toArr || !isWord(from))) incompatible(target);
		return wrapConvert(src, target);
	}
	if (fromPntr) {
		if (!isWord(resolved)) incompatible(resolved);
		return wrapConvert(src, resolved);
	}

	const PlnType* target  = registry_.fromJson(resolved);
	const PlnType* srcType = registry_.fromJson(from);
	TypeCompat compat      = typeCompat(srcType, target, registry_);
	if (compat == TypeCompat::Identical) {
		return src;
	} else if (compat == TypeCompat::ExplicitCast && isBoolPrim(target)) {
		// C's conversion to _Bool: any nonzero value is 1, not a truncation.
		// LCOV_EXCL_BR_START
		json zero = isFloatPrim(srcType)
			? json{{"expr-type", "lit-flo"}, {"value", "0.0"}, {"value-type", from}}
			: json{{"expr-type", "lit-int"}, {"value", "0"}, {"value-type", from}};
		return {{"expr-type", "cmp"}, {"op", "!="}, {"left", src}, {"right", zero},
		        {"value-type", resolved}};
		// LCOV_EXCL_BR_STOP
	} else if (compat == TypeCompat::ImplicitWiden || compat == TypeCompat::ExplicitCast) {
		return wrapConvert(src, registry_.toJson(target));
	}
	incompatible(resolved);
} // LCOV_EXCL_BR_LINE

json PlnSemanticAnalyzer::resolveEnumerator(const json& expr)
{
	string typeName = typeNameWritten(expr["object"]);
	if (typeName.empty()) return json();
	requireWritableTypeName(expr, typeName);
	json enumType = enumTypeNamed(typeName);
	if (enumType.is_null()) return json();

	const auto& values = enumDefs_.at(enumType["enum"].get<string>()).values;
	string field = expr["field"].get<string>();
	auto it = values.find(field);
	if (it == values.end()) {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_UnknownEnumerator, field, typeName) << endl;
		exit(1);
	}
	json lit = {{"expr-type", "lit-int"}, {"value", to_string(it->second)}, {"value-type", enumType}}; // LCOV_EXCL_BR_LINE
	if (expr.contains("loc")) lit["loc"] = expr["loc"];
	return lit;
} // LCOV_EXCL_BR_LINE

// `Name.X` for each enum with an enumerator `name`, as source can write it
// here. An enum's own name (`V.Name` under an alias import) is preferred over
// an alias of it, such as a C typedef of a tagged enum.
vector<string> PlnSemanticAnalyzer::enumeratorSpellings(const string& name) const
{
	map<string, string> spelling;  // program-wide enum name -> written name
	for (auto& [written, t] : typeAliases_) {
		if (!t.contains("enum")) continue;
		string enumName = t["enum"].get<string>();
		if (!enumDefs_.at(enumName).values.count(name)) continue;
		size_t dot = written.find('.');
		bool own = written.substr(dot == string::npos ? 0 : dot + 1) == PlnSaMessage::writtenTypeName(enumName);
		auto [it, isNew] = spelling.emplace(enumName, written);
		if (!isNew && own) it->second = written;
	}
	vector<string> result;
	for (auto& [enumName, written] : spelling) result.push_back(written + "." + name);
	sort(result.begin(), result.end());
	return result;
} // LCOV_EXCL_BR_LINE

json PlnSemanticAnalyzer::sa_expr_field_access(const json& expr, bool forWrite)
{
	FieldChain chain = resolveObjectChain(expr["object"], forWrite);
	string fn = expr["field"].get<string>();
	const FieldLayout& fld = findFieldOrExit(chain.structName, fn, expr, forWrite);
	json out = makeFieldAccess(chain, fld);
	// A read-only field's pointer, read outside, must not lend its pointee for writing.
	if (fld.access == "readonly" && fld.typeKind == "raw-ptr" && outsideTypeModule(chain.structName))
		for (json* t = &out["value-type"]; t->value("type-kind", "") == "pntr"; t = &(*t)["base-type"])
			if (t->contains("mutable")) (*t)["mutable"] = false;
	return out;
} // LCOV_EXCL_BR_LINE

json PlnSemanticAnalyzer::makeFieldAccess(const FieldChain& chain, const FieldLayout& fld)
{
	// LCOV_EXCL_BR_START
	json vt = fieldValueType(fld);
	int off = chain.offset + fld.offset;
	// embed/embed-arr/embed-varr fields are inline data (no pointer is actually stored
	// at this offset): the field's "value" is its own address, computed as
	// ptr+offset, not a load of the memory there.
	bool addrOnly = (fld.typeKind == "embed" || fld.typeKind == "embed-arr" || fld.typeKind == "embed-varr");
	json out = !chain.isPointerBased
		? json{{"expr-type","field-access"},{"var",chain.varName},{"offset",off},{"value-type",vt},{"addr-only",addrOnly}}
		: json{{"expr-type","field-access"},{"ptr-expr",chain.ptrExpr},{"offset",off},{"value-type",vt},{"addr-only",addrOnly}};
	if (fld.bitWidth) out["bits"] = {{"shift", fld.bitShift}, {"width", fld.bitWidth}};
	return out;
	// LCOV_EXCL_BR_STOP
} // LCOV_EXCL_BR_LINE

// What `arr[i]` indexes, as a write through it needs it.
json PlnSemanticAnalyzer::sa_index_base(const json& arr, bool forWrite)
{
	string arr_kind = arr.value("expr-type", "");
	json sa_array = !forWrite ? sa_expression(arr)
		: arr_kind == "field-access" ? sa_expr_field_access(arr, true)
		: arr_kind == "arr-index"    ? sa_expr_arr_index(arr, true)
		: sa_expression(arr);
	requireOwnedBase(sa_array, arr);
	return sa_array;
}

// `d[k]` as the address of k's value that the generated function `op` ("at",
// or "put" that adds k if it is not there) returns, typed as the values' data
// pointer is so that indexing it by 0 makes it an element as `[+]T`'s are.
json PlnSemanticAnalyzer::dictValueAddr(json saDict, const json& dictAst, const json& keyAst,
                                        const string& op, const json& locNode)
{
	// LCOV_EXCL_BR_START
	json vt = saDict["value-type"];
	const json& elem = vt["base-type"]["base-type"];
	bool isPut = op == "put";
	json self = dictPntrType(elem, isVarrEmbedded(vt));
	self["mutable"] = isPut;
	json argAst = dictAst;
	if (isDictStorage(vt)) {
		saDict["value-type"]["mutable"] = isPut;
		argAst = {{"expr-type","addr-of"},{"object",dictAst},{"mutable",isPut}};
	}
	json key = {{"name","k"},{"var-type",{{"type-kind","pntr"},{"mutable",false},{"base-type",{{"type-kind","prim"},{"type-name","uint8"}}}}}};
	AllocShapeRef shape = dictShape(vt);
	json def = {{"name",op},{"func-type","palan"},{"symbol",allocHelperSymbol(shape, op)},
	            {"parameters",json::array({{{"name","d"},{"var-type",move(self)}}, move(key)})},
	            {"len-args",helperLens(shape.elem)}};
	vector<CallArg> args;
	args.push_back(CallArg{argAst, false, move(saDict)});
	// The key is only read, as an index is, so an array is given by name.
	json keyArg = foldConstExpr(keyAst);
	if (isLocationExpr(keyArg)) {
		json kt = operandValueType(keyArg);
		if ((isArrLevel(kt) || isVarrPntr(kt)) && !isArrBorrowVar(kt))
			keyArg = {{"expr-type","addr-of"},{"object",keyAst},{"mutable",false},{"loc",keyAst.value("loc", json::array())}};
	}
	for (auto& a : analyzeCallArgs(json::array({keyArg}))) args.push_back(move(a));
	json callAst = {{"expr-type","call"},{"loc",locNode.value("loc", json::array())}};
	json call = makeCallNode(callAst, Callee{&def, false}, "[]", &args);
	call["value-type"] = valuesDataType(vt, false);
	return call;
	// LCOV_EXCL_BR_STOP
} // LCOV_EXCL_BR_LINE

json PlnSemanticAnalyzer::sa_expr_arr_index(const json& expr, bool forWrite, json saBase)
{
	json sa_expr = expr;
	const json& arr = expr["array"];
	json sa_array = saBase.is_null() ? sa_index_base(arr, forWrite) : move(saBase);
	json sa_index;
	if (isDictPntr(sa_array["value-type"])) {
		sa_array = dictValueAddr(move(sa_array), arr, expr["index"], "at", expr);
		sa_index = lenLit(0);
	}
	if (isVarrPntr(sa_array["value-type"]))
		sa_array = varrData(move(sa_array));
	const json& array_type = sa_array["value-type"];
	if (array_type.value("type-kind", "") != "pntr") {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_NotArrayType) << endl;
		exit(1);
	}
	const json& elem_type = array_type["base-type"];

	if (sa_index.is_null()) sa_index = sa_expression(expr["index"]);
	const PlnType* idxType = registry_.fromJson(sa_index["value-type"]);
	if (isFloatPrim(idxType)) {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_ArrayIndexNotInteger) << endl;
		exit(1);
	}

	json uint64_type = {{"type-kind","prim"},{"type-name","uint64"}}; // LCOV_EXCL_BR_LINE

	if (array_type.value("embedded", false)) {
		if (elem_type.value("type-kind", "") == "struct") {
			// [n]$T contiguous struct array: pts[i] -> pntr(struct(T)), inline address
			// (stride = T.totalSize, carried on array_type; arr-index itself does not deref)
			int64_t stride = array_type.value("stride", (int64_t)0);
			json row_pntr = {{"type-kind","pntr"},{"base-type",elem_type}}; // LCOV_EXCL_BR_LINE
			json elem_size_node = {
				{"expr-type","lit-uint"},{"value",to_string(stride)},{"value-type",uint64_type}
			}; // LCOV_EXCL_BR_LINE
			sa_expr["array"]      = sa_array;
			sa_expr["index"]      = sa_index;
			sa_expr["elem-size"]  = elem_size_node;
			sa_expr["value-type"] = row_pntr;
			sa_expr["addr-only"]  = true;
			return sa_expr;
		}

		if (elem_type.value("type-kind", "") == "prim") {
			// [n]$T embedded array field, primitive leaf: data[i] -> scalar value
			int64_t stride = array_type.value("stride", (int64_t)0);
			json elem_size_node = {
				{"expr-type","lit-uint"},{"value",to_string(stride)},{"value-type",uint64_type}
			};
			sa_expr["array"]      = sa_array;
			sa_expr["index"]      = sa_index;
			sa_expr["elem-size"]  = elem_size_node;
			sa_expr["value-type"] = elem_type;
			sa_expr["addr-only"]  = false;
			return sa_expr;
		}

		// [n]$[m]T row access: mat[i] -> the row, laid out in place. A row
		// whose length is known only at run time is a variable's.
		auto [root, depth] = arrRoot(sa_array);
		json elem_size_node = inPlaceSizeExpr(elem_type, depth + 1, [&](size_t k) { return runtimeLen(*root, k); });
		sa_expr["array"]      = sa_array;
		sa_expr["index"]      = sa_index;
		sa_expr["elem-size"]  = elem_size_node;
		sa_expr["value-type"] = elem_type;
		sa_expr["addr-only"]  = true;
		return sa_expr;
	}

	if (elem_type.value("type-kind", "") == "struct") {
		// `p[i]` on a struct pointee is an address computation (base +
		// i*sizeof(T)), not a load -- Palan has no register-sized struct value.
		// The element is the struct's storage, like a struct stored in an
		// array; writes are checked against p's own permission.
		int64_t stride = requireCompleteStruct(elem_type["type-name"].get<string>(), expr).totalSize;
		json elem_pntr = {{"type-kind","pntr"},{"base-type",elem_type}}; // LCOV_EXCL_BR_LINE
		json elem_size_node = {
			{"expr-type","lit-uint"},{"value",to_string(stride)},{"value-type",uint64_type}
		}; // LCOV_EXCL_BR_LINE
		sa_expr["array"]      = sa_array;
		sa_expr["index"]      = sa_index;
		sa_expr["elem-size"]  = elem_size_node;
		sa_expr["value-type"] = elem_pntr;
		sa_expr["addr-only"]  = true;
		return sa_expr;
	}

	if (elem_type.value("type-kind","") == "prim" && elem_type.value("type-name","") == "void") {
		// `void` has no size; unlike E_UnknownType below (an unrecognized
		// name), this pointee name IS recognized -- it's just not indexable.
		// Covers read (p[i]), write (v -> p[i] via sa_arr_assign_stmt), and
		// element-address-of (@p[i], reached before its own guard runs).
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_DerefVoidPointer) << endl;
		exit(1);
	}
	int sz = (elem_type.value("type-kind","") == "pntr") ? 8
		: elemSizeBytes(elem_type.value("type-name","")); // LCOV_EXCL_BR_LINE
	if (sz < 0) {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_UnknownType, elem_type.value("type-name","")) << endl;
		exit(1);
	}
	json elem_size_node = {
		{"expr-type", "lit-uint"}, {"value", to_string(sz)}, {"value-type", uint64_type}
	}; // LCOV_EXCL_BR_LINE

	sa_expr["array"]      = sa_array;
	sa_expr["index"]      = sa_index;
	sa_expr["elem-size"]  = elem_size_node;
	sa_expr["value-type"] = elem_type;
	sa_expr["addr-only"]  = false;
	return sa_expr;
} // LCOV_EXCL_BR_LINE
