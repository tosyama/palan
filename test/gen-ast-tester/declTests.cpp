/// gen-ast tests: declarations, types and C interop
///
/// @file declTests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <map>
#include <vector>
#include "../../src/common/PlnDefs.h"
#include "../test-base/testBase.h"
#include "genAstTestUtil.h"

using namespace std;
namespace fs = std::filesystem;

TEST(gen_ast, struct_def) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/022_struct_def.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 1);

	// type Point { int64 x; int64 y; } → struct-def
	const auto& s = stmts[0];
	ASSERT_EQ(s["stmt-type"], "struct-def");
	ASSERT_EQ(s["name"], "Point");
	ASSERT_EQ(s["fields"].size(), 2);
	ASSERT_EQ(s["fields"][0]["name"], "x");
	ASSERT_EQ(s["fields"][0]["var-type"]["type-kind"], "prim");
	ASSERT_EQ(s["fields"][0]["var-type"]["type-name"], "int64");
	ASSERT_EQ(s["fields"][1]["name"], "y");
	ASSERT_EQ(s["fields"][1]["var-type"]["type-name"], "int64");
}

TEST(gen_ast, struct_field_init) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/157_field_init.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& fields = jout["ast"]["statements"][0]["fields"];
	ASSERT_FALSE(fields[0].contains("init"));
	ASSERT_EQ(fields[1]["init"]["expr-type"], "add");
	ASSERT_EQ(fields[2]["init"]["expr-type"], "arr-lit");
	ASSERT_EQ(fields[2]["init"]["items"].size(), 1u);
}

TEST(gen_ast, type_alias) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/034_type_alias.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 1);

	// type MyInt = int64; → type-alias
	const auto& s = stmts[0];
	ASSERT_EQ(s["stmt-type"], "type-alias");
	ASSERT_EQ(s["name"], "MyInt");
	ASSERT_EQ(s["type"]["type-kind"], "prim");
	ASSERT_EQ(s["type"]["type-name"], "int64");
}

TEST(gen_ast, opaque_type) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/164_opaque.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts[1]["stmt-type"], "opaque-def");
	ASSERT_EQ(stmts[1]["name"], "Handle");
	ASSERT_EQ(stmts[1]["contents"]["type-name"], "Inner");
	ASSERT_TRUE(stmts[1]["export"].get<bool>());
	ASSERT_EQ(stmts[2]["contents"]["specifier"], "variable");
	ASSERT_FALSE(stmts[2].contains("export"));
	ASSERT_TRUE(jout["ast"]["functions"][0]["release"].get<bool>());
}

TEST(gen_ast, const_decl) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/102_const_decl.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 1);

	// const MAX = 100; → const-decl
	const auto& s = stmts[0];
	ASSERT_EQ(s["stmt-type"], "const-decl");
	ASSERT_EQ(s["name"], "MAX");
	ASSERT_EQ(s["value"]["expr-type"], "lit-int");
	ASSERT_EQ(s["value"]["value"], "100");
}

TEST(gen_ast, field_access) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/023_field_access.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 2);

	// p.x → field-access expr
	const auto& fa = stmts[0]["body"];
	ASSERT_EQ(fa["expr-type"], "field-access");
	ASSERT_EQ(fa["object"]["expr-type"], "id");
	ASSERT_EQ(fa["object"]["name"], "p");
	ASSERT_EQ(fa["field"], "x");

	// 10 -> p.x → field-assign
	const auto& as = stmts[1];
	ASSERT_EQ(as["stmt-type"], "field-assign");
	ASSERT_EQ(as["object"]["expr-type"], "id");
	ASSERT_EQ(as["object"]["name"], "p");
	ASSERT_EQ(as["field"], "x");
	ASSERT_EQ(as["value"]["expr-type"], "lit-int");
	ASSERT_EQ(as["value"]["value"], "10");
}

TEST(gen_ast, embed_struct_field) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/024_embed_struct_field.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 2);

	// type Point { int64 x; int64 y; } — first struct parses correctly
	ASSERT_EQ(stmts[0]["name"], "Point");
	ASSERT_EQ(stmts[0]["fields"].size(), 2);

	// type Line { $Point a; $Point b; }
	const auto& s = stmts[1];
	ASSERT_EQ(s["stmt-type"], "struct-def");
	ASSERT_EQ(s["name"], "Line");
	ASSERT_EQ(s["fields"].size(), 2);
	ASSERT_EQ(s["fields"][0]["name"], "a");
	ASSERT_EQ(s["fields"][0]["var-type"]["type-kind"], "embed");
	ASSERT_EQ(s["fields"][0]["var-type"]["base-type"]["type-kind"], "prim");
	ASSERT_EQ(s["fields"][0]["var-type"]["base-type"]["type-name"], "Point");
	ASSERT_EQ(s["fields"][1]["name"], "b");
	ASSERT_EQ(s["fields"][1]["var-type"]["type-kind"], "embed");
	ASSERT_EQ(s["fields"][1]["var-type"]["base-type"]["type-name"], "Point");
}

TEST(gen_ast, owned_struct_field) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/025_owned_struct_field.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 2);

	// type Point { int64 x; int64 y; } — first struct parses correctly
	ASSERT_EQ(stmts[0]["name"], "Point");
	ASSERT_EQ(stmts[0]["fields"].size(), 2);

	// type Rect { Point tl; Point br; }
	const auto& s = stmts[1];
	ASSERT_EQ(s["stmt-type"], "struct-def");
	ASSERT_EQ(s["name"], "Rect");
	ASSERT_EQ(s["fields"].size(), 2);
	ASSERT_EQ(s["fields"][0]["name"], "tl");
	ASSERT_EQ(s["fields"][0]["var-type"]["type-kind"], "prim");
	ASSERT_EQ(s["fields"][0]["var-type"]["type-name"], "Point");
	ASSERT_EQ(s["fields"][1]["name"], "br");
	ASSERT_EQ(s["fields"][1]["var-type"]["type-kind"], "prim");
	ASSERT_EQ(s["fields"][1]["var-type"]["type-name"], "Point");
}

TEST(gen_ast, ptr_struct_field) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/026_ptr_struct_field.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 1);

	// type Node { int64 val; @Node next; }
	const auto& s = stmts[0];
	ASSERT_EQ(s["fields"].size(), 2);
	ASSERT_EQ(s["fields"][1]["name"], "next");
	ASSERT_EQ(s["fields"][1]["var-type"]["type-kind"], "pntr");
	ASSERT_FALSE(s["fields"][1]["var-type"].value("mutable", false));
	ASSERT_EQ(s["fields"][1]["var-type"]["base-type"]["type-kind"], "prim");
	ASSERT_EQ(s["fields"][1]["var-type"]["base-type"]["type-name"], "Node");
}

TEST(gen_ast, mutable_ptr_struct_field) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/027_mutable_ptr_struct_field.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 1);

	// type Node { int64 val; @!Node next; }
	const auto& s = stmts[0];
	ASSERT_EQ(s["fields"].size(), 2);
	ASSERT_EQ(s["fields"][1]["name"], "next");
	ASSERT_EQ(s["fields"][1]["var-type"]["type-kind"], "pntr");
	ASSERT_EQ(s["fields"][1]["var-type"]["mutable"], true);
	ASSERT_EQ(s["fields"][1]["var-type"]["base-type"]["type-name"], "Node");
}

