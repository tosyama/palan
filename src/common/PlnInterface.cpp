/// Writes a module's declarations as an interface (.pai).
///
/// @file PlnInterface.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <algorithm>
#include <cstdio>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <vector>
#include "PlnDefs.h"
#include "PlnInterface.h"

using namespace std;
using json = nlohmann::json;

namespace {

class Writer {
public:
	Writer(const json& ast, const vector<string>& libs) : ast_(ast), libs_(libs) {}
	string write();
	string type(const json& t) const;

private:
	const json& ast_;
	const vector<string>& libs_;

	[[noreturn]] void fail(const json& node, const string& what) const;
	string expr(const json& e) const;
	string operand(const json& e) const;
	string args(const json& list) const;
	string vars(const json& node, const json& list, bool isRets) const;
	string funcDecl(const json& f, bool exported = true) const;
	string statement(const json& s) const;
};

const map<string, string> binaryOps = {
	{"add", "+"}, {"sub", "-"}, {"mul", "*"}, {"div", "/"}, {"mod", "%"},
	{"bitand", "&"}, {"bitor", "|"}, {"bitxor", "^"}, {"shl", "<<<"}, {"shr", ">>>"},
	{"logical-and", "&&"}, {"logical-or", "||"},
};

void Writer::fail(const json& node, const string& what) const
{
	string at = ast_["original"].get<string>();
	if (node.contains("loc"))
		at += ":" + to_string(node["loc"][0].get<int>()) + ":" + to_string(node["loc"][1].get<int>());
	throw runtime_error(at + ": error: an interface cannot state " + what + ".");
}

string Writer::type(const json& t) const
{
	if (t.contains("not-impl")) fail(t, "this type");
	string kind = t["type-kind"];
	if (kind == "prim") return t["type-name"];
	if (kind == "embed") return "$" + type(t["base-type"]);
	if (kind == "keep") return "_";
	if (kind == "pntr") {
		string at = t.value("mutable", true) ? "@!" : "@";
		const json& base = t["base-type"];
		if (base["type-kind"] != "func") return at + type(base);
		string s = at + "func(";
		const char* sep = "";
		for (auto& p : base["parameters"]) {
			s += sep + type(p["var-type"]);
			sep = ", ";
		}
		s += ")";
		const json& ret = base["ret-type"];
		bool isVoid = ret["type-kind"] == "prim" && ret["type-name"] == "void";
		return isVoid ? s : s + " -> " + type(ret);
	}
	// Only "arr" is left: the other kinds come from C headers, whose declarations are not written.
	string mark = t["specifier"] == "fixed" ? "#" : t["specifier"] == "variable" ? "+"
		: t["specifier"] == "variadic" ? "..." : "";
	string size = t["size-expr"].is_null() ? "" : expr(t["size-expr"]);
	return "[" + mark + size + "]" + (t.value("embedded", false) ? "$" : "") + type(t["base-type"]);
}

// A nested operator is bracketed rather than relying on precedence; brackets leave no trace in the AST.
string Writer::operand(const json& e) const
{
	string kind = e.value("expr-type", "");
	bool isOperator = binaryOps.count(kind) || kind == "cmp" || kind == "cast" || kind == "chain-call";
	return isOperator ? "(" + expr(e) + ")" : expr(e);
}

string Writer::args(const json& list) const
{
	string s;
	const char* sep = "";
	for (auto& a : list) {
		s += sep + expr(a);
		sep = ", ";
	}
	return s;
}

string Writer::expr(const json& e) const
{
	string kind = e.value("expr-type", "");
	auto op = binaryOps.find(kind);
	if (op != binaryOps.end()) return operand(e["left"]) + " " + op->second + " " + operand(e["right"]);
	if (kind == "cmp") return operand(e["left"]) + " " + e["op"].get<string>() + " " + operand(e["right"]);
	if (kind == "lit-int") {
		bool isBool = e.contains("value-type") && e["value-type"].value("type-name", "") == "bool";
		if (isBool) return e["value"] == "1" ? "true" : "false";
		// Written as a character, it keeps the type restriction a character literal has.
		if (e.value("char", false)) {
			char esc[16];
			snprintf(esc, sizeof(esc), "'\\u{%lX}'", stol(e["value"].get<string>()));
			return string(esc);
		}
		return e["value"];
	}
	if (kind == "lit-uint") return e["value"].get<string>() + "u";
	// The value is already the literal's Palan spelling.
	if (kind == "lit-str") return "\"" + e["value"].get<string>() + "\"";
	if (kind == "lit-flo" || kind == "id") return e[kind == "id" ? "name" : "value"];
	if (kind == "neg") return "-" + operand(e["operand"]);
	if (kind == "bitnot") return "~" + operand(e["operand"]);
	if (kind == "logical-not") return "!" + operand(e["operand"]);
	if (kind == "sizeof") return "sizeof(" + type(e["type"]) + ")";
	if (kind == "cast") return operand(e["src"]) + " as " + type(e["target-type"]);
	if (kind == "call") return e["name"].get<string>() + "(" + args(e["args"]) + ")";
	if (kind == "member-call")
		return operand(e["object"]) + "." + e["method"].get<string>() + "(" + args(e["args"]) + ")";
	if (kind == "chain-call") return operand(e["value"]) + " -> " + expr(e["call"]);
	if (kind == "arr-index") return operand(e["array"]) + "[" + expr(e["index"]) + "]";
	if (kind == "field-access") return operand(e["object"]) + "." + e["field"].get<string>();
	if (kind == "addr-of") return (e["mutable"].get<bool>() ? "@!" : "@") + expr(e["object"]);
	if (kind == "arr-lit") return "[" + args(e["items"]) + "]";
	fail(e, "this expression");
}

// A parameter's default is filled in by the caller; a named return's initial value is run by the body.
string Writer::vars(const json& node, const json& list, bool isRets) const
{
	string s;
	const char* sep = "";
	for (auto& v : list) {
		if (!v.contains("var-type")) fail(v.contains("loc") ? v : node, "this parameter or return");
		s += sep + type(v["var-type"]) + " " + v["name"].get<string>();
		if (v.contains("init") && !isRets) s += " = " + expr(v["init"]);
		sep = ", ";
	}
	return s;
}

string Writer::funcDecl(const json& f, bool exported) const
{
	bool isSyscall = f["func-type"] == "syscall";
	string s = string(exported ? "export " : "") + (isSyscall ? "syscall " : "func ") + f["name"].get<string>()
		+ "(" + vars(f, f["parameters"], false) + ")";
	if (f.contains("rets")) s += " -> " + vars(f, f["rets"], true);
	else if (f.contains("ret-type")) s += " -> " + type(f["ret-type"]);
	if (isSyscall) s += " = " + expr(f["syscall-number"]);
	return s + ";\n";
}

string Writer::statement(const json& s) const
{
	string exported = s.value("export", false) ? "export " : "";
	string kind = s["stmt-type"];
	string name = s.value("name", "");
	if (kind == "const-decl") return "const " + name + " = " + expr(s["value"]) + ";\n";
	if (kind == "type-alias") return exported + "type " + name + " = " + type(s["type"]) + ";\n";
	if (kind == "enum-def") {
		string out = exported + "type " + name + " enum {\n";
		for (auto& e : s["enumerators"]) {
			out += "\t" + e["name"].get<string>();
			if (e.contains("value")) out += " = " + expr(e["value"]);
			out += ",\n";
		}
		return out + "};\n";
	}
	if (kind == "struct-def") {
		string out = exported + "type " + name + " {\n";
		for (auto& f : s["fields"])
			out += "\t" + (f.contains("access") ? f["access"].get<string>() + " " : "") // LCOV_EXCL_BR_LINE
				+ type(f["var-type"]) + " " + f["name"].get<string>()
				+ (f.contains("init") ? " = " + expr(f["init"]) : "") + ";\n";
		return out + "};\n";
	}
	string path = s["path-type"] == "src" ? "\"" + s["path"].get<string>() + "\"" : "<" + s["path"].get<string>() + ">";
	string alias = s.contains("alias") ? " as " + s["alias"].get<string>() : "";
	auto names = [](const json& list) {
		string out;
		const char* sep = "";
		for (auto& n : list) {
			out += sep + n.get<string>();
			sep = ", ";
		}
		return out;
	};
	if (kind == "cinclude") return "cinclude " + path + alias + " export " + names(s["export-types"]) + ";\n";
	string targets = s.contains("targets") ? names(s["targets"]) + " from " : "";
	return "import " + targets + path + alias + ";\n";
}

// Every library any `cinclude` of the module links, in source order, however deep in a body.
void collectLinks(const json& node, vector<string>& libs)
{
	if (node.is_object() && node.value("stmt-type", "") == "cinclude") {
		for (auto& l : node.value("libs", json::array()))
			if (find(libs.begin(), libs.end(), l.get<string>()) == libs.end()) libs.push_back(l);
		return;
	}
	if (node.is_structured())
		for (auto& child : node) collectLinks(child, libs);
}

void collectIds(const json& node, set<string>& ids)
{
	if (node.is_object() && node.value("expr-type", "") == "id") {
		ids.insert(node["name"].get<string>());
		return;
	}
	// `Color.Red` names a type, never a variable or a const.
	if (node.is_object() && node.value("expr-type", "") == "field-access"
			&& node["object"].value("expr-type", "") == "id")
		return;
	if (node.is_structured())
		for (auto& child : node) collectIds(child, ids);
}

bool hasCall(const json& node)
{
	string kind = node.is_object() ? node.value("expr-type", "") : "";
	if (kind == "call" || kind == "member-call" || kind == "chain-call") return true;
	if (node.is_structured())
		for (auto& child : node)
			if (hasCall(child)) return true;
	return false;
}

// The variable's type with each size its array literal gave filled in, if the
// sizes are then known without running anything: all a const can read from it.
optional<json> sizedArrayType(const json& var, const set<string>& consts)
{
	if (!var.contains("var-type")) return nullopt;
	json result = var["var-type"];
	static const json noRows = json::array();
	bool hasLiteral = var.contains("init") && var["init"].value("expr-type", "") == "arr-lit";
	const json* rows = hasLiteral ? &var["init"]["items"] : &noRows;
	json* level = result["type-kind"] == "embed" ? &result["base-type"] : &result;
	if (level->value("type-kind", "") != "arr") return nullopt;
	for (bool top = true; level->value("type-kind", "") == "arr"; top = false) {
		json& size = (*level)["size-expr"];
		bool isVarr = (*level)["specifier"] == "variable";
		if (size.is_null() && (top || !isVarr)) {
			if (rows->empty()) return nullopt;
			size = {{"expr-type", "lit-int"}, {"value", to_string(rows->size())}};
		} else if (!size.is_null()) {
			set<string> ids;
			collectIds(size, ids);
			for (auto& id : ids)
				if (!consts.count(id)) return nullopt;
			if (hasCall(size)) return nullopt;
		}
		rows = !rows->empty() && (*rows)[0].contains("items") ? &(*rows)[0]["items"] : &noRows;
		level = &(*level)["base-type"];
	}
	return result;
} // LCOV_EXCL_BR_LINE -- closing brace of a function with local json temporaries; the branch coverpoints here are compiler-generated destructor dispatch, not source-level conditionals

string Writer::write()
{
	const json& stmts = ast_["ast"]["statements"];

	// A const stays if everything its value names does: another const staying,
	// or a top-level array declared before it whose size is known. Any other
	// could not be used by a declaration anyway (E_ConstUsedBeforeKnown).
	set<string> consts;
	for (auto& s : stmts)
		if (s.value("stmt-type", "") == "const-decl") consts.insert(s["name"].get<string>());
	map<const json*, json> usedVars;
	for (bool changed = true; changed;) {
		changed = false;
		usedVars.clear();
		map<string, pair<const json*, json>> arrays;
		for (auto& s : stmts) {
			string kind = s.value("stmt-type", "");
			if (kind == "var-decl") {
				for (auto& v : s["vars"]) {
					auto sized = sizedArrayType(v, consts);
					if (sized) arrays[v["name"]] = {&v, *sized};
				}
			} else if (kind == "const-decl" && consts.count(s["name"])) {
				set<string> ids;
				collectIds(s["value"], ids);
				for (auto& id : ids) {
					auto arr = arrays.find(id);
					if (arr != arrays.end()) {
						usedVars[arr->second.first] = arr->second.second;
					} else if (!consts.count(id)) {
						consts.erase(s["name"].get<string>());
						changed = true;
						break;
					}
				}
			}
		}
	}

	string out;
	string module = ast_["module"];
	size_t dot = module.rfind('.');
	if (dot != string::npos) out += "library " + module.substr(0, dot) + ";\n";
	out += "built \"" PALAN_VERSION "\";\n";
	out += "target \"" + ast_["target"].get<string>() + "\";\n";
	for (auto& lib : libs_) out += "link \"" + lib + "\";\n";
	for (auto& s : stmts) {
		string kind = s.value("stmt-type", "");
		if (kind == "var-decl") {
			for (auto& v : s["vars"]) {
				auto used = usedVars.find(&v);
				if (used != usedVars.end()) out += type(used->second) + " " + v["name"].get<string>() + ";\n";
			}
		} else if (kind == "const-decl") {
			if (consts.count(s["name"])) out += statement(s);
		} else if (kind == "struct-def" || kind == "enum-def" || kind == "type-alias" || kind == "import"
				|| (kind == "cinclude" && s.contains("export-types"))) {
			out += statement(s);
		}
	}
	for (auto& f : ast_.value("export", json::array())) out += funcDecl(f);
	// A type's '->' comes with the type, unexported.
	for (auto& f : ast_["ast"].value("functions", json::array()))
		if (f["name"] == "->") out += funcDecl(f, false);
	return out;
}

// One library's modules merged into the AST of one interface. Names are looked
// up in the module writing them, since two modules may mean different things by one.
class Merger {
public:
	Merger(const json& main, const vector<json>& internals);
	json merged();

private:
	using Key = pair<int, string>;  // a module's index, a name it declares
	struct Decl { const json* stmt; const json* var; };

