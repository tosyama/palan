/// semantic analyzer tests: modules and calls
///
/// @file moduleTests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include <fstream>
#include "../../src/common/PlnDefs.h"
#include <regex>
#include <set>
#include "../test-base/testBase.h"
#include "saTestUtil.h"

using namespace std;

TEST(sa, import_unqualified)
{
	cleanTestEnv();
	genLibSaImport();
	json jout = run_sa("../test/testdata/sa/058_import_unqualified.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& init = jout["statements"][0]["vars"][0]["init"];
	ASSERT_EQ(init["expr-type"], "call");
	ASSERT_EQ(init["func-type"], "palan");
	ASSERT_EQ(funcName(init["name"]), "square");
}

TEST(sa, import_block_scope)
{
	cleanTestEnv();
	genLibSaImport();
	json jout = run_sa("../test/testdata/sa/059_import_block_scope.pa");
	ASSERT_TRUE(jout.is_object());
	auto& block = jout["statements"][0];
	ASSERT_EQ(block["stmt-type"], "block");
	const auto& inner = block["body"][0];
	ASSERT_EQ(inner["stmt-type"], "var-decl");
	ASSERT_EQ(inner["vars"][0]["init"]["func-type"], "palan");
}

TEST(sa, import_selective)
{
	cleanTestEnv();
	genLibSaImport();
	json jout = run_sa("../test/testdata/sa/060_import_selective.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& init = jout["statements"][0]["vars"][0]["init"];
	ASSERT_EQ(init["func-type"], "palan");
	ASSERT_EQ(funcName(init["name"]), "square");
}

TEST(sa, import_alias)
{
	cleanTestEnv();
	genLibSaImport();
	json jout = run_sa("../test/testdata/sa/061_import_alias.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& a_init = jout["statements"][0]["vars"][0]["init"];
	ASSERT_EQ(a_init["expr-type"], "call");
	ASSERT_EQ(a_init["func-type"], "palan");
	ASSERT_EQ(funcName(a_init["name"]), "square");
	const auto& b_init = jout["statements"][1]["vars"][0]["init"];
	ASSERT_EQ(b_init["func-type"], "palan");
	ASSERT_EQ(funcName(b_init["name"]), "cube");
}

TEST(sa, import_alias_tapple)
{
	cleanTestEnv();
	genLibSaImport();
	json jout = run_sa("../test/testdata/sa/062_import_alias_tapple.pa");
	ASSERT_TRUE(jout.is_object());
	auto& tapple = jout["statements"][0];
	ASSERT_EQ(tapple["stmt-type"], "tapple-decl");
	ASSERT_EQ(tapple["value"]["expr-type"], "call");
	ASSERT_EQ(tapple["value"]["func-type"], "palan");
	ASSERT_EQ(funcName(tapple["value"]["name"]), "divmod");
	ASSERT_EQ(tapple["value"]["value-types"].size(), 2u);
}

TEST(sa, import_non_pa_ignored)
{
	// import "somelib.txt" — non-.pa extension → SA returns early, no error
	// Covers: imp_path.ends_with(".pa") == false → early return branch
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/091_import_non_pa.pa");
	ASSERT_TRUE(jout.is_object());
	// SA must succeed and produce output (import was silently ignored)
	ASSERT_TRUE(jout.contains("statements"));
}

static void genLibNoExport()
{
	genModuleAst("../test/testdata/sa/lib_no_export.pa");
}

TEST(sa, import_no_export_ignored)
{
	// lib_no_export.pa has no "export" section → SA returns early after parsing
	// Covers: imp_ast.contains("export") == false → early return branch
	cleanTestEnv();
	genLibNoExport();
	json jout = run_sa("../test/testdata/sa/092_import_no_export.pa");
	ASSERT_TRUE(jout.is_object());
	ASSERT_TRUE(jout.contains("statements"));
}

static void genLibSaNamedRet()
{
	genModuleAst("../test/testdata/sa/lib_sa_named_ret.pa");
}

TEST(sa, import_single_named_ret)
{
	// lib_sa_named_ret.pa: export func getVal() -> int64 ret { ... }
	// Single named return in export → SA synthesizes ret-type in import
	// Covers: funcEntry["ret-type"] = funcEntry["rets"][0]["var-type"] in sa_import
	cleanTestEnv();
	genLibSaNamedRet();
	json jout = run_sa("../test/testdata/sa/093_import_named_ret.pa");
	ASSERT_TRUE(jout.is_object());

	// int64 x = getVal() → call annotated as palan with ret-type int64
	const auto& init = jout["statements"][0]["vars"][0]["init"];
	ASSERT_EQ(init["expr-type"],  "call");
	ASSERT_EQ(init["func-type"], "palan");
	ASSERT_EQ(funcName(init["name"]),       "getVal");
	ASSERT_EQ(init["value-type"]["type-name"], "int64");
}

TEST(sa, import_absolute_path)
{
	// import "/nonexistent_lib.so" — absolute path, non-.pa extension
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/097_import_absolute_path.pa");
	ASSERT_TRUE(jout.is_object());

	// Import should be silently ignored; printf call should be present
	bool found = false;
	for (auto& s : jout["statements"])
		if (s["stmt-type"] == "expr" && s["body"]["name"] == "printf")
			{ found = true; break; }
	ASSERT_TRUE(found);
}

static void genLibSaSyscallImport()
{
	genModuleAst("../test/testdata/sa/lib_sa_syscall.pa");
}

TEST(sa, import_syscall) {
	// An exported syscall declaration's number is an unevaluated expression
	// node in ast["export"] (gen-ast doesn't fold it); sa_import must run it
	// through the same validateSyscallDecl fold that a local declaration
	// gets via preregisterFunc, or the importing call node would carry a
	// raw expression node instead of an integer.
	cleanTestEnv();
	genLibSaSyscallImport();
	json jout = run_sa("../test/testdata/sa/190_import_syscall.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& writeCall = jout["statements"][1]["vars"][0]["init"];
	ASSERT_EQ(writeCall["func-type"], "syscall");
	ASSERT_EQ(writeCall["name"], "write");
	ASSERT_EQ(writeCall["syscall-number"], 1);

	const auto& getpidCall = jout["statements"][2]["vars"][0]["init"];
	ASSERT_EQ(getpidCall["func-type"], "syscall");
	ASSERT_EQ(getpidCall["name"], "getpid");
	ASSERT_EQ(getpidCall["syscall-number"], 39);
}

TEST(sa, import_syscall_alias) {
	// sa_expr_member_call used to hardcode func-type to "palan" for any
	// non-C callee, silently flattening an aliased call to an imported
	// syscall declaration. Both paths now share applyPlnCalleeSig.
	cleanTestEnv();
	genLibSaSyscallImport();
	json jout = run_sa("../test/testdata/sa/191_import_syscall_alias.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& writeCall = jout["statements"][1]["vars"][0]["init"];
	ASSERT_EQ(writeCall["func-type"], "syscall");
	ASSERT_EQ(writeCall["name"], "write");
	ASSERT_EQ(writeCall["syscall-number"], 1);
}

static void collectCalls(const json& node, const string& name, vector<json>& out)
{
	if (node.is_object()) {
		if (node.value("expr-type", "") == "call" && funcName(node.value("name", "")) == name)
			out.push_back(node);
		for (auto& [k, v] : node.items()) collectCalls(v, name, out);
	} else if (node.is_array()) {
		for (auto& v : node) collectCalls(v, name, out);
	}
}

TEST(sa, method_call) {
	json jout = written(run_sa("../test/testdata/sa/226_method_call.pa"));
	ASSERT_TRUE(jout.is_object());
	auto calls = [&](const string& name) { vector<json> v; collectCalls(jout, name, v); return v; };

	// Every receiver is passed as a mutable borrow in the first argument.
	auto setx = calls("setx");
	ASSERT_EQ(setx.size(), 2u);
	for (auto& c : setx) {
		ASSERT_EQ(c["args"].size(), 2u);
		ASSERT_EQ(c["args"][0]["value-type"]["mutable"], true);
		ASSERT_EQ(c["args"][0]["value-type"]["base-type"]["type-name"], "Outer");
	}
	auto setv = calls("setv");
	ASSERT_EQ(setv.size(), 4u);
	for (auto& c : setv)
		ASSERT_EQ(c["args"][0]["value-type"]["mutable"], true);
	ASSERT_EQ(setv[0]["args"][0]["expr-type"], "field-access");	// o.inner.setv() inside setx
	ASSERT_EQ(setv[2]["args"][0]["expr-type"], "arr-index");	// ins[1].setv()
	ASSERT_FALSE(setv[3]["args"][0].value("addr-only", false));	// h.p.setv() loads the borrow
	auto fill = calls("fill");
	ASSERT_EQ(fill.size(), 2u);
	for (auto& c : fill) {
		ASSERT_EQ(c["args"][0]["value-type"]["mutable"], true);
		ASSERT_EQ(c["args"][0]["value-type"]["arr-size"], 3);
	}

	auto swapxy = calls("swapxy");
	ASSERT_EQ(swapxy.size(), 2u);
	ASSERT_EQ(swapxy[0]["value-types"].size(), 2u);
	ASSERT_EQ(swapxy[0]["args"][0]["name"], "o");

	auto mktime = calls("mktime");
	ASSERT_EQ(mktime.size(), 1u);
	ASSERT_EQ(mktime[0]["func-type"], "c");
	ASSERT_EQ(mktime[0]["args"][0]["name"], "t");
	ASSERT_EQ(mktime[0]["args"][0]["value-type"]["mutable"], true);
}

TEST(sa, method_call_alias) {
	cleanTestEnv();
	genLibSaImport();
	json jout = written(run_sa("../test/testdata/sa/227_method_call_alias.pa"));
	ASSERT_TRUE(jout.is_object());
	vector<json> v;
	collectCalls(jout, "square", v);
	ASSERT_EQ(v.size(), 2u);
	// The block's variable M shadows the alias M; outside it, M is the alias.
	ASSERT_EQ(v[0]["args"][0]["name"], "M");
	ASSERT_EQ(v[0]["args"][0]["value-type"]["base-type"]["type-name"], "T");
	ASSERT_EQ(v[1]["args"][0]["expr-type"], "lit-int");
}

TEST(sa, import_type)
{
	cleanTestEnv();
	genLibSaTypes();
	json jout = run_sa("../test/testdata/sa/228_import_type.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	// Laid out from the exporting module, where the non-exported Inner and
	// the cinclude'd FILE are known.
	const auto& p = stmts[0]["vars"][0]["init"];
	ASSERT_EQ(p["name"], "malloc");
	ASSERT_EQ(p["args"][0]["value"], "24");
	ASSERT_EQ(funcName(stmts[1]["vars"][0]["init"]["name"]), "__pln_alloc_arr_P9__pln_B24");
	ASSERT_EQ(stmts[2]["vars"][0]["var-type"]["type-name"], "int64");
	ASSERT_EQ(stmts[3]["vars"][0]["var-type"]["enum"], "Color");
	ASSERT_EQ(stmts[3]["vars"][0]["init"]["value"], "5");
	// getX's return type is the exporting module's non-exported alias Priv.
	const auto& x = stmts[4]["vars"][0]["init"];
	ASSERT_EQ(funcName(x["name"]), "getX");
	ASSERT_EQ(x["value-type"]["type-name"], "int32");
	ASSERT_EQ(x["args"][0]["value-type"]["base-type"]["type-name"], "Vec");
}

// The alias's size is K as its module declares it, not the importer's K.
TEST(sa, import_alias_size)
{
	cleanTestEnv();
	genModuleAst("../test/testdata/sa/lib_sa_alias_size.pa");
	json jout = run_sa("../test/testdata/sa/347_import_alias_size.pa");
	ASSERT_TRUE(jout.is_object());
	ASSERT_EQ(jout["statements"][1]["vars"][0]["init"]["value"], "3");
}

TEST(sa, import_type_alias)
{
	cleanTestEnv();
	genLibSaTypes();
	json jout = run_sa("../test/testdata/sa/229_import_type_alias.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts[0]["vars"][0]["init"]["args"][0]["value"], "24");
	ASSERT_EQ(stmts[1]["vars"][0]["var-type"]["type-name"], "int64");
	ASSERT_EQ(stmts[2]["vars"][0]["var-type"]["enum"], "Color");
	ASSERT_EQ(stmts[2]["vars"][0]["init"]["value"], "5");
	ASSERT_EQ(stmts[3]["vars"][0]["init"]["value-type"]["enum"], "Color");
	ASSERT_EQ(funcName(stmts[4]["vars"][0]["init"]["name"]), "getX");
	// A top-level const names the enumerator through the alias.
	ASSERT_EQ(stmts[5]["vars"][0]["init"]["value"], "5");
	// A module-alias call chained into a call is its first argument.
	const auto& z = stmts[6]["vars"][0]["init"];
	ASSERT_EQ(funcName(z["name"]), "id");
	ASSERT_EQ(funcName(z["args"][0]["name"]), "getX");
}

TEST(sa, import_type_selective)
{
	cleanTestEnv();
	genLibSaTypes();
	json jout = run_sa("../test/testdata/sa/230_import_type_selective.pa");
	ASSERT_TRUE(jout.is_object());
	ASSERT_EQ(funcName(jout["statements"][1]["vars"][0]["init"]["name"]), "getX");
}

// The var-decl of `name` among `stmts`: its declared type, and its malloc size if a struct.
static pair<json, string> declaredVar(const json& stmts, const string& name)
{
	for (auto& st : stmts)
		for (auto& v : st.value("vars", json::array()))
			if (v["name"] == name) {
				const json& init = v.value("init", json::object());
				return {v["var-type"], init.contains("args") ? init["args"][0]["value"].get<string>() : ""};
			}
	return {json(), ""};
}

TEST(sa, import_hidden_type_name_reuse)
{
	cleanTestEnv();
	genLibSaTypes();
	json jout = written(run_sa("../test/testdata/sa/278_import_hidden_type_name_reuse.pa"));
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	// V.Vec is lib's 24-byte Vec, Vec main's own.
	ASSERT_EQ(declaredVar(stmts, "v").second, "24");
	ASSERT_EQ(declaredVar(stmts, "w").second, "4");
	ASSERT_EQ(declaredVar(stmts, "m").first["type-name"], "int32");
	ASSERT_EQ(declaredVar(stmts, "n").first["type-name"], "int64");
	const auto& body = jout["functions"][0]["body"];
	ASSERT_EQ(declaredVar(body, "c").second, "8");
	ASSERT_EQ(declaredVar(body, "d").first["enum"], "Color");
}

TEST(sa, import_dep_type_name_reuse)
{
	cleanTestEnv();
	genLibSaTypes();
	json jout = written(run_sa("../test/testdata/sa/279_import_dep_type_name_reuse.pa"));
	ASSERT_TRUE(jout.is_object());
	// getX brings lib's Vec and Inner along, without making them writable.
	ASSERT_EQ(declaredVar(jout["statements"], "p").second, "4");
	ASSERT_EQ(declaredVar(jout["statements"], "i").second, "16");
	const auto& body = jout["functions"][0]["body"];
	ASSERT_EQ(declaredVar(body, "v").second, "24");
	ASSERT_EQ(declaredVar(body, "w").second, "4");
}

TEST(sa, import_type_diamond)
{
	cleanTestEnv();
	genLibSaTypes();
	json jout = run_sa("../test/testdata/sa/231_import_type_diamond.pa");
	ASSERT_TRUE(jout.is_object());
	ASSERT_EQ(funcName(jout["statements"][1]["body"]["name"]), "setX");
	ASSERT_EQ(funcName(jout["statements"][2]["vars"][0]["init"]["name"]), "getX");
}

TEST(sa, import_type_field)
{
	cleanTestEnv();
	genLibSaTypes();
	json jout = written(run_sa("../test/testdata/sa/232_import_type_field.pa"));
	ASSERT_TRUE(jout.is_object());
	json shape;
	for (auto& s : jout["alloc-shapes"])
		if (s.value("shape-name", "") == "S") shape = s;
	// e $Vec 24 + o Vec 8 + arr [2]$Vec 48 + c Color 4 (+4 padding) + q $V.Vec 24
	ASSERT_EQ(shape["total-size"], 112);
	ASSERT_EQ(shape["owned-fields"].size(), 1u);
	ASSERT_EQ(shape["owned-fields"][0]["name"], "o");
	ASSERT_EQ(shape["owned-fields"][0]["elem"]["name"], "__pln_B24");
}

TEST(sa, top_decl_order)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/234_top_decl_order.pa"));
	ASSERT_TRUE(jout.is_object());
	json shape;
	for (auto& s : jout["alloc-shapes"])
		if (s.value("shape-name", "") == "S") shape = s;
	// a 8 + ts 16 + v $[3]int32 12 + t 4 + u 8 + al 8 + e 4 (+4) + p 8
	ASSERT_EQ(shape["total-size"], 72);
	ASSERT_EQ(shape["owned-fields"][0]["elem"]["name"], "__pln_B4");
}

TEST(sa, import_mutual_type)
{
	cleanTestEnv();
	for (string f : {"lib_sa_mutual", "235_import_mutual_type"})
		genModuleAst("../test/testdata/sa/" + f + ".pa");
	json jout = run_sa("../test/testdata/sa/235_import_mutual_type.pa");
	ASSERT_TRUE(jout.is_object());
	// Pair: n $Node (id 4 (+4) + leaf 16 + pair 8) + ls [2]$Leaf 32
	ASSERT_EQ(jout["statements"][0]["vars"][0]["init"]["args"][0]["value"], "64");
}

TEST(sa, import_mutual_alias)
{
	// Leaf points at the importer's Node through an alias, and at a private
	// struct that embeds Node, so both resolve only after Node is settled.
	cleanTestEnv();
	for (string f : {"lib_sa_mutual_alias", "252_import_mutual_alias"})
		genModuleAst("../test/testdata/sa/" + f + ".pa");
	json jout = run_sa("../test/testdata/sa/252_import_mutual_alias.pa");
	ASSERT_TRUE(jout.is_object());
	// Node: id 4 (+4) + leaf (v 8 + up 8 + o 8)
	ASSERT_EQ(jout["statements"][0]["vars"][0]["init"]["args"][0]["value"], "32");
}

TEST(sa, import_c_enum_field)
{
	cleanTestEnv();
	genModuleAst("../test/testdata/sa/lib_sa_c_enum.pa");
	json jout = run_sa("../test/testdata/sa/251_import_c_enum_field.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& init = jout["statements"].back()["vars"][0]["init"];
	ASSERT_EQ(init["src"]["value-type"]["enum"], "Color");
}

TEST(sa, chain_call)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/249_chain_call.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts.size(), 16);

	const auto& plain = stmts[5]["body"];
	ASSERT_EQ(funcName(plain["name"]), "add");
	ASSERT_EQ(plain["args"][0]["name"], "x");
	ASSERT_EQ(plain["args"][1]["value"], "4");

	// '@' may be omitted for a '@T' parameter; a borrow is passed as it is.
	for (int i : {6, 7}) {
		const auto& arg = stmts[i]["body"]["args"][0];
		ASSERT_EQ(arg["name"], "v") << i;
		ASSERT_EQ(arg["value-type"]["mutable"], false) << i;
	}
	ASSERT_EQ(stmts[8]["body"]["args"][0]["name"], "b");
	ASSERT_EQ(stmts[9]["body"]["args"][0]["value-type"]["mutable"], true);

	const auto& temp = stmts[10]["body"]["args"][0];
	ASSERT_EQ(tempInit(temp)["expr-type"], "frame-alloc");
	ASSERT_EQ(funcName(temp["body"][1]["body"]["name"]), "mk");
	ASSERT_EQ(temp["value"]["value-type"]["mutable"], false);
	ASSERT_EQ(temp["release"].size(), 0u);

	// Return values are received into temps for the statement.
	const auto& multi = stmts[11];
	ASSERT_EQ(multi["stmt-type"], "block");
	ASSERT_EQ(multi["body"][0]["stmt-type"], "tapple-decl");
	const auto& show = multi["body"][1]["body"];
	ASSERT_EQ(funcName(show["name"]), "show");
	ASSERT_EQ(show["args"][0]["name"], multi["body"][0]["vars"][0]["var-name"]);
	ASSERT_EQ(show["args"][1]["name"], multi["body"][0]["vars"][1]["var-name"]);
	ASSERT_EQ(show["args"][2]["value"], "1");

	// A returned struct is written into a temp allocated ahead of the call,
	// borrowed for a '@T' parameter and freed after the call.
	const auto& structRet = stmts[12]["body"];
	ASSERT_EQ(structRet.size(), 4);
	string vtemp = structRet[0]["vars"][0]["name"];
	ASSERT_EQ(structRet[1]["vars"][0]["ret-dest"]["name"], vtemp);
	ASSERT_EQ(structRet[2]["body"]["args"][0]["name"], vtemp);
	ASSERT_EQ(structRet[2]["body"]["args"][0]["value-type"]["mutable"], false);
	ASSERT_EQ(structRet[3]["body"]["name"], "free");
	ASSERT_EQ(structRet[3]["body"]["args"][0]["name"], vtemp);

	const auto& twice = stmts[13]["body"];
	ASSERT_EQ(funcName(twice[0]["value"]["name"]), "divmod");
	ASSERT_EQ(twice[1]["stmt-type"], "tapple-decl");
	ASSERT_EQ(funcName(twice[1]["value"]["name"]), "swap");
	ASSERT_EQ(twice[2]["body"]["args"][0]["name"], twice[1]["vars"][0]["var-name"]);

	const auto& nested = stmts[14]["body"];
	ASSERT_EQ(funcName(nested["args"][0]["name"]), "add");
	ASSERT_EQ(nested["args"][0]["args"][0]["name"], "x");

	ASSERT_EQ(stmts[15]["stmt-type"], "tapple-decl");
	ASSERT_EQ(stmts[15]["value"]["args"][0]["value"], "9");
}

TEST(sa, chain_lit)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/250_chain_lit.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts.size(), 3);

	// A literal chained to a read-only borrow parameter is passed as '@[..]'.
	const char* callees[] = {"len2", "len2", "first"};
	for (int i = 0; i < 3; i++) {
		const auto& call = stmts[i]["body"];
		ASSERT_EQ(funcName(call["name"]), callees[i]) << i;
		const auto& temp = call["args"][0];
		ASSERT_EQ(temp["expr-type"], "temp") << i;
		ASSERT_EQ(temp["value"]["value-type"]["mutable"], false) << i;
		ASSERT_EQ(tempRelease(temp), "free") << i;
	}
}

// With two `[+]T` shapes the builtins are overloaded; a chained `[+]T`
// variable still picks its own.
TEST(sa, chain_varr_overload)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/301_chain_varr_overload.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	const auto& n = stmts[2]["vars"][0]["init"];
	ASSERT_EQ(n["expr-type"], "field-access");
	ASSERT_EQ(n["ptr-expr"]["name"], "a");
	ASSERT_EQ(funcName(stmts[3]["body"]["name"]), "__pln_push_varr_int32");
}

TEST(sa, chain_method_call)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/292_chain_method_call.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts.size(), 17);

	// The chained value comes after the receiver, '@' added for a '@T' parameter.
	const auto& plain = stmts[4]["body"];
	ASSERT_EQ(funcName(plain["name"]), "add");
	ASSERT_EQ(plain["args"][0]["name"], "acc");
	ASSERT_EQ(plain["args"][0]["value-type"]["mutable"], true);
	ASSERT_EQ(plain["args"][1]["name"], "x");
	const auto& borrowed = stmts[5]["body"]["args"][1];
	ASSERT_EQ(borrowed["name"], "v");
	ASSERT_EQ(borrowed["value-type"]["mutable"], false);

	const auto& multi = stmts[6]["body"];
	ASSERT_EQ(multi[0]["stmt-type"], "tapple-decl");
	const auto& add2 = multi[1]["body"]["args"];
	ASSERT_EQ(add2[0]["name"], "acc");
	ASSERT_EQ(add2[1]["name"], multi[0]["vars"][0]["var-name"]);
	ASSERT_EQ(add2[2]["name"], multi[0]["vars"][1]["var-name"]);
	ASSERT_EQ(add2[3]["value"], "9");

	// With a call in the value or the receiver, the value is evaluated first.
	for (int i : {7, 8}) {
		const auto& body = stmts[i]["body"];
		ASSERT_EQ(body[0]["stmt-type"], "var-decl") << i;
		ASSERT_EQ(body[1]["body"]["args"][1]["name"], body[0]["vars"][0]["name"]) << i;
	}
	ASSERT_EQ(funcName(stmts[7]["body"][0]["vars"][0]["init"]["name"]), "g");
	ASSERT_EQ(stmts[8]["body"][0]["vars"][0]["init"]["name"], "x");

	// A returned struct is written into a frame temp and borrowed for the call;
	// it owns nothing to release.
	const auto& owned = stmts[9]["body"];
	ASSERT_EQ(owned.size(), 3);
	string vtemp = owned[0]["vars"][0]["name"];
	ASSERT_EQ(owned[0]["vars"][0]["init"]["expr-type"], "frame-alloc");
	ASSERT_EQ(owned[1]["body"]["ret-dest"]["name"], vtemp);
	ASSERT_EQ(owned[2]["body"]["args"][1]["name"], vtemp);
	ASSERT_EQ(owned[2]["body"]["args"][1]["value-type"]["mutable"], false);

	ASSERT_EQ(stmts[10]["stmt-type"], "tapple-decl");
	ASSERT_EQ(stmts[10]["value"]["args"][1]["name"], "x");

	const auto& callBorrow = stmts[12]["body"];
	ASSERT_EQ(callBorrow.size(), 3);
	ASSERT_EQ(callBorrow[0]["vars"][0]["init"]["expr-type"], "frame-alloc");
	ASSERT_EQ(callBorrow[2]["body"]["args"][1]["name"], callBorrow[0]["vars"][0]["name"]);

	// A struct field is held by its address; a struct variable stays in place.
	const auto& field = stmts[13]["body"];
	ASSERT_EQ(field[0]["vars"][0]["init"]["expr-type"], "field-access");
	ASSERT_EQ(field[1]["body"]["args"][1]["name"], field[0]["vars"][0]["name"]);
	ASSERT_EQ(stmts[14]["body"].size(), 1);
	ASSERT_EQ(stmts[14]["body"][0]["body"]["args"][1]["name"], "v");

	const auto& twice = stmts[15]["body"];
	ASSERT_EQ(funcName(twice[0]["vars"][0]["init"]["name"]), "g");
	ASSERT_EQ(funcName(twice[1]["vars"][0]["init"]["name"]), "addr");
	ASSERT_EQ(funcName(twice[2]["body"]["name"]), "add");
	ASSERT_EQ(twice[2]["body"]["args"][1]["name"], twice[1]["vars"][0]["name"]);

	// A constant is left in place.
	ASSERT_EQ(stmts[16]["body"].size(), 1);
	ASSERT_EQ(stmts[16]["body"][0]["body"]["args"][1]["value"], "1");
}