TEST(gen_ast, write_field_arr_index) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/031_write_field_arr_index.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 1);

	// 10 -> s.f[0]; → arr-assign, target.array is a field-access node (not a crash)
	const auto& s = stmts[0];
	ASSERT_EQ(s["stmt-type"], "arr-assign");
	const auto& target = s["target"];
	ASSERT_EQ(target["expr-type"], "arr-index");
	ASSERT_EQ(target["array"]["expr-type"], "field-access");
	ASSERT_EQ(target["array"]["field"], "f");
	ASSERT_EQ(target["array"]["object"]["expr-type"], "id");
	ASSERT_EQ(target["array"]["object"]["name"], "s");
	ASSERT_EQ(target["index"]["value"], "0");
}

TEST(gen_ast, write_nested_field_arr_index) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/032_write_nested_field_arr_index.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 1);

	// 10 -> s.f[0].sub; → field-assign, object is an arr-index expr node
	// whose array is a field-access node
	const auto& s = stmts[0];
	ASSERT_EQ(s["stmt-type"], "field-assign");
	ASSERT_EQ(s["field"], "sub");
	const auto& obj = s["object"];
	ASSERT_EQ(obj["expr-type"], "arr-index");
	ASSERT_EQ(obj["array"]["expr-type"], "field-access");
	ASSERT_EQ(obj["array"]["field"], "f");
	ASSERT_EQ(obj["array"]["object"]["name"], "s");
}

TEST(gen_ast, write_arr_index_regression) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/033_write_arr_index_regression.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 3);

	// 10 -> pts[0]; → var-based arr-index, unchanged shape
	ASSERT_EQ(stmts[0]["stmt-type"], "arr-assign");
	ASSERT_EQ(stmts[0]["target"]["array"]["expr-type"], "id");
	ASSERT_EQ(stmts[0]["target"]["array"]["name"], "pts");

	// 20 -> pts[0].x; → field-assign over an arr-index object, array unchanged
	ASSERT_EQ(stmts[1]["stmt-type"], "field-assign");
	ASSERT_EQ(stmts[1]["object"]["expr-type"], "arr-index");
	ASSERT_EQ(stmts[1]["object"]["array"]["expr-type"], "id");
	ASSERT_EQ(stmts[1]["object"]["array"]["name"], "pts");

	// 30 -> mat[0][1]; → nested arr-index (2D), unchanged shape
	ASSERT_EQ(stmts[2]["stmt-type"], "arr-assign");
	ASSERT_EQ(stmts[2]["target"]["array"]["expr-type"], "arr-index");
	ASSERT_EQ(stmts[2]["target"]["array"]["array"]["expr-type"], "id");
	ASSERT_EQ(stmts[2]["target"]["array"]["array"]["name"], "mat");
}

TEST(gen_ast, arr_at_struct) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/028_arr_at_struct.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 2);

	// [4]@Point rpts;
	const auto& decl = stmts[1];
	ASSERT_EQ(decl["stmt-type"], "var-decl");
	const auto& vt = decl["vars"][0]["var-type"];
	ASSERT_EQ(vt["type-kind"], "arr");
	ASSERT_EQ(vt["specifier"], "raw");
	ASSERT_EQ(vt["size-expr"]["value"], "4");
	ASSERT_EQ(vt["base-type"]["type-kind"], "pntr");
	ASSERT_FALSE(vt["base-type"].value("mutable", false));
	ASSERT_EQ(vt["base-type"]["base-type"]["type-kind"], "prim");
	ASSERT_EQ(vt["base-type"]["base-type"]["type-name"], "Point");
}

TEST(gen_ast, arr_at_bang_struct) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/029_arr_at_bang_struct.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 2);

	// [4]@!Point wpts;
	const auto& decl = stmts[1];
	ASSERT_EQ(decl["stmt-type"], "var-decl");
	const auto& vt = decl["vars"][0]["var-type"];
	ASSERT_EQ(vt["type-kind"], "arr");
	ASSERT_EQ(vt["specifier"], "raw");
	ASSERT_EQ(vt["size-expr"]["value"], "4");
	ASSERT_EQ(vt["base-type"]["type-kind"], "pntr");
	ASSERT_EQ(vt["base-type"]["mutable"], true);
	ASSERT_EQ(vt["base-type"]["base-type"]["type-kind"], "prim");
	ASSERT_EQ(vt["base-type"]["base-type"]["type-name"], "Point");
}

// `[...]E` is an array type of its own, written only last among the
// parameters of a function or a function pointer type.
TEST(gen_ast, variadic_param) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/160_variadic.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& funcs = jout["ast"]["functions"];
	const json elem = {{"type-kind","pntr"},{"mutable",false},{"base-type",{{"type-kind","arr"},
		{"specifier","raw"},{"size-expr",nullptr},{"base-type",{{"type-kind","prim"},{"type-name","uint8"}}}}}};
	ASSERT_EQ(funcs[0]["parameters"][1]["name"], "xs");
	ASSERT_EQ(funcs[0]["parameters"][1]["var-type"],
	          json({{"type-kind","arr"},{"specifier","variadic"},{"size-expr",nullptr},{"base-type",elem}}));
	ASSERT_EQ(funcs[1]["parameters"].size(), 1);
	const auto& fp = jout["ast"]["statements"][0]["vars"][0]["var-type"]["base-type"];
	ASSERT_EQ(fp["parameters"][1]["var-type"]["specifier"], "variadic");
	ASSERT_EQ(fp["parameters"][1]["var-type"]["base-type"]["type-name"], "int64");
}

TEST(gen_ast, spread_arg) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/168_spread_arg.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& sts = jout["ast"]["statements"];
	const auto& copy = sts[0]["body"]["args"][1];
	ASSERT_EQ(copy["expr-type"], "spread");
	ASSERT_EQ(copy["value"]["name"], "a");
	ASSERT_EQ(copy["loc"], json({1, 8, 1, 12}));
	const auto& given = sts[1]["body"]["args"][0];
	ASSERT_EQ(given["expr-type"], "spread");
	ASSERT_EQ(given["value"]["expr-type"], "transfer");
	ASSERT_EQ(sts[2]["body"]["args"][0]["expr-type"], "spread");
	ASSERT_EQ(sts[3]["body"]["args"][1]["expr-type"], "spread");
	ASSERT_EQ(sts[4]["body"]["call"]["args"][0]["expr-type"], "spread");
}

TEST(gen_ast, func_arr_at_struct_param) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/030_func_arr_at_struct_param.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& funcs = jout["ast"]["functions"];
	ASSERT_EQ(funcs.size(), 1);

	// func f([]@!Point pts, int64 n) { }
	const auto& params = funcs[0]["parameters"];
	ASSERT_EQ(params.size(), 2);
	const auto& vt = params[0]["var-type"];
	ASSERT_EQ(vt["type-kind"], "arr");
	ASSERT_EQ(vt["specifier"], "raw");
	ASSERT_TRUE(vt["size-expr"].is_null());
	ASSERT_EQ(vt["base-type"]["type-kind"], "pntr");
	ASSERT_EQ(vt["base-type"]["mutable"], true);
	ASSERT_EQ(vt["base-type"]["base-type"]["type-kind"], "prim");
	ASSERT_EQ(vt["base-type"]["base-type"]["type-name"], "Point");
}

TEST(gen_ast, cinclude_local_header) {
	cleanTestEnv();
	// palan-gen-ast runs from build/, while the .pa and .h fixtures live under
	// test/testdata/gen-ast/ - this only passes if the quoted local header path is
	// resolved relative to the source file, not the process cwd.
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/035_cinclude_local_header.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	bool found_add_one = false;
	for (auto& stmt : jout["ast"]["statements"]) {
		if (stmt["stmt-type"] != "cinclude") continue;
		for (auto& f : stmt["functions"]) {
			if (f["name"] == "add_one" && f["func-type"] == "c") {
				found_add_one = true;
				break;
			}
		}
		if (found_add_one) break;
	}
	ASSERT_TRUE(found_add_one);
}

