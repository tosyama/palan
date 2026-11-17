/// Palan Semantic Analyzer — declaration analysis
///
/// @file PlnSaDecl.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <iostream>
#include <algorithm>
#include <functional>
#include <cstdint>
#include <optional>
#include "PlnSemanticAnalyzer.h"
#include "PlnSaMessage.h"
#include "PlnSaInternal.h"
#include "../common/PlnDefs.h"
#include "../common/PlnCBitField.h"

static int alignUp(int val, int align) { return (val + align - 1) & ~(align - 1); }

// The non-pointer type a pointer field finally points to, and the mutability
// of each pointer level passed on the way (excluding the field's own).
struct PointeeLeaf { const json* type; vector<bool> innerPtrMutable; };
static PointeeLeaf pointeeLeaf(const json& pntr)
{
	PointeeLeaf leaf{&pntr["base-type"], {}};
	while (leaf.type->value("type-kind", "") == "pntr") {
		leaf.innerPtrMutable.push_back(leaf.type->value("mutable", false));
		leaf.type = &(*leaf.type)["base-type"];
	}
	return leaf;
}

// Only a cinclude'd struct can have such a field: a native function type is
// checked where it is written.
static string unsupportedPointee(const PointeeLeaf& leaf)
{
	return leaf.type->value("type-kind", "") == "func" && !unrepresentableTypeName(*leaf.type).empty()
		? "function pointer" : "";
}

static string pointeeElemKind(const PointeeLeaf& leaf)
{
	if (leaf.type->value("type-kind", "") == "func") return "func";
	return isPrimPointeeName(leaf.type->value("type-name", "")) ? "prim" : "struct";
}

static json pointeeFuncType(const PointeeLeaf& leaf)
{
	return leaf.type->value("type-kind", "") == "func" ? *leaf.type : json();
}

// Its levels are checked first, so that their errors are the field's own. A
// C pointer slot keeps its place even when its pointee cannot be accessed.
FieldLayout PlnSemanticAnalyzer::inlineArrField(const json& locNode, const string& structName, const json& arr)
{
	const json* leaf = &arr;
	for (; leaf->value("type-kind", "") == "arr" && !isVarrAstType(*leaf); leaf = &(*leaf)["base-type"]) {
		if ((*leaf)["size-expr"].is_null()) {
			cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_UnsupportedStructFieldType) << endl;
			exit(1);
		}
		fieldArrSize(locNode, (*leaf)["size-expr"]);
	}
	if (leaf->value("type-name", "") == structName) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_RecursiveStruct, structName) << endl;
		exit(1);
	}
	FieldLayout f{.typeKind="embed-arr", .isMutable=false}; // LCOV_EXCL_BR_LINE
	json laidOut = arr;
	if (leaf->value("type-kind", "") == "pntr") {
		f.unsupported = unsupportedPointee(pointeeLeaf(*leaf));
		// Only the slots' layout is needed of a field nothing can access.
		if (!f.unsupported.empty()) {
			json* slot = &laidOut;
			while (slot->value("type-kind", "") == "arr") slot = &(*slot)["base-type"];
			*slot = {{"type-kind","pntr"},{"mutable",false},{"base-type",{{"type-kind","prim"},{"type-name","void"}}}}; // LCOV_EXCL_BR_LINE
		}
	} else {
		requireKnownTypeNames(locNode, arr);
		f.enumName = leaf->value("enum", "");
	}
	f.type = sizedArrLevel(locNode, laidOut, nullopt);
	f.size = inPlaceSize(f.type);
	return f;
} // LCOV_EXCL_BR_LINE

StructDef PlnSemanticAnalyzer::buildStructDef(const string& name, const json& fields,
                                              const json& locNode, bool isUnion, bool packed, int aligned)
{
	string errPrefix = locPrefix(locNode);
	StructDef def;
	def.name = name;
	def.isUnion = isUnion;
	int offset = 0, maxAlign = aligned;
	int64_t tailBits = 0;  // unused bits of offset's last byte, after a bit-field
	bool fieldPacked = false;
	int fieldAligned = 1;
	// Returns the new field's offset; a union overlays every field at 0 and
	// sizes to its largest member instead of advancing past each one.
	// GCC's packed/aligned attributes (C structs only) override `align`.
	auto place = [&](int64_t size, int align) {
		int naturalAlign = align;
		if (packed || fieldPacked) align = 1;
		align = max(align, fieldAligned);
		maxAlign = max(maxAlign, align);
		if (isUnion) {
			offset = max<int64_t>(offset, size);
			return 0;
		}
		int at = alignUp(offset, align);
		if (at % naturalAlign) def.hasUnalignedField = true;
		offset = at + size;
		return at;
	};
	for (auto& f : fields) {
		fieldPacked = f.value("packed", false);
		fieldAligned = f.value("aligned", 1);
		const json& vtype = f["var-type"];
		string tk = vtype.value("type-kind", "");
		string fieldName = f.value("name", "");

		if (f.contains("bits")) {
			// Only C structs have bit-fields, always of an integer type. Laid out
			// so the struct matches C, but nothing in Palan can read or write one.
			string tname = vtype["type-name"].get<string>();
			int64_t width = f["bits"].get<int64_t>();
			int align = elemSizeBytes(tname);
			bool isPacked = packed || fieldPacked;
			if (!fieldName.empty() && !isPacked) maxAlign = max(maxAlign, align);
			int64_t at = 0;
			if (isUnion) {
				offset = max<int64_t>(offset, (width + 7) / 8);
			} else {
				at = placeCBitField(int64_t(offset) * 8 - tailBits, width, align, isPacked);
				offset = (at + width + 7) / 8;
				tailBits = int64_t(offset) * 8 - (at + width);
			}
			if (!fieldName.empty())
				def.fields.push_back({.name=fieldName, .typeKind="prim", .typeName=tname, .isMutable=false,
				                      .offset=int(at / 8), .size=int((at + width + 7) / 8 - at / 8),
				                      .unsupported="bit-field"});
			continue;
		}
		tailBits = 0;

		if (tk == "prim") {
			string tname = vtype["type-name"].get<string>();
			if (structDefs_.count(tname)) {
				const StructDef& target = structDefs_.at(tname);
				if (!target.isComplete && !target.isOpaque) {
					cerr << errPrefix << PlnSaMessage::getMessage(E_IncompleteStructType, structDisplayName(tname),
					                                  target.incompleteReason, target.keyword()) << endl;
					exit(1);
				}
				int at = place(8, 8);
				def.fields.push_back({.name=fieldName, .typeKind="owned", .isMutable=false,
				                      .offset=at, .size=8, .type=toStructPntrType(vtype)});
				def.ownsFields = true;
			} else {
				int sz = elemSizeBytes(tname);
				if (sz < 0) {
					cerr << errPrefix << PlnSaMessage::getMessage(E_UnknownType, tname) << endl;
					exit(1);
				}
				int at = place(sz, sz);
				def.fields.push_back({.name=fieldName, .typeKind="prim",
				                      .typeName=tname, .isMutable=false,
				                      .offset=at, .size=sz, .enumName=vtype.value("enum", "")});
			}
		} else if (tk == "embed" && isVarrAstType(vtype["base-type"])) {
			// `$[+]U`: the header laid out in the struct, as `$[+]U` lays it out in a frame.
			int at = place(24, 8);
			def.fields.push_back({.name=fieldName, .typeKind="embed-varr", .isMutable=false,
			                      .offset=at, .size=24, .type=ownedFieldType(locNode, vtype["base-type"])});
			def.ownsFields = true;
		} else if (tk == "embed" && vtype["base-type"].value("type-kind", "") == "arr") {
			// `$[n]U`: the array laid out in the struct, as `$[n]U` lays it out in a frame.
			FieldLayout fl = inlineArrField(locNode, name, vtype["base-type"]);
			fl.name = fieldName;
			fl.offset = place(fl.size, inPlaceAlign(fl.type));
			// Its owned elements' slots are built with the struct.
			if (ownsElems(fl.type)) def.ownsFields = true;
			const json* leaf = &fl.type;
			while (isArrLevel(*leaf)) leaf = &(*leaf)["base-type"];
			if (leaf->value("type-kind", "") == "struct" // LCOV_EXCL_BR_LINE
			    && structDefs_.at((*leaf)["type-name"].get<string>()).hasUnalignedField)
				def.hasUnalignedField = true;
			def.fields.push_back(move(fl));
		} else if (tk == "embed") {
			if (vtype["base-type"].value("type-kind", "") != "prim") {
				cerr << errPrefix << PlnSaMessage::getMessage(E_EmbedElemNotStruct,
				                                  typeDisplayName(vtype["base-type"])) << endl;
				exit(1);
			}
			string structName = vtype["base-type"]["type-name"].get<string>();
			if (structName == name) {
				cerr << errPrefix << PlnSaMessage::getMessage(E_RecursiveStruct, name) << endl;
				exit(1);
			}
			if (!structDefs_.count(structName)) {
				PlnSaMessageCode code = elemSizeBytes(structName) >= 0 ? E_EmbedElemNotStruct : E_UnknownType;
				cerr << errPrefix << PlnSaMessage::getMessage(code, structName) << endl;
				exit(1);
			}
			const StructDef& sub = structDefs_.at(structName);
			if (!sub.isComplete) {
				cerr << errPrefix << PlnSaMessage::getMessage(E_IncompleteStructType, structDisplayName(structName),
				                                  sub.incompleteReason, sub.keyword()) << endl;
				exit(1);
			}
			if (sub.ownsFields) def.ownsFields = true;
			int align = sub.maxAlign;
			int at = place(sub.totalSize, align);
			if (sub.hasUnalignedField) def.hasUnalignedField = true;
			if (!f.contains("name")) {
				// A C11 anonymous member: promote its fields so every consumer
				// finds s.a as an ordinary field of the owner. sub was registered
				// first, so its own anonymous members are already promoted.
				for (FieldLayout fl : sub.fields) {
					fl.offset += at;
					def.fields.push_back(fl);
				}
				continue;
			}
			def.fields.push_back({.name=fieldName, .typeKind="embed",
			                      .typeName=structName, .isMutable=false,
			                      .offset=at, .size=sub.totalSize});
		} else if (tk == "pntr") {
			int sz = 8, align = 8;
			PointeeLeaf leaf = pointeeLeaf(vtype);
			string baseName = leaf.type->value("type-name", "");
			bool isMut = vtype.value("mutable", false);
			int at = place(sz, align);
			def.fields.push_back({.name=fieldName, .typeKind="raw-ptr",
			                      .typeName=baseName, .isMutable=isMut,
			                      .offset=at, .size=sz, .elemKind=pointeeElemKind(leaf),
			                      .enumName=leaf.type->value("enum", ""),
			                      .innerPtrMutable=leaf.innerPtrMutable,
			                      .unsupported=unsupportedPointee(leaf),
			                      .funcType=pointeeFuncType(leaf)});
		} else if (tk == "arr") {
			int at = place(8, 8);
			def.fields.push_back({.name=fieldName, .typeKind="owned", .isMutable=false,
			                      .offset=at, .size=8, .type=ownedFieldType(locNode, vtype)});
			def.ownsFields = true;
		} else {
			cerr << errPrefix << PlnSaMessage::getMessage(E_UnsupportedStructFieldType) << endl;
			exit(1);
		}
	}
	def.totalSize  = alignUp(offset, maxAlign);
	def.maxAlign   = maxAlign;
	def.isComplete = true;
	setFieldInits(def, fields);
	return def;
} // LCOV_EXCL_BR_LINE

