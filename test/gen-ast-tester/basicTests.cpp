/// gen-ast tests
///
/// @file basicTests.cpp
/// @copyright 2024-2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include <fstream>
#include "../test-base/testBase.h"
#include "genAstTestUtil.h"

using namespace std;

TEST(gen_ast, target) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/001_basicPattern.pa");
	ASSERT_TRUE(checkerr(output));
	ASSERT_EQ(json::parse(output)["target"], "x86_64-linux-gnu");

	// The source has a cinclude, so c2ast must accept the target too.
	output = execTestCommand("palan-gen-ast --target x86_64-linux-gnu ../test/testdata/gen-ast/001_basicPattern.pa");
	ASSERT_TRUE(checkerr(output));
	ASSERT_EQ(json::parse(output)["target"], "x86_64-linux-gnu");
}

// Sources are written into out/ since most of what matters here is their name.
static string moduleOf(const string& fileName, const string& source = "") {
	string path = "out/" + fileName;
	ofstream(path) << source;
	string output = execTestCommand("palan-gen-ast '" + path + "'");
	return json::parse(output)["module"];
}

TEST(gen_ast, module_identity) {
	cleanTestEnv();
	EXPECT_EQ(moduleOf("util.pa"), "util");
	EXPECT_EQ(moduleOf("utf8.pa", "library std;\n"), "std.utf8");
	EXPECT_EQ(moduleOf("file.pa", "// a comment\nlibrary std.io;\nint32 x;\n"), "std.io.file");
	EXPECT_EQ(moduleOf("001_hello.pa"), "_001_hello");
	EXPECT_EQ(moduleOf("1x.pa", "library a;\n"), "a._1x");
	EXPECT_EQ(moduleOf("my-lib.v2.pa"), "my_2dlib_2ev2");
	EXPECT_EQ(moduleOf("my_2dlib.pa"), "my_5f2dlib");
	EXPECT_EQ(moduleOf("a_2.pa"), "a_2");
	EXPECT_EQ(moduleOf("a_2D.pa"), "a_2D");
	EXPECT_EQ(moduleOf("a_x.pa"), "a_x");
}

TEST(gen_ast, basic_tests) {
	cleanTestEnv();
	string output;
	output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/001_basicPattern.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	ASSERT_EQ(jout["ast"]["statements"].size(), 19);

	bool found_printf = false;
	for (auto& stmt : jout["ast"]["statements"]) {
		if (stmt["stmt-type"] != "cinclude") continue;
		for (auto& f : stmt["functions"]) {
			if (f["name"] == "printf" && f["func-type"] == "c") {
				found_printf = true;
				break;
			}
		}
		if (found_printf) break;
	}
	ASSERT_TRUE(found_printf);

	output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/100_quicksort.pa");
	ASSERT_TRUE(checkerr(output));
	jout = json::parse(output);
	ASSERT_EQ(jout["ast"]["statements"].size(), 6);

	output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/002_arraypattern.pa");
	ASSERT_TRUE(checkerr(output));
	jout = json::parse(output);
	ASSERT_EQ(jout["ast"]["statements"].size(), 12);
}

static json stripLoc(const json& j) {
	if (j.is_object()) {
		json r = json::object();
		for (auto& [k, v] : j.items())
			if (k != "loc") r[k] = stripLoc(v);
		return r;
	}
	if (j.is_array()) {
		json r = json::array();
		for (auto& v : j) r.push_back(stripLoc(v));
		return r;
	}
	return j;
}

TEST(gen_ast, arr_lit_2d_forms) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/002_arraypattern.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	const json* vars = nullptr;
	for (auto& stmt : jout["ast"]["statements"])
		if (stmt["stmt-type"] == "var-decl" && stmt["vars"][0]["name"] == "a1")
			vars = &stmt["vars"];
	ASSERT_NE(vars, nullptr);
	ASSERT_EQ(vars->size(), 2);

	const json& a1 = (*vars)[0]["init"];
	ASSERT_EQ(a1["expr-type"], "arr-lit");
	ASSERT_EQ(a1["items"].size(), 2);
	for (auto& row : a1["items"]) {
		ASSERT_EQ(row["expr-type"], "arr-lit");
		ASSERT_EQ(row["items"].size(), 3);
	}
	ASSERT_EQ(stripLoc(a1), stripLoc((*vars)[1]["init"]));
}

TEST(gen_ast, varr_var_decl) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/002_arraypattern.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];

	// [+]int64 a6 = [1,3,5]
	ASSERT_EQ(stmts[5]["stmt-type"], "var-decl");
	const json& a6 = stmts[5]["vars"][0];
	ASSERT_EQ(a6["name"], "a6");
	ASSERT_EQ(a6["var-type"]["specifier"], "variable");
	ASSERT_TRUE(a6["var-type"]["size-expr"].is_null());
	// [+][+]int64 a8
	ASSERT_EQ(stmts[7]["stmt-type"], "var-decl");
	ASSERT_EQ(stmts[7]["vars"][0]["var-type"]["base-type"]["specifier"], "variable");
}

TEST(gen_ast, arr_lit_trailing_comma) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/121_arr_lit_trailing_comma.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	int pairs = 0;
	for (auto& stmt : jout["ast"]["statements"]) {
		if (stmt["stmt-type"] != "var-decl") continue;
		const json& vars = stmt["vars"];
		ASSERT_EQ(vars.size(), 2);
		ASSERT_EQ(stripLoc(vars[0]["init"]), stripLoc(vars[1]["init"]));
		pairs++;
	}
	ASSERT_EQ(pairs, 4);
}

TEST(gen_ast, dict_lit) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/122_dict_lit.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	const json& items = jout["ast"]["statements"][0]["vars"][0]["init"]["items"];
	ASSERT_EQ(items.size(), 2);
	ASSERT_EQ(items[0]["expr-type"], "dict-lit");
	const json& first = items[0]["items"];
	ASSERT_EQ(first.size(), 2);
	ASSERT_EQ(first[0]["name"], "x");
	ASSERT_EQ(first[0]["value"]["expr-type"], "lit-int");
	ASSERT_EQ(first[1]["name"], "y");
	ASSERT_EQ(first[1]["value"]["expr-type"], "add");
	ASSERT_EQ(first[1]["loc"], json::array({1, 22, 1, 30}));
	// A trailing comma is allowed, as in array literals.
	ASSERT_EQ(items[1]["items"].size(), 2);
	ASSERT_EQ(items[1]["items"][0]["name"], "y");
}

TEST(gen_ast, addition) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/build-mgr/003_addition.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	bool found_add = false;
	bool found_sub = false;
	for (auto& stmt : jout["ast"]["statements"]) {
		if (stmt["stmt-type"] != "expr") continue;
		auto& body = stmt["body"];
		if (body["expr-type"] == "call" && body["name"] == "printf") {
			for (auto& arg : body["args"]) {
				if (arg["expr-type"] == "add") {
					ASSERT_EQ(arg["left"]["expr-type"],  "id");
					ASSERT_EQ(arg["right"]["expr-type"], "id");
					found_add = true;
				}
				if (arg["expr-type"] == "sub") {
					ASSERT_EQ(arg["left"]["expr-type"],  "id");
					ASSERT_EQ(arg["right"]["expr-type"], "id");
					found_sub = true;
				}
			}
		}
	}
	ASSERT_TRUE(found_add);
	ASSERT_TRUE(found_sub);
}

TEST(gen_ast, unary_minus) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/build-mgr/007_unary_minus.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	// -42 folds into one negative lit-int; -x stays a neg node.
	auto& neg = jout["ast"]["functions"][0]["block"]["body"][0]["then"]["body"][0]["values"][0];
	ASSERT_EQ(neg["expr-type"], "neg");
	ASSERT_EQ(neg["operand"]["expr-type"], "id");

	bool found_neg_lit = false;
	for (auto& stmt : jout["ast"]["statements"]) {
		if (stmt["stmt-type"] != "expr") continue;
		auto& body = stmt["body"];
		if (body["expr-type"] != "call" || body["name"] != "printf") continue;
		auto& arg = body["args"][1];
		ASSERT_EQ(arg["expr-type"], "lit-int");
		ASSERT_EQ(arg["value"], "-42");
		found_neg_lit = true;
	}
	ASSERT_TRUE(found_neg_lit);
}