TEST(sa, unexported_type_name)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/277_unexported_type_name.pa");
	ASSERT_TRUE(jout.is_object());
	// Another module may declare its own A; an importer writes B as is.
	const json& st = jout["statements"];
	string a = st[0]["vars"][0]["var-type"]["base-type"]["type-name"];
	ASSERT_TRUE(regex_match(a, regex("__pln_L[0-9a-f]{16}_A"))) << a;
	ASSERT_EQ(st[1]["vars"][0]["var-type"]["base-type"]["type-name"], "B");
	ASSERT_EQ(jout["alloc-shapes"][1]["owned-fields"][0]["elem"]["name"], a);
}

TEST(sa, block_type_scope)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/258_block_type_scope.pa");
	ASSERT_TRUE(jout.is_object());
	// The two block-local P's get distinct program-wide names; the written
	// name reaches no type.
	string out = jout.dump();
	regex internal("__pln_L[0-9a-f]{16}_[0-9]+_P\\b");
	set<string> ps;
	for (sregex_iterator it(out.begin(), out.end(), internal), end; it != end; ++it)
		ps.insert(it->str());
	ASSERT_EQ(ps.size(), 2u);
	ASSERT_EQ(out.find("\"type-name\":\"P\""), string::npos);
}

TEST(sa, block_type_before_def)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/259_block_type_before_def.pa");
	ASSERT_TRUE(jout.is_object());
}

