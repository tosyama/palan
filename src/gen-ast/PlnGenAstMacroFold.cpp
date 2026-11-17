/// cinclude macro-constant table and reference folding pass.
///
/// @file PlnGenAstMacroFold.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include "PlnGenAstMacroFold.h"

void registerMacroConstants(MacroTable& macros, const json& constants, const json& cincludeLoc)
{
	for (auto& c : constants) {
		string name = c["name"].get<string>();
		macros[name].push_back(MacroEntry{ c["value"], c.value("value-type", json()), cincludeLoc, "" }); // LCOV_EXCL_BR_LINE
	}
}

void registerUnexportedMacros(MacroTable& macros, const json& unexported, const json& cincludeLoc)
{
	for (auto& m : unexported) {
		string name = m["name"].get<string>();
		macros[name].push_back(MacroEntry{ json(), json(), cincludeLoc, // LCOV_EXCL_BR_LINE
			m.value("function-like", false) ? "function-like" : "not-constant" });
	}
}

void registerUndefinedMacros(MacroTable& macros, const json& undefined, const json& cincludeLoc)
{
	for (auto& n : undefined) {
		MacroEntry e{ json(), json(), cincludeLoc, "" }; // LCOV_EXCL_BR_LINE
		e.undefined = true;
		macros[n.get<string>()].push_back(e);
	}
}

// True if `loc` (this id reference) is textually at or after `macroLoc`
// (the registering cinclude statement), comparing by begin (line, column).
static bool isAfterMacroLoc(const json& loc, const json& macroLoc)
{
	int refLine = loc[0].get<int>(), refCol = loc[1].get<int>();
	int macroLine = macroLoc[0].get<int>(), macroCol = macroLoc[1].get<int>();
	if (refLine != macroLine) return refLine > macroLine;
	return refCol > macroCol;
}

// The registration of `name` in effect at reference `loc`, or null.
static const MacroEntry* findMacroAt(const MacroTable& macros, const string& name, const json& loc)
{
	auto it = macros.find(name);
	if (it == macros.end()) return nullptr;
	for (auto e = it->second.rbegin(); e != it->second.rend(); ++e)
		if (isAfterMacroLoc(loc, e->loc) && (e->end.is_null() || !isAfterMacroLoc(loc, e->end))) return &*e;
	return nullptr;
}

void closeMacroScope(MacroTable& macros, const json& open, const json& close)
{
	for (auto& [name, entries] : macros)
		for (auto e = entries.rbegin(); e != entries.rend() && isAfterMacroLoc(e->loc, open); ++e)
			if (e->end.is_null()) e->end = close;
}

void foldMacroConstants(json& node, const MacroTable& macros)
{
	if (node.is_array()) {
		for (auto& e : node) foldMacroConstants(e, macros);
		return;
	}
	if (!node.is_object()) return;

	if (node.value("stmt-type", "") == "cinclude") return;

	string exprType = node.value("expr-type", "");
	if (exprType == "id" || exprType == "call") {
		const MacroEntry* e = findMacroAt(macros, node["name"].get<string>(), node["loc"]);
		if (e && !e->undefined) {
			if (!e->unexported.empty()) {
				node["c-macro"] = e->unexported;
			} else if (exprType == "id") {
				json loc = node["loc"];
				node = { {"expr-type", "lit-int"}, {"value", e->value}, {"loc", loc} }; // LCOV_EXCL_BR_LINE
				if (!e->valueType.is_null()) node["value-type"] = e->valueType;
			}
		} // LCOV_EXCL_BR_LINE
		if (exprType == "id") return;
	}

	for (auto& [key, val] : node.items()) {
		foldMacroConstants(val, macros);
	}
} // LCOV_EXCL_BR_LINE