TEST(gen_ast, comparison) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/build-mgr/005_comparison.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	bool found_lt = false;
	bool found_eq = false;
	for (auto& stmt : jout["ast"]["statements"]) {
		if (stmt["stmt-type"] != "expr") continue;
		auto& body = stmt["body"];
		if (body["expr-type"] == "call" && body["name"] == "printf") {
			for (auto& arg : body["args"]) {
				if (arg["expr-type"] != "cmp") continue;
				ASSERT_EQ(arg["left"]["expr-type"],  "id");
				ASSERT_EQ(arg["right"]["expr-type"], "id");
				if (arg["op"] == "<")  found_lt = true;
				if (arg["op"] == "==") found_eq = true;
			}
		}
	}
	ASSERT_TRUE(found_lt);
	ASSERT_TRUE(found_eq);
}

TEST(gen_ast, var_decl_int32) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/build-mgr/004_int32_widening.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	bool found = false;
	for (auto& stmt : jout["ast"]["statements"]) {
		if (stmt["stmt-type"] != "var-decl") continue;
		for (auto& v : stmt["vars"]) {
			if (v["name"] == "x") {
				ASSERT_EQ(v["var-type"]["type-kind"], "prim");
				ASSERT_EQ(v["var-type"]["type-name"], "int32");
				found = true;
			}
		}
	}
	ASSERT_TRUE(found);
}

TEST(gen_ast, cast) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/101_cast.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	// cast_node: int32(x) — cast of an identifier
	bool found_simple = false;
	for (auto& stmt : jout["ast"]["statements"]) {
		if (stmt["stmt-type"] != "expr") continue;
		auto& expr = stmt["body"];
		if (expr["expr-type"] != "cast" || expr["target-type"]["type-name"] != "int32") continue;
		ASSERT_EQ(expr["src"]["expr-type"], "id");
		ASSERT_EQ(expr["src"]["name"], "x");
		found_simple = true;
		break;
	}
	ASSERT_TRUE(found_simple);

	// cast_compound_expr: uint64(y+z) — cast of a compound expression
	bool found_compound = false;
	for (auto& stmt : jout["ast"]["statements"]) {
		if (stmt["stmt-type"] != "expr") continue;
		auto& expr = stmt["body"];
		if (expr["expr-type"] != "cast" || expr["target-type"]["type-name"] != "uint64") continue;
		ASSERT_EQ(expr["src"]["expr-type"], "add");
		ASSERT_EQ(expr["src"]["left"]["expr-type"],  "id");
		ASSERT_EQ(expr["src"]["right"]["expr-type"], "id");
		found_compound = true;
		break;
	}
	ASSERT_TRUE(found_compound);
}

TEST(gen_ast, func_def) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/005_func_def.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	ASSERT_EQ(jout["ast"]["statements"].size(), 0);
	ASSERT_EQ(jout["ast"]["functions"].size(), 3);

	// add: parameters, ret-type, return stmt
	bool found_add = false;
	for (auto& f : jout["ast"]["functions"]) {
		if (f["name"] != "add") continue;
		ASSERT_EQ(f["func-type"], "palan");
		ASSERT_EQ(f["parameters"].size(), 2u);
		ASSERT_EQ(f["parameters"][0]["name"], "a");
		ASSERT_EQ(f["parameters"][0]["var-type"]["type-name"], "int32");
		ASSERT_TRUE(f.contains("ret-type"));
		ASSERT_EQ(f["ret-type"]["type-name"], "int32");
		bool has_return = false;
		for (auto& s : f["block"]["body"])
			if (s["stmt-type"] == "return") { has_return = true; }
		ASSERT_TRUE(has_return);
		found_add = true;
	}
	ASSERT_TRUE(found_add);

	// divmod: rets, assign stmt, bare return
	bool found_divmod = false;
	for (auto& f : jout["ast"]["functions"]) {
		if (f["name"] != "divmod") continue;
		ASSERT_FALSE(f.contains("ret-type"));
		ASSERT_TRUE(f.contains("rets"));
		ASSERT_EQ(f["rets"].size(), 2u);
		ASSERT_EQ(f["rets"][0]["name"], "q");
		ASSERT_EQ(f["rets"][1]["name"], "r");
		bool has_assign = false;
		for (auto& s : f["block"]["body"])
			if (s["stmt-type"] == "assign") {
				ASSERT_EQ(s["name"], "q");
				has_assign = true;
			}
		ASSERT_TRUE(has_assign);
		found_divmod = true;
	}
	ASSERT_TRUE(found_divmod);
}

TEST(gen_ast, block_stmt) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/006_block.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	// top-level: statements has 1 block, functions has outer only
	ASSERT_EQ(jout["ast"]["statements"].size(), 1);
	ASSERT_EQ(jout["ast"]["functions"].size(), 1);

	// block stmt structure: top-level block has functions=[] and body=[var-decl, assign]
	const auto& blk = jout["ast"]["statements"][0];
	ASSERT_EQ(blk["stmt-type"], "block");
	ASSERT_EQ(blk["body"].size(), 2);  // var-decl + assign
	ASSERT_EQ(blk["functions"].size(), 0);

	// outer function block contains one block in body
	const auto& outerFunc = jout["ast"]["functions"][0];
	ASSERT_EQ(outerFunc["name"], "outer");
	const auto& outerBody = outerFunc["block"]["body"];
	ASSERT_EQ(outerBody.size(), 1);
	ASSERT_EQ(outerBody[0]["stmt-type"], "block");

	// inner func-def is in block.functions, body has the return stmt
	ASSERT_EQ(outerBody[0]["functions"].size(), 1);
	ASSERT_EQ(outerBody[0]["functions"][0]["name"], "inner");
	ASSERT_EQ(outerBody[0]["body"].size(), 1);
	ASSERT_EQ(outerBody[0]["body"][0]["stmt-type"], "return");
}

TEST(gen_ast, export_func) {
	cleanTestEnv();
	string output = execTestCommand(
		"palan-gen-ast ../test/testdata/gen-ast/007_export_func.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	// root "export" array check (SA reads this)
	ASSERT_TRUE(jout.contains("export"));
	ASSERT_EQ(jout["export"].size(), 1u);
	ASSERT_EQ(jout["export"][0]["name"], "add");
	ASSERT_FALSE(jout["export"][0].contains("block"));  // block is excluded

	// ast["ast"]["functions"] export flag check
	bool found_export = false, found_noexport = false;
	for (auto& f : jout["ast"]["functions"]) {
		if (f["name"] == "add") {
			ASSERT_TRUE(f.value("export", false));
			found_export = true;
		}
		if (f["name"] == "noexport") {
			ASSERT_FALSE(f.value("export", false));
			found_noexport = true;
		}
	}
	ASSERT_TRUE(found_export);
	ASSERT_TRUE(found_noexport);
}

TEST(gen_ast, if_stmt) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/008_if_stmt.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	// clamp function: body has 2 stmts (if + return)
	bool found_clamp = false;
	for (auto& f : jout["ast"]["functions"]) {
		if (f["name"] != "clamp") continue;
		found_clamp = true;
		const auto& body = f["block"]["body"];
		ASSERT_EQ(body.size(), 2u);
		const auto& if_node = body[0];
		ASSERT_EQ(if_node["stmt-type"], "if");
		ASSERT_EQ(if_node["cond"]["expr-type"], "cmp");
		ASSERT_EQ(if_node["cond"]["op"], "<");
		ASSERT_TRUE(if_node.contains("then"));
		ASSERT_FALSE(if_node.contains("else"));
	}
	ASSERT_TRUE(found_clamp);

	// sign function: if-else
	bool found_sign = false;
	for (auto& f : jout["ast"]["functions"]) {
		if (f["name"] != "sign") continue;
		found_sign = true;
		const auto& body = f["block"]["body"];
		ASSERT_EQ(body.size(), 2u);
		const auto& if_node = body[0];
		ASSERT_EQ(if_node["stmt-type"], "if");
		ASSERT_TRUE(if_node.contains("then"));
		ASSERT_TRUE(if_node.contains("else"));
		ASSERT_EQ(if_node["else"]["stmt-type"], "block");
	}
	ASSERT_TRUE(found_sign);
}