// The generated module stores every integer as the signed integer of its
// width (layoutElem), so a value is written as that integer's.
static pair<string, string> layoutValue(const string& typeName, const json& lit)
{
	static const map<string, pair<string, int>> ints = {
		{"int8",{"int8",8}}, {"uint8",{"int8",8}}, {"bool",{"int8",8}},
		{"int16",{"int16",16}}, {"uint16",{"int16",16}},
		{"int32",{"int32",32}}, {"uint32",{"int32",32}},
		{"int64",{"int64",64}}, {"uint64",{"int64",64}},
	};
	string text = lit["value"];
	auto it = ints.find(typeName);
	if (it == ints.end()) return {typeName, text};
	__int128 v = text[0] == '-' ? (__int128)stoll(text) : (__int128)stoull(text);
	int bits = it->second.second;
	__int128 span = (__int128)1 << bits;
	v %= span;
	if (v < 0) v += span;
	if (v >= span / 2) v -= span;
	return {it->second.first, to_string((int64_t)v)};
}

// Each initializer is a constant, which palan-sa writes into the struct's
// shape for the generated allocators to store (SpecAndDesign.md 3.6).
void PlnSemanticAnalyzer::setFieldInits(StructDef& def, const json& fields)
{
	// A value reaches here typed and, from an integer to a float, converted.
	auto constant = [&](const json& value, const string& fieldName, const json& locNode) -> json {
		const json* v = &value;
		if (v->value("expr-type", "") == "convert") v = &(*v)["src"];
		string et = v->value("expr-type", "");
		if (et != "lit-int" && et != "lit-uint" && et != "lit-flo") {
			cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_FieldInitNotConst, fieldName) << endl;
			exit(1);
		}
		return *v;
	};
	// Variables and functions are not known yet when types are declared, so a
	// value naming one is rejected before it is analyzed. An enum member is a
	// field access on its type.
	function<bool(const json&)> readsRuntime = [&](const json& e) {
		string et = e.is_object() ? e.value("expr-type", "") : "";
		if (et == "id" || et == "call" || et == "chain-call" || et == "member-call") return true;
		if (et == "field-access") return false;
		if (e.is_structured())
			for (auto& child : e)
				if (readsRuntime(child)) return true;
		return false;
	};
	auto folded = [&](const json& e, const string& fieldName) {
		json v = foldConstExpr(e);
		if (readsRuntime(v)) {
			cerr << locPrefix(e) << PlnSaMessage::getMessage(E_FieldInitNotConst, fieldName) << endl;
			exit(1);
		}
		return v;
	};
	for (auto& af : fields) {
		if (!af.contains("init")) continue;
		string fn = af["name"];
		auto f = find_if(def.fields.begin(), def.fields.end(), [&](const FieldLayout& l) { return l.name == fn; });
		const json& init = af["init"];
		json type = fieldValueType(*f);
		json entry = {{"offset", f->offset}};
		json values = json::array();
		string elemName;
		if (f->typeKind == "prim" && f->unsupported.empty()) {
			const PlnType* toType = registry_.fromJson(type);
			json value = convertForBinding(init, sa_expression(folded(init, fn), toType), toType, type);
			elemName = f->typeName;
			values.push_back(constant(value, fn, init));
			entry["kind"] = "prim";
		} else if (isLitArrField(*f) && (isVarrStorage(type) ? type["base-type"]["base-type"] : type["base-type"])
		                                    .value("type-kind", "") == "prim") {
			if (init.value("expr-type", "") != "arr-lit") {
				cerr << locPrefix(init) << PlnSaMessage::getMessage(E_ArrLitDimMismatch, fn) << endl;
				exit(1);
			}
			json lit = init;
			for (auto& item : lit["items"]) item = folded(item, fn);
			ArrLitShape shape = arrLitShape(init, fn, type, true);
			checkArrLitCount(init, lit["items"], fn, shape);
			for (auto& value : sa_arr_lit_values(lit, shape, fn))
				values.push_back(constant(value, fn, init));
			elemName = shape.leaf["type-name"];
			entry["kind"] = f->typeKind == "embed-arr" ? "run" : f->typeKind == "embed-varr" ? "embed-varr"
			              : isVarrStorage(type) ? "varr" : "row";
			if (isVarrStorage(type)) entry["shape-key"] = allocShapeKey(varrShape(type));
		} else {
			cerr << locPrefix(init) << PlnSaMessage::getMessage(E_FieldInitUnsupported, fn) << endl;
			exit(1);
		}
		// A literal leaving the field out takes these values, already typed, so
		// that a module importing the struct needs none of the names they came from.
		json typed = values;
		if (init.contains("loc"))
			for (auto& v : typed) v["loc"] = init["loc"];
		if (entry["kind"] == "prim") {
			f->initAst = typed[0];
		} else {
			f->initAst = {{"expr-type", "arr-lit"}, {"items", move(typed)}};
			if (init.contains("loc")) f->initAst["loc"] = init["loc"];
		}
		json texts = json::array();
		for (auto& v : values) {
			auto [layout, text] = layoutValue(elemName, v);
			entry["type"] = layout;
			texts.push_back(text);
		}
		entry["values"] = move(texts);
		def.inits.push_back(move(entry));
	}
	if (!def.inits.empty()) def.hasInits = true;
	for (auto& f : def.fields) {
		const json* leaf = &f.type;
		if (f.typeKind == "embed-arr")
			while (isArrLevel(*leaf)) leaf = &(*leaf)["base-type"];
		string sub = f.typeKind == "embed" ? f.typeName
		           : f.typeKind == "embed-arr" && leaf->value("type-kind", "") == "struct" ? (*leaf)["type-name"].get<string>() : "";
		if (!sub.empty() && structDefs_.at(sub).hasInits) def.hasInits = true;
	}
} // LCOV_EXCL_BR_LINE

json PlnSemanticAnalyzer::sa_expression_stmt(const json& stmt)
{
	return discardedValueStmt(stmt, sa_expr_maybe_void(stmt["body"]));
}

json PlnSemanticAnalyzer::discardedValueStmt(const json& stmt, json body)
{
	// A temp holding a call (a chained call's return values) becomes a block,
	// since only a call is lowered as an expression statement.
	if (body.value("expr-type", "") == "temp") {
		// LCOV_EXCL_BR_START
		json stmts = body["body"];
		stmts.push_back(discardedValueStmt(stmt, body["value"]));
		for (auto& r : body["release"])
			stmts.push_back(r);
		return {{"stmt-type","block"},{"body",move(stmts)}};
		// LCOV_EXCL_BR_STOP
	} // LCOV_EXCL_LINE -- only exception cleanup is attributed here
	// A discarded struct is written into the frame and only what it owns released.
	if (isStructRetCall(body)) {
		// LCOV_EXCL_BR_START
		string temp = "__tmp_" + to_string(tempVarCounter_++);
		json st = body["value-type"];
		json stmts = declareStructRet(temp, st, move(body), VarStorage::Frame);
		json release = frameStructRelease(temp, st);
		if (!release.is_null()) stmts.push_back(move(release));
		return {{"stmt-type","block"},{"body",move(stmts)}};
		// LCOV_EXCL_BR_STOP
	}
	// A discarded block returned by a call has no other owner.
	if (isExpiringOwned(body)) {
		// LCOV_EXCL_BR_START
		string temp = "__tmp_" + to_string(tempVarCounter_++);
		const json& st = body["value-type"];
		json decl = {{"stmt-type","var-decl"},
		             {"vars",json::array({{{"name",temp},{"var-type",st},{"init",body}}})}};
		return {{"stmt-type","block"},{"body",json::array({decl, makeOwnedFreeStmt(temp, st)})}};
		// LCOV_EXCL_BR_STOP
	}
	return {
		{"stmt-type", "expr"},
		{"body", body}
	}; // LCOV_EXCL_BR_LINE
} // LCOV_EXCL_BR_LINE

json PlnSemanticAnalyzer::sa_var_decl(const json& stmt)
{
	json stmt2 = stmt;
	vector<VarStorage> storage;
	bool takes = false;
	for (auto& var : stmt2["vars"]) {
		storage.push_back(takeVarStorage(var["var-type"]));
		var["var-type"] = resolveKnownType(stmt2, var["var-type"]);
		takes = takes || var.value("ownership-transfer", false);
	}
	if (!takes)
		return sa_resolved_var_decl(stmt2, move(storage));
	// A `<<=` initializer is evaluated (and its owner reset) before its variable
	// is declared, so each variable is declared on its own, in order.
	// LCOV_EXCL_BR_START
	json result = json::array();
	const json& vars = stmt2["vars"];
	for (size_t i = 0; i < vars.size(); i++) {
		json one = stmt2;
		one["vars"] = json::array({vars[i]});
		if (vars[i].value("ownership-transfer", false)) {
			json& var = one["vars"][0];
			var["init"] = takeOverInit(one, var, storage[i], result);
			var.erase("ownership-transfer");
		}
		for (auto& s : sa_resolved_var_decl(one, {storage[i]}))
			result.push_back(move(s));
	}
	return result;
	// LCOV_EXCL_BR_STOP
} // LCOV_EXCL_BR_LINE

json PlnSemanticAnalyzer::takeOverInit(const json& stmt, const json& var, VarStorage storage, json& pre)
{
	const string& name = var["name"].get<string>();
	if (storage == VarStorage::Frame) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_TakeIntoFrameVar, name) << endl;
		exit(1);
	}
	json owned = ownedTypeOf(stmt, var["var-type"]);
	if (owned.is_null()) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_TransferToNonOwnedVar, name, "<<=") << endl;
		exit(1);
	}
	json value = giveValue(var["init"], stmt, "<<=", pre, pre);
	if (!copyShapeMatch(value["value-type"], owned)) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_TransferShapeMismatch,
			typeDisplayName(owned), typeDisplayName(value["value-type"])) << endl;
		exit(1);
	}
	// Sizes known only at run time on either side are the variable's from here on.
	value["value-type"] = owned;
	return {{"expr-type", "pinned"}, {"sa", move(value)}}; // LCOV_EXCL_BR_LINE
} // LCOV_EXCL_BR_LINE

