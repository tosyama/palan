/// Palan Semantic Analyzer — calls and callee lookup
///
/// @file PlnSaCall.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <iostream>
#include <algorithm>
#include <cstdint>
#include "PlnSemanticAnalyzer.h"
#include "PlnSaMessage.h"
#include "PlnSaInternal.h"

// LCOV_EXCL_BR_START
static json makeTempExpr(json body, json value, json release)
{
	json out = {{"expr-type","temp"},{"body",move(body)},{"release",move(release)}};
	if (value.contains("value-type")) out["value-type"] = value["value-type"];
	if (value.contains("category")) out["category"] = value["category"];
	if (value.value("resized", false)) out["resized"] = true;
	out["value"] = move(value);
	return out;
}
// LCOV_EXCL_BR_STOP

// A function pointer type names no parameters.
static string paramLabel(const json& param, size_t argIdx)
{
	string name = param.value("name", "");
	return name.empty() ? "#" + to_string(argIdx + 1) : name;
}

static bool givenIn(const json& arg)
{
	return arg.value("expr-type", "") == "transfer";
}

json PlnSemanticAnalyzer::sa_arg_expression(const json& ast, const json* toType)
{
	bool saved = transferAllowed_;
	transferAllowed_ = givenIn(ast);
	json out = toType ? sa_expression_for(ast, *toType) : sa_expression(ast);
	transferAllowed_ = saved;
	return out;
}

json PlnSemanticAnalyzer::sa_expr_transfer(const json& expr)
{
	if (!transferAllowed_) {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_TransferNotArg) << endl;
		exit(1);
	}
	transferAllowed_ = false;
	json body = json::array(), reset = json::array();
	json given = giveValue(expr["value"], expr, expr.value("op", ">>"), body, reset);
	transferAllowed_ = true;
	if (body.empty()) return given;
	json temp = makeTempExpr(move(body), move(given), json::array());
	// Later arguments may still borrow from the owner, so it is reset just
	// before the call, after all of them.
	if (!reset.empty()) temp["reset"] = move(reset);
	return temp;
}

// The registered definition of the function `funcDef` defines, among those
// sharing its name.
const json* PlnSemanticAnalyzer::findPlnFuncDef(const json& funcDef) const
{
	const string& name = funcDef["name"].get<string>();
	// The definition is always registered, in the scope being analyzed.
	for (auto it = plnFuncScopes.rbegin(); it != plnFuncScopes.rend(); ++it) { // LCOV_EXCL_BR_LINE
		auto f = it->find(name);
		if (f == it->end()) continue; // LCOV_EXCL_BR_LINE
		for (auto& def : f->second) // LCOV_EXCL_BR_LINE
			if (def.value("loc", json()) == funcDef.value("loc", json())) return &def;
	}
	BOOST_ASSERT(false); // LCOV_EXCL_LINE
	return nullptr; // LCOV_EXCL_LINE
}

// One function brought in more than once (a header included again, a module
// imported again, a C-named function and its C declaration) has one key.
string PlnSemanticAnalyzer::calleeKey(const json& def, bool isC) const
{
	if (isC) return "c:" + cLinkName(def).get<string>();
	if (def.value("c-export", false)) return "c:" + def["name"].get<string>();
	string key = def.value("_module", moduleId_) + ":" + def["name"].get<string>();
	for (auto& p : def.value("parameters", json::array()))
		key += "|" + p["var-type"].dump();
	return key;
}