TEST(gen_ast, while_loop) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/009_while_loop.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	bool found_countdown = false;
	for (auto& f : jout["ast"]["functions"]) {
		if (f["name"] != "countDown") continue;
		found_countdown = true;
		const auto& body = f["block"]["body"];
		ASSERT_GE(body.size(), 1u);
		const auto& wh = body[0];
		ASSERT_EQ(wh["stmt-type"], "while");
		ASSERT_EQ(wh["cond"]["expr-type"], "cmp");
		ASSERT_EQ(wh["cond"]["op"], ">");
		ASSERT_TRUE(wh.contains("body"));
		ASSERT_EQ(wh["body"].size(), 1u);
		ASSERT_EQ(wh["body"][0]["stmt-type"], "assign");
	}
	ASSERT_TRUE(found_countdown);
}

TEST(gen_ast, stmt_list_patterns) {
	cleanTestEnv();
	// Covers stmt_list_b/body_list_b grammar rules added for optional semicolon:
	// B;E and B;func_def at top-level, and B B / B;B / B func_def / B;func_def inside blocks.
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/012_stmt_list_patterns.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	// top-level: x var-decl and two if stmts plus helper/outer functions
	ASSERT_GE(jout["ast"]["statements"].size(), 1u);
	ASSERT_GE(jout["ast"]["functions"].size(), 2u);  // helper + outer

	// outer function has multiple statements (while/if mix)
	bool found_outer = false;
	for (auto& f : jout["ast"]["functions"]) {
		if (f["name"] != "outer") continue;
		found_outer = true;
		ASSERT_GE(f["block"]["body"].size(), 4u);
		// block-local inner funcs declared inside outer
		ASSERT_GE(f["block"]["functions"].size(), 2u);
	}
	ASSERT_TRUE(found_outer);
}

TEST(gen_ast, break_continue) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/011_break_continue.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	// while body: if-break, if-continue
	const auto& stmts = jout["ast"]["statements"];
	bool found_break = false;
	bool found_continue = false;
	for (auto& stmt : stmts) {
		if (stmt["stmt-type"] != "while") continue;
		for (auto& s : stmt["body"]) {
			if (s["stmt-type"] != "if") continue;
			for (auto& b : s["then"]["body"]) {
				if (b["stmt-type"] == "break")    found_break    = true;
				if (b["stmt-type"] == "continue") found_continue = true;
			}
		}
	}
	ASSERT_TRUE(found_break);
	ASSERT_TRUE(found_continue);
}

TEST(gen_ast, optional_semicolon) {
	cleanTestEnv();
	// Last statement in a block can omit the semicolon.
	// The AST output should be identical to the version with semicolon.
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/010_optional_semicolon.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	// cinclude + 2 var-decls (x and y); y has no trailing ';'
	ASSERT_EQ(jout["ast"]["statements"].size(), 3u);
	const auto& y_decl = jout["ast"]["statements"][2];
	ASSERT_EQ(y_decl["stmt-type"], "var-decl");
	ASSERT_EQ(y_decl["vars"][0]["name"], "y");
	ASSERT_EQ(y_decl["vars"][0]["init"]["expr-type"], "add");
}

TEST(gen_ast, float_literal) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/013_float_literal.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	ASSERT_EQ(jout["ast"]["statements"].size(), 2u);

	// flo64 x = 3.14
	const auto& x_decl = jout["ast"]["statements"][0];
	ASSERT_EQ(x_decl["stmt-type"], "var-decl");
	ASSERT_EQ(x_decl["vars"][0]["name"], "x");
	ASSERT_EQ(x_decl["vars"][0]["var-type"]["type-name"], "flo64");
	ASSERT_EQ(x_decl["vars"][0]["init"]["expr-type"], "lit-flo");
	ASSERT_EQ(x_decl["vars"][0]["init"]["value"], "3.14");

	// flo32 y = 1.5
	const auto& y_decl = jout["ast"]["statements"][1];
	ASSERT_EQ(y_decl["vars"][0]["name"], "y");
	ASSERT_EQ(y_decl["vars"][0]["var-type"]["type-name"], "flo32");
	ASSERT_EQ(y_decl["vars"][0]["init"]["expr-type"], "lit-flo");
	ASSERT_EQ(y_decl["vars"][0]["init"]["value"], "1.5");
}

TEST(gen_ast, cli_tests) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast -h");
	ASSERT_EQ(output.substr(0,7), "Usage: ");

	output = execTestCommand("palan-gen-ast not_exist_file.pa");
	ASSERT_EQ(output, "return1::Could not open file 'not_exist_file.pa'.\n");

	output = execTestCommand("palan-gen-ast -i ../test/testdata/gen-ast/001_basicPattern.pa -o out/001ast.json");
	ASSERT_EQ(output, "");
}

TEST(gen_ast, array_decl) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/014_array_decl.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	ASSERT_EQ(jout["ast"]["statements"].size(), 3);

	bool found_buf = false, found_arr = false;
	for (auto& stmt : jout["ast"]["statements"]) {
		if (stmt["stmt-type"] != "var-decl") continue;
		for (auto& v : stmt["vars"]) {
			if (v["name"] == "buf") {
				ASSERT_EQ(v["var-type"]["type-kind"], "arr");
				ASSERT_EQ(v["var-type"]["size-expr"]["expr-type"], "lit-int");
				ASSERT_EQ(v["var-type"]["size-expr"]["value"], "64");
				ASSERT_EQ(v["var-type"]["base-type"]["type-kind"], "prim");
				ASSERT_EQ(v["var-type"]["base-type"]["type-name"], "uint8");
				found_buf = true;
			}
			if (v["name"] == "arr") {
				ASSERT_EQ(v["var-type"]["type-kind"], "arr");
				ASSERT_EQ(v["var-type"]["size-expr"]["expr-type"], "id");
				ASSERT_EQ(v["var-type"]["size-expr"]["name"], "n");
				ASSERT_EQ(v["var-type"]["base-type"]["type-kind"], "prim");
				ASSERT_EQ(v["var-type"]["base-type"]["type-name"], "int64");
				found_arr = true;
			}
		}
	}
	ASSERT_TRUE(found_buf);
	ASSERT_TRUE(found_arr);
}

TEST(gen_ast, arr_index_expr) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/015_arr_index_expr.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	// statements: var-decl, expr(a[0]), expr(a[i+1])
	ASSERT_EQ(jout["ast"]["statements"].size(), 3);

	// a[0]: expr stmt with arr-index body
	const auto& s1 = jout["ast"]["statements"][1];
	ASSERT_EQ(s1["stmt-type"], "expr");
	ASSERT_EQ(s1["body"]["expr-type"], "arr-index");
	ASSERT_EQ(s1["body"]["array"]["expr-type"], "id");
	ASSERT_EQ(s1["body"]["array"]["name"], "a");
	ASSERT_EQ(s1["body"]["index"]["expr-type"], "lit-int");
	ASSERT_EQ(s1["body"]["index"]["value"], "0");

	// a[i+1]: arr-index with add expression as index
	const auto& s2 = jout["ast"]["statements"][2];
	ASSERT_EQ(s2["stmt-type"], "expr");
	ASSERT_EQ(s2["body"]["expr-type"], "arr-index");
	ASSERT_EQ(s2["body"]["index"]["expr-type"], "add");
}