TEST(sa, block_import_type)
{
	// A block's import makes the types writable only in that block, so a
	// later block imports them again.
	cleanTestEnv();
	genLibSaTypes();
	json jout = run_sa("../test/testdata/sa/260_block_import_type.pa");
	ASSERT_TRUE(jout.is_object());
}

// A call names a function wherever it comes from, before a type alias of the
// same name.
TEST(sa, import_func_over_alias)
{
	cleanTestEnv();
	genLibSaImport();
	json jout = run_sa("../test/testdata/sa/284_import_func_over_alias.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& init = jout["statements"][0]["vars"][0]["init"];
	ASSERT_EQ(init["expr-type"], "call");
	ASSERT_EQ(funcName(init["name"]), "square");
}

// Of several functions of one name, the one every argument fits, or the one
// every argument matches exactly.
TEST(sa, overload)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/286_overload.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	auto call = [&](size_t i) -> const json& { return stmts[i]["body"]; };
	auto retName = [&](size_t i) { return call(i)["value-type"]["type-name"].get<string>(); };
	ASSERT_EQ(retName(3), "int32");                       // f(x), int32
	ASSERT_EQ(retName(4), "int64");                       // f(y), int64
	ASSERT_EQ(retName(5), "int64");                       // f(1): a literal's default type
	ASSERT_EQ(retName(6), "int32");                       // f(int32(1))
	ASSERT_EQ(call(7)["func-type"], "c");                 // abs(x): C's abs(int)
	ASSERT_EQ(call(8)["func-type"], "palan");             // abs(-1.5)
	ASSERT_EQ(call(11)["args"][0]["value-type"]["mutable"], false);   // v -> len(): @v
	ASSERT_EQ(call(12)["args"][0]["value-type"]["base-type"], call(13)["args"][0]["value-type"]["base-type"]);
	ASSERT_EQ(funcName(call(13)["name"]), "add");                   // c.add(1): only add(@!Counter, ...)
	ASSERT_FALSE(call(13).contains("method-form"));
	ASSERT_EQ(stmts[14]["vars"][0]["init"]["expr-type"], "func-ref");
	ASSERT_EQ(stmts[14]["vars"][0]["init"]["value-type"]["base-type"]["ret-type"]["type-name"], "int32");
	ASSERT_EQ(call(15)["args"][0]["value-type"]["base-type"]["ret-type"]["type-name"], "int32");  // apply(h, 2)
	ASSERT_EQ(retName(16), "int32");                      // s(@[1, 2]): only s(@[2]int32) takes it
	ASSERT_EQ(retName(17), "int64");                      // 1 -> f()
	ASSERT_EQ(stmts[18]["stmt-type"], "tapple-decl");
	ASSERT_EQ(stmts[18]["value"]["args"][0]["value-type"]["type-name"], "int32");   // dm(x): dm(int32)
}

