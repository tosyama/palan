/// Palan Semantic Analyzer — statement, control flow, and function analysis
///
/// @file PlnSaStmt.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <iostream>
#include <algorithm>
#include <functional>
#include <cstdint>
#include "PlnSemanticAnalyzer.h"
#include "PlnSaMessage.h"
#include "PlnSaInternal.h"

json PlnSemanticAnalyzer::sa_statements(const json& stmts)
{
	json result = json::array();
	for (auto& rawStmt : stmts) {
		// A type declaration is tracked by its address (prescannedDecls_).
		json resolved = qualifiedGlobals_ && !isTypeDecl(rawStmt["stmt-type"])
		              ? resolveQualifiedGlobals(rawStmt) : json();
		const json& stmt = resolved.is_null() ? rawStmt : resolved;
		string t = stmt["stmt-type"];
		if      (t == "import")   sa_import(stmt);
		else if (t == "cinclude") sa_cinclude(stmt);
		else if (t == "expr" && chainsFromStore(stmt["body"])) { for (auto& s : sa_store_scope(stmt)) result.push_back(s); }
		else if (t == "expr")     result.push_back(sa_expression_stmt(stmt));
		else if (t == "var-decl" && stmt.value("global", false)) sa_global_decl(stmt);
		else if (t == "var-decl") { for (auto& s : sa_var_decl(stmt)) result.push_back(s); }
		else if (t == "global-permit") sa_global_permit(stmt);
		else if (isStoreStmt(stmt)) { for (auto& s : sa_store_scope(stmt)) result.push_back(s); }
		else if (prescannedDecls_.count(&rawStmt)) continue;
		else if (t == "struct-def")   sa_struct_def(stmt);
		else if (t == "opaque-def")   sa_opaque_def(stmt);
		else if (t == "type-alias")   sa_type_alias(stmt);
		else if (t == "const-decl")   sa_const_decl(stmt);
		else if (t == "enum-def")     sa_enum_def(stmt);
		else if (t == "return") {
			if (funcBodyScopeIdx_ > 0) {
				json retStmt = stmt;
				for (auto& s : bindReturnLit(retStmt)) result.push_back(s);
				if (retStmt.contains("values") && retStmt["values"].size() == 1
						&& retStmt["values"][0].value("expr-type","") == "id") {
					string name = retStmt["values"][0]["name"];
					// An owned block return is a copy (copyReturnBlock); anything
					// else would point into the frame.
					if (isFrameVar(name) && !isOwnedBlock((*currentFunc_)["ret-type"])) {
						cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_ReturnFrameArr, name) << endl;
						exit(1);
					}
				}
				// The caller takes over a returned owned variable, so this return
				// leaves it out of its frees; other paths still free it.
				// A `$` struct moves into the return slot as an owned one does.
				string returned;
				if (retStmt.contains("values") && retStmt["values"].size() == 1
						&& retStmt["values"][0].value("expr-type","") == "id") {
					string name = retStmt["values"][0]["name"];
					if (isOwnedVar(name) || (!retSlot_.empty() && name != retSlot_ && isFrameVar(name)))
						returned = name;
				}
				json ret = sa_return_stmt(retStmt);
				// `return x as T` between an opaque type and its contents, or
				// setting an array's sizes, moves x.
				string retCategory = ret.contains("values") ? ret["values"][0].value("category", "") : "";
				if (returned.empty() && (retCategory == "opaque-convert" || retCategory == "resized")) {
					const json& conv = retStmt["values"][0]["src"];
					if (conv.value("expr-type", "") == "id" && isOwnedVar(conv["name"]))
						returned = conv["name"];
				}
				if (!retSlot_.empty() && ret.contains("values"))
					for (auto& s : returnIntoSlot(retStmt, ret, returned)) result.push_back(s);
				else if (returned.empty())
					for (auto& s : copyReturnBlock(retStmt, ret)) result.push_back(s);
				json frees = collectFreeStmts(funcBodyScopeIdx_, arrayScopeVars_.size(), returned);
				// The return value may read what the frees release, so it is
				// evaluated into a temp first.
				if (!frees.empty() && ret.contains("values"))
					result.push_back(bindReturnValueToTemp(retStmt, ret));
				for (auto& s : frees) result.push_back(s);
				result.push_back(ret);
			} else
				result.push_back(sa_return_stmt(stmt)); // LCOV_EXCL_BR_LINE
			given_.flow.reachable = false;
		}
		else if (t == "tapple-decl") { for (auto& s : sa_tapple_decl(stmt)) result.push_back(s); }
		else if (t == "tapple-assign" && chainsFromStore(stmt["value"])) { for (auto& s : sa_store_scope(stmt)) result.push_back(s); }
		else if (t == "tapple-assign") result.push_back(sa_tapple_assign(stmt));
		else if (t == "if")       result.push_back(sa_if_stmt(stmt));
		else if (t == "while")    result.push_back(sa_while_stmt(stmt));
		else if (t == "for")      result.push_back(sa_for_stmt(stmt));
		else if (t == "break") {
			if (!whileScopeStack_.empty()) {
				json frees = collectFreeStmts(whileScopeStack_.back(), arrayScopeVars_.size());
				for (auto& s : frees) result.push_back(s);
			}
			result.push_back(sa_break_stmt(stmt));
			// A function in a loop's body has no loop of its own here.
			if (!given_.loops.empty()) mergeGivenFlow(given_.loops.back().exit, given_.flow);
			given_.flow.reachable = false;
		}
		else if (t == "continue") {
			if (!whileScopeStack_.empty()) {
				json frees = collectFreeStmts(whileScopeStack_.back(), arrayScopeVars_.size());
				for (auto& s : frees) result.push_back(s);
				for (auto& s : loopSteps_.back()) result.push_back(s);
			}
			result.push_back(sa_continue_stmt(stmt));
			// A function in a loop's body has no loop of its own here.
			if (!given_.loops.empty()) mergeGivenFlow(given_.loops.back().back, given_.flow);
			given_.flow.reachable = false;
		}
		else if (t == "not-impl") {
			if (stmt.contains("untyped-var"))
				cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_VarTypeInferenceNotImpl, stmt["untyped-var"]) << endl;
			else
				cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_StmtNotImplemented) << endl;
			exit(1);
		}
		else if (t == "func-def") {
			cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_InternalError, "1") << endl;
			exit(1);
		}
		else if (t == "block")    result.push_back(sa_block(stmt));
		else {
			cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_InternalError, "2") << endl;
			exit(1);
		}
		applyPendingGiven();
	}
	return result;
}

// Null when nothing changes. A statement list (a block's, a loop's body) is
// left to sa_statements, which reaches it in its own scope; an `else if`'s
// condition is not, so it is resolved here with its `if`.
json PlnSemanticAnalyzer::resolveQualifiedGlobals(const json& node) const
{
	// LCOV_EXCL_BR_START
	// The global `M.count` that a field access, or a field-assign's target, names.
	auto globalOf = [&](const json& n) -> string {
		if (n.value("expr-type", "") != "field-access" && n.value("stmt-type", "") != "field-assign") return "";
		if (n["object"].value("expr-type", "") != "id") return "";
		string alias = typeNameWritten(n["object"]);
		if (alias.empty()) return "";
		string q = alias + "." + n["field"].get<string>();
		return findVar(q) || globals_.count(q) ? q : "";
	};
	if (node.is_object()) {
		if (node.value("stmt-type", "") == "block") return json();
		string q = globalOf(node);
		if (!q.empty()) {
			if (node.contains("expr-type")) {
				json id = {{"expr-type", "id"}, {"name", q}};
				if (node.contains("loc")) id["loc"] = node["loc"];
				return id;
			}
			json assign = node;
			assign["stmt-type"] = "assign";
			assign["name"] = q;
			assign.erase("object");
			assign.erase("field");
			json value = resolveQualifiedGlobals(assign["value"]);
			if (!value.is_null()) assign["value"] = move(value);
			return assign;
		}
		json out;
		for (auto& [k, v] : node.items()) {
			if (k == "functions") continue;
			json r = resolveQualifiedGlobals(v);
			if (r.is_null()) continue;
			if (out.is_null()) out = node;
			out[k] = move(r);
		}
		return out;
	}
	if (node.is_array()) {
		if (!node.empty() && node[0].is_object() && node[0].contains("stmt-type")) return json();
		json out;
		for (size_t i = 0; i < node.size(); i++) {
			json r = resolveQualifiedGlobals(node[i]);
			if (r.is_null()) continue;
			if (out.is_null()) out = node;
			out[i] = move(r);
		}
		return out;
	}
	return json();
	// LCOV_EXCL_BR_STOP
} // LCOV_EXCL_BR_LINE

void PlnSemanticAnalyzer::sa_functions(const json& funcs)
{
	for (auto& f : funcs) sa_function(f, true);
}

json PlnSemanticAnalyzer::sa_block(const json& stmt)
{
	enterScope();
	// Before the functions, whose signatures may name the block's types.
	enterBlockDecls(stmt["body"]);

	const json& funcs = stmt.value("functions", json::array());
	string savedSymbolScope = symbolScope_;
	if (!funcs.empty())
		symbolScope_ += "..b" + to_string(++blockSymbolSeq_);

	// pre-register block-local func-defs (forward reference support)
	for (auto& f : funcs) {
		if (f.value("export", false)) {
			cerr << locPrefix(f) << PlnSaMessage::getMessage(E_ExportInBlock, f["name"].get<string>()) << endl;
			exit(1);
		}
		preregisterFunc(f, symbolScope_);
	}

	// analyze block-local func bodies -> appended to sa["functions"]
	for (auto& f : funcs)
		sa_function(f);
	symbolScope_ = savedSymbolScope;

	// analyze body statements (no func-defs)
	json body = sa_statements(stmt["body"]);

	// Append free() for array vars declared in this block (reverse declaration order)
	size_t idx = arrayScopeVars_.size() - 1;
	json frees = collectFreeStmts(idx, idx + 1);
	for (auto& s : frees) body.push_back(s);

	leaveBlockDecls();
	leaveScope();
	return {{"stmt-type", "block"}, {"body", move(body)}}; // LCOV_EXCL_BR_LINE
} // LCOV_EXCL_BR_LINE

json PlnSemanticAnalyzer::sa_if_stmt(const json& stmt)
{
	json result;
	result["stmt-type"] = "if";
	result["cond"] = sa_expression(stmt["cond"]);
	applyPendingGiven();
	GivenFlow before = given_.flow;
	result["then"] = sa_block(stmt["then"]);
	GivenFlow afterThen = move(given_.flow);
	given_.flow = move(before);
	if (stmt.contains("else")) {
		const json& els = stmt["else"];
		if (els["stmt-type"] == "if")
			result["else"] = sa_if_stmt(els);
		else
			result["else"] = sa_block(els);
	}
	mergeGivenFlow(given_.flow, afterThen);
	if (stmt.contains("loc"))
		result["loc"] = stmt["loc"];
	return result;
}

json PlnSemanticAnalyzer::sa_while_stmt(const json& stmt)
{
	json result;
	result["stmt-type"] = "while";
	enterGivenLoop();
	result["cond"] = sa_expression(stmt["cond"]);
	applyPendingGiven();
	result["body"] = sa_loop_body(stmt["body"], json::array(), json::array());
	leaveGivenLoop();
	if (stmt.contains("loc"))
		result["loc"] = stmt["loc"];
	return result;
}

// heads: statements ahead of the body in the loop's scope, their types
// already resolved.
json PlnSemanticAnalyzer::sa_loop_body(const json& astBody, const json& heads, json step)
{
	enterScope();
	whileScopeStack_.push_back(arrayScopeVars_.size() - 1);
	loopSteps_.push_back(step);
	loopDepth_++;
	enterBlockDecls(astBody);

	json body = json::array();
	for (auto& head : heads) {
		string t = head["stmt-type"];
		json out = t == "var-decl" ? sa_resolved_var_decl(head, {})
		         : t == "tapple-decl" ? sa_resolved_tapple_decl(head, vector<VarStorage>(head["vars"].size(), VarStorage::Heap)) // LCOV_EXCL_BR_LINE
		         : sa_statements(json::array({head})); // LCOV_EXCL_BR_LINE
		for (auto& s : out) body.push_back(move(s));
	}
	for (auto& s : sa_statements(astBody)) body.push_back(move(s));

	// Append free() for array vars in while body scope (reverse declaration order)
	size_t idx = arrayScopeVars_.size() - 1;
	json frees = collectFreeStmts(idx, idx + 1);
	for (auto& s : frees) body.push_back(s);
	for (auto& s : step) body.push_back(move(s));

	leaveBlockDecls();
	loopDepth_--;
	loopSteps_.pop_back();
	whileScopeStack_.pop_back();
	leaveScope();
	return body;
}

// A variable that has given its value away holds NULL, so any use of it is
// an error until '->>' fills it again. A use a loop's head reaches is checked
// again when the loop ends, against what its iterations leave.
void PlnSemanticAnalyzer::checkNotGiven(const string& name, const json& locNode)
{
	if (!given_.flow.reachable || nullCompare_) return;
	auto v = given_.flow.vars.find(name);
	GivenState st = v == given_.flow.vars.end() ? GivenState{} : v->second;
	if (st.given) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_UseAfterGiven, name) << endl;
		exit(1);
	}
	for (auto& loop : given_.loops)
		if (loop.entry > st.since) loop.exposed.emplace(name, locNode);
}

// The variable a location (`a`, `a.f[i]`, `@a`) is part of.
void PlnSemanticAnalyzer::checkRootNotGiven(const json& location)
{
	const json* e = &location;
	for (;;) {
		string et = e->value("expr-type", "");
		if (et == "field-access" || et == "addr-of") e = &(*e)["object"];
		else if (et == "arr-index") e = &(*e)["array"];
		else break;
	}
	if (e->value("expr-type", "") == "id" && findVar((*e)["name"].get<string>()))
		checkNotGiven((*e)["name"].get<string>(), e->contains("loc") ? *e : location);
}

void PlnSemanticAnalyzer::applyPendingGiven()
{
	if (given_.flow.reachable)
		for (auto& [name, given] : given_.pending) {
			GivenState& st = given_.flow.vars[name];
			st.given = given;
			if (!given) st.since = given_.seq;
		}
	given_.pending.clear();
}

// Given on any path is given; a use is checked against the oldest fill.
void PlnSemanticAnalyzer::mergeGivenFlow(GivenFlow& into, const GivenFlow& from)
{
	if (!from.reachable) return;
	if (!into.reachable) {
		into = from;
		return;
	}
	for (auto& [name, st] : into.vars) {
		auto f = from.vars.find(name);
		GivenState other = f == from.vars.end() ? GivenState{} : f->second;
		st.given = st.given || other.given;
		st.since = min(st.since, other.since);
	}
	for (auto& [name, st] : from.vars)
		if (!into.vars.count(name)) into.vars[name] = {st.given, 0};
}