TEST(gen_ast, arr_assign_stmt) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/016_arr_assign_stmt.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	// statements: var-decl, arr-assign(1->a[0]), arr-assign(x->a[i])
	ASSERT_EQ(jout["ast"]["statements"].size(), 3);

	// 1 -> a[0]
	const auto& s1 = jout["ast"]["statements"][1];
	ASSERT_EQ(s1["stmt-type"], "arr-assign");
	ASSERT_EQ(s1["target"]["expr-type"], "arr-index");
	ASSERT_EQ(s1["target"]["array"]["name"], "a");
	ASSERT_EQ(s1["target"]["index"]["expr-type"], "lit-int");
	ASSERT_EQ(s1["target"]["index"]["value"], "0");
	ASSERT_EQ(s1["value"]["expr-type"], "lit-int");
	ASSERT_EQ(s1["value"]["value"], "1");

	// x -> a[i]
	const auto& s2 = jout["ast"]["statements"][2];
	ASSERT_EQ(s2["stmt-type"], "arr-assign");
	ASSERT_EQ(s2["target"]["array"]["name"], "a");
	ASSERT_EQ(s2["target"]["index"]["expr-type"], "id");
	ASSERT_EQ(s2["target"]["index"]["name"], "i");
	ASSERT_EQ(s2["value"]["expr-type"], "id");
	ASSERT_EQ(s2["value"]["name"], "x");
}

TEST(gen_ast, multidim_arr) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/017_multidim_arr.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 4);  // m-decl, n-decl, mat-decl, arr-assign

	// var-decl: [m][n]int32 mat
	const auto& mat_decl = stmts[2];
	ASSERT_EQ(mat_decl["stmt-type"], "var-decl");
	const auto& vt = mat_decl["vars"][0]["var-type"];
	ASSERT_EQ(vt["type-kind"], "arr");
	ASSERT_EQ(vt["specifier"], "raw");
	ASSERT_FALSE(vt["size-expr"].is_null());
	const auto& inner = vt["base-type"];
	ASSERT_EQ(inner["type-kind"], "arr");
	ASSERT_EQ(inner["specifier"], "raw");
	ASSERT_FALSE(inner["size-expr"].is_null());
	ASSERT_EQ(inner["base-type"]["type-kind"], "prim");
	ASSERT_EQ(inner["base-type"]["type-name"], "int32");

	// arr-assign: int32(1) -> mat[0][1]  (chained store_loc)
	const auto& assign = stmts[3];
	ASSERT_EQ(assign["stmt-type"], "arr-assign");
	const auto& target = assign["target"];
	ASSERT_EQ(target["expr-type"], "arr-index");           // outer index [1]
	ASSERT_EQ(target["array"]["expr-type"], "arr-index");  // inner index [0]
	ASSERT_EQ(target["array"]["array"]["expr-type"], "id");
	ASSERT_EQ(target["array"]["array"]["name"], "mat");
}

TEST(gen_ast, embed_arr_decl) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/019_embed_arr_decl.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 2);  // rows-decl, mat-decl

	// var-decl: [rows]$[4]int32 mat
	const auto& mat_decl = stmts[1];
	ASSERT_EQ(mat_decl["stmt-type"], "var-decl");
	const auto& vt = mat_decl["vars"][0]["var-type"];
	ASSERT_EQ(vt["type-kind"], "arr");
	ASSERT_EQ(vt["specifier"], "raw");
	ASSERT_FALSE(vt["size-expr"].is_null());
	ASSERT_TRUE(vt.value("embedded", false));
	const auto& inner = vt["base-type"];
	ASSERT_EQ(inner["type-kind"], "arr");
	ASSERT_EQ(inner["specifier"], "raw");
	ASSERT_FALSE(inner["size-expr"].is_null());
	ASSERT_EQ(inner["base-type"]["type-kind"], "prim");
	ASSERT_EQ(inner["base-type"]["type-name"], "int32");

	// []$[4]int32 param: same shape as the sized form, with a null outer size
	const auto& pvt = jout["ast"]["functions"][0]["parameters"][0]["var-type"];
	ASSERT_TRUE(pvt["size-expr"].is_null());
	ASSERT_TRUE(pvt.value("embedded", false));
	ASSERT_EQ(pvt["base-type"]["type-kind"], "arr");
	ASSERT_EQ(pvt["base-type"]["base-type"]["type-name"], "int32");
}

TEST(gen_ast, logical_ops) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/018_logical_ops.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 6);  // 2 var-decls + 4 expr-stmts

	// a && b
	ASSERT_EQ(stmts[2]["stmt-type"], "expr");
	const auto& land = stmts[2]["body"];
	ASSERT_EQ(land["expr-type"], "logical-and");
	ASSERT_EQ(land["left"]["name"],  "a");
	ASSERT_EQ(land["right"]["name"], "b");

	// a || b
	const auto& lor = stmts[3]["body"];
	ASSERT_EQ(lor["expr-type"], "logical-or");
	ASSERT_EQ(lor["left"]["name"],  "a");
	ASSERT_EQ(lor["right"]["name"], "b");

	// !a
	const auto& lnot = stmts[4]["body"];
	ASSERT_EQ(lnot["expr-type"], "logical-not");
	ASSERT_EQ(lnot["operand"]["name"], "a");

	// a && b || !a  →  (a && b) || (!a)  (&& binds tighter than ||)
	const auto& mixed = stmts[5]["body"];
	ASSERT_EQ(mixed["expr-type"], "logical-or");
	ASSERT_EQ(mixed["left"]["expr-type"],  "logical-and");
	ASSERT_EQ(mixed["right"]["expr-type"], "logical-not");
}

TEST(gen_ast, bitwise_ops) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/036_bitwise_ops.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 8);  // 2 var-decls + 6 expr-stmts

	// a & b
	const auto& band = stmts[2]["body"];
	ASSERT_EQ(band["expr-type"], "bitand");
	ASSERT_EQ(band["left"]["name"],  "a");
	ASSERT_EQ(band["right"]["name"], "b");

	// a | b
	const auto& bor = stmts[3]["body"];
	ASSERT_EQ(bor["expr-type"], "bitor");
	ASSERT_EQ(bor["left"]["name"],  "a");
	ASSERT_EQ(bor["right"]["name"], "b");

	// a ^ b
	const auto& bxor = stmts[4]["body"];
	ASSERT_EQ(bxor["expr-type"], "bitxor");
	ASSERT_EQ(bxor["left"]["name"],  "a");
	ASSERT_EQ(bxor["right"]["name"], "b");

	// ~a
	const auto& bnot = stmts[5]["body"];
	ASSERT_EQ(bnot["expr-type"], "bitnot");
	ASSERT_EQ(bnot["operand"]["name"], "a");

	// a & b == c  →  (a & b) == c  (& binds tighter than comparisons)
	const auto& prec1 = stmts[6]["body"];
	ASSERT_EQ(prec1["expr-type"], "cmp");
	ASSERT_EQ(prec1["left"]["expr-type"], "bitand");

	// a + b | c  →  a + (b | c)  (| binds tighter than +, same class as *, so this
	// ticket's deliberately simpler precedence choice does NOT match C's `(a+b)|c`)
	const auto& prec2 = stmts[7]["body"];
	ASSERT_EQ(prec2["expr-type"], "add");
	ASSERT_EQ(prec2["left"]["expr-type"], "id");
	ASSERT_EQ(prec2["right"]["expr-type"], "bitor");
}

TEST(gen_ast, addr_of) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/104_addr_of.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 4);

	// @int64 p = @x;
	const auto& p = stmts[2]["vars"][0];
	ASSERT_EQ(p["var-type"]["type-kind"], "pntr");
	ASSERT_EQ(p["var-type"]["mutable"], false);
	const auto& addr_x = p["init"];
	ASSERT_EQ(addr_x["expr-type"], "addr-of");
	ASSERT_EQ(addr_x["object"]["expr-type"], "id");
	ASSERT_EQ(addr_x["object"]["name"], "x");
	ASSERT_EQ(addr_x["mutable"], false);
	ASSERT_FALSE(addr_x["loc"].is_null());

	// @!int64 q = @!y;
	const auto& q = stmts[3]["vars"][0];
	ASSERT_EQ(q["var-type"]["type-kind"], "pntr");
	ASSERT_EQ(q["var-type"]["mutable"], true);
	const auto& addr_y = q["init"];
	ASSERT_EQ(addr_y["expr-type"], "addr-of");
	ASSERT_EQ(addr_y["object"]["expr-type"], "id");
	ASSERT_EQ(addr_y["object"]["name"], "y");
	ASSERT_EQ(addr_y["mutable"], true);
}