json PlnSemanticAnalyzer::sa_resolved_var_decl(const json& stmt2, vector<VarStorage> storage)
{
	storage.resize(stmt2["vars"].size(), VarStorage::Heap);
	auto isArrInit = [](const json& var) {
		return var["var-type"].value("type-kind", "") == "arr" && var.contains("init");
	};
	auto isLitInit = [](const json& var) { return var.contains("init") && isAggregateLit(var["init"]); };
	auto isStructInit = [&](const json& var) {
		return var["var-type"].value("type-kind", "") == "prim" && var.contains("init")
			&& structDefs_.count(var["var-type"].value("type-name", ""));
	};
	auto ownRun = [&](const json& var) { return isArrInit(var) || isLitInit(var) || isStructInit(var); };

	// The lowering paths below apply one var-type to every var in a statement,
	// so the declaration is split into runs of equal var-type. The comparison
	// includes loc, so only vars inheriting the same source type node (`[f()]int32 a, b`)
	// share one evaluation of its size expression. Each array literal fixes its own
	// variable's size (`[]int32 a = [1,2], b = [1,2,3]`), so it always gets its own run,
	// as does an array initialized by copying another one or a struct literal.
	// An initialized struct gets its own run too: a copy or a call returning into
	// it runs after the var-decl, so a shared run would break left-to-right order
	// (`Pair a = b, c = mutate(@!b);`).
	json result = json::array();
	const json& vars = stmt2["vars"];
	size_t begin = 0;
	for (size_t i = 1; i <= vars.size(); i++) {
		if (i < vars.size() && vars[i]["var-type"] == vars[i-1]["var-type"]
				&& storage[i] == storage[i-1] && !ownRun(vars[i]) && !ownRun(vars[i-1]))
			continue;
		json run = stmt2;
		run["vars"] = json(vars.begin() + begin, vars.begin() + i);
		json lowered;
		if (isLitInit(vars[begin]) && !isArrInit(vars[begin]))
			lowered = sa_struct_lit_var_decl(run, storage[begin]);
		else if (!isArrInit(vars[begin]))
			lowered = sa_var_decl_group(run, storage[begin]);
		else if (vars[begin]["init"].value("expr-type", "") == "arr-lit")
			lowered = sa_arr_lit_var_decl(run, storage[begin]);
		else
			lowered = sa_arr_copy_var_decl(run, storage[begin]);
		for (auto& s : lowered)
			result.push_back(move(s));
		begin = i;
	}
	return result;
} // LCOV_EXCL_LINE

json PlnSemanticAnalyzer::sa_arr_copy_var_decl(const json& stmt, VarStorage storage)
{
	json var = stmt["vars"][0];
	string name = var["name"];
	// Evaluated before the declaration so the variable is not in scope in its
	// own initializer.
	json src = sa_expression(var["init"]);
	// A heap `[+]T` or `[n]T` takes over a returned block instead of copying from it.
	if (storage == VarStorage::Heap && isExpiringOwned(src)) {
		json vt = takeOverType(stmt, var["var-type"]);
		if (!vt.is_null() && src["value-type"] == vt)
			return sa_arr_var_decl(stmt, storage, arrVarStorage(stmt, var["var-type"]), false, &src);
	}
	json declStmt = stmt;
	declStmt["vars"][0].erase("init");
	json result = sa_var_decl_group(declStmt, storage);
	const json& vt = *findVar(name);
	json dst = {{"expr-type","id"},{"name",name},{"var-type",vt},{"value-type",vt}}; // LCOV_EXCL_BR_LINE
	result.push_back(makeCopyStmt(stmt, dst, src, CopyInto::New));
	return result;
} // LCOV_EXCL_BR_LINE

json PlnSemanticAnalyzer::sa_var_decl_group(const json& stmt2, VarStorage storage)
{
	const json& vtype = stmt2["vars"][0]["var-type"];
	string tk = vtype.value("type-kind", "");

	string spec = vtype.value("specifier", "");
	if (tk == "arr" && (spec == "variable" || spec == "raw"))
		return sa_arr_var_decl(stmt2, storage);
	if (tk == "prim" && structDefs_.count(vtype.value("type-name", "")))
		return sa_struct_var_decl(stmt2, storage);
	if (storage == VarStorage::Frame) {
		cerr << locPrefix(stmt2) << PlnSaMessage::getMessage(E_FrameVarType, typeDisplayName(vtype)) << endl;
		exit(1);
	}

	json borrowType = normalizeArrBorrowType(stmt2, vtype);
	bool isArrBorrow = borrowType.contains("arr-size");
	bool reshaped = borrowType != vtype;

	json sa_stmt = {{"stmt-type", "var-decl"}, {"vars", json::array()}}; // LCOV_EXCL_BR_LINE
	for (auto& var : stmt2["vars"]) {
		string name = var["name"];
		json sa_var = var;
		json varType = deepNormalizePrimToStruct(stmt2, reshaped ? borrowType : var["var-type"]);
		sa_var["var-type"] = varType;
		if (var.contains("init")) {
			// Evaluate init before declaring the variable so that the variable
			// itself is not in scope during its own initializer (e.g. `int32 z = z+1`
			// should be an error, not silently read uninitialized z).
			const PlnType* toType = registry_.fromJson(varType);
			json init = sa_expression(literalConversion(var["init"], varType), toType);
			if (isArrBorrow)
				checkArrBorrowBinding(stmt2, var["init"], init, varType);
			init = convertForBinding(stmt2, init, toType, varType);
			checkBorrowSource(stmt2, init, varType);
			checkPtrPermission(stmt2, init["value-type"], varType);
			checkRangeBorrowBinding(stmt2, var["init"], varType);
			sa_var["init"] = init;
		}
		declareVar(name, varType, &stmt2);
		sa_stmt["vars"].push_back(sa_var);
	}
	return json::array({sa_stmt}); // LCOV_EXCL_BR_LINE
} // LCOV_EXCL_BR_LINE

json PlnSemanticAnalyzer::sa_arr_size_expr(const json& stmt, const json& sizeExprAst)
{
	const PlnType* uint64Type = registry_.prim(PrimType::Name::Uint64);
	json sz = sa_expression(sizeExprAst, uint64Type);
	if (!sz.contains("value-type")) return sz;
	const PlnType* t = registry_.fromJson(sz["value-type"]);
	bool is_int = t->kind == PlnType::Kind::Prim
		&& static_cast<const PrimType*>(t)->name != PrimType::Name::Float32
		&& static_cast<const PrimType*>(t)->name != PrimType::Name::Float64;
	if (!is_int) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_ArraySizeNotInteger) << endl;
		exit(1);
	}
	// Every user of the size (a uint64 length variable, the byte-count
	// math) reads it as 8 bytes, so a narrower value -- a variable, an
	// expression, or a macro-folded lit-int that keeps its C-declared type --
	// must be widened here.
	if (t != uint64Type)
		sz = wrapConvert(sz, registry_.toJson(uint64Type));
	return sz;
} // LCOV_EXCL_LINE

// Fills an omitted dimension with `count`; otherwise returns the declared
// size, which must be a compile-time constant.
string PlnSemanticAnalyzer::arrLitDimSize(const json& stmt, const string& name, json& sizeExpr, size_t count)
{
	if (sizeExpr.is_null()) {
		sizeExpr = {{"expr-type", "lit-uint"}, {"value", to_string(count)}}; // LCOV_EXCL_BR_LINE
		return to_string(count);
	}
	json sz = sa_arr_size_expr(stmt, sizeExpr);
	const json& lit = sz.value("expr-type", "") == "convert" ? sz["src"] : sz;
	string et = lit.value("expr-type", "");
	if (et != "lit-int" && et != "lit-uint") {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_ArrLitSizeNotConst, name) << endl;
		exit(1);
	}
	return lit["value"].get<string>();
} // LCOV_EXCL_BR_LINE

// An omitted fixed size is the literal's count (of its first row, for a
// row's size); a `[+]T` row takes any count. Returns the top level's size.
string PlnSemanticAnalyzer::fillArrLitSizes(const json& stmt, const string& name, json& varType, const json& items)
{
	static const json noRows = json::array();
	json* level = &varType;
	const json* rows = &items;
	string declared;
	for (bool top = true; level->value("type-kind", "") == "arr"; top = false) {
		bool isVarr = isVarrAstType(*level);
		if (!isVarr && (*level)["size-expr"].is_null() && rows->empty()) {
			cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_ArrLitEmptySize, name) << endl;
			exit(1);
		}
		if (top || !isVarr) {
			string size = arrLitDimSize(stmt, name, (*level)["size-expr"], rows->size());
			if (top) declared = size;
		}
		rows = !rows->empty() && (*rows)[0].contains("items") ? &(*rows)[0]["items"] : &noRows;
		level = &(*level)["base-type"];
	}
	return declared;
} // LCOV_EXCL_LINE

json PlnSemanticAnalyzer::sa_arr_lit_var_decl(const json& stmt, VarStorage storage)
{
	const json& var = stmt["vars"][0];
	// A literal stored through a temp is reported by the store's target.
	string name = var.value("lit-name", var["name"].get<string>());
	const json& items = var["init"]["items"];

	json declStmt = stmt;
	json& declVar = declStmt["vars"][0];
	declVar.erase("init");
	declVar.erase("lit-name");

	string declared = fillArrLitSizes(stmt, name, declVar["var-type"], items);
	Storage st = arrVarStorage(stmt, declVar["var-type"]);
	ArrLitShape shape = arrLitShape(stmt, name, st.type, true);

	// Elements are analyzed before the variable is declared, so an element
	// cannot read the array it is initializing.
	json values = sa_arr_lit_values(var["init"], shape, name);

	// A variable-length array's size is its length.
	bool isVarr = isVarrStorage(st.type);
	if (isVarr && stoull(declared) < items.size()) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_ArrLitCountMismatch,
		                                  name, declared, to_string(items.size())) << endl;
		exit(1);
	}
	checkArrLitCount(stmt, items, name, shape);

	json result = sa_arr_var_decl(declStmt, storage, st, isVarr);
	emitArrLitAssigns({{"expr-type", "id"}, {"name", declVar["name"]}}, shape, values, result); // LCOV_EXCL_BR_LINE
	return result;
} // LCOV_EXCL_BR_LINE

// The levels of an already declared array, down to the elements a literal can
// give: numeric values or structs (stored in place or owned). A fixed level's
// size must be a compile-time constant.
PlnSemanticAnalyzer::ArrLitShape PlnSemanticAnalyzer::arrLitShape(const json& locNode, const string& name,
                                                                  const json& saType, bool varrTop)
{
	ArrLitShape shape = {{}, {}, nullptr};
	const json* t = &saType;
	while (isArrLevel(*t) || (isVarrStorage(*t) && (varrTop || !shape.levels.empty()))) {
		if (isVarrStorage(*t)) {
			shape.levels.push_back({-1, *t});
			t = &(*t)["base-type"]["base-type"];
			continue;
		}
		if (!t->contains("arr-size")) {
			cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_ArrLitSizeNotConst, name) << endl;
			exit(1);
		}
		shape.levels.push_back({(*t)["arr-size"].get<int64_t>(), json()});
		t = &(*t)["base-type"];
	}
	const json& leaf = *t;
	string leafKind = leaf.value("type-kind", "");
	shape.leaf = leaf;
	if (isStructStorage(leaf))
		shape.structDef = &requireCompleteStruct(leaf["base-type"]["type-name"].get<string>(), locNode);
	else if (leafKind == "struct")
		shape.structDef = &requireCompleteStruct(leaf["type-name"].get<string>(), locNode);
	else if (leafKind != "prim") {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_ArrLitElemType, name) << endl;
		exit(1);
	}
	return shape;
} // LCOV_EXCL_BR_LINE

