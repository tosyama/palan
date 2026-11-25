/// semantic analyzer tests: arrays
///
/// @file arrayTests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include <map>
#include <fstream>
#include "../test-base/testBase.h"
#include "saTestUtil.h"

using namespace std;

TEST(sa, array_top_level_malloc) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/043_array_top_level.pa");
	ASSERT_TRUE(jout.is_object());

	bool found_buf = false, found_arr = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "var-decl") continue;
		for (auto& v : stmt["vars"]) {
			if (v["name"] == "buf") {
				// arr → pntr/uint8
				ASSERT_EQ(v["var-type"]["type-kind"], "pntr");
				ASSERT_EQ(v["var-type"]["base-type"]["type-name"], "uint8");
				// init = __pln_malloc(64)
				ASSERT_EQ(v["init"]["expr-type"], "call");
				ASSERT_EQ(v["init"]["name"], "__pln_malloc");
				ASSERT_EQ(v["init"]["func-type"], "c");
				ASSERT_EQ(v["init"]["args"][0]["value"], "64");
				found_buf = true;
			}
			if (v["name"] == "arr") {
				// arr → pntr/int64
				ASSERT_EQ(v["var-type"]["type-kind"], "pntr");
				ASSERT_EQ(v["var-type"]["base-type"]["type-name"], "int64");
				// init = __pln_malloc(__arr_d0 * 8), the run-time size held in __arr_d0
				ASSERT_EQ(v["init"]["expr-type"], "call");
				ASSERT_EQ(v["init"]["name"], "__pln_malloc");
				ASSERT_EQ(v["init"]["args"][0]["expr-type"], "mul");
				ASSERT_EQ(v["init"]["args"][0]["left"]["name"], "__arr_d0");
				ASSERT_EQ(v["init"]["args"][0]["right"]["value"], "8");
				found_arr = true;
			}
		}
	}
	ASSERT_TRUE(found_buf);
	ASSERT_TRUE(found_arr);
	// Top-level: no free() stmt in top-level statements
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "expr") continue;
		ASSERT_NE(stmt["body"].value("name", ""), "free");
	}
}

TEST(sa, array_func_scope_free) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/044_array_scope_free.pa");
	ASSERT_TRUE(jout.is_object());
	ASSERT_FALSE(jout["functions"].empty());

	auto& body = jout["functions"][0]["body"];
	ASSERT_GE(body.size(), 2u);
	// body[0]: var-decl with pntr type and malloc init
	ASSERT_EQ(body[0]["stmt-type"], "var-decl");
	ASSERT_EQ(body[0]["vars"][0]["name"], "buf");
	ASSERT_EQ(body[0]["vars"][0]["var-type"]["type-kind"], "pntr");
	ASSERT_EQ(body[0]["vars"][0]["init"]["name"], "__pln_malloc");
	// body.back(): free(buf)
	auto& last = body.back();
	ASSERT_EQ(last["stmt-type"], "expr");
	ASSERT_EQ(last["body"]["name"], "free");
	ASSERT_EQ(last["body"]["args"][0]["name"], "buf");
}

TEST(sa, array_while_break_free) {
	// while loop with array + break: SA must insert free(buf) before break
	// and at end of while body.  Covers collectFreeStmts path for break.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/046_array_while_break.pa");
	ASSERT_TRUE(jout.is_object());
	// Find the while statement
	json* while_stmt = nullptr;
	for (auto& s : jout["statements"])
		if (s["stmt-type"] == "while") { while_stmt = &s; break; }
	ASSERT_NE(while_stmt, nullptr);

	auto& body = (*while_stmt)["body"];
	// body[0]: var-decl buf = __pln_malloc(64)
	ASSERT_EQ(body[0]["stmt-type"], "var-decl");
	ASSERT_EQ(body[0]["vars"][0]["name"], "buf");
	ASSERT_EQ(body[0]["vars"][0]["init"]["name"], "__pln_malloc");

	// body.back(): free(buf) at end of while body (normal iteration exit)
	ASSERT_EQ(body.back()["stmt-type"], "expr");
	ASSERT_EQ(body.back()["body"]["name"], "free");
	ASSERT_EQ(body.back()["body"]["args"][0]["name"], "buf");

	// Find if stmt and verify free(buf) is inserted before break in then block
	json* if_stmt = nullptr;
	for (auto& s : body)
		if (s["stmt-type"] == "if") { if_stmt = &s; break; }
	ASSERT_NE(if_stmt, nullptr);
	auto& then_body = (*if_stmt)["then"]["body"];
	ASSERT_GE(then_body.size(), 2u);
	ASSERT_EQ(then_body[0]["stmt-type"], "expr");
	ASSERT_EQ(then_body[0]["body"]["name"], "free");
	ASSERT_EQ(then_body[1]["stmt-type"], "break");
}

TEST(sa, array_while_continue_free) {
	// while loop with array + continue: SA must insert free(buf) before continue
	// and at end of while body.  Covers collectFreeStmts path for continue.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/047_array_while_continue.pa");
	ASSERT_TRUE(jout.is_object());
	json* while_stmt = nullptr;
	for (auto& s : jout["statements"])
		if (s["stmt-type"] == "while") { while_stmt = &s; break; }
	ASSERT_NE(while_stmt, nullptr);

	auto& body = (*while_stmt)["body"];
	// body[0]: var-decl buf = __pln_malloc(64)
	ASSERT_EQ(body[0]["stmt-type"], "var-decl");
	ASSERT_EQ(body[0]["vars"][0]["name"], "buf");

	// body.back(): free(buf) at end of while body
	ASSERT_EQ(body.back()["stmt-type"], "expr");
	ASSERT_EQ(body.back()["body"]["name"], "free");
	ASSERT_EQ(body.back()["body"]["args"][0]["name"], "buf");

	// Find if stmt and verify free(buf) is inserted before continue in then block
	json* if_stmt = nullptr;
	for (auto& s : body)
		if (s["stmt-type"] == "if") { if_stmt = &s; break; }
	ASSERT_NE(if_stmt, nullptr);
	auto& then_body = (*if_stmt)["then"]["body"];
	ASSERT_GE(then_body.size(), 2u);
	ASSERT_EQ(then_body[0]["stmt-type"], "expr");
	ASSERT_EQ(then_body[0]["body"]["name"], "free");
	ASSERT_EQ(then_body[1]["stmt-type"], "continue");
}

TEST(sa, arr_index_elem_type) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/048_arr_index_elem_type.pa");
	ASSERT_TRUE(jout.is_object());

	// statements: [0] var-decl a (malloc), [1] var-decl i, [2] var-decl x (a[i])
	ASSERT_GE(jout["statements"].size(), 3u);
	const auto& decl = jout["statements"][2];
	ASSERT_EQ(decl["stmt-type"], "var-decl");

	const auto& body = decl["vars"][0]["init"];
	ASSERT_EQ(body["expr-type"],                    "arr-index");
	ASSERT_EQ(body["value-type"]["type-name"],       "int32");
	ASSERT_EQ(body["elem-size"]["expr-type"],        "lit-uint");
	ASSERT_EQ(body["elem-size"]["value"],            "4");
	ASSERT_EQ(body["array"]["name"],                 "a");
	ASSERT_EQ(body["array"]["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(body["index"]["name"],                 "i");
}

TEST(sa, arr_assign_convert) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/049_arr_assign_convert.pa");
	ASSERT_TRUE(jout.is_object());

	// statements: [0] var-decl a, [1] var-decl x, [2] var-decl i, [3] arr-assign
	ASSERT_GE(jout["statements"].size(), 4u);
	const auto& stmt = jout["statements"][3];
	ASSERT_EQ(stmt["stmt-type"], "arr-assign");

	// target is arr-index with value-type int64
	ASSERT_EQ(stmt["target"]["expr-type"],           "arr-index");
	ASSERT_EQ(stmt["target"]["value-type"]["type-name"], "int64");

	// value is wrapped in convert: int32 -> int64
	ASSERT_EQ(stmt["value"]["expr-type"],              "convert");
	ASSERT_EQ(stmt["value"]["from-type"]["type-name"], "int32");
	ASSERT_EQ(stmt["value"]["value-type"]["type-name"], "int64");
	ASSERT_EQ(stmt["value"]["src"]["name"],            "x");
}

TEST(sa, array_multi_var_shared_size) {
	// [4]int32 a, b; then [n]int32 c, d; -- the size is evaluated once per statement.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/045_array_multi_var.pa");
	ASSERT_TRUE(jout.is_object());
	auto& stmts = jout["statements"];
	ASSERT_GE(stmts.size(), 7u);

	// a and b: each its own var-decl, __pln_malloc(4 * 4) with the constant size
	for (int i = 0; i < 2; i++) {
		ASSERT_EQ(stmts[i]["stmt-type"], "var-decl");
		ASSERT_EQ(stmts[i]["vars"].size(), 1u);
		auto& v = stmts[i]["vars"][0];
		ASSERT_EQ(v["name"], i == 0 ? "a" : "b");
		ASSERT_EQ(v["var-type"]["base-type"]["type-name"], "int32");
		ASSERT_EQ(v["init"]["name"], "__pln_malloc");
		ASSERT_EQ(v["init"]["args"][0]["expr-type"], "mul");
		ASSERT_EQ(v["init"]["args"][0]["left"]["value"], "4");   // count
		ASSERT_EQ(v["init"]["args"][0]["right"]["value"], "4");  // elem_size of int32
	}

	// stmts[2] is `int64 n = 3;`. c's size is n, held in __c_d0; d's copies it.
	ASSERT_EQ(stmts[3]["vars"][0]["name"], "__c_d0");
	ASSERT_EQ(stmts[3]["vars"][0]["init"]["expr-type"], "convert");
	ASSERT_EQ(stmts[3]["vars"][0]["init"]["src"]["name"], "n");
	// A debug build checks the size once, before c's block is allocated.
	ASSERT_EQ(stmts[4]["stmt-type"], "if");
	ASSERT_EQ(stmts[4]["cond"]["left"]["src"]["name"], "__c_d0");
	ASSERT_EQ(stmts[4]["then"]["body"][0]["body"]["name"], "__pln_negative_size");
	ASSERT_EQ(stmts[5]["vars"][0]["name"], "c");
	ASSERT_EQ(stmts[5]["vars"][0]["init"]["args"][0]["left"]["name"], "__c_d0");
	ASSERT_EQ(stmts[6]["vars"][0]["name"], "__d_d0");
	ASSERT_EQ(stmts[6]["vars"][0]["init"]["name"], "__c_d0");
	ASSERT_EQ(stmts[7]["vars"][0]["name"], "d");
	ASSERT_EQ(stmts[7]["vars"][0]["init"]["args"][0]["left"]["name"], "__d_d0");
}