TEST(gen_ast, addr_of_field) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/106_addr_of_field.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 6);

	// @int64 rp = @s.x;
	const auto& addr_x = stmts[3]["vars"][0]["init"];
	ASSERT_EQ(addr_x["expr-type"], "addr-of");
	ASSERT_EQ(addr_x["mutable"], false);
	ASSERT_EQ(addr_x["object"]["expr-type"], "field-access");
	ASSERT_EQ(addr_x["object"]["field"], "x");
	ASSERT_EQ(addr_x["object"]["object"]["expr-type"], "id");
	ASSERT_EQ(addr_x["object"]["object"]["name"], "s");

	// @!int64 wp = @!s.y;
	const auto& addr_y = stmts[4]["vars"][0]["init"];
	ASSERT_EQ(addr_y["expr-type"], "addr-of");
	ASSERT_EQ(addr_y["mutable"], true);
	ASSERT_EQ(addr_y["object"]["field"], "y");

	// @!int64 np = @!s.in.v;  (nested field chain)
	const auto& addr_v = stmts[5]["vars"][0]["init"];
	ASSERT_EQ(addr_v["expr-type"], "addr-of");
	ASSERT_EQ(addr_v["mutable"], true);
	ASSERT_EQ(addr_v["object"]["expr-type"], "field-access");
	ASSERT_EQ(addr_v["object"]["field"], "v");
	ASSERT_EQ(addr_v["object"]["object"]["expr-type"], "field-access");
	ASSERT_EQ(addr_v["object"]["object"]["field"], "in");
	ASSERT_EQ(addr_v["object"]["object"]["object"]["name"], "s");
}

TEST(gen_ast, addr_of_arr_index) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/107_addr_of_arr_index.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 3);

	// @int64 rp = @arr[1];
	const auto& addr_rp = stmts[1]["vars"][0]["init"];
	ASSERT_EQ(addr_rp["expr-type"], "addr-of");
	ASSERT_EQ(addr_rp["mutable"], false);
	ASSERT_EQ(addr_rp["object"]["expr-type"], "arr-index");
	ASSERT_EQ(addr_rp["object"]["array"]["expr-type"], "id");
	ASSERT_EQ(addr_rp["object"]["array"]["name"], "arr");
	ASSERT_EQ(addr_rp["object"]["index"]["value"], "1");

	// @!int64 wp = @!arr[2];
	const auto& addr_wp = stmts[2]["vars"][0]["init"];
	ASSERT_EQ(addr_wp["expr-type"], "addr-of");
	ASSERT_EQ(addr_wp["mutable"], true);
	ASSERT_EQ(addr_wp["object"]["expr-type"], "arr-index");
	ASSERT_EQ(addr_wp["object"]["index"]["value"], "2");
}

TEST(gen_ast, addr_of_call) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/132_addr_of_call.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 4);

	const auto& ro = stmts[0]["body"]["args"][0];
	ASSERT_EQ(ro["expr-type"], "addr-of");
	ASSERT_EQ(ro["mutable"], false);
	ASSERT_EQ(ro["object"]["expr-type"], "call");
	ASSERT_EQ(ro["object"]["name"], "g");
	ASSERT_EQ(ro["object"]["args"].size(), 1);
	ASSERT_TRUE(ro["object"].contains("loc"));

	const auto& rw = stmts[1]["body"]["args"][0];
	ASSERT_EQ(rw["mutable"], true);
	ASSERT_EQ(rw["object"]["expr-type"], "call");

	// A call as an index base is an ordinary expression; a type-name cast stays unsupported.
	ASSERT_EQ(stmts[2]["body"]["args"][0]["object"]["array"]["expr-type"], "call");
	ASSERT_EQ(stmts[3]["body"]["args"][0]["object"]["expr-type"], "not-impl");
}

TEST(gen_ast, addr_of_member_call) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/133_addr_of_member_call.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 5);

	// '@v.f()' is '@(v.f())' as '@v.x' is '@(v.x)', not '(@v).f()'.
	const auto& ro = stmts[0]["body"]["args"][0];
	ASSERT_EQ(ro["expr-type"], "addr-of");
	ASSERT_EQ(ro["mutable"], false);
	ASSERT_EQ(ro["object"]["expr-type"], "member-call");
	ASSERT_EQ(ro["object"]["object"]["name"], "v");
	ASSERT_EQ(ro["object"]["method"], "f");
	ASSERT_TRUE(ro["object"].contains("loc"));

	const auto& rw = stmts[1]["body"]["args"][0];
	ASSERT_EQ(rw["mutable"], true);
	ASSERT_EQ(rw["object"]["expr-type"], "member-call");
	ASSERT_EQ(rw["object"]["object"]["name"], "M");
	ASSERT_EQ(rw["object"]["args"].size(), 1);

	const auto& chained = stmts[2]["body"]["args"][0]["object"];
	ASSERT_EQ(chained["expr-type"], "member-call");
	ASSERT_EQ(chained["object"]["expr-type"], "call");
	ASSERT_EQ(chained["object"]["name"], "mk");

	ASSERT_EQ(stmts[3]["body"]["args"][0]["object"]["expr-type"], "not-impl");

	const auto& neg = stmts[4]["vars"][0]["init"];
	ASSERT_EQ(neg["expr-type"], "neg");
	ASSERT_EQ(neg["operand"]["expr-type"], "member-call");
}

TEST(gen_ast, addr_of_type_alias_mix) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/105_addr_of_type_alias_mix.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 4);

	// type IntPtr = @int64;  (type-position '@', must still resolve as a type)
	ASSERT_EQ(stmts[0]["stmt-type"], "type-alias");
	ASSERT_EQ(stmts[0]["type"]["type-kind"], "pntr");

	// @z;
	ASSERT_EQ(stmts[2]["body"]["expr-type"], "addr-of");
	ASSERT_EQ(stmts[2]["body"]["object"]["expr-type"], "id");
	ASSERT_EQ(stmts[2]["body"]["object"]["name"], "z");

	// @!z;
	ASSERT_EQ(stmts[3]["body"]["expr-type"], "addr-of");
	ASSERT_EQ(stmts[3]["body"]["mutable"], true);
}

TEST(gen_ast, uint_literal) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/020_uint_literal.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 1);

	const auto& decl = stmts[0];
	ASSERT_EQ(decl["stmt-type"], "var-decl");
	const auto& var = decl["vars"][0];
	ASSERT_EQ(var["var-type"]["type-kind"], "prim");
	ASSERT_EQ(var["var-type"]["type-name"], "uint64");

	const auto& init = var["init"];
	ASSERT_EQ(init["expr-type"], "lit-uint");
	ASSERT_EQ(init["value"], "18446744073709551615");
}

TEST(gen_ast, member_call) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/021_member_call.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 2);

	// M.f(1, 2) → member-call
	const auto& mc = stmts[0]["body"];
	ASSERT_EQ(mc["expr-type"], "member-call");
	ASSERT_EQ(mc["object"]["expr-type"], "id");
	ASSERT_EQ(mc["object"]["name"], "M");
	ASSERT_EQ(mc["method"], "f");
	ASSERT_EQ(mc["args"].size(), 2);

	// f(1, 2) → call (unchanged)
	const auto& c = stmts[1]["body"];
	ASSERT_EQ(c["expr-type"], "call");
	ASSERT_EQ(c["name"], "f");
	ASSERT_EQ(c["args"].size(), 2);
}

TEST(gen_ast, char_literal) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/120_char_literal.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	auto& stmts = jout["ast"]["statements"];
	const char* expected[] = {"97", "10", "39", "92", "0", "32", "13", "9"};
	ASSERT_EQ(stmts.size(), 8u);
	for (int i = 0; i < 8; i++) {
		auto& init = stmts[i]["vars"][0]["init"];
		EXPECT_EQ(init["expr-type"], "lit-int") << i;
		EXPECT_EQ(init["value"], expected[i]) << i;
	}
	ASSERT_EQ(stmts[0]["vars"][0]["init"]["loc"], json::parse("[1, 11, 1, 14]"));
	ASSERT_EQ(stmts[0]["vars"][0]["init"]["char"], true);
}