// The values level by level: a row is an array of the next level's values,
// and a struct element is the array sa_struct_lit gives.
json PlnSemanticAnalyzer::sa_arr_lit_values(const json& lit, const ArrLitShape& shape, const string& name, size_t depth)
{
	bool isRow = depth + 1 < shape.levels.size();
	const PlnType* elemType = shape.structDef || isRow ? nullptr : registry_.fromJson(shape.leaf);
	json values = json::array();
	for (auto& item : lit["items"]) {
		bool isLit = item.value("expr-type", "") == "arr-lit";
		if (isRow != isLit && !(shape.structDef && !isRow)) {
			cerr << locPrefix(item) << PlnSaMessage::getMessage(E_ArrLitDimMismatch, name) << endl;
			exit(1);
		}
		if (isRow) {
			values.push_back(sa_arr_lit_values(item, shape, name, depth + 1));
		} else if (shape.structDef) {
			values.push_back(sa_struct_lit(item, *shape.structDef));
		} else {
			json value = sa_expression(literalConversion(item, shape.leaf), elemType);
			values.push_back(convertForBinding(item, value, elemType, shape.leaf));
		}
	}
	return values;
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

// A `[+]T` level takes any count, which its row is resized to.
void PlnSemanticAnalyzer::checkArrLitCount(const json& locNode, const json& items, const string& name,
                                           const ArrLitShape& shape, size_t depth)
{
	int64_t size = shape.levels[depth].size;
	if (size >= 0 && uint64_t(size) != items.size()) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(
			depth == 0 ? E_ArrLitCountMismatch : E_ArrLitRowSizeMismatch,
			name, to_string(size), to_string(items.size())) << endl;
		exit(1);
	}
	if (depth + 1 == shape.levels.size())
		return;
	for (auto& row : items)
		checkArrLitCount(row, row["items"], name, shape, depth + 1);
} // LCOV_EXCL_BR_LINE

void PlnSemanticAnalyzer::emitArrLitAssigns(const json& arrAst, const ArrLitShape& shape, const json& values, json& out,
                                            bool sized, size_t depth)
{
	auto indexOf = [](json array, size_t i) -> json {
		return {{"expr-type", "arr-index"}, {"array", move(array)},
		        {"index", {{"expr-type", "lit-uint"}, {"value", to_string(i)}}}}; // LCOV_EXCL_BR_LINE
	}; // LCOV_EXCL_BR_LINE
	const json& varr = shape.levels[depth].varr;
	if (!sized && !varr.is_null()) {
		// LCOV_EXCL_BR_START
		json n = {{"expr-type","lit-int"},{"value",to_string(values.size())},
		          {"value-type",{{"type-kind","prim"},{"type-name","int64"}}}};
		json resize = allocHelperCall(varrShape(varr), "resize", json::array({sa_expression(arrAst), n}));
		out.push_back({{"stmt-type","expr"},{"body",move(resize)}});
		// LCOV_EXCL_BR_STOP
	}
	bool isRow = depth + 1 < shape.levels.size();
	for (size_t i = 0; i < values.size(); i++) {
		json elemAst = indexOf(arrAst, i);
		const json& value = values[i];
		if (isRow) {
			emitArrLitAssigns(elemAst, shape, value, out, false, depth + 1);
		} else if (shape.structDef && value.is_object()) {
			out.push_back(makeCopyStmt(elemAst, sa_expression(elemAst), value, CopyInto::New));
		} else if (shape.structDef) {
			emitStructLitAssigns(elemAst, *shape.structDef, value, out);
		} else {
			out.push_back({{"stmt-type", "arr-assign"}, {"target", sa_expression(elemAst)}, {"value", value}}); // LCOV_EXCL_BR_LINE
		}
	}
} // LCOV_EXCL_BR_LINE

json PlnSemanticAnalyzer::sa_struct_lit_var_decl(const json& stmt, VarStorage storage)
{
	const json& var = stmt["vars"][0];
	const json& vtype = var["var-type"];
	auto isStructName = [&](const json& t) {
		return t.value("type-kind", "") == "prim" && structDefs_.count(t.value("type-name", ""));
	};
	if (vtype.value("type-kind", "") == "pntr" && isStructName(vtype["base-type"])) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_StructLitTarget) << endl;
		exit(1);
	}
	if (!isStructName(vtype))
		return sa_var_decl_group(stmt, storage);

	const StructDef& def = requireCompleteStruct(vtype["type-name"].get<string>(), stmt);
	// Evaluated before the declaration so the variable is not in scope in its
	// own initializer.
	json values = sa_struct_lit(var["init"], def);
	json declStmt = stmt;
	declStmt["vars"][0].erase("init");
	json result = sa_var_decl_group(declStmt, storage);
	emitStructLitAssigns({{"expr-type", "id"}, {"name", var["name"]}}, def, values, result); // LCOV_EXCL_BR_LINE
	return result;
} // LCOV_EXCL_BR_LINE

// Normalizes a struct value written in field order (`[0, 1]`) or by name
// (`{x: 0, y: 1}`) to one array of analyzed values in field declaration
// order; an entry for a struct field is itself such a value, and one for an
// array field is what sa_arr_lit_values gives. A struct value written as an
// expression stays that analyzed expression (an object, not an array): it is
// copied from, like the source of '->' to a struct.
json PlnSemanticAnalyzer::sa_struct_lit(const json& item, const StructDef& def)
{
	string et = item.value("expr-type", "");
	if (et != "arr-lit" && et != "dict-lit") {
		json structVT = {{"type-kind", "pntr"}, {"base-type", {{"type-kind", "struct"}, {"type-name", def.name}}}}; // LCOV_EXCL_BR_LINE
		json value = sa_expression(literalConversion(item, structVT));
		if (!value.contains("value-type") || !isStructPntr(value["value-type"])) {
			cerr << locPrefix(item) << PlnSaMessage::getMessage(E_StructLitExpected, structDisplayName(def.name)) << endl;
			exit(1);
		}
		const json& vt = value["value-type"];
		if (vt["base-type"]["type-name"] != def.name) {
			cerr << locPrefix(item) << PlnSaMessage::getMessage(E_CopyShapeMismatch,
				typeDisplayName(structVT), typeDisplayName(vt)) << endl;
			exit(1);
		} // LCOV_EXCL_LINE -- only cleanup skipped by exit(1) is attributed here
		return value;
	}
	const json& items = item["items"];
	vector<const json*> given(def.fields.size(), nullptr);
	bool outside = outsideTypeModule(def.name);
	if (et == "arr-lit") {
		bool hidden = any_of(def.fields.begin(), def.fields.end(), [](const FieldLayout& f){ return !f.access.empty(); });
		if (outside && hidden) {
			cerr << locPrefix(item) << PlnSaMessage::getMessage(E_StructLitPositionalHidden,
				PlnSaMessage::writtenTypeName(def.name)) << endl;
			exit(1);
		}
		if (items.size() != def.fields.size()) {
			cerr << locPrefix(item) << PlnSaMessage::getMessage(E_StructLitFieldCount,
			                                  structDisplayName(def.name), to_string(def.fields.size()), to_string(items.size())) << endl;
			exit(1);
		}
		for (size_t k = 0; k < items.size(); k++)
			given[k] = &items[k];
	} else {
		for (auto& named : items) {
			string fn = named["name"].get<string>();
			auto it = find_if(def.fields.begin(), def.fields.end(), [&](const FieldLayout& f){ return f.name == fn; });
			if (it == def.fields.end()) {
				cerr << locPrefix(named) << PlnSaMessage::getMessage(E_UnknownField, structDisplayName(def.name), fn, def.keyword()) << endl;
				exit(1);
			}
			if (outside && !it->access.empty()) {
				cerr << locPrefix(named) << PlnSaMessage::getMessage(E_StructLitHiddenField,
					PlnSaMessage::writtenTypeName(def.name), fn, it->access) << endl;
				exit(1);
			}
			const json*& slot = given[it - def.fields.begin()];
			if (slot) {
				cerr << locPrefix(named) << PlnSaMessage::getMessage(E_StructLitDupField, fn) << endl;
				exit(1);
			}
			slot = &named["value"];
		}
		for (size_t k = 0; k < given.size(); k++) {
			if (!given[k] && !def.fields[k].initAst.is_null()) given[k] = &def.fields[k].initAst;
			if (!given[k] && outside && !def.fields[k].access.empty()) {
				cerr << locPrefix(item) << PlnSaMessage::getMessage(E_StructLitHiddenNoInit,
					PlnSaMessage::writtenTypeName(def.name), def.fields[k].name, def.fields[k].access) << endl;
				exit(1);
			}
			if (!given[k]) {
				cerr << locPrefix(item) << PlnSaMessage::getMessage(E_StructLitMissingField, structDisplayName(def.name), def.fields[k].name) << endl;
				exit(1);
			}
		}
	}

	json values = json::array();
	for (size_t k = 0; k < def.fields.size(); k++) {
		const FieldLayout& f = def.fields[k];
		const json& v = *given[k];
		if (!f.unsupported.empty()) {
			cerr << locPrefix(v) << PlnSaMessage::getMessage(E_UnsupportedCFieldType, structDisplayName(def.name), f.name, f.unsupported) << endl;
			exit(1);
		}
		string structName = fieldStructName(f);
		if (!structName.empty()) {
			values.push_back(sa_struct_lit(v, requireCompleteStruct(structName, v)));
			continue;
		}
		if (isLitArrField(f)) {
			string vet = v.value("expr-type", "");
			if (vet == "dict-lit") {
				cerr << locPrefix(v) << PlnSaMessage::getMessage(E_DictLitContext) << endl;
				exit(1);
			}
			if (vet != "arr-lit") {
				cerr << locPrefix(v) << PlnSaMessage::getMessage(E_ArrLitDimMismatch, f.name) << endl;
				exit(1);
			}
			ArrLitShape shape = arrLitShape(v, f.name, fieldValueType(f), true);
			values.push_back(sa_arr_lit_values(v, shape, f.name));
			checkArrLitCount(v, v["items"], f.name, shape);
			continue;
		}
		if (f.typeKind != "prim") {
			cerr << locPrefix(v) << PlnSaMessage::getMessage(E_StructLitFieldType, structDisplayName(def.name), f.name) << endl;
			exit(1);
		}
		json fieldType = fieldValueType(f);
		const PlnType* toType = registry_.fromJson(fieldType);
		json value = sa_expression(v, toType);
		values.push_back(convertForBinding(v, value, toType, fieldType));
	}
	return values;
} // LCOV_EXCL_BR_LINE

void PlnSemanticAnalyzer::emitStructLitAssigns(const json& objAst, const StructDef& def, const json& values, json& out)
{
	for (size_t k = 0; k < def.fields.size(); k++) {
		const FieldLayout& f = def.fields[k];
		if (f.typeKind == "prim") {
			out.push_back(makeFieldAssign(resolveObjectChain(objAst, /*forWrite=*/true), f, values[k]));
			continue;
		}
		json fieldAst = {{"expr-type", "field-access"}, {"object", objAst}, {"field", f.name}}; // LCOV_EXCL_BR_LINE
		if (isLitArrField(f))
			emitArrLitAssigns(fieldAst, arrLitShape(objAst, f.name, fieldValueType(f), true), values[k], out, false); // LCOV_EXCL_BR_LINE
		else if (values[k].is_object())
			out.push_back(makeCopyStmt(objAst, makeFieldAccess(resolveObjectChain(objAst, /*forWrite=*/true), f), values[k], CopyInto::New)); // LCOV_EXCL_BR_LINE
		else
			emitStructLitAssigns(fieldAst, requireCompleteStruct(fieldStructName(f), objAst), values[k], out);
	}
} // LCOV_EXCL_BR_LINE