TEST(sa, array_size_unchecked_in_release) {
	cleanTestEnv();
	string pa = "../test/testdata/sa/045_array_multi_var.pa";
	genModuleAst(pa);
	ASSERT_EQ(execTestCommand("palan-sa --release " + moduleAstPath(pa) + " -o out/test.sa.json"), "");
	json jout = json::parse(ifstream("out/test.sa.json"));
	ASSERT_EQ(jout["statements"][3]["vars"][0]["name"], "__c_d0");
	ASSERT_EQ(jout["statements"][4]["vars"][0]["name"], "c");
}

TEST(sa, unsized_arr_sig) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/050_unsized_arr_sig.pa");
	ASSERT_TRUE(jout.is_object());

	auto findFunc = [&](const string& name) -> const json* {
		for (auto& f : jout["functions"])
			if (funcName(f["name"]) == name) return &f;
		return nullptr;
	};

	// getArr: []int32 return type → pntr(int32)
	const json* getArr = findFunc("getArr");
	ASSERT_NE(getArr, nullptr);
	ASSERT_EQ((*getArr)["ret-type"]["type-kind"], "pntr");
	ASSERT_EQ((*getArr)["ret-type"]["base-type"]["type-kind"], "prim");
	ASSERT_EQ((*getArr)["ret-type"]["base-type"]["type-name"], "int32");

	// sumArr: []int32 parameter → pntr(int32)
	const json* sumArr = findFunc("sumArr");
	ASSERT_NE(sumArr, nullptr);
	ASSERT_EQ((*sumArr)["parameters"][0]["var-type"]["type-kind"], "pntr");
	ASSERT_EQ((*sumArr)["parameters"][0]["var-type"]["base-type"]["type-kind"], "prim");
	ASSERT_EQ((*sumArr)["parameters"][0]["var-type"]["base-type"]["type-name"], "int32");

	// getNestedArr: [][]int32 return type → pntr(pntr(int32))
	const json* getNestedArr = findFunc("getNestedArr");
	ASSERT_NE(getNestedArr, nullptr);
	ASSERT_EQ((*getNestedArr)["ret-type"]["type-kind"], "pntr");
	ASSERT_EQ((*getNestedArr)["ret-type"]["base-type"]["type-kind"], "pntr");
	ASSERT_EQ((*getNestedArr)["ret-type"]["base-type"]["base-type"]["type-name"], "int32");
	ASSERT_FALSE((*getNestedArr)["ret-type"]["base-type"].contains("mutable"));

	// getSlotArr: []@![]int32 return type → pntr(writable slot pntr(int32))
	const json* getSlotArr = findFunc("getSlotArr");
	ASSERT_NE(getSlotArr, nullptr);
	ASSERT_EQ((*getSlotArr)["ret-type"]["base-type"]["mutable"], true);
	ASSERT_EQ((*getSlotArr)["ret-type"]["base-type"]["base-type"]["type-name"], "int32");
}

TEST(sa, pntr_arr_decl) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/051_pntr_arr_decl.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& body = jout["functions"][0]["body"];

	// body[0]: var-decl ptrs → pntr(pntr(int32)), __pln_malloc(5 * 8)
	ASSERT_EQ(body[0]["stmt-type"], "var-decl");
	const auto& var = body[0]["vars"][0];
	ASSERT_EQ(var["name"], "ptrs");
	ASSERT_EQ(var["var-type"]["type-kind"], "pntr");
	ASSERT_EQ(var["var-type"]["base-type"]["type-kind"], "pntr");
	ASSERT_EQ(var["var-type"]["base-type"]["base-type"]["type-name"], "int32");

	// __pln_malloc(5 * 8)
	ASSERT_EQ(var["init"]["name"], "__pln_malloc");
	ASSERT_EQ(var["init"]["args"][0]["expr-type"], "mul");
	ASSERT_EQ(var["init"]["args"][0]["left"]["value"], "5");
	ASSERT_EQ(var["init"]["args"][0]["right"]["value"], "8");

	// body[1]: ptrs[i] — arr-index with elem-size=8, value-type=pntr(int32)
	ASSERT_EQ(body[1]["stmt-type"], "var-decl");
	const auto& idx = body[1]["vars"][0]["init"];
	ASSERT_EQ(idx["expr-type"], "arr-index");
	ASSERT_EQ(idx["elem-size"]["value"], "8");
	ASSERT_EQ(idx["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(idx["value-type"]["base-type"]["type-name"], "int32");

	// free(ptrs), then the trailing return
	ASSERT_EQ(body.back()["stmt-type"], "return");
	const auto& last = body[body.size() - 2];
	ASSERT_EQ(last["stmt-type"], "expr");
	ASSERT_EQ(last["body"]["name"], "free");
	ASSERT_EQ(last["body"]["args"][0]["name"], "ptrs");
}

TEST(sa, ownership_transfer) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/052_ownership_transfer.pa");
	ASSERT_TRUE(jout.is_object());

	// functions[0]: testOwnedReturn() -> []int32
	{
		const auto& body = jout["functions"][0]["body"];
		// body[0]: var-decl arr
		ASSERT_EQ(body[0]["stmt-type"], "var-decl");
		ASSERT_EQ(body[0]["vars"][0]["name"], "arr");
		// body[1]: return arr — no free(arr) before return
		ASSERT_EQ(body[1]["stmt-type"], "return");
		ASSERT_EQ(body[1]["values"][0]["name"], "arr");
		ASSERT_EQ(body.size(), 2u);
	}

	// functions[1]: testTransfer(int64 i)
	{
		const auto& body = jout["functions"][1]["body"];
		ASSERT_EQ(body[0]["stmt-type"], "var-decl");
		ASSERT_EQ(body[0]["vars"][0]["name"], "inner");
		ASSERT_EQ(body[1]["stmt-type"], "var-decl");
		ASSERT_EQ(body[1]["vars"][0]["name"], "outer");
		// body[2]: the row outer[i] owned is freed before it takes over inner
		ASSERT_EQ(body[2]["stmt-type"], "expr");
		ASSERT_EQ(body[2]["body"]["name"], "free");
		ASSERT_EQ(body[2]["body"]["args"][0]["expr-type"], "arr-index");
		ASSERT_EQ(body[3]["stmt-type"], "arr-assign");
		ASSERT_EQ(body[3].value("ownership-transfer", false), true);
		// body[4]: null-assign for inner (inner = 0)
		ASSERT_EQ(body[4]["stmt-type"], "assign");
		ASSERT_EQ(body[4]["name"], "inner");
		ASSERT_EQ(body[4]["value"]["expr-type"], "lit-int");
		ASSERT_EQ(body[4]["value"]["value"], "0");
		ASSERT_EQ(body[4]["value"]["value-type"]["type-kind"], "pntr");
		// body[5]: testOwnedReturn() call with category: "expiring"
		ASSERT_EQ(body[5]["stmt-type"], "expr");
		ASSERT_EQ(funcName(body[5]["body"]["name"]), "testOwnedReturn");
		ASSERT_EQ(body[5]["body"]["category"], "expiring");
		// body[6..7]: free outer, free(inner) before return
		ASSERT_EQ(funcName(body[6]["body"]["name"]), "__pln_free_arr_Pint32");
		ASSERT_EQ(body[7]["body"]["name"], "free");
		ASSERT_EQ(body[8]["stmt-type"], "return");
	}

	// functions[2]: testFreePtr(int64 i)
	{
		const auto& body = jout["functions"][2]["body"];
		// body[0]: var-decl outer
		ASSERT_EQ(body[0]["stmt-type"], "var-decl");
		// body[1]: user-written free(outer[i]) — arg is arr-index
		ASSERT_EQ(body[1]["stmt-type"], "expr");
		ASSERT_EQ(body[1]["body"]["name"], "free");
		ASSERT_EQ(body[1]["body"]["args"][0]["expr-type"], "arr-index");
		// body[2]: SA-generated free(outer) at return
		ASSERT_EQ(body[2]["stmt-type"], "expr");
		ASSERT_EQ(body[2]["body"]["name"], "free");
		ASSERT_EQ(body[2]["body"]["args"][0]["name"], "outer");
		// body[3]: return
		ASSERT_EQ(body[3]["stmt-type"], "return");
	}
}