TEST(gen_ast, import_path_absolute) {
	cleanTestEnv();
	fs::create_directories("out/d");
	ofstream("out/d/h.h") << "";
	ofstream("out/d/m.pa") << "import \"../x.pa\";\nimport a from \"./sub/../y.txt\";\nimport <p/z>;\ncinclude \"h.h\";\n";
	string output = execTestCommand("palan-gen-ast out/d/m.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	string out = fs::weakly_canonical("out").string();

	ASSERT_EQ(jout["import"][0]["path"], out + "/x.pa");
	ASSERT_EQ(jout["import"][1]["path"], out + "/d/y.txt");
	ASSERT_EQ(jout["import"][2]["path"], "p/z");
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts[0]["path"], out + "/x.pa");
	ASSERT_EQ(stmts[1]["path"], out + "/d/y.txt");
	ASSERT_EQ(stmts[2]["path"], "p/z");
	ASSERT_EQ(stmts[3]["path"], "h.h");
}

TEST(gen_ast, cinclude_same_line) {
	cleanTestEnv();
	// A greedy <...> rule would read "stdio.h>; cinclude <stdlib.h" as one path.
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/143_cinclude_same_line.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	vector<string> paths;
	bool found_printf = false, found_malloc = false;
	for (auto& stmt : jout["ast"]["statements"]) {
		if (stmt["stmt-type"] != "cinclude") continue;
		paths.push_back(stmt["path"]);
		for (auto& f : stmt["functions"]) {
			if (f["name"] == "printf") found_printf = true;
			if (f["name"] == "malloc") found_malloc = true;
		}
	}
	ASSERT_EQ(paths, (vector<string>{"stdio.h", "stdlib.h"}));
	ASSERT_TRUE(found_printf);
	ASSERT_TRUE(found_malloc);
}

TEST(gen_ast, expr_rooted_access) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/144_expr_rooted_access.pa");
	ASSERT_TRUE(checkerr(output));
	json stmts = json::parse(output)["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 5);

	json a = stmts[0]["vars"][0]["init"];
	ASSERT_EQ(a["expr-type"], "field-access");
	ASSERT_EQ(a["object"]["expr-type"], "call");

	json b = stmts[1]["vars"][0]["init"];
	ASSERT_EQ(b["expr-type"], "arr-index");
	ASSERT_EQ(b["array"]["expr-type"], "call");

	json c = stmts[2]["vars"][0]["init"];
	ASSERT_EQ(c["expr-type"], "field-access");
	ASSERT_EQ(c["object"]["expr-type"], "member-call");
	ASSERT_EQ(c["object"]["object"]["expr-type"], "call");

	ASSERT_EQ(stmts[3]["stmt-type"], "arr-assign");
	ASSERT_EQ(stmts[3]["target"]["array"]["expr-type"], "id");

	ASSERT_EQ(stmts[4]["stmt-type"], "field-assign");
	ASSERT_EQ(stmts[4]["object"]["expr-type"], "call");
}

TEST(gen_ast, cinclude_constant) {
	cleanTestEnv();
	// c2ast exports object-like macro constants into ast.constants; gen-ast
	// registers them into an internal macro table (not carried onto the
	// cinclude statement) and folds later ANSWER references -- an array
	// size-expr and a binary-op operand -- into lit-int nodes. An unsuffixed
	// macro stays untyped, like a source literal.
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/103_cinclude_constant.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	for (auto& stmt : jout["ast"]["statements"]) {
		if (stmt["stmt-type"] != "cinclude") continue;
		ASSERT_FALSE(stmt.contains("constants"));
	}

	auto& body = jout["ast"]["functions"][0]["block"]["body"];
	auto& size_expr = body[0]["vars"][0]["var-type"]["size-expr"];
	ASSERT_EQ(size_expr["expr-type"], "lit-int");
	ASSERT_EQ(size_expr["value"], "42");
	ASSERT_FALSE(size_expr.contains("value-type"));

	auto& add_left = body[1]["values"][0]["left"];
	ASSERT_EQ(add_left["expr-type"], "lit-int");
	ASSERT_EQ(add_left["value"], "42");
	ASSERT_FALSE(add_left.contains("value-type"));
}

TEST(gen_ast, cinclude_unexported_macro) {
	cleanTestEnv();
	// Only references after the cinclude are marked, and a later header's
	// redefinition as a constant replaces the mark from there on.
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/140_cinclude_unexported_macro.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	auto& stmts = jout["ast"]["statements"];

	ASSERT_FALSE(stmts[0]["vars"][0]["init"].contains("c-macro"));
	ASSERT_EQ(stmts[2]["vars"][0]["init"]["left"]["c-macro"], "not-constant");

	auto& after = stmts[4]["vars"][0]["init"];
	ASSERT_EQ(after["left"]["expr-type"], "lit-int");
	ASSERT_EQ(after["left"]["value"], "7");
	ASSERT_EQ(after["right"]["c-macro"], "function-like");
	ASSERT_EQ(after["right"]["c-macro-form"], "?:");
	ASSERT_EQ(after["right"]["args"][0]["value"], "7");

	// An expandable definition is palan-sa's: its call is not marked, and the
	// cinclude redefining it as a constant ends it.
	ASSERT_EQ(stmts[1]["expandables"][0]["name"], "EX");
	ASSERT_FALSE(stmts[2]["vars"][0]["init"]["right"].contains("c-macro"));
	ASSERT_EQ(stmts[3]["ended-macros"], json::array({"EX"}));
}

TEST(gen_ast, cinclude_macro_state) {
	cleanTestEnv();
	// b.h is preprocessed with a.h's macros, as in one C translation unit.
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/141_cinclude_macro_state.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	auto& stmts = jout["ast"]["statements"];

	ASSERT_EQ(stmts[1]["vars"][0]["init"]["value"], "3");
	ASSERT_EQ(stmts[3]["vars"][0]["init"]["value"], "5");
	ASSERT_EQ(stmts[4]["vars"][0]["init"]["value"], "1");
	ASSERT_EQ(stmts[5]["vars"][0]["init"]["expr-type"], "id");
}

TEST(gen_ast, cinclude_block_state) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/166_cinclude_block_state.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	auto& stmts = jout["ast"]["statements"];
	auto& outer = stmts[1]["body"];

	ASSERT_EQ(stmts[0]["vars"][0]["init"]["expr-type"], "id");
	ASSERT_EQ(outer[1]["body"][1]["vars"][0]["init"]["value"], "7");
	ASSERT_EQ(outer[2]["vars"][0]["init"]["value"], "3");
	ASSERT_EQ(outer[3]["vars"][0]["init"]["expr-type"], "id");
	ASSERT_EQ(stmts[2]["vars"][0]["init"]["expr-type"], "id");
	ASSERT_EQ(stmts[3]["functions"][0]["name"], "block_state_f");
	ASSERT_EQ(stmts[4]["vars"][0]["init"]["value"], "7");
}

TEST(gen_ast, cinclude_global) {
	cleanTestEnv();
	// c2ast exports file-scope "extern" object declarations into
	// ast.globals; this verifies PlnParser.yy's cinclude rule lifts that
	// array onto the cinclude statement's "globals" field, same as it
	// already does for functions/constants/structs.
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/108_cinclude_global.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	bool found_counter = false;
	for (auto& stmt : jout["ast"]["statements"]) {
		if (stmt["stmt-type"] != "cinclude") continue;
		ASSERT_TRUE(stmt.contains("globals"));
		for (auto& g : stmt["globals"]) {
			if (g["name"] == "shared_counter") {
				ASSERT_EQ(g["var-type"]["type-kind"], "prim");
				ASSERT_EQ(g["var-type"]["type-name"], "int32");
				found_counter = true;
			}
		}
	}
	ASSERT_TRUE(found_counter);
}

