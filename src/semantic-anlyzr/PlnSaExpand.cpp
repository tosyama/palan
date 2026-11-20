/// Palan Semantic Analyzer — expandable definitions (function-like C macros)
///
/// @file PlnSaExpand.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <iostream>
#include "PlnSemanticAnalyzer.h"
#include "PlnSaMessage.h"
#include "PlnSaInternal.h"

// A cast's C type becomes the type a Palan cast writes, as a C typedef does
// (registerCIncludeTypes). Returns the form an unrepresentable type makes the
// body, or "".
string PlnSemanticAnalyzer::normalizeExpandableBody(json& node) const
{
	if (node.is_array()) {
		for (auto& e : node) {
			string form = normalizeExpandableBody(e);
			if (!form.empty()) return form;
		} // LCOV_EXCL_BR_LINE
		return "";
	}
	if (!node.is_object()) return "";
	if (node.value("expr-type", "") == "cast") {
		json t = node["target-type"];
		resolveCTypeRefs(t);
		t = normalizeCType(t);
		string bad = unrepresentableTypeName(t);
		if (!bad.empty()) return "a cast to " + bad;
		node["target-type"] = cTypeAsAlias(t);
	}
	for (const char* key : {"left", "right", "operand", "src", "array", "index", "object", "args"}) {
		if (!node.contains(key)) continue;
		string form = normalizeExpandableBody(node[key]);
		if (!form.empty()) return form;
	} // LCOV_EXCL_BR_LINE
	return "";
}

void PlnSemanticAnalyzer::registerExpandables(const json& stmt)
{
	auto& scope = expandableScopes.back();
	if (stmt.contains("ended-macros"))
		for (auto& n : stmt["ended-macros"]) scope[n.get<string>()] = json();
	if (!stmt.contains("expandables")) return;
	for (auto& d : stmt["expandables"]) {
		json def = d;
		if (stmt.contains("alias")) def["_alias"] = stmt["alias"];
		string form = normalizeExpandableBody(def["body"]);
		if (!form.empty()) def["_form"] = form;
		string name = def["name"].get<string>();
		scope[name] = move(def);
	}
}

const json* PlnSemanticAnalyzer::findExpandable(const string& name) const
{
	for (auto it = expandableScopes.rbegin(); it != expandableScopes.rend(); ++it) {
		auto f = it->find(name);
		if (f != it->end()) return f->second.is_null() ? nullptr : &f->second;
	}
	return nullptr;
}

// C functions alone, under any cinclude alias: an expandable body's C names
// need no qualifier.
vector<Callee> PlnSemanticAnalyzer::findCCallees(const string& name) const
{
	vector<Callee> cands;
	set<string> seen;
	auto add = [&](const json& def) {
		if (seen.insert(calleeKey(def, true)).second) cands.push_back({&def, true});
	};
	for (auto it = cFuncScopes.rbegin(); it != cFuncScopes.rend(); ++it) {
		auto f = it->find(name);
		if (f != it->end()) add(f->second);
	}
	for (auto it = importScopes.rbegin(); it != importScopes.rend(); ++it)
		for (auto& [alias, bucket] : *it) {
			auto f = bucket.find(name);
			if (f == bucket.end()) continue;
			for (auto& def : f->second)
				if (def.value("_c-func", false)) add(def);
		}
	return cands;
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

// The first C name in `body` the call can't reach: one not declared, or a C
// global a Palan variable of its name hides, since sa.json names both alike.
string PlnSemanticAnalyzer::unreachableCName(const json& body) const
{
	if (body.is_array()) {
		for (auto& e : body) {
			string name = unreachableCName(e);
			if (!name.empty()) return name;
		} // LCOV_EXCL_BR_LINE
		return "";
	}
	if (!body.is_object()) return "";
	if (body.contains("c-name")) {
		const string& name = body["name"].get_ref<const string&>();
		bool found = body["expr-type"] == "call" ? !findCCallees(name).empty()
		           : findCGlobal(name) && !findVar(name);
		if (!found) return name;
	}
	for (auto& [key, val] : body.items()) {
		string name = unreachableCName(val);
		if (!name.empty()) return name;
	}
	return "";
}

// Every place a parameter appears takes its argument's expression, located at
// the call, as the preprocessor substitutes it.
static json substitute(const json& body, const json& def, const json& args, const json& loc)
{
	if (body.is_array()) {
		json out = json::array();
		for (auto& e : body) out.push_back(substitute(e, def, args, loc));
		return out;
	}
	if (!body.is_object()) return body;
	if (body.value("expr-type", "") == "param") {
		const json& params = def["params"];
		for (size_t i = 0; i < params.size(); i++)
			if (params[i] == body["name"]) return args[i];
	}
	json out = json::object();
	for (auto& [key, val] : body.items())
		out[key] = key == "target-type" ? val : substitute(val, def, args, loc);
	if (out.contains("expr-type") && !out.contains("loc") && !loc.is_null()) out["loc"] = loc;
	return out;
} // LCOV_EXCL_BR_LINE

// The call `expr` to `def` with arguments `args`, as its body; null when it
// can't be expanded, with `why` set to the message for when nothing else is
// called instead.
json PlnSemanticAnalyzer::expandCall(const json& expr, const json& def, const json& args, SaDiag& why)
{
	const string& name = def["name"].get_ref<const string&>();
	if (def.contains("_form")) {
		why = {E_CMacroFuncLike, name, def["_form"].get<string>()};
		return json();
	}
	string unreachable = unreachableCName(def["body"]);
	if (!unreachable.empty()) {
		why = {E_CMacroUnresolvedName, name, unreachable};
		return json();
	}
	size_t count = def["params"].size();
	if (args.size() != count) {
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_ArgCountMismatch,
			name, to_string(count), to_string(args.size())) << endl;
		exit(1);
	}
	return substitute(def["body"], def, args, expr.value("loc", json()));
} // LCOV_EXCL_BR_LINE

// A call that names an expandable definition, `alias` qualifying it or not, as
// the expression it expands to. Null when none is named or it can't be
// expanded and a function of its name is called instead.
json PlnSemanticAnalyzer::expandedCall(const json& expr, const string& alias, const string& name)
{
	const json* def = findExpandable(name);
	if (!def) return json();
	string defAlias = def->value("_alias", "");
	if (defAlias != alias) {
		if (!alias.empty()) return json();
		cerr << locPrefix(expr) << PlnSaMessage::getMessage(E_UnqualifiedAliasCall, name) << endl;
		exit(1);
	}
	SaDiag why{E_CMacroFuncLike}; // LCOV_EXCL_BR_LINE
	json expanded = expandCall(expr, *def, expr.value("args", json::array()), why);
	if (!expanded.is_null()) return expanded;
	bool hasFunc = alias.empty() ? !findCallees(name).empty() : !findCalleesByAlias(alias, name).empty();
	if (hasFunc) return json();
	cerr << locPrefix(expr) << PlnSaMessage::getMessage(why.code, why.arg1, why.arg2) << endl;
	exit(1);
}