// A character literal is the code point it holds, written as is or as '\u{X}';
// in a string literal '\u{X}' writes the code point's UTF-8 bytes.
TEST(gen_ast, code_point_literal) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/159_code_point_literal.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	auto& stmts = jout["ast"]["statements"];
	const char* expected[] = {"233", "128512", "8364", "65"};
	for (int i = 0; i < 4; i++) {
		auto& init = stmts[i]["vars"][0]["init"];
		EXPECT_EQ(init["value"], expected[i]) << i;
		EXPECT_EQ(init["char"], true) << i;
	}
	ASSERT_EQ(stmts[4]["value"]["value"], "x€y😀\\x00");
	// '\x' without two hex digits, or an unknown escape, is the letter itself.
	ASSERT_EQ(stmts[5]["value"]["value"], "\\x07\\x08\\n\\r\\t\\x0B\\x00xZqJ");
}

TEST(gen_ast, copy_func)
{
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/162_copy_func.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const json& f = jout["ast"]["functions"][0];
	ASSERT_EQ(f["name"], "->");
	ASSERT_EQ(f["parameters"].size(), 2u);
	ASSERT_FALSE(jout.contains("export"));
}

// '<<' binds as the shifts do: tighter than '->', looser than '+'.
TEST(gen_ast, append)
{
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/161_append.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	ASSERT_EQ(jout["ast"]["functions"][0]["name"], "<<");
	auto& stmts = jout["ast"]["statements"];
	auto& sum = stmts[0]["body"];
	ASSERT_EQ(sum["expr-type"], "append");
	ASSERT_EQ(sum["right"]["expr-type"], "add");
	ASSERT_EQ(stmts[1]["body"]["expr-type"], "chain-call");
	ASSERT_EQ(stmts[1]["body"]["value"]["expr-type"], "append");
	auto& chain = stmts[2]["body"];
	ASSERT_EQ(chain["left"]["expr-type"], "append");
	ASSERT_EQ(chain["right"]["name"], "b");
	ASSERT_EQ(stmts[3]["value"]["expr-type"], "shl");
}

TEST(gen_ast, bool_literal) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/124_bool_literal.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);

	auto& stmts = jout["ast"]["statements"];
	json bool_type = {{"type-kind", "prim"}, {"type-name", "bool"}};
	auto& t = stmts[0]["vars"][0]["init"];
	EXPECT_EQ(t["expr-type"], "lit-int");
	EXPECT_EQ(t["value"], "1");
	EXPECT_EQ(t["value-type"], bool_type);
	EXPECT_EQ(t["loc"], json::parse("[1, 10, 1, 14]"));
	auto& f = stmts[1]["vars"][0]["init"];
	EXPECT_EQ(f["value"], "0");
	EXPECT_EQ(f["value-type"], bool_type);
}

TEST(gen_ast, linear_parse_time) {
	cleanTestEnv();
	const int n = 2000;
	auto parse = [](const string& name, const string& src) {
		ofstream("out/" + name) << src;
		string output = execTestCommand("palan-gen-ast out/" + name);
		EXPECT_TRUE(checkerr(output)) << name;
		return output[0] == '{' ? json::parse(output) : json();
	};

	const int depth = 60;
	string top, body = "func f() {\nint32 a = 0;\n", arr = "[]int32 a = [", args = "f(", nested = "func f() {\n";
	for (int i = 0; i < depth; i++) nested += "while 1 < 2 {\n";
	for (int i = 0; i < n; i++) {
		top += "int32 a" + to_string(i) + " = " + to_string(i) + ";\n";
		body += "a = a + 1;\n";
		nested += "a = a + 1;\n";
		arr += to_string(i) + ",";
		args += to_string(i) + ",";
	}
	body += "}\n";
	arr += "0];\n";
	args += "0);\n";
	nested += string(depth, '}') + "\n}\n";

	json jout = parse("top.pa", top);
	ASSERT_EQ(jout["ast"]["statements"].size(), n);
	jout = parse("body.pa", body);
	ASSERT_EQ(jout["ast"]["functions"][0]["block"]["body"].size(), n + 1);
	jout = parse("arr.pa", arr);
	ASSERT_EQ(jout["ast"]["statements"][0]["vars"][0]["init"]["items"].size(), n + 1);
	jout = parse("args.pa", args);
	ASSERT_EQ(jout["ast"]["statements"][0]["body"]["args"].size(), n + 1);
	jout = parse("nested.pa", nested);
	json* inner = &jout["ast"]["functions"][0]["block"]["body"][0];
	for (int i = 1; i < depth; i++) inner = &(*inner)["body"][0];
	ASSERT_EQ((*inner)["body"].size(), n);
}

TEST(gen_ast, deep_nesting) {
	// Deeper than the GLR stack's original initial size of 200 items, where
	// growing the stack used to double-free semantic values.
	cleanTestEnv();
	const int depth = 100;
	string src = "func f() {\nint32 a = 0;\n";
	for (int i = 0; i < depth; i++) src += "while 1 < 2 {\n";
	src += "a + 1 -> a;\n" + string(depth, '}') + "\n}\n";
	ofstream("out/deep_nesting.pa") << src;

	string output = execTestCommand("palan-gen-ast out/deep_nesting.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	json* inner = &jout["ast"]["functions"][0]["block"]["body"][1];
	for (int i = 1; i < depth; i++) inner = &(*inner)["body"][0];
	ASSERT_EQ((*inner)["body"][0]["stmt-type"], "assign");
}

TEST(gen_ast, sizeof) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/129_sizeof.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	const auto& a = stmts[0]["vars"][0]["init"];
	ASSERT_EQ(a["expr-type"], "sizeof");
	ASSERT_EQ(a["type"]["type-name"], "int32");
	const auto& b = stmts[1]["vars"][0]["init"];
	ASSERT_EQ(b["type"]["type-kind"], "arr");
	ASSERT_EQ(b["type"]["embedded"], true);
	ASSERT_EQ(b["type"]["base-type"]["type-name"], "Vec3f");
	ASSERT_EQ(stmts[2]["vars"][0]["init"]["expr-type"], "not-impl");
}

TEST(gen_ast, float_exponent_no_point) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/131_float_exponent_no_point.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	const char* values[] = {"1e10", "1E-3", "2e+5"};
	for (int i = 0; i < 3; i++) {
		const auto& init = stmts[i]["vars"][0]["init"];
		ASSERT_EQ(init["expr-type"], "lit-flo");
		ASSERT_EQ(init["value"], values[i]);
	}
	ASSERT_EQ(stmts[3]["vars"][0]["init"]["expr-type"], "lit-uint");
}

TEST(gen_ast, addr_of_lit) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/134_addr_of_lit.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 4);

	const auto& ro = stmts[0]["body"]["args"][0];
	ASSERT_EQ(ro["expr-type"], "addr-of");
	ASSERT_EQ(ro["mutable"], false);
	ASSERT_EQ(ro["object"]["expr-type"], "arr-lit");
	ASSERT_EQ(ro["object"]["items"].size(), 2);
	ASSERT_TRUE(ro.contains("loc"));

	// The row sequence is one 2D literal under '@!'.
	const auto& rows = stmts[1]["body"]["args"][0];
	ASSERT_EQ(rows["mutable"], true);
	ASSERT_EQ(rows["object"]["items"].size(), 2);
	ASSERT_EQ(rows["object"]["items"][1]["items"][0]["value"], "3");

	ASSERT_EQ(stmts[2]["body"]["args"][0]["object"]["expr-type"], "dict-lit");
	const auto& args = stmts[3]["body"]["args"];
	ASSERT_EQ(args[0]["mutable"], true);
	ASSERT_EQ(args[0]["object"]["expr-type"], "dict-lit");
	// '@' binds tighter than a binary operator.
	ASSERT_EQ(args[1]["expr-type"], "add");
	ASSERT_EQ(args[1]["left"]["expr-type"], "addr-of");
}