TEST(gen_ast, cinclude_global_only) {
	// A header with globals but no functions used to leave "functions": null
	// on the cinclude statement (the lift unconditionally moved
	// c_ast["ast"]["functions"], unlike the contains()-guarded
	// constants/structs/globals). Now it must be omitted, same as the other
	// three fields when absent.
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/110_cinclude_global_only.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	bool found_cinclude = false;
	for (auto& stmt : jout["ast"]["statements"]) {
		if (stmt["stmt-type"] != "cinclude") continue;
		found_cinclude = true;
		ASSERT_FALSE(stmt.contains("functions"));
		ASSERT_TRUE(stmt.contains("globals"));
	}
	ASSERT_TRUE(found_cinclude);
}

TEST(gen_ast, cinclude_typedefs) {
	// c2ast's ast.typedefs section is lifted onto the cinclude statement,
	// same contains()-guarded pattern as functions/constants/structs/globals.
	// Header declares zero functions (a my_size_t typedef only), so this
	// also proves the lift is independent of the "functions" section's
	// presence.
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/111_cinclude_typedefs.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	bool found_typedef = false;
	for (auto& stmt : jout["ast"]["statements"]) {
		if (stmt["stmt-type"] != "cinclude") continue;
		ASSERT_TRUE(stmt.contains("typedefs"));
		for (auto& td : stmt["typedefs"]) {
			if (td["name"] == "my_size_t") {
				ASSERT_EQ(td["var-type"]["type-kind"], "prim");
				ASSERT_EQ(td["var-type"]["type-name"], "int32");
				found_typedef = true;
			}
		}
	}
	ASSERT_TRUE(found_typedef);
}

TEST(gen_ast, cinclude_no_decl) {
	// A header that declares nothing (e.g. stdarg.h) makes palan-c2ast exit 0
	// with a JSON object that has no "ast" key. The cinclude fatal-error
	// check must not mistake this no-op for a c2ast failure.
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/109_cinclude_no_decl.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	bool found_cinclude = false;
	for (auto& stmt : jout["ast"]["statements"]) {
		if (stmt["stmt-type"] != "cinclude") continue;
		found_cinclude = true;
		ASSERT_FALSE(stmt.contains("functions"));
	}
	ASSERT_TRUE(found_cinclude);
}

TEST(gen_ast, void_ptr_type) {
	// @void/@!void spell C's void* directly. Same file
	// also carries a bare "(void, int64 z) = myFunc(x);" tapple-decl slot
	// discard (KW_VOID's original, sole use) to pin that it still parses --
	// the two new type_expr productions must not shadow it.
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/037_void_ptr_type.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 4);

	// @void p;
	const auto& p = stmts[0]["vars"][0]["var-type"];
	ASSERT_EQ(p["type-kind"], "pntr");
	ASSERT_EQ(p["mutable"], false);
	ASSERT_EQ(p["base-type"]["type-kind"], "prim");
	ASSERT_EQ(p["base-type"]["type-name"], "void");

	// @!void q;
	const auto& q = stmts[1]["vars"][0]["var-type"];
	ASSERT_EQ(q["type-kind"], "pntr");
	ASSERT_EQ(q["mutable"], true);
	ASSERT_EQ(q["base-type"]["type-kind"], "prim");
	ASSERT_EQ(q["base-type"]["type-name"], "void");

	// @!@!void pp;
	const auto& pp = stmts[2]["vars"][0]["var-type"];
	ASSERT_EQ(pp["type-kind"], "pntr");
	ASSERT_EQ(pp["mutable"], true);
	ASSERT_EQ(pp["base-type"]["type-kind"], "pntr");
	ASSERT_EQ(pp["base-type"]["mutable"], true);
	ASSERT_EQ(pp["base-type"]["base-type"]["type-name"], "void");

	// (void, int64 z) = myFunc(x); -- the void slot is discarded, not
	// present in "vars"
	ASSERT_EQ(stmts[3]["stmt-type"], "tapple-decl");
	ASSERT_EQ(stmts[3]["vars"].size(), 1);
	ASSERT_EQ(stmts[3]["vars"][0]["var-name"], "z");
}

TEST(gen_ast, cinclude_link) {
	// An optional `link` clause on cinclude collects library names into
	// "libs" (string array), omitted when absent -- same contains()-guarded
	// convention as functions/constants/structs/globals/typedefs.
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/112_cinclude_link.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 4);

	ASSERT_EQ(stmts[0]["stmt-type"], "cinclude");
	ASSERT_EQ(stmts[0]["libs"], json::array({"m"}));
	ASSERT_FALSE(stmts[0].contains("alias"));

	ASSERT_EQ(stmts[1]["stmt-type"], "cinclude");
	ASSERT_EQ(stmts[1]["libs"], json::array({"m", "rt"}));

	ASSERT_EQ(stmts[2]["stmt-type"], "cinclude");
	ASSERT_EQ(stmts[2]["alias"], "M");
	ASSERT_EQ(stmts[2]["libs"], json::array({"m"}));

	ASSERT_EQ(stmts[3]["stmt-type"], "cinclude");
	ASSERT_FALSE(stmts[3].contains("libs"));
}

TEST(gen_ast, cinclude_link_obj) {
	// A ".o" entry becomes an absolute path relative to the source file's
	// directory, so downstream tools never depend on the process cwd.
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/139_cinclude_link_obj.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 2);

	string dir = std::filesystem::absolute("../test/testdata/gen-ast").lexically_normal().string();
	ASSERT_EQ(stmts[0]["libs"], json::array({"m"}));
	ASSERT_EQ(stmts[0]["objs"], json::array({dir + "/abi.o", dir + "/lib/x.o"}));

	ASSERT_FALSE(stmts[1].contains("libs"));
	ASSERT_EQ(stmts[1]["objs"], json::array({"/opt/y.o"}));
}

TEST(gen_ast, link_as_identifier) {
	// "link" is a context-dependent keyword recognized
	// only as `KW_CINCLUDE import_path import_as ID link_libs` where that ID
	// spells "link". Everywhere else -- as a called C function (unistd.h
	// really exports one named link()) or as a cinclude alias -- it must
	// keep parsing as a plain identifier.
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/113_link_identifier.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	auto& stmts = jout["ast"]["statements"];

	bool found_link_func = false;
	bool found_link_alias = false;
	for (auto& stmt : stmts) {
		if (stmt["stmt-type"] != "cinclude") continue;
		if (stmt.value("alias", "") == "link") {
			found_link_alias = true;
			continue;
		}
		for (auto& f : stmt["functions"]) {
			if (f["name"] == "link") found_link_func = true;
		}
	}
	ASSERT_TRUE(found_link_func);
	ASSERT_TRUE(found_link_alias);
}