void PlnSemanticAnalyzer::enterGivenLoop()
{
	given_.loops.push_back({++given_.seq, {}, given_.flow});
} // LCOV_EXCL_BR_LINE

// The loop's head is reached from before it and from the end of each
// iteration (or a 'continue'); it is left from its head or by a 'break'.
void PlnSemanticAnalyzer::leaveGivenLoop()
{
	GivenLoop loop = move(given_.loops.back());
	given_.loops.pop_back();
	mergeGivenFlow(loop.back, given_.flow);
	for (auto& [name, locNode] : loop.exposed) {
		auto v = loop.back.vars.find(name);
		if (loop.back.reachable && v != loop.back.vars.end() && v->second.given) {
			cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_UseAfterGiven, name) << endl;
			exit(1);
		}
	}
	given_.flow = move(loop.entry_flow);
	mergeGivenFlow(given_.flow, loop.back);
	mergeGivenFlow(given_.flow, loop.exit);
}

// The path checkRangeChange compares: a variable, then "." + field, or
// "[<n>]" for an element at a constant index and "[]" for one at any other.
bool PlnSemanticAnalyzer::locationPath(const json& e, vector<string>& path)
{
	string et = e.value("expr-type", "");
	if (et == "id") {
		path.push_back(e["name"]);
		return true;
	}
	if (et == "field-access" && locationPath(e["object"], path)) {
		path.push_back("." + e["field"].get<string>());
		return true;
	}
	if (et == "arr-index" && locationPath(e["array"], path)) {
		// A dictionary's values at different constant keys are different too.
		json key = foldConstExpr(e["index"]);
		optional<ConstVal> idx = evalConst(e["index"]);
		if (key.value("expr-type", "") == "lit-str")
			path.push_back("[\"" + key["value"].get<string>() + "\"]");
		else if (idx && idx->kind == ConstVal::Kind::Int)
			path.push_back("[" + to_string(idx->i) + "]");
		else if (idx && idx->kind == ConstVal::Kind::Uint)
			path.push_back("[" + to_string(idx->u) + "]");
		else
			path.push_back("[]");
		return true;
	}
	return false;
}

// Elements at different constant indices are different arrays; an index
// known only at run time may be either.
static bool samePathStep(const string& a, const string& b)
{
	return a == b || (a == "[]" && b[0] == '[') || (b == "[]" && a[0] == '[');
}

// A 'for' holds the storage and the length of what it iterates over until it
// ends, so nothing in its body may free or replace that array or change its
// length, through it or through what contains it. A copy into a fixed-length
// array or a borrow of one changes neither, nor does rebinding a borrow.
void PlnSemanticAnalyzer::checkRangeChange(const json& locNode, const json& locAst, RangeChange change)
{
	vector<string> path;
	if (pinnedRanges_.empty() || !locationPath(locAst, path))
		return;
	for (auto& r : pinnedRanges_) {
		if (path.size() > r.path.size() || !equal(path.begin(), path.end(), r.path.begin(), samePathStep))
			continue;
		if (path.size() == r.path.size()) {
			if (r.borrow && change != RangeChange::Borrow) continue;
			if (!r.resizable && change != RangeChange::Transfer) continue;
		}
		string name;
		for (auto& p : r.path) name += p;
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_ChangeForRange, name) << endl;
		exit(1);
	}
}

// A writable borrow passed on as it is, e.g. `p` for a `@![+]T` p.
void PlnSemanticAnalyzer::checkRangeBorrowBinding(const json& locNode, const json& srcAst, const json& dstType)
{
	if (dstType.value("mutable", false))
		checkRangeChange(locNode, srcAst, RangeChange::Borrow);
}

// Each use of `name` in a loop body becomes `elem`, keeping its loc; an
// assignment to it becomes one to the element. A nested function can't see
// the loop's variables, so its body is left alone.
static void aliasLoopElem(json& node, const string& name, const json& elem)
{
	if (node.is_array()) {
		for (auto& n : node) aliasLoopElem(n, name, elem);
		return;
	}
	if (!node.is_object()) return;
	if (node.value("expr-type", "") == "id" && node["name"] == name) {
		json e = elem;
		if (node.contains("loc")) e["loc"] = node["loc"];
		node = move(e);
		return;
	}
	if (node.value("stmt-type", "") == "assign" && node["name"] == name) {
		node["stmt-type"] = "arr-assign";
		node["target"] = elem;
		if (node.contains("loc")) node["target"]["loc"] = node["loc"];
		node.erase("name");
	}
	for (auto& [k, v] : node.items())
		if (k != "functions") aliasLoopElem(v, name, elem);
}

// `for a, i in arr { }` becomes a while over i, in a block of its own that
// holds arr, evaluated once, and its length. Over a dictionary `for a, k in d`,
// i runs over the values' positions that std/rt finds live, and k is the key
// at i.
json PlnSemanticAnalyzer::sa_for_array(const json& stmt, json range)
{
	// LCOV_EXCL_BR_START
	const json& loc = stmt["loc"];
	const json& elem = stmt["elem"];
	auto id = [&](const string& name) { return json{{"expr-type","id"},{"name",name},{"loc",loc}}; };
	auto varDecl = [&](const string& name, const json& type, json init, const json& loc) {
		return json{{"stmt-type","var-decl"},{"loc",loc},
		            {"vars",json::array({{{"name",name},{"var-type",type},{"init",move(init)}}})}};
	};
	const json i64 = {{"type-kind","prim"},{"type-name","int64"}};
	// A read-only view of a `[m][n]T` makes its rows read-only too, as a borrow does.
	auto readOnly = [](json t) {
		if (isArrLevel(t)) return withArrPermission(move(t), false);
		t["mutable"] = false;
		return t;
	};
	enterScope();
	json block = json::array();

	json arrType = range["value-type"];
	bool isDict = isDictPntr(arrType);
	bool typedIdx = stmt.contains("index-type");
	if (isDict && typedIdx) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_ForDictKeyTyped) << endl;
		exit(1);
	}
	bool rangeBorrow = arrType.contains("mutable");
	string arr = "__for_" + to_string(tempVarCounter_++);
	// The range is moved into `arr` below; a length known only at run time is
	// kept by the location it names, which the copy's rows then index by.
	json runLens = bindRunLens(arr, range);
	if (isExpiringOwned(range)) {
		holdCallTemps(range);
		declareVar(arr, arrType);
		addOwnedScopeVar(arr, makeOwnedFreeStmt(arr, arrType));
	} else {
		if (holdsCallTemps(range)) {
			cerr << locPrefix(stmt["range"]) << PlnSaMessage::getMessage(E_BorrowOfTempEscapes) << endl;
			exit(1);
		}
		// Through a read-only '@a', the range is only read.
		if (isWritableThrough(arrType) && !(elem["borrow"] && !elem["mutable"])) arrType["mutable"] = true;
		else arrType = readOnly(move(arrType));
		declareVar(arr, arrType);
	}
	block.push_back({{"stmt-type","var-decl"},
	                 {"vars",json::array({{{"name",arr},{"var-type",arrType},{"init",move(range)}}})}});
	for (auto& s : runLens) block.push_back(s);

	// A typed index is a conversion of a hidden int64 counter, so an index type
	// too narrow for a run-time length wraps like `int32(i)` instead of
	// looping forever, and the elements are still reached through the counter.
	string idx = typedIdx || isDict || !stmt.contains("index") ? "__for_i_" + to_string(tempVarCounter_++) : stmt["index"].get<string>();
	json saArr = {{"expr-type","id"},{"name",arr},{"var-type",arrType},{"value-type",arrType}};
	json idxPlus1 = {{"expr-type","add"},{"left",id(idx)},{"loc",loc},
	                 {"right",{{"expr-type","lit-int"},{"value","1"},{"loc",loc}}}};
	auto rtCall = [&](const string& name, json pos, json ret) {
		return json{{"expr-type","call"},{"name",name},{"func-type","c"},
		            {"args",json::array({saArr, move(pos)})},{"value-type",move(ret)}};
	};
	json step = json::array();
	if (isDict) {
		declareVar(idx, i64);
		block.push_back({{"stmt-type","var-decl"},
		                 {"vars",json::array({{{"name",idx},{"var-type",i64},{"init",rtCall("__pln_dict_next", lenLit(0), i64)}}})}});
		step.push_back({{"stmt-type","assign"},{"name",idx},{"value",rtCall("__pln_dict_next", sa_expression(idxPlus1), i64)}});
		if (stmt.contains("index")) {
			const string& key = stmt["index"];
			json keyType = {{"type-kind","pntr"},{"mutable",false},{"base-type",{{"type-kind","prim"},{"type-name","uint8"}}}};
			declareVar(key, keyType);
			block.push_back({{"stmt-type","var-decl"},
			                 {"vars",json::array({{{"name",key},{"var-type",keyType},{"init",rtCall("__pln_dict_key", id(idx), keyType)}}})}});
			step.push_back({{"stmt-type","assign"},{"name",key},{"value",rtCall("__pln_dict_key", sa_expression(id(idx)), keyType)}});
		}
	} else {
		for (auto& s : sa_var_decl(varDecl(idx, i64, {{"expr-type","lit-int"},{"value","0"},{"loc",loc}}, loc)))
			block.push_back(move(s));
		step.push_back({{"stmt-type","assign"},{"name",idx},{"value",sa_expression(idxPlus1)}});
	}
	string len = "__for_len_" + to_string(tempVarCounter_++);
	json lenVal = isDict ? json{{"expr-type","field-access"},{"ptr-expr",saArr},{"offset",DictUsedOffset},{"value-type",i64}}
	                     : arrLenOf(saArr, stmt["range"]);
	declareVar(len, i64);
	block.push_back({{"stmt-type","var-decl"},
	                 {"vars",json::array({{{"name",len},{"var-type",i64},{"init",move(lenVal)}}})}});

	json cond = sa_expression({{"expr-type","cmp"},{"op","<"},{"left",id(idx)},{"right",id(len)},{"loc",loc}});

	// The values do not move, as the body cannot add a key; they are indexed as
	// an array is.
	if (isDict) {
		arr = "__for_vals_" + to_string(tempVarCounter_++);
		json data = varrData(saArr);
		arrType = data["value-type"];
		arrType["mutable"] = isWritableThrough(arrType);
		declareVar(arr, arrType);
		block.push_back({{"stmt-type","var-decl"},
		                 {"vars",json::array({{{"name",arr},{"var-type",arrType},{"init",move(data)}}})}});
		saArr = {{"expr-type","id"},{"name",arr},{"var-type",arrType},{"value-type",arrType}};
	}

	const json& elemType = isVarrPntr(arrType) ? arrType["base-type"]["base-type"] : arrType["base-type"];
	json declType = astElemTypeOf(elemType);
	if (declType.is_null()) declType = elemType;
	json heads = json::array();
	if (typedIdx) heads.push_back(forIndexHead(stmt, idx, &arrType));
	json astBody = stmt["body"];
	if (elem["borrow"]) {
		// `@a`/`@!a` is the element itself, so the body reads and writes it as
		// it would `arr[i]`. An owned range is writable, so a read-only `@a`
		// reads it through a read-only view.
		bool mut = elem["mutable"];
		if (mut && !isWritableThrough(arrType)) {
			cerr << locPrefix(elem) << PlnSaMessage::getMessage(E_PtrMutabilityUpgrade) << endl;
			exit(1);
		}
		string view = arr;
		if (!mut && isWritableThrough(arrType)) {
			view = "__for_ro_" + to_string(tempVarCounter_++);
			json roType = readOnly(arrType);
			declareVar(view, roType);
			block.push_back({{"stmt-type","var-decl"},
			                 {"vars",json::array({{{"name",view},{"var-type",roType},{"init",saArr}}})}});
			for (auto& s : bindRunLens(view, saArr)) block.push_back(move(s));
		}
		// The name stays declared so the body can't declare it again.
		declareVar(elem["name"], {{"type-kind","pntr"},{"mutable",mut},{"base-type",move(declType)}});
		aliasLoopElem(astBody, elem["name"], {{"expr-type","arr-index"},{"array",id(view)},{"index",id(idx)}});
	} else {
		json init = {{"expr-type","arr-index"},{"array",id(arr)},{"index",id(idx)},{"loc",elem["loc"]}};
		heads.push_back(varDecl(elem["name"], declType, move(init), elem["loc"]));
	}

	const json& rangeAst = stmt["range"];
	bool viaAddrOf = rangeAst.value("expr-type", "") == "addr-of";
	vector<string> path;
	bool pinned = locationPath(viaAddrOf ? rangeAst["object"] : rangeAst, path);
	if (pinned) pinnedRanges_.push_back({move(path), isDict || isVarrPntr(arrType), !viaAddrOf && rangeBorrow});
	string idxName = stmt.value("index", idx);
	forIndexVars_.insert(idxName);
	enterGivenLoop();
	json body = sa_loop_body(astBody, heads, move(step));
	leaveGivenLoop();
	forIndexVars_.erase(idxName);
	if (pinned) pinnedRanges_.pop_back();
	block.push_back({{"stmt-type","while"},{"cond",move(cond)},{"body",move(body)},{"loc",loc}});

	size_t scope = arrayScopeVars_.size() - 1;
	for (auto& s : collectFreeStmts(scope, scope + 1)) block.push_back(move(s));
	leaveScope();
	return {{"stmt-type","block"},{"body",move(block)}};
	// LCOV_EXCL_BR_STOP
} // LCOV_EXCL_BR_LINE

// The temps the calls in `node` were given are released with the current scope
// rather than after the expression, since a value a call returns may hold a
// borrow of one. Those a condition or a '&&'/'||' operand ends stay there.
void PlnSemanticAnalyzer::holdCallTemps(json& node)
{
	// LCOV_EXCL_BR_START
	if (node.is_array()) {
		for (auto& n : node) holdCallTemps(n);
		return;
	}
	if (!node.is_object()) return;
	string et = node.value("expr-type", "");
	if (et == "logical-and" || et == "logical-or") return;
	string st = node.value("stmt-type", "");
	if (st == "if" || st == "while" || st == "block") return;
	if (et != "temp") {
		for (auto& [key, child] : node.items())
			if (child.is_structured()) holdCallTemps(child);
		return;
	}
	holdCallTemps(node["body"]);
	holdCallTemps(node["value"]);
	// Scope releases run last first.
	json& release = node["release"];
	for (auto r = release.rbegin(); r != release.rend(); ++r)
		arrayScopeVars_.back().push_back({"__held_" + to_string(tempVarCounter_++), move(*r), false});
	release = json::array();
	// LCOV_EXCL_BR_STOP
} // LCOV_EXCL_BR_LINE