TEST(gen_ast, chain_call) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/135_chain_call.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 9);

	const auto& plain = stmts[0]["body"];
	ASSERT_EQ(plain["expr-type"], "chain-call");
	ASSERT_EQ(plain["value"]["name"], "x");
	ASSERT_EQ(plain["call"]["name"], "f");
	ASSERT_EQ(plain["call"]["args"].size(), 1);
	ASSERT_TRUE(plain["call"].contains("loc"));

	ASSERT_EQ(stmts[1]["body"]["value"]["expr-type"], "addr-of");
	ASSERT_EQ(stmts[1]["body"]["value"]["object"]["name"], "f");
	ASSERT_EQ(stmts[2]["body"]["call"]["expr-type"], "member-call");
	ASSERT_EQ(stmts[2]["body"]["call"]["object"]["name"], "M");

	// A call followed by '->' is evaluated once, as the assigned value.
	ASSERT_EQ(stmts[3]["stmt-type"], "assign");
	ASSERT_EQ(stmts[3]["value"]["expr-type"], "chain-call");

	// An assignment feeding a call is the call's value; SA reads its target back.
	ASSERT_EQ(stmts[4]["body"]["value"]["stmt-type"], "assign");
	ASSERT_EQ(stmts[4]["body"]["value"]["name"], "v");
	const auto& g = stmts[5]["body"];
	ASSERT_EQ(g["call"]["name"], "g");
	ASSERT_EQ(g["value"]["name"], "w");
	ASSERT_EQ(g["value"]["value"]["call"]["name"], "f");
	ASSERT_EQ(g["value"]["value"]["value"]["name"], "v");

	ASSERT_EQ(stmts[6]["stmt-type"], "tapple-decl");
	ASSERT_EQ(stmts[6]["value"]["expr-type"], "chain-call");
	ASSERT_EQ(stmts[7]["stmt-type"], "tapple-assign");
	ASSERT_EQ(stmts[7]["value"]["expr-type"], "chain-call");

	const auto& method = stmts[8]["body"];
	ASSERT_EQ(method["call"]["expr-type"], "member-call");
	ASSERT_EQ(method["call"]["object"]["name"], "s");
	ASSERT_EQ(method["value"]["call"]["object"]["expr-type"], "arr-index");
	ASSERT_EQ(method["value"]["call"]["method"], "f");
}

TEST(gen_ast, as_cast) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/136_as_cast.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 7);

	const auto& prim = stmts[0]["body"];
	ASSERT_EQ(prim["expr-type"], "cast");
	ASSERT_EQ(prim["target-type"]["type-name"], "int32");
	ASSERT_EQ(prim["src"]["name"], "x");
	ASSERT_TRUE(prim.contains("loc"));

	const auto& borrow = stmts[1]["body"]["target-type"];
	ASSERT_EQ(borrow["type-kind"], "pntr");
	ASSERT_EQ(borrow["mutable"], true);
	ASSERT_EQ(borrow["base-type"]["type-kind"], "arr");

	// 'as' binds tighter than a binary operator and looser than a unary one.
	ASSERT_EQ(stmts[2]["body"]["expr-type"], "add");
	ASSERT_EQ(stmts[2]["body"]["right"]["expr-type"], "cast");
	ASSERT_EQ(stmts[3]["body"]["expr-type"], "cast");
	ASSERT_EQ(stmts[4]["body"]["expr-type"], "cmp");
	ASSERT_EQ(stmts[4]["body"]["left"]["expr-type"], "cast");
	ASSERT_EQ(stmts[5]["body"]["array"]["expr-type"], "cast");
	ASSERT_EQ(stmts[6]["body"]["target-type"]["type-name"], "L.T");
}

TEST(gen_ast, shift) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/137_shift.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 5);

	// A shift binds looser than '+'/'-' and tighter than a comparison.
	ASSERT_EQ(stmts[0]["body"]["expr-type"], "shl");
	ASSERT_EQ(stmts[0]["body"]["left"]["expr-type"], "add");
	ASSERT_EQ(stmts[1]["body"]["right"]["expr-type"], "bitor");
	ASSERT_EQ(stmts[2]["body"]["expr-type"], "cmp");
	ASSERT_EQ(stmts[2]["body"]["left"]["expr-type"], "shr");
	ASSERT_EQ(stmts[3]["body"]["expr-type"], "shr");
	ASSERT_EQ(stmts[3]["body"]["left"]["expr-type"], "shl");
	// '>>' stays the mark that gives a value away.
	ASSERT_EQ(stmts[4]["body"]["args"][0]["expr-type"], "transfer");
}

TEST(gen_ast, give_take) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/152_give_take.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 6);

	// '>>' marks the value given away, binding tighter than 'as' and a binary operator.
	const auto& args = stmts[0]["body"]["args"];
	ASSERT_EQ(args[0]["expr-type"], "transfer");
	ASSERT_EQ(args[0]["value"]["name"], "a");
	ASSERT_EQ(args[1]["expr-type"], "cast");
	ASSERT_EQ(args[1]["src"]["expr-type"], "transfer");
	ASSERT_EQ(args[2]["expr-type"], "add");
	ASSERT_EQ(args[2]["right"]["expr-type"], "transfer");

	// '<<=' initializes a variable by taking over its value, also an inherited-type one.
	const auto& vars = stmts[1]["vars"];
	ASSERT_EQ(vars[0]["ownership-transfer"], true);
	ASSERT_EQ(vars[0]["init"]["expr-type"], "cast");
	ASSERT_EQ(vars[1]["ownership-transfer"], true);
	ASSERT_EQ(vars[1]["var-type"]["type-name"], "P");

	// 'v ->> f()' chains v given away.
	ASSERT_EQ(stmts[2]["body"]["expr-type"], "chain-call");
	ASSERT_EQ(stmts[2]["body"]["value"]["expr-type"], "transfer");
	ASSERT_EQ(stmts[2]["body"]["value"]["op"], "->>");
	ASSERT_EQ(stmts[3]["body"]["call"]["expr-type"], "member-call");
	ASSERT_EQ(stmts[3]["body"]["value"]["expr-type"], "transfer");

	// A parenthesized expression is called through, and '_' in a function type keeps a place.
	const auto& cast = stmts[4]["body"]["callee"];
	ASSERT_EQ(stmts[4]["body"]["expr-type"], "call-ptr");
	ASSERT_EQ(stmts[4]["body"]["args"].size(), 2);
	ASSERT_EQ(cast["expr-type"], "cast");
	const auto& func = cast["target-type"]["base-type"];
	ASSERT_EQ(func["parameters"][0]["var-type"]["type-kind"], "keep");
	ASSERT_EQ(func["parameters"][1]["var-type"]["type-name"], "P");
	ASSERT_EQ(func["ret-type"]["type-kind"], "keep");
	ASSERT_EQ(stmts[5]["body"]["callee"]["name"], "g");
	ASSERT_EQ(stmts[5]["body"]["args"].size(), 0);
}

// 'as' after a '->>' target views that location as the owner it receives; after
// a chained call it still casts the call's result.
TEST(gen_ast, owner_view_target) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/153_owner_view_target.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 4);
	ASSERT_EQ(stmts[0]["stmt-type"], "field-assign");
	ASSERT_EQ(stmts[0]["ownership-transfer"], true);
	ASSERT_EQ(stmts[0]["owner-view-type"]["size-expr"]["value"], "8");
	ASSERT_EQ(stmts[1]["stmt-type"], "assign");
	ASSERT_EQ(stmts[1]["owner-view-type"]["size-expr"]["name"], "n");
	ASSERT_EQ(stmts[2]["stmt-type"], "arr-assign");
	ASSERT_TRUE(stmts[2].contains("owner-view-type"));
	const auto& init = stmts[3]["vars"][0]["init"];
	ASSERT_EQ(init["expr-type"], "cast");
	ASSERT_EQ(init["src"]["expr-type"], "chain-call");
}

// An operator names a function where a function name can be written: its
// definition, an import target and a global's permit.
TEST(gen_ast, operator_func) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/154_operator_func.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& fs = jout["ast"]["functions"];
	ASSERT_EQ(fs.size(), 2);
	ASSERT_EQ(fs[0]["name"], "+");
	ASSERT_EQ(fs[0]["parameters"].size(), 2);
	ASSERT_EQ(fs[1]["name"], "-");
	ASSERT_EQ(fs[1]["export"], true);
	ASSERT_EQ(fs[1]["parameters"].size(), 1);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts[1]["targets"], json::array({"vec3", "*", "%"}));
	ASSERT_EQ(stmts[3]["funcs"][0]["name"], "/");
	ASSERT_EQ(stmts[3]["funcs"][0]["writable"], true);
	ASSERT_EQ(stmts[3]["funcs"][1]["name"], "+");
}