void PlnSemanticAnalyzer::enterBlockDecls(const json& stmts)
{
	vector<string> names;
	set<string> vars;
	for (auto& stmt : stmts) {
		addDeclaredVarNames(stmt, vars);
		string t = stmt.value("stmt-type", "");
		if (!isTypeDecl(t) && t != "const-decl") continue;
		string name = stmt["name"].get<string>();
		bool varTaken = t == "const-decl" && (findVar(name) || vars.count(name));
		if (varTaken || isDeclNameTaken(name) || find(names.begin(), names.end(), name) != names.end()) {
			cerr << locPrefix(stmt) << PlnSaMessage::getMessage(
				t == "const-decl" ? E_DuplicateConstDecl : E_DuplicateTypeName, name) << endl;
			exit(1);
		}
		names.push_back(name);
		topDecls_.emplace(name, TopDecl{&stmt});
		if (declaresStructType(t) || t == "enum-def") declareTypeIdentity(stmt, ++blockTypeSeq_);
	}
	blockDeclNames_.push_back(names);
	blockImportedTypes_.emplace_back();
	for (auto& name : names) ensureTypeName(name, false);
} // LCOV_EXCL_BR_LINE

void PlnSemanticAnalyzer::declareTypeIdentity(const json& stmt, int seq)
{
	string name = stmt["name"].get<string>();
	string internal = isAllocatorsModule(moduleId_) ? name : PlnSaMessage::typeIdentity(moduleId_, seq, name);
	typeIdentities_[name] = internal;
	// A pointer may name the struct before its layout is resolved.
	if (declaresStructType(stmt["stmt-type"])) {
		typeAliases_[name] = {{"type-kind", "prim"}, {"type-name", internal}}; // LCOV_EXCL_BR_LINE
		declaredStructs_.insert(internal);
	}
} // LCOV_EXCL_BR_LINE

void PlnSemanticAnalyzer::leaveBlockDecls()
{
	for (auto& name : blockDeclNames_.back()) {
		topDecls_.erase(name);
		typeAliases_.erase(name);
		constDecls_.erase(name);
		typeIdentities_.erase(name);
	}
	blockDeclNames_.pop_back();
	// An imported type stays registered: a type the block used may depend on it.
	for (auto& written : blockImportedTypes_.back()) typeAliases_.erase(written);
	blockImportedTypes_.pop_back();
}

string PlnSemanticAnalyzer::declaredTypeName(const string& written) const
{
	auto it = typeIdentities_.find(written);
	return it == typeIdentities_.end() ? written : it->second;
}

string PlnSemanticAnalyzer::topTypeIdentity(const string& written) const
{
	return isAllocatorsModule(moduleId_) ? written : PlnSaMessage::typeIdentity(moduleId_, 0, written);
}

bool PlnSemanticAnalyzer::isDeclNameTaken(const string& name) const
{
	return topDecls_.count(name) || isWrittenNameTaken(name);
}

// A const is visible from the top of its block, so a variable declared
// before it in the same block still clashes with it.
void PlnSemanticAnalyzer::addDeclaredVarNames(const json& stmt, set<string>& names)
{
	string t = stmt.value("stmt-type", "");
	if (t != "var-decl" && t != "tapple-decl") return;
	const char* key = t == "var-decl" ? "name" : "var-name";
	for (auto& v : stmt["vars"]) names.insert(v[key].get<string>());
}

// A Palan type registered only as another type's dependency, or as `V.T`,
// is not written by its name here, so a declaration may reuse that name.
bool PlnSemanticAnalyzer::isWrittenNameTaken(const string& name) const
{
	bool isCType = (structDefs_.count(name) || enumDefs_.count(name)) && !typeOrigins_.count(name);
	return importedTypes_.count(name) || typeAliases_.count(name) || isCType
		|| constDecls_.count(name) || elemSizeBytes(name) >= 0;
}

json PlnSemanticAnalyzer::sa_struct_def(const json& stmt)
{
	string written = stmt["name"].get<string>();
	string name = declaredTypeName(written);
	declareTypeName(name, stmt);
	// A local struct was bound to its program-wide name when declared.
	typeAliases_.emplace(written, json{{"type-kind", "prim"}, {"type-name", name}}); // LCOV_EXCL_BR_LINE
	json fields = stmt["fields"];
	for (auto& f : fields) {
		f["var-type"] = resolveWrittenType(stmt, f["var-type"]);
		// Only a pointee may name a struct that is not registered yet; every
		// other field type is checked by buildStructDef against structDefs_.
		const json* type = &f["var-type"];
		if (type->value("type-kind", "") == "embed") type = &(*type)["base-type"];
		const json* t = type;
		while (t->value("type-kind", "") == "arr") t = &(*t)["base-type"];
		if (t->value("type-kind", "") != "pntr") continue;
		string pointee = pointeeLeaf(*t).type->value("type-name", "");
		bool laterDef = pointee == name || declaredStructs_.count(pointee);
		if (!laterDef) requireKnownTypeNames(stmt, *type);
		json* leaf = const_cast<json*>(t);
		while (leaf->value("type-kind", "") == "pntr") leaf = &(*leaf)["base-type"];
		if (leaf->value("type-kind", "") == "func") *leaf = deepNormalizePrimToStruct(stmt, *leaf);
	}
	StructDef def = buildStructDef(name, fields, stmt);
	if (copyFuncStructs_.count(&stmt)) def.copyFunc = copyFuncSymbol(name);
	for (auto& f : fields) {
		if (!f.contains("access")) continue;
		if (!stmt.value("export", false)) {
			cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_FieldAccessNotExported,
				f["access"].get<string>(), f["name"].get<string>()) << endl;
			exit(1);
		}
		for (auto& fl : def.fields)
			if (fl.name == f["name"].get<string>()) fl.access = f["access"].get<string>();
	}
	structDefs_[name] = move(def);
	return json::array();
} // LCOV_EXCL_BR_LINE

// c2ast has no concept of Palan's $T inline-embedding sugar, so it represents
// a C struct/union-by-value field as a plain tag; translate the normalizeCType'd
// "struct" to the "embed" shape buildStructDef expects.
static json cFieldVarType(const json& vtype)
{
	auto isStruct = [](const json& t) { return t.value("type-kind", "") == "struct"; };
	if (isStruct(vtype)) {
		return {{"type-kind", "embed"}, // LCOV_EXCL_BR_LINE
		        {"base-type", {{"type-kind", "prim"}, {"type-name", vtype["type-name"]}}}}; // LCOV_EXCL_BR_LINE
	}
	if (vtype.value("type-kind", "") != "arr" || !vtype.contains("base-type"))
		return vtype;
	// A C array lies in its struct, as Palan's `$[n]T` does; c2ast marks every
	// level "embedded", which in Palan marks only the levels whose elements
	// (rows, struct values) are laid out in place.
	function<json(json)> level = [&](json t) { // LCOV_EXCL_LINE -- only exception cleanup is attributed here
		json& bt = t["base-type"];
		if (bt.value("type-kind", "") == "arr" && bt.contains("base-type")) {
			bt = level(move(bt));
		} else if (isStruct(bt)) {
			bt = {{"type-kind", "prim"}, {"type-name", bt["type-name"]}}; // LCOV_EXCL_BR_LINE
		} else {
			t.erase("embedded");
		}
		return t;
	}; // LCOV_EXCL_BR_LINE
	return {{"type-kind", "embed"}, {"base-type", level(vtype)}}; // LCOV_EXCL_BR_LINE
} // LCOV_EXCL_BR_LINE

// An unsupported field type (e.g. a C bitfield) registers its
// owning tag as an incomplete struct -- usable only through @T/@!T -- rather
// than exit(1), since cinclude pulls in glibc-internal structs never
// referenced by Palan interop code. A field naming a still-incomplete struct
// is rejected the same way.
static bool isSupportedCFieldType(const json& vtype, const map<string, StructDef>& structDefs,
                                   const string& ownerName)
{
	auto isCompleteStruct = [&](const string& name) {
		return name != ownerName && structDefs.count(name) && structDefs.at(name).isComplete;
	};
	string tk = vtype.value("type-kind", "");
	if (tk == "prim")
		return elemSizeBytes(vtype.value("type-name", "")) >= 0;
	if (tk != "embed")
		return tk == "pntr";
	const json* t = &vtype["base-type"];
	for (; t->value("type-kind", "") == "arr"; t = &(*t)["base-type"]) {
		if (!t->contains("size-expr") || !t->contains("base-type")) return false;
		// e.g. "T name[];" -- not a constant size
		string set = (*t)["size-expr"].is_null() ? "" : (*t)["size-expr"].value("expr-type", "");
		if (set != "lit-int" && set != "lit-uint") return false;
	}
	if (t->value("type-kind", "") == "pntr") return true;
	string leaf = t->value("type-name", "");
	return elemSizeBytes(leaf) >= 0 || isCompleteStruct(leaf);
}

void PlnSemanticAnalyzer::registerCStruct(const json& cincludeStmt, const json& s)
{
	string name = s["name"].get<string>();
	requireNotPalanType(name, cincludeStmt);
	if (s.contains("typedef-name"))
		cStructDisplayNames_[name] = s["typedef-name"].get<string>();
	auto existing = structDefs_.find(name);
	if (existing != structDefs_.end() && existing->second.isComplete)
		return;  // first complete definition wins

	if (!s.contains("fields")) {
		// A forward-declared or bare-referenced tag registers as incomplete
		// rather than not at all, so requireCompleteStruct can name it in a
		// diagnostic instead of leaving it unresolved.
		if (existing == structDefs_.end()) {
			StructDef def;
			def.name             = name;
			def.isUnion          = s.value("union", false);
			def.incompleteReason = "forward-declared";
			structDefs_[name] = def;
		}
		return;
	}

	json fields = json::array();
	for (auto& f : s["fields"]) {
		json vt = f["var-type"];
		resolveCTypeRefs(vt);
		vt = cFieldVarType(normalizeCType(vt));
		bool unknownWidth = f.contains("bits") && f["bits"].is_null();
		if (unknownWidth || !isSupportedCFieldType(vt, structDefs_, name)) {
			// Register the tag as an incomplete struct rather than not at all: the
			// name is real (glibc defines it), only its layout is unavailable this
			// version. This lets SA report E_IncompleteStructType -- distinct from
			// "no such type" -- and still allows @T/@!T use of the tag.
			StructDef def;
			def.name             = name;
			def.isUnion          = s.value("union", false);
			def.incompleteReason = "unsupported-field";
			structDefs_[name] = def;
			return;
		}
		json field = {{"var-type", vt}}; // LCOV_EXCL_BR_LINE
		for (const char* key : {"name", "packed", "aligned", "bits"})
			if (f.contains(key)) field[key] = f[key];
		fields.push_back(field);
	}
	structDefs_[name] = buildStructDef(name, fields, cincludeStmt,
	                                   s.value("union", false), s.value("packed", false), s.value("aligned", 1));
} // LCOV_EXCL_BR_LINE