// Permissions count at every level; with no exact match, the one that only
// takes a writable borrow as read-only is chosen.
TEST(sa, overload_permission)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/317_overload_permission.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	auto retName = [&](size_t i) { return stmts[i]["body"]["value-type"]["type-name"].get<string>(); };
	ASSERT_EQ(retName(3), "int64");                       // f(@s): f(@[2]@!P)
	ASSERT_EQ(retName(4), "int32");                       // f(@r): f(@[2]@P)
	ASSERT_EQ(retName(5), "int64");                       // k(@x): k(@@!int32)
	const json& g = jout["functions"][2];
	ASSERT_EQ(funcName(g["name"]), "g");
	ASSERT_EQ(g["body"][0]["body"]["value-type"]["type-name"], "int64");  // f(b): f(@[2]@!P)
}

// Imported functions are candidates as well, and a module imported twice
// gives each function once.
TEST(sa, overload_import)
{
	cleanTestEnv();
	genLibSaImport();
	json jout = run_sa("../test/testdata/sa/287_overload_import.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts[0]["vars"][0]["init"]["value-type"]["type-name"], "flo64");
	ASSERT_EQ(stmts[1]["vars"][0]["init"]["value-type"]["type-name"], "int64");
	ASSERT_EQ(funcName(stmts[2]["vars"][0]["init"]["name"]), "local");
	ASSERT_EQ(stmts[3]["vars"][0]["init"]["value-type"]["type-name"], "int64");   // 3 -> square()
}

// The paths an argument is matched by: a function name, a pointer's
// permission, a literal under operators, a chained value as it is or borrowed.
TEST(sa, overload_paths)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/288_overload_paths.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	auto retName = [&](size_t i) { return stmts[i]["body"]["value-type"]["type-name"].get<string>(); };
	ASSERT_EQ(retName(3), "int32");    // app(h): the function pointer parameter
	ASSERT_EQ(retName(4), "int32");    // q(@v): only q(@Vec)
	ASSERT_EQ(retName(5), "int64");    // q(@!v): q(@!Vec) exactly
	ASSERT_EQ(retName(6), "int64");    // r(1): a literal fits no pointer
	ASSERT_EQ(retName(7), "int64");    // r(1 <<< len(@v))
	ASSERT_EQ(retName(8), "flo64");    // h(x + 1.5): flo64 by default
	ASSERT_EQ(retName(9), "int64");    // f(-(1 <<< x)): int64 by default
	ASSERT_EQ(retName(10), "int32");   // x -> f(): x as it is
	ASSERT_EQ(retName(11), "int32");   // x -> pp(): @x
	ASSERT_EQ(retName(12), "int32");   // arr -> sumA(): the array's shape
	ASSERT_EQ(retName(13), "int64");   // g2(@v, 1)
	ASSERT_EQ(stmts[14]["body"]["args"][0]["expr-type"], "temp");   // mk() -> len()
	ASSERT_EQ(stmts[15]["stmt-type"], "tapple-decl");
	ASSERT_EQ(retName(16), "int32");
	ASSERT_EQ(retName(17), "int64");   // v -> lenx(): @v is no @int32
	ASSERT_EQ(retName(18), "uint64");  // u(x + 2u): uint64 by default
	ASSERT_EQ(retName(19), "int64");   // r2(len(@v) + (1 <<< x))
}