TEST(sa, 2d_arr_decl) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/053_2d_arr_decl.pa");
	ASSERT_TRUE(jout.is_object());

	// alloc-shapes: one array of int32 rows, whose length is not in the key
	ASSERT_TRUE(jout.contains("alloc-shapes"));
	ASSERT_EQ(jout["alloc-shapes"].size(), 1u);
	auto& shape = jout["alloc-shapes"][0];
	ASSERT_EQ(shape["shape-kind"], "arr");
	ASSERT_EQ(shape["shape-key"], "arr_Pint32");
	ASSERT_EQ(shape["elem"]["kind"], "row");
	ASSERT_EQ(shape["elem"]["key"], "Pint32");
	ASSERT_FALSE(shape["elem"].contains("count"));
	ASSERT_EQ(shape["elem"]["elem"]["kind"], "prim");

	ASSERT_FALSE(jout["functions"].empty());
	const auto& body = jout["functions"][0]["body"];

	// body[0]: rows var-decl, body[1]: cols var-decl
	// body[2], body[4]: __mat_d0 = rows, __mat_d1 = cols (uint64 temps for both lengths), each
	// followed by its check of a debug build
	ASSERT_EQ(body[3]["stmt-type"], "if");
	ASSERT_EQ(body[3]["then"]["body"][0]["body"]["name"], "__pln_negative_size");
	ASSERT_EQ(body[5]["stmt-type"], "if");
	ASSERT_EQ(body[2]["stmt-type"], "var-decl");
	ASSERT_EQ(body[2]["vars"][0]["name"], "__mat_d0");
	ASSERT_EQ(body[2]["vars"][0]["var-type"]["type-kind"], "prim");
	ASSERT_EQ(body[2]["vars"][0]["var-type"]["type-name"], "uint64");
	ASSERT_EQ(body[2]["vars"][0]["init"]["src"]["name"], "rows");
	ASSERT_EQ(body[4]["vars"][0]["name"], "__mat_d1");
	ASSERT_EQ(body[4]["vars"][0]["init"]["src"]["name"], "cols");

	// body[6]: mat = __pln_alloc_arr_Pint32(__mat_d0, __mat_d1)
	ASSERT_EQ(body[6]["stmt-type"], "var-decl");
	ASSERT_EQ(body[6]["vars"][0]["name"], "mat");
	ASSERT_EQ(body[6]["vars"][0]["var-type"]["type-kind"], "pntr");
	ASSERT_EQ(body[6]["vars"][0]["var-type"]["base-type"]["type-kind"], "pntr");
	ASSERT_EQ(body[6]["vars"][0]["var-type"]["base-type"]["base-type"]["type-name"], "int32");
	auto& init = body[6]["vars"][0]["init"];
	ASSERT_EQ(init["expr-type"], "call");
	ASSERT_EQ(funcName(init["name"]), "__pln_alloc_arr_Pint32");
	ASSERT_EQ(init["func-type"], "palan");
	ASSERT_EQ(init["args"][0]["name"], "__mat_d0");
	ASSERT_EQ(init["args"][1]["name"], "__mat_d1");

	// body.back(): __pln_free_arr_Pint32(mat, __mat_d0, __mat_d1) at scope exit
	auto& last = body.back();
	ASSERT_EQ(last["stmt-type"], "expr");
	ASSERT_EQ(funcName(last["body"]["name"]), "__pln_free_arr_Pint32");
	ASSERT_EQ(last["body"]["func-type"], "palan");
	ASSERT_EQ(last["body"]["args"][0]["name"], "mat");
	ASSERT_EQ(last["body"]["args"][1]["name"], "__mat_d0");
	ASSERT_EQ(last["body"]["args"][2]["name"], "__mat_d1");
}

TEST(sa, embed_arr_decl_const_inner) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/055_embed_arr_decl.pa");
	ASSERT_TRUE(jout.is_object());

	// alloc-shapes must be empty (embedded arrays use stdlib malloc/free)
	ASSERT_TRUE(jout["alloc-shapes"].empty());

	// func f: single body stmt + free at scope end
	ASSERT_FALSE(jout["functions"].empty());
	const auto& body_f = jout["functions"][0]["body"];

	// body[0]: __mat_d0 = rows, the run-time row count
	ASSERT_EQ(body_f[0]["vars"][0]["name"], "__mat_d0");
	ASSERT_EQ(body_f[0]["vars"][0]["init"]["src"]["name"], "rows");

	// body[1]: mat = __pln_malloc(__mat_d0 * 16)
	ASSERT_EQ(body_f[2]["stmt-type"], "var-decl");
	ASSERT_EQ(body_f[2]["vars"][0]["name"], "mat");
	const auto& mat_vtype = body_f[2]["vars"][0]["var-type"];
	ASSERT_EQ(mat_vtype["type-kind"],   "pntr");
	ASSERT_TRUE(mat_vtype.value("embedded", false));
	ASSERT_EQ(mat_vtype["stride"],      16);
	ASSERT_EQ(mat_vtype["base-type"]["arr-size"], 4);
	ASSERT_EQ(mat_vtype["base-type"]["base-type"]["type-name"], "int32");

	const auto& init = body_f[2]["vars"][0]["init"];
	ASSERT_EQ(init["expr-type"], "call");
	ASSERT_EQ(init["name"], "__pln_malloc");
	ASSERT_EQ(init["args"][0]["expr-type"],  "mul");
	ASSERT_EQ(init["args"][0]["left"]["name"], "__mat_d0");
	ASSERT_EQ(init["args"][0]["right"]["value"], "16");

	// free(mat), then the trailing return
	ASSERT_EQ(body_f.back()["stmt-type"], "return");
	const auto& last_f = body_f[body_f.size() - 2];
	ASSERT_EQ(last_f["stmt-type"], "expr");
	ASSERT_EQ(last_f["body"]["name"], "free");
	ASSERT_EQ(last_f["body"]["args"][0]["name"], "mat");
}

TEST(sa, embed_arr_decl_variable_inner) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/055_embed_arr_decl.pa");
	ASSERT_TRUE(jout.is_object());
	ASSERT_GE(jout["functions"].size(), 2u);

	const auto& body_g = jout["functions"][1]["body"];

	// body[0], body[2]: __mat_d0 = rows, __mat_d1 = cols (uint64 hidden vars for both dimensions)
	ASSERT_EQ(body_g[0]["vars"][0]["name"], "__mat_d0");
	ASSERT_EQ(body_g[0]["vars"][0]["init"]["src"]["name"], "rows");
	ASSERT_EQ(body_g[2]["stmt-type"], "var-decl");
	ASSERT_EQ(body_g[2]["vars"][0]["name"], "__mat_d1");
	ASSERT_EQ(body_g[2]["vars"][0]["var-type"]["type-kind"], "prim");
	ASSERT_EQ(body_g[2]["vars"][0]["var-type"]["type-name"], "uint64");
	ASSERT_EQ(body_g[2]["vars"][0]["init"]["src"]["name"], "cols");

	// body[2]: mat = __pln_malloc(__mat_d0 * (__mat_d1 * 4))  — the row size is not in the type
	ASSERT_EQ(body_g[4]["stmt-type"], "var-decl");
	ASSERT_EQ(body_g[4]["vars"][0]["name"], "mat");
	const auto& mat_vtype_g = body_g[4]["vars"][0]["var-type"];
	ASSERT_EQ(mat_vtype_g["type-kind"], "pntr");
	ASSERT_TRUE(mat_vtype_g.value("embedded", false));
	ASSERT_FALSE(mat_vtype_g.contains("stride"));
	ASSERT_FALSE(mat_vtype_g["base-type"].contains("arr-size"));
	ASSERT_EQ(mat_vtype_g["base-type"]["base-type"]["type-name"], "int32");

	const auto& init_g = body_g[4]["vars"][0]["init"];
	ASSERT_EQ(init_g["expr-type"], "call");
	ASSERT_EQ(init_g["name"], "__pln_malloc");
	// args[0]: mul(__mat_d0, mul(__mat_d1, lit-uint(4)))
	ASSERT_EQ(init_g["args"][0]["expr-type"], "mul");
	ASSERT_EQ(init_g["args"][0]["left"]["name"], "__mat_d0");
	ASSERT_EQ(init_g["args"][0]["right"]["expr-type"], "mul");
	ASSERT_EQ(init_g["args"][0]["right"]["left"]["name"], "__mat_d1");
	ASSERT_EQ(init_g["args"][0]["right"]["right"]["value"], "4");

	// free(mat), then the trailing return
	ASSERT_EQ(body_g.back()["stmt-type"], "return");
	const auto& last_g = body_g[body_g.size() - 2];
	ASSERT_EQ(last_g["stmt-type"], "expr");
	ASSERT_EQ(last_g["body"]["name"], "free");
	ASSERT_EQ(last_g["body"]["args"][0]["name"], "mat");
}

TEST(sa, embed_arr_func_param) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/056_embed_arr_func_param.pa");
	ASSERT_TRUE(jout.is_object());
	ASSERT_GE(jout["functions"].size(), 2u);

	// func process: first param is pntr, embedded:true, a 4-element row of 16 bytes
	const auto& params = jout["functions"][0]["parameters"];
	ASSERT_EQ(params[0]["name"], "mat");
	const auto& mat_vt = params[0]["var-type"];
	ASSERT_EQ(mat_vt["type-kind"], "pntr");
	ASSERT_TRUE(mat_vt.value("embedded", false));
	ASSERT_EQ(mat_vt["stride"], 16);
	ASSERT_EQ(mat_vt["base-type"]["arr-size"], 4);
	ASSERT_EQ(mat_vt["base-type"]["base-type"]["type-name"], "int32");

	// return mat[0][0]: nested arr-index, outer elem-size=16, inner elem-size=4
	const auto& ret = jout["functions"][0]["body"][0];
	ASSERT_EQ(ret["stmt-type"], "return");
	const auto& outer_idx = ret["values"][0];
	ASSERT_EQ(outer_idx["expr-type"], "arr-index");
	ASSERT_EQ(outer_idx["elem-size"]["value"], "4");
	ASSERT_EQ(outer_idx["array"]["expr-type"], "arr-index");
	ASSERT_EQ(outer_idx["array"]["elem-size"]["value"], "16");

	// func caller: call to process succeeds (no error)
	const auto& caller_body = jout["functions"][1]["body"];
	bool found_call = false;
	for (const auto& stmt : caller_body) {
		if (stmt["stmt-type"] == "expr" && stmt["body"]["expr-type"] == "call"
				&& funcName(stmt["body"]["name"]) == "process") {
			found_call = true;
			break;
		}
	}
	ASSERT_TRUE(found_call);
}