// `for a, i in x { }`: over an array, or over a struct by its type's `iter`
// and `next`.
json PlnSemanticAnalyzer::sa_for_stmt(const json& stmt)
{
	const json& range = stmt["range"];
	// A location is borrowed by `iter` as it is written, so a struct field
	// held inline is not read as a value.
	if (isLocationExpr(range)) {
		json vt = operandValueType(range);
		if (isStructPntr(vt)) return sa_for_struct(stmt, vt, nullptr);
	}
	const json& elem = stmt["elem"];
	string rangeKind = range.value("expr-type", "");
	bool writes = elem.value("borrow", false) && elem.value("mutable", false);
	json rangeSa = !writes ? sa_expression(range)
		: rangeKind == "field-access" ? sa_expr_field_access(range, true)
		: rangeKind == "arr-index"    ? sa_expr_arr_index(range, true)
		: sa_expression(range);
	const json& vt = rangeSa["value-type"];
	if (isStructPntr(vt)) {
		json t = vt;
		return sa_for_struct(stmt, t, &rangeSa);
	}
	if (!isVarrPntr(vt) && !isArrLevel(vt) && !isDictPntr(vt)) {
		cerr << locPrefix(range) << PlnSaMessage::getMessage(E_ForRangeNotArray) << endl;
		exit(1);
	}
	return sa_for_array(stmt, move(rangeSa));
} // LCOV_EXCL_BR_LINE

// A typed index `T i` declared from the hidden int64 counter `idx`. Every
// index of an array of constant size must fit in T; any other length is known
// only at run time.
json PlnSemanticAnalyzer::forIndexHead(const json& stmt, const string& idx, const json* arrType)
{
	// LCOV_EXCL_BR_START
	const json& loc = stmt["loc"];
	json idxType = resolveKnownType(stmt, stmt["index-type"]);
	const PlnType* t = registry_.fromJson(idxType);
	if (!isIntegerPrim(t) || isBoolPrim(t)) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_ForIndexNotInteger, typeDisplayName(idxType)) << endl;
		exit(1);
	}
	if (arrType && !isVarrPntr(*arrType) && !arrType->value("run-size", false)) {
		int64_t n = constArrSizeOrExit(*arrType, stmt["range"]);
		if (n > 0 && !intLiteralFits(to_string(n - 1), static_cast<const PrimType*>(t)->name)) {
			cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_ForIndexTooNarrow,
				typeDisplayName(idxType), to_string(n)) << endl;
			exit(1);
		}
	}
	json conv = {{"expr-type","cast"},{"target-type",idxType},{"src",{{"expr-type","id"},{"name",idx},{"loc",loc}}},{"loc",loc}};
	return {{"stmt-type","var-decl"},{"loc",loc},
	        {"vars",json::array({{{"name",stmt["index"]},{"var-type",idxType},{"init",move(conv)}}})}};
	// LCOV_EXCL_BR_STOP
} // LCOV_EXCL_BR_LINE

// `for c, i in s { }` over a struct becomes, in a block of its own,
//   I __for_it = iter(@s);   (or @!s)
//   while true { (bool ok, E e) = next(@!__for_it); if !ok { break } ... }
// The iterator may borrow s, so a struct a call returns is held in the block
// until the loop ends. rangeSa is s analyzed, or null for a location.
json PlnSemanticAnalyzer::sa_for_struct(const json& stmt, const json& rangeVT, json* rangeSa)
{
	// LCOV_EXCL_BR_START
	const json& loc = stmt["loc"];
	const json& elem = stmt["elem"];
	const json& rangeAst = stmt["range"];
	auto id = [&](const string& name) { return json{{"expr-type","id"},{"name",name},{"loc",loc}}; };
	auto borrowOf = [&](json obj, bool mut) {
		return json{{"expr-type","addr-of"},{"mutable",mut},{"object",move(obj)},{"loc",loc}};
	};
	json storageVT = rangeVT;
	storageVT.erase("mutable");
	string typeName = typeDisplayName(storageVT);

	vector<Callee> iters = forProtocolCallees("iter", storageVT);
	if (iters.empty()) {
		cerr << locPrefix(rangeAst) << PlnSaMessage::getMessage(E_ForIterShape, typeName) << endl;
		exit(1);
	}
	if (iters.size() > 1) {
		cerr << locPrefix(rangeAst) << PlnSaMessage::getMessage(E_ForIterAmbiguous, typeName) << endl;
		exit(1);
	}
	const json& iterDef = *iters[0].def;
	bool iterMut = iterDef["parameters"][0]["var-type"]["mutable"];
	const json& itType = iterDef["ret-type"];
	vector<Callee> nexts = forProtocolCallees("next", itType);
	if (nexts.empty()) {
		cerr << locPrefix(rangeAst) << PlnSaMessage::getMessage(E_ForNextShape, typeDisplayName(itType)) << endl;
		exit(1);
	}
	json eType = (*nexts[0].def)["rets"][1]["var-type"];
	bool eBorrow = eType.contains("mutable");
	if ((elem["borrow"] && !eBorrow) || (elem.value("mutable", false) && !eType.value("mutable", false))) {
		string written = string(elem.value("mutable", false) ? "@!" : "@") + elem["name"].get<string>();
		cerr << locPrefix(elem) << PlnSaMessage::getMessage(E_ForElemForm, written, typeDisplayName(eType)) << endl;
		exit(1);
	}

	enterScope();
	json block = json::array();
	json rangeArg;
	if (rangeSa) {
		holdCallTemps(*rangeSa);
		// Receiving the struct would put a temp's body in a block of its own.
		if (rangeSa->value("expr-type", "") == "temp") {
			for (auto& b : (*rangeSa)["body"]) block.push_back(move(b));
			json value = move((*rangeSa)["value"]);
			*rangeSa = move(value);
		}
		bool owned = isExpiringOwned(*rangeSa);
		string held = pinTemp(move(*rangeSa), rangeAst, block);
		if (owned) addOwnedScopeVar(held, makeOwnedFreeStmt(held, storageVT));
		rangeArg = isPtrBorrow(rangeVT) ? id(held) : borrowOf(id(held), iterMut);
	} else {
		rangeArg = isPtrBorrow(rangeVT) ? rangeAst : borrowOf(rangeAst, iterMut);
	}
	json iterCall = {{"expr-type","call"},{"name","iter"},{"args",json::array({move(rangeArg)})},
	                 {"iterates",storageVT},{"loc",loc}};
	string it = pinTemp(sa_expression(iterCall), stmt, block);
	addOwnedScopeVar(it, makeOwnedFreeStmt(it, itType));

	const json i64 = {{"type-kind","prim"},{"type-name","int64"}};
	bool typedIdx = stmt.contains("index-type");
	string idx = typedIdx || !stmt.contains("index") ? "__for_i_" + to_string(tempVarCounter_++) : stmt["index"].get<string>();
	for (auto& s : sa_var_decl({{"stmt-type","var-decl"},{"loc",loc},
	                            {"vars",json::array({{{"name",idx},{"var-type",i64},
	                             {"init",{{"expr-type","lit-int"},{"value","0"},{"loc",loc}}}}})}}))
		block.push_back(move(s));
	json step = json::array({{{"stmt-type","assign"},{"name",idx},
		{"value",sa_expression({{"expr-type","add"},{"left",id(idx)},{"loc",loc},
		                        {"right",{{"expr-type","lit-int"},{"value","1"},{"loc",loc}}}})}}});
	json heads = json::array();
	if (typedIdx) heads.push_back(forIndexHead(stmt, idx, nullptr));

	// The element is received into the loop variable itself only when it is a
	// copy already; a borrow is received into a hidden one the variable reads
	// through, read-only for `@c`.
	bool direct = !elem["borrow"] && !eBorrow;
	string e = direct ? elem["name"].get<string>() : "__for_e_" + to_string(tempVarCounter_++);
	json recvType = eType;
	if (eBorrow && !elem.value("mutable", false)) {
		if (isArrLevel(recvType)) recvType = withArrPermission(move(recvType), false);
		else recvType["mutable"] = false;
	}
	string ok = "__for_ok_" + to_string(tempVarCounter_++);
	heads.push_back({{"stmt-type","tapple-decl"},{"loc",loc},
		{"vars",json::array({{{"var-name",ok},{"var-type",{{"type-kind","prim"},{"type-name","bool"}}}},
		                     {{"var-name",e},{"var-type",astTypeOf(recvType)}}})},
		{"value",{{"expr-type","call"},{"name","next"},{"args",json::array({borrowOf(id(it), true)})},
		          {"iterates",itType},{"loc",loc}}}});
	heads.push_back({{"stmt-type","if"},{"loc",loc},
		{"cond",{{"expr-type","logical-not"},{"operand",id(ok)},{"loc",loc}}},
		{"then",{{"stmt-type","block"},{"body",json::array({{{"stmt-type","break"},{"loc",loc}}})}}}});
	// A borrowed array is the element as it is; any other borrow is read
	// and written through `[0]`.
	json through = isArrLevel(eType) ? id(e)
	             : json{{"expr-type","arr-index"},{"array",id(e)},{"index",{{"expr-type","lit-int"},{"value","0"},{"loc",loc}}}};
	json astBody = stmt["body"];
	if (elem["borrow"]) {
		declareVar(elem["name"], recvType);
		aliasLoopElem(astBody, elem["name"], through);
	} else if (!direct) {
		json copyType = eType;
		copyType.erase("mutable");
		through["loc"] = elem["loc"];
		heads.push_back({{"stmt-type","var-decl"},{"loc",elem["loc"]},
			{"vars",json::array({{{"name",elem["name"]},{"var-type",astTypeOf(isArrLevel(eType) ? copyType : eType["base-type"])},
			                      {"init",move(through)}}})}});
	}

	vector<string> path;
	bool pinned = !rangeSa && locationPath(rangeAst, path);
	if (pinned) pinnedRanges_.push_back({move(path), true, false});
	string idxName = stmt.value("index", idx);
	forIndexVars_.insert(idxName);
	enterGivenLoop();
	json body = sa_loop_body(astBody, heads, move(step));
	leaveGivenLoop();
	forIndexVars_.erase(idxName);
	if (pinned) pinnedRanges_.pop_back();
	json cond = sa_expression({{"expr-type","lit-int"},{"value","1"},{"loc",loc},
	                           {"value-type",{{"type-kind","prim"},{"type-name","bool"}}}});
	block.push_back({{"stmt-type","while"},{"cond",move(cond)},{"body",move(body)},{"loc",loc}});

	size_t scope = arrayScopeVars_.size() - 1;
	for (auto& s : collectFreeStmts(scope, scope + 1)) block.push_back(move(s));
	leaveScope();
	return {{"stmt-type","block"},{"body",move(block)}};
	// LCOV_EXCL_BR_STOP
} // LCOV_EXCL_BR_LINE

// `name`, another name for the array at `location`, keeps the lengths that
// location keeps for its levels sized at run time, as a declaration would.
json PlnSemanticAnalyzer::bindRunLens(const string& name, const json& location)
{
	json decls = json::array();
	const json& at = location["expr-type"] == "addr-of" ? location["object"] : location;
	const json u64 = {{"type-kind","prim"},{"type-name","uint64"}}; // LCOV_EXCL_BR_LINE
	size_t k = 0;
	for (const json* t = &at["value-type"]; isArrLevel(*t); t = &(*t)["base-type"], k++) {
		if (!t->value("run-size", false)) continue;
		auto [root, depth] = arrRoot(at);
		string lenName = "__" + name + "_d" + to_string(k);
		declareVar(lenName, u64);
		decls.push_back({{"stmt-type","var-decl"}, // LCOV_EXCL_BR_LINE
		                 {"vars",json::array({{{"name",lenName},{"var-type",u64},{"init",runtimeLen(*root, depth + k)}}})}}); // LCOV_EXCL_BR_LINE
	}
	return decls;
} // LCOV_EXCL_BR_LINE

json PlnSemanticAnalyzer::sa_break_stmt(const json& stmt)
{
	if (loopDepth_ == 0) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_BreakOutsideLoop) << endl;
		exit(1);
	}
	return {{"stmt-type", "break"}}; // LCOV_EXCL_BR_LINE
} // LCOV_EXCL_BR_LINE

json PlnSemanticAnalyzer::sa_continue_stmt(const json& stmt)
{
	if (loopDepth_ == 0) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_ContinueOutsideLoop) << endl;
		exit(1);
	}
	return {{"stmt-type", "continue"}}; // LCOV_EXCL_BR_LINE
} // LCOV_EXCL_BR_LINE