json PlnSemanticAnalyzer::sa_type_alias(const json& stmt)
{
	// An alias has no program-wide name to claim: it is only written.
	string name = stmt["name"].get<string>();
	json type = resolveWrittenType(stmt, stmt["type"]);
	foldTypeSizes(type);
	typeAliases_[name] = move(type);
	return json::array();
}

json PlnSemanticAnalyzer::sa_opaque_def(const json& stmt)
{
	string written = stmt["name"].get<string>();
	string name = declaredTypeName(written);
	declareTypeName(name, stmt);
	typeAliases_.emplace(written, json{{"type-kind", "prim"}, {"type-name", name}}); // LCOV_EXCL_BR_LINE
	StructDef def;
	def.name = name;
	def.isOpaque = true;
	def.releaseFunc = releaseFuncSymbol(name);
	if (stmt.contains("contents")) {
		json resolved = resolveKnownType(stmt, stmt["contents"]);
		bool ownable = resolved.value("type-kind", "") == "arr"
			|| (isStructType(resolved) && (structDefs_.at(resolved["type-name"].get<string>()).isComplete
			                               || structDefs_.at(resolved["type-name"].get<string>()).isOpaque));
		json owned = ownable ? ownedTypeOf(stmt, resolved) : json();
		if (owned.is_null() || !isOwnedBlock(owned)) {
			cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_OpaqueContents,
				typeDisplayName(stmt["contents"])) << endl;
			exit(1);
		}
		def.contents = move(resolved);
		def.contentsOwned = move(owned);
	}
	structDefs_[name] = move(def);
	return json::array();
} // LCOV_EXCL_BR_LINE

// An alias is written where other names are in scope, even in another module,
// so its sizes take the values their names have where it is declared.
void PlnSemanticAnalyzer::foldTypeSizes(json& type)
{
	if (type.is_array()) {
		for (auto& e : type) foldTypeSizes(e);
		return;
	}
	if (!type.is_object()) return;
	for (auto& [key, value] : type.items()) {
		if (key == "size-expr" && !value.is_null()) value = foldConstExpr(value);
		else foldTypeSizes(value);
	}
}

json PlnSemanticAnalyzer::sa_const_decl(const json& stmt)
{
	string name = stmt["name"].get<string>();
	// Stored untyped so each reference is typed by its own context, exactly
	// like the literal written in place of the name.
	json value = foldConstExpr(stmt["value"]);
	string et = sa_expression(value).value("expr-type", "");
	if (et != "lit-int" && et != "lit-uint" && et != "lit-flo" && et != "lit-str") {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_ConstNotCompileTimeValue, name) << endl;
		exit(1);
	}
	value.erase("loc");
	constDecls_[name] = value;
	return json::array();
}

json PlnSemanticAnalyzer::sa_enum_def(const json& stmt)
{
	string written = stmt["name"].get<string>();
	string name = declaredTypeName(written);
	declareTypeName(name, stmt);

	map<string, int64_t> values;
	int64_t next = 0;
	bool nextOverflows = false;
	for (auto& e : stmt["enumerators"]) {
		string ename = e["name"].get<string>();
		int64_t v = next;
		if (e.contains("value")) {
			json lit = foldConstExpr(e["value"]);
			if (lit.value("expr-type", "") != "lit-int" && lit.value("expr-type", "") != "lit-uint") {
				cerr << locPrefix(e) << PlnSaMessage::getMessage(E_EnumValueNotConst, ename) << endl;
				exit(1);
			}
			string text = lit["value"].get<string>();
			try { v = stoll(text); }
			catch (const out_of_range&) {
				cerr << locPrefix(e) << PlnSaMessage::getMessage(E_EnumValueOutOfRange, ename, text) << endl;
				exit(1);
			} // LCOV_EXCL_LINE -- only cleanup skipped by exit(1) is attributed here
		} else if (nextOverflows) {
			cerr << locPrefix(e) << PlnSaMessage::getMessage(E_EnumValueOutOfRange, ename, "9223372036854775808") << endl;
			exit(1);
		}
		if (!values.emplace(ename, v).second) {
			cerr << locPrefix(e) << PlnSaMessage::getMessage(E_DuplicateEnumerator, ename, name) << endl;
			exit(1);
		}
		nextOverflows = v == INT64_MAX;
		next = nextOverflows ? v : v + 1;
	}
	registerEnum(name, move(values), stmt["loc"], written);
	return json::array();
} // LCOV_EXCL_BR_LINE

void PlnSemanticAnalyzer::registerEnum(const string& name, map<string, int64_t> values, const json& loc,
                                       const string& written)
{
	int64_t minV = 0, maxV = 0;
	bool first = true;
	for (auto& [ename, v] : values) {
		minV = first ? v : min(minV, v);
		maxV = first ? v : max(maxV, v);
		first = false;
	}
	// C types an enumerator as int when it fits, and GCC widens past that the
	// same way; the width always matches the C enum's, so values pass to and
	// from C unchanged.
	string base = minV >= INT32_MIN && maxV <= INT32_MAX ? "int32"
		: minV >= 0 && maxV <= UINT32_MAX ? "uint32"
		: minV < 0 ? "int64" : "uint64";
	enumDefs_[name] = {move(values), loc};
	typeAliases_[written] = {{"type-kind", "prim"}, {"type-name", base}, {"enum", name}}; // LCOV_EXCL_BR_LINE
} // LCOV_EXCL_BR_LINE

void PlnSemanticAnalyzer::registerCEnum(const json& cincludeStmt, const json& e)
{
	string name = e["name"].get<string>();
	auto existing = enumDefs_.find(name);
	if (existing != enumDefs_.end() && existing->second.loc.is_null())
		return;  // first definition wins, as for a C struct
	if (existing != enumDefs_.end() || structDefs_.count(name) || typeAliases_.count(name)) {
		cerr << locPrefix(cincludeStmt) << PlnSaMessage::getMessage(E_DuplicateTypeName, name) << endl;
		exit(1);
	}
	map<string, int64_t> values;
	for (auto& en : e["enumerators"])
		values[en["name"].get<string>()] = stoll(en["value"].get<string>());
	registerEnum(name, move(values), json(), name);
} // LCOV_EXCL_BR_LINE

// Each cinclude is a separate c2ast run, so a header relying on an earlier
// one's typedef (jpeglib.h on stdio.h's size_t) gets "user" for it; that is
// replaced here by what c2ast would have emitted had both been one
// translation unit. An unregistered enum (c2ast couldn't compute its values)
// or unknown "user" name stays as is, reported as unrepresentable where used.
void PlnSemanticAnalyzer::resolveCTypeRefs(json& node) const
{
	if (node.is_array()) {
		for (auto& e : node) resolveCTypeRefs(e);
		return;
	}
	if (!node.is_object()) return;
	if (node.value("type-kind", "") == "user" && node.contains("type-name")) {
		auto it = cTypedefs_.find(node["type-name"].get<string>());
		if (it == cTypedefs_.end()) return;
		bool isConst = node.value("const", false);
		node = it->second;
		if (isConst) node["const"] = true;
		return;
	}
	if (node.value("type-kind", "") == "enum" && node.contains("type-name")) {
		json t = enumTypeNamed(node["type-name"].get<string>());
		if (t.is_null()) return;
		if (node.value("const", false)) t["const"] = true;
		node = move(t);
		return;
	}
	for (auto& [key, val] : node.items()) resolveCTypeRefs(val);
} // LCOV_EXCL_BR_LINE

const json* PlnSemanticAnalyzer::findTypeBinding(const string& written) const
{
	auto it = typeAliases_.find(written);
	return it == typeAliases_.end() ? nullptr : &it->second;
}

// Every Palan type source may write here is bound in typeAliases_; an
// unbound one is registered only as another type's dependency, or as `V.T`.
void PlnSemanticAnalyzer::requireWritableTypeName(const json& locNode, const string& written) const
{
	if (typeAliases_.count(written)) return;
	auto it = hiddenTypeNames_.find(written);
	if (it == hiddenTypeNames_.end()) return;
	cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_UnqualifiedAliasType, written, it->second) << endl;
	exit(1);
}

// A binding already holds program-wide names, so it is never looked up again:
// in another scope the same string may name a different type. Leaves a name
// as "prim" rather than "struct", so callers' structDefs_.count(type-name)
// checks keep working.
json PlnSemanticAnalyzer::resolveWrittenType(const json& locNode, const json& type) const
{
	string tk = type.value("type-kind", "");
	if (tk == "prim") {
		string tname = type.value("type-name", "");
		requireWritableTypeName(locNode, tname);
		const json* bound = findTypeBinding(tname);
		return bound ? *bound : type;
	}
	json resolved = type;
	if ((tk == "pntr" || tk == "arr" || tk == "embed") && type.contains("base-type"))
		resolved["base-type"] = resolveWrittenType(locNode, type["base-type"]);
	if (tk == "func") {
		for (auto& p : resolved["parameters"]) p["var-type"] = resolveWrittenType(locNode, p["var-type"]);
		resolved["ret-type"] = resolveWrittenType(locNode, resolved["ret-type"]);
	}
	return resolved;
}

json PlnSemanticAnalyzer::resolveKnownType(const json& locNode, const json& type) const
{
	json resolved = resolveWrittenType(locNode, type);
	requireKnownTypeNames(locNode, resolved);
	return resolved;
}

bool PlnSemanticAnalyzer::claimTypeName(const string& name, const string& origin, const json& locNode)
{
	auto [it, isNew] = typeOrigins_.emplace(name, origin);
	// Every Palan struct and enum is in typeOrigins_, so a new name already
	// registered here is a C type or a primitive.
	bool taken = isNew && (structDefs_.count(name) || enumDefs_.count(name) || elemSizeBytes(name) >= 0);
	if (taken || (!isNew && it->second != origin)) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_DuplicateTypeName, name) << endl;
		exit(1);
	}
	return isNew;
}

// Unlike an import, which may bring the same type in twice, a declaration
// claims its name exactly once.
void PlnSemanticAnalyzer::declareTypeName(const string& name, const json& stmt)
{
	if (!claimTypeName(name, moduleId_, stmt)) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_DuplicateTypeName, name) << endl;
		exit(1);
	}
}

bool PlnSemanticAnalyzer::namesPalanType(const json& binding) const
{
	return typeOrigins_.count(binding.value("type-name", "")) || typeOrigins_.count(binding.value("enum", ""));
}

// A cinclude inside a block runs after the Palan types are declared.
void PlnSemanticAnalyzer::requireNotPalanType(const string& name, const json& locNode) const
{
	auto d = topDecls_.find(name);
	bool isAlias = d != topDecls_.end() && (*d->second.stmt)["stmt-type"] == "type-alias";
	auto b = typeAliases_.find(name);
	if (typeIdentities_.count(name) || (b != typeAliases_.end() && (isAlias || namesPalanType(b->second)))) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_DuplicateTypeName, name) << endl;
		exit(1);
	}
}