// The functions `alias.name` names in the innermost scope that imports one.
vector<Callee> PlnSemanticAnalyzer::findCalleesByAlias(const string& alias, const string& name) const
{
	vector<Callee> cands;
	set<string> seen;
	for (auto it = importScopes.rbegin(); it != importScopes.rend(); ++it) {
		auto alias_it = it->find(alias);
		if (alias_it == it->end()) continue;
		auto f_it = alias_it->second.find(name);
		if (f_it == alias_it->second.end()) continue;
		for (auto& def : f_it->second) {
			bool isC = def.value("_c-func", false);
			if (seen.insert(calleeKey(def, isC)).second) cands.push_back({&def, isC});
		}
		break;
	}
	return cands;
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

// The functions a call by `name` can mean: C functions, Palan functions,
// unqualified imports and `[+]T` built-ins alike, each once.
vector<Callee> PlnSemanticAnalyzer::findCallees(const string& name) const
{
	vector<Callee> cands;
	map<string, size_t> seen;
	// A C-named function takes the place of its C declaration, keeping its own types.
	auto add = [&](const json& def, bool isC) {
		auto [it, added] = seen.emplace(calleeKey(def, isC), cands.size());
		if (added) cands.push_back({&def, isC});
		else if (!isC && cands[it->second].isC) cands[it->second] = {&def, false};
	};
	for (auto it = cFuncScopes.rbegin(); it != cFuncScopes.rend(); ++it) {
		auto f = it->find(name);
		if (f != it->end()) add(f->second, true);
	}
	for (auto it = plnFuncScopes.rbegin(); it != plnFuncScopes.rend(); ++it) {
		auto f = it->find(name);
		if (f != it->end())
			for (auto& def : f->second) add(def, false);
	}
	for (auto it = importScopes.rbegin(); it != importScopes.rend(); ++it) {
		auto alias_it = it->find("");
		if (alias_it == it->end()) continue;
		auto f = alias_it->second.find(name);
		if (f != alias_it->second.end())
			for (auto& def : f->second) add(def, def.value("_c-func", false));
	}
	auto b = builtins_.find(name);
	if (b != builtins_.end())
		for (auto& def : b->second) add(def, false);
	return cands;
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

void PlnSemanticAnalyzer::requireAlias(const json& locNode, const string& alias) const
{
	for (auto it = importScopes.rbegin(); it != importScopes.rend(); ++it)
		if (it->count(alias)) return;
	cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_UnknownAlias, alias) << endl;
	exit(1);
}

// C's `...`, whose arguments take no parameter's type.
static bool isCVariadicSig(const json& funcParams)
{
	for (auto& p : funcParams)
		if (p.value("name", "") == "...") return true;
	return false;
}

// The parameter an argument at or past a `[...]E` parameter is given as.
static json variadicElemParam(const json& variadic)
{
	return {{"name", variadic.value("name", "")}, {"var-type", variadic["var-type"]["base-type"]}}; // LCOV_EXCL_BR_LINE
} // LCOV_EXCL_BR_LINE

static bool isAggregateLitArg(const json& arg)
{
	return isAggregateLit(arg)
		|| (arg.value("expr-type", "") == "addr-of" && isAggregateLit(arg["object"])); // LCOV_EXCL_BR_LINE
}

static bool isScalar(const PlnType* t)
{
	return t->kind == PlnType::Kind::Prim || t->kind == PlnType::Kind::Enum;
}

static bool firstParamMutable(const json& def)
{
	const json& params = def["parameters"];
	return !params.empty() && params[0]["var-type"].value("mutable", false);
}

// The type registry does not tell a read-only pointer from a writable one at
// any level, down through the pointers and the elements of a `[+]T` or `[""]T`.
ArgFit PlnSemanticAnalyzer::typeFit(const json& argVT, const json& paramVT)
{
	if (registry_.fromJson(argVT) != registry_.fromJson(paramVT)) return ArgFit::Viable;
	ArgFit fit = ArgFit::Exact;
	const json* a = &argVT;
	const json* p = &paramVT;
	for (bool outermost = true; ; outermost = false) {
		string kind = p->value("type-kind", "");
		if (kind != "pntr" && kind != "varr" && kind != "strdict") return fit;
		bool aw = a->value("mutable", true), pw = p->value("mutable", true);
		if (kind == "pntr" && aw != pw) {
			if (!outermost || !aw) return ArgFit::Viable;
			fit = ArgFit::Narrowed;
		}
		a = &(*a)["base-type"];
		p = &(*p)["base-type"];
	}
}

// How an argument fits a parameter. A pending argument is judged by what it
// can become once the parameter is known: a literal fits any primitive and is
// exact only for its default type, an aggregate literal fits a parameter it
// can be made into and is never exact, and a function name is exact when one of its
// functions has the parameter's type.
ArgFit PlnSemanticAnalyzer::argFit(const CallArg& arg, const json& param, size_t argIdx,
                                   bool isCFunc, const string& funcName)
{
	const json& paramVT = param["var-type"];
	// A literal never fits a struct parameter as it is.
	if (literalConversion(arg.ast, paramVT) != arg.ast) return ArgFit::Converted;
	auto fitOf = [&](const json& ast, const json& sa0) {
		json sa = takesVarrData(sa0["value-type"], &paramVT, isCFunc) ? varrData(sa0) : sa0;
		if (callArgMismatch(ast, isCFunc ? sa : typedNull(sa, paramVT), param, argIdx, isCFunc, funcName)) return ArgFit::None;
		return typeFit(sa["value-type"], paramVT);
	};
	if (!arg.pending) return fitOf(arg.ast, arg.sa);
	if (isAggregateLitArg(arg.ast)) {
		// `{x: 1}` is read as a dictionary only when x is a string constant, and
		// a string key only as a dictionary; otherwise as a struct.
		const json& lit = isAggregateLit(arg.ast) ? arg.ast : arg.ast["object"];
		const json& items = lit["items"];
		bool dictLit = lit["expr-type"] == "dict-lit";
		auto constKey = [&](const json& item) {
			return item.contains("key")
				|| foldConstExpr({{"expr-type", "id"}, {"name", item["name"]}}).value("expr-type", "") == "lit-str"; // LCOV_EXCL_BR_LINE
		}; // LCOV_EXCL_BR_LINE
		if (isDictPntr(paramVT))
			return dictLit && all_of(items.begin(), items.end(), constKey) ? ArgFit::Viable : ArgFit::None;
		if (dictLit && (!isStructPntr(paramVT)
		                || any_of(items.begin(), items.end(), [](const json& item) { return item.contains("key"); })))
			return ArgFit::None;
		// What sa_lit_arg and ownedBlockArg make the literal into: a struct, a
		// sized array or a block the callee owns, never an unsized borrow.
		bool takes = !isCFunc && (isStructPntr(paramVT) || paramVT.contains("arr-size") || isOwnedBlock(paramVT));
		return takes ? ArgFit::Viable : ArgFit::None;
	}
	if (namesOverloadedFunc(arg.ast)) {
		bool fits = isFuncPtr(paramVT) && !funcsOfType(arg.ast["name"].get<string>(), paramVT).empty();
		return fits ? ArgFit::Exact : ArgFit::None;
	}
	if (!arg.chained || !arg.probe.contains("expr-type")) {
		const PlnType* t = registry_.fromJson(paramVT);
		if (t->kind != PlnType::Kind::Prim) return ArgFit::None;
		return !arg.probe.is_null() && typeFit(arg.probe, paramVT) == ArgFit::Exact ? ArgFit::Exact : ArgFit::Viable;
	}
	// A chained value: '@v' when the parameter would take it so.
	const json& vt = arg.probe["value-type"];
	json form = chainArg(arg.ast, &paramVT, isCFunc, &vt);
	if (form.value("expr-type", "") != "addr-of" || arg.ast.value("expr-type", "") == "addr-of") // LCOV_EXCL_BR_LINE
		return fitOf(arg.ast, arg.probe);
	const json& base = paramVT["base-type"];
	bool match = isArrLevel(vt) ? arrShapeMatch(vt, paramVT)
	           : isStructStorage(vt) ? base.value("type-kind", "") == "struct" // LCOV_EXCL_BR_LINE
	                                   && base["type-name"] == vt["base-type"]["type-name"]
	           : isVarrStorage(vt) || isDictStorage(vt) ? base == vt["base-type"]
	           : base == vt;
	return match ? ArgFit::Exact : ArgFit::None;
}

static json withoutPermission(json t)
{
	string kind = t.value("type-kind", "");
	if (kind != "pntr" && kind != "varr" && kind != "strdict") return t;
	t.erase("mutable");
	t["base-type"] = withoutPermission(move(t["base-type"]));
	return t;
} // LCOV_EXCL_BR_LINE

// Whether an argument has a parameter's type also in what the type registry
// does not hold: lengths, rows, elements' owners and functions' signatures.
bool PlnSemanticAnalyzer::sameShape(const CallArg& arg, const json& paramVT, bool isCFunc)
{
	if (arg.pending && namesOverloadedFunc(arg.ast)) {
		json fp;
		for (auto& c : funcsOfType(arg.ast["name"].get<string>(), paramVT))
			if (funcRefType(c, fp).empty() && fp == paramVT) return true; // LCOV_EXCL_BR_LINE
		return false;
	}
	const json& sa = arg.pending ? arg.probe : arg.sa;
	if (!sa.contains("value-type")) return true;  // typed by the parameter
	const json vt = takesVarrData(sa["value-type"], &paramVT, isCFunc) ? varrData(sa)["value-type"] : sa["value-type"]; // LCOV_EXCL_BR_LINE
	return withoutPermission(vt) == withoutPermission(paramVT);
} // LCOV_EXCL_BR_LINE

// Parameters with defaults come last, so the first one ends those a call must give.
static size_t requiredArgCount(const json& params, size_t fixedCount)
{
	for (size_t i = 0; i < fixedCount; i++)
		if (params[i].contains("default")) return i;
	return fixedCount;
}

string PlnSemanticAnalyzer::argTypesText(const vector<CallArg>& args)
{
	string text;
	for (auto& a : args) {
		if (!text.empty()) text += ", "; // LCOV_EXCL_BR_LINE
		const json& vt = !a.pending ? a.sa["value-type"]
		               : a.chained && a.probe.contains("value-type") ? a.probe["value-type"]
		               : a.probe;
		if (a.spread) text += "...";
		text += vt.is_object() ? typeDisplayName(vt) // LCOV_EXCL_BR_LINE
		      : namesOverloadedFunc(a.ast) ? string("function")
		      : string("literal");
	}
	return text;
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

// A default as its function's reader would write it.
string PlnSemanticAnalyzer::defaultText(const json& d) const
{
	const json& lit = d["expr-type"] == "convert" ? d["src"] : d;
	if (lit["expr-type"] == "lit-str") return "\"" + lit["value"].get<string>() + "\"";
	string value = lit["value"];
	const json& vt = lit.value("value-type", json::object());
	if (vt.value("type-name", "") == "bool") return value == "0" ? "false" : "true";
	auto e = vt.contains("enum") ? enumDefs_.find(vt["enum"].get<string>()) : enumDefs_.end();
	if (e != enumDefs_.end())
		for (auto& [name, v] : e->second.values)
			if (to_string(v) == value) return typeDisplayName(vt) + "." + name;
	return value;
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

string PlnSemanticAnalyzer::calleesText(const string& name, const vector<Callee>& cands) const
{
	string text;
	for (auto& c : cands) {
		if (!text.empty()) text += ", "; // LCOV_EXCL_BR_LINE
		text += name + "(";
		const json* written = c.def->contains("_written-params") ? &(*c.def)["_written-params"] : nullptr;
		size_t i = 0;
		for (auto& p : (*c.def)["parameters"]) {
			if (i) text += ", ";
			text += written ? (*written)[i].get<string>() // LCOV_EXCL_BR_LINE
			      : p.value("name", "") == "..." ? string("...") : typeDisplayName(p["var-type"]); // LCOV_EXCL_BR_LINE
			i++;
			if (p.contains("default")) text += " = " + defaultText(p["default"]);
		}
		text += ")";
	}
	return text;
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

// Among several functions of one name, the one every argument fits; when more
// than one fits, the one every argument matches exactly, or else the one every
// argument matches but for a writable borrow given as read-only; among several
// of these, the one whose types the arguments have also in shape. Those needing
// a literal converted are only looked at when no other fits, and are
// ranked the same way among themselves, a converted argument as exact.
CalleeChoice PlnSemanticAnalyzer::chooseCallee(const string& name, const vector<Callee>& cands,
                                               const vector<CallArg>& args)
{
	struct Fitting {
		vector<Callee> viable, exact, narrowed;
		bool unranked = false;
	} plain, converting;
	bool spread = !args.empty() && args.back().spread;
	for (auto& c : cands) {
		const json& params = (*c.def)["parameters"];
		const json* palanVariadic = palanVariadicParam(params);
		bool variadic = palanVariadic || isCVariadicSig(params);
		size_t fixedCount = params.size() - (variadic ? 1 : 0);
		size_t required = requiredArgCount(params, fixedCount);
		if (args.size() < required || (!variadic && args.size() > fixedCount)) continue;
		if (spread && (!palanVariadic || args.size() != fixedCount + 1)) continue;
		json elemParam = palanVariadic ? variadicElemParam(*palanVariadic) : json();
		size_t checked = spread ? fixedCount : palanVariadic ? args.size() : min(fixedCount, args.size());
		bool fits = true, converts = false;
		ArgFit worst = args.size() == fixedCount ? ArgFit::Exact : ArgFit::Viable;
		for (size_t i = 0; i < checked && fits; i++) {
			ArgFit f = argFit(args[i], i < fixedCount ? params[i] : elemParam, i, c.isC, name);
			fits = f != ArgFit::None;
			converts = converts || f == ArgFit::Converted;
			worst = min(worst, f == ArgFit::Converted ? ArgFit::Exact : f);
		}
		if (fits && spread)
			fits = !spreadMismatch(args.back().sa["value-type"], elemParam["var-type"]);
		if (!fits) continue;
		Fitting& to = converts ? converting : plain;
		to.unranked = to.unranked || args.size() < fixedCount || palanVariadic;
		to.viable.push_back(c);
		if (worst == ArgFit::Exact) to.exact.push_back(c);
		if (worst == ArgFit::Narrowed) to.narrowed.push_back(c);
	}
	// Converting is the last resort, so that adding a conversion function
	// changes no call that resolves without one.
	const Fitting& fit = plain.viable.empty() ? converting : plain;
	if (fit.viable.size() == 1) return {fit.viable[0], {}};
	if (fit.viable.empty()) return {};
	// A candidate fitting through its defaults or its variadic parameter is not
	// ranked against the others, so that a reader follows no further rule to
	// tell which one is called.
	if (fit.unranked) return {nullopt, fit.viable};
	const vector<Callee>& best = !fit.exact.empty() ? fit.exact : fit.narrowed;
	if (best.empty()) return {nullopt, fit.viable};
	if (best.size() == 1) return {best[0], {}};
	vector<Callee> shaped;
	for (auto& c : best) {
		const json& params = (*c.def)["parameters"];
		bool same = true;
		for (size_t i = 0; i < args.size() && same; i++)
			same = sameShape(args[i], params[i]["var-type"], c.isC);
		if (same) shaped.push_back(c);
	}
	if (shaped.size() == 1) return {shaped[0], {}};
	return {nullopt, best};
}

Callee PlnSemanticAnalyzer::selectCallee(const json& locNode, const string& name,
                                         const vector<Callee>& cands, const vector<CallArg>& args)
{
	CalleeChoice choice = chooseCallee(name, cands, args);
	if (choice.chosen) return *choice.chosen;
	if (choice.tied.empty()) {
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_NoMatchingFunc,
			name, argTypesText(args), calleesText(name, cands)) << endl;
		exit(1);
	}
	cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_AmbiguousCall,
		name, calleesText(name, choice.tied)) << endl;
	exit(1);
}

// The functions a call marked by normalizeMethodCall (a method-form call) or
// sa_tapple_decl (receiving several values) can mean.
vector<Callee> PlnSemanticAnalyzer::usableCallees(const json& expr, const vector<Callee>& cands) const
{
	vector<Callee> usable;
	for (auto& c : cands)
		if ((!expr.value("method-form", false) || firstParamMutable(*c.def))
		    && (!expr.contains("multi-ret")
		        || (*c.def).value("rets", json::array()).size() == expr["multi-ret"].get<size_t>())) // LCOV_EXCL_BR_LINE
			usable.push_back(c);
	return usable;
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

static json writableBorrow(const json& obj)
{
	json b = {{"expr-type", "addr-of"}, {"mutable", true}, {"object", obj}}; // LCOV_EXCL_BR_LINE
	if (obj.contains("loc")) b["loc"] = obj["loc"];
	return b;
} // LCOV_EXCL_BR_LINE

// A chained value given to one of several functions: whether it is borrowed
// ('@v') depends on the function chosen, so bindCallArgs decides it.
static json chainedValue(const json& value)
{
	return {{"expr-type", "chained-value"}, {"value", value}}; // LCOV_EXCL_BR_LINE
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

// An arithmetic operator with a struct operand: a call of the functions the
// operator names, given the operands as values chained into it, so that each
// function chosen decides whether it borrows or copies one. An operand
// already analyzed (in `probes`) is not analyzed again: a nested operator
// would otherwise be analyzed once per enclosing one.
json PlnSemanticAnalyzer::sa_operator_call(const json& expr, const string& op,
                                           const vector<const json*>& operands, vector<json> probes)
{
	json call = {{"expr-type", "call"}, {"name", op}, {"args", json::array()}}; // LCOV_EXCL_BR_LINE
	if (expr.contains("loc")) call["loc"] = expr["loc"];
	vector<CallArg> args;
	for (size_t i = 0; i < operands.size(); i++) {
		CallArg a;
		a.ast = *operands[i];
		a.pending = a.chained = true;
		if (!probes[i].is_null()) a.probe = move(probes[i]);
		else if (!isAggregateLitArg(a.ast)) a.probe = literalDefaultType(a.ast);
		args.push_back(move(a));
	}
	vector<Callee> cands = findCallees(op);
	if (cands.empty()) {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_NoOperatorFunc, op, argTypesText(args)) << endl;
		exit(1);
	}
	Callee callee = selectCallee(call, op, cands, args);
	return makeCallNode(call, callee, op, &args);
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

// `s << x`: a call of the '<<' functions, given the left operand as a
// method-form receiver is, or a '@!T' a call returns, so that `s << a << b`
// chains. The left operand is analyzed once here: analyzing a nested '<<'
// again for each enclosing one would take time exponential in the chain.
json PlnSemanticAnalyzer::sa_expr_append(const json& expr)
{
	const json& left = expr["left"];
	const json& right = expr["right"];
	string let = left.value("expr-type", "");
	bool isLocation = isLocationExpr(left);
	bool isCall = let == "call" || let == "member-call" || let == "chain-call" || let == "call-ptr" || let == "append";
	json leftSa = let == "field-access" ? json() : sa_expr_maybe_void(left);
	json leftVT = leftSa.is_null() ? operandValueType(left) : leftSa.value("value-type", json()); // LCOV_EXCL_BR_LINE
	auto fail = [&](PlnSaMessageCode code, const string& arg = "\x01") {
		cerr << locPrefix(left.contains("loc") ? left : expr) << PlnSaMessage::getMessage(code, arg) << endl;
		exit(1);
	};
	if (leftVT.is_null()) fail(E_AppendLeftOperand);
	if (!isStructPntr(leftVT)) fail(E_AppendNotStruct, typeDisplayName(leftVT));
	if (isStructBorrow(leftVT) && !leftVT["mutable"].get<bool>()) fail(E_AppendReadOnly);

	json recv, probes[2];
	if (isStructBorrow(leftVT) && (isLocation || isCall)) {
		recv = left;
		probes[0] = move(leftSa);
	} else if (isStructStorage(leftVT) && isLocation) {
		recv = writableBorrow(left);
		probes[0] = sa_expression(recv);
	} else {
		fail(E_AppendLeftOperand);
	}
	// As an arithmetic operand: one that takes its type from the parameter is
	// left for the function chosen to type.
	if (!isAggregateLit(right) && foldConstExpr(right)["expr-type"] != "lit-int") { // LCOV_EXCL_BR_LINE
		probes[1] = sa_expression(right);
		if (!isStructPntr(probes[1]["value-type"]) && takesExpectedType(right)) probes[1] = json(); // LCOV_EXCL_BR_LINE
	}
	return sa_operator_call(expr, "<<", {&recv, &right}, {move(probes[0]), move(probes[1])}); // LCOV_EXCL_BR_LINE
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

// A comparison a type defines: '!=' '>' '<=' '>=' are derived from '==' and
// '<'. Where the operands are passed swapped, the left one is evaluated ahead
// into a temp when a call could see the other's evaluation, as a value chained
// to a method-form call is.
json PlnSemanticAnalyzer::sa_compare_call(const json& expr, json probes[2])
{
	const string& op = expr["op"].get<string>();
	bool swap = op == ">" || op == "<=";
	string func = op == "==" || op == "!=" ? "==" : "<";
	const json& left = expr["left"];
	const json& right = expr["right"];
	json call;
	if (swap && (hasCall(left) || hasCall(right))) {
		enterScope();
		json body = json::array();
		json ahead = chainValueAhead(left, nullptr, body);
		json aheadProbe = probes[0].is_null() ? json() : sa_expression(ahead);
		json saCall = sa_operator_call(expr, func, {&right, &ahead}, {move(probes[1]), move(aheadProbe)}); // LCOV_EXCL_BR_LINE
		size_t idx = arrayScopeVars_.size() - 1;
		json release = collectFreeStmts(idx, idx + 1);
		leaveScope();
		call = makeTempExpr(move(body), move(saCall), move(release));
	} else if (swap) {
		call = sa_operator_call(expr, func, {&right, &left}, {move(probes[1]), move(probes[0])}); // LCOV_EXCL_BR_LINE
	} else {
		call = sa_operator_call(expr, func, {&left, &right}, {move(probes[0]), move(probes[1])}); // LCOV_EXCL_BR_LINE
	}
	if (op != "!=" && op != "<=" && op != ">=") return call;
	// LCOV_EXCL_BR_START
	json out = {{"expr-type", "logical-not"}, {"operand", move(call)},
	            {"value-type", {{"type-kind", "prim"}, {"type-name", "bool"}}}};
	if (expr.contains("loc")) out["loc"] = expr["loc"];
	return out;
	// LCOV_EXCL_BR_STOP
} // LCOV_EXCL_BR_LINE

// A call of one of `cands`: a single one is bound as it is, so its own
// diagnostics are what an unfit argument reports.
json PlnSemanticAnalyzer::makeCall(const json& expr, const vector<Callee>& cands, const string& writtenName)
{
	if (cands.size() == 1) return makeCallNode(expr, cands[0], writtenName);
	vector<CallArg> args = analyzeCallArgs(expr["args"]);
	Callee callee = selectCallee(expr, writtenName, cands, args);
	return makeCallNode(expr, callee, writtenName, &args);
}

// A struct returned by a call and given as an argument is held in a temp,
// since the callee may return a borrow of it.
json PlnSemanticAnalyzer::bindCallTemp(json saArg)
{
	// LCOV_EXCL_BR_START
	string name = "__tmp_" + to_string(tempVarCounter_++);
	json storage = saArg["value-type"];
	storage.erase("mutable");
	json valueType = saArg["value-type"];
	json value = {{"expr-type","id"},{"name",name},{"var-type",storage},{"value-type",move(valueType)}};
	if (isStructRetCall(saArg)) {
		json body = json::array(), release = json::array();
		json fini = frameStructRelease(name, storage);
		if (!fini.is_null()) release.push_back(move(fini));
		// The struct may hold a borrow of a chained call's temps, so they are released after it.
		if (saArg["expr-type"] == "temp") {
			body = move(saArg["body"]);
			for (auto& s : saArg["release"]) release.push_back(move(s));
			json call = move(saArg["value"]);
			saArg = move(call);
		}
		for (auto& s : declareStructRet(name, storage, move(saArg), VarStorage::Frame)) body.push_back(move(s));
		return makeTempExpr(move(body), move(value), move(release));
	}
	json decl = {{"stmt-type","var-decl"},
	             {"vars",json::array({{{"name",name},{"var-type",storage},{"init",move(saArg)}}})}};
	return makeTempExpr(json::array({move(decl)}), move(value),
	                    json::array({makeOwnedFreeStmt(name, storage)}));
	// LCOV_EXCL_BR_STOP
} // LCOV_EXCL_BR_LINE

// '@f()'/'@!f()' as a call argument: the returned struct is borrowed for the
// call, held in a temp (bindCallTemp).
json PlnSemanticAnalyzer::sa_call_borrow_arg(const json& arg)
{
	json saArg = sa_expression(arg["object"]);
	if (!isExpiringOwned(saArg)) {
		cerr << locPrefix(arg) << PlnSaMessage::getMessage(E_AddrOfNotAddressable) << endl;
		exit(1);
	}
	return borrowExpiring(move(saArg), arg["mutable"]);
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

json PlnSemanticAnalyzer::borrowExpiring(json saArg, bool isMutable)
{
	json& vt = saArg["value-type"];
	if (isArrLevel(vt)) vt = withArrPermission(vt, isMutable);
	else vt["mutable"] = isMutable;
	return saArg;
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

// A literal argument ('[..]', '{..}', or one under '@'/'@!') becomes a temp of
// the parameter's struct or array type, initialized by body; the argument is
// then analyzed as the temp written in its place ('@__lit_N'), returned with
// argAst set to that form. A null paramVT is a C function's parameter.
json PlnSemanticAnalyzer::sa_lit_arg(const json& arg, const json* paramVT, const string& paramName,
                                     const string& funcName, json& argAst, json& body, json& release)
{
	bool isAddrOf = arg.value("expr-type", "") == "addr-of";
	const json& lit = isAddrOf ? arg["object"] : arg;
	auto rejectParamType = [&](PlnSaMessageCode code) __attribute__((noreturn)) {
		cerr << locPrefix(lit) << PlnSaMessage::getMessage(code, paramName, typeDisplayName(*paramVT)) << endl;
		exit(1);
	};
	auto rejectContext = [&]() {
		if (!paramVT) {
			cerr << locPrefix(lit) << PlnSaMessage::getMessage(E_LitArgCFunc, funcName) << endl;
			exit(1);
		}
		rejectParamType(lit["expr-type"] == "arr-lit" ? E_ArrLitArgType : E_DictLitArgType);
	};
	if (!paramVT)
		rejectContext();
	bool isStruct = isStructPntr(*paramVT);
	bool isDict = isDictPntr(*paramVT);
	if (isDict && lit["expr-type"] != "dict-lit")
		rejectParamType(E_ArrLitArgType);
	json declType = isStruct
		? json{{"type-kind","prim"},{"type-name",(*paramVT)["base-type"]["type-name"]}} // LCOV_EXCL_BR_LINE
		: paramVT->contains("arr-size") || isDict ? astTypeOf(*paramVT) : json();
	if (declType.is_null()) {
		if (!paramVT->contains("arr-size"))
			rejectContext();
		cerr << locPrefix(lit) << PlnSaMessage::getMessage(E_ArrLitElemType, paramName) << endl;
		exit(1);
	}

	enterScope();
	// LCOV_EXCL_BR_START
	string name = "__lit_" + to_string(tempVarCounter_++);
	json declVar = {{"name",name},{"var-type",declType}};
	if (isDict) declVar["init"] = lit;
	json decl = {{"stmt-type","var-decl"},{"loc",lit.value("loc", json::array())},
		{"vars",json::array({move(declVar)})}};
	body = sa_resolved_var_decl(decl);
	json target = {{"expr-type","id"},{"name",name}};
	const json varType = *findVar(name);
	if (isStruct) {
		const StructDef& def = requireCompleteStruct(varType["base-type"]["type-name"].get<string>(), lit);
		emitStructLitAssigns(target, def, sa_struct_lit(lit, def), body);
	} else if (!isDict) {
		if (lit["expr-type"] != "arr-lit")
			rejectParamType(E_DictLitArgType);
		ArrLitShape shape = arrLitShape(lit, paramName, varType);
		json values = sa_arr_lit_values(lit, shape, paramName);
		checkArrLitCount(lit, lit["items"], paramName, shape);
		emitArrLitAssigns(target, shape, values, body);
	}
	argAst = isAddrOf ? json{{"expr-type","addr-of"},{"object",target},{"mutable",arg["mutable"]},{"loc",arg.value("loc", json::array())}}
	                  : target;
	json saArg = sa_expression(argAst);
	size_t idx = arrayScopeVars_.size() - 1;
	release = collectFreeStmts(idx, idx + 1);
	// LCOV_EXCL_BR_STOP
	leaveScope();
	return saArg;
} // LCOV_EXCL_BR_LINE

// The call node for `expr` calling `callee`, as written by `writtenName`.
json PlnSemanticAnalyzer::makeCallNode(const json& expr, const Callee& callee, const string& writtenName,
                                       vector<CallArg>* analyzedArgs)
{
	const json& def = *callee.def;
	json sa_expr = expr;
	sa_expr["expr-type"] = "call";
	sa_expr.erase("object");
	sa_expr.erase("method");
	sa_expr.erase("method-form");
	sa_expr.erase("multi-ret");
	if (callee.isC) {
		requireSupportedCFuncSig(def, writtenName, expr);
		sa_expr["func-type"] = "c";
		sa_expr["name"] = cLinkName(def);
		if (def.contains("ret-type") && def["ret-type"].value("type-name", "") != "void")
			sa_expr["value-type"] = def["ret-type"];
	} else {
		sa_expr["func-type"] = def["func-type"];
		sa_expr["name"] = def.contains("symbol") ? def["symbol"] : json(writtenName); // a syscall has none
		if (def.contains("ret-type"))
			sa_expr["value-type"] = def["ret-type"];
		if (def.value("func-type", "") == "syscall")
			sa_expr["syscall-number"] = def["syscall-number"];
	}
	if (sa_expr.contains("value-type") && sa_expr["value-type"].value("type-kind","") == "pntr")
		sa_expr["category"] = "expiring";
	markStructRet(sa_expr, expr);
	sa_expr["args"] = analyzedArgs
		? bindCallArgs(expr, *analyzedArgs, def["parameters"], callee.isC, writtenName)
		: saCallArgs(expr, expr["args"], def["parameters"], callee.isC, writtenName);
	if (def.value("inline", false)) return arrLenOf(sa_expr["args"][0], expr);
	// A builtin's generated function also takes the lengths the element's type gives.
	if (def.contains("len-args"))
		for (auto& len : def["len-args"]) sa_expr["args"].push_back(len);
	return sa_expr;
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

// The `@func` type a call through a pointer to `def` would have, or why
// there can be none.
string PlnSemanticAnalyzer::funcRefType(const Callee& callee, json& funcPtr) const
{
	const json& def = *callee.def;
	json func = {{"type-kind","func"},{"parameters",json::array()}}; // LCOV_EXCL_BR_LINE
	if (def.value("func-type", "") == "syscall") return "it is a syscall";
	if (def.contains("rets") && def["rets"].size() > 1) return "it returns multiple values";
	// LCOV_EXCL_BR_START
	func["ret-type"] = def.contains("ret-type") ? def["ret-type"]
	                 : def.contains("rets") ? def["rets"][0]["var-type"]
	                 : json{{"type-kind","prim"},{"type-name","void"}};
	// LCOV_EXCL_BR_STOP
	for (auto& p : def["parameters"]) {
		if (p.value("name", "") == "...") return "it takes variable arguments";
		func["parameters"].push_back({{"var-type", p["var-type"]}}); // LCOV_EXCL_BR_LINE
	}
	// A C function's by-value struct parameter has an owned struct's shape but
	// is passed in registers or on the stack, which no function pointer does.
	for (auto& p : func["parameters"])
		if (!isFuncSigType(p["var-type"]) || (callee.isC && isStructStorage(p["var-type"])))
			return "its parameter type '" + typeDisplayName(p["var-type"]) + "' cannot be passed through one";
	const json& ret = func["ret-type"];
	if (ret.value("type-name", "") != "void" && !isFuncSigType(ret)) // LCOV_EXCL_BR_LINE
		return "its return type '" + typeDisplayName(ret) + "' cannot be returned through one";
	funcPtr = {{"type-kind","pntr"},{"mutable",false},{"base-type",move(func)}}; // LCOV_EXCL_BR_LINE
	return "";
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

// A name that is not a variable but several functions: as a value, it needs
// the type it is given to.
bool PlnSemanticAnalyzer::namesOverloadedFunc(const json& expr) const
{
	if (expr.value("expr-type", "") != "id") return false;
	const string& name = expr["name"].get<string>();
	return !findVar(name) && !findCGlobal(name) && findCallees(name).size() > 1; // LCOV_EXCL_BR_LINE
}

// The functions of a name that fit a function pointer type, or the one whose
// own type it is. Those differing from it only in permission, owner or shape
// go to `mismatched`.
vector<Callee> PlnSemanticAnalyzer::funcsOfType(const string& name, const json& funcPtr, vector<Callee>* mismatched)
{
	const PlnType* want = registry_.fromJson(funcPtr);
	vector<Callee> fitting, exact;
	for (auto& c : findCallees(name)) {
		json fp;
		if (!funcRefType(c, fp).empty() || registry_.fromJson(fp) != want) continue; // LCOV_EXCL_BR_LINE
		if (ptrPermissionOk(fp, funcPtr)) fitting.push_back(c);
		else if (mismatched) mismatched->push_back(c);
		if (fp == funcPtr) exact.push_back(c);
	}
	return exact.size() == 1 ? exact : fitting;
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

// A function named as a value: its address, typed as the `@func` it can be
// called through. Null if `expr` names no function. Of several functions of
// the name, the one fitting toType.
json PlnSemanticAnalyzer::sa_func_ref(const json& expr, const json* toType)
{
	const string& name = expr["name"].get<string>();
	vector<Callee> cands = findCallees(name);
	if (cands.empty()) return json();
	Callee callee = cands[0];
	json funcPtr;
	if (cands.size() > 1) {
		vector<Callee> typed, mismatched;
		if (toType) typed = funcsOfType(name, *toType, &mismatched);
		// A single one is bound as it is, so that its mismatch is what is reported.
		if (typed.empty() && mismatched.size() == 1) typed = mismatched;
		if (typed.size() > 1) {
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_AmbiguousCall,
				name, calleesText(name, typed)) << endl;
			exit(1);
		}
		if (typed.empty()) {
			cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_FuncNameNeedsType,
				name, calleesText(name, cands)) << endl;
			exit(1);
		}
		callee = typed[0];
	}
	if (callee.isC) requireSupportedCFuncSig(*callee.def, name, expr);
	string reason = funcRefType(callee, funcPtr);
	if (!reason.empty()) {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_FuncNotValue, name, reason) << endl;
		exit(1);
	}
	// LCOV_EXCL_BR_START
	json out = {{"expr-type", "func-ref"}, {"name", callee.isC ? cLinkName(*callee.def) : (*callee.def)["symbol"]},
	            {"func-type", callee.isC ? "c" : "palan"}, {"value-type", move(funcPtr)}};
	// LCOV_EXCL_BR_STOP
	if (expr.contains("loc")) out["loc"] = expr["loc"];
	return out;
} // LCOV_EXCL_BR_LINE

// The function pointer variable a call names, or null. A function of the same
// name makes the call ambiguous.
const json* PlnSemanticAnalyzer::findFuncPtrVar(const json& call) const
{
	const string& name = call["name"].get<string>();
	const json* vt = findVar(name);
	if (!vt) {
		const json* g = findCGlobal(name);
		if (g) vt = &(*g)["var-type"];
	}
	if (!vt || !isFuncPtr(*vt)) return nullptr;
	if (!findCallees(name).empty()) {
		cerr << locPrefix(call) << PlnSaMessage::getMessage(E_AmbiguousFuncPtrCall, name) << endl;
		exit(1);
	}
	return vt;
}

// `s.f(args)` calls the function pointer field `f` of `s`, if it has one: the
// field access, or null. A function `f` the call could also be the method
// form of makes it ambiguous.
json PlnSemanticAnalyzer::funcPtrField(const json& memberCall)
{
	if (!isMethodForm(memberCall)) return json();
	const json& obj = memberCall["object"];
	const string& method = memberCall["method"].get<string>();
	json vt = operandValueType(obj);
	if (!isStructPntr(vt)) return json();
	auto& fields = structDefs_.at(vt["base-type"]["type-name"].get<string>()).fields;
	auto f = find_if(fields.begin(), fields.end(), [&](const FieldLayout& f) { return f.name == method; });
	if (f == fields.end() || !f->unsupported.empty() || !isFuncPtr(fieldValueType(*f))) return json();
	for (auto& c : findCallees(method))
		if (firstParamMutable(*c.def)) {
			cerr << locPrefix(memberCall) << PlnSaMessage::getMessage(E_AmbiguousFuncPtrCall, method) << endl;
			exit(1);
		}
	json access = {{"expr-type", "field-access"}, {"object", obj}, {"field", method}}; // LCOV_EXCL_BR_LINE
	if (memberCall.contains("loc")) access["loc"] = memberCall["loc"];
	return access;
} // LCOV_EXCL_BR_LINE

// A call through a function pointer. Its arguments follow a Palan function's
// rules whatever the pointer came from: the pointer's type is all that is
// known of the callee.
json PlnSemanticAnalyzer::sa_expr_call_ptr(const json& expr, json callee, const string& funcName)
{
	const json& func = callee["value-type"]["base-type"];
	json sa_expr = {{"expr-type", "call-ptr"}, {"callee", callee}}; // LCOV_EXCL_BR_LINE
	if (expr.contains("loc")) sa_expr["loc"] = expr["loc"];
	if (func["ret-type"].value("type-name", "") != "void")
		sa_expr["value-type"] = func["ret-type"];
	if (isOwnedBlock(func["ret-type"])) sa_expr["category"] = "expiring";
	markStructRet(sa_expr, expr);
	sa_expr["args"] = saCallArgs(expr, expr["args"], func["parameters"], false, funcName);
	return sa_expr;
} // LCOV_EXCL_BR_LINE

// The functions `name` means, also under any alias: for one called where its
// name is never written.
vector<Callee> PlnSemanticAnalyzer::findCalleesUnderAnyAlias(const string& name) const
{
	vector<Callee> found = findCallees(name);
	for (auto it = importScopes.rbegin(); it != importScopes.rend(); ++it)
		for (auto& [alias, bucket] : *it)
			if (!alias.empty())
				for (auto& c : findCalleesByAlias(alias, name)) found.push_back(c);
	return found;
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

// The `iter` or `next` functions a 'for' over the struct `structVT` points to
// calls: `iter(@T s) -> I` or `iter(@!T s) -> I` for a struct I, and
// `next(@!I it) -> bool ok, E`.
vector<Callee> PlnSemanticAnalyzer::forProtocolCallees(const string& name, const json& structVT) const
{
	const json& structType = structVT["base-type"];
	bool isNext = name == "next";
	vector<Callee> fit;
	set<string> seen;
	for (auto& c : findCalleesUnderAnyAlias(name)) {
		if (c.isC) continue;
		const json& def = *c.def;
		const json& params = def["parameters"];
		if (params.size() != 1 || params[0].contains("default") || palanVariadicParam(params)) continue;
		const json& pt = params[0]["var-type"];
		if (!isStructBorrow(pt) || pt["base-type"] != structType) continue;
		if (isNext) {
			const json& rets = def.value("rets", json::array());
			if (!pt.value("mutable", false) || rets.size() != 2
			    || rets[0]["var-type"] != json{{"type-kind","prim"},{"type-name","bool"}}) continue; // LCOV_EXCL_BR_LINE
		} else if (!isStructStorage(def.value("ret-type", json()))) {
			continue;
		}
		if (seen.insert(calleeKey(def, false)).second) fit.push_back(c); // LCOV_EXCL_BR_LINE
	}
	return fit;
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

// What a literal conversion converts `ast` to: the literal it folds to, unless
// that literal has a type of its own other than bool (a character, a C
// constant), or null when it folds to none. An array or dictionary literal
// converts only to a union: a struct already reads one as its fields.
json PlnSemanticAnalyzer::convertibleLiteral(const json& ast, bool toUnion)
{
	if (isAggregateLit(ast)) return toUnion ? ast : json();
	json lit = foldConstExpr(ast);
	string et = lit.value("expr-type", "");
	if (et == "lit-str" || et == "lit-uint" || et == "lit-flo") return lit;
	if (et == "lit-int" && !lit.contains("char")
	    && (!lit.contains("value-type") || lit["value-type"] == boolType())) return lit;
	return json();
}

// The functions converting the literal `lit` to the struct `structVT` points
// to: those visible by the type's name, under any alias since a conversion is
// never written qualified, returning the struct and callable with the literal
// alone. `true` converts only by a function taking bool, as a bool widens to
// any number.
vector<Callee> PlnSemanticAnalyzer::literalConversions(const json& structVT, const json& lit)
{
	const json& structType = structVT["base-type"];
	string name = PlnSaMessage::writtenTypeName(structType["type-name"].get<string>());
	vector<Callee> found = findCalleesUnderAnyAlias(name);

	// As analyzeCallArgs gives the literal: a number waits for the parameter's type.
	string et = lit["expr-type"];
	bool isBool = et == "lit-int" && lit.contains("value-type");
	// LCOV_EXCL_BR_START
	CallArg litArg = et == "lit-str" ? CallArg{lit, false, {{"expr-type", "lit-str"}, {"value-type", strLitType()}}}
	               : isBool ? CallArg{lit, false, lit}
	               : isAggregateLit(lit) ? CallArg{lit, true}
	               : CallArg{lit, true, json(), false, literalDefaultType(lit)};
	// LCOV_EXCL_BR_STOP
	vector<Callee> convs;
	set<string> seen;
	for (auto& c : found) {
		const json& def = *c.def;
		const json& ret = def.value("ret-type", json::object());
		const json& params = def["parameters"];
		if (!isStructStorage(ret) || ret["base-type"] != structType || params.empty()) continue; // LCOV_EXCL_BR_LINE
		// A first parameter taking a struct would ask for a conversion of its own.
		if (isStructPntr(params[0]["var-type"])) continue;
		const json* variadic = palanVariadicParam(params);
		size_t fixedCount = params.size() - (variadic ? 1 : 0);
		if (requiredArgCount(params, fixedCount) > 1) continue;
		const json first = fixedCount ? params[0] : variadicElemParam(*variadic);
		if (isBool && first["var-type"] != boolType()) continue;
		if (argFit(litArg, first, 0, c.isC, name) == ArgFit::None) continue;
		if (seen.insert(calleeKey(def, c.isC)).second) convs.push_back(c); // LCOV_EXCL_BR_LINE
	}
	return convs;
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

// A literal where a struct `T` or a borrow `@T` is expected, as the call
// converting it would be written: `T(lit)` or `@T(lit)`. Anything else is
// returned as it is.
json PlnSemanticAnalyzer::literalConversion(const json& ast, const json& toVT)
{
	if (!isStructPntr(toVT) || toVT.value("mutable", false)) return ast; // LCOV_EXCL_BR_LINE
	json lit = convertibleLiteral(ast, isUnionPntr(toVT));
	if (lit.is_null()) return ast;
	json storage = toVT;
	storage.erase("mutable");
	if (literalConversions(storage, lit).empty()) return ast;
	// LCOV_EXCL_BR_START
	if (ast.contains("loc")) lit["loc"] = ast["loc"];
	json call = {{"expr-type", "call"},
	             {"name", PlnSaMessage::writtenTypeName(storage["base-type"]["type-name"].get<string>())},
	             {"args", json::array({lit})}, {"converts-to", storage}};
	if (ast.contains("loc")) call["loc"] = ast["loc"];
	if (isStructStorage(toVT)) return call;
	json borrow = {{"expr-type", "addr-of"}, {"mutable", false}, {"object", move(call)}};
	if (ast.contains("loc")) borrow["loc"] = ast["loc"];
	return borrow;
	// LCOV_EXCL_BR_STOP
} // LCOV_EXCL_BR_LINE

// Whether `vt` is a union or a borrow of one: what an array or dictionary
// literal converts to rather than being read as its fields.
bool PlnSemanticAnalyzer::isUnionPntr(const json& vt) const
{
	if (!isStructPntr(vt)) return false;
	auto def = structDefs_.find(vt["base-type"]["type-name"].get<string>());
	return def != structDefs_.end() && def->second.isTagged;
}

json PlnSemanticAnalyzer::sa_expr_call(const json& expr)
{
	const string& callName = expr["name"].get<string>();
	if (expr.contains("converts-to")) {
		json call = expr;
		call.erase("converts-to");
		return makeCall(call, literalConversions(expr["converts-to"], expr["args"][0]), callName);
	}
	if (expr.contains("iterates")) {
		json call = expr;
		call.erase("iterates");
		return makeCall(call, forProtocolCallees(callName, expr["iterates"]), callName);
	}
	if (findFuncPtrVar(expr)) {
		json id = {{"expr-type", "id"}, {"name", expr["name"]}}; // LCOV_EXCL_BR_LINE
		if (expr.contains("loc")) id["loc"] = expr["loc"];
		return sa_expr_call_ptr(expr, sa_expression(id), callName);
	}

	vector<Callee> cands = findCallees(callName);
	if (cands.empty()) {
		for (auto it = importScopes.rbegin(); it != importScopes.rend(); ++it)
			for (auto& [als, bucket] : *it)
				if (als != "" && bucket.count(callName)) {
					cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_UnqualifiedAliasCall, callName) << endl;
					exit(1);
				}
		undefinedNameError(expr, expr, E_UndefinedFunction);
	}
	return makeCall(expr, usableCallees(expr, cands), callName);
} // LCOV_EXCL_BR_LINE

// Shared by sa_expr_call and sa_expr_member_call: these used to duplicate
// only part of this loop, silently dropping variadic promotion for an
// aliased call like `S.printf("%f\n", someFlo32)`.
json PlnSemanticAnalyzer::saCallArgs(const json& locNode, const json& args, const json& funcParams,
                                      bool isCFunc, const string& funcName)
{
	checkSpreadPlace(locNode, args, funcParams, funcName);
	bool isVariadic = palanVariadicParam(funcParams) || isCVariadicSig(funcParams);
	size_t fixedCount = funcParams.size() - (isVariadic ? 1 : 0);
	// C's `()` arrives as an empty list just like `(void)` and is checked as
	// `(void)` (as C23 reads it); glibc headers are fully prototyped.
	size_t required = requiredArgCount(funcParams, fixedCount);
	if (args.size() < required || (!isVariadic && args.size() > fixedCount)) {
		string expected = isVariadic ? "at least " + to_string(fixedCount)
		                : required < fixedCount ? to_string(required) + " to " + to_string(fixedCount)
		                : to_string(fixedCount);
		cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_ArgCountMismatch,
			funcName, expected, to_string(args.size())) << endl;
		exit(1);
	} // LCOV_EXCL_LINE -- only cleanup skipped by exit(1) is attributed here

	vector<CallArg> callArgs = analyzeCallArgs(args);
	return bindCallArgs(locNode, callArgs, funcParams, isCFunc, funcName);
}

// Analyzes the arguments, left to right, before any parameter is known.
vector<CallArg> PlnSemanticAnalyzer::analyzeCallArgs(const json& args)
{
	vector<CallArg> callArgs;
	for (size_t i = 0; i < args.size(); i++) {
		const json& rawArg = args[i];
		if (rawArg.value("expr-type", "") == "spread") {
			callArgs.push_back(spreadCallArg(rawArg, i + 1 == args.size()));
			continue;
		}
		CallArg a;
		a.chained = rawArg.value("expr-type", "") == "chained-value";
		a.ast = a.chained ? rawArg["value"] : rawArg;
		bool typeless = isAggregateLitArg(a.ast) || namesOverloadedFunc(a.ast);
		bool literal = !typeless && takesExpectedType(a.ast);
		a.pending = a.chained || typeless || literal;
		if (literal)
			a.probe = literalDefaultType(a.ast);
		else if (!typeless)
			(a.chained ? a.probe : a.sa) = isCallBorrow(a.ast) ? sa_call_borrow_arg(a.ast) : sa_arg_expression(a.ast);
		callArgs.push_back(move(a));
	}
	return callArgs;
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

// `...a`, analyzed as an array that keeps its length.
CallArg PlnSemanticAnalyzer::spreadCallArg(const json& rawArg, bool last)
{
	auto fail = [&](PlnSaMessageCode code, string arg1 = "\x01") {
		cerr << locPrefix(rawArg) << PlnSaMessage::getMessage(code, arg1) << endl;
		exit(1);
	};
	if (!last) fail(E_SpreadPlace);
	CallArg a;
	a.spread = true;
	a.pending = false;
	a.ast = rawArg["value"];
	if (a.ast.value("expr-type", "") == "addr-of") fail(E_SpreadBorrow);
	if (isAggregateLitArg(a.ast)) fail(E_SpreadNotArray, "an array literal");
	if (namesOverloadedFunc(a.ast)) fail(E_SpreadNotArray, "a function");
	a.sa = sa_arg_expression(a.ast);
	const json& vt = a.sa["value-type"];
	if (!isVarrPntr(vt) && !isArrLevel(vt)) fail(E_SpreadNotArray, "'" + typeDisplayName(vt) + "'");
	if (isArrLevel(vt) && !vt.contains("arr-size") && !vt.value("run-size", false))
		fail(E_SpreadUnsized, typeDisplayName(vt)); // LCOV_EXCL_BR_LINE
	return a;
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

// The spread array takes the place of every argument for the `[...]E` parameter.
void PlnSemanticAnalyzer::checkSpreadPlace(const json& locNode, const json& args, const json& funcParams,
                                           const string& funcName)
{
	if (args.empty() || args.back().value("expr-type", "") != "spread") return;
	const json* variadic = palanVariadicParam(funcParams);
	const json& at = args.back().contains("loc") ? args.back() : locNode;
	if (!variadic) {
		cerr << locPrefix(at) << PlnSaMessage::getMessage(E_SpreadNotVariadic, funcName) << endl;
		exit(1);
	}
	if (args.size() != funcParams.size()) {
		cerr << locPrefix(at) << PlnSaMessage::getMessage(E_SpreadPosition, variadic->value("name", ""), funcName) << endl;
		exit(1);
	}
}

// The elements are not converted, so they must be the callee's array's own.
optional<SaDiag> PlnSemanticAnalyzer::spreadMismatch(const json& vt, const json& elemVT) const
{
	// LCOV_EXCL_BR_START
	json xs = {{"type-kind","pntr"},{"run-size",true},{"base-type",elemVT}};
	if (copySourceMatch(vt, xs)) return nullopt;
	json param = {{"type-kind","pntr"},{"variadic",true},{"base-type",elemVT}};
	return SaDiag{E_SpreadElemMismatch, typeDisplayName(param), typeDisplayName(vt)};
	// LCOV_EXCL_BR_STOP
} // LCOV_EXCL_BR_LINE

// Whether an analyzed argument fits a parameter, and if not, why.
optional<SaDiag> PlnSemanticAnalyzer::callArgMismatch(const json& argAst, const json& saArg,
		const json& param, size_t argIdx, bool isCFunc, const string& funcName)
{
	const json& paramVT = param["var-type"];
	// What an owned parameter takes is copied into it.
	if (!isCFunc && isOwnedBlock(paramVT)) {
		if (givenIn(argAst) && isArrLevel(paramVT)) {
			MoveFit fit = moveShapeFit(saArg["value-type"], paramVT, /*inherits=*/false, saArg.value("resized", false));
			if (fit == MoveFit::Match) return nullopt;
			return SaDiag{fit == MoveFit::RunSize ? E_MoveRunSize : E_TransferShapeMismatch, // LCOV_EXCL_BR_LINE
			              typeDisplayName(paramVT), typeDisplayName(saArg["value-type"])};
		}
		if (copySourceMatch(saArg["value-type"], paramVT)) return nullopt;
		return SaDiag{E_IncompatibleTypes, typeDisplayName(saArg["value-type"]), typeDisplayName(paramVT)}; // LCOV_EXCL_BR_LINE
	}
	if (paramVT.contains("arr-size")) {
		if (auto d = arrBorrowBindingMismatch(argAst, saArg, paramVT)) return d;
	} else if (paramVT.value("embedded", false)) {
		// The type registry can't tell contiguous elements from pointer slots,
		// nor a contiguous struct array from a single struct.
		const json& argVT = saArg["value-type"];
		bool argContig = argVT.value("embedded", false) && argVT.contains("stride");
		const json& paramElem = paramVT["base-type"];
		if (isArrLevel(paramElem)) {
			const json* argRow = argContig ? &argVT["base-type"] : nullptr;
			bool argSized = argRow && isArrLevel(*argRow) && argRow->contains("arr-size");
			if (!argSized || (*argRow)["arr-size"] != paramElem["arr-size"]) {
				string actual = argSized
					? to_string((*argRow)["arr-size"].get<int64_t>()) : "variable";
				// LCOV_EXCL_BR_START
				return SaDiag{E_EmbeddedArrInnerSizeMismatch,
				              to_string(paramElem["arr-size"].get<int64_t>()), actual};
				// LCOV_EXCL_BR_STOP
			}
		} else if (!argContig) {
			return SaDiag{E_IncompatibleTypes, typeDisplayName(argVT), typeDisplayName(paramVT)}; // LCOV_EXCL_BR_LINE
		}
	}
	if (auto d = elemShapeMismatch(saArg, paramVT)) return d;
	if (auto d = callArgConvMismatch(saArg, paramVT)) return d;
	if (isStructBorrow(paramVT) && isStructStorage(saArg["value-type"]))
		return SaDiag{E_StructBorrowNeedsAddrOf}; // LCOV_EXCL_BR_LINE
	if (isVarrPntr(paramVT) && isPtrBorrow(paramVT) && isVarrStorage(saArg["value-type"]))
		return SaDiag{E_VarrBorrowNeedsAddrOf}; // LCOV_EXCL_BR_LINE
	if (isDictPntr(paramVT) && isPtrBorrow(paramVT) && isDictStorage(saArg["value-type"]))
		return SaDiag{E_DictBorrowNeedsAddrOf}; // LCOV_EXCL_BR_LINE
	// A C function takes an array by name, as C passes one.
	if (!isCFunc && isPtrBorrow(paramVT) && namesArrStorage(saArg))
		return SaDiag{E_ArrBorrowNeedsAddrOf}; // LCOV_EXCL_BR_LINE
	// A by-value struct is only read, to copy it, so a read-only source is fine.
	if (isCFunc && isStructStorage(paramVT)) return nullopt;
	// An upgrade's diagnostic differs by callee kind, since a C parameter's
	// permission comes from const-qualification while a Palan parameter's comes
	// from `@!T`.
	if (isCFunc && ptrPermission(saArg["value-type"], paramVT) == PtrPermission::Upgrade) {
		return SaDiag{E_ReadOnlyPtrToNonConstCParam, funcName, paramLabel(param, argIdx)}; // LCOV_EXCL_BR_LINE
	}
	return ptrPermissionMismatch(saArg["value-type"], paramVT);
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

static json withoutLoc(const json& e)
{
	if (e.is_object()) {
		json out = json::object();
		for (auto& [k, v] : e.items())
			if (k != "loc") out[k] = withoutLoc(v);
		return out;
	}
	if (e.is_array()) {
		json out = json::array();
		for (auto& v : e) out.push_back(withoutLoc(v));
		return out;
	}
	return e;
}

// The steps of a location path from its variable: "v:name", ".field",
// "[index]"; empty for anything but a path.
static vector<string> pathKeys(const json& e)
{
	string et = e.value("expr-type", "");
	vector<string> keys;
	if (et == "id") {
		keys.push_back("v:" + e["name"].get<string>());
	} else if (et == "field-access" || et == "arr-index") {
		bool field = et == "field-access";
		keys = pathKeys(e[field ? "object" : "array"]);
		if (!keys.empty())
			keys.push_back(field ? "." + e["field"].get<string>() : "[" + withoutLoc(e["index"]).dump() + "]");
	}
	return keys;
}

static bool isPrefixOf(const vector<string>& a, const vector<string>& b)
{
	return a.size() <= b.size() && equal(a.begin(), a.end(), b.begin());
}

// Each path read in e, and each of their own prefixes.
static void collectPaths(const json& e, set<vector<string>>& paths, set<string>& vars)
{
	if (e.is_array()) {
		for (auto& v : e) collectPaths(v, paths, vars);
		return;
	}
	if (!e.is_object()) return;
	vector<string> keys = pathKeys(e);
	if (!keys.empty()) {
		paths.insert(keys);
		vars.insert(keys[0].substr(2));
	}
	for (auto& [k, v] : e.items()) collectPaths(v, paths, vars);
}

// What a later argument must not change while the owner given away by
// `given` waits to be reset: the containers of the path (not the path itself,
// whose borrow only reaches what it owns), and every path its indices read.
struct GivenOwner {
	string                 text;
	vector<string>         path;
	set<vector<string>>    changeable;
	set<string>            vars;
};

static GivenOwner givenOwnerOf(const json& given)
{
	GivenOwner g;
	g.path = pathKeys(given["value"]);
	for (size_t n = 1; n < g.path.size(); n++)
		g.changeable.insert(vector<string>(g.path.begin(), g.path.begin() + n));
	if (g.path.empty()) return g;
	g.vars.insert(g.path[0].substr(2));
	for (auto& k : g.path)
		g.text += k[0] == 'v' ? k.substr(2) : k[0] == '.' ? k : "[..]";
	for (const json* e = &given["value"]; e->value("expr-type", "") != "id"; ) {
		bool field = (*e)["expr-type"] == "field-access";
		if (!field) collectPaths((*e)["index"], g.changeable, g.vars);
		e = &(*e)[field ? "object" : "array"];
	}
	return g;
} // LCOV_EXCL_LINE

// The node in e that may change the owner g waits to reset, if any.
static const json* changesGiven(const json& e, const GivenOwner& g)
{
	if (e.is_array()) {
		for (auto& v : e)
			if (const json* c = changesGiven(v, g)) return c;
		return nullptr;
	}
	if (!e.is_object()) return nullptr;
	string et = e.value("expr-type", "");
	if (et == "transfer") {
		vector<string> p = pathKeys(e["value"]);
		if (!p.empty() && (isPrefixOf(p, g.path) || isPrefixOf(g.path, p))) return &e;
	} // LCOV_EXCL_BR_LINE
	if (et == "addr-of" && e.value("mutable", false) && g.changeable.count(pathKeys(e["object"])))
		return &e;
	// A method-form receiver is borrowed with '@!'.
	if (et == "member-call" && g.changeable.count(pathKeys(e["object"])))
		return &e;
	for (auto& [k, v] : e.items())
		if (const json* c = changesGiven(v, g)) return c;
	return nullptr;
}

// A function binds a global it uses statically; at the top level a global is
// a variable of the outermost scope.
bool PlnSemanticAnalyzer::isGlobalName(const string& name) const
{
	if (isStaticBound(name)) return true;
	for (size_t i = varScopes.size(); i > 0; --i)
		if (varScopes[i-1].count(name))
			return i == 1 && globals_.count(name);
	return false;
}

// An owner given away with '>>' is reset only after all of the call's
// arguments, so none after it may change it in between.
void PlnSemanticAnalyzer::checkGivenOwners(const json& locNode, const vector<json>& argAsts, const json& saArgs)
{
	for (size_t i = 0; i < argAsts.size(); i++) {
		if (!givenIn(argAsts[i]) || !saArgs[i].contains("reset")) continue;
		GivenOwner g = givenOwnerOf(argAsts[i]);
		if (g.path.empty()) continue;
		bool global = any_of(g.vars.begin(), g.vars.end(), [&](const string& v) { return isGlobalName(v); });
		for (size_t j = i + 1; j < argAsts.size(); j++) {
			const json* c = changesGiven(argAsts[j], g);
			// A function a global permits may write it.
			if (!c && global && hasCall(saArgs[j])) c = &argAsts[j];
			if (!c) continue;
			cerr << locPrefix(c->contains("loc") ? *c : locNode)
			     << PlnSaMessage::getMessage(E_GivenChangedByLaterArg, g.text) << endl;
			exit(1);
		}
	}
}

// Binds the analyzed arguments to the chosen callee's parameters: a pending
// argument is analyzed now with its parameter's type, then each is checked
// and converted.
json PlnSemanticAnalyzer::bindCallArgs(const json& locNode, vector<CallArg>& args, const json& funcParams,
                                        bool isCFunc, const string& funcName)
{
	const json* palanVariadic = palanVariadicParam(funcParams);
	bool cVariadic = isCVariadicSig(funcParams);
	size_t fixedCount = funcParams.size() - (palanVariadic || cVariadic ? 1 : 0);
	json elemParam = palanVariadic ? variadicElemParam(*palanVariadic) : json();
	// A string default is analyzed here, where its label joins this module's table.
	for (size_t i = args.size(); i < fixedCount; i++) {
		const json& d = funcParams[i]["default"];
		if (d["expr-type"] == "lit-str") {
			args.push_back(analyzeCallArgs(json::array({d}))[0]); // LCOV_EXCL_BR_LINE
		} else {
			CallArg ca{d, false, d}; // LCOV_EXCL_BR_LINE
			args.push_back(move(ca));
		}
	}
	json saArgs = json::array();
	vector<json> argAsts;
	json spreadCount;
	for (size_t argIdx = 0; argIdx < args.size(); argIdx++) {
		CallArg& ca = args[argIdx];
		if (ca.spread) {
			argAsts.push_back(ca.ast);
			saArgs.push_back(spreadArrayArg(locNode, ca, elemParam["var-type"], spreadCount));
			continue;
		}
		const json* param = argIdx < fixedCount ? &funcParams[argIdx] : palanVariadic ? &elemParam : nullptr;
		const json* paramVT = param ? &(*param)["var-type"] : nullptr;
		if (paramVT) {
			json converted = literalConversion(ca.ast, *paramVT);
			if (converted != ca.ast) ca = CallArg{move(converted), true};
		}
		bool probed = ca.chained && ca.probe.contains("expr-type");
		json arg = ca.chained ? chainArg(ca.ast, paramVT, isCFunc, probed ? &ca.probe["value-type"] : nullptr)
		                      : ca.ast;
		bool byValStruct = isCFunc && paramVT && isStructStorage(*paramVT);
		json argAst = arg;
		json litBody, litRelease;
		bool isLit = isAggregateLitArg(arg);
		bool ownedParam = !isCFunc && paramVT && isOwnedBlock(*paramVT);
		argAsts.push_back(arg);
		if (givenIn(arg) && !ownedParam) {
			string paramName = paramLabel(param ? *param : json::object(), argIdx); // LCOV_EXCL_BR_LINE
			cerr << locPrefix(locNode) << PlnSaMessage::getMessage(E_TransferToParam, paramName, funcName) << endl;
			exit(1);
		} // LCOV_EXCL_LINE
		if (ownedParam) {
			saArgs.push_back(ownedBlockArg(locNode, arg, ca, *param, argIdx, funcName));
			continue;
		}
		// A returned value chained as '@v' is borrowed as it was analyzed: an
		// operator's operand may be one with no call written for '@' to take.
		bool borrowsProbe = probed && arg != ca.ast && isExpiringOwned(ca.probe);
		json saArg = isLit ? sa_lit_arg(arg, isCFunc && !byValStruct ? nullptr : paramVT,
		                                paramLabel(param ? *param : json::object(), argIdx), funcName, // LCOV_EXCL_BR_LINE
		                                argAst, litBody, litRelease)
		           : probed && arg == ca.ast ? move(ca.probe)
		           : borrowsProbe ? borrowExpiring(move(ca.probe), arg["mutable"])
		           : !ca.pending ? move(ca.sa)
		           : isCallBorrow(arg) ? sa_call_borrow_arg(arg)
		           : sa_arg_expression(arg, paramVT);
		bool expiring = borrowsProbe || isCallBorrow(arg) || isExpiringOwned(saArg);
		if (takesVarrData(saArg["value-type"], paramVT, isCFunc))
			saArg = varrData(move(saArg));
		if (paramVT) {
			if (!isCFunc) saArg = typedNull(move(saArg), *paramVT);
			if (auto d = callArgMismatch(argAst, saArg, *param, argIdx, isCFunc, funcName))
				reportDiag(locNode, *d);
			if (!isCFunc)
				checkRangeBorrowBinding(locNode, argAst, *paramVT);
			saArg = convertCallArg(move(saArg), *paramVT);
		} else if (cVariadic) {
			const PlnType* fromType = registry_.fromJson(saArg["value-type"]);
			const PlnType* promoted = variadicPromote(fromType, registry_);
			if (promoted != fromType)
				saArg = wrapConvert(saArg, registry_.toJson(promoted));
		}
		if (isLit)
			saArg = makeTempExpr(move(litBody), move(saArg), move(litRelease));
		else if (expiring)
			saArg = bindCallTemp(move(saArg));
		if (byValStruct) {
			const StructDef& def = requireCompleteStruct((*paramVT)["base-type"]["type-name"].get<string>(), locNode);
			saArg["struct-arg"] = {{"size", def.totalSize}, {"align", def.maxAlign}, {"eightbytes", eightbytesJson(def, structDefs_, false)}}; // LCOV_EXCL_BR_LINE
		} else {
			noteEscape(saArg);
		}
		saArgs.push_back(saArg);
	}
	checkGivenOwners(locNode, argAsts, saArgs);
	if (!spreadCount.is_null()) {
		saArgs.push_back(move(spreadCount));
	} else if (palanVariadic) {
		json elems = json::array();
		while (saArgs.size() > fixedCount) {
			elems.insert(elems.begin(), move(saArgs.back()));
			saArgs.erase(saArgs.size() - 1);
		}
		size_t count = elems.size();
		saArgs.push_back(variadicArrayArg(locNode, elemParam["var-type"], move(elems)));
		saArgs.push_back({{"expr-type","lit-int"},{"value",to_string(count)}, // LCOV_EXCL_BR_LINE
		                  {"value-type",{{"type-kind","prim"},{"type-name","uint64"}}}}); // LCOV_EXCL_BR_LINE
	}
	return saArgs;
} // LCOV_EXCL_BR_LINE

// The arguments for a `[...]E` parameter, laid out in an array in the caller's
// frame: NULL when there are none. An argument's temporaries move to the
// array's own, so that what they release outlives the call.
json PlnSemanticAnalyzer::variadicArrayArg(const json& locNode, const json& elemVT, json elems)
{
	// LCOV_EXCL_BR_START
	if (elems.empty())
		return {{"expr-type","lit-int"},{"value","0"},
		        {"value-type",{{"type-kind","pntr"},{"base-type",{{"type-kind","prim"},{"type-name","void"}}}}}};
	string name = "__va_" + to_string(tempVarCounter_++);
	int64_t slot = elemVT.value("type-kind", "") == "prim" ? typeByteSize(locNode, elemVT) : 8;
	json arrType = {{"type-kind","pntr"},{"arr-size",(int64_t)elems.size()},{"base-type",elemVT}};
	json body = json::array({{{"stmt-type","var-decl"},{"vars",json::array({{{"name",name},{"var-type",arrType},
		{"init",{{"expr-type","frame-alloc"},{"size",slot * (int64_t)elems.size()},{"align",8},{"value-type",arrType}}}}})}}});
	declareVar(name, arrType);
	json release = json::array(), reset = json::array();
	for (size_t i = 0; i < elems.size(); i++) {
		json value = move(elems[i]);
		if (value.value("expr-type", "") == "temp") {
			for (auto& s : value["body"]) body.push_back(move(s));
			for (auto& s : value["release"]) release.push_back(move(s));
			for (auto& s : value.value("reset", json::array())) reset.push_back(s);
			value = json(value["value"]);
		}
		json target = sa_expression({{"expr-type","arr-index"},{"array",{{"expr-type","id"},{"name",name}}},
		                             {"index",{{"expr-type","lit-int"},{"value",to_string(i)}}}});
		body.push_back({{"stmt-type","arr-assign"},{"target",move(target)},{"value",move(value)}});
	}
	json array = {{"expr-type","id"},{"name",name},{"var-type",arrType},{"value-type",arrType}};
	json temp = makeTempExpr(move(body), move(array), move(release));
	if (!reset.empty()) temp["reset"] = move(reset);
	return temp;
	// LCOV_EXCL_BR_STOP
} // LCOV_EXCL_BR_LINE

// The array `...a` gives a `[...]E` parameter: a copy, or the array itself
// when it is given away. The callee releases the elements either way, so only
// the block is freed after the call.
json PlnSemanticAnalyzer::spreadArrayArg(const json& locNode, CallArg& ca, const json& elemVT, json& count)
{
	json src = move(ca.sa);
	if (auto d = spreadMismatch(src["value-type"], elemVT)) reportDiag(locNode, *d);
	// LCOV_EXCL_BR_START
	json body = json::array(), release = json::array(), reset = json::array();
	if (src.value("expr-type", "") == "temp") {
		for (auto& s : src["body"]) body.push_back(move(s));
		for (auto& s : src["release"]) release.push_back(move(s));
		for (auto& s : src.value("reset", json::array())) reset.push_back(s);
		src = json(src["value"]);
	}
	const json vt = src["value-type"];
	const json u64 = {{"type-kind","prim"},{"type-name","uint64"}};
	auto idOf = [](const string& name, const json& type) {
		return json{{"expr-type","id"},{"name",name},{"var-type",type},{"value-type",type}};
	};
	auto countOf = [&](const json& len) {
		int64_t n = constArrSize(len);
		return n >= 0 ? json{{"expr-type","lit-int"},{"value",to_string(n)},{"value-type",u64}} : wrapConvert(len, u64);
	};
	json value;
	if (givenIn(ca.ast) || isExpiringOwned(src)) {
		if (src.value("expr-type", "") != "id") src = idOf(pinTemp(move(src), locNode, body), vt);
		count = countOf(valueLens(src)[0]);
		if (isVarrPntr(vt)) {
			json data = varrData(src);
			json dataType = data["value-type"];
			string dataName = pinTemp(move(data), locNode, body);
			value = idOf(dataName, dataType);
			release.push_back(makeFreeStmt(dataName, dataType));
		} else {
			value = src;
		}
		release.push_back(makeFreeStmt(src["name"].get<string>(), vt));
	} else {
		if (hasCall(src)) src = pinValue(move(src), ca.ast, body)["sa"];
		json len = countOf(valueLens(src)[0]);
		string name = "__va_" + to_string(tempVarCounter_++);
		json arrType = {{"type-kind","pntr"},{"base-type",elemVT}};
		setArrSize(arrType, len);
		json decl = {{"stmt-type","var-decl"},{"loc",locNode.value("loc", json::array())},
		             {"vars",json::array({{{"name",name}}})}};
		enterScope();
		for (auto& s : sa_arr_var_decl(decl, VarStorage::Heap, Storage{arrType, {len}}, false)) body.push_back(move(s));
		value = idOf(name, arrType);
		body.push_back(makeCopyStmt(locNode, value, src, CopyInto::New));
		takeScopeRelease(name);
		leaveScope();
		count = arrType.contains("arr-size") ? len : idOf("__" + name + "_d0", u64);
		release.push_back(makeFreeStmt(name, arrType));
	}
	json temp = makeTempExpr(move(body), move(value), move(release));
	if (!reset.empty()) temp["reset"] = move(reset);
	return temp;
	// LCOV_EXCL_BR_STOP
} // LCOV_EXCL_BR_LINE

// An owned parameter takes the argument over: a returned array block as it
// is, a returned struct written into a block of its own, anything else as a
// copy the callee frees.
json PlnSemanticAnalyzer::ownedBlockArg(const json& locNode, const json& arg, CallArg& ca,
                                         const json& param, size_t argIdx, const string& funcName)
{
	const json& paramVT = param["var-type"];
	bool isLit = isAggregateLitArg(arg);
	json saArg;
	if (!isLit) {
		bool probed = ca.chained && ca.probe.contains("expr-type") && arg == ca.ast;
		saArg = !ca.pending ? move(ca.sa) : probed ? move(ca.probe)
		      : sa_arg_expression(arg, &paramVT);
		if (auto d = callArgMismatch(arg, saArg, param, argIdx, false, funcName))
			reportDiag(locNode, *d);
		if (isStructRetCall(saArg)) {
			// LCOV_EXCL_BR_START
			string name = "__arg_" + to_string(tempVarCounter_++);
			json value = {{"expr-type","id"},{"name",name},{"var-type",paramVT},{"value-type",paramVT}};
			return makeTempExpr(declareStructRet(name, paramVT, move(saArg), VarStorage::Heap), move(value), json::array());
			// LCOV_EXCL_BR_STOP
		}
		if (isExpiringOwned(saArg)) return saArg;
	}
	// LCOV_EXCL_BR_START
	string name = "__arg_" + to_string(tempVarCounter_++);
	json var = {{"name",name},{"var-type",astTypeOf(paramVT)}};
	if (isLit) {
		var["init"] = arg;
		var["lit-name"] = paramLabel(param, argIdx);
	}
	json decl = {{"stmt-type","var-decl"},{"loc",arg.value("loc", json::array())},{"vars",json::array({var})}};
	enterScope();
	json body = sa_resolved_var_decl(decl);
	json value = {{"expr-type","id"},{"name",name},{"var-type",paramVT},{"value-type",paramVT}};
	if (!isLit) body.push_back(makeCopyStmt(arg, value, saArg, CopyInto::New));
	takeScopeRelease(name);
	leaveScope();
	return makeTempExpr(move(body), move(value), json::array());
	// LCOV_EXCL_BR_STOP
} // LCOV_EXCL_BR_LINE

// Embedded-array shape and pointer-permission checks are callArgMismatch's,
// since those differ by callee kind (C vs. Palan).
optional<SaDiag> PlnSemanticAnalyzer::callArgConvMismatch(const json& saArg, const json& paramVT)
{
	const PlnType* fromType = registry_.fromJson(saArg["value-type"]);
	const PlnType* toType   = registry_.fromJson(paramVT);
	if (fromType == toType) return nullopt;
	if (isScalar(fromType) && isScalar(toType)) {
		if (argConvOk(fromType, toType)) return nullopt;
		return SaDiag{E_InvalidNarrowingConv, typeDisplayName(saArg["value-type"]), typeDisplayName(paramVT)}; // LCOV_EXCL_BR_LINE
	}
	// pntr(void) is bidirectionally Identical with any pntr(T), which is how NULL passes.
	if (typeCompat(fromType, toType, registry_) == TypeCompat::Incompatible)
		return SaDiag{E_IncompatibleTypes, typeDisplayName(saArg["value-type"]), typeDisplayName(paramVT)}; // LCOV_EXCL_BR_LINE
	return nullopt;
}

// Converts an argument callArgConvMismatch accepted to its parameter's type;
// it accepts a scalar only into a scalar.
json PlnSemanticAnalyzer::convertCallArg(json saArg, const json& paramVT)
{
	const PlnType* fromType = registry_.fromJson(saArg["value-type"]);
	const PlnType* toType   = registry_.fromJson(paramVT);
	if (fromType == toType || !isScalar(fromType)) return saArg;
	return wrapConvert(saArg, registry_.toJson(toType));
}

// A member-call whose object is a variable (or C global) is a method-form
// call; otherwise it is a module-alias call `L.f(args)`. A variable takes
// precedence over an alias of the same name, as it does over an enum name.
bool PlnSemanticAnalyzer::isMethodForm(const json& memberCall) const
{
	const json& obj = memberCall["object"];
	if (obj.value("expr-type", "") != "id") return true;
	const string& name = obj["name"].get<string>();
	return findVar(name) || findCGlobal(name);
}

// The function a call names, looked up as sa_expr_call does; null when it is
// unknown or ambiguous, which sa_expr_call reports.
const json* PlnSemanticAnalyzer::findCalleeByName(const string& name, bool& isCFunc) const
{
	vector<Callee> cands = findCallees(name);
	if (cands.size() != 1) return nullptr;
	isCFunc = cands[0].isC;
	return cands[0].def;
}

// How many functions the call a value is chained to chooses among.
size_t PlnSemanticAnalyzer::countCallees(const json& call) const
{
	string et = call.value("expr-type", "");
	if (et == "call") return findCallees(call["name"].get<string>()).size();
	// chainTarget has rewritten a method-form call.
	BOOST_ASSERT(et == "member-call" && !isMethodForm(call));
	return findCalleesByAlias(call["object"]["name"].get<string>(), call["method"].get<string>()).size();
}

// The function a call, a module-alias call, a method-form call, or a chained
// call (its last call) names; null otherwise.
const json* PlnSemanticAnalyzer::findCallee(const json& call, bool& isCFunc) const
{
	isCFunc = false;
	string et = call.value("expr-type", "");
	if (et == "chain-call")
		return findCallee(call["call"], isCFunc);
	if (et == "call")
		return findCalleeByName(call["name"].get<string>(), isCFunc);
	if (et != "member-call")
		return nullptr;
	if (isMethodForm(call))
		return findCalleeByName(call["method"].get<string>(), isCFunc);
	vector<Callee> cands = findCalleesByAlias(call["object"]["name"].get<string>(), call["method"].get<string>());
	if (cands.size() != 1) return nullptr;
	isCFunc = cands[0].isC;
	return cands[0].def;
}

// The value type of a method-form receiver or a chained value; null when it
// has none (a void call). An inline struct field is storage and cannot be
// evaluated as a value, so its type comes from the layout.
json PlnSemanticAnalyzer::operandValueType(const json& obj)
{
	if (obj.value("expr-type", "") == "field-access") {
		FieldChain chain = resolveObjectChain(obj["object"], /*forWrite=*/false);
		return fieldValueType(findFieldOrExit(chain.structName, obj["field"].get<string>(), obj, false));
	}
	return sa_expression(obj)["value-type"];
}

// Rewrites a method-form call `aa.f(args)` to the call `f(@!aa, args)`. Any
// other expression, including a module-alias call `L.f(args)`, is returned as
// is.
json PlnSemanticAnalyzer::normalizeMethodCall(const json& expr)
{
	if (expr["expr-type"] != "member-call" || !isMethodForm(expr)) return expr;
	const json& obj = expr["object"];

	const string& method = expr["method"].get<string>();
	vector<Callee> cands = findCallees(method);
	if (!cands.empty() && none_of(cands.begin(), cands.end(),
	                              [](const Callee& c) { return firstParamMutable(*c.def); })) {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_MethodNeedsMutableFirstParam, method) << endl;
		exit(1);
	}

	// A receiver that is already a borrow is passed as is: '@!' on it would
	// be rejected (or would mean a pointer to the borrow's slot).
	json recvType = operandValueType(obj);
	bool isBorrow = isPtrBorrow(recvType) || isArrBorrowVar(recvType);
	json recv = isBorrow ? obj : writableBorrow(obj);

	json call = {{"expr-type", "call"}, {"name", method}, {"args", json::array({recv})}}; // LCOV_EXCL_BR_LINE
	if (cands.size() > 1) call["method-form"] = true;
	for (auto& a : expr["args"]) call["args"].push_back(a);
	if (expr.contains("loc")) call["loc"] = expr["loc"];
	return call;
} // LCOV_EXCL_BR_LINE

static const json* fixedParamType(const json* callee, size_t idx)
{
	if (!callee) return nullptr;
	const json* variadic = palanVariadicParam((*callee)["parameters"]);
	if (variadic && idx + 1 >= (*callee)["parameters"].size()) return &(*variadic)["var-type"]["base-type"];
	if (idx >= (*callee)["parameters"].size()) return nullptr;
	const json& param = (*callee)["parameters"][idx];
	if (param.value("name", "") == "...") return nullptr;
	return &param["var-type"];
}

// A chained value given for a parameter: '@' may be omitted for a read-only
// borrow, so it is added here, while '@!' stays explicit so a write is
// visible at the call. A C function takes an array as it is.
json PlnSemanticAnalyzer::chainArg(const json& value, const json* paramVT, bool isCFunc, const json* valueType)
{
	string vet = value.value("expr-type", "");
	if (!paramVT || vet == "addr-of" || vet == "transfer") return value;
	if (paramVT->value("type-kind", "") != "pntr" || paramVT->value("mutable", true)) return value;
	// A literal has no type of its own until it is given a target, so it is
	// borrowed as '@[..]'/'@{..}' and checked against the parameter.
	string et = value.value("expr-type", "");
	if (et != "arr-lit" && et != "dict-lit") {
		json vt = valueType ? *valueType : operandValueType(value);
		bool ownedArr = isArrLevel(vt) && !isArrBorrowVar(vt);
		if (!isStructStorage(vt) && !isVarrStorage(vt) && !isDictStorage(vt) && vt.value("type-kind", "") != "prim"
				&& !(ownedArr && !isCFunc && isArrBorrowVar(*paramVT)))
			return value;
	}
	json out = {{"expr-type", "addr-of"}, {"mutable", false}, {"object", value}}; // LCOV_EXCL_BR_LINE
	if (value.contains("loc")) out["loc"] = value["loc"];
	return out;
} // LCOV_EXCL_BR_LINE

size_t PlnSemanticAnalyzer::chainRetCount(const json& value) const
{
	bool isCFunc;
	const json* callee = findCallee(value, isCFunc);
	return callee && callee->contains("rets") ? (*callee)["rets"].size() : 0;
}

// The call a value is chained to, and where the chained values go in its
// arguments: after the receiver of a method-form call.
json PlnSemanticAnalyzer::chainTarget(const json& call, size_t& at)
{
	json out = normalizeMethodCall(call);
	at = call["expr-type"] == "member-call" && out["expr-type"] == "call" ? 1 : 0;
	return out;
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

json PlnSemanticAnalyzer::chainInto(json call, size_t at, const vector<json>& values)
{
	bool isCFunc;
	const json* callee = findCallee(call, isCFunc);
	bool overloaded = countCallees(call) > 1;
	json& args = call["args"];
	for (size_t i = 0; i < values.size(); i++) {
		json arg = overloaded ? chainedValue(values[i]) : chainArg(values[i], fixedParamType(callee, at + i), isCFunc);
		args.insert(args.begin() + at + i, move(arg)); // LCOV_EXCL_BR_LINE
	}
	return call;
} // LCOV_EXCL_BR_LINE

// A value chained to a method-form call, evaluated into a temp in body so that
// it comes before the receiver; returns what is chained in its place. What no
// call can change, and what takes its type from the parameter, is left as is.
json PlnSemanticAnalyzer::chainValueAhead(const json& value, const json* paramVT, json& body)
{
	// LCOV_EXCL_BR_START
	if (foldConstExpr(value)["expr-type"].get<string>().rfind("lit-", 0) == 0
	    || isAggregateLitArg(value) || namesOverloadedFunc(value) || value["expr-type"] == "transfer")
		return value;
	auto tempId = [&](const string& name) {
		json id = {{"expr-type", "id"}, {"name", name}};
		if (value.contains("loc")) id["loc"] = value["loc"];
		return id;
	};
	// A returned struct is written into a frame temp of the statement's scope.
	auto frameTemp = [&](json sa) {
		json type = sa["value-type"];
		string name = "__pin_" + to_string(tempVarCounter_++);
		declareVar(name, type);
		for (auto& s : declareStructRet(name, type, move(sa), VarStorage::Frame)) body.push_back(move(s));
		arrayScopeVars_.back().push_back({name, frameStructRelease(name, type), false});
		return name;
	};
	if (isCallBorrow(value)) {
		json sa = sa_call_borrow_arg(value);
		sa["value-type"].erase("mutable");
		string name;
		if (isStructRetCall(sa)) {
			name = frameTemp(move(sa));
		} else {
			json storage = sa["value-type"];
			name = pinTemp(move(sa), value, body);
			addOwnedScopeVar(name, makeOwnedFreeStmt(name, storage));
		}
		return {{"expr-type", "addr-of"}, {"mutable", value["mutable"]}, {"object", tempId(name)}};
	}
	json sa;
	if (isLocationExpr(value) && isCopiedByValue(operandValueType(value))) {
		// A variable's storage stays where it is; that of a field or element
		// is held by its address.
		if (value["expr-type"] == "id") return value;
		sa = sa_expression({{"expr-type", "addr-of"}, {"mutable", false}, {"object", value}});
		sa["value-type"].erase("mutable");
	} else {
		bool typed = paramVT && paramVT->value("type-kind", "") != "pntr";
		sa = typed ? sa_expression_for(value, *paramVT) : sa_expression(value);
		if (!sa.contains("value-type")) return value;
	}
	noteEscape(sa);
	if (isStructRetCall(sa))
		return tempId(frameTemp(move(sa)));
	bool owned = isExpiringOwned(sa);
	json type = sa["value-type"];
	string name = pinTemp(move(sa), value, body);
	if (name.empty()) return value;
	if (owned) addOwnedScopeVar(name, makeOwnedFreeStmt(name, type));
	return tempId(name);
	// LCOV_EXCL_BR_STOP
} // LCOV_EXCL_BR_LINE

// Rewrites a chained call `v -> f(args)` to `f(v, args)`, and `v -> x.f(args)`
// to `f(@!x, v, args)`. A chain whose value is evaluated into temps first -- a
// call returning several values, or a value given to a method-form call where
// it or the receiver has a call -- is left as a chain-call for
// sa_expr_chain_call.
json PlnSemanticAnalyzer::normalizeChainCall(const json& expr)
{
	json value = expr["value"];
	if (value.value("expr-type", "") == "chain-call")
		value = normalizeChainCall(value);
	const json& call = expr["call"];
	bool method = call["expr-type"] == "member-call" && isMethodForm(call);
	if (chainRetCount(value) >= 2 || (method && (hasCall(value) || hasCall(call["object"])))) {
		json out = expr;
		out["value"] = move(value);
		return out;
	}
	size_t at;
	json target = chainTarget(call, at);
	return chainInto(move(target), at, {move(value)}); // LCOV_EXCL_BR_LINE
} // LCOV_EXCL_BR_LINE

// Evaluates a chained call's values into temps, appending their declarations
// to body, and returns the call with the temps in their place.
json PlnSemanticAnalyzer::chainAheadCall(const json& chain, json& body)
{
	// LCOV_EXCL_BR_START
	json value = chain["value"];
	if (value["expr-type"] == "chain-call")
		value = chainAheadCall(value, body);
	size_t at;
	json call = chainTarget(chain["call"], at);
	bool isCFunc;
	if (chainRetCount(value) < 2) {
		const json* paramVT = countCallees(call) > 1 ? nullptr : fixedParamType(findCallee(call, isCFunc), at);
		return chainInto(move(call), at, {chainValueAhead(value, paramVT, body)});
	}
	const json& rets = (*findCallee(value, isCFunc))["rets"];
	json loc = chain.value("loc", json::array());
	json decl = {{"stmt-type", "tapple-decl"}, {"vars", json::array()}, {"value", value}, {"loc", loc}};
	vector<json> temps;
	for (auto& r : rets) {
		string name = "__tap_" + to_string(tempVarCounter_++);
		decl["vars"].push_back({{"var-name", name}, {"var-type", r["var-type"]}});
		temps.push_back({{"expr-type", "id"}, {"name", name}, {"loc", loc}});
	} // LCOV_EXCL_LINE -- only exception cleanup is attributed here
	for (auto& s : sa_tapple_decl(decl))
		body.push_back(s);
	return chainInto(move(call), at, temps);
	// LCOV_EXCL_BR_STOP
} // LCOV_EXCL_BR_LINE

json PlnSemanticAnalyzer::sa_expr_chain_call(const json& expr)
{
	json norm = normalizeChainCall(expr);
	if (norm["expr-type"] != "chain-call")
		return sa_expr_maybe_void(norm);

	enterScope();
	json body = json::array();
	json saCall = sa_expr_maybe_void(chainAheadCall(norm, body));
	size_t idx = arrayScopeVars_.size() - 1;
	json release = collectFreeStmts(idx, idx + 1);
	leaveScope();
	return makeTempExpr(move(body), move(saCall), move(release));
} // LCOV_EXCL_BR_LINE

json PlnSemanticAnalyzer::sa_expr_member_call(const json& expr)
{
	const string& alias = expr["object"]["name"].get<string>();
	const string& method = expr["method"].get<string>();
	requireAlias(expr, alias);
	vector<Callee> cands = findCalleesByAlias(alias, method);
	if (cands.empty()) {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_UndefinedFunction, method) << endl;
		exit(1);
	}
	return makeCall(expr, usableCallees(expr, cands), method);
} // LCOV_EXCL_BR_LINE

const json& PlnSemanticAnalyzer::findMultiRetFunc(const json& stmt, const json& callExpr, size_t recvCount)
{
	vector<Callee> cands;
	string fname;
	if (callExpr["expr-type"] == "member-call") {
		const string& alias = callExpr["object"]["name"].get<string>();
		fname = callExpr["method"].get<string>();
		requireAlias(stmt, alias);
		cands = findCalleesByAlias(alias, fname);
	} else {
		fname = callExpr["name"].get<string>();
		cands = callExpr.contains("iterates") ? forProtocolCallees(fname, callExpr["iterates"]) : findCallees(fname);
	}
	if (cands.empty()) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_TupleUndefinedFunction, fname) << endl;
		exit(1);
	}
	const json* pFunc = cands[0].def;
	if (cands.size() > 1) {
		// The receivers are declared before the arguments choose a function, so
		// those returning recvCount values must agree on what they return.
		vector<Callee> multi;
		for (auto& c : cands)
			if ((*c.def).value("rets", json::array()).size() == recvCount) multi.push_back(c);
		if (!multi.empty()) {
			pFunc = multi[0].def;
			for (auto& c : multi)
				if ((*c.def)["rets"] != (*pFunc)["rets"]) {
					cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_AmbiguousCall,
						fname, calleesText(fname, multi)) << endl;
					exit(1);
				}
		}
	}
	if (!pFunc->contains("rets") || (*pFunc)["rets"].size() < 2) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_TupleNeedsMultiRet, fname) << endl;
		exit(1);
	}
	if (recvCount != (*pFunc)["rets"].size()) {
		cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_TupleVarCountMismatch, fname) << endl;
		exit(1);
	}
	return *pFunc;
}

// The call whose return values a multiple-return declaration or assignment
// receives.
json PlnSemanticAnalyzer::tappleCallExpr(const json& stmt)
{
	json value = stmt["value"];
	if (value.value("expr-type", "") == "chain-call") {
		value = normalizeChainCall(value);
		if (value["expr-type"] == "chain-call") {
			cerr << locPrefix(stmt) << PlnSaMessage::getMessage(E_ChainMultiToTapple) << endl;
			exit(1);
		}
	}
	return normalizeMethodCall(value);
}