void PlnSemanticAnalyzer::sa_function(const json& funcDef, bool topLevel)
{
	// A syscall declaration is a prototype: no block to analyze, and nothing to emit.
	if (funcDef.value("func-type", "") == "syscall") return;
	// Resolved here, before the body's declarations are in scope.
	const json sig = resolveSigTypes(funcDef);

	// Save var scopes and use a fresh function scope instead
	auto savedVarScopes      = varScopes;
	auto savedCurrentFunc    = currentFunc_;
	auto savedCurrentFuncDef = currentFuncDef_;
	auto savedRetSlot        = retSlot_;
	auto savedArrayScopeVars = arrayScopeVars_;
	auto savedFuncBodyIdx    = funcBodyScopeIdx_;
	auto savedEscapedVars    = move(escapedVars_);
	auto savedForIndexVars   = move(forIndexVars_);
	auto savedPinnedRanges   = move(pinnedRanges_);
	auto savedPendingAliases = pendingAliases_;
	auto savedSymbolScope    = symbolScope_;
	auto savedBlockSymbolSeq = blockSymbolSeq_;
	auto savedPermitFunc     = move(permitFunc_);
	auto savedStaticBindings = move(staticBindings_);
	auto savedReadOnlyGlobals = move(readOnlyGlobals_);
	auto savedGiven          = move(given_);
	given_ = {};
	permitFunc_ = topLevel ? funcDef["name"].get<string>() : "";
	staticBindings_ = json::array();
	readOnlyGlobals_.clear();
	escapedVars_.clear();
	forIndexVars_.clear();
	pinnedRanges_.clear();
	pendingAliases_ = 0;

	varScopes       = {{}}; // LCOV_EXCL_BR_LINE
	arrayScopeVars_ = {{}};  // scope[0] = params; no arrays expected here // LCOV_EXCL_BR_LINE

	// A named return owning a block is allocated at body entry, where an
	// initializer can't run yet. A struct is written into storage the caller
	// provides, which the function sees as a variable it does not own.
	auto isStructRet = [&](const json& t) { return isStructValue(sigType(funcDef, t)); };
	auto allocatedAtEntry = [&](const json& t) { return isOwnedBlock(sigType(funcDef, t)) && !isStructRet(t); };
	json ownedParams = json::array();
	// A variadic parameter is an array sized at run time, as a local `[n]E` is,
	// whose length arrives in a parameter of its own.
	string variadic;
	json variadicType;
	const json u64 = {{"type-kind","prim"},{"type-name","uint64"}}; // LCOV_EXCL_BR_LINE
	if (sig.contains("parameters"))
		for (auto& p : sig["parameters"]) {
			json vt = sigType(funcDef, p["var-type"]);
			if (vt.value("variadic", false)) {
				variadic = p["name"];
				variadicType = {{"type-kind","pntr"},{"run-size",true},{"base-type",vt["base-type"]}}; // LCOV_EXCL_BR_LINE
				declareVar(variadic, variadicType, &funcDef);
				declareVar("__" + variadic + "_d0", u64, &funcDef);
				continue;
			}
			if (isOwnedBlock(vt)) ownedParams.push_back(p["name"]);
			declareVar(p["name"], move(vt), &funcDef);
		}
	if (sig.contains("rets"))
		for (auto& r : sig["rets"]) {
			if (isStructRet(r["var-type"]) && r.contains("init")) {
				cerr << locPrefix(r["init"]) << PlnSaMessage::getMessage(E_NamedRetInitOnStruct, r["name"].get<string>()) << endl;
				exit(1);
			}
			if (!allocatedAtEntry(r["var-type"])) {
				declareVar(r["name"], sigType(funcDef, r["var-type"]), &funcDef);
			} else if (r.contains("init")) {
				cerr << locPrefix(r["init"]) << PlnSaMessage::getMessage(E_NamedRetInitOnStruct, r["name"].get<string>()) << endl;
				exit(1);
			}
		}

	if (topLevel)
		declarePermittedGlobals(permitFunc_, funcDef);

	currentFunc_ = findPlnFuncDef(funcDef);
	currentFuncDef_ = &sig;
	retSlot_.clear();
	if (sig.contains("ret-type") && isStructRet(sig["ret-type"])) {
		retSlot_ = "__ret";
		declareVar(retSlot_, sigType(funcDef, sig["ret-type"]), &funcDef);
	} else if (sig.contains("rets") && sig["rets"].size() == 1 && isStructRet(sig["rets"][0]["var-type"])) {
		retSlot_ = sig["rets"][0]["name"];
	}
	symbolScope_ = (*currentFunc_)["symbol"];
	blockSymbolSeq_ = 0;
	enterScope();  // push scope[1] = function body
	funcBodyScopeIdx_ = arrayScopeVars_.size() - 1;  // = 1
	// The caller hands an owned argument over (a copy, or a call's result), so
	// the function frees it as one of its own variables.
	// A release function's own parameter releases its contents: releasing it
	// as its type would call the release function again.
	for (auto& name : ownedParams) {
		const json& vt = *findVar(name);
		const StructDef* opaque = funcDef.value("release", false) ? opaqueDefOf(vt) : nullptr;
		json id = {{"expr-type","id"},{"name",name},{"var-type",vt},{"value-type",vt}}; // LCOV_EXCL_BR_LINE
		addOwnedScopeVar(name, opaque ? makeOwnedValueFreeStmt(asOpaqueContents(id, *opaque))
		                              : makeOwnedFreeStmt(name, vt));
	}
	// The array is the caller's, as a frame array is no one's to free; the
	// elements it owns are the function's.
	if (!variadic.empty())
		arrayScopeVars_.back().push_back({variadic, elemsFiniStmt(variadic, variadicType), false});

	const json& blk = funcDef["block"];
	enterBlockDecls(blk["body"]);

	// pre-register inner func-defs (visible only within this function)
	for (auto& f : blk.value("functions", json::array())) {
		if (f.value("export", false)) {
			cerr << locPrefix(f) << PlnSaMessage::getMessage(E_ExportInFunction, f["name"].get<string>()) << endl;
			exit(1);
		}
		preregisterFunc(f, symbolScope_);
	}

	// analyze inner func bodies -> appended to sa["functions"]
	for (auto& f : blk.value("functions", json::array()))
		sa_function(f);

	json saFunc = sig;
	// Each call is given every argument, so the definition keeps no defaults.
	if (saFunc.contains("parameters"))
		for (auto& p : saFunc["parameters"]) p.erase("init");
	saFunc["name"] = (*currentFunc_)["symbol"];
	if (currentFunc_->value("export", false)) saFunc["export"] = true;
	normalizeSig(saFunc);
	if (!variadic.empty()) {
		saFunc["parameters"].back()["var-type"] = variadicType;
		saFunc["parameters"].push_back({{"name","__" + variadic + "_d0"},{"var-type",u64}}); // LCOV_EXCL_BR_LINE
	}
	// Single named return: add ret-type so codegen knows the return type
	if (!saFunc.contains("ret-type") && saFunc.contains("rets") && saFunc["rets"].size() == 1)
		saFunc["ret-type"] = saFunc["rets"][0]["var-type"];

	json body = json::array();
	if (!retSlot_.empty()) {
		const json& type = *findVar(retSlot_);
		const StructDef& def = structDefs_.at(type["base-type"]["type-name"].get<string>());
		json ebs = eightbytesJson(def, structDefs_, true);
		// Returned in registers, the struct is built in the frame first.
		if (!ebs.empty())
			body.push_back({{"stmt-type","var-decl"},{"vars",json::array({{{"name",retSlot_},{"var-type",type}, // LCOV_EXCL_BR_LINE
				{"init",{{"expr-type","frame-alloc"},{"size",def.totalSize},{"align",def.maxAlign},{"value-type",type}}}}})}}); // LCOV_EXCL_BR_LINE
		saFunc["ret-slot"] = {{"name",retSlot_},{"size",def.totalSize},{"eightbytes",move(ebs)}}; // LCOV_EXCL_BR_LINE
		// Only a named return is used before a return statement fills the slot.
		if (sig.contains("rets")) {
			json init = structInitStmt(retSlot_, type);
			if (!init.is_null()) body.push_back(move(init));
		}
	} else if (saFunc.contains("rets") && saFunc["rets"].size() >= 2) {
		for (auto& r : saFunc["rets"]) {
			if (!isStructValue(r["var-type"])) continue;
			r["slot"] = true;
			json init = structInitStmt(r["name"], r["var-type"]);
			if (!init.is_null()) body.push_back(move(init));
		}
	}
	if (sig.contains("rets")) {
		auto& bodyScopeVars = arrayScopeVars_[funcBodyScopeIdx_];
		size_t nonRetVars = bodyScopeVars.size();
		for (auto& r : sig["rets"]) {
			if (!allocatedAtEntry(r["var-type"])) continue;
			json decl = {{"stmt-type", "var-decl"},
			             {"vars", json::array({{{"name", r["name"]}, {"var-type", r["var-type"]}}})}, // LCOV_EXCL_BR_LINE
			             {"loc", r.value("loc", json::array())}}; // LCOV_EXCL_BR_LINE
			for (auto& s : sa_resolved_var_decl(decl)) body.push_back(move(s));
		}
		// The caller takes over the named returns, so they are not freed here.
		bodyScopeVars.resize(nonRetVars);
	}
	if (saFunc.contains("rets"))
		for (auto& r : saFunc["rets"]) {
			if (!r.contains("init")) continue;
			json assign = {{"stmt-type", "assign"}, {"name", r["name"]}, {"value", r["init"]},
			               {"loc", r["init"].value("loc", json::array())}}; // LCOV_EXCL_BR_LINE
			for (auto& st : sa_assign_stmt(assign)) body.push_back(move(st));
			r.erase("init");
		}
	for (auto& s : sa_statements(blk["body"])) body.push_back(move(s));

	// Append free() for array vars in function body scope (reverse declaration order).
	// A trailing return already freed what it doesn't return.
	if (body.empty() || body.back().value("stmt-type", "") != "return") {
		json frees = collectFreeStmts(funcBodyScopeIdx_, arrayScopeVars_.size());
		for (auto& s : frees) body.push_back(s);
	}

	if (pendingAliases_)
		resolvePending(body);
	saFunc["body"] = move(body);
	saFunc.erase("block");
	if (!staticBindings_.empty())
		saFunc["globals"] = staticBindings_;

	leaveBlockDecls();
	leaveScope();
	varScopes        = savedVarScopes;
	arrayScopeVars_  = savedArrayScopeVars;
	funcBodyScopeIdx_ = savedFuncBodyIdx;
	currentFunc_     = savedCurrentFunc;
	currentFuncDef_  = savedCurrentFuncDef;
	retSlot_         = savedRetSlot;
	escapedVars_     = move(savedEscapedVars);
	forIndexVars_    = move(savedForIndexVars);
	pinnedRanges_    = move(savedPinnedRanges);
	pendingAliases_  = savedPendingAliases;
	symbolScope_     = savedSymbolScope;
	permitFunc_      = move(savedPermitFunc);
	staticBindings_  = move(savedStaticBindings);
	readOnlyGlobals_ = move(savedReadOnlyGlobals);
	blockSymbolSeq_  = savedBlockSymbolSeq;
	given_           = move(savedGiven);

	sa["functions"].push_back(saFunc);
} // LCOV_EXCL_BR_LINE

json PlnSemanticAnalyzer::sa_assign_stmt(const json& stmt)
{
	string name = stmt["name"];
	const json* varType = lookupVar(name, stmt, !stmt.value("ownership-transfer", false));
	if (varType == nullptr) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_UndefinedVariable, name) << endl;
		exit(1);
	}
	requireAssignable(name, stmt);
	if (isAggregateLit(stmt["value"]) && (isVarrStorage(*varType) || isDictStorage(*varType) || assignsByFunc(*varType)))
		return sa_lit_assign_via_temp(stmt, *varType);
	if (isAggregateLit(stmt["value"]) && (isStructPntr(*varType) || isArrLevel(*varType)))
		return json::array({sa_aggregate_lit_assign(stmt, {{"expr-type", "id"}, {"name", name}}, *varType)}); // LCOV_EXCL_BR_LINE
	const PlnType* toType = registry_.fromJson(*varType);
	if (stmt.value("ownership-transfer", false))
		return sa_transfer_to_var(stmt, name, *varType, toType);
	json value = typedNull(sa_expression_for(literalConversion(stmt["value"], *varType), *varType), *varType);
	// A borrowed array variable ("mutable" on its levels) is rebound to what
	// '@x' names.
	if (isCopiedByValue(*varType) && !varType->contains("mutable")) {
		json dst = {{"expr-type","id"},{"name",name},{"var-type",*varType},{"value-type",*varType}}; // LCOV_EXCL_BR_LINE
		return json::array({makeCopyStmt(stmt, dst, value, CopyInto::Existing)}); // LCOV_EXCL_BR_LINE
	}
	if (varType->contains("arr-size") && !isOwnedVar(name))
		checkArrBorrowBinding(stmt, stmt["value"], value, *varType);
	value = convertForBinding(stmt, value, toType, registry_.toJson(toType));
	checkElemShape(stmt, value, *varType);
	checkBorrowSource(stmt, value, *varType);
	checkPtrPermission(stmt, value["value-type"], *varType);
	checkRangeBorrowBinding(stmt, stmt["value"], *varType);
	return json::array({{{"stmt-type", "assign"}, {"name", name}, {"value", value}}}); // LCOV_EXCL_BR_LINE
} // LCOV_EXCL_BR_LINE

// The name an array-literal diagnostic gives the target: the variable or
// field that holds it (a row's array).
static string arrLitTargetName(const json& ast)
{
	string et = ast.value("expr-type", "");
	if (et == "field-access")
		return ast["field"].get<string>();
	if (et == "arr-index")
		return arrLitTargetName(ast["array"]);
	return ast["name"].get<string>();
}

// `[..] -> v` is `$[]T __lit = [..]; __lit -> v;`: every element is
// evaluated before v changes, and the frame array costs no allocation. An
// empty literal is an empty frame `$[+]T`, as a sized array has elements.
// So is a literal for a value whose parts a type's '->' writes: the literal
// is a new value, made whole, and the store decides which existing parts
// '->' writes and which a `[+]T` gains.
json PlnSemanticAnalyzer::sa_lit_assign_via_temp(const json& stmt, const json& varType, json* litAddr)
{
	if (stmt.value("ownership-transfer", false)) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_TransferFromNonOwner, "->>") << endl;
		exit(1);
	}
	// LCOV_EXCL_BR_START
	json result = json::array();
	json store = stmt;
	string st = stmt["stmt-type"];
	json targetAst = st == "assign" ? json{{"expr-type", "id"}, {"name", stmt["name"]}}
	               : st == "arr-assign" ? stmt["target"]
	               : json{{"expr-type", "field-access"}, {"object", stmt["object"]}, {"field", stmt["field"]}};
	if (litAddr) {
		targetAst = pinLocation(move(targetAst), result);
		if (st == "arr-assign") store["target"] = targetAst;
		else if (st == "field-assign") store["object"] = targetAst["object"];
	}
	json loc = stmt.value("loc", json::array());
	string temp = "__lit_" + to_string(tempVarCounter_++);
	json arr = astTypeOf(varType);
	bool isDict = isDictPntr(varType);
	if (!isStructPntr(varType) && !isDict && !stmt["value"]["items"].empty()) arr["specifier"] = "raw";
	string target = arrLitTargetName(targetAst);
	json decl = {{"stmt-type","var-decl"},{"loc",loc},
		{"vars",json::array({{{"name",temp},{"lit-name",target},{"var-type",move(arr)},{"init",stmt["value"]}}})}};
	store["value"] = {{"expr-type","id"},{"name",temp},{"loc",loc}};
	enterScope();
	// std/rt allocates a dictionary, so it cannot be laid out in the frame.
	json body = sa_resolved_var_decl(decl, {isDict ? VarStorage::Heap : VarStorage::Frame});
	for (auto& st : sa_store_stmt(store)) body.push_back(move(st));
	size_t idx = arrayScopeVars_.size() - 1;
	for (auto& st : collectFreeStmts(idx, idx + 1)) body.push_back(move(st));
	leaveScope();
	result.push_back({{"stmt-type","block"},{"body",move(body)}});
	if (litAddr) *litAddr = {{"expr-type","pinned"},{"sa",sa_expression(targetAst)}};
	return result;
	// LCOV_EXCL_BR_STOP
} // LCOV_EXCL_BR_LINE

// The variable releases what it held the way its scope exit would, which
// knows its sizes, so the value must have those same sizes.
json PlnSemanticAnalyzer::sa_transfer_to_var(const json& stmt, const string& name, const json& varType,
                                             const PlnType* toType)
{
	if (!isOwnedVar(name)) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_TransferToNonOwnedVar, name, "->>") << endl;
		exit(1);
	}
	const json& src = stmt["value"];
	if (src.value("expr-type", "") == "id" && src["name"] == name) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_TransferToSelf, name) << endl;
		exit(1);
	}
	json value = sa_move_source(src, toType);
	json store = {{"stmt-type", "assign"}, {"name", name}, {"value", value}}; // LCOV_EXCL_BR_LINE
	// Whether the source owns what it gives is told first.
	json result = transferStmts(stmt, scopeRelease(name), move(store), value);
	checkMoveShape(stmt, value, varType, /*inherits=*/true);
	// Its block was released by the lengths it had.
	for (auto& s : inheritRunLens(name, varType, value)) result.push_back(move(s));
	given_.pending.push_back({name, false});
	return result;
} // LCOV_EXCL_BR_LINE