// '->' binds tighter than comparisons and logical operators, looser than arithmetic.
TEST(gen_ast, arrow_precedence) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/146_arrow_precedence.pa");
	ASSERT_TRUE(checkerr(output));
	json ast = json::parse(output)["ast"];
	const auto& stmts = ast["statements"];
	// i < a -> len()
	ASSERT_EQ(stmts[0]["body"]["expr-type"], "cmp");
	ASSERT_EQ(stmts[0]["body"]["right"]["expr-type"], "chain-call");
	ASSERT_EQ(stmts[0]["body"]["right"]["value"]["name"], "a");
	// a == b && c -> f()
	ASSERT_EQ(stmts[1]["body"]["expr-type"], "logical-and");
	ASSERT_EQ(stmts[1]["body"]["left"]["expr-type"], "cmp");
	ASSERT_EQ(stmts[1]["body"]["right"]["expr-type"], "chain-call");
	// x + 1 -> x
	ASSERT_EQ(stmts[2]["stmt-type"], "assign");
	ASSERT_EQ(stmts[2]["value"]["expr-type"], "add");
	// (x < y) -> b
	ASSERT_EQ(stmts[3]["stmt-type"], "assign");
	ASSERT_EQ(stmts[3]["value"]["expr-type"], "cmp");
}

// `[+]` levels nest under `[+]` and `[n]`, and a literal row may be empty.
TEST(gen_ast, nested_varr) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/147_nested_varr.pa");
	ASSERT_TRUE(checkerr(output));
	json ast = json::parse(output)["ast"];
	const auto& stmts = ast["statements"];
	const auto& m = stmts[0]["vars"][0];
	ASSERT_EQ(m["var-type"]["specifier"], "variable");
	ASSERT_EQ(m["var-type"]["base-type"]["specifier"], "variable");
	ASSERT_EQ(m["init"]["items"][1]["expr-type"], "arr-lit");
	ASSERT_EQ(m["init"]["items"][1]["items"].size(), 0);
	const auto& a = stmts[1]["vars"][0]["var-type"];
	ASSERT_EQ(a["specifier"], "raw");
	ASSERT_EQ(a["base-type"]["specifier"], "variable");
	ASSERT_EQ(stmts[2]["stmt-type"], "arr-assign");
	ASSERT_EQ(stmts[2]["value"]["items"].size(), 0);
}

// 'in' and 'copy' are matched by spelling, so a field or variable may still be named so.
TEST(gen_ast, for_loop) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/148_for.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts[0]["stmt-type"], "for");
	ASSERT_EQ(stmts[0]["elem"]["name"], "a");
	ASSERT_EQ(stmts[0]["elem"]["borrow"], false);
	ASSERT_FALSE(stmts[0].contains("index"));
	ASSERT_EQ(stmts[0]["range"]["name"], "v");
	ASSERT_EQ(stmts[0]["body"].size(), 0);
	ASSERT_EQ(stmts[1]["elem"]["borrow"], true);
	ASSERT_EQ(stmts[1]["elem"]["mutable"], false);
	ASSERT_EQ(stmts[1]["index"], "i");
	ASSERT_EQ(stmts[1]["range"]["field"], "in");
	ASSERT_EQ(stmts[1]["body"][0]["stmt-type"], "continue");
	ASSERT_EQ(stmts[2]["elem"]["mutable"], true);
	ASSERT_EQ(stmts[2]["elem"]["loc"][1], 5);
	ASSERT_EQ(stmts[2]["range"]["expr-type"], "call");
	ASSERT_EQ(stmts[3]["elem"]["name"], "b");
	ASSERT_EQ(stmts[3]["elem"]["borrow"], false);
	ASSERT_EQ(stmts[3]["range"]["name"], "copy");
}

TEST(gen_ast, for_loop_index_type) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/150_for_index_type.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts[0]["index"], "i");
	ASSERT_EQ(stmts[0]["index-type"]["type-name"], "int32");
	ASSERT_EQ(stmts[0]["range"]["name"], "v");
	ASSERT_EQ(stmts[1]["elem"]["borrow"], false);
	ASSERT_EQ(stmts[1]["index-type"]["type-name"], "Idx");
}

TEST(gen_ast, global_decl) {
	// A `for` list becomes its own global-permit statement after the var-decl,
	// the same statement a later `global name for ...;` produces.
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/151_global.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 6);
	ASSERT_EQ(stmts[0]["stmt-type"], "var-decl");
	ASSERT_EQ(stmts[0]["global"], true);
	ASSERT_EQ(stmts[0]["vars"][0]["name"], "hits");
	ASSERT_EQ(stmts[1]["stmt-type"], "global-permit");
	ASSERT_EQ(stmts[1]["name"], "hits");
	ASSERT_EQ(stmts[1]["funcs"][0]["name"], "count");
	ASSERT_EQ(stmts[1]["funcs"][0]["writable"], true);
	ASSERT_EQ(stmts[1]["funcs"][1]["name"], "report");
	ASSERT_EQ(stmts[1]["funcs"][1]["writable"], false);
	ASSERT_EQ(stmts[2]["global"], true);
	ASSERT_FALSE(stmts[2]["vars"][0].contains("init"));
	ASSERT_EQ(stmts[3]["stmt-type"], "global-permit");
	ASSERT_EQ(stmts[3]["funcs"].size(), 1);
	ASSERT_EQ(stmts[4]["stmt-type"], "global-permit");
	ASSERT_EQ(stmts[4]["name"], "optind");
	ASSERT_TRUE(stmts[4]["funcs"].empty());
	ASSERT_EQ(stmts[5]["stmt-type"], "not-impl");
}

// An exported global is marked as an exported type is; a permit names an
// imported one with its alias as one name.
TEST(gen_ast, export_global) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/165_export_global.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts.size(), 6);
	ASSERT_EQ(stmts[0]["global"], true);
	ASSERT_EQ(stmts[0]["export"], true);
	ASSERT_EQ(stmts[1]["stmt-type"], "global-permit");
	ASSERT_EQ(stmts[1]["name"], "count");
	ASSERT_EQ(stmts[2]["export"], true);
	ASSERT_EQ(stmts[2]["vars"][0]["name"], "arr");
	ASSERT_EQ(stmts[3]["stmt-type"], "global-permit");
	ASSERT_EQ(stmts[3]["name"], "M.count");
	ASSERT_EQ(stmts[3]["funcs"][1]["writable"], true);
	ASSERT_EQ(stmts[4]["name"], "M.count");
	ASSERT_TRUE(stmts[4]["funcs"].empty());
	ASSERT_EQ(stmts[5]["stmt-type"], "not-impl");
	ASSERT_FALSE(stmts[5].contains("export"));
}

TEST(gen_ast, str_lit_bytes) {
	cleanTestEnv();
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/158_str_lit_bytes.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const auto& stmts = jout["ast"]["statements"];
	ASSERT_EQ(stmts[0]["value"]["value"], "x\\xFFy\\x00z");
	ASSERT_EQ(stmts[1]["value"]["value"], "\\\\\\\"\\n\\t\\r\\x01");
	// A valid UTF-8 character stays as it is however it was written; a byte written raw is escaped.
	ASSERT_EQ(stmts[2]["value"]["value"], "€€\\xFFé");
}

// A marker is a word only before a field's type: 'private' and 'readonly' stay names.
TEST(gen_ast, hidden_fields)
{
	string output = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/163_hidden_fields.pa");
	ASSERT_TRUE(checkerr(output));
	json jout = json::parse(output);
	const json& f = jout["ast"]["statements"][0]["fields"];
	ASSERT_EQ(f[0]["access"], "private");
	ASSERT_EQ(f[1]["access"], "readonly");
	ASSERT_EQ(f[2]["name"], "private");
	ASSERT_EQ(f[2]["access"], "private");
	ASSERT_EQ(f[3]["var-type"]["type-name"], "readonly");
	ASSERT_FALSE(f[3].contains("access"));
}