TEST(sa, embed_arr_var_row_access) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/057_embed_arr_var_row.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& body = jout["functions"][0]["body"];

	// body[0], body[2]: hidden vars __mat_d0 = n, __mat_d1 = cols
	ASSERT_EQ(body[0]["vars"][0]["name"], "__mat_d0");
	ASSERT_EQ(body[2]["stmt-type"], "var-decl");
	ASSERT_EQ(body[2]["vars"][0]["name"], "__mat_d1");
	ASSERT_EQ(body[2]["vars"][0]["init"]["src"]["name"], "cols");

	// body[4]: mat = __pln_malloc(__mat_d0 * (__mat_d1 * 4)), no stride
	ASSERT_EQ(body[4]["vars"][0]["name"], "mat");
	ASSERT_FALSE(body[4]["vars"][0]["var-type"].contains("stride"));

	// body[5]: mat[0][0] is evaluated into a temp before body[6] frees mat;
	// the inner arr-index (row access) has mul elem-size
	ASSERT_EQ(body[6]["body"]["name"], "free");
	ASSERT_EQ(body[7]["values"][0]["name"], body[5]["vars"][0]["name"]);
	const auto& ret_val = body[5]["vars"][0]["init"];
	ASSERT_EQ(ret_val["expr-type"], "arr-index");
	const auto& row_idx = ret_val["array"];
	ASSERT_EQ(row_idx["expr-type"], "arr-index");
	ASSERT_EQ(row_idx["elem-size"]["expr-type"],    "mul");
	ASSERT_EQ(row_idx["elem-size"]["left"]["name"], "__mat_d1");
	ASSERT_EQ(row_idx["elem-size"]["right"]["value"], "4");
}

TEST(sa, array_in_if_block)
{
	// Array declared inside an if block: free stmt appended to block body
	// Covers: sa_block frees loop (PlnSaStmt.cpp line 99)
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/096_array_in_block.pa");
	ASSERT_TRUE(jout.is_object());

	// Find test() function
	json* func = nullptr;
	for (auto& f : jout["functions"])
		if (funcName(f["name"]) == "test") { func = &f; break; }
	ASSERT_NE(func, nullptr);

	// The if stmt is the second statement in the function body
	const auto& stmts = (*func)["body"];
	json* ifStmt = nullptr;
	for (auto& s : stmts)
		if (s["stmt-type"] == "if") { ifStmt = &const_cast<json&>(s); break; }
	ASSERT_NE(ifStmt, nullptr);

	// The then block body should end with a free() call for arr
	const auto& thenBody = (*ifStmt)["then"]["body"];
	ASSERT_GE(thenBody.size(), 2u);
	const auto& last = thenBody[thenBody.size() - 1];
	ASSERT_EQ(last["stmt-type"], "expr");
	ASSERT_EQ(last["body"]["name"], "free");
}

TEST(sa, arr_borrow)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/201_arr_borrow.pa");
	ASSERT_TRUE(jout.is_object());

	// '@!a' borrows the array pointer itself: an id, not an address of its slot.
	const json* call = nullptr;
	for (auto& st : jout["statements"])
		if (st["stmt-type"] == "expr") call = &st["body"];
	ASSERT_NE(call, nullptr);
	const auto& arg = (*call)["args"][0];
	ASSERT_EQ(arg["expr-type"], "id");
	ASSERT_EQ(arg["value-type"]["mutable"], true);
	ASSERT_EQ(arg["value-type"]["arr-size"], 2);
	ASSERT_EQ(arg["value-type"]["base-type"]["mutable"], true);
	ASSERT_EQ(arg["value-type"]["base-type"]["arr-size"], 3);

	const auto& pt = jout["functions"][0]["parameters"][0]["var-type"];
	ASSERT_EQ(pt["type-kind"], "pntr");
	ASSERT_EQ(pt["arr-size"], 2);
	ASSERT_EQ(pt["base-type"]["arr-size"], 3);
	ASSERT_EQ(pt["base-type"]["base-type"]["type-name"], "int32");
}

TEST(sa, arr_lit_assign)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/242_arr_lit_assign.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	// Values first, then the target's address as a mutable borrow, then the stores through it.
	auto check = [&](int i, const string& targetExpr, const string& store, size_t count) {
		const auto& body = stmts[i]["body"];
		ASSERT_EQ(stmts[i]["stmt-type"], "block");
		ASSERT_EQ(body.size(), 2 * count + 1);
		const auto& target = body[count]["vars"][0];
		ASSERT_EQ(target["init"]["expr-type"], targetExpr);
		ASSERT_EQ(target["var-type"]["mutable"], true);
		for (size_t k = count + 1; k < body.size(); k++)
			ASSERT_EQ(body[k]["stmt-type"], store);
	};
	check(3, "id", "arr-assign", 2);
	ASSERT_EQ(stmts[3]["body"][3]["value"]["name"], stmts[3]["body"][0]["vars"][0]["name"]);
	ASSERT_EQ(stmts[3]["body"][3]["target"]["array"]["name"], stmts[3]["body"][2]["vars"][0]["name"]);
	// The owned array field's pointer is read; the embedded one's address is computed.
	check(5, "field-access", "arr-assign", 4);
	ASSERT_EQ(stmts[5]["body"][4]["vars"][0]["init"]["addr-only"], false);
	check(6, "field-access", "field-assign", 4);
	ASSERT_EQ(stmts[6]["body"][4]["vars"][0]["init"]["addr-only"], true);
	check(12, "arr-index", "arr-assign", 2);

	const auto& q = stmts[13]["vars"][0]["init"];
	ASSERT_EQ(q["expr-type"], "field-access");
	ASSERT_EQ(q["value-type"]["embedded"], true);
	ASSERT_EQ(q["value-type"]["mutable"], true);
}

TEST(sa, varr_basic)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/293_varr_basic.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts.size(), 9);

	// The variable points at a header allocated even without a capacity.
	const auto& a = stmts[0]["vars"][0];
	ASSERT_EQ(a["var-type"]["type-kind"], "pntr");
	ASSERT_EQ(a["var-type"]["base-type"]["type-kind"], "varr");
	ASSERT_EQ(a["var-type"]["base-type"]["base-type"]["type-name"], "int32");
	ASSERT_EQ(funcName(a["init"]["name"]), "__pln_alloc_varr_int32");
	ASSERT_EQ(a["init"]["args"][0]["value"], "0");
	ASSERT_EQ(a["init"]["args"][1]["value"], "0");
	// [+4]int64: length 0, capacity 4
	const auto& bArgs = stmts[1]["vars"][0]["init"]["args"];
	ASSERT_EQ(bArgs[0]["value"], "0");
	ASSERT_EQ(bArgs[1]["value"], "4");
	// [+3]uint8 = [1, 2]: the size is the length
	const auto& cArgs = stmts[2]["vars"][0]["init"]["args"];
	ASSERT_EQ(cArgs[0]["value"], "3");
	ASSERT_EQ(cArgs[1]["value"], "3");
	ASSERT_EQ(stmts[3]["stmt-type"], "arr-assign");

	// v[i] indexes the header's data pointer.
	const auto& store = stmts[5]["target"]["array"];
	ASSERT_EQ(store["expr-type"], "field-access");
	ASSERT_EQ(store["offset"], 16);
	ASSERT_EQ(store["ptr-expr"]["name"], "b");
	ASSERT_EQ(store["value-type"]["base-type"]["type-name"], "int64");
	ASSERT_EQ(stmts[6]["vars"][0]["init"]["array"]["expr-type"], "field-access");

	// A C function receives the data pointer.
	const auto& cArg = stmts[7]["body"]["args"][0];
	ASSERT_EQ(cArg["expr-type"], "field-access");
	ASSERT_EQ(cArg["ptr-expr"]["name"], "c");

	// $[+2]int32: the header is in the frame and only its data is released.
	const auto& block = stmts[8]["body"];
	ASSERT_EQ(block[0]["vars"][0]["init"]["expr-type"], "frame-alloc");
	ASSERT_EQ(block[0]["vars"][0]["init"]["size"], 24);
	ASSERT_EQ(funcName(block[1]["body"]["name"]), "__pln_init_varr_int32");
	ASSERT_EQ(funcName(block.back()["body"]["name"]), "__pln_fini_varr_int32");

	const auto& shapes = jout["alloc-shapes"];
	ASSERT_EQ(shapes.size(), 3);
	ASSERT_EQ(shapes[0]["shape-kind"], "varr");
	ASSERT_EQ(shapes[0]["elem"], json({{"kind","prim"},{"key","int32"},{"type","int32"},{"size",4}}));
}

TEST(sa, varr_builtins)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/294_varr_builtins.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts.size(), 8);

	// The header pointer is passed as it is, with the borrow's permission.
	auto expectCall = [&](const json& call, const string& name, bool isMutable) {
		ASSERT_EQ(funcName(call["name"]), name);
		const auto& self = call["args"][0];
		ASSERT_EQ(self["expr-type"], "id");
		ASSERT_EQ(self["name"], "v");
		ASSERT_EQ(self["value-type"]["mutable"], isMutable);
	};
	// push(@!v, 1), v.push(2), 3 -> v.push()
	for (int i = 1; i <= 3; i++) {
		expectCall(stmts[i]["body"], "__pln_push_varr_int32", true);
		ASSERT_EQ(stmts[i]["body"]["args"][1]["value"], to_string(i));
	}
	// len(@v), v -> len(): the header's len, read in place.
	for (int i = 4; i <= 5; i++) {
		const auto& len = stmts[i]["vars"][0]["init"];
		ASSERT_EQ(len["expr-type"], "field-access");
		ASSERT_EQ(len["offset"], 0);
		ASSERT_EQ(len["ptr-expr"]["name"], "v");
		ASSERT_EQ(len["ptr-expr"]["value-type"]["mutable"], false);
		ASSERT_EQ(len["value-type"]["type-name"], "int64");
	}
	const auto& pop = stmts[6]["vars"][0]["init"];
	expectCall(pop, "__pln_pop_varr_int32", true);
	ASSERT_EQ(pop["value-type"]["type-name"], "int32");
	expectCall(stmts[7]["body"], "__pln_resize_varr_int32", true);
	ASSERT_EQ(stmts[7]["body"]["args"][1]["name"], "n");
}