// The move of `value` into a location of type `to`; `inherits` when the
// location is a variable, which takes over lengths known only at run time.
void PlnSemanticAnalyzer::checkMoveShape(const json& locNode, const json& value, const json& to, bool inherits)
{
	const json& from = value["value-type"];
	MoveFit fit = moveShapeFit(from, to, inherits, value.value("resized", false));
	if (fit == MoveFit::Match) return;
	cerr << locPrefix(locNode) << PlnSaMessage::getMessage(fit == MoveFit::RunSize ? E_MoveRunSize : E_TransferShapeMismatch,
		typeDisplayName(to), typeDisplayName(from)) << endl;
	exit(1);
}

// The lengths known only at run time of the variable `name` taking `value` over.
json PlnSemanticAnalyzer::inheritRunLens(const string& name, const json& varType, const json& value)
{
	// LCOV_EXCL_BR_START
	json out = json::array();
	if (!hasRunSize(varType)) return out;
	json lens = valueLens(value);
	const json u64 = {{"type-kind","prim"},{"type-name","uint64"}};
	size_t k = 0;
	for (const json* t = &varType; isArrLevel(*t); t = &(*t)["base-type"], k++)
		if (t->value("run-size", false))
			out.push_back({{"stmt-type","assign"},{"name","__" + name + "_d" + to_string(k)},
			               {"value",wrapConvert(lens[k], u64)}});
	return out;
	// LCOV_EXCL_BR_STOP
} // LCOV_EXCL_BR_LINE

// `v ->> loc as T` stores the pointer to what v owns into loc, for C to hold.
// What loc held is no Palan owner's, so nothing frees it.
json PlnSemanticAnalyzer::sa_transfer_to_owner_view(const json& stmt, const json& target)
{
	string et = target["expr-type"];
	json loc = et == "id" ? sa_expression(target)
	         : et == "arr-index" ? sa_expr_arr_index(target, /*forWrite=*/true)
	         : sa_expr_field_access(target, /*forWrite=*/true); // LCOV_EXCL_BR_LINE
	const json& locType = loc["value-type"];
	json owned = ownedCastType(stmt, stmt["owner-view-type"]);
	if (owned.is_null() || !isPtrBorrow(locType) || !locType["mutable"].get<bool>()) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_OwnerViewTargetNeedsPtr, typeDisplayName(locType)) << endl;
		exit(1);
	}
	json value = sa_move_source(stmt["value"]);
	if (!isOwnedBlock(value["value-type"])) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_TransferFromNonOwner, "->>") << endl;
		exit(1);
	}
	if (!copyShapeMatch(value["value-type"], owned)) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_TransferShapeMismatch,
			typeDisplayName(owned), typeDisplayName(value["value-type"])) << endl;
		exit(1);
	}
	// LCOV_EXCL_BR_START
	json result = json::array();
	value = heapStructRet(move(value), result);
	json pointer = value;
	pointer.erase("category");
	pointer["value-type"] = locType;
	json store = stmt;
	store.erase("ownership-transfer");
	store.erase("owner-view-type");
	store["value"] = {{"expr-type","pinned"},{"sa",move(pointer)}};
	for (auto& s : sa_store_stmt(store)) result.push_back(move(s));
	appendTransferSourceReset(result, stmt, value);
	return result;
	// LCOV_EXCL_BR_STOP
} // LCOV_EXCL_BR_LINE

// The key of the location a store writes to; none for a plain variable.
static const char* storeTargetKey(const json& stmt)
{
	string t = stmt["stmt-type"];
	if (t == "arr-assign") return "target";
	if (t == "field-assign") return "object";
	return nullptr;
}

// A store needs a scope for temps when its value is a store whose target is
// read back, when its target has a call (the value is evaluated before it, and
// a '->>' target is freed and stored to through one evaluation), or when '->>'
// resets the location it read.
static bool storePins(const json& stmt)
{
	const json& v = stmt["value"];
	if (chainsFromStore(v))
		return true;
	const char* target = storeTargetKey(stmt);
	if (target && hasCall(stmt[target]))
		return true;
	return stmt.value("ownership-transfer", false) && hasCall(v) && isLocationExpr(v);
}

// Whether stmts declare a temp at their own level, so they need a block.
static bool declaresTemp(const json& stmts)
{
	for (auto& st : stmts) {
		string t = st.value("stmt-type", "");
		if (t == "var-decl" || (t == "pending-alias" && declaresTemp(st["unaliased"])))
			return true;
	}
	return false;
}

// Stores run left to right, each evaluating every call in it once. A
// statement whose value chains from a store runs that store first, and one
// that evaluates a call into a temp holds the temp in a block of its own.
json PlnSemanticAnalyzer::sa_store_scope(const json& stmt)
{
	string t = stmt["stmt-type"];
	if (isStoreStmt(stmt) && !storePins(stmt))
		return sa_store_stmt(stmt);
	enterScope();
	json body = json::array();
	if (t == "expr") {
		json s = stmt;
		hoistStores(s["body"], body);
		body.push_back(sa_expression_stmt(s));
	} else if (t == "tapple-assign") {
		json s = stmt;
		hoistStores(s["value"], body);
		body.push_back(sa_tapple_assign(s));
	} else {
		sa_store_steps(stmt, body, false);
	}
	size_t idx = arrayScopeVars_.size() - 1;
	json frees = collectFreeStmts(idx, idx + 1);
	leaveScope();
	if (!declaresTemp(body) && frees.empty())
		return body;
	for (auto& f : frees)
		body.push_back(move(f));
	return json::array({{{"stmt-type", "block"}, {"body", move(body)}}}); // LCOV_EXCL_BR_LINE
} // LCOV_EXCL_BR_LINE

json PlnSemanticAnalyzer::sa_store_stmt(const json& stmt, json* litAddr)
{
	string t = stmt["stmt-type"];
	// LCOV_EXCL_BR_START
	json target = t == "assign" ? json{{"expr-type", "id"}, {"name", stmt["name"]}}
	            : t == "arr-assign" ? stmt["target"]
	            : json{{"expr-type", "field-access"}, {"object", stmt["object"]}, {"field", stmt["field"]}};
	// LCOV_EXCL_BR_STOP
	checkRangeChange(stmt, target, stmt.value("ownership-transfer", false) ? RangeChange::Transfer : RangeChange::Copy);
	if (stmt.contains("owner-view-type"))
		return sa_transfer_to_owner_view(stmt, target);
	if (t == "assign")
		return sa_assign_stmt(stmt);
	if (t == "arr-assign")
		return sa_arr_assign_stmt(stmt, litAddr);
	return sa_field_assign(stmt, litAddr);
} // LCOV_EXCL_BR_LINE

// Runs the store `value` is, or the store a chained call's value starts from,
// and replaces it with the target it stored, read back.
void PlnSemanticAnalyzer::hoistStores(json& value, json& body)
{
	if (isStoreStmt(value))
		value = sa_store_steps(value, body, true);
	else if (value.value("expr-type", "") == "chain-call")
		hoistStores(value["value"], body);
}

// Appends the statements of one store to body and returns its target as an
// expression. The value is evaluated before the calls in the target, and a
// target read back, or one '->>' frees before storing to, has its calls
// evaluated into temps first so every use is of the same location.
json PlnSemanticAnalyzer::sa_store_steps(const json& stmt, json& body, bool readBack)
{
	json s = stmt;
	hoistStores(s["value"], body);
	json& value = s["value"];
	const char* target = storeTargetKey(s);
	bool targetCall = target && hasCall(s[target]);
	bool transfer = s.value("ownership-transfer", false);
	// '->>' reads the location it gives away again to reset it.
	if (hasCall(value) && isLocationExpr(value) && (targetCall || transfer))
		value = pinLocation(move(value), body);
	bool litReadBack = readBack && targetCall && isAggregateLit(value);
	json targetPins = json::array();
	if (targetCall && !litReadBack && (readBack || transfer))
		s[target] = pinLocation(move(s[target]), targetPins);

	json sFirst = s;
	json pre = json::array(), post = json::array();
	set<string> roots;
	bool first = targetCall && !isAggregateLit(value) && valueFirst(sFirst, pre, post, roots);

	json litAddr;
	json plain = targetPins;
	if (!first || !roots.empty())
		for (auto& r : sa_store_stmt(s, litReadBack ? &litAddr : nullptr))
			plain.push_back(move(r));
	if (!first) {
		for (auto& p : plain)
			body.push_back(move(p));
	} else {
		// LCOV_EXCL_BR_START
		json aliased = pre;
		for (auto& p : targetPins)
			aliased.push_back(p);
		for (auto& r : sa_store_stmt(sFirst))
			aliased.push_back(move(r));
		for (auto& f : post)
			aliased.push_back(move(f));
		// Only pins read back must outlive the store.
		if (!readBack)
			aliased = json::array({{{"stmt-type", "block"}, {"body", move(aliased)}}});
		if (roots.empty()) {
			for (auto& a : aliased)
				body.push_back(move(a));
		} else {
			pendingAliases_++;
			body.push_back({{"stmt-type", "pending-alias"}, {"vars", roots},
			                {"aliased", move(aliased)}, {"unaliased", move(plain)}});
		}
		// LCOV_EXCL_BR_STOP
	}

	if (litReadBack)
		return litAddr;
	string t = s["stmt-type"];
	json readExpr;
	if (t == "assign")
		readExpr = {{"expr-type", "id"}, {"name", s["name"]}}; // LCOV_EXCL_BR_LINE
	else if (t == "arr-assign")
		return s["target"];
	else
		readExpr = {{"expr-type", "field-access"}, {"object", s["object"]}, {"field", s["field"]}}; // LCOV_EXCL_BR_LINE
	if (s.contains("loc")) readExpr["loc"] = s["loc"];
	return readExpr;
} // LCOV_EXCL_BR_LINE

// Rewrites s so its value is evaluated into pre, ahead of the target's calls,
// with post releasing what pre holds; false when there is nothing to evaluate
// ahead. When all the value reads is the storage of local variables, roots
// gets them: the value can change only if one of them has given a pointer to
// its storage away, which the end of the function tells.
bool PlnSemanticAnalyzer::valueFirst(json& s, json& pre, json& post, set<string>& roots)
{
	json& value = s["value"];
	json toType = storeTargetType(s);
	auto settle = [&](bool known) {
		if (!known) roots.clear();
	};
	if (s.value("ownership-transfer", false)) {
		// A variable's own slot can't be reached through a pointer.
		if (value.value("expr-type", "") == "id")
			return false;
		bool cast = value.value("expr-type", "") == "cast";
		if (!isLocationExpr(value) && !cast) {
			if (!hasCall(value))
				return false;
			value = pinValue(sa_expression_for(value, toType), value, pre);
			return true;
		}
		// What it gives away moves out now: the pointer is read and the
		// location reset before the target's calls.
		json saSrc = sa_move_source(value);
		settle(storageRoots(saSrc, roots, false));
		json src = s;
		value = pinValue(saSrc, value, pre);
		appendTransferSourceReset(pre, src, saSrc);
		value["sa"]["category"] = "expiring";
		return true;
	}
	if (isCopiedByValue(toType)) {
		json saSrc = sa_expression(value);
		if (isExpiringOwned(saSrc)) {
			value = pinValue(move(saSrc), value, pre);
			return true;
		}
		settle(storageRoots(saSrc, roots, true));
		value = snapshotCopy(saSrc, value, pre, post);
		return true;
	}
	json saValue = sa_expression_for(value, toType);
	if (!hasCall(value)) {
		bool known = storageRoots(saValue, roots, false);
		if (known && roots.empty())
			return false;
		settle(known);
	}
	value = pinValue(move(saValue), value, pre);
	return true;
} // LCOV_EXCL_BR_LINE

json PlnSemanticAnalyzer::storeTargetType(const json& stmt)
{
	if (stmt["stmt-type"] == "arr-assign")
		return sa_expr_arr_index(stmt["target"], /*forWrite=*/true)["value-type"];
	FieldChain chain = resolveObjectChain(stmt["object"], /*forWrite=*/true);
	return fieldValueType(findFieldOrExit(chain.structName, stmt["field"].get<string>(), stmt, true));
} // LCOV_EXCL_BR_LINE

// A copy of what saSrc holds, made into pre and released by post, standing in
// for the value as a `pinned` node; the value itself when its type can't be
// declared, which the store then rejects as a copy.
json PlnSemanticAnalyzer::snapshotCopy(const json& saSrc, const json& locNode, json& pre, json& post)
{
	// LCOV_EXCL_BR_START
	const json& vt = saSrc["value-type"];
	bool isStruct = isStructPntr(vt);
	json declType = isStruct ? json{{"type-kind", "prim"}, {"type-name", vt["base-type"]["type-name"]}}
	              : isVarrPntr(vt) || isDictPntr(vt) || vt.contains("arr-size") ? astTypeOf(vt) : json();
	if (declType.is_null())
		return locNode;
	string name = "__snap_" + to_string(tempVarCounter_++);
	json decl = {{"stmt-type", "var-decl"}, {"loc", locNode.value("loc", json::array())},
	             {"vars", json::array({{{"name", name}, {"var-type", declType}}})}};
	for (auto& st : isStruct ? sa_struct_var_decl(decl, VarStorage::Heap) : sa_resolved_var_decl(decl))
		pre.push_back(st);
	post.push_back(takeScopeRelease(name));
	const json type = *findVar(name);
	json dst = {{"expr-type", "id"}, {"name", name}, {"var-type", type}, {"value-type", type}};
	pre.push_back(makeCopyStmt(locNode, dst, saSrc, CopyInto::New));
	json pinned = {{"expr-type", "pinned"}, {"sa", move(dst)}};
	if (locNode.contains("loc")) pinned["loc"] = locNode["loc"];
	return pinned;
	// LCOV_EXCL_BR_STOP
} // LCOV_EXCL_BR_LINE

// The release of a variable of the current scope, which its caller now runs
// instead of the scope's end.
json PlnSemanticAnalyzer::takeScopeRelease(const string& name)
{
	auto& scope = arrayScopeVars_.back();
	auto it = find_if(scope.begin(), scope.end(), [&](const ScopeVar& v){ return v.name == name; });
	json release = move(it->release);
	scope.erase(it);
	return release;
}

const json& PlnSemanticAnalyzer::scopeRelease(const string& name) const
{
	for (auto scope = arrayScopeVars_.rbegin(); scope != arrayScopeVars_.rend(); ++scope)
		for (auto& v : *scope)
			if (v.name == name) return v.release;
	static const json none;
	return none; // LCOV_EXCL_LINE -- callers check isOwnedVar first
}