TEST(gen_ast, syscall_decl) {
	// A "syscall" prototype declaration parses like func_def but carries an
	// unevaluated "syscall-number" expression node (SA evaluates it and
	// diagnoses signature constraints).
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/114_syscall_decl.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	auto& funcs = jout["ast"]["functions"];
	ASSERT_EQ(funcs.size(), 4u);  // write, exit, foo, main

	bool found_write = false, found_exit = false, found_foo = false;
	for (auto& f : funcs) {
		if (f["name"] == "write") {
			ASSERT_EQ(f["func-type"], "syscall");
			ASSERT_EQ(f["parameters"].size(), 3u);
			ASSERT_TRUE(f.contains("ret-type"));
			ASSERT_EQ(f["ret-type"]["type-name"], "int64");
			ASSERT_EQ(f["syscall-number"]["expr-type"], "id");
			ASSERT_EQ(f["syscall-number"]["name"], "SYS_write");
			ASSERT_TRUE(f.value("export", false));
			found_write = true;
		}
		if (f["name"] == "exit") {
			ASSERT_EQ(f["func-type"], "syscall");
			ASSERT_FALSE(f.contains("ret-type"));
			ASSERT_FALSE(f.contains("rets"));
			ASSERT_EQ(f["syscall-number"]["expr-type"], "id");
			ASSERT_EQ(f["syscall-number"]["name"], "SYS_exit");
			found_exit = true;
		}
		if (f["name"] == "foo") {
			ASSERT_EQ(f["func-type"], "syscall");
			ASSERT_EQ(f["parameters"].size(), 0u);
			ASSERT_EQ(f["syscall-number"]["expr-type"], "lit-int");
			ASSERT_EQ(f["syscall-number"]["value"], "42");
			found_foo = true;
		}
	}
	ASSERT_TRUE(found_write);
	ASSERT_TRUE(found_exit);
	ASSERT_TRUE(found_foo);

	// root "export" array carries syscall-number too (no symbol to link
	// against; the importer needs the number itself).
	ASSERT_TRUE(jout.contains("export"));
	ASSERT_EQ(jout["export"].size(), 1u);
	ASSERT_EQ(jout["export"][0]["name"], "write");
	ASSERT_EQ(jout["export"][0]["func-type"], "syscall");
	ASSERT_TRUE(jout["export"][0].contains("syscall-number"));

	// nested declaration: lands in the enclosing block's "functions", not
	// the global ast["ast"]["functions"] list.
	bool found_main = false;
	for (auto& f : funcs) {
		if (f["name"] != "main") continue;
		found_main = true;
		auto& nested = f["block"]["functions"];
		ASSERT_EQ(nested.size(), 1u);
		ASSERT_EQ(nested[0]["name"], "getpid");
		ASSERT_EQ(nested[0]["func-type"], "syscall");
	}
	ASSERT_TRUE(found_main);
}

TEST(gen_ast, macro_fold) {
	// Macro-constant references fold into lit-int both in a
	// syscall-number position and as a pointer-typed initializer, in both
	// ast.functions and the deep-copied ast.export.
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/116_macro_fold.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	for (auto& f : jout["ast"]["functions"]) {
		if (f["name"] != "foo") continue;
		ASSERT_EQ(f["syscall-number"]["expr-type"], "lit-int");
		ASSERT_EQ(f["syscall-number"]["value"], "5");
		ASSERT_FALSE(f["syscall-number"].contains("value-type"));
	}
	ASSERT_EQ(jout["export"][0]["syscall-number"]["expr-type"], "lit-int");
	ASSERT_EQ(jout["export"][0]["syscall-number"]["value"], "5");

	for (auto& f : jout["ast"]["functions"]) {
		if (f["name"] != "main") continue;
		auto& init = f["block"]["body"][0]["vars"][0]["init"];
		ASSERT_EQ(init["expr-type"], "lit-int");
		ASSERT_EQ(init["value"], "0");
		ASSERT_EQ(init["value-type"]["type-kind"], "pntr");
		ASSERT_EQ(init["value-type"]["base-type"]["type-name"], "void");
	}
}

TEST(gen_ast, macro_fold_backref) {
	// A reference textually before its cinclude registers is left as an
	// unresolved "id" node -- macro visibility is text order, same as C.
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/117_macro_fold_backref.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	auto& ret = jout["ast"]["functions"][0]["block"]["body"][0]["values"][0];
	ASSERT_EQ(ret["expr-type"], "id");
	ASSERT_EQ(ret["name"], "LATE_ANSWER");
}

TEST(gen_ast, macro_fold_sameline) {
	// A macro reference on the same source line as its registering cinclude
	// still resolves via a column comparison, not just a line comparison.
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/119_macro_fold_sameline.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	auto& ret = jout["ast"]["functions"][0]["block"]["body"][0]["values"][0];
	ASSERT_EQ(ret["expr-type"], "lit-int");
	ASSERT_EQ(ret["value"], "9");
}

TEST(gen_ast, macro_fold_duplicate_name) {
	// Two cincludes defining the same macro name: the second header redefines
	// it, as in one C translation unit, so the reference after both sees it.
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/118_macro_fold_dup.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	auto& ret = jout["ast"]["functions"][0]["block"]["body"][0]["values"][0];
	ASSERT_EQ(ret["expr-type"], "lit-int");
	ASSERT_EQ(ret["value"], "2");
}

TEST(gen_ast, return_def_nonprim) {
	// return_def's "ARROW type_expr" form used to keep ret-type only for
	// "prim" and unsized raw arrays, silently dropping any other
	// representable type (pointer, fixed-size array, ...) and turning the
	// function into an accidental void. isDeclarableVarType (shared with
	// var_declaration's no-init form) fixes the whitelist.
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/115_return_def_nonprim.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	auto& funcs = jout["ast"]["functions"];
	bool found_mkPtr = false, found_mkArr = false, found_sret = false;
	for (auto& f : funcs) {
		if (f["name"] == "mkPtr") {
			ASSERT_TRUE(f.contains("ret-type"));
			ASSERT_EQ(f["ret-type"]["type-kind"], "pntr");
			found_mkPtr = true;
		}
		if (f["name"] == "mkArr") {
			ASSERT_TRUE(f.contains("ret-type"));
			ASSERT_EQ(f["ret-type"]["type-kind"], "arr");
			found_mkArr = true;
		}
		if (f["name"] == "sret") {
			ASSERT_TRUE(f.contains("ret-type"));
			ASSERT_EQ(f["ret-type"]["type-kind"], "pntr");
			found_sret = true;
		}
	}
	ASSERT_TRUE(found_mkPtr);
	ASSERT_TRUE(found_mkArr);
	ASSERT_TRUE(found_sret);
}

TEST(gen_ast, tapple_assign) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/123_tapple_assign.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const json& stmts = jout["ast"]["statements"];

	ASSERT_EQ(stmts[0]["stmt-type"], "tapple-assign");
	ASSERT_EQ(stmts[0]["value"]["expr-type"], "call");
	const json& targets = stmts[0]["targets"];
	ASSERT_EQ(targets.size(), 3);
	ASSERT_EQ(targets[0]["expr-type"], "id");
	ASSERT_EQ(targets[1]["expr-type"], "arr-index");
	ASSERT_EQ(targets[2]["expr-type"], "field-access");
	ASSERT_EQ(targets[2]["object"]["expr-type"], "field-access");

	ASSERT_EQ(stmts[1]["stmt-type"], "tapple-assign");
	ASSERT_EQ(stmts[1]["value"]["expr-type"], "member-call");
}

TEST(gen_ast, field_transfer) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/125_field_transfer.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 4);

	ASSERT_EQ(stmts[2]["stmt-type"], "field-assign");
	ASSERT_EQ(stmts[2]["object"]["name"], "w");
	ASSERT_EQ(stmts[2]["field"], "own");
	ASSERT_EQ(stmts[2]["value"]["name"], "t");
	ASSERT_EQ(stmts[2].value("ownership-transfer", false), true);

	ASSERT_EQ(stmts[3]["stmt-type"], "field-assign");
	ASSERT_FALSE(stmts[3].contains("ownership-transfer"));
}

TEST(gen_ast, var_transfer) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/145_var_transfer.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& s = jout["ast"]["statements"][2];
	ASSERT_EQ(s["stmt-type"], "assign");
	ASSERT_EQ(s["name"], "c");
	ASSERT_EQ(s["value"]["name"], "a");
	ASSERT_EQ(s.value("ownership-transfer", false), true);
}