// The length depends on the type, or on the root variable for a length kept at
// run time, so an index with a call is not evaluated, like sizeof's operand.
TEST(sa, len_call_index)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/350_len_call_index.pa");
	ASSERT_TRUE(jout.is_object());
	map<string, json> init;
	for (auto& s : jout["statements"])
		if (s["stmt-type"] == "var-decl") init[s["vars"][0]["name"]] = s["vars"][0]["init"];
	for (auto [name, value] : {pair{"n", "3"}, {"m", "3"}, {"r", "2"}}) {
		ASSERT_EQ(init[name]["expr-type"], "lit-int") << name;
		ASSERT_EQ(init[name]["value"], value) << name;
	}
	ASSERT_EQ(init["q"]["expr-type"], "convert");
	ASSERT_EQ(init["q"]["src"]["name"], "__b_d1");
}

// A constant-size array's len is folded to its size, like sizeof.
TEST(sa, arr_len)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/295_arr_len.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts.size(), 14);
	// [len(@a) * 2]int32 b
	ASSERT_EQ(stmts[4]["vars"][0]["var-type"]["arr-size"], 6);
	auto expectLen = [&](const json& stmt, const string& value) {
		const auto& init = stmt["vars"][0]["init"];
		ASSERT_EQ(init["expr-type"], "lit-int");
		ASSERT_EQ(init["value"], value);
	};
	expectLen(stmts[7], "3");	// len(@c)
	expectLen(stmts[8], "4");	// len(@c[0])
	expectLen(stmts[9], "5");	// len(@p.arr)
	expectLen(stmts[10], "3");	// a -> len()
	expectLen(stmts[11], "3");	// len(@!a)
	expectLen(stmts[12], "3");	// N
	// Without '@', a user's len taking the array is called.
	const auto& call = stmts[13]["vars"][0]["init"];
	ASSERT_EQ(call["expr-type"], "call");
	ASSERT_EQ(funcName(call["name"]), "len");

	const auto& ret = jout["functions"][0]["body"][0];
	ASSERT_EQ(ret["value"]["expr-type"], "lit-int");
	ASSERT_EQ(ret["value"]["value"], "3");
}

// A borrowed array is given to len by name, as to a function; a row of it
// is not a borrow of its own and keeps '@'.
TEST(sa, len_borrow)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/326_len_borrow.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	auto expectLen = [&](const json& stmt, const string& value) {
		const auto& init = stmt["vars"][0]["init"];
		ASSERT_EQ(init["expr-type"], "lit-int");
		ASSERT_EQ(init["value"], value);
	};
	expectLen(stmts[5], "2");	// len(r)
	expectLen(stmts[6], "2");	// len(@g[1])
	expectLen(stmts[8], "3");	// N = len(g)
	// len(h) of a '@[+]T' reads the length at run time.
	ASSERT_EQ(stmts[7]["vars"][0]["init"]["expr-type"], "field-access");
}

// A borrow passes the header itself; a `[]T` parameter gets the data.
TEST(sa, varr_borrow)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/296_varr_borrow.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& funcs = jout["functions"];
	ASSERT_EQ(funcs.size(), 4);
	ASSERT_EQ(funcs[0]["name"], "_296_varr_borrow..add.WVint32.int32");
	ASSERT_EQ(funcs[1]["name"], "_296_varr_borrow..total.RVint32");
	const auto& param = funcs[1]["parameters"][0]["var-type"];
	ASSERT_EQ(param["base-type"]["type-kind"], "varr");
	ASSERT_EQ(param["mutable"], false);

	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts.size(), 7);
	const auto& total = stmts[3]["vars"][0]["init"]["args"][0];
	ASSERT_EQ(total["name"], "v");
	ASSERT_EQ(total["value-type"]["mutable"], true);
	const auto& data = stmts[4]["vars"][0]["init"]["args"][0];
	ASSERT_EQ(data["expr-type"], "field-access");
	ASSERT_EQ(data["offset"], 16);
	ASSERT_EQ(data["ptr-expr"]["name"], "v");
	const auto& p = stmts[5]["vars"][0];
	ASSERT_EQ(p["var-type"]["base-type"]["type-kind"], "varr");
	ASSERT_EQ(p["var-type"]["mutable"], true);
	ASSERT_EQ(stmts[6]["stmt-type"], "assign");
	ASSERT_EQ(stmts[6]["value"]["name"], "w");
}

// A copy into a `[+]T` passes the source's elements and count; a transfer
// frees the target's header and takes the source's.
TEST(sa, varr_copy)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/297_varr_copy.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts.size(), 16);

	const auto& fromVarr = stmts[5]["body"];
	ASSERT_EQ(funcName(fromVarr["name"]), "__pln_copy_varr_int32");
	ASSERT_EQ(fromVarr["args"][0]["name"], "b");
	ASSERT_EQ(fromVarr["args"][1]["offset"], 16);
	ASSERT_EQ(fromVarr["args"][1]["ptr-expr"]["name"], "a");
	ASSERT_EQ(fromVarr["args"][2]["offset"], 0);
	const auto& fromSized = stmts[7]["body"];
	ASSERT_EQ(fromSized["args"][1]["name"], "c");
	ASSERT_EQ(fromSized["args"][2]["value"], "4");
	ASSERT_EQ(funcName(stmts[9]["body"]["name"]), "__pln_copy_varr_int32");
	ASSERT_EQ(stmts[11]["body"]["args"][1]["ptr-expr"]["name"], "p");

	// A literal is laid out in the frame first, then copied.
	const auto& lit = stmts[12]["body"];
	ASSERT_EQ(lit[0]["vars"][0]["init"]["expr-type"], "frame-alloc");
	ASSERT_EQ(lit.back()["body"]["args"][0]["name"], "d");
	ASSERT_EQ(lit.back()["body"]["args"][2]["value"], "2");

	ASSERT_EQ(funcName(stmts[13]["body"]["name"]), "__pln_free_varr_int32");
	ASSERT_EQ(stmts[13]["body"]["args"][0]["name"], "b");
	ASSERT_EQ(stmts[14]["name"], "b");
	ASSERT_EQ(stmts[14]["value"]["name"], "a");
	ASSERT_EQ(stmts[15]["name"], "a");
	ASSERT_EQ(stmts[15]["value"]["value"], "0");
}

// A `[+]T` is returned and received like a struct, and an owned parameter
// takes a copy of its argument, or a returned header as it is.
TEST(sa, varr_return_param)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/298_varr_return_param.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& funcs = jout["functions"];
	ASSERT_EQ(funcs.size(), 5);

	// The named return is allocated at entry and not freed.
	const auto& mk = funcs[0]["body"];
	ASSERT_EQ(mk.size(), 2);
	ASSERT_EQ(funcName(mk[0]["vars"][0]["init"]["name"]), "__pln_alloc_varr_int32");
	// A borrow is returned as a copy; a borrowed struct is copied into the return slot.
	const auto& dup = funcs[1]["body"];
	ASSERT_EQ(dup.size(), 3);
	ASSERT_EQ(dup[2]["values"][0]["name"], dup[0]["vars"][0]["name"]);
	ASSERT_EQ(funcName(dup[1]["body"]["name"]), "__pln_copy_varr_int32");
	const auto& cp = funcs[3];
	ASSERT_EQ(cp["ret-slot"]["name"], "__ret");
	ASSERT_EQ(cp["body"].size(), 3);
	ASSERT_EQ(cp["body"][1]["body"]["name"], "memcpy");
	ASSERT_EQ(cp["body"][1]["body"]["args"][0]["name"], "__ret");
	ASSERT_FALSE(cp["body"][2].contains("values"));
	// The owned parameter is returned as it is.
	ASSERT_EQ(funcs[2]["name"], "_298_varr_return_param..own.PVint32");
	const auto& own = funcs[2]["body"];
	ASSERT_EQ(own.size(), 1);
	ASSERT_EQ(own[0]["values"][0]["name"], "x");

	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts.size(), 4);
	ASSERT_EQ(funcName(stmts[0]["vars"][0]["init"]["name"]), "mk");
	const auto& received = stmts[1]["body"];
	ASSERT_EQ(received.size(), 3);
	ASSERT_EQ(funcName(received[1]["body"]["name"]), "__pln_copy_varr_int32");
	ASSERT_EQ(funcName(received[2]["body"]["name"]), "__pln_free_varr_int32");
	const auto& copied = stmts[2]["vars"][0]["init"]["args"][0];
	ASSERT_EQ(copied["expr-type"], "temp");
	ASSERT_EQ(copied["release"].size(), 0);
	ASSERT_EQ(funcName(copied["body"][1]["body"]["name"]), "__pln_copy_varr_int32");
	ASSERT_EQ(funcName(stmts[3]["vars"][0]["init"]["args"][0]["name"]), "mk");
}