// The variable whose storage a pointer value points into, if one does.
static string pointeeVar(const json& v)
{
	string et = v.value("expr-type", "");
	if (et == "id" || et == "addr-of")
		return v["name"];
	if (et == "convert")
		return pointeeVar(v["src"]);
	if (et == "field-access")
		return v.contains("var") ? v["var"].get<string>() : pointeeVar(v["ptr-expr"]);
	if (et == "arr-index")
		return pointeeVar(v["array"]);
	return "";
}

void PlnSemanticAnalyzer::noteEscape(const json& saValue)
{
	if (!saValue.contains("value-type") || saValue["value-type"].value("type-kind", "") != "pntr")
		return;
	string name = pointeeVar(saValue);
	if (!name.empty())
		escapedVars_.insert(name);
}

// Collects into roots the local variables whose storage v reads -- with
// contents, also all that v points to, as a copy of it reads. False when v
// reads memory reached through a pointer, which a call may reach too.
bool PlnSemanticAnalyzer::storageRoots(const json& v, set<string>& roots, bool contents)
{
	string et = v.value("expr-type", "");
	auto ownStorage = [&](const string& name) { return isOwnedVar(name) || isFrameVar(name); };
	if (et == "id") {
		string name = v["name"];
		const json* vt = findVar(name);
		if (vt == nullptr || (contents && vt->value("type-kind", "") == "pntr" && !ownStorage(name)))
			return false;
		roots.insert(name);
		return true;
	}
	// The struct a field is read from, or the array an element is: storage
	// reached only through an owner, never through a borrow or raw pointer.
	auto ownedBase = [&](const json& base) {
		return !base["value-type"].contains("mutable") && storageRoots(base, roots, true);
	};
	if (et == "field-access") {
		if (!v.contains("var"))
			return ownedBase(v["ptr-expr"]);
		string name = v["var"];
		if (!ownStorage(name))
			return false;
		roots.insert(name);
		return true;
	}
	if (et == "arr-index")
		return ownedBase(v["array"]) && storageRoots(v["index"], roots, false);
	if (et == "call" || et == "call-ptr" || et == "temp")
		return false;
	for (auto& [k, child] : v.items())
		if (child.is_object() && child.contains("expr-type") && !storageRoots(child, roots, false))
			return false;
	return true;
} // LCOV_EXCL_BR_LINE

// Replaces each `pending-alias` with the statements for whether one of its
// variables gave a pointer to its storage away.
void PlnSemanticAnalyzer::resolvePending(json& stmts)
{
	if (stmts.is_object()) {
		for (auto& [k, v] : stmts.items())
			resolvePending(v);
		return;
	}
	if (!stmts.is_array())
		return;
	json out = json::array();
	for (auto& st : stmts) {
		if (!st.is_object() || st.value("stmt-type", "") != "pending-alias") {
			resolvePending(st);
			out.push_back(move(st));
			continue;
		}
		bool aliased = false;
		for (auto& name : st["vars"])
			aliased = aliased || escapedVars_.count(name.get<string>());
		json& pick = st[aliased ? "aliased" : "unaliased"];
		resolvePending(pick);
		for (auto& p : pick)
			out.push_back(move(p));
	}
	stmts = move(out);
} // LCOV_EXCL_BR_LINE

// The calls along a location -- its indices and the expression it starts
// from -- each evaluated once into a temp, leaving the same location.
json PlnSemanticAnalyzer::pinLocation(json loc, json& body)
{
	string et = loc.value("expr-type", "");
	if (et == "arr-index") {
		loc["array"] = pinLocation(move(loc["array"]), body);
		if (hasCall(loc["index"]))
			loc["index"] = pinValue(sa_expression(loc["index"]), loc["index"], body);
	} else if (et == "field-access") {
		loc["object"] = pinLocation(move(loc["object"]), body);
	} else if (et != "id" && hasCall(loc)) {
		loc = pinValue(sa_expression(loc), loc, body);
	}
	return loc;
} // LCOV_EXCL_BR_LINE

// saValue evaluated into a temp `__pin_N` declared in body; empty when it is
// left where it is.
string PlnSemanticAnalyzer::pinTemp(json saValue, const json& locNode, json& body)
{
	if (holdsCallTemps(saValue)) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_BorrowOfTempEscapes) << endl;
		exit(1);
	}
	// LCOV_EXCL_BR_START
	string name = "__pin_" + to_string(tempVarCounter_++);
	const json type = saValue["value-type"];
	declareVar(name, type);
	// The pinned struct is a heap block like any other returned one.
	if (isStructRetCall(saValue)) {
		for (auto& s : declareStructRet(name, type, move(saValue), VarStorage::Heap)) body.push_back(move(s));
		return name;
	}
	body.push_back({{"stmt-type", "var-decl"},
	                {"vars", json::array({{{"name", name}, {"var-type", type}, {"init", move(saValue)}}})}});
	return name;
	// LCOV_EXCL_BR_STOP
} // LCOV_EXCL_BR_LINE

// saValue evaluated into a temp, standing in for the expression it was
// analyzed from: a `pinned` node, which sa_expression gives back as the temp
// with saValue's type and category, so a store treats it as the value itself.
json PlnSemanticAnalyzer::pinValue(json saValue, const json& astValue, json& body)
{
	// LCOV_EXCL_BR_START
	const json type = saValue["value-type"];
	json category = saValue.value("category", json());
	bool resized = saValue.value("resized", false);
	// The temp keeps no lengths of its own: they stay those of what it holds.
	json lens = hasRunSize(type) ? valueLens(saValue) : json();
	string name = pinTemp(move(saValue), astValue, body);
	if (name.empty())
		return astValue;
	json temp = {{"expr-type", "id"}, {"name", name}, {"var-type", type}, {"value-type", type}};
	if (!category.is_null()) temp["category"] = move(category);
	if (resized) temp["resized"] = true;
	if (!lens.is_null()) temp["lens"] = move(lens);
	json pinned = {{"expr-type", "pinned"}, {"sa", move(temp)}};
	if (astValue.contains("loc")) pinned["loc"] = astValue["loc"];
	return pinned;
	// LCOV_EXCL_BR_STOP
} // LCOV_EXCL_BR_LINE

// `x -> d[k]` adds k if it is not there, which may move the values, so x is
// evaluated first (`len(@d) -> d["n"]` counts without "n") and stored at the
// address the adding returns.
json PlnSemanticAnalyzer::sa_dict_store_stmt(const json& stmt, json saDict, json* litAddr)
{
	checkRangeChange(stmt, stmt["target"]["array"], RangeChange::Borrow);
	if (!isWritableThrough(saDict["value-type"])) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_WriteThroughReadOnlyPtr) << endl;
		exit(1);
	}
	const json& value = stmt["value"];
	const json elem = saDict["value-type"]["base-type"]["base-type"];
	if (isAggregateLit(value) && (isStructPntr(elem) || isCopiedByValue(elem) || elem["type-kind"] == "struct"))
		return sa_lit_assign_via_temp(stmt, elem, litAddr);
	// LCOV_EXCL_BR_START
	const json& target = stmt["target"];
	json body = json::array();
	json store = stmt;
	if (hasCall(value)) {
		json saValue = stmt.value("ownership-transfer", false) ? sa_move_source(value) : sa_expression(value);
		store["value"] = pinValue(move(saValue), value, body);
	}
	json addr = dictValueAddr(move(saDict), target["array"], target["index"], "put", stmt);
	store["target"] = {{"expr-type","arr-index"},{"array",pinValue(move(addr), target, body)},
	                   {"index",{{"expr-type","lit-int"},{"value","0"}}},{"loc",target.value("loc", json::array())}};
	for (auto& s : sa_arr_assign_stmt(store)) body.push_back(move(s));
	return body;
	// LCOV_EXCL_BR_STOP
} // LCOV_EXCL_BR_LINE

json PlnSemanticAnalyzer::sa_arr_assign_stmt(const json& stmt, json* litAddr)
{
	json sa_base = sa_index_base(stmt["target"]["array"], /*forWrite=*/true);
	if (isDictPntr(sa_base["value-type"]))
		return sa_dict_store_stmt(stmt, move(sa_base), litAddr);
	json sa_target = sa_expr_arr_index(stmt["target"], /*forWrite=*/true, move(sa_base));
	bool transfer = stmt.value("ownership-transfer", false);
	if (!isWritableThrough(sa_target["array"]["value-type"])) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_WriteThroughReadOnlyPtr) << endl;
		exit(1);
	}
	if (isAggregateLit(stmt["value"]) && (isVarrStorage(sa_target["value-type"]) || isDictStorage(sa_target["value-type"]) // LCOV_EXCL_BR_LINE
	    || assignsByFunc(withoutArrPermission(sa_target["value-type"])))) // LCOV_EXCL_BR_LINE
		return sa_lit_assign_via_temp(stmt, sa_target["value-type"], litAddr);
	if (isAggregateLit(stmt["value"]) && (isStructPntr(sa_target["value-type"]) || isArrLevel(sa_target["value-type"]))) {
		json lit = sa_aggregate_lit_assign(stmt, stmt["target"], sa_target["value-type"], litAddr);
		return litAddr ? lit : json::array({move(lit)}); // LCOV_EXCL_BR_LINE
	}
	if (!transfer && isCopiedByValue(sa_target["value-type"]))
		return json::array({makeCopyStmt(stmt, sa_target, // LCOV_EXCL_BR_LINE
			sa_expression(literalConversion(stmt["value"], sa_target["value-type"])), CopyInto::Existing)}); // LCOV_EXCL_BR_LINE
	if (sa_target.value("addr-only", false)) {
		// The element itself is an address computation (e.g. a struct array
		// element), not a storage slot to overwrite -- assign to its fields instead.
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_AssignToWholeStructElem) << endl;
		exit(1);
	}
	const json& targetType = sa_target["value-type"];
	// A borrowed slot never owns what it points to, so it cannot be given anything.
	if (transfer && isPtrBorrow(targetType)) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_TransferToBorrowSlot) << endl;
		exit(1);
	}
	const PlnType* toType = registry_.fromJson(targetType);
	json sa_value = transfer ? sa_move_source(stmt["value"], toType)
	                         : typedNull(sa_expression_for(literalConversion(stmt["value"], targetType), targetType), targetType); // LCOV_EXCL_BR_LINE
	sa_value = convertForBinding(stmt, sa_value, toType, registry_.toJson(toType));
	checkElemShape(stmt, sa_value, targetType);
	checkBorrowSource(stmt, sa_value, targetType);
	checkPtrPermission(stmt, sa_value["value-type"], targetType);
	checkRangeBorrowBinding(stmt, stmt["value"], targetType);

	json arr_assign = {{"stmt-type", "arr-assign"}, {"target", sa_target}, {"value", sa_value}}; // LCOV_EXCL_BR_LINE
	if (!transfer)
		return json::array({arr_assign}); // LCOV_EXCL_BR_LINE

	arr_assign["ownership-transfer"] = true;
	json result = transferStmts(stmt, makeOwnedValueFreeStmt(sa_target), move(arr_assign), sa_value);
	// The row's owner frees it by the row's lengths, which the new row must have.
	if (isArrLevel(targetType)) checkMoveShape(stmt, sa_value, targetType, /*inherits=*/false);
	return result;
} // LCOV_EXCL_BR_LINE

// Every value is evaluated into a temp before anything is written, so
// `[v.y, v.x] -> v[0]` swaps, and the target's address is taken once, so an
// index expression in it is evaluated once rather than once per element.
json PlnSemanticAnalyzer::sa_aggregate_lit_assign(const json& stmt, const json& targetAst, const json& targetType,
                                                   json* litAddr)
{
	if (stmt.value("ownership-transfer", false)) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_TransferFromNonOwner, "->>") << endl;
		exit(1);
	}
	bool isStruct = isStructPntr(targetType);
	const StructDef* def = nullptr;
	ArrLitShape shape;
	json values;
	if (isStruct) {
		if (!isStructStorage(targetType)) {
			cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_StructLitTarget) << endl;
			exit(1);
		}
		def = &requireCompleteStruct(targetType["base-type"]["type-name"].get<string>(), stmt);
		values = sa_struct_lit(stmt["value"], *def);
	} else {
		string name = arrLitTargetName(targetAst);
		// A row reached through a borrowed array has the same type, but it is
		// the borrowed storage itself, so only the variable or field is a borrow.
		string tet = targetAst.value("expr-type", "");
		if ((tet == "id" || tet == "field-access") && isArrBorrowVar(targetType)) {
			cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_ArrLitTarget, name) << endl;
			exit(1);
		}
		const json& lit = stmt["value"];
		if (lit.value("expr-type", "") != "arr-lit") {
			cerr << locPrefix(lit) << PlnSaMessage::getMessage(E_DictLitContext) << endl;
			exit(1);
		}
		shape = arrLitShape(stmt, name, targetType);
		values = sa_arr_lit_values(lit, shape, name);
		checkArrLitCount(stmt, lit["items"], name, shape);
	}

	if (!litAddr)
		enterScope();
	// LCOV_EXCL_BR_START
	json body = json::array();
	auto bindTemp = [&](json init) -> string {
		string name = "__lit_" + to_string(tempVarCounter_++);
		const json type = init["value-type"];
		declareVar(name, type);
		body.push_back({{"stmt-type", "var-decl"},
		                {"vars", json::array({{{"name", name}, {"var-type", type}, {"init", move(init)}}})}});
		return name;
	};
	// A struct value is copied into a temp struct of its own, so a source
	// overlapping the target (`[q.b, q.a] -> q`) is read before any write.
	auto bindStructTemp = [&](json v) -> string {
		if (isStructRetCall(v)) {
			string name = "__lit_" + to_string(tempVarCounter_++);
			const json type = v["value-type"];
			declareVar(name, type);
			for (auto& s : declareStructRet(name, type, move(v), VarStorage::Heap)) body.push_back(move(s));
			addOwnedScopeVar(name, makeOwnedFreeStmt(name, type));
			return name;
		}
		string name = "__lit_" + to_string(tempVarCounter_++);
		json decl = {{"stmt-type", "var-decl"}, {"loc", stmt.value("loc", json::array())},
			{"vars", json::array({{{"name", name},
				{"var-type", {{"type-kind", "prim"}, {"type-name", v["value-type"]["base-type"]["type-name"]}}}}})}};
		for (auto& s : sa_struct_var_decl(decl, VarStorage::Heap)) body.push_back(s);
		const json type = *findVar(name);
		body.push_back(makeCopyStmt(stmt, {{"expr-type", "id"}, {"name", name}, {"var-type", type}, {"value-type", type}}, v, CopyInto::New));
		return name;
	};
	function<void(json&)> bindLeaves = [&](json& vals) { // LCOV_EXCL_LINE -- only exception cleanup is attributed here
		for (auto& v : vals) {
			if (v.is_array()) {
				bindLeaves(v);
				continue;
			}
			if (isDictLitValues(v)) {
				bindLeaves(v["values"]);
				continue;
			}
			string name = isStructPntr(v["value-type"]) ? bindStructTemp(move(v)) : bindTemp(move(v));
			const json type = *findVar(name);
			v = {{"expr-type", "id"}, {"name", name}, {"var-type", type}, {"value-type", type}};
		}
	};
	bindLeaves(values);
	json target = {{"expr-type", "id"},
	               {"name", bindTemp(sa_expression({{"expr-type", "addr-of"}, {"mutable", true}, {"object", targetAst}}))}};
	if (isStruct) {
		json obj = {{"expr-type", "arr-index"}, {"array", target},
		            {"index", {{"expr-type", "lit-uint"}, {"value", "0"}}}};
		emitStructLitAssigns(obj, *def, values, body);
	} else {
		emitArrLitAssigns(target, shape, values, body);
	}
	// The caller's scope holds the temps and releases them.
	if (litAddr) {
		const json type = *findVar(target["name"].get<string>());
		*litAddr = {{"expr-type", "pinned"},
		            {"sa", {{"expr-type", "id"}, {"name", target["name"]}, {"var-type", type}, {"value-type", type}}}};
		return body;
	}
	size_t idx = arrayScopeVars_.size() - 1;
	for (auto& s : collectFreeStmts(idx, idx + 1)) body.push_back(s);
	// LCOV_EXCL_BR_STOP
	leaveScope();
	return {{"stmt-type", "block"}, {"body", move(body)}}; // LCOV_EXCL_BR_LINE
} // LCOV_EXCL_BR_LINE