// An alias imported twice gives each function once.
TEST(sa, overload_alias_twice)
{
	cleanTestEnv();
	genLibSaImport();
	json jout = run_sa("../test/testdata/sa/289_overload_alias_twice.pa");
	ASSERT_TRUE(jout.is_object());
	ASSERT_EQ(funcName(jout["statements"][0]["vars"][0]["init"]["name"]), "square");
	ASSERT_EQ(funcName(jout["statements"][1]["vars"][0]["init"]["name"]), "square");
}

// A function, a call to it and a func-ref to it carry the same symbol: the
// module (escaped file name), the name and the parameter types.
TEST(sa, symbol_names)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/290_symbol-names.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& fns = jout["functions"];
	ASSERT_EQ(fns[0]["name"], "_290_symbol_2dnames..show.int32");
	ASSERT_EQ(fns[1]["name"], "_290_symbol_2dnames..show.Wint64");
	ASSERT_EQ(fns[2]["name"], "_290_symbol_2dnames..none.v");
	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts[0]["body"]["name"], "_290_symbol_2dnames..show.int32");
	ASSERT_EQ(stmts[2]["body"]["name"], "_290_symbol_2dnames..show.Wint64");
	ASSERT_EQ(stmts[3]["body"]["name"], "_290_symbol_2dnames..none.v");
	ASSERT_EQ(stmts[4]["vars"][0]["init"]["expr-type"], "func-ref");
	ASSERT_EQ(stmts[4]["vars"][0]["init"]["name"], "_290_symbol_2dnames..show.int32");
}