// A chain into array elements used to split the GLR parse at every step:
// 4 steps never finished, so execTestCommand's timeout catches a regression.
TEST(gen_ast, assign_chain) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/127_assign_chain.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 2);

	// Each step's value is the previous step, whose target SA reads back.
	const json* step = &stmts[0];
	for (string idx : {"o", "n", "m", "l", "k", "j", "i"}) {
		ASSERT_EQ((*step)["stmt-type"], "arr-assign");
		ASSERT_EQ((*step)["target"]["index"]["name"], idx);
		step = &(*step)["value"];
	}
	ASSERT_EQ((*step)["expr-type"], "lit-int");

	const auto& outer = stmts[1];
	ASSERT_EQ(outer["stmt-type"], "field-assign");
	ASSERT_EQ(outer["object"]["expr-type"], "arr-index");
	ASSERT_EQ(outer.value("ownership-transfer", false), true);
	ASSERT_EQ(outer["value"]["stmt-type"], "field-assign");
	ASSERT_EQ(outer["value"]["object"]["name"], "s");
	ASSERT_EQ(outer["value"]["field"], "a");
	ASSERT_EQ(outer["value"]["value"]["name"], "x");
	ASSERT_EQ(outer["value"].value("ownership-transfer", false), true);

	const auto& body = jout["ast"]["functions"][0]["block"]["body"];
	ASSERT_EQ(body.size(), 1);
	ASSERT_EQ(body[0]["name"], "b");
	ASSERT_EQ(body[0]["value"]["stmt-type"], "assign");
	ASSERT_EQ(body[0]["value"]["name"], "a");
}

// Copying a list on every append made parse time quadratic: 2000 items took
// 40s+, so execTestCommand's 5s timeout catches a regression. Copying a block
// once per enclosing level took 13s+ for 2000 statements at depth 60.
TEST(gen_ast, decl_variants) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/126_decl_variants.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 5);

	ASSERT_EQ(stmts[0]["stmt-type"], "type-alias");
	ASSERT_EQ(stmts[0]["name"], "Alias");
	ASSERT_EQ(stmts[0]["loc"], json::array({4, 1, 4, 26}));
	ASSERT_EQ(stmts[1]["body"]["args"].size(), 2);
	ASSERT_EQ(stmts[1]["body"]["args"][1]["expr-type"], "transfer");
	ASSERT_EQ(stmts[1]["body"]["args"][1]["loc"], json::array({5, 6, 5, 9}));
	ASSERT_EQ(stmts[2]["vars"].size(), 1);
	ASSERT_EQ(stmts[2]["vars"][0]["var-name"], "c");
	ASSERT_EQ(stmts[3]["vars"].size(), 0);
	ASSERT_EQ(stmts[4]["stmt-type"], "expr");
	ASSERT_EQ(stmts[4]["body"]["expr-type"], "not-impl");
	ASSERT_EQ(stmts[4]["body"]["loc"], json::array({8, 1, 8, 12}));

	// Definitions with an unsupported parameter form are kept wherever they
	// appear, so SA can reject them where they are defined.
	const auto& funcs = jout["ast"]["functions"];
	ASSERT_EQ(funcs.size(), 8);
	ASSERT_EQ(funcs[0]["name"], "dropFirst");
	ASSERT_EQ(funcs[0]["parameters"][0],
	          json({{"not-impl", true}, {"untyped-var", "x"}, {"loc", {1, 16, 1, 21}}}));
	ASSERT_EQ(funcs[3]["name"], "untypedFirst");
	ASSERT_EQ(funcs[3]["parameters"].size(), 2);
	ASSERT_EQ(funcs[3]["parameters"][1], json({{"not-impl", true}, {"loc", {9, 1, 9, 32}}}));
	ASSERT_EQ(funcs[6]["name"], "exportedSysNotImpl");
	ASSERT_EQ(funcs[7]["name"], "outer");
	ASSERT_EQ(funcs[7]["block"]["functions"].size(), 4);
	ASSERT_EQ(funcs[7]["block"]["body"].size(), 1);
	ASSERT_EQ(jout["export"].size(), 2);
}

TEST(gen_ast, enum_def) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/128_enum_def.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 3);

	ASSERT_EQ(stmts[0]["stmt-type"], "enum-def");
	ASSERT_EQ(stmts[0]["name"], "Color");
	const auto& es = stmts[0]["enumerators"];
	ASSERT_EQ(es.size(), 3);
	ASSERT_EQ(es[0]["name"], "RED");
	ASSERT_FALSE(es[0].contains("value"));
	ASSERT_EQ(es[1]["value"]["value"], "5");
	ASSERT_EQ(es[2]["name"], "BLUE");

	ASSERT_EQ(stmts[1]["stmt-type"], "enum-def");
	ASSERT_EQ(stmts[1]["enumerators"][0]["value"]["value"], "-1");

	const auto& init = stmts[2]["vars"][0]["init"];
	ASSERT_EQ(init["expr-type"], "field-access");
	ASSERT_EQ(init["object"]["name"], "Color");
	ASSERT_EQ(init["field"], "GREEN");
}

TEST(gen_ast, export_type) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/130_export_type.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts[0]["stmt-type"], "struct-def");
	ASSERT_EQ(stmts[0]["export"], true);
	ASSERT_EQ(stmts[1]["stmt-type"], "enum-def");
	ASSERT_EQ(stmts[1]["export"], true);
	ASSERT_EQ(stmts[2]["stmt-type"], "type-alias");
	ASSERT_EQ(stmts[2]["export"], true);
	ASSERT_FALSE(stmts[3].contains("export"));
	ASSERT_FALSE(jout["ast"].contains("export"));

	ASSERT_EQ(stmts[4]["vars"][0]["var-type"]["type-name"], "V.vec3");
	ASSERT_EQ(stmts[5]["vars"][0]["var-type"]["base-type"]["type-name"], "V.vec3");
	const auto& e = stmts[6]["fields"][0]["var-type"];
	ASSERT_EQ(e["type-kind"], "embed");
	ASSERT_EQ(e["base-type"]["type-name"], "V.vec3");
	ASSERT_EQ(stmts[7]["vars"][0]["init"]["type"]["type-name"], "V.vec3");
}

TEST(gen_ast, func_ptr_type) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/138_func_ptr_type.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 5);

	const auto& f = stmts[0]["vars"][0]["var-type"];
	ASSERT_EQ(f["type-kind"], "pntr");
	ASSERT_EQ(f["mutable"], false);
	ASSERT_EQ(f["base-type"]["type-kind"], "func");
	ASSERT_EQ(f["base-type"]["parameters"].size(), 2);
	ASSERT_EQ(f["base-type"]["parameters"][1]["var-type"]["mutable"], true);
	ASSERT_EQ(f["base-type"]["ret-type"]["type-name"], "int32");
	// No return is a void return, as c2ast writes a C function type.
	ASSERT_EQ(stmts[1]["vars"][0]["var-type"]["base-type"]["ret-type"]["type-name"], "void");
	// `@!func` is kept for SA to reject.
	ASSERT_EQ(stmts[2]["vars"][0]["var-type"]["mutable"], true);
	ASSERT_EQ(stmts[3]["vars"][0]["var-type"]["type-kind"], "arr");
	ASSERT_EQ(stmts[3]["vars"][0]["var-type"]["base-type"]["base-type"]["ret-type"]["type-kind"], "pntr");

	const auto& fn = jout["ast"]["functions"][0];
	ASSERT_EQ(fn["parameters"][0]["var-type"]["base-type"]["type-kind"], "func");
	ASSERT_EQ(fn["ret-type"]["base-type"]["ret-type"]["type-name"], "int32");

	// The return of a type after `as` is taken greedily, so this is a cast
	// to a function returning int32, then chained into f.
	ASSERT_EQ(stmts[4]["stmt-type"], "assign");
	ASSERT_EQ(stmts[4]["value"]["target-type"]["base-type"]["ret-type"]["type-name"], "int32");
}