json PlnSemanticAnalyzer::transferStmts(const json& stmt, json targetRelease, json store, const json& value)
{
	json result = json::array();
	if (isStructRetCall(value))
		store["value"] = heapStructRet(store["value"], result);
	// A computed value may read what the target holds (`f(v) ->> v`), so it is
	// evaluated before the target releases that.
	else if (value.value("category", "") == "expiring" && !isLocationExpr(value)) {
		// LCOV_EXCL_BR_START
		json type = value["value-type"];
		string name = "__pin_" + to_string(tempVarCounter_++);
		declareVar(name, type);
		result.push_back({{"stmt-type", "var-decl"},
		                  {"vars", json::array({{{"name", name}, {"var-type", type}, {"init", move(store["value"])}}})}});
		store["value"] = {{"expr-type", "id"}, {"name", name}, {"var-type", type}, {"value-type", type}, {"category", "expiring"}};
		// LCOV_EXCL_BR_STOP
	}
	result.push_back(move(targetRelease));
	result.push_back(move(store));
	appendTransferSourceReset(result, stmt, value);
	return result;
} // LCOV_EXCL_LINE

// A struct a call returns, given to an owner, as a heap block of its own
// declared in pre.
json PlnSemanticAnalyzer::heapStructRet(json value, json& pre)
{
	if (!isStructRetCall(value))
		return value;
	// LCOV_EXCL_BR_START
	json type = value["value-type"];
	string name = "__pin_" + to_string(tempVarCounter_++);
	declareVar(name, type);
	for (auto& s : declareStructRet(name, type, move(value), VarStorage::Heap)) pre.push_back(move(s));
	return {{"expr-type", "id"}, {"name", name}, {"var-type", type}, {"value-type", type}, {"category", "expiring"}};
	// LCOV_EXCL_BR_STOP
} // LCOV_EXCL_BR_LINE

// Only an owner may give its value away, and it then holds NULL so its own free
// is a no-op: C free and the build-mgr generated __pln_free_* functions all
// accept NULL. A call result has no owner to reset.
void PlnSemanticAnalyzer::appendTransferSourceReset(json& stmts, const json& stmt, const json& saValue, const string& op)
{
	if (isExpiringOwned(saValue))
		return;
	if (saValue.value("category", "") == "opaque-convert" || saValue.value("category", "") == "resized") {
		json converted = stmt;
		converted["value"] = stmt["value"]["src"];
		appendTransferSourceReset(stmts, converted, saValue["src"], op);
		return;
	}
	// A pointer viewed as an owner is no Palan owner to reset.
	if (saValue.value("category", "") == "owner-view") {
		if (saValue.value("read-only", false)) {
			cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_MoveFromReadOnlyView, op) << endl;
			exit(1);
		}
		return;
	}
	const json& src = stmt["value"];
	checkRangeChange(stmt, src, RangeChange::Transfer);
	string et = src.value("expr-type", "");
	auto nullOf = [](const json& vt) -> json {
		return {{"expr-type","lit-int"},{"value","0"},{"value-type",vt}}; // LCOV_EXCL_BR_LINE
	}; // LCOV_EXCL_BR_LINE
	if (et == "id" && saValue.value("category", "") == "owned") {
		stmts.push_back({{"stmt-type", "assign"}, {"name", saValue["name"]}, // LCOV_EXCL_BR_LINE
		                 {"value", nullOf(saValue["value-type"])}});
		// Any function may fill a global again.
		if (!isGlobalName(saValue["name"])) given_.pending.push_back({saValue["name"], true});
		return;
	}
	if (et == "field-access" || et == "arr-index") {
		json loc = et == "field-access" ? sa_expr_field_access(src, /*forWrite=*/true)
		                                : sa_expr_arr_index(src, /*forWrite=*/true);
		if (!loc.value("addr-only", false) && isCopiedByValue(loc["value-type"])) {
			if (et == "arr-index") {
				if (!isWritableThrough(loc["array"]["value-type"])) {
					cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_WriteThroughReadOnlyPtr) << endl;
					exit(1);
				}
				stmts.push_back({{"stmt-type", "arr-assign"}, {"target", loc}, // LCOV_EXCL_BR_LINE
				                 {"value", nullOf(loc["value-type"])}});
			} else {
				json reset = {{"stmt-type", "field-assign"}, {"offset", loc["offset"]},
				              {"value-type", loc["value-type"]}, {"value", nullOf(loc["value-type"])}}; // LCOV_EXCL_BR_LINE
				if (loc.contains("var")) reset["var"] = loc["var"];
				else reset["ptr-expr"] = loc["ptr-expr"];
				stmts.push_back(move(reset));
			}
			return;
		}
	}
	cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_TransferFromNonOwner, op) << endl;
	exit(1);
} // LCOV_EXCL_BR_LINE

json PlnSemanticAnalyzer::sa_move_source(const json& ast, const PlnType* expectedType)
{
	if (ast.value("expr-type", "") == "cast") return sa_expr_cast(ast, true);
	return sa_expression(ast, expectedType);
}

// A length known only at run time is read where the array keeps it: a cast
// or a temp holding the array keeps it as "lens", a variable in its own.
json PlnSemanticAnalyzer::valueLens(const json& value)
{
	// LCOV_EXCL_BR_START
	if (value.contains("lens")) return value["lens"];
	const json& vt = value["value-type"];
	const json& at = value.value("expr-type", "") == "addr-of" ? value["object"] : value;
	json lens = json::array();
	const json* t = &vt;
	// A `[+]T`'s rows have constant lengths.
	if (isVarrPntr(vt)) {
		lens.push_back(varrLen(value));
		t = &vt["base-type"]["base-type"];
	}
	for (size_t k = 0; isArrLevel(*t); t = &(*t)["base-type"], k++) {
		if (t->contains("arr-size")) {
			lens.push_back(lenLit((*t)["arr-size"].get<int64_t>()));
		} else {
			auto [root, depth] = arrRoot(at);
			lens.push_back(runtimeLen(*root, depth + k));
		}
	}
	return lens;
	// LCOV_EXCL_BR_STOP
} // LCOV_EXCL_BR_LINE

json PlnSemanticAnalyzer::giveValue(const json& srcAst, const json& locNode, const string& op, json& body, json& reset)
{
	// The location is read again to reset it; its calls run once.
	json src = isLocationExpr(srcAst) && hasCall(srcAst) ? pinLocation(srcAst, body) : srcAst;
	json saValue = sa_move_source(src);
	if (!isOwnedBlock(saValue["value-type"])) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_TransferFromNonOwner, op) << endl;
		exit(1);
	}
	if (isExpiringOwned(saValue))
		return saValue;
	// LCOV_EXCL_BR_START
	json owner = {{"value", src}, {"loc", locNode.value("loc", json::array())}};
	json given = pinValue(saValue, src, body)["sa"];
	appendTransferSourceReset(reset, owner, saValue, op);
	given["category"] = "expiring";
	return given;
	// LCOV_EXCL_BR_STOP
} // LCOV_EXCL_BR_LINE

void PlnSemanticAnalyzer::checkReturnedBorrow(const json& stmt, const json& valueAst)
{
	const json* at = &valueAst;
	while (at->value("expr-type", "") == "cast") at = &(*at)["src"];
	if (at->value("expr-type", "") != "addr-of") return;
	at = &(*at)["object"];
	while (at->value("expr-type", "") != "id") {
		string et = at->value("expr-type", "");
		if (et != "field-access" && et != "arr-index") return;
		at = &(*at)[et == "arr-index" ? "array" : "object"];
		// Storage reached through a borrow or a raw pointer is not its variable's.
		json vt = operandValueType(*at);
		if (!isOwnedBlock(vt) || vt.contains("mutable")) return;
	}
	string name = (*at)["name"];
	// isLocalVar leaves out the parameters; an owned one is the function's too.
	const json* vt = findVar(name);
	if (!vt || (!isLocalVar(name) && !isOwnedBlock(*vt))) return;
	cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_ReturnOwnBorrow, name) << endl;
	exit(1);
}

json PlnSemanticAnalyzer::sa_return_stmt(const json& stmt)
{
	if (currentFunc_ == nullptr) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_ReturnOutsideFunction) << endl;
		exit(1);
	}
	bool hasRets   = currentFunc_->contains("rets");
	bool hasRetType = currentFunc_->contains("ret-type");
	bool hasValues = stmt.contains("values");

	if (hasRets) {
		// Named return values: bare return only
		if (hasValues) {
			cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_NamedRetBareReturn) << endl;
			exit(1);
		}
		return {{"stmt-type", "return"}}; // LCOV_EXCL_BR_LINE
	}
	if (hasRetType) {
		// Single return value: exactly one expression required
		if (!hasValues || stmt["values"].size() != 1) {
			cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_SingleRetOneExpr) << endl;
			exit(1);
		}
		const json& retType = (*currentFunc_)["ret-type"];
		const PlnType* toType = registry_.fromJson(retType);
		const json& valueAst = stmt["values"][0];
		json value = valueAst.value("expr-type", "") == "cast" && isArrLevel(retType) ? sa_move_source(valueAst)
		           : typedNull(sa_expression_for(literalConversion(valueAst, retType), retType), retType); // LCOV_EXCL_BR_LINE
		// A borrow is returned as a copy (copyReturnBlock), which checks its shape.
		if (isOwnedBlock(retType) && isPtrBorrow(value["value-type"]))
			return {{"stmt-type", "return"}, {"values", json::array({value})}}; // LCOV_EXCL_BR_LINE
		// The type registry ignores sizes, and an owned array variable is
		// returned without a copy that would check them.
		if (isArrLevel(retType) && isOwnedBlock(retType)) {
			// An owned variable is moved out; anything else is copied (copyReturnBlock).
			bool moved = value.value("category", "") == "resized"
				|| (valueAst.value("expr-type", "") == "id" && isOwnedVar(valueAst["name"]));
			if (moved) {
				checkMoveShape(stmt, value, retType, /*inherits=*/false);
			} else if (!copySourceMatch(value["value-type"], retType)) {
				cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_CopyShapeMismatch,
					typeDisplayName(retType), typeDisplayName(value["value-type"])) << endl;
				exit(1);
			}
			return {{"stmt-type", "return"}, {"values", json::array({value})}}; // LCOV_EXCL_BR_LINE
		}
		checkReturnedBorrow(stmt, valueAst);
		value = convertForBinding(stmt, value, toType, registry_.toJson(toType));
		checkElemShape(stmt, value, (*currentFunc_)["ret-type"]);
		checkBorrowSource(stmt, value, (*currentFunc_)["ret-type"]);
		checkPtrPermission(stmt, value["value-type"], (*currentFunc_)["ret-type"]);
		return {{"stmt-type", "return"}, {"values", json::array({value})}}; // LCOV_EXCL_BR_LINE
	}
	// Void function: bare return only
	if (hasValues) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_VoidBareReturn) << endl;
		exit(1);
	}
	return {{"stmt-type", "return"}}; // LCOV_EXCL_BR_LINE
} // LCOV_EXCL_BR_LINE

// Moves ret's single value into a fresh temp var; returns that var's decl and
// rewrites ret to return the temp.
json PlnSemanticAnalyzer::bindReturnValueToTemp(const json& stmt, json& ret)
{
	json& value = ret["values"][0];
	json type = value.contains("value-type") ? value["value-type"] : (*currentFunc_)["ret-type"];
	string name = "__ret_" + to_string(tempVarCounter_++);
	declareVar(name, type, &stmt);
	json decl = {
		{"stmt-type", "var-decl"},
		{"vars", json::array({{{"name", name}, {"var-type", type}, {"init", value}}})} // LCOV_EXCL_BR_LINE
	}; // LCOV_EXCL_BR_LINE
	value = {{"expr-type", "id"}, {"name", name}, {"var-type", type}, {"value-type", type}}; // LCOV_EXCL_BR_LINE
	return decl;
} // LCOV_EXCL_BR_LINE

// `return [..];` from a function returning a block becomes
// `T __ret_N = [..]; return __ret_N;`, so the literal goes through the
// variable's initializer and the return through the path of a returned
// variable. The declared type is the resolved one, not the normalized
// ret-type, which is the block's storage pointer.
json PlnSemanticAnalyzer::bindReturnLit(json& stmt)
{
	if (currentFunc_->contains("rets") || !currentFunc_->contains("ret-type")
			|| !isOwnedBlock((*currentFunc_)["ret-type"]) || !stmt.contains("values") || stmt["values"].size() != 1
			|| !isAggregateLit(stmt["values"][0]))
		return json::array();
	if (!retSlot_.empty()) {
		// LCOV_EXCL_BR_START
		json& value = stmt["values"][0];
		const json& type = *findVar(retSlot_);
		const StructDef& def = structDefs_.at(type["base-type"]["type-name"].get<string>());
		json out = json::array();
		json init = structInitStmt(retSlot_, type);
		if (!init.is_null()) out.push_back(move(init));
		emitStructLitAssigns({{"expr-type", "id"}, {"name", retSlot_}}, def, sa_struct_lit(value, def), out);
		value = {{"expr-type", "id"}, {"name", retSlot_}, {"loc", value.value("loc", stmt.value("loc", json::array()))}};
		return out;
		// LCOV_EXCL_BR_STOP
	}
	string name = "__ret_" + to_string(tempVarCounter_++);
	json& value = stmt["values"][0];
	const json loc = stmt.value("loc", json::array());
	json var = {{"name", name}, {"var-type", (*currentFuncDef_)["ret-type"]}, {"init", value}}; // LCOV_EXCL_BR_LINE
	const json& retType = (*currentFunc_)["ret-type"];
	if (!isStructStorage(retType)) var["lit-name"] = typeDisplayName(retType);
	json decl = {{"stmt-type", "var-decl"}, {"loc", loc}, {"vars", json::array({move(var)})}}; // LCOV_EXCL_BR_LINE
	value = {{"expr-type", "id"}, {"name", name}, {"loc", value.value("loc", loc)}}; // LCOV_EXCL_BR_LINE
	return sa_resolved_var_decl(decl);
} // LCOV_EXCL_BR_LINE