	vector<const json*> modules_;
	map<string, int> moduleOf_;
	vector<map<string, Decl>> decls_;
	vector<map<string, const json*>> cTypes_;  // a C type name, the cinclude naming it
	set<Key> reached_;
	set<Key> public_;  // reached without going through a private struct or a const
	vector<Key> queue_;
	bool marksPublic_ = true;
	map<pair<string, string>, vector<string>> cNames_;
	map<pair<string, string>, const json*> cIncludes_;
	vector<pair<string, string>> cOrder_;
	map<Key, string> finals_;
	const json* from_ = nullptr;  // the declaration being walked
	int fromModule_ = 0;

	const json& stmts(int m) const { return (*modules_[m])["ast"]["statements"]; }
	optional<Key> find(int m, const string& written) const;
	void mark(const Key& key);
	bool isPublic(const Key& key) const;
	void ref(json& nameNode, int m, bool rename);
	void walk(json& node, int m, bool rename);
	json declBody(const Key& key) const;
	void assignFinalNames();
};

Merger::Merger(const json& main, const vector<json>& internals)
{
	modules_.push_back(&main);
	for (auto& ast : internals) modules_.push_back(&ast);
	static const json noStmts = json::array();
	decls_.resize(modules_.size());
	cTypes_.resize(modules_.size());
	for (int m = 0; m < modules_.size(); m++) {
		moduleOf_[(*modules_[m])["original"].get<string>()] = m;
		const json& list = (*modules_[m])["ast"].contains("statements") ? stmts(m) : noStmts;
		for (auto& s : list) {
			string kind = s.value("stmt-type", "");
			if (kind == "struct-def" || kind == "enum-def" || kind == "type-alias" || kind == "const-decl")
				decls_[m][s["name"]] = {&s, nullptr};
			else if (kind == "var-decl")
				for (auto& v : s["vars"]) decls_[m][v["name"]] = {&s, &v};
			else if (kind == "cinclude" && s.contains("export-types"))
				for (auto& name : s["export-types"]) cTypes_[m][name] = &s;
		}
	}
}

optional<Merger::Key> Merger::find(int m, const string& written) const
{
	size_t dot = written.find('.');
	string alias = dot == string::npos ? "" : written.substr(0, dot);
	string name = dot == string::npos ? written : written.substr(dot + 1);
	if (alias.empty() && decls_[m].count(name)) return Key{m, name};
	if (!(*modules_[m])["ast"].contains("statements")) return nullopt;
	for (auto& s : stmts(m)) {
		if (s.value("stmt-type", "") != "import" || s.value("alias", "") != alias) continue;
		auto target = moduleOf_.find(s.value("path", ""));
		if (target == moduleOf_.end()) continue;
		if (s.contains("targets")) {
			const json& targets = s["targets"];
			if (std::find(targets.begin(), targets.end(), name) == targets.end()) continue;
		}
		auto decl = decls_[target->second].find(name);
		if (decl != decls_[target->second].end() && decl->second.stmt->value("export", false))
			return Key{target->second, name};
	}
	return nullopt;
}

void Merger::mark(const Key& key)
{
	bool isNew = reached_.insert(key).second;
	bool nowPublic = marksPublic_ && public_.insert(key).second;
	if (isNew || nowPublic) queue_.push_back(key);
}

bool Merger::isPublic(const Key& key) const
{
	return public_.count(key) && decls_[key.first].at(key.second).stmt->value("export", false);
}

void Merger::ref(json& nameNode, int m, bool rename)
{
	string written = nameNode;
	auto key = find(m, written);
	if (key) {
		if (rename) {
			nameNode = finals_.at(*key);
			return;
		}
		string kind = (*decls_[key->first].at(key->second).stmt)["stmt-type"];
		// A type is identified by its module, which the .pai, read as the main's, would change.
		if (key->first != 0 && (kind == "struct-def" || kind == "enum-def")) {
			string at = (*modules_[fromModule_])["original"].get<string>();
			if (from_ && from_->contains("loc"))
				at += ":" + to_string((*from_)["loc"][0].get<int>()) + ":" + to_string((*from_)["loc"][1].get<int>());
			throw runtime_error(at + ": error: a library's public declaration cannot use the type '" + written
				+ "' of the internal module '" + (*modules_[key->first])["original"].get<string>()
				+ "': declare it in the main source.");
		}
		mark(*key);
		return;
	}
	auto c = cTypes_[m].find(written);
	if (c == cTypes_[m].end() || rename) return;
	const json& inc = *c->second;
	if (inc["path-type"] == "src") {
		string at = (*modules_[m])["original"].get<string>() + ":" + to_string(inc["loc"][0].get<int>()) + ":"
			+ to_string(inc["loc"][1].get<int>());
		throw runtime_error(at + ": error: a library's public declaration cannot yet use a C type of the local header '"
			+ inc["path"].get<string>() + "': its reader would not find the header.");
	}
	pair<string, string> id{inc["path"], inc.value("alias", "")};
	if (!cIncludes_.count(id)) {
		cIncludes_[id] = &inc;
		cOrder_.push_back(id);
	}
	auto& names = cNames_[id];
	if (std::find(names.begin(), names.end(), written) == names.end()) names.push_back(written);
}

void Merger::walk(json& node, int m, bool rename)
{
	if (node.is_array()) {
		for (auto& child : node) walk(child, m, rename);
		return;
	}
	if (!node.is_object()) return;
	string kind = node.value("expr-type", "");
	if (kind == "id" || kind == "call") ref(node["name"], m, rename);
	if (node.contains("type-kind") && node.contains("type-name") && node["type-name"].is_string())
		ref(node["type-name"], m, rename);
	for (auto& [k, child] : node.items())
		if (child.is_structured()) walk(child, m, rename);
}

// What of a declaration names other declarations: all of it but a variable's
// initial value, of which an interface keeps only the sizes.
json Merger::declBody(const Key& key) const
{
	const Decl& d = decls_[key.first].at(key.second);
	if (!d.var) return *d.stmt;
	json body = *d.var;
	body.erase("init");
	return body;
}

// The main's names are in its types' identities and its symbols, so an
// internal module's const or alias gives way.
void Merger::assignFinalNames()
{
	set<string> taken;
	for (auto& [id, names] : cNames_) taken.insert(names.begin(), names.end());
	for (auto& key : reached_)
		if (key.first == 0) {
			taken.insert(key.second);
			finals_[key] = key.second;
		}
	for (int m = 1; m < modules_.size(); m++)
		for (auto& [name, decl] : decls_[m]) {
			Key key{m, name};
			if (!reached_.count(key) || finals_.count(key)) continue;
			finals_[key] = taken.insert(name).second ? name : "__m" + to_string(m) + "_" + name;
			taken.insert(finals_[key]);
		}
}

json Merger::merged()
{
	const json& main = *modules_[0];
	static const json noStmts = json::array();
	const json& mainStmts = main["ast"].contains("statements") ? stmts(0) : noStmts;
	for (auto& s : mainStmts) {
		string kind = s.value("stmt-type", "");
		bool isDecl = kind == "struct-def" || kind == "enum-def" || kind == "type-alias";
		if (isDecl && s.value("export", false)) mark({0, s["name"]});
	}
	json funcs = main.value("export", json::array());
	for (auto& f : funcs) {
		from_ = &f;
		walk(f, 0, false);
	}
	while (!queue_.empty()) {
		Key key = queue_.back();
		queue_.pop_back();
		from_ = decls_[key.first].at(key.second).stmt;
		fromModule_ = key.first;
		// A signature's symbol holds what an alias stands for, so an alias, even a
		// private one, hides nothing. Only a layout depends on a private struct's fields.
		const json& stmt = *decls_[key.first].at(key.second).stmt;
		string kind = stmt["stmt-type"];
		marksPublic_ = public_.count(key) && (kind == "type-alias" || isPublic(key));
		json body = declBody(key);
		walk(body, key.first, false);
	}
	assignFinalNames();
	walk(funcs, 0, true);

	json out = json::array();
	// A library imported by any module stays: its archive is linked.
	set<string> imported;
	for (int m = 0; m < modules_.size(); m++) {
		if (!(*modules_[m])["ast"].contains("statements")) continue;
		for (auto& s : stmts(m)) {
			if (s.value("stmt-type", "") != "import" || s["path-type"] == "src") continue;
			if (imported.insert(s["path"].get<string>() + " " + s.value("alias", "")).second) out.push_back(s);
		}
	}
	for (auto& id : cOrder_) {
		const json& inc = *cIncludes_.at(id);
		json c = {{"stmt-type", "cinclude"}, {"path", inc["path"]}, {"path-type", inc["path-type"]}, {"export-types", cNames_.at(id)}};
		if (inc.contains("alias")) c["alias"] = inc["alias"];
		out.push_back(move(c));
	}
	for (int m = 0; m < modules_.size(); m++) {
		if (!(*modules_[m])["ast"].contains("statements")) continue;
		for (auto& s : stmts(m)) {
			string kind = s.value("stmt-type", "");
			if (kind == "var-decl") {
				json decl = s;
				decl["vars"] = json::array();
				for (auto& v : s["vars"]) {
					Key key{m, v["name"]};
					if (!reached_.count(key)) continue;
					json var = v;
					walk(var["var-type"], m, true);
					var["name"] = finals_.at(key);
					decl["vars"].push_back(move(var));
				}
				if (!decl["vars"].empty()) out.push_back(move(decl));
			} else if (kind == "struct-def" || kind == "enum-def" || kind == "type-alias" || kind == "const-decl") {
				Key key{m, s["name"]};
				if (!reached_.count(key)) continue;
				json decl = s;
				walk(decl, m, true);
				decl["name"] = finals_.at(key);
				if (!isPublic(key)) decl.erase("export");
				out.push_back(move(decl));
			}
		}
	}
	json copyFuncs = json::array();
	for (int m = 0; m < modules_.size(); m++)
		for (auto& f : (*modules_[m])["ast"].value("functions", json::array())) {
			if (f["name"] != "->") continue;
			auto key = find(m, f["parameters"][0]["var-type"]["base-type"]["type-name"].get<string>());
			if (!key || !reached_.count(*key)) continue;
			json decl = f;
			decl.erase("block");
			walk(decl["parameters"], m, true);
			copyFuncs.push_back(move(decl));
		} // LCOV_EXCL_BR_LINE
	return {{"original", main["original"]}, {"module", main["module"]}, {"target", main["target"]},
		{"ast", {{"statements", move(out)}, {"functions", move(copyFuncs)}}}, {"export", move(funcs)}}; // LCOV_EXCL_BR_LINE
} // LCOV_EXCL_BR_LINE -- closing brace of a function with many local json/string temporaries; the branch coverpoints here are compiler-generated destructor dispatch, not source-level conditionals

} // namespace

string writeInterface(const json& ast)
{
	vector<string> libs;
	collectLinks(ast["ast"], libs);
	return Writer(ast, libs).write();
}

string writeLibraryInterface(const json& main, const vector<json>& internals)
{
	vector<string> libs;
	collectLinks(main["ast"], libs);
	for (auto& ast : internals) collectLinks(ast["ast"], libs);
	json merged = Merger(main, internals).merged();
	return Writer(merged, libs).write();
}

string writeType(const json& astType)
{
	static const json noModule = {{"original", ""}}; // LCOV_EXCL_BR_LINE
	static const vector<string> noLibs; // LCOV_EXCL_BR_LINE
	return Writer(noModule, noLibs).type(astType);
} // LCOV_EXCL_BR_LINE