TEST(gen_ast, frame_var) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/142_frame_var.pa");
	ASSERT_TRUE(checkerr(output));
	json ast = json::parse(output)["ast"];
	const auto& stmts = ast["statements"];
	ASSERT_EQ(stmts[1]["stmt-type"], "var-decl");
	ASSERT_EQ(stmts[1]["vars"][0]["var-type"]["type-kind"], "embed");
	ASSERT_EQ(stmts[1]["vars"][0]["var-type"]["base-type"]["type-name"], "P");
	ASSERT_EQ(stmts[2]["vars"][0]["var-type"]["type-kind"], "embed");
	ASSERT_EQ(stmts[2]["vars"][0]["var-type"]["base-type"]["type-kind"], "arr");
	// A later name takes the first one's type, '$' included.
	ASSERT_EQ(stmts[3]["vars"][1]["var-type"]["type-kind"], "embed");
}

TEST(gen_ast, ro_ptr_slot_decl) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/149_ro_ptr_slot_decl.pa");
	ASSERT_TRUE(checkerr(output));
	json ast = json::parse(output)["ast"];
	const auto& stmts = ast["statements"];
	ASSERT_EQ(stmts[0]["stmt-type"], "var-decl");
	ASSERT_EQ(stmts[0]["vars"][0]["var-type"]["base-type"]["mutable"], false);
	ASSERT_EQ(stmts[1]["vars"][0]["var-type"]["base-type"]["mutable"], true);
}

static string readText(const string& path) {
	ifstream f(path);
	return string(istreambuf_iterator<char>(f), {});
}

static json readJson(const string& path) {
	ifstream f(path);
	return json::parse(f);
}

TEST(gen_ast, cinclude_export_clause) {
	cleanTestEnv();
	ofstream("out/m.pa") << "cinclude <stdio.h> as C export FILE, fpos_t link \"m\";\ncinclude <time.h>;\n";
	ASSERT_EQ(execTestCommand("palan-gen-ast out/m.pa -o out/m.json"), "");
	const auto stmts = readJson("out/m.json")["ast"]["statements"];
	ASSERT_EQ(stmts[0]["export-types"], json::array({"FILE", "fpos_t"}));
	ASSERT_EQ(stmts[0]["alias"], "C");
	ASSERT_EQ(stmts[0]["libs"], json::array({"m"}));
	ASSERT_FALSE(stmts[0]["functions"].empty());
	ASSERT_FALSE(stmts[1].contains("export-types"));
}

TEST(gen_ast, interface_text) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand("palan-gen-ast ../test/testdata/gen-ast/155_interface.pa"
		" -o out/m.json --interface out/io.pai"), "");
	string abs = fs::weakly_canonical("../test/testdata/gen-ast").string();
	ASSERT_EQ(readText("out/io.pai"),
		"library std.io;\n"
		"built \"" PALAN_VERSION "\";\n"
		"target \"" PALAN_TARGET "\";\n"
		"link \"m\";\n"
		"link \"rt\";\n"
		"import \"" + abs + "/other.pa\";\n"
		"import a, + from \"" + abs + "/other2.pa\" as o;\n"
		"cinclude <stdio.h> export FILE, fpos_t;\n"
		"type Hidden {\n\tint32 a;\n};\n"
		"export type Shown {\n\tHidden h;\n\t@FILE fp;\n};\n"
		"export type Color enum {\n\tRed,\n\tGreen = 5,\n};\n"
		"export type Size = int64;\n"
		"export type Handle = opaque Hidden;\n"
		"const N = 3 * (8192 + 1);\n"
		"[3]int32 lit;\n"
		"[2]$[2]int32 rows;\n"
		"const M = lit -> len();\n"
		"const GR = rows[0] -> len();\n"
		"export func f(int32 x, @!Shown s) -> int32;\n"
		"export func g() -> int32 a, int32 b;\n"
		"export func +(@Shown a, @Shown b) -> int32;\n"
		"export func h();\n"
		"export func k(int32 a, int32 b = N + 1, Color c = Color.Green, @[]uint8 s = \"a\\tb\", bool on = true, flo64 d = -1.5) -> int32;\n"
		"export func va(int32 a, [...]@Shown xs);\n"
		"export syscall sys(int64 x) -> int64 = 39;\n"
		"export func close(Handle h) release;\n");
}

static json withoutLoc(json j) {
	if (j.is_object()) j.erase("loc");
	if (j.is_structured())
		for (auto& child : j) child = withoutLoc(child);
	return j;
}

// A library's users see the AST of the text, so it must say what the source said.
TEST(gen_ast, interface_keeps_export_import) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand("palan-gen-ast ../test/testdata/gen-ast/174_export_import.pa"
		" -o out/m.json --interface out/m.pai"), "");
	string abs = fs::weakly_canonical("../test/testdata/gen-ast").string();
	string text = readText("out/m.pai");
	ASSERT_NE(text.find("export import \"" + abs + "/other.pa\";\n"
		"export import a, b from <std/utf8>;\n"
		"import \"" + abs + "/plain.pa\";\n"), string::npos) << text;
}

TEST(gen_ast, interface_reads_back_as_the_source) {
	for (string file : {"155_interface.pa", "156_interface_forms.pa"}) {
		cleanTestEnv();
		if (file == "156_interface_forms.pa") {
			// A signature here has no body: read once as a .pai to get a source to write.
			execTestCommand("cp ../test/testdata/gen-ast/" + file + " out/forms.pai");
			ofstream("out/forms.pai", ios::app) << "built \"" PALAN_VERSION "\";\ntarget \"" PALAN_TARGET "\";\n";
			ASSERT_EQ(execTestCommand("palan-gen-ast out/forms.pai -o out/source.json --interface out/m.pai"), "");
		} else {
			ASSERT_EQ(execTestCommand("palan-gen-ast ../test/testdata/gen-ast/" + file
				+ " -o out/source.json --interface out/m.pai"), "");
		}
		ASSERT_EQ(execTestCommand("palan-gen-ast out/m.pai -o out/pai.json"), "") << file;
		json source = readJson("out/source.json"), pai = readJson("out/pai.json");
		ASSERT_EQ(pai["built"], PALAN_VERSION);
		ASSERT_EQ(withoutLoc(pai["export"]), withoutLoc(source["export"])) << file;

		// Each kept statement as written, but for a variable's type and the C declarations.
		vector<json> kept;
		for (auto& s : source["ast"]["statements"]) {
			string kind = s.value("stmt-type", "");
			if (kind == "struct-def" || kind == "opaque-def" || kind == "enum-def" || kind == "type-alias" || kind == "import")
				kept.push_back(withoutLoc(s));
		}
		vector<json> read;
		for (auto& s : pai["ast"]["statements"]) {
			string kind = s.value("stmt-type", "");
			if (kind == "struct-def" || kind == "opaque-def" || kind == "enum-def" || kind == "type-alias" || kind == "import")
				read.push_back(withoutLoc(s));
		}
		ASSERT_EQ(json(read), json(kept)) << file;
	}
}