// The same type may come in through two imports. A C type's name is written
// as is, so an imported Palan type cannot be written by it too.
void PlnSemanticAnalyzer::bindImportedTypeName(const json& locNode, const string& name, const json& type)
{
	bool cType = (structDefs_.count(name) || enumDefs_.count(name)) && namesPalanType(type);
	auto [it, isNew] = typeAliases_.emplace(name, type);
	if (cType || (!isNew && it->second != type)) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_DuplicateTypeName, name) << endl;
		exit(1);
	}
}

void PlnSemanticAnalyzer::setTypeNameable(const string& name, bool nameable, const string& alias)
{
	if (nameable)
		hiddenTypeNames_.erase(name);
	else if (!alias.empty() && !typeAliases_.count(name))
		hiddenTypeNames_.emplace(name, alias);
}

// Registers the types `stmt` imports, as the exporting module resolved them.
void PlnSemanticAnalyzer::adoptExportedTypes(const json& stmt, const json& impAst, PlnSemanticAnalyzer& sub)
{
	if (!impAst["ast"].contains("statements")) return;
	set<string> targets;
	bool selective = stmt.contains("targets");
	if (selective)
		for (auto& t : stmt["targets"])
			targets.insert(t.get<string>());

	string alias = stmt.value("alias", "");
	for (auto& d : impAst["ast"]["statements"]) {
		if (!d.value("export", false) || !isTypeDecl(d["stmt-type"])) continue;
		string name = d["name"].get<string>();
		if (selective && !targets.count(name)) continue;
		string written = alias.empty() ? name : alias + "." + name;
		bool wasWritable = typeAliases_.count(written);
		adoptExportedType(stmt, d, sub);
		if (!blockImportedTypes_.empty() && !wasWritable) blockImportedTypes_.back().push_back(written);
	}
}

void PlnSemanticAnalyzer::adoptExportedType(const json& stmt, const json& decl, PlnSemanticAnalyzer& sub)
{
	string alias = stmt.value("alias", "");
	string name = decl["name"].get<string>();
	sub.ensureTypeName(name, false);
	string kind = decl["stmt-type"].get<string>();
	if (declaresStructType(kind)) {
		string id = sub.topTypeIdentity(name);
		adoptStruct(sub, id, name, alias, stmt);
		if (!alias.empty())
			typeAliases_[alias + "." + name] = {{"type-kind", "prim"}, {"type-name", id}}; // LCOV_EXCL_BR_LINE
	} else if (kind == "enum-def") {
		adoptEnum(sub, sub.topTypeIdentity(name), name, alias, stmt);
		if (!alias.empty())
			typeAliases_[alias + "." + name] = sub.typeAliases_.at(name);
	} else {
		const json& t = sub.typeAliases_.at(name);
		adoptTypeDeps(sub, t, stmt);
		if (alias.empty()) {
			bindImportedTypeName(stmt, name, t);
			setTypeNameable(name, true, alias);
		} else {
			typeAliases_[alias + "." + name] = t;
		}
	}
} // LCOV_EXCL_BR_LINE

// A struct comes with every struct and enum its fields use, nameable or not:
// its layout and allocator need them. `written` is the imported type's name in
// its module, empty for one adopted only as a dependency.
void PlnSemanticAnalyzer::adoptStruct(PlnSemanticAnalyzer& sub, const string& name, const string& written,
		const string& alias, const json& locNode)
{
	auto origin = sub.typeOrigins_.find(name);
	// This module's own struct, which it resolves itself.
	if (origin != sub.typeOrigins_.end() && origin->second == moduleId_) return;
	const StructDef& def = sub.structDefs_.at(name);
	bool adopted;
	if (origin == sub.typeOrigins_.end()) {
		// A C struct, global as in C: first complete definition wins, as for a cinclude.
		auto it = structDefs_.find(name);
		adopted = it == structDefs_.end() || (!it->second.isComplete && def.isComplete);
		if (adopted) structDefs_[name] = def;
	} else {
		adopted = claimTypeName(name, origin->second, locNode);
		if (adopted) structDefs_[name] = def;
		if (!written.empty() && alias.empty())
			bindImportedTypeName(locNode, written, {{"type-kind", "prim"}, {"type-name", name}}); // LCOV_EXCL_BR_LINE
		if (!written.empty()) setTypeNameable(written, alias.empty(), alias);
	}
	if (!adopted) return;
	for (auto& f : def.fields) {
		if (f.typeKind == "owned" || f.typeKind == "embed-arr" || f.typeKind == "embed-varr") {
			adoptVarTypeDeps(sub, f.type, locNode);
			continue;
		}
		if (sub.structDefs_.count(f.typeName)) {
			adoptStruct(sub, f.typeName, "", "", locNode);
		} else if (f.elemKind == "struct" && sub.declaredStructs_.count(f.typeName)) {
			// A pointee the exporting module has yet to resolve.
			string pointee = f.typeName;
			const json* loc = &locNode;
			whenSettled([this, &sub, pointee, loc] { // LCOV_EXCL_BR_LINE
				sub.ensureStructDef(pointee);
				adoptStruct(sub, pointee, "", "", *loc);
			});
		}
		if (!f.enumName.empty()) adoptEnum(sub, f.enumName, "", "", locNode);
		if (f.elemKind == "func") adoptTypeDeps(sub, f.funcType, locNode);
	}
} // LCOV_EXCL_BR_LINE

void PlnSemanticAnalyzer::adoptEnum(const PlnSemanticAnalyzer& sub, const string& name, const string& written,
		const string& alias, const json& locNode)
{
	auto origin = sub.typeOrigins_.find(name);
	if (origin != sub.typeOrigins_.end() && origin->second == moduleId_) return;
	if (origin == sub.typeOrigins_.end()) {
		if (!enumDefs_.count(name)) {
			enumDefs_[name]   = sub.enumDefs_.at(name);
			typeAliases_[name] = sub.typeAliases_.at(name);
		}
		return;
	}
	bool adopted = claimTypeName(name, origin->second, locNode);
	if (adopted) enumDefs_[name] = sub.enumDefs_.at(name);
	if (written.empty()) return;
	if (alias.empty()) bindImportedTypeName(locNode, written, sub.typeAliases_.at(written));
	setTypeNameable(written, alias.empty(), alias);
}

// The defining module got the builtins of each `[+]U` level from
// varrStorageType; an access here needs them too.
void PlnSemanticAnalyzer::adoptVarTypeDeps(PlnSemanticAnalyzer& sub, const json& type, const json& locNode)
{
	adoptTypeDeps(sub, type, locNode);
	for (const json* t = &type; t->contains("base-type"); t = &(*t)["base-type"])
		if (isVarrStorage(*t)) recordVarrShape(*t);
}

void PlnSemanticAnalyzer::adoptTypeDeps(PlnSemanticAnalyzer& sub, const json& type, const json& locNode)
{
	const json* t = &type;
	while (t->contains("base-type")) t = &(*t)["base-type"];
	string tname = t->value("type-name", "");
	if (sub.structDefs_.count(tname)) adoptStruct(sub, tname, "", "", locNode);
	if (t->contains("enum")) adoptEnum(sub, (*t)["enum"].get<string>(), "", "", locNode);
}

// A module seeing the contents allocates and copies them in a hidden owner of
// their type, which the variable then takes over, so its release is still the
// opaque type's.
json PlnSemanticAnalyzer::sa_opaque_var_decl(const json& stmt, VarStorage storage)
{
	// LCOV_EXCL_BR_START
	const string& typeName = stmt["vars"][0]["var-type"]["type-name"].get<string>();
	const StructDef& def = structDefs_.at(typeName);
	if (storage == VarStorage::Frame) requireCompleteStruct(typeName, stmt, "'$'");
	json vt = {{"type-kind","pntr"},{"base-type",{{"type-kind","struct"},{"type-name",typeName}}}};
	json result = json::array();
	for (auto& var : stmt["vars"]) {
		string name = var["name"].get<string>();
		json init, copySrc;
		if (var.contains("init")) {
			json given = sa_expression(var["init"]);
			if (given.value("category", "") == "opaque-convert") {
				cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_OpaqueConvertMove) << endl;
				exit(1);
			}
			if (isExpiringOwned(given) && given["value-type"] == vt) init = move(given);
			else copySrc = move(given);
		}
		if (init.is_null()) {
			if (!opaqueContentsVisible(def))
				requireCompleteStruct(typeName, stmt, copySrc.is_null() ? "a declaration without an initializer" : "a copy");
			string hidden = "__opq_" + to_string(tempVarCounter_++);
			json decl = stmt;
			decl["vars"] = json::array({{{"name",hidden},{"var-type",def.contents}}});
			for (auto& s : sa_resolved_var_decl(decl, {VarStorage::Heap})) result.push_back(move(s));
			const json& hvt = *findVar(hidden);
			json hiddenId = {{"expr-type","id"},{"name",hidden},{"var-type",hvt},{"value-type",hvt}};
			if (!copySrc.is_null())
				result.push_back(makeCopyStmt(stmt, hiddenId, opaqueDefOf(copySrc.value("value-type", json())) == &def
				                              ? asOpaqueContents(copySrc, def) : copySrc, CopyInto::New));
			result.push_back({{"stmt-type","var-decl"},{"vars",json::array({{{"name",name},{"var-type",vt},
				{"init",wrapConvert(hiddenId, vt)}}})}});
			result.push_back({{"stmt-type","assign"},{"name",hidden},
				{"value",{{"expr-type","lit-int"},{"value","0"},{"value-type",hvt}}}});
		} else {
			result.push_back({{"stmt-type","var-decl"},{"vars",json::array({{{"name",name},{"var-type",vt},{"init",init}}})}});
		}
		declareVar(name, vt, &stmt);
		addOwnedScopeVar(name, makeOwnedFreeStmt(name, vt));
	}
	return result;
	// LCOV_EXCL_BR_STOP
} // LCOV_EXCL_BR_LINE