// `[+]P` holds owned element slots and `[+]$P` the elements in place; each has
// its own generated helpers.
TEST(sa, varr_struct)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/299_varr_struct.pa"));
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts.size(), 12);

	const auto& v = stmts[0]["vars"][0];
	ASSERT_EQ(v["var-type"]["base-type"]["base-type"]["type-kind"], "pntr");
	ASSERT_EQ(v["var-type"]["base-type"]["base-type"]["base-type"]["type-name"], "P");
	ASSERT_EQ(funcName(v["init"]["name"]), "__pln_alloc_varr_P8__pln_B8");
	const auto& e = stmts[1]["vars"][0];
	ASSERT_EQ(e["var-type"]["base-type"]["embedded"], true);
	ASSERT_EQ(e["var-type"]["base-type"]["base-type"]["type-kind"], "struct");
	ASSERT_EQ(funcName(e["init"]["name"]), "__pln_alloc_evarr_8__pln_B8");
	// The literal is stored field by field into the elements init built.
	ASSERT_EQ(stmts[2]["stmt-type"], "field-assign");
	ASSERT_EQ(stmts[3]["stmt-type"], "field-assign");

	// v[0] is a slot; e[1] is in the data, `stride` bytes apart.
	const auto& slot = stmts[4]["ptr-expr"];
	ASSERT_EQ(slot["elem-size"]["value"], "8");
	ASSERT_EQ(slot["addr-only"], false);
	const auto& inPlace = stmts[5]["vars"][0]["init"]["ptr-expr"];
	ASSERT_EQ(inPlace["addr-only"], true);
	ASSERT_EQ(inPlace["array"]["value-type"]["embedded"], true);
	ASSERT_EQ(inPlace["array"]["value-type"]["stride"], 8);

	ASSERT_EQ(funcName(stmts[6]["body"]["name"]), "__pln_push_varr_P8__pln_B8");
	ASSERT_EQ(stmts[6]["body"]["args"][1]["expr-type"], "temp");
	// A popped struct is returned into the receiving variable's storage.
	ASSERT_EQ(stmts[7]["vars"][0]["init"]["name"], "__pln_malloc");
	const auto& popped = stmts[8]["body"];
	ASSERT_EQ(funcName(popped["name"]), "__pln_pop_varr_P8__pln_B8");
	ASSERT_EQ(popped["ret-dest"]["name"], "p");
	ASSERT_EQ(funcName(stmts[9]["body"]["name"]), "__pln_push_evarr_8__pln_B8");
	ASSERT_EQ(funcName(stmts[10]["body"]["name"]), "__pln_resize_varr_P8__pln_B8");

	// A struct element's own shape comes before its varr shape; P owns
	// nothing, so it is laid out as bytes and its arrays are named after them.
	const auto& shapes = jout["alloc-shapes"];
	ASSERT_EQ(shapes.size(), 5);
	ASSERT_EQ(shapes[0]["shape-name"], "__pln_B8");
	ASSERT_EQ(shapes[1]["shape-key"], "varr_P8__pln_B8");
	ASSERT_EQ(shapes[1]["elem"]["kind"], "struct");
	ASSERT_EQ(shapes[1]["elem"]["size"], 8);
	ASSERT_EQ(shapes[2]["shape-key"], "evarr_8__pln_B8");
	ASSERT_EQ(shapes[2]["elem"]["kind"], "embed-struct");
	ASSERT_EQ(shapes[3]["shape-name"], "Q");
	ASSERT_EQ(shapes[4]["shape-key"], "varr_P1Q");
}

// Owned slots and elements in place are different arrays even of one element
// type, so functions taking each are distinct overloads.
TEST(sa, varr_embedded_symbol)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/300_varr_embedded_symbol.pa"));
	ASSERT_TRUE(jout.is_object());
	const auto& funcs = jout["functions"];
	ASSERT_EQ(funcs[0]["name"], "_300_varr_embedded_symbol..n.RVP1P");
	ASSERT_EQ(funcs[1]["name"], "_300_varr_embedded_symbol..n.RVC1P");
}

// An element `[+]T` is owned by the array holding it, whose generated
// helpers build, free and copy it through the element's own.
TEST(sa, nested_varr)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/302_nested_varr.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	const auto& m = stmts[0]["vars"][0];
	const auto& elem = m["var-type"]["base-type"]["base-type"];
	ASSERT_EQ(elem["base-type"]["type-kind"], "varr");
	ASSERT_EQ(elem.contains("mutable"), false);
	ASSERT_EQ(funcName(m["init"]["name"]), "__pln_alloc_varr_PVint32");
	ASSERT_EQ(funcName(stmts[1]["body"]["name"]), "__pln_resize_varr_PVint32");

	// m[0].push(): the element is borrowed in place.
	const auto& push = stmts[2]["body"];
	ASSERT_EQ(funcName(push["name"]), "__pln_push_varr_int32");
	ASSERT_EQ(push["args"][0]["expr-type"], "arr-index");
	ASSERT_EQ(push["args"][0]["value-type"]["mutable"], true);
	ASSERT_EQ(stmts[3]["vars"][0]["init"]["ptr-expr"]["expr-type"], "arr-index");
	const auto& popped = stmts[4]["vars"][0]["init"];
	ASSERT_EQ(funcName(popped["name"]), "__pln_pop_varr_PVint32");
	ASSERT_EQ(popped["category"], "expiring");

	ASSERT_EQ(funcName(stmts[5]["vars"][0]["init"]["name"]), "__pln_alloc_arr_PVint32");
	ASSERT_EQ(funcName(stmts[6]["body"]["name"]), "__pln_copy_varr_PVint32");

	const auto& shapes = jout["alloc-shapes"];
	ASSERT_EQ(shapes.size(), 3);
	ASSERT_EQ(shapes[0]["shape-key"], "varr_int32");
	ASSERT_EQ(shapes[1]["shape-key"], "varr_PVint32");
	ASSERT_EQ(shapes[1]["elem"]["kind"], "varr");
	ASSERT_EQ(shapes[1]["elem"]["shape-key"], "varr_int32");
	ASSERT_EQ(shapes[1]["elem"]["size"], 8);
	ASSERT_EQ(shapes[2]["shape-key"], "arr_PVint32");
	ASSERT_EQ(shapes[2]["elem"]["kind"], "varr");
}

// Each `[+]` row of a literal is resized to its own count before its
// elements are stored.
TEST(sa, nested_varr_lit)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/303_nested_varr_lit.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts[0]["vars"][0]["init"]["args"][0]["value"], "3");
	auto resizeTo = [&](size_t i) {
		EXPECT_EQ(funcName(stmts[i]["body"]["name"]), "__pln_resize_varr_int32");
		return stmts[i]["body"]["args"][1]["value"].get<string>();
	};
	ASSERT_EQ(resizeTo(1), "1");
	ASSERT_EQ(stmts[2]["stmt-type"], "arr-assign");
	ASSERT_EQ(resizeTo(3), "0");
	ASSERT_EQ(resizeTo(4), "2");
	ASSERT_EQ(stmts[6]["stmt-type"], "arr-assign");

	// `[[4]] -> m` goes through a frame `[1][+]int32`; `[] -> m[0]` through
	// an empty frame `[+]int32`.
	const auto& lit = stmts[7]["body"];
	ASSERT_EQ(lit[0]["vars"][0]["init"]["expr-type"], "frame-alloc");
	ASSERT_EQ(funcName(lit[1]["body"]["name"]), "__pln_init_arr_PVint32");
	ASSERT_EQ(funcName(lit[4]["body"]["name"]), "__pln_copy_varr_PVint32");
	ASSERT_EQ(funcName(lit[5]["body"]["name"]), "__pln_fini_arr_PVint32");
	const auto& empty = stmts[8]["body"];
	ASSERT_EQ(empty[0]["vars"][0]["init"]["expr-type"], "frame-alloc");
	ASSERT_EQ(empty[0]["vars"][0]["init"]["size"], 24);
	ASSERT_EQ(funcName(empty[2]["body"]["name"]), "__pln_copy_varr_int32");

	// [2][+]int32: rows of a fixed level are resized too.
	ASSERT_EQ(resizeTo(10), "1");
	ASSERT_EQ(resizeTo(12), "0");
}

// A row of `[+][n]T` is a block of its own, which pop hands to the caller;
// `[+]$[n]T` lays the rows out in the data. push copies from a borrowed row.
TEST(sa, varr_row)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/304_varr_row.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	const auto& elem = stmts[0]["vars"][0]["var-type"]["base-type"]["base-type"];
	ASSERT_EQ(elem["arr-size"], 4);
	ASSERT_EQ(elem.contains("mutable"), false);
	const auto& push = stmts[2]["body"];
	ASSERT_EQ(funcName(push["name"]), "__pln_push_varr_Pint32");
	// The row is copied into a block that push takes over.
	ASSERT_EQ(push["args"][1]["expr-type"], "temp");
	ASSERT_EQ(push["args"][1]["value"]["value-type"], elem);
	ASSERT_EQ(push["args"][2]["value"], "4");

	// `[4]int32 r = v.pop();` takes the popped row over; a discarded one is freed.
	const auto& r = stmts[3]["vars"][0];
	ASSERT_EQ(funcName(r["init"]["name"]), "__pln_pop_varr_Pint32");
	ASSERT_EQ(r["init"]["category"], "expiring");
	ASSERT_EQ(r["var-type"], elem);
	ASSERT_EQ(stmts[4]["body"][1]["body"]["name"], "free");

	const auto& rows = stmts[6]["target"]["array"]["array"]["value-type"];
	ASSERT_EQ(rows["embedded"], true);
	ASSERT_EQ(rows["stride"], 16);

	const auto& shapes = jout["alloc-shapes"];
	ASSERT_EQ(shapes.size(), 4);
	ASSERT_EQ(shapes[0]["shape-key"], "varr_Pint32");
	ASSERT_EQ(shapes[0]["elem"]["kind"], "row");
	ASSERT_EQ(shapes[0]["elem"]["elem"]["kind"], "prim");
	ASSERT_EQ(shapes[1]["shape-key"], "evarr_Pint32");
	ASSERT_EQ(shapes[1]["elem"]["kind"], "embed-row");
	ASSERT_EQ(shapes[1]["elem"]["depth"], 1);
	ASSERT_EQ(shapes[1]["elem"]["key"], "CPint32");
	ASSERT_EQ(shapes[3]["elem"]["kind"], "embed-row");
	ASSERT_EQ(shapes[3]["elem"]["elem"]["kind"], "struct");
}