// An inner function's symbol carries the enclosing function's; a block with
// functions is numbered among those of its function or of the top level.
TEST(sa, nested_func_symbols)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/291_nested-func-symbols.pa");
	ASSERT_TRUE(jout.is_object());
	set<string> names;
	for (auto& f : jout["functions"]) names.insert(f["name"].get<string>());
	const string m = "_291_nested_2dfunc_2dsymbols..";
	ASSERT_EQ(names, (set<string>{m + "a.v", m + "a.v..f.v", m + "a.v..f.v..g.v",
		m + "a.v..b1..k.v", m + "a.v..b2..k.v", m + "b1..k.v"}));
}

// The .pai is what a library's users compile against, so a program must come out
// the same from it as from the source it was written from.
TEST(sa, import_through_interface) {
	cleanTestEnv();
	const string lib = "../test/testdata/sa/lib_interface.pa";
	const string entry = "../test/testdata/sa/333_import_interface.pa";
	string libAst = moduleAstPath(lib);
	ASSERT_EQ(execTestCommand("palan-gen-ast " + lib + " -o " + libAst + " --interface out/lib_interface.pai"), "");
	json fromSource = run_sa(entry);
	ASSERT_TRUE(fromSource.is_object());
	ASSERT_FALSE(fromSource["statements"].empty());

	ASSERT_EQ(execTestCommand("palan-gen-ast out/lib_interface.pai -o " + libAst), "");
	string entryAst = moduleAstPath(entry);
	ASSERT_EQ(execTestCommand("palan-gen-ast " + entry + " -o " + entryAst), "");
	ASSERT_EQ(execTestCommand("palan-sa " + entryAst + " -o out/from_pai.sa.json"), "");
	ifstream f("out/from_pai.sa.json");
	ASSERT_EQ(json::parse(f), fromSource);
}

TEST(sa_error, library_import_without_its_location) {
	cleanTestEnv();
	ofstream("out/m.pa") << "import <p/z>;\n";
	ASSERT_EQ(execTestCommand("palan-gen-ast out/m.pa -o out/m.json"), "");
	string out = execTestCommand("palan-sa out/m.json -o out/m.sa.json");
	ASSERT_NE(out.find("Could not open import file 'p/z'."), string::npos) << out;
}

TEST(sa_error, interface_as_program) {
	cleanTestEnv();
	ofstream("out/m.pai") << "built \"" PALAN_VERSION "\";\ntarget \"" PALAN_TARGET "\";\nexport func f();\n";
	ASSERT_EQ(execTestCommand("palan-gen-ast out/m.pai -o out/m.json"), "");
	string out = execTestCommand("palan-sa out/m.json -o out/m.sa.json");
	ASSERT_NE(out.find("is an interface, which has no code to compile."), string::npos) << out;
}