json PlnSemanticAnalyzer::sa_struct_var_decl(const json& stmt, VarStorage storage)
{
	const string& structName = stmt["vars"][0]["var-type"]["type-name"].get<string>();
	if (structDefs_.at(structName).isOpaque) return sa_opaque_var_decl(stmt, storage);
	const StructDef& def = requireCompleteStruct(structName, stmt);
	json pntr_type = {{"type-kind","pntr"},
	                  {"base-type",{{"type-kind","struct"},{"type-name",structName}}}}; // LCOV_EXCL_BR_LINE
	json result = json::array();
	json sa_stmt = {{"stmt-type","var-decl"},{"vars",json::array()}}; // LCOV_EXCL_BR_LINE
	// sa_var_decl gives each initialized var its own run, so these only follow
	// the var-decl that allocates their storage.
	json initStmts = json::array();
	json post = json::array();
	// LCOV_EXCL_BR_START
	for (auto& var : stmt["vars"]) {
		string name = var["name"].get<string>();
		json copySrc;        // stays null unless `var` is initialized by copying another struct
		json returned;       // stays null unless a call returns `var`'s value into it
		json takenOver;      // stays null unless `var` takes over a block given by `<<=`
		if (var.contains("init")) {
			// Evaluate before declareVar: the initializer must not see the
			// variable it is initializing (same rule as sa_var_decl).
			json userInit = sa_expression(literalConversion(var["init"], pntr_type));
			bool sameStruct = userInit.contains("value-type")
				&& userInit["value-type"].value("base-type", json()) == pntr_type["base-type"];
			if (isStructRetCall(userInit) && sameStruct) {
				returned = move(userInit);
			} else if (isExpiringOwned(userInit) && sameStruct) {
				takenOver = move(userInit);
			} else {
				// A struct of another type is left to makeCopyStmt's shape check.
				if (!userInit.contains("value-type")
						|| (userInit.value("category", "") == "expiring" && !isExpiringOwned(userInit))) {
					cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_StructInitNotSupported, structName) << endl;
					exit(1);
				}
				copySrc = move(userInit);
			}
		}
		declareVar(name, pntr_type, &stmt);
		json var_id = {{"expr-type","id"},{"name",name},{"var-type",pntr_type},{"value-type",pntr_type}};

		json init;
		if (!takenOver.is_null()) {
			// takeOverInit rejects '<<=' into frame storage, and a call's struct
			// is `returned` instead, so a block is only taken over by the heap.
			BOOST_ASSERT(storage == VarStorage::Heap); // LCOV_EXCL_BR_LINE
			init = move(takenOver);
			addOwnedScopeVar(name, makeOwnedFreeStmt(name, pntr_type));
		} else {
			// The callee builds what a returned struct owns, so its storage is only allocated.
			init = placeAggregateVar(stmt, name, pntr_type, structAllocExpr(structName, returned.is_null()),
			                         makeOwnedFreeStmt(name, pntr_type), storage, def.totalSize, def.maxAlign,
			                         post, structShape(structName));
		}
		sa_stmt["vars"].push_back({{"name",name},{"var-type",pntr_type},{"init",init}});
		if (!returned.is_null())
			for (auto& st : bindStructRet(move(returned), var_id)) initStmts.push_back(move(st));
		if (!copySrc.is_null())
			initStmts.push_back(makeCopyStmt(stmt, var_id, copySrc, CopyInto::New));
	}
	result.push_back(sa_stmt);
	for (auto& s : post)
		result.push_back(s);
	for (auto& s : initStmts)
		result.push_back(s);
	// LCOV_EXCL_BR_STOP
	return result;
} // LCOV_EXCL_BR_LINE

const json* PlnSemanticAnalyzer::lookupVar(const string& name, const json& locNode)
{
	if (const json* vt = findVar(name)) return vt;
	const json* g = findCGlobal(name);
	if (!g) return nullptr;
	requireSupportedCGlobal(*g, name, locNode);
	bindStatic(name, name, (*g)["var-type"]);
	return &(*g)["var-type"];
}

void PlnSemanticAnalyzer::bindStatic(const string& name, const string& label, const json& type)
{
	for (auto& b : staticBindings_)
		if (b["name"] == name) return;
	staticBindings_.push_back({{"name", name}, {"label", label}, {"var-type", type}}); // LCOV_EXCL_BR_LINE
} // LCOV_EXCL_BR_LINE

bool PlnSemanticAnalyzer::isStaticBound(const string& name) const
{
	for (auto& b : staticBindings_)
		if (b["name"] == name) return true;
	return false;
}

// Only reached for a name lookupVar resolved, so one not in varScopes is a C global.
void PlnSemanticAnalyzer::requireAssignable(const string& name, const json& locNode) const
{
	if (forIndexVars_.count(name)) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_AssignToForIndex, name) << endl;
		exit(1);
	}
	auto ro = readOnlyGlobals_.find(name);
	if (ro != readOnlyGlobals_.end()) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(
			ro->second ? E_GlobalNotReplaceable : E_GlobalNotWritable, name) << endl;
		exit(1);
	}
	if (findVar(name)) return;
	auto p = cGlobalPermits_.find(name);
	bool writable = p != cGlobalPermits_.end()
		&& (!currentFunc_ || (p->second.count(permitFunc_) && p->second.at(permitFunc_)));
	if (!writable) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_GlobalNotWritable, name) << endl;
		exit(1);
	}
}

// A global's declaration lowers as a top-level variable's does, into the
// module's `..init`; its value then lives in static storage under its label
// instead of in the frame. The top level sees it as before, bound at its entry.
void PlnSemanticAnalyzer::sa_global_decl(const json& stmt)
{
	if (varScopes.size() != 1 || currentFunc_) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_GlobalNotTopLevel) << endl;
		exit(1);
	}
	json plain = stmt;
	plain.erase("global");
	plain.erase("export");
	string name = stmt["vars"][0]["name"];
	string label = symbolScope_ + "." + name;
	bool exported = stmt.value("export", false);
	json importedType = exported ? exportedGlobalType(stmt) : json();

	swap(varScopes, initVarScopes_);
	swap(arrayScopeVars_, initScopeVars_);
	swap(staticBindings_, initBindings_);
	globalInitTopScope_ = &initVarScopes_[0];
	globalInitName_ = name;
	bool labeled = false;
	for (auto& s : sa_var_decl(plain)) {
		if (!labeled && s.value("stmt-type", "") == "var-decl")
			for (auto& v : s["vars"])
				if (v["name"] == name) {
					v["label"] = label;
					if (v.contains("init") && v["init"].value("expr-type", "") == "frame-alloc") {
						v["init"]["expr-type"] = "static-alloc";
						v["init"]["label"] = label + ".data";
					}
					labeled = true;
				}
		initBody_.push_back(move(s));
	}
	BOOST_ASSERT(labeled);
	globalInitTopScope_ = nullptr;
	optional<ScopeVar> scopeVar;
	for (auto& v : arrayScopeVars_.back())
		if (v.name == name) {
			v.global = true;
			scopeVar = v;
		}
	json type = *findVar(name);
	swap(varScopes, initVarScopes_);
	swap(arrayScopeVars_, initScopeVars_);
	swap(staticBindings_, initBindings_);

	BOOST_ASSERT(!exported || importedType == type);

	bindGlobal(name, label, type, scopeVar, &stmt);
	registerGlobal(name, label, type, &stmt);
	json entry = {{"label", label}, {"var-type", type}}; // LCOV_EXCL_BR_LINE
	if (exported) entry["export"] = true; // LCOV_EXCL_BR_LINE
	sa["globals"].push_back(move(entry));
} // LCOV_EXCL_BR_LINE

void PlnSemanticAnalyzer::bindGlobal(const string& name, const string& label, const json& type,
                                     const optional<ScopeVar>& scopeVar, const json* locNode)
{
	declareVar(name, type, locNode);
	if (scopeVar) arrayScopeVars_.back().push_back(*scopeVar);
	bindStatic(name, label, type);
}

// After bindGlobal: a function's view of an owned block depends on its ScopeVar.
void PlnSemanticAnalyzer::registerGlobal(const string& name, const string& label, const json& type, const json* decl)
{
	GlobalVar g{label, type, decl, {}, {}}; // LCOV_EXCL_BR_LINE
	if (!isArrBorrowVar(type) && !isStructBorrow(type)) {
		g.view[0] = storageBorrowType(name, type, false);
		g.view[1] = storageBorrowType(name, type, true);
	}
	globals_[name] = move(g);
}

void PlnSemanticAnalyzer::sa_global_permit(const json& stmt)
{
	if (varScopes.size() != 1 || currentFunc_) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_GlobalNotTopLevel) << endl;
		exit(1);
	}
	string name = stmt["name"];
	map<string, bool>* funcs;
	auto g = globals_.find(name);
	size_t dot = name.find('.');
	if (g != globals_.end()) {
		funcs = &g->second.funcs;
	} else if (dot == string::npos && !findVar(name) && findCGlobal(name)) { // LCOV_EXCL_BR_LINE
		funcs = &cGlobalPermits_[name];
	} else {
		if (dot != string::npos) requireAlias(stmt, name.substr(0, dot));
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_GlobalPermitUnknownVar, name) << endl;
		exit(1);
	}
	for (auto& f : stmt["funcs"]) {
		string fn = f["name"];
		if (!topFuncSymbols_.count(fn)) {
			cerr << locPrefix(f) << PlnSaMessage::getMessage(E_GlobalPermitNotFunc, fn) << endl;
			exit(1);
		}
		if (funcs->count(fn)) {
			cerr << locPrefix(f) << PlnSaMessage::getMessage(E_GlobalPermitDuplicate, name, fn) << endl;
			exit(1);
		}
		(*funcs)[fn] = f["writable"].get<bool>();
	}
}

// A function sees a global as it would a parameter: an array or struct as a
// borrow of it, anything else with its own type.
void PlnSemanticAnalyzer::declarePermittedGlobals(const string& funcName, const json& funcDef)
{
	for (auto& [name, g] : globals_) {
		auto f = g.funcs.find(funcName);
		if (f == g.funcs.end()) continue;
		bool writable = f->second;
		const json& borrowed = g.view[writable];
		const json& type = borrowed.is_null() ? g.type : borrowed;
		declareVar(name, type, &funcDef);
		bindStatic(name, g.label, type);
		if (!borrowed.is_null()) readOnlyGlobals_[name] = true;
		else if (!writable) readOnlyGlobals_[name] = false;
	}
}

// Runs once: a circle of imports reaches it again while it runs. The flag is
// set first, so such a call returns at once.
void PlnSemanticAnalyzer::emitInitFunc()
{
	// LCOV_EXCL_BR_START
	string name = moduleId_ + "..init";
	json body = json::array();
	if (!initImports_.empty() || !initBody_.empty()) {
		json flagType = {{"type-kind","prim"},{"type-name","uint8"}};
		string flag = "__init_done";
		string label = name + ".done";
		sa["globals"].push_back({{"label", label}, {"var-type", flagType}});
		initBindings_.insert(initBindings_.begin(), json{{"name", flag}, {"label", label}, {"var-type", flagType}});
		json id = {{"expr-type","id"},{"name",flag},{"var-type",flagType},{"value-type",flagType}};
		auto lit = [&](const char* v) { return json{{"expr-type","lit-int"},{"value",v},{"value-type",flagType}}; };
		json cond = {{"expr-type","cmp"},{"op","!="},{"left",id},{"right",lit("0")},
			{"value-type",{{"type-kind","prim"},{"type-name","bool"}}}};
		body.push_back({{"stmt-type","if"},{"cond",move(cond)},
			{"then",{{"stmt-type","block"},{"body",json::array({{{"stmt-type","return"}}})}}}});
		body.push_back({{"stmt-type","assign"},{"name",flag},{"value",lit("1")}});
		for (auto& m : initImports_)
			body.push_back({{"stmt-type","expr"},{"body",{{"expr-type","call"},{"name",m + "..init"},
				{"func-type","palan"},{"args",json::array()}}}});
		for (auto& s : initBody_) body.push_back(move(s));
	}
	json func = {{"name",name},{"func-type","palan"},{"export",true},{"parameters",json::array()},{"body",move(body)}};
	if (!initBindings_.empty()) func["globals"] = initBindings_;
	sa["functions"].push_back(move(func));
	sa["init"] = name;
	// LCOV_EXCL_BR_STOP
}