TEST(sa, varr_row_lit)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/305_varr_row_lit.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts[0]["vars"][0]["init"]["args"][0]["value"], "2");
	for (int i = 1; i <= 4; i++) ASSERT_EQ(stmts[i]["stmt-type"], "arr-assign");

	const auto& fixed = stmts[5]["body"];
	ASSERT_EQ(fixed[0]["vars"][0]["var-type"]["arr-size"], 1);
	ASSERT_EQ(funcName(fixed[4]["body"]["name"]), "__pln_copy_varr_Pint32");

	// The temp of rows holding a `[+]U` is a frame `[1][2][+]U` too.
	const auto& rows = stmts[10]["body"];
	ASSERT_EQ(rows[0]["vars"][0]["init"]["expr-type"], "frame-alloc");
	ASSERT_EQ(funcName(rows[1]["body"]["name"]), "__pln_init_arr_PPVint32");
	ASSERT_EQ(rows[1]["body"]["args"][2]["value"], "2");
	ASSERT_EQ(funcName(rows[7]["body"]["name"]), "__pln_copy_varr_PPVint32");
}

// Rows are named without their length, so `[m][k]T` and `[+][n]T` share
// the row helpers, and `[+][n]T` shares its own with any other length.
TEST(sa, shared_row_helpers)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/306_shared_row_helpers.pa"));
	ASSERT_TRUE(jout.is_object());
	map<string, json> elems;
	for (auto& s : jout["alloc-shapes"])
		if (s.contains("shape-key")) elems[s["shape-key"]] = s["elem"];
	ASSERT_EQ(elems.at("arr_Pint32")["key"], "Pint32");
	ASSERT_FALSE(elems.at("arr_Pint32").contains("count"));
	ASSERT_EQ(elems.at("varr_Pint32")["key"], "Pint32");
	ASSERT_FALSE(elems.at("varr_Pint32").contains("count"));
	ASSERT_EQ(elems.at("arr_PP8__pln_B8")["key"], "PP8__pln_B8");
	ASSERT_EQ(elems.at("varr_PP8__pln_B8")["key"], "PP8__pln_B8");
}

TEST(sa, nested_arr_depth)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/307_nested_arr_depth.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	auto alloc = [&](int i) -> const json& { return stmts[i]["vars"][0]["init"]; };
	auto lens = [&](const json& call, size_t from) {
		string s;
		for (size_t i = from; i < call["args"].size(); i++) s += call["args"][i]["value"].get<string>() + " ";
		return s;
	};
	ASSERT_EQ(funcName(alloc(0)["name"]), "__pln_alloc_arr_PPint32");
	ASSERT_EQ(lens(alloc(0), 0), "2 3 4 ");
	ASSERT_EQ(funcName(alloc(1)["name"]), "__pln_alloc_varr_PPint32");
	ASSERT_EQ(lens(alloc(1), 2), "3 4 ");
	ASSERT_EQ(funcName(alloc(2)["name"]), "__pln_alloc_arr_PPVint32");
	ASSERT_EQ(lens(alloc(2), 0), "2 3 ");
	ASSERT_EQ(funcName(alloc(3)["name"]), "__pln_alloc_varr_PPVPint32");
	ASSERT_EQ(lens(alloc(3), 2), "3 4 ");
	ASSERT_EQ(funcName(alloc(4)["name"]), "__pln_alloc_evarr_EPPint32");
	ASSERT_EQ(lens(alloc(4), 2), "3 4 ");
	// Rows laid out in place are one run of their elements' slots.
	ASSERT_EQ(lens(alloc(5), 0), "6 ");
	const auto& copy = stmts[7]["body"];
	ASSERT_EQ(funcName(copy["name"]), "__pln_copy_arr_PPint32");
	ASSERT_EQ(lens(copy, 2), "2 3 4 ");

	map<string, json> elems;
	for (auto& s : jout["alloc-shapes"])
		if (s.contains("shape-key")) elems[s["shape-key"]] = s["elem"];
	ASSERT_EQ(elems.at("varr_PPVPint32")["elem"]["key"], "PVPint32");
	const auto& embed = elems.at("evarr_EPPint32");
	ASSERT_EQ(embed["kind"], "embed-row");
	ASSERT_EQ(embed["depth"], 2);
	ASSERT_EQ(embed["elem"]["kind"], "prim");
}

// `[+][3]T` and `[+][4]T` share their functions and pass their own lengths.
TEST(sa, varr_row_len_args)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/308_varr_row_len_args.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	const auto& x = stmts[4]["body"];
	const auto& z = stmts[5]["body"];
	ASSERT_EQ(x["name"], z["name"]);
	ASSERT_EQ(funcName(x["name"]), "__pln_push_varr_Pint32");
	ASSERT_EQ(x["args"][2]["value"], "3");
	ASSERT_EQ(z["args"][2]["value"], "4");
	// A row given away with '>>' is taken without a copy.
	ASSERT_EQ(z["args"][1]["category"], "expiring");
	ASSERT_EQ(z["args"][1]["reset"][0]["name"], "q");
}

// A row of pointer slots keeps the slots' type, and its helpers lay them out as int64.
TEST(sa, varr_ptr_slot_rows)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/352_varr_ptr_slot_rows.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	const auto& row = stmts[0]["vars"][0]["var-type"]["base-type"]["base-type"];
	ASSERT_EQ(row["arr-size"], 2);
	ASSERT_EQ(row["base-type"]["type-kind"], "pntr");
	ASSERT_EQ(row["base-type"]["mutable"], false);
	ASSERT_EQ(row["base-type"]["base-type"]["type-kind"], "struct");
	const auto& shapes = jout["alloc-shapes"];
	ASSERT_EQ(shapes.size(), 1);
	ASSERT_EQ(shapes[0]["shape-key"], "varr_Pint64");
	ASSERT_EQ(shapes[0]["elem"]["elem"]["kind"], "prim");
	ASSERT_EQ(funcName(stmts[2]["body"]["name"]), "__pln_push_varr_Pint64");
}

// An owned `[n]T` parameter takes a copy, or a call's result as it is; a
// returned borrow is copied, and the caller takes the result over.
TEST(sa, arr_return_param)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/309_arr_return_param.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& funcs = jout["functions"];
	ASSERT_EQ(funcs.size(), 7);
	const string m = "_309_arr_return_param..";

	// The named return is allocated at entry and not freed.
	ASSERT_EQ(funcs[0]["body"].size(), 2);
	ASSERT_EQ(funcName(funcs[0]["body"][0]["vars"][0]["init"]["name"]), "__pln_malloc");
	// A borrow is returned as a copy.
	const auto& dup = funcs[1]["body"];
	ASSERT_EQ(dup.size(), 3);
	ASSERT_EQ(funcName(dup[1]["body"]["name"]), "memcpy");
	ASSERT_EQ(dup[2]["values"][0]["name"], dup[0]["vars"][0]["name"]);
	// The owned parameter is returned as it is, or freed by the callee.
	ASSERT_EQ(funcs[2]["name"], m + "own.A3_Pint32");
	ASSERT_EQ(funcs[2]["body"].size(), 1);
	ASSERT_EQ(funcs[2]["body"][0]["values"][0]["name"], "x");
	ASSERT_EQ(funcs[3]["name"], m + "f.A3_Pint32");
	ASSERT_EQ(funcName(funcs[3]["body"][0]["body"]["name"]), "free");
	ASSERT_EQ(funcs[4]["name"], m + "f.A3_Rint32");
	ASSERT_EQ(funcs[4]["body"].size(), 0);
	ASSERT_EQ(funcs[5]["name"], m + "grid.ES12_A2_PA3_Pint32.A2_PPVint32");

	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts.size(), 5);
	ASSERT_EQ(funcName(stmts[0]["vars"][0]["init"]["name"]), "mk");
	ASSERT_EQ(funcName(stmts[1]["body"][1]["body"]["name"]), "memcpy");
	ASSERT_EQ(funcName(stmts[1]["body"][2]["body"]["name"]), "free");
	const auto& copied = stmts[2]["vars"][0]["init"]["args"][0];
	ASSERT_EQ(copied["expr-type"], "temp");
	ASSERT_EQ(copied["release"].size(), 0);
	ASSERT_EQ(funcName(copied["body"][1]["body"]["name"]), "memcpy");
	ASSERT_EQ(funcName(stmts[3]["vars"][0]["init"]["args"][0]["name"]), "mk");
}

TEST(sa, lit_arg_struct_arr)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/253_lit_arg_struct_arr.pa"));
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts.size(), 2);
	const auto& embedded = stmts[0]["vars"][0]["init"]["args"][0]["value"]["value-type"];
	ASSERT_EQ(embedded["embedded"], true);
	ASSERT_EQ(embedded["base-type"]["type-name"], "P");
	const auto& slots = stmts[1]["vars"][0]["init"]["args"][0]["value"]["value-type"];
	ASSERT_FALSE(slots.contains("embedded"));
	ASSERT_EQ(slots["base-type"]["base-type"]["type-name"], "P");
}

TEST(sa, owned_rows_ro_slots)
{
	// Owned rows and structs may be lent as read-only '@T' slots; a C 'T**'
	// that does not write takes them through 'as'.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/266_owned_rows_ro_slots.pa");
	ASSERT_TRUE(jout.is_object());
}

TEST(sa, runtime_size_aggregates)
{
	cleanTestEnv();
	genModuleAst("../test/testdata/sa/lib_sa_func_ptr_field.pa");
	json jout = run_sa("../test/testdata/sa/283_runtime_size_aggregates.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	// Each array is preceded by its runtime element count and its check.
	for (int i : {3, 6, 9, 12}) {
		ASSERT_EQ(stmts[i - 1]["stmt-type"], "if");
		ASSERT_EQ(stmts[i - 2]["vars"][0]["name"], "__" + stmts[i]["vars"][0]["name"].get<string>() + "_d0");
		ASSERT_EQ(stmts[i]["vars"][0]["init"]["expr-type"], "call");
	}
}

// A narrower size is widened before it is held in the uint64 __<name>_d0,
// which is read as 8 bytes.
TEST(sa, arr_size_width)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/319_arr_size_width.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	for (int i : {1, 4}) {
		const auto& init = stmts[i]["vars"][0]["init"];
		ASSERT_EQ(init["expr-type"], "convert");
		ASSERT_EQ(init["value-type"]["type-name"], "uint64");
		ASSERT_EQ(init["src"]["name"], "n");
	}
}