// Writes ret's struct into the return slot and leaves ret bare. `returned`
// names a variable whose contents move there, so only its block is freed.
json PlnSemanticAnalyzer::returnIntoSlot(const json& stmt, json& ret, const string& returned)
{
	// LCOV_EXCL_BR_START
	json value = move(ret["values"][0]);
	ret.erase("values");
	const json& type = *findVar(retSlot_);
	json slot = {{"expr-type", "id"}, {"name", retSlot_}, {"var-type", type}, {"value-type", type}};
	if (value.value("expr-type", "") == "id" && value["name"] == retSlot_)
		return json::array();
	if (isStructRetCall(value))
		return bindStructRet(move(value), move(slot));
	json out = json::array();
	if (!returned.empty()) {
		const StructDef& def = structDefs_.at(type["base-type"]["type-name"].get<string>());
		json uint64_type = {{"type-kind", "prim"}, {"type-name", "uint64"}};
		json size = {{"expr-type", "lit-uint"}, {"value", to_string(def.totalSize)}, {"value-type", uint64_type}};
		out.push_back({{"stmt-type", "expr"}, {"body", {{"expr-type", "call"}, {"name", "memcpy"}, {"func-type", "c"},
			{"args", json::array({slot, value, size})}}}});
		if (isOwnedVar(returned)) out.push_back(makeFreeStmt(returned, value["value-type"]));
		return out;
	}
	json init = structInitStmt(retSlot_, type);
	if (!init.is_null()) out.push_back(move(init));
	out.push_back(makeCopyStmt(stmt, slot, value, CopyInto::New));
	return out;
	// LCOV_EXCL_BR_STOP
} // LCOV_EXCL_BR_LINE

// A returned block is freed by the caller, so one the function doesn't own
// (a borrow, a field, an element, a `$` variable) is returned as a copy. The copy is made after sa_return_stmt so a wrong value is still
// reported as a return error.
json PlnSemanticAnalyzer::copyReturnBlock(const json& stmt, json& ret)
{
	if (!ret.contains("values") || !isOwnedBlock((*currentFunc_)["ret-type"]))
		return json::array();
	json& value = ret["values"][0];
	if (!value.contains("value-type") || isExpiringOwned(value)
			|| !(isStructPntr(value["value-type"]) || isVarrPntr(value["value-type"])
			     || isDictPntr(value["value-type"]) || isArrLevel(value["value-type"])))
		return json::array();
	string name = "__ret_" + to_string(tempVarCounter_++);
	json decl = {{"stmt-type", "var-decl"}, {"loc", stmt.value("loc", json::array())}, // LCOV_EXCL_BR_LINE
		{"vars", json::array({{{"name", name}, {"var-type", (*currentFuncDef_)["ret-type"]}}})}}; // LCOV_EXCL_BR_LINE
	json result = sa_resolved_var_decl(decl);
	removeFromArrayScope(name);
	const json type = *findVar(name);
	json dst = {{"expr-type", "id"}, {"name", name}, {"var-type", type}, {"value-type", type}}; // LCOV_EXCL_BR_LINE
	result.push_back(makeCopyStmt(stmt, dst, value, CopyInto::New));
	value = dst;
	return result;
} // LCOV_EXCL_BR_LINE

// A variable whose declared type differs from its return value's receives it
// through a temp and is declared as `T x = temp;`, so it follows the same
// conversion rules as a single-value declaration.
json PlnSemanticAnalyzer::sa_tapple_decl(const json& stmt)
{
	json stmt2 = stmt;
	vector<VarStorage> storage;
	for (auto& v : stmt2["vars"]) {
		storage.push_back(takeVarStorage(v["var-type"]));
		v["var-type"] = resolveKnownType(stmt2, v["var-type"]);
	}
	return sa_resolved_tapple_decl(stmt2, storage);
} // LCOV_EXCL_BR_LINE

json PlnSemanticAnalyzer::sa_resolved_tapple_decl(const json& stmt, const vector<VarStorage>& storages)
{
	json callExpr = tappleCallExpr(stmt);
	const json& rets = findMultiRetFunc(stmt, callExpr, stmt["vars"].size())["rets"];
	callExpr["multi-ret"] = rets.size();

	// Process the call expression via sa_expression (resolves func-type, annotates args)
	json saCall = sa_expr_maybe_void(callExpr);

	// Add multi-return value-types from the function's rets
	json valueTypes = json::array();
	for (auto& r : rets)
		valueTypes.push_back(r["var-type"]);
	saCall["value-types"] = valueTypes;
	if (holdsCallTemps(saCall)) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_BorrowOfTempEscapes) << endl;
		exit(1);
	}

	json vars = json::array();
	json pre = json::array();
	json convDecls = json::array();
	vector<VarStorage> convStorage;
	// A returned struct is written into storage declared ahead of the call.
	auto structDest = [&](const string& name, const json& type, VarStorage storage) {
		// LCOV_EXCL_BR_START
		const StructDef& def = structDefs_.at(type["base-type"]["type-name"].get<string>());
		json post = json::array();
		json init = placeAggregateVar(stmt, name, type, structAllocExpr(def.name, false), makeOwnedFreeStmt(name, type),
		                              storage, def.totalSize, def.maxAlign, post, structShape(def.name));
		pre.push_back({{"stmt-type", "var-decl"}, {"vars", json::array({{{"name", name}, {"var-type", type}, {"init", move(init)}}})}});
		vars.push_back({{"ret-dest", {{"expr-type", "id"}, {"name", name}, {"var-type", type}, {"value-type", type}}}});
		// LCOV_EXCL_BR_STOP
	}; // LCOV_EXCL_BR_LINE
	for (size_t i = 0; i < stmt["vars"].size(); i++) {
		const json& v = stmt["vars"][i];
		const json& retType = rets[i]["var-type"];
		json varType = v["var-type"];
		VarStorage storage = storages[i];
		string name = v["var-name"];
		// A frame variable can't take a returned array block over, so it is
		// declared from the temp, which moves the block's contents into the
		// frame and frees the block.
		bool movesIntoFrame = storage == VarStorage::Frame && isOwnedBlock(retType) && !isStructValue(retType);
		// A length known only at run time is no signature's, so such an array
		// copies from the returned one.
		json declared = varType.value("type-kind","") == "arr" || isDictAstType(varType) ? takeOverType(stmt, varType)
		              : sigType(stmt, varType);
		if (isStructValue(retType) && declared == retType) {
			declareVar(name, retType);
			structDest(name, retType, storage);
			continue;
		}
		if (storage == VarStorage::Frame || declared != retType) {
			string temp = "__tap_" + to_string(tempVarCounter_++);
			json init = {{"expr-type", "id"}, {"name", temp}, {"loc", stmt["loc"]}}; // LCOV_EXCL_BR_LINE
			if (movesIntoFrame)
				init = {{"expr-type", "pinned"}, {"loc", stmt["loc"]}, {"sa", {{"expr-type", "id"}, {"name", temp}, // LCOV_EXCL_BR_LINE
					{"var-type", retType}, {"value-type", retType}, {"category", "expiring"}}}}; // LCOV_EXCL_BR_LINE
			convDecls.push_back({{"stmt-type", "var-decl"}, {"loc", stmt["loc"]}, // LCOV_EXCL_BR_LINE
				{"vars", json::array({{{"name", name}, {"var-type", varType}, {"init", move(init)}}})}}); // LCOV_EXCL_BR_LINE
			convStorage.push_back(storage);
			name = temp;
		}
		declareVar(name, retType);
		if (isStructValue(retType)) {
			structDest(name, retType, VarStorage::Heap);
			continue;
		}
		// A returned block is owned by the caller.
		if (isOwnedBlock(retType) && !movesIntoFrame)
			addOwnedScopeVar(name, makeOwnedFreeStmt(name, retType));
		vars.push_back({{"var-name", name}, {"var-type", retType}}); // LCOV_EXCL_BR_LINE
	} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

	json result = pre;
	result.push_back({{"stmt-type", "tapple-decl"}, {"vars", vars}, {"value", saCall}}); // LCOV_EXCL_BR_LINE
	for (size_t i = 0; i < convDecls.size(); i++)
		for (auto& s : sa_resolved_var_decl(convDecls[i], {convStorage[i]}))
			result.push_back(s);
	return result;
} // LCOV_EXCL_BR_LINE

// All return values are received into temps first, then assigned left to
// right through the single-target assignment paths, so `f() -> (i, arr[i])`
// indexes with the new i. The block limits the temps to this statement and
// releases a returned struct once it has been copied into its target.
json PlnSemanticAnalyzer::sa_tapple_assign(const json& stmt)
{
	const json& targets = stmt["targets"];
	json callExpr = tappleCallExpr(stmt);
	const json& rets = findMultiRetFunc(stmt, callExpr, targets.size())["rets"];

	json decl = {{"stmt-type", "tapple-decl"}, {"vars", json::array()}, {"value", callExpr},
	             {"loc", stmt["loc"]}}; // LCOV_EXCL_BR_LINE
	vector<string> temps;
	for (auto& r : rets) {
		temps.push_back("__tap_" + to_string(tempVarCounter_++));
		decl["vars"].push_back({{"var-name", temps.back()}, {"var-type", r["var-type"]}}); // LCOV_EXCL_BR_LINE
	}
	json body = json::array({decl}); // LCOV_EXCL_BR_LINE

	for (size_t i = 0; i < targets.size(); i++) {
		const json& t = targets[i];
		json value = {{"expr-type", "id"}, {"name", temps[i]}, {"loc", t["loc"]}}; // LCOV_EXCL_BR_LINE
		string et = t["expr-type"];
		if (et == "id")
			body.push_back({{"stmt-type", "assign"}, {"name", t["name"]}, {"value", value}, {"loc", t["loc"]}}); // LCOV_EXCL_BR_LINE
		else if (et == "arr-index")
			body.push_back({{"stmt-type", "arr-assign"}, {"target", t}, {"value", value}, {"loc", t["loc"]}}); // LCOV_EXCL_BR_LINE
		else
			body.push_back({{"stmt-type", "field-assign"}, {"object", t["object"]}, // LCOV_EXCL_BR_LINE
			                {"field", t["field"]}, {"value", value}, {"loc", t["loc"]}});
	}
	return sa_block({{"stmt-type", "block"}, {"body", body}}); // LCOV_EXCL_BR_LINE
} // LCOV_EXCL_BR_LINE

json PlnSemanticAnalyzer::sa_field_assign(const json& stmt, json* litAddr)
{
	FieldChain chain = resolveObjectChain(stmt["object"], /*forWrite=*/true);
	string fn = stmt["field"].get<string>();
	const FieldLayout* it = &findFieldOrExit(chain.structName, fn, stmt, true);
	bool transfer = stmt.value("ownership-transfer", false);
	if (transfer && it->typeKind != "owned") {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_TransferToNonOwnedField, fn) << endl;
		exit(1);
	}
	json fieldType = fieldValueType(*it);
	if (isAggregateLit(stmt["value"]) && !transfer
	    && (isVarrStorage(fieldType) || isDictStorage(fieldType) || assignsByFunc(withoutArrPermission(fieldType))))
		return sa_lit_assign_via_temp(stmt, fieldType, litAddr);
	if (isAggregateLit(stmt["value"]) && (isStructPntr(fieldType) || isArrLevel(fieldType))) {
		json fieldAst = {{"expr-type", "field-access"}, {"object", stmt["object"]}, {"field", fn}}; // LCOV_EXCL_BR_LINE
		json lit = sa_aggregate_lit_assign(stmt, fieldAst, fieldType, litAddr);
		return litAddr ? lit : json::array({move(lit)}); // LCOV_EXCL_BR_LINE
	}
	const PlnType* toType = registry_.fromJson(fieldType);
	json value = transfer ? sa_move_source(stmt["value"], toType)
	                      : typedNull(sa_expression_for(literalConversion(stmt["value"], fieldType), fieldType), fieldType); // LCOV_EXCL_BR_LINE
	if (!transfer && it->typeKind != "raw-ptr" && isCopiedByValue(fieldType))
		return json::array({makeCopyStmt(stmt, makeFieldAccess(chain, *it), value, CopyInto::Existing)}); // LCOV_EXCL_BR_LINE
	if (it->typeKind == "raw-ptr" && fieldType.contains("arr-size"))
		checkArrBorrowBinding(stmt, stmt["value"], value, fieldType);
	value = convertForBinding(stmt, value, toType, fieldType);
	checkElemShape(stmt, value, fieldType);
	checkBorrowSource(stmt, value, fieldType);
	checkPtrPermission(stmt, value["value-type"], fieldType);
	checkRangeBorrowBinding(stmt, stmt["value"], fieldType);
	if (!transfer)
		return json::array({makeFieldAssign(chain, *it, move(value))}); // LCOV_EXCL_BR_LINE
	// The field owns what it held, so '->>' releases that before taking over.
	json result = transferStmts(stmt, makeOwnedValueFreeStmt(makeFieldAccess(chain, *it)),
	                            makeFieldAssign(chain, *it, value), value);
	if (isArrLevel(fieldType)) checkMoveShape(stmt, value, fieldType, /*inherits=*/false);
	return result;
} // LCOV_EXCL_BR_LINE

json PlnSemanticAnalyzer::makeFieldAssign(const FieldChain& chain, const FieldLayout& field, json value)
{
	// LCOV_EXCL_BR_START
	int off = chain.offset + field.offset;
	json fieldType = fieldValueType(field);
	if (field.bitWidth) checkBitFieldLiteral(field, value);
	json out = !chain.isPointerBased
		? json{{"stmt-type","field-assign"},{"var",chain.varName}}
		: json{{"stmt-type","field-assign"},{"ptr-expr",chain.ptrExpr}};
	out["offset"] = off;
	out["value-type"] = move(fieldType);
	out["value"] = move(value);
	if (field.bitWidth) out["bits"] = {{"shift", field.bitShift}, {"width", field.bitWidth}};
	return out;
	// LCOV_EXCL_BR_STOP
} // LCOV_EXCL_BR_LINE