TEST(gen_ast, interface_const_values_read_back) {
	cleanTestEnv();
	execTestCommand("cp ../test/testdata/gen-ast/156_interface_forms.pa out/forms.pai");
	ofstream("out/forms.pai", ios::app) << "built \"" PALAN_VERSION "\";\ntarget \"" PALAN_TARGET "\";\n";
	ASSERT_EQ(execTestCommand("palan-gen-ast out/forms.pai -o out/source.json --interface out/m.pai"), "");
	ASSERT_EQ(execTestCommand("palan-gen-ast out/m.pai -o out/pai.json"), "");
	map<string, json> source, pai;
	const json sourceAst = readJson("out/source.json"), paiAst = readJson("out/pai.json");
	for (auto& s : sourceAst["ast"]["statements"])
		if (s["stmt-type"] == "const-decl") source[s["name"]] = withoutLoc(s["value"]);
	for (auto& s : paiAst["ast"]["statements"])
		if (s["stmt-type"] == "const-decl") pai[s["name"]] = withoutLoc(s["value"]);
	ASSERT_EQ(pai.size(), 19u);
	for (auto& [name, value] : pai) ASSERT_EQ(value, source.at(name)) << name;
}

// What only runs must not reach importers, even if it moves the declarations' lines.
TEST(gen_ast, interface_ignores_what_only_runs) {
	cleanTestEnv();
	auto paiOf = [](const string& source) {
		ofstream("out/m.pa") << source;
		EXPECT_EQ(execTestCommand("palan-gen-ast out/m.pa -o out/m.json --interface out/m.pai"), "");
		return readText("out/m.pai");
	};
	const string sig = "export func f() -> int32 { return 1; }\nexport func g() -> int32 { return 1; }\n";
	string base = paiOf("[]int32 a = [1, 2];\nconst N = a->len();\n" + sig);
	ASSERT_EQ(base, paiOf("[]int32 a = [3, 4];\nconst N = a->len();\n" + sig));
	ASSERT_EQ(base, paiOf("[]int32 a = [1, 2];\nconst N = a->len();\n"
		"export func f() -> int32 {\n\tint32 x = 2;\n\treturn x;\n}\nexport func g() -> int32 { return 1; }\n"));
	ASSERT_EQ(base, paiOf("[]int32 a = [1, 2];\nint32 b = 1;\nconst N = a->len();\nb + 1 -> b;\n" + sig));
	ASSERT_NE(base, paiOf("[]int32 a = [1, 2, 3];\nconst N = a->len();\n" + sig));
	ASSERT_NE(base, paiOf("[]int32 a = [1, 2];\nconst N = a->len();\n"
		"export func f() -> int64 { return 1; }\nexport func g() -> int32 { return 1; }\n"));
}

// An exported global is published without its initializer, but with the
// sizes its array literal gives, and reads back as one.
TEST(gen_ast, interface_export_global) {
	cleanTestEnv();
	auto paiOf = [](const string& source) {
		ofstream("out/m.pa") << source;
		EXPECT_EQ(execTestCommand("palan-gen-ast out/m.pa -o out/m.json --interface out/m.pai"), "");
		return readText("out/m.pai");
	};
	string pai = paiOf("export global int64 count = 3 for f!;\nexport global []int32 arr = [1, 2];\n"
		"global int64 hidden = 1;\nfunc f() { }\n");
	ASSERT_NE(pai.find("export global int64 count;\nexport global [2]int32 arr;\n"), string::npos) << pai;
	ASSERT_EQ(pai.find("hidden"), string::npos) << pai;
	ASSERT_EQ(pai, paiOf("export global int64 count = 4;\nexport global []int32 arr = [5, 6];\n"));

	json jout = json::parse(execTestCommand("palan-gen-ast out/m.pai"));
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts[0]["stmt-type"], "var-decl");
	ASSERT_EQ(stmts[0]["global"], true);
	ASSERT_EQ(stmts[0]["export"], true);
	ASSERT_FALSE(stmts[0]["vars"][0].contains("init"));
}

TEST(gen_ast, c_headers) {
	cleanTestEnv();
	json jout = json::parse(execTestCommand("palan-gen-ast ../test/testdata/gen-ast/035_cinclude_local_header.pa"));
	bool found = false;
	for (auto& header : jout["c-headers"])
		found = found || header.get<string>().ends_with(".h");
	ASSERT_TRUE(found) << jout["c-headers"];

	ofstream("out/none.pa") << "int32 x;\n";
	jout = json::parse(execTestCommand("palan-gen-ast out/none.pa"));
	ASSERT_FALSE(jout.contains("c-headers"));
}

TEST(gen_ast, c_export) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/167_c_export.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& exports = jout["export"];
	ASSERT_EQ(exports[0]["name"], "add");
	ASSERT_EQ(exports[0]["c-export"], true);
	ASSERT_FALSE(exports[1].contains("c-export"));

	ofstream("out/m.pai") << "built \"" PALAN_VERSION "\";\ntarget \"" PALAN_TARGET "\";\n"
	                         "cexport func add(int32 a, int32 b) -> int32;\n";
	jout = json::parse(execTestCommand("palan-gen-ast out/m.pai"));
	ASSERT_EQ(jout["export"][0]["c-export"], true);
}

TEST(gen_ast, dict_type) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/169_dict_type.pa");
	ASSERT_TRUE(checkerr(output));
	const json ast = json::parse(output)["ast"];
	const json& stmts = ast["statements"];
	ASSERT_EQ(stmts.size(), 5);

	const json i32Dict = json::parse(R"({"type-kind": "dict", "base-type": {"type-kind": "prim", "type-name": "int32"}})");
	EXPECT_EQ(stmts[0]["fields"][1]["var-type"], i32Dict);
	EXPECT_EQ(stmts[1]["vars"][0]["var-type"], json::parse(R"({"type-kind": "dict", "embedded": true,
		"base-type": {"type-kind": "prim", "type-name": "P"}})"));
	EXPECT_EQ(stmts[2]["vars"][0]["var-type"], json::parse(R"({"type-kind": "arr", "specifier": "variable", "size-expr": null,
		"base-type": {"type-kind": "dict", "base-type": {"type-kind": "pntr", "mutable": false,
			"base-type": {"type-kind": "prim", "type-name": "P"}}}})"));
	EXPECT_EQ(stmts[3]["vars"][0]["var-type"]["base-type"], i32Dict);
	// '[""]' with nothing after it stays an array literal.
	EXPECT_EQ(stmts[4]["value"]["expr-type"], "arr-lit");

	const json& h = ast["functions"][0];
	EXPECT_EQ(h["parameters"][0]["var-type"]["base-type"], i32Dict);
	EXPECT_EQ(h["ret-type"]["type-kind"], "dict");
}

TEST(gen_ast, empty_dict_lit) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/170_empty_dict_lit.pa");
	ASSERT_TRUE(checkerr(output));
	const json ast = json::parse(output)["ast"];
	const json& stmts = ast["statements"];
	ASSERT_EQ(stmts.size(), 5);
	const json empty = json::parse(R"({"expr-type": "dict-lit", "items": []})");
	auto noLoc = [](json e) { e.erase("loc"); return e; };
	EXPECT_EQ(noLoc(stmts[0]["vars"][0]["init"]), empty);
	EXPECT_EQ(noLoc(stmts[1]["vars"][0]["init"]["items"][1]["value"]), empty);
	const json& v = stmts[2]["vars"][0]["init"]["items"];
	EXPECT_EQ(noLoc(v[0]), empty);
	EXPECT_EQ(noLoc(v[1]["items"][0]["value"]), empty);
	const json& args = stmts[3]["body"]["args"];
	EXPECT_EQ(noLoc(args[0]), empty);
	EXPECT_EQ(noLoc(args[1]["object"]), empty);
	EXPECT_EQ(args[2]["mutable"], true);
	EXPECT_EQ(noLoc(ast["functions"][0]["block"]["body"][0]["values"][0]), empty);
	// At the start of a statement '{}' stays an empty block.
	EXPECT_EQ(stmts[4]["body"][0]["stmt-type"], "block");
}