TEST(sa, top_var_const_in_sig)
{
	json jout = run_sa("../test/testdata/sa/328_top_var_const.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& fs = jout["functions"];
	ASSERT_EQ(fs[0]["parameters"][0]["var-type"]["arr-size"], 3);	// ps->len()
	const auto& a = fs[1]["parameters"][0]["var-type"];
	ASSERT_EQ(a["arr-size"], 4);	// len(@g)
	ASSERT_EQ(a["base-type"]["arr-size"], 2);	// g[0]->len()
	// len(a) + len(@q.v): q.v is sized by M = PN * 2.
	ASSERT_EQ(fs[1]["body"][0]["value"]["value"], "10");
}

TEST(sa, arr_borrow_return)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/336_arr_borrow_return.pa");
	ASSERT_TRUE(jout.is_object());
	map<string, json> funcs;
	for (auto& f : jout["functions"]) funcs[funcName(f["name"])] = f;
	const json elems = {{"type-kind","pntr"},{"mutable",false},{"base-type",{{"type-kind","prim"},{"type-name","uint8"}}}};
	ASSERT_EQ(funcs.at("cs")["ret-type"], elems);
	// A borrowed [+]T gives its elements: the header's data pointer.
	const json& ret = funcs.at("cs")["body"][0]["values"][0];
	ASSERT_EQ(ret["expr-type"], "field-access");
	ASSERT_EQ(ret["offset"], 16);
	ASSERT_EQ(ret["value-type"], elems);
	ASSERT_EQ(funcs.at("ar")["ret-type"]["arr-size"], 4);
	ASSERT_EQ(funcs.at("ar")["ret-type"]["mutable"], true);
	ASSERT_EQ(funcs.at("vb")["ret-type"]["base-type"]["type-kind"], "varr");
}

// A dictionary is one word pointing at the header std/rt keeps; its values'
// `[+]T` shape is recorded with its own, as the generated functions use it.
TEST(sa, dict_var)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/357_dict_var.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& f = jout["functions"][0];
	const auto& d = f["parameters"][0]["var-type"];
	ASSERT_EQ(d["base-type"]["type-kind"], "strdict");
	ASSERT_EQ(d["mutable"], false);
	ASSERT_FALSE(f["parameters"][1]["var-type"].contains("mutable"));

	const auto& body = f["body"];
	ASSERT_EQ(funcName(body[0]["vars"][0]["init"]["name"]), "__pln_alloc_dict_int32");
	ASSERT_EQ(funcName(body[1]["body"]["name"]), "__pln_copy_dict_int32");
	ASSERT_EQ(body[1]["body"]["args"][1]["name"], "d");
	ASSERT_EQ(funcName(body[2]["body"]["name"]), "__pln_copy_dict_int32");
	ASSERT_EQ(funcName(body[7]["body"]["name"]), "__pln_free_dict_P24__pln_L13_357_dict_var_P");
	ASSERT_EQ(body[7]["body"]["args"][0]["name"], "q");

	map<string, json> shapes;
	for (auto& s : jout["alloc-shapes"]) shapes[s.value("shape-key", s.value("shape-name", ""))] = s;
	ASSERT_EQ(shapes["dict_int32"]["shape-kind"], "dict");
	ASSERT_EQ(shapes["dict_int32"]["values"], "varr_int32");
	ASSERT_EQ(shapes["varr_int32"]["shape-kind"], "varr");
	ASSERT_EQ(shapes["dict_P24__pln_L13_357_dict_var_P"]["values"], "varr_P24__pln_L13_357_dict_var_P");
}

// `d[k]` indexes by 0 the value's address that a generated function returns;
// a whole store evaluates its value before the key is added.
TEST(sa, dict_index)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/358_dict_index.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& body = jout["functions"][0]["body"];
	ASSERT_EQ(funcName(body[0]["vars"][0]["init"]["name"]), "__pln_put_dict_int32");
	ASSERT_EQ(body[1]["target"]["array"]["name"], body[0]["vars"][0]["name"]);
	ASSERT_EQ(body[1]["target"]["index"]["value"], "0");
	ASSERT_EQ(body[2]["vars"][0]["init"]["src"]["expr-type"], "field-access");
	ASSERT_EQ(funcName(body[3]["vars"][0]["init"]["name"]), "__pln_put_dict_int32");
	ASSERT_EQ(body[4]["value"]["name"], body[2]["vars"][0]["name"]);
	const auto& read = body[6]["vars"][0]["init"];
	ASSERT_EQ(funcName(read["array"]["name"]), "__pln_at_dict_int32");
	ASSERT_EQ(read["array"]["args"][1]["name"], "b");
	ASSERT_EQ(read["index"]["value"], "0");

	const auto& g = jout["functions"][1]["body"][0]["values"][0];
	ASSERT_EQ(funcName(g["left"]["name"]), "__pln_has_dict_int32");
	ASSERT_EQ(funcName(g["right"]["name"]), "__pln_has_dict_int32");
}

// A 'for' over a dictionary runs over the positions std/rt finds live, with the
// key at each, and indexes the values' data as an array.
TEST(sa, dict_for)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/359_dict_for.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& block = jout["functions"][0]["body"][0]["body"];
	ASSERT_EQ(block[1]["vars"][0]["init"]["name"], "__pln_dict_next");
	ASSERT_EQ(block[2]["vars"][0]["name"], "k");
	ASSERT_EQ(block[2]["vars"][0]["init"]["name"], "__pln_dict_key");
	ASSERT_EQ(block[3]["vars"][0]["init"]["offset"], 8);
	ASSERT_EQ(block[4]["vars"][0]["init"]["offset"], 64);
	const auto& loop = block[5]["body"];
	ASSERT_EQ(loop[loop.size() - 2]["value"]["name"], "__pln_dict_next");
	ASSERT_EQ(loop[loop.size() - 1]["name"], "k");
	const auto& writes = jout["functions"][1]["body"][0]["body"];
	ASSERT_EQ(writes[3]["vars"][0]["var-type"]["mutable"], true);
}

// A string constant as the key is its string, not a variable.
TEST(sa, dict_const_key)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/360_dict_const_key.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& at = jout["functions"][0]["body"][0]["values"][0]["array"];
	ASSERT_EQ(at["args"][1]["expr-type"], "lit-str");
}

// A literal declares the dictionary, then adds each key and writes its value
// at the address the adding returns.
TEST(sa, dict_lit)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/361_dict_lit.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& body = jout["functions"][0]["body"];
	ASSERT_EQ(funcName(body[0]["vars"][0]["init"]["name"]), "__pln_alloc_dict_int32");
	for (int i : {1, 3}) {
		ASSERT_EQ(funcName(body[i]["vars"][0]["init"]["name"]), "__pln_put_dict_int32");
		ASSERT_EQ(body[i]["vars"][0]["init"]["args"][1]["expr-type"], "lit-str");
		ASSERT_EQ(body[i + 1]["target"]["array"]["name"], body[i]["vars"][0]["name"]);
	}
}

// A dictionary's values are any element of a `[+]T`: its functions take the
// lengths the values' do, `[""]$T` has functions of its own, and a dictionary
// of dictionaries keeps a dictionary in each value's slot.
TEST(sa, dict_value_types)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/362_dict_value_types.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& body = jout["statements"][0]["body"];
	const auto& rowsAlloc = body[0]["vars"][0]["init"];
	ASSERT_EQ(funcName(rowsAlloc["name"]), "__pln_alloc_dict_Pint32");
	ASSERT_EQ(rowsAlloc["args"][0]["value"], "3");
	ASSERT_EQ(funcName(body[2]["vars"][0]["init"]["name"]), "__pln_alloc_edict_8__pln_B8");
	ASSERT_EQ(body[2]["vars"][0]["var-type"]["base-type"]["embedded"], true);
	const auto& rowsFree = body[9]["body"];
	ASSERT_EQ(funcName(rowsFree["name"]), "__pln_free_dict_Pint32");
	ASSERT_EQ(rowsFree["args"][1]["value"], "3");
	const json* values = nullptr;
	for (auto& s : jout["alloc-shapes"])
		if (s["shape-key"] == "varr_PDint32") values = &s;
	ASSERT_NE(values, nullptr);
	ASSERT_EQ((*values)["elem"]["kind"], "dict");
	ASSERT_EQ((*values)["elem"]["shape-key"], "dict_int32");
}

// A dictionary held as a field or an element is a slot of elem kind "dict",
// built, freed and copied by the dictionary's own functions.
TEST(sa, dict_holders)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/363_dict_holders.pa");
	ASSERT_TRUE(jout.is_object());
	map<string, const json*> shapes;
	for (auto& s : jout["alloc-shapes"])
		shapes[s.contains("shape-key") ? s["shape-key"].get<string>() : s["shape-name"].get<string>()] = &s;
	const json& field = (*shapes.at("__pln_L17_363_dict_holders_S"))["owned-fields"][0];
	ASSERT_EQ(field["name"], "d");
	ASSERT_EQ(field["elem"]["kind"], "dict");
	ASSERT_EQ(field["elem"]["shape-key"], "dict_int32");
	ASSERT_TRUE(shapes.at("__pln_L17_363_dict_holders_S")->contains("init-func"));
	for (const char* key : {"arr_PDint32", "varr_PDint32"})
		ASSERT_EQ((*shapes.at(key))["elem"]["kind"], "dict");
}
