/// semantic analyzer tests
///
/// @file basicTests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include "../test-base/testBase.h"
#include "saTestUtil.h"

using namespace std;

TEST(sa, helloworld_sa) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/build-mgr/001_helloworld.pa");

	ASSERT_TRUE(jout.is_object());
	ASSERT_EQ(jout["target"], "x86_64-linux-gnu");

	// printf call is annotated as C function
	bool found = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "expr") continue;
		auto& expr = stmt["body"];
		if (expr["expr-type"] == "call" && expr["name"] == "printf") {
			ASSERT_EQ(expr["func-type"], "c");
			found = true;
			break;
		}
	}
	ASSERT_TRUE(found);

	// string literals are collected
	ASSERT_TRUE(jout.contains("str-literals"));
	auto& lits = jout["str-literals"];
	ASSERT_EQ(lits.size(), 1u);
	ASSERT_EQ(lits[0]["value"], "Hello World!\\n");
	ASSERT_EQ(lits[0]["label"], ".str0");

	// cinclude nodes are not emitted to SA output
	for (auto& stmt : jout["statements"])
		ASSERT_NE(stmt["stmt-type"], "cinclude");
}

TEST(sa, var_decl_emitted) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/build-mgr/002_var_decl.pa");

	ASSERT_TRUE(jout.is_object());
	bool found = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "var-decl") continue;
		for (auto& v : stmt["vars"]) {
			if (v["name"] == "x" && v["var-type"]["type-name"] == "int64") {
				ASSERT_EQ(v["init"]["expr-type"], "lit-int");
				ASSERT_EQ(v["init"]["value"],     "10");
				found = true;
			}
		}
	}
	ASSERT_TRUE(found);
}

TEST(sa, addition_sa) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/build-mgr/003_addition.pa");

	ASSERT_TRUE(jout.is_object());

	bool found_add = false;
	bool found_sub = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "expr") continue;
		auto& body = stmt["body"];
		if (body["expr-type"] == "call" && body["name"] == "printf") {
			for (auto& arg : body["args"]) {
				if (arg["expr-type"] == "add") {
					ASSERT_EQ(arg["left"]["expr-type"],  "id");
					ASSERT_EQ(arg["right"]["expr-type"], "id");
					ASSERT_EQ(arg["value-type"]["type-name"],          "int64");
					ASSERT_EQ(arg["left"]["value-type"]["type-name"],  "int64");
					ASSERT_EQ(arg["right"]["value-type"]["type-name"], "int64");
					found_add = true;
				}
				if (arg["expr-type"] == "sub") {
					ASSERT_EQ(arg["left"]["expr-type"],  "id");
					ASSERT_EQ(arg["right"]["expr-type"], "id");
					ASSERT_EQ(arg["value-type"]["type-name"],          "int64");
					ASSERT_EQ(arg["left"]["value-type"]["type-name"],  "int64");
					ASSERT_EQ(arg["right"]["value-type"]["type-name"], "int64");
					found_sub = true;
				}
			}
		}
	}
	ASSERT_TRUE(found_add);
	ASSERT_TRUE(found_sub);
}

TEST(sa, comparison_sa) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/build-mgr/005_comparison.pa");

	ASSERT_TRUE(jout.is_object());

	bool found_lt = false;
	bool found_eq = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "expr") continue;
		auto& body = stmt["body"];
		if (body["expr-type"] == "call" && body["name"] == "printf") {
			for (auto& a : body["args"]) {
				if (a["expr-type"] != "convert" || a["src"]["expr-type"] != "cmp") continue;
				ASSERT_EQ(a["value-type"]["type-name"], "int32");
				const auto& arg = a["src"];
				ASSERT_EQ(arg["value-type"]["type-name"], "bool");
				ASSERT_EQ(arg["left"]["value-type"]["type-name"],  "int64");
				ASSERT_EQ(arg["right"]["value-type"]["type-name"], "int64");
				if (arg["op"] == "<")  found_lt = true;
				if (arg["op"] == "==") found_eq = true;
			}
		}
	}
	ASSERT_TRUE(found_lt);
	ASSERT_TRUE(found_eq);
}

TEST(sa, convert_widening_sa) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/001_convert_widening.pa");

	ASSERT_TRUE(jout.is_object());

	bool found = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "var-decl") continue;
		for (auto& v : stmt["vars"]) {
			if (v["name"] != "y") continue;
			auto& init = v["init"];
			// convert_node_widening: convert node structure
			ASSERT_EQ(init["expr-type"],               "convert");
			ASSERT_EQ(init["from-type"]["type-name"],  "int32");
			ASSERT_EQ(init["value-type"]["type-name"], "int64");
			ASSERT_EQ(init["src"]["expr-type"],        "id");
			ASSERT_EQ(init["src"]["name"],             "x");
			// int32_id_value_type: src id carries int32 value-type
			ASSERT_EQ(init["src"]["value-type"]["type-kind"], "prim");
			ASSERT_EQ(init["src"]["value-type"]["type-name"], "int32");
			found = true;
		}
	}
	ASSERT_TRUE(found);
}

TEST(sa, add_mixed_types_wraps_narrower_in_convert) {
	cleanTestEnv();
	// int32 a + int64 b → the int32 operand must be wrapped in a convert node
	json jout = run_sa("../test/testdata/sa/002_add_mixed_types.pa");

	ASSERT_TRUE(jout.is_object());

	bool found = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "expr") continue;
		auto& body = stmt["body"];
		if (body["expr-type"] != "call" || body["name"] != "printf") continue;
		for (auto& arg : body["args"]) {
			if (arg["expr-type"] != "add") continue;
			// left operand (int32 a) must be wrapped in convert → int64
			ASSERT_EQ(arg["left"]["expr-type"],               "convert");
			ASSERT_EQ(arg["left"]["from-type"]["type-name"],  "int32");
			ASSERT_EQ(arg["left"]["value-type"]["type-name"], "int64");
			ASSERT_EQ(arg["left"]["src"]["expr-type"],        "id");
			ASSERT_EQ(arg["left"]["src"]["name"],             "a");
			// right operand (int64 b) needs no convert
			ASSERT_EQ(arg["right"]["expr-type"],              "id");
			ASSERT_EQ(arg["right"]["value-type"]["type-name"],"int64");
			ASSERT_EQ(arg["value-type"]["type-name"],         "int64");
			found = true;
		}
	}
	ASSERT_TRUE(found);
}

TEST(sa, lit_int_default_value_type) {
	cleanTestEnv();
	// lit-int used directly as a function arg (no expected type context) → value-type: int64
	json jout = run_sa("../test/testdata/sa/003_lit_int_printf.pa");

	ASSERT_TRUE(jout.is_object());

	bool found = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "expr") continue;
		auto& body = stmt["body"];
		if (body["expr-type"] != "call" || body["name"] != "printf") continue;
		for (auto& arg : body["args"]) {
			if (arg["expr-type"] != "lit-int") continue;
			ASSERT_EQ(arg["value-type"]["type-kind"], "prim");
			ASSERT_EQ(arg["value-type"]["type-name"], "int64");
			found = true;
		}
	}
	ASSERT_TRUE(found);
}

TEST(sa, cast_explicit_emits_convert) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/006_cast_explicit.pa");
	ASSERT_TRUE(jout.is_object());

	bool found = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "expr") continue;
		auto& call = stmt["body"];
		if (call["expr-type"] != "call" || call["name"] != "printf") continue;
		// args[1] is int32(x) → convert node
		auto& arg = call["args"][1];
		ASSERT_EQ(arg["expr-type"],              "convert");
		ASSERT_EQ(arg["from-type"]["type-name"],  "int64");
		ASSERT_EQ(arg["value-type"]["type-name"], "int32");
		ASSERT_EQ(arg["src"]["expr-type"],        "id");
		ASSERT_EQ(arg["src"]["name"],             "x");
		found = true;
		break;
	}
	ASSERT_TRUE(found);
}

TEST(sa, cast_identical_returns_src) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/007_cast_identical.pa");
	ASSERT_TRUE(jout.is_object());

	bool found = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "var-decl" || stmt["vars"][0]["name"] != "y") continue;
		auto& expr = stmt["vars"][0]["init"];
		// cast disappears; id node comes through directly
		ASSERT_EQ(expr["expr-type"], "id");
		ASSERT_EQ(expr["name"],      "x");
		found = true;
		break;
	}
	ASSERT_TRUE(found);
}

TEST(sa, cast_signed_to_unsigned_emits_convert) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/008_cast_signed_unsigned.pa");
	ASSERT_TRUE(jout.is_object());

	bool found = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "expr") continue;
		auto& call = stmt["body"];
		if (call["expr-type"] != "call" || call["name"] != "printf") continue;
		auto& arg = call["args"][1];
		ASSERT_EQ(arg["expr-type"],              "convert");
		ASSERT_EQ(arg["from-type"]["type-name"],  "int32");
		ASSERT_EQ(arg["value-type"]["type-name"], "uint32");
		ASSERT_EQ(arg["src"]["expr-type"],        "id");
		ASSERT_EQ(arg["src"]["name"],             "x");
		found = true;
		break;
	}
	ASSERT_TRUE(found);
}

TEST(sa, block_scope) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/016_block_scope.pa");

	// block statement emitted in sa["statements"]
	bool found = false;
	for (auto& s : jout["statements"])
		if (s["stmt-type"] == "block") { found = true; break; }
	ASSERT_TRUE(found);

	// block body has var-decl for y and expr for printf
	for (auto& s : jout["statements"]) {
		if (s["stmt-type"] != "block") continue;
		bool has_decl = false, has_expr = false;
		for (auto& bs : s["body"]) {
			if (bs["stmt-type"] == "var-decl") has_decl = true;
			if (bs["stmt-type"] == "expr")     has_expr = true;
		}
		ASSERT_TRUE(has_decl);
		ASSERT_TRUE(has_expr);
	}
}

TEST(sa, if_stmt_sa) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/036_if_stmt.pa");

	ASSERT_TRUE(jout.is_object());
	bool found_if = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "if") continue;
		found_if = true;
		ASSERT_EQ(stmt["cond"]["expr-type"], "cmp");
		ASSERT_EQ(stmt["cond"]["value-type"]["type-name"], "bool");
		// then block contains a printf call
		ASSERT_EQ(stmt["then"]["stmt-type"], "block");
		ASSERT_FALSE(stmt.contains("else"));
	}
	ASSERT_TRUE(found_if);
}

TEST(sa, if_else_stmt_sa) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/037_if_else_stmt.pa");

	ASSERT_TRUE(jout.is_object());
	bool found_if = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "if") continue;
		found_if = true;
		ASSERT_EQ(stmt["then"]["stmt-type"], "block");
		ASSERT_TRUE(stmt.contains("else"));
		ASSERT_EQ(stmt["else"]["stmt-type"], "block");
	}
	ASSERT_TRUE(found_if);
}

TEST(sa, unary_minus) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/038_unary_minus.pa");

	// Second var-decl: y = -x  should have neg init with value-type int64
	bool found_neg = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "var-decl") continue;
		for (auto& v : stmt["vars"]) {
			if (!v.contains("init")) continue;
			auto& init = v["init"];
			if (init["expr-type"] == "neg") {
				ASSERT_TRUE(init.contains("operand"));
				ASSERT_EQ(init["value-type"]["type-name"], "int64");
				ASSERT_EQ(init["operand"]["expr-type"], "id");
				found_neg = true;
			}
		}
	}
	ASSERT_TRUE(found_neg);
}

TEST(sa, while_stmt) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/039_while_loop.pa");

	ASSERT_TRUE(jout.is_object());
	bool found_while = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "while") continue;
		found_while = true;
		ASSERT_EQ(stmt["cond"]["expr-type"], "cmp");
		ASSERT_EQ(stmt["cond"]["value-type"]["type-name"], "bool");
		ASSERT_EQ(stmt["cond"]["op"], "<");
		// body is a raw array containing an assign statement
		ASSERT_TRUE(stmt.contains("body"));
		ASSERT_EQ(stmt["body"].size(), 1u);
		ASSERT_EQ(stmt["body"][0]["stmt-type"], "assign");
	}
	ASSERT_TRUE(found_while);
}

TEST(sa, float_var_decl) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/041_float_var_decl.pa");

	ASSERT_TRUE(jout.is_object());
	ASSERT_EQ(jout["statements"].size(), 3u);

	// flo64 x = 3.14 → lit-flo adopts flo64 type
	const auto& x = jout["statements"][0]["vars"][0];
	ASSERT_EQ(x["name"], "x");
	ASSERT_EQ(x["var-type"]["type-name"], "flo64");
	ASSERT_EQ(x["init"]["expr-type"], "lit-flo");
	ASSERT_EQ(x["init"]["value-type"]["type-name"], "flo64");

	// flo32 y = 1.5 → lit-flo adopts flo32 type
	const auto& y = jout["statements"][1]["vars"][0];
	ASSERT_EQ(y["name"], "y");
	ASSERT_EQ(y["var-type"]["type-name"], "flo32");
	ASSERT_EQ(y["init"]["value-type"]["type-name"], "flo32");

	// flo64 z (no init)
	const auto& z = jout["statements"][2]["vars"][0];
	ASSERT_EQ(z["name"], "z");
	ASSERT_EQ(z["var-type"]["type-name"], "flo64");
	ASSERT_FALSE(z.contains("init"));
}

TEST(sa, flo80) {
	// C's long double is flo80; a constant is folded at its precision.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/355_flo80.pa");
	ASSERT_TRUE(jout.is_object());
	const json& stmts = jout["statements"];
	json f80 = {{"type-kind", "prim"}, {"type-name", "flo80"}};
	const json& acosl = stmts[0]["vars"][0]["init"];
	ASSERT_EQ(acosl["name"], "acosl");
	ASSERT_EQ(acosl["value-type"], f80);
	ASSERT_EQ(acosl["args"][0]["value-type"], f80);
	const json& third = stmts[1]["vars"][0]["init"];
	ASSERT_EQ(third["expr-type"], "lit-flo");
	ASSERT_EQ(third["value"], "0.33333333333333333334");
	ASSERT_EQ(third["value-type"], f80);
	ASSERT_EQ(stmts[2]["vars"][0]["init"]["right"]["value-type"], f80);
	const json& narrow = stmts[3]["vars"][0]["init"];
	ASSERT_EQ(narrow["expr-type"], "convert");
	ASSERT_EQ(narrow["from-type"], f80);
}

TEST(sa, int_to_float_implicit) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/042_int_to_float_implicit.pa");

	ASSERT_TRUE(jout.is_object());
	ASSERT_EQ(jout["statements"].size(), 2u);

	// int64 n = 3 → plain var-decl, no convert
	const auto& n = jout["statements"][0]["vars"][0];
	ASSERT_EQ(n["var-type"]["type-name"], "int64");

	// flo64 x = n → init wrapped in convert(int64 → flo64)
	const auto& x = jout["statements"][1]["vars"][0];
	ASSERT_EQ(x["var-type"]["type-name"], "flo64");
	ASSERT_EQ(x["init"]["expr-type"], "convert");
	ASSERT_EQ(x["init"]["from-type"]["type-name"], "int64");
	ASSERT_EQ(x["init"]["value-type"]["type-name"], "flo64");
	ASSERT_EQ(x["init"]["src"]["expr-type"], "id");
	ASSERT_EQ(x["init"]["src"]["name"], "n");
}

TEST(sa, logical_ops) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/054_logical_ops.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	// [0] int64 a, [1] int64 b, [2] int32 c, [3] a&&b, [4] a||b, [5] !a, [6] a&&b||!a, [7] a&&int32(c)
	ASSERT_GE(stmts.size(), 8u);

	// a && b  →  value-type bool
	const auto& land = stmts[3]["vars"][0]["init"];
	ASSERT_EQ(land["expr-type"],               "logical-and");
	ASSERT_EQ(land["value-type"]["type-name"], "bool");
	ASSERT_EQ(land["left"]["name"],            "a");
	ASSERT_EQ(land["right"]["name"],           "b");

	// a || b  →  value-type bool
	const auto& lor = stmts[4]["vars"][0]["init"];
	ASSERT_EQ(lor["expr-type"],               "logical-or");
	ASSERT_EQ(lor["value-type"]["type-name"], "bool");

	// !a  →  value-type bool
	const auto& lnot = stmts[5]["vars"][0]["init"];
	ASSERT_EQ(lnot["expr-type"],               "logical-not");
	ASSERT_EQ(lnot["value-type"]["type-name"], "bool");
	ASSERT_EQ(lnot["operand"]["name"],         "a");

	// a && b || !a  →  (a&&b) || (!a)
	const auto& mixed = stmts[6]["vars"][0]["init"];
	ASSERT_EQ(mixed["expr-type"],                  "logical-or");
	ASSERT_EQ(mixed["value-type"]["type-name"],    "bool");
	ASSERT_EQ(mixed["left"]["expr-type"],          "logical-and");
	ASSERT_EQ(mixed["right"]["expr-type"],         "logical-not");

	// a && int32(c)  →  convert wraps c, value-type bool
	const auto& with_cast = stmts[7]["vars"][0]["init"];
	ASSERT_EQ(with_cast["expr-type"],               "logical-and");
	ASSERT_EQ(with_cast["value-type"]["type-name"], "bool");
}

TEST(sa, bitwise_ops) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/158_bitwise_ops.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	// [0] int64 a, [1] int64 b, [2] int32 c,
	// [3] a&b, [4] a|b, [5] a^b, [6] ~a, [7] ~c, [8] a & int32(c)
	ASSERT_GE(stmts.size(), 9u);

	// a & b  →  value-type int64 (both operands already int64)
	const auto& band = stmts[3]["vars"][0]["init"];
	ASSERT_EQ(band["expr-type"],               "bitand");
	ASSERT_EQ(band["value-type"]["type-name"], "int64");
	ASSERT_EQ(band["left"]["name"],             "a");
	ASSERT_EQ(band["right"]["name"],            "b");

	// a | b  →  value-type int64
	const auto& bor = stmts[4]["vars"][0]["init"];
	ASSERT_EQ(bor["expr-type"],               "bitor");
	ASSERT_EQ(bor["value-type"]["type-name"], "int64");

	// a ^ b  →  value-type int64
	const auto& bxor = stmts[5]["vars"][0]["init"];
	ASSERT_EQ(bxor["expr-type"],               "bitxor");
	ASSERT_EQ(bxor["value-type"]["type-name"], "int64");

	// ~a  →  value-type int64 (preserves operand width, unlike logical-not which
	// always collapses to int32)
	const auto& bnot64 = stmts[6]["vars"][0]["init"];
	ASSERT_EQ(bnot64["expr-type"],               "bitnot");
	ASSERT_EQ(bnot64["value-type"]["type-name"], "int64");
	ASSERT_EQ(bnot64["operand"]["name"],         "a");

	// ~c  →  value-type int32 (preserves the narrower operand's own width)
	const auto& bnot32 = stmts[7]["vars"][0]["init"];
	ASSERT_EQ(bnot32["expr-type"],               "bitnot");
	ASSERT_EQ(bnot32["value-type"]["type-name"], "int32");

	// a & int32(c)  →  int64 & int32: right side widened to int64 via a convert node
	const auto& mixed = stmts[8]["vars"][0]["init"];
	ASSERT_EQ(mixed["expr-type"],               "bitand");
	ASSERT_EQ(mixed["value-type"]["type-name"], "int64");
	ASSERT_EQ(mixed["right"]["expr-type"],      "convert");
}

TEST(sa, lit_uint_typed)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/064_lit_uint_typed.pa");
	ASSERT_TRUE(jout.is_object());

	// uint32 x = 5u → lit-uint adopts uint32 from expectedType
	const auto& x = jout["statements"][0]["vars"][0];
	ASSERT_EQ(x["name"], "x");
	ASSERT_EQ(x["init"]["expr-type"], "lit-uint");
	ASSERT_EQ(x["init"]["value-type"]["type-name"], "uint32");

	// uint8 y = 3u → lit-uint adopts uint8 from expectedType
	const auto& y = jout["statements"][1]["vars"][0];
	ASSERT_EQ(y["name"], "y");
	ASSERT_EQ(y["init"]["value-type"]["type-name"], "uint8");
}

TEST(sa, cmp_mixed_int_widening)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/066_cmp_mixed_int.pa");
	ASSERT_TRUE(jout.is_object());

	// int32 x = a(int32) < b(int64) → left(a) wrapped in convert
	const auto& x_cmp = jout["statements"][2]["vars"][0]["init"]["src"];
	ASSERT_EQ(x_cmp["expr-type"], "cmp");
	ASSERT_EQ(x_cmp["left"]["expr-type"],              "convert");
	ASSERT_EQ(x_cmp["left"]["value-type"]["type-name"], "int64");

	// int32 y = b(int64) < a(int32) → right(a) wrapped in convert
	const auto& y_cmp = jout["statements"][3]["vars"][0]["init"]["src"];
	ASSERT_EQ(y_cmp["expr-type"], "cmp");
	ASSERT_EQ(y_cmp["right"]["expr-type"],              "convert");
	ASSERT_EQ(y_cmp["right"]["value-type"]["type-name"], "int64");
}

TEST(sa, arith_right_widen)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/067_arith_right_widen.pa");
	ASSERT_TRUE(jout.is_object());

	// int64 c = a(int64) + b(int32) → right(b) wrapped in convert to int64
	const auto& add = jout["statements"][2]["vars"][0]["init"];
	ASSERT_EQ(add["expr-type"], "add");
	ASSERT_EQ(add["left"]["expr-type"],               "id");
	ASSERT_EQ(add["left"]["value-type"]["type-name"],  "int64");
	ASSERT_EQ(add["right"]["expr-type"],               "convert");
	ASSERT_EQ(add["right"]["value-type"]["type-name"], "int64");
}

TEST(sa, arith_lit_retypes_to_right)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/068_arith_lit_retypes.pa");
	ASSERT_TRUE(jout.is_object());

	// int64 c = 4 + b(int64) → lit-int on left retypes to int64 via second pass
	const auto& add = jout["statements"][1]["vars"][0]["init"];
	ASSERT_EQ(add["expr-type"], "add");
	ASSERT_EQ(add["left"]["expr-type"],               "lit-int");
	ASSERT_EQ(add["left"]["value-type"]["type-name"],  "int64");
	ASSERT_EQ(add["right"]["value-type"]["type-name"], "int64");
}

TEST(sa, type_alias_var)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/120_type_alias_var.pa");
	ASSERT_TRUE(jout.is_object());

	// type-alias is consumed; first stmt is var-decl for x, resolved to int64
	const auto& decl = jout["statements"][0];
	ASSERT_EQ(decl["stmt-type"], "var-decl");
	const auto& v = decl["vars"][0];
	ASSERT_EQ(v["name"], "x");
	ASSERT_EQ(v["var-type"]["type-kind"], "prim");
	ASSERT_EQ(v["var-type"]["type-name"], "int64");
	ASSERT_EQ(v["init"]["value-type"]["type-name"], "int64");
}

TEST(sa, type_alias_func)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/121_type_alias_func.pa");
	ASSERT_TRUE(jout.is_object());

	const json* doubleFunc = nullptr;
	for (auto& f : jout["functions"])
		if (funcName(f["name"]) == "double") doubleFunc = &f;
	ASSERT_NE(doubleFunc, nullptr);

	// Count param/ret resolved to uint64, not left as the alias name
	ASSERT_EQ((*doubleFunc)["parameters"][0]["var-type"]["type-kind"], "prim");
	ASSERT_EQ((*doubleFunc)["parameters"][0]["var-type"]["type-name"], "uint64");
	ASSERT_EQ((*doubleFunc)["ret-type"]["type-kind"], "prim");
	ASSERT_EQ((*doubleFunc)["ret-type"]["type-name"], "uint64");

	// body: n * 2 -> r; assign stmt with mul expression, no crash resolving Count
	bool found_assign = false;
	for (auto& stmt : (*doubleFunc)["body"]) {
		if (stmt["stmt-type"] != "assign") continue;
		ASSERT_EQ(stmt["name"], "r");
		ASSERT_EQ(stmt["value"]["value-type"]["type-name"], "uint64");
		found_assign = true;
	}
	ASSERT_TRUE(found_assign);
}

TEST(sa, flo32_to_flo64_assign)
{
	// flo32 x; 0.5->x; flo64 y; x->y;
	// x->y: typeCompat(flo32,flo64)=ImplicitWiden → sa_assign_stmt wraps in convert
	// Covers: sa_assign_stmt wrapConvert branch, primRank Float32+Float64
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/085_flo32_to_flo64_assign.pa");
	ASSERT_TRUE(jout.is_object());

	// statements: [0]=decl x, [1]=assign x=0.5, [2]=decl y, [3]=assign y=x
	const auto& assign_stmt = jout["statements"][3];
	ASSERT_EQ(assign_stmt["stmt-type"], "assign");
	ASSERT_EQ(assign_stmt["name"],      "y");
	const auto& val = assign_stmt["value"];
	ASSERT_EQ(val["expr-type"],                  "convert");
	ASSERT_EQ(val["from-type"]["type-name"],     "flo32");
	ASSERT_EQ(val["value-type"]["type-name"],    "flo64");
}

TEST(sa, int8_to_int16_assign)
{
	// int8 a; 10->a; int16 b; a->b;
	// a->b: typeCompat(int8,int16)=ImplicitWiden → convert wrapper
	// Covers: primRank Int16
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/086_int8_to_int16_assign.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& assign_stmt = jout["statements"][3];
	ASSERT_EQ(assign_stmt["stmt-type"], "assign");
	ASSERT_EQ(assign_stmt["name"],      "b");
	const auto& val = assign_stmt["value"];
	ASSERT_EQ(val["expr-type"],               "convert");
	ASSERT_EQ(val["from-type"]["type-name"],  "int8");
	ASSERT_EQ(val["value-type"]["type-name"], "int16");
}

TEST(sa, if_else_if_chain)
{
	// if x==1 { ... } else if x==2 { ... }
	// Covers: sa_if_stmt else-if branch (els["stmt-type"] == "if")
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/087_if_else_if.pa");
	ASSERT_TRUE(jout.is_object());

	// Find the if statement
	json* if_stmt = nullptr;
	for (auto& s : jout["statements"])
		if (s["stmt-type"] == "if") { if_stmt = &s; break; }
	ASSERT_NE(if_stmt, nullptr);

	// else clause must be an if statement directly (not a block)
	ASSERT_TRUE((*if_stmt).contains("else"));
	ASSERT_EQ((*if_stmt)["else"]["stmt-type"], "if");
	ASSERT_EQ((*if_stmt)["else"]["cond"]["op"], "==");
}

TEST(sa, const_decl_basic)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/125_const_decl_basic.pa");
	ASSERT_TRUE(jout.is_object());
	ASSERT_EQ(jout["statements"].size(), 1);

	// const-decl is consumed; first stmt is var-decl for x, init inlined to lit-int 100
	const auto& decl = jout["statements"][0];
	ASSERT_EQ(decl["stmt-type"], "var-decl");
	const auto& v = decl["vars"][0];
	ASSERT_EQ(v["name"], "x");
	ASSERT_EQ(v["init"]["expr-type"], "lit-int");
	ASSERT_EQ(v["init"]["value"], "100");
	ASSERT_EQ(v["init"]["value-type"]["type-name"], "int64");
}

TEST(sa, const_decl_chain)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/126_const_decl_chain.pa");
	ASSERT_TRUE(jout.is_object());
	ASSERT_EQ(jout["statements"].size(), 1);
	ASSERT_EQ(jout["statements"][0]["vars"][0]["init"]["value"], "10");
}

TEST(sa, int_literal_range)
{
	// Boundary values of each literal's adopted type are accepted, and a
	// literal operand is typed only from the other operand (300 never takes
	// a provisional narrower type).
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/199_int_literal_range.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts[0]["vars"][0]["init"]["value"], "-128");
	ASSERT_EQ(stmts[2]["vars"][0]["init"]["value"], "-9223372036854775808");
	ASSERT_EQ(stmts[3]["vars"][0]["init"]["value"], "-128");
	ASSERT_EQ(stmts[4]["vars"][0]["init"]["value-type"]["type-name"], "uint64");
	ASSERT_EQ(stmts[5]["vars"][0]["init"]["value-type"]["type-name"], "uint8");
	auto& g = stmts[7]["vars"][0]["init"];
	ASSERT_EQ(g["left"]["value-type"]["type-name"], "int64");
}

TEST(sa, recursive_type)
{
	cleanTestEnv();
	for (string file : {"1211_borrow_field_cycle", "1297_self_borrow_varr", "1298_self_borrow_dict",
	                    "1301_self_row_borrow", "1305_self_local"})
		ASSERT_TRUE(run_sa("../test/testdata/sa/" + file + ".pa").is_object()) << file;
	json jout = run_sa("../test/testdata/sa/1374_recursive_type.pa");
	ASSERT_TRUE(jout.is_object());
	set<string> keys;
	for (auto& s : jout["alloc-shapes"])
		EXPECT_TRUE(keys.insert(s.contains("shape-key") ? s["shape-key"] : s["shape-name"]).second) << s;
	EXPECT_EQ(keys.size(), 3u);
}

TEST(sa, bool_type)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/200_bool.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts[0]["vars"][0]["init"]["value-type"]["type-name"], "bool");

	// bool(x) tests x != 0 rather than truncating, for integers and floats alike.
	for (int i : {2, 3}) {
		const auto& c = stmts[i]["vars"][0]["init"];
		ASSERT_EQ(c["expr-type"], "cmp");
		ASSERT_EQ(c["value-type"]["type-name"], "bool");
		ASSERT_EQ(c["op"], "!=");
	}
	ASSERT_EQ(stmts[3]["vars"][0]["init"]["right"]["expr-type"], "lit-flo");

	const auto& w = stmts[4]["vars"][0]["init"];
	ASSERT_EQ(w["expr-type"], "convert");
	ASSERT_EQ(w["value-type"]["type-name"], "int32");

	// Operators promote a bool operand to int32, so `t + 1` is 2, not a 1-byte sum.
	const auto& s = stmts[5]["vars"][0]["init"];
	ASSERT_EQ(s["value-type"]["type-name"], "int32");
	ASSERT_EQ(s["left"]["expr-type"], "convert");
	ASSERT_EQ(s["right"]["value-type"]["type-name"], "int32");
	ASSERT_EQ(stmts[6]["vars"][0]["init"]["operand"]["expr-type"], "convert");
	ASSERT_EQ(stmts[7]["cond"]["left"]["expr-type"], "convert");

	ASSERT_EQ(stmts[8]["cond"]["value-type"]["type-name"], "bool");
	ASSERT_EQ(stmts[9]["body"]["args"][0]["value-type"]["type-name"], "bool");
	ASSERT_EQ(stmts[10]["vars"][0]["init"]["args"][0]["value"], "2");
	ASSERT_EQ(stmts[12]["vars"][0]["init"]["value-type"]["type-name"], "bool");
	ASSERT_EQ(stmts[13]["body"]["args"][0]["value-type"]["type-name"], "int32");

	// A variadic bool argument is promoted to int32, as C does for _Bool.
	const auto& pa = stmts[14]["body"]["args"];
	ASSERT_EQ(pa[1]["expr-type"], "convert");
	ASSERT_EQ(pa[1]["value-type"]["type-name"], "int32");
	ASSERT_EQ(pa[2]["value-type"]["type-name"], "int32");
}

TEST(sa, enum_type) {
	json jout = written(run_sa("../test/testdata/sa/222_enum.pa"));
	ASSERT_TRUE(jout.is_object());
	auto enumType = [](const char* base, const char* name) {
		return json{{"type-kind","prim"},{"type-name",base},{"enum",name}};
	};
	json color = enumType("int32", "Color");
	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts[0]["vars"][0]["var-type"], color);
	ASSERT_EQ(stmts[0]["vars"][0]["init"],
		(json{{"expr-type","lit-int"},{"value","5"},{"value-type",color},{"loc",{10,11,10,19}}}));
	const auto& widen = stmts[1]["vars"][0]["init"];
	ASSERT_EQ(widen["expr-type"], "convert");
	ASSERT_EQ(widen["from-type"], color);
	// The base type is C's enumerator type: int32 whenever every value fits.
	ASSERT_EQ(stmts[3]["vars"][0]["var-type"], enumType("int32", "Sign"));
	ASSERT_EQ(stmts[4]["vars"][0]["var-type"], enumType("uint32", "Flags"));
	ASSERT_EQ(stmts[5]["vars"][0]["var-type"], enumType("uint64", "Big"));
	ASSERT_EQ(stmts[6]["vars"][0]["var-type"], enumType("int64", "Wide"));

	const auto& next = jout["functions"][0];
	ASSERT_EQ(next["parameters"][0]["var-type"], color);
	ASSERT_EQ(next["ret-type"], color);
	// Color(c + 1): the sum is int32, and only the enum label is put back.
	json cast = next["body"][0]["values"][0];
	ASSERT_EQ(cast["expr-type"], "add");
	ASSERT_EQ(cast["value-type"], color);
}

TEST(sa, sizeof) {
	json jout = run_sa("../test/testdata/sa/225_sizeof.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	const char* sizes[] = {"2", "16", "4", "12", "8", "12", "12", "24", "36", "32", "16", "12", "12"};
	for (size_t i = 0; i < size(sizes); i++) {
		const auto& init = stmts[i]["vars"][0]["init"];
		ASSERT_EQ(init["expr-type"], "lit-uint") << i;
		ASSERT_EQ(init["value"], sizes[i]) << i;
	}
	ASSERT_EQ(stmts[0]["vars"][0]["init"]["value-type"]["type-name"], "uint64");
	ASSERT_EQ(stmts[12]["vars"][0]["init"]["value-type"]["type-name"], "uint32");
	ASSERT_EQ(stmts[13]["vars"][0]["var-type"]["arr-size"], 16);
}

TEST(sa, alias_cast)
{
	cleanTestEnv();
	genLibSaTypes();
	json jout = written(run_sa("../test/testdata/sa/236_alias_cast.pa"));
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts[1]["vars"][0]["init"]["expr-type"], "convert");
	ASSERT_EQ(stmts[1]["vars"][0]["init"]["value-type"]["type-name"], "uint8");
	ASSERT_EQ(stmts[2]["vars"][0]["init"]["value-type"]["type-name"], "uint64");
	ASSERT_EQ(stmts[3]["vars"][0]["init"]["value-type"]["enum"], "Color");
	ASSERT_EQ(stmts[4]["vars"][0]["init"]["value-type"]["type-name"], "int64");
}

TEST(sa, as_cast)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/263_as_cast.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	auto init = [&](size_t i) -> const json& { return stmts[i]["vars"][0]["init"]; };
	// '@T' -> '@!T' is allowed by an explicit cast.
	ASSERT_EQ(init(2)["expr-type"], "convert");
	ASSERT_EQ(init(2)["value-type"]["mutable"], true);
	ASSERT_EQ(init(3)["value-type"]["type-name"], "uint64");
	ASSERT_EQ(init(4)["from-type"]["type-name"], "uint64");
	ASSERT_EQ(init(6)["value-type"]["arr-size"], 4);
	ASSERT_EQ(init(6)["value-type"]["base-type"]["type-name"], "int32");
	// A pointer alias casts like the type it names.
	ASSERT_EQ(init(8)["value-type"]["base-type"]["type-name"], "int32");
	ASSERT_EQ(init(9)["value-type"]["type-name"], "int32");
	// A cast to the source's own type leaves no convert node.
	ASSERT_EQ(init(10)["expr-type"], "id");
}

TEST(sa, const_expr)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/265_const_expr.pa");
	ASSERT_TRUE(jout.is_object());
	auto& st = jout["statements"];
	auto init = [&](int i) { return st[i]["vars"][0]["init"]; };
	const pair<string, string> lits[] = {
		{"lit-int", "11"}, {"lit-int", "1"}, {"lit-int", "-3"}, {"lit-int", "255"},
		{"lit-flo", "3.0"}, {"lit-flo", "3.25"}, {"lit-uint", "8"}, {"lit-uint", "13"},
		{"lit-int", "5"},
	};
	for (int i = 0; i < 9; i++) {
		ASSERT_EQ(init(i)["expr-type"], lits[i].first) << i;
		ASSERT_EQ(init(i)["value"], lits[i].second) << i;
	}
	// S { [A + 1]$int32 v; } is 16 bytes.
	ASSERT_EQ(init(9)["args"][0]["value"], "16");
	ASSERT_EQ(st[10]["vars"][0]["var-type"]["arr-size"], 6);
	// Only an all-constant subtree folds; `b + 1 + 2` is (b + 1) + 2.
	ASSERT_EQ(init(11)["expr-type"], "add");
	// A typed C macro constant loses its type inside a constant expression.
	ASSERT_EQ(init(12)["expr-type"], "lit-int");
	ASSERT_EQ(init(12)["value"], "2147483648");
}

TEST(sa, const_expr_ops)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/281_const_expr_ops.pa");
	ASSERT_TRUE(jout.is_object());
	const pair<string, string> lits[] = {
		{"lit-uint", "3"}, {"lit-uint", "1"}, {"lit-uint", "2"}, {"lit-uint", "7"}, {"lit-uint", "5"},
		{"lit-uint", "0"}, {"lit-int", "0"}, {"lit-int", "7"}, {"lit-flo", "1.0"}, {"lit-flo", "1.5"},
		{"lit-flo", "3.5"}, {"lit-uint", "8"}, {"lit-int", "4"}, {"lit-int", "15"}, {"lit-int", "5"},
	};
	for (int i = 0; i < 15; i++) {
		const json& init = jout["statements"][i]["vars"][0]["init"];
		ASSERT_EQ(init["expr-type"], lits[i].first) << i;
		ASSERT_EQ(init["value"], lits[i].second) << i;
	}
}

TEST(sa, shift)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/267_shift.pa");
	ASSERT_TRUE(jout.is_object());
	auto init = [&](int i) { return jout["statements"][i]["vars"][0]["init"]; };
	// The result keeps the left operand's type; the count is converted to it.
	ASSERT_EQ(init(2)["expr-type"], "shl");
	ASSERT_EQ(init(2)["value-type"]["type-name"], "int8");
	ASSERT_EQ(init(2)["right"]["expr-type"], "convert");
	ASSERT_EQ(init(2)["right"]["value-type"]["type-name"], "int8");
	ASSERT_EQ(init(3)["value-type"]["type-name"], "int32");
	// An untyped constant shifts at 64 bits.
	ASSERT_EQ(init(4)["value"], "1099511627776");
	ASSERT_EQ(init(5)["value"], "9223372036854775804");
}

TEST(sa, store_value_first_plain)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/280_store_value_first_plain.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];

	// A value no call can reach is stored as written: no temp, no block.
	ASSERT_EQ(stmts[2]["stmt-type"], "arr-assign");
	ASSERT_EQ(stmts[2]["value"]["name"], "x");
	ASSERT_EQ(stmts[5]["stmt-type"], "expr");
	ASSERT_EQ(stmts[7]["stmt-type"], "arr-assign");

	// c is given to memset afterwards, so its element is read before f().
	const auto& blk = stmts[9];
	ASSERT_EQ(blk["stmt-type"], "block");
	ASSERT_EQ(blk["body"][0]["stmt-type"], "var-decl");
	ASSERT_EQ(blk["body"][0]["vars"][0]["init"]["array"]["name"], "c");
	ASSERT_EQ(blk["body"][1]["value"]["name"], blk["body"][0]["vars"][0]["name"]);
	ASSERT_EQ(jout.dump().find("pending-alias"), string::npos);
}

// The top level builds the header of `args` from its argc/argv only when it
// names `args`, and the builtins come with it.
TEST(sa, implicit_args)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/320_implicit_args.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	const auto& decl = stmts[0]["vars"][0];
	ASSERT_EQ(decl["name"], "args");
	ASSERT_EQ(decl["var-type"]["mutable"], false);
	ASSERT_EQ(decl["var-type"]["base-type"]["type-kind"], "varr");
	ASSERT_EQ(decl["var-type"]["base-type"]["base-type"]["mutable"], false);
	ASSERT_EQ(decl["init"]["expr-type"], "frame-alloc");
	ASSERT_EQ(decl["init"]["size"], 24);
	const pair<int, string> fields[] = {{0, "__argc"}, {8, "__argc"}, {16, "__argv"}};
	for (int i = 0; i < 3; i++) {
		const auto& fa = stmts[i + 1];
		ASSERT_EQ(fa["stmt-type"], "field-assign");
		ASSERT_EQ(fa["var"], "args");
		ASSERT_EQ(fa["offset"], fields[i].first);
		ASSERT_EQ(fa["value"]["name"], fields[i].second);
	}
	ASSERT_EQ(stmts[4]["vars"][0]["init"]["args"][0]["name"], "args");
	ASSERT_EQ(jout["alloc-shapes"].size(), 1);
	ASSERT_EQ(jout["alloc-shapes"][0]["shape-key"], "varr_int64");

	// A function's own `args` is unrelated, and leaves the top level as it was.
	jout = run_sa("../test/testdata/sa/321_implicit_args_unused.pa");
	ASSERT_TRUE(jout.is_object());
	ASSERT_EQ(jout["statements"].size(), 1);
	ASSERT_EQ(jout["statements"][0]["vars"][0]["name"], "n");
	ASSERT_TRUE(jout["alloc-shapes"].empty());
	ASSERT_EQ(jout["functions"][0]["parameters"][0]["name"], "args");
}

// A for loop becomes a block holding the array and its length, around a while
// that steps the index at its end and before each continue.
TEST(sa, for_loop)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/322_for.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	const auto& b = stmts[3]["body"];
	ASSERT_EQ(stmts[3]["stmt-type"], "block");
	ASSERT_EQ(b.size(), 4);
	ASSERT_EQ(b[0]["vars"][0]["init"]["name"], "v");
	ASSERT_EQ(b[0]["vars"][0]["var-type"]["mutable"], true);
	ASSERT_EQ(b[1]["vars"][0]["name"], "i");
	ASSERT_EQ(b[2]["vars"][0]["init"]["expr-type"], "field-access");
	ASSERT_EQ(b[2]["vars"][0]["init"]["offset"], 0);
	const auto& w = b[3];
	ASSERT_EQ(w["stmt-type"], "while");
	ASSERT_EQ(w["cond"]["op"], "<");
	ASSERT_EQ(w["body"][0]["vars"][0]["name"], "a");
	ASSERT_EQ(w["body"][0]["vars"][0]["init"]["expr-type"], "arr-index");
	const auto& cont = w["body"][1]["then"]["body"];
	ASSERT_EQ(cont[0]["stmt-type"], "assign");
	ASSERT_EQ(cont[0]["name"], "i");
	ASSERT_EQ(cont[1]["stmt-type"], "continue");
	ASSERT_EQ(w["body"].back()["stmt-type"], "assign");

	// A constant-size array's length is a literal; '@!x' is the element itself.
	const auto& c = stmts[5]["body"];
	ASSERT_EQ(c[2]["vars"][0]["init"]["value"], "3");
	const auto& x = c[3]["body"][0];
	ASSERT_EQ(x["stmt-type"], "arr-assign");
	ASSERT_EQ(x["target"]["array"]["name"], c[0]["vars"][0]["name"]);
	ASSERT_EQ(x["target"]["index"]["name"], c[1]["vars"][0]["name"]);

	// '@y' reads the range through a read-only binding.
	const auto& d = stmts[6]["body"];
	ASSERT_EQ(d[0]["vars"][0]["var-type"]["mutable"], false);
	ASSERT_EQ(d[3]["body"][0]["vars"][0]["init"]["array"]["name"], d[0]["vars"][0]["name"]);
}

// A typed index is declared in each iteration from the hidden int64 counter.
TEST(sa, for_loop_index_type)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/327_for_index_type.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& b = jout["statements"][1]["body"];
	ASSERT_EQ(b[1]["vars"][0]["var-type"]["type-name"], "int64");
	string counter = b[1]["vars"][0]["name"];
	ASSERT_NE(counter, "i");
	const auto& head = b[3]["body"][0]["vars"][0];
	ASSERT_EQ(head["name"], "i");
	ASSERT_EQ(head["var-type"]["type-name"], "uint8");
	ASSERT_EQ(head["init"]["expr-type"], "convert");
	ASSERT_EQ(head["init"]["src"]["name"], counter);
}

// A struct is iterated by its type's 'iter' and 'next': the iterator is held
// in the loop's block, and '@!c' is the element 'next' returns a borrow of.
// The body may read the struct and write its fields.
// Outside its module a type's read-only fields are read, and whole values are copied and lent.
TEST(sa, hidden_fields)
{
	cleanTestEnv();
	genModuleAst("../test/testdata/sa/lib_sa_hidden.pa");
	json jout = run_sa("../test/testdata/sa/345_hidden_fields.pa");
	ASSERT_TRUE(jout.is_object());
}

TEST(sa, for_struct)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/344_for_struct.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& b = jout["statements"][1]["body"];
	string it = b[0]["vars"][0]["name"];
	ASSERT_EQ(b[1]["body"]["args"][0]["name"], "g");
	ASSERT_EQ(b[2]["vars"][0]["name"], "i");
	ASSERT_EQ(b[3]["stmt-type"], "while");
	ASSERT_EQ(b[3]["cond"]["value"], "1");
	const auto& w = b[3]["body"];
	ASSERT_EQ(w[0]["stmt-type"], "tapple-decl");
	ASSERT_EQ(w[0]["value"]["args"][0]["name"], it);
	string e = w[0]["vars"][1]["var-name"];
	ASSERT_EQ(w[1]["stmt-type"], "if");
	ASSERT_EQ(w[1]["then"]["body"].back()["stmt-type"], "break");
	ASSERT_EQ(w[2]["target"]["array"]["name"], e);
	ASSERT_EQ(w[2]["target"]["index"]["value"], "0");
	ASSERT_EQ(w[3]["stmt-type"], "field-assign");
	ASSERT_EQ(b[4]["body"]["args"][0]["name"], it);
}

// Changes that leave the iterated array's storage and length alone: its
// elements, deeper arrays, read-only borrows, a fixed-length array in place,
// and rebinding a borrow.
TEST(sa, for_loop_allowed_changes)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/323_for_mutation.pa");
	ASSERT_TRUE(jout.is_object());
}

TEST(sa, global_var)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/329_global.pa");
	ASSERT_TRUE(jout.is_object());
	// The three globals, then the initialization's flag.
	ASSERT_EQ(jout["globals"].size(), 4);
	ASSERT_EQ(jout["globals"][0]["label"], "_329_global.hits");
	ASSERT_EQ(jout["globals"][3]["label"], "_329_global..init.done");

	// Globals are initialized in the module's ..init, so the top level only
	// binds them; what a global owns is never freed.
	ASSERT_EQ(jout["init"], "_329_global..init");
	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts.size(), 1);
	ASSERT_EQ(stmts[0]["body"]["name"], "_329_global..count.v");
	ASSERT_EQ(jout["main"]["globals"][0]["label"], "_329_global.hits");

	const auto& init = jout["functions"].back();
	ASSERT_EQ(init["name"], "_329_global..init");
	ASSERT_EQ(init["export"], true);
	ASSERT_EQ(init["globals"][0]["label"], "_329_global..init.done");
	const auto& body = init["body"];
	ASSERT_EQ(body[0]["stmt-type"], "if");
	ASSERT_EQ(body[1]["name"], "__init_done");
	ASSERT_EQ(body[2]["vars"][0]["label"], "_329_global.hits");
	// A `$T` global's storage is static too, so it outlives the initialization.
	const auto& origin = body[6]["vars"][0];
	ASSERT_EQ(origin["label"], "_329_global.origin");
	ASSERT_EQ(origin["init"]["expr-type"], "static-alloc");
	ASSERT_EQ(origin["init"]["label"], "_329_global.origin.data");
	ASSERT_EQ(body.size(), 9);

	const auto& fs = jout["functions"];
	// count: a writable scalar and a C global it is permitted to write.
	ASSERT_EQ(fs[0]["globals"][0]["name"], "hits");
	ASSERT_EQ(fs[0]["globals"][1]["label"], "optind");
	ASSERT_EQ(fs[0]["body"][1]["stmt-type"], "assign");
	ASSERT_EQ(fs[0]["body"][1]["name"], "optind");
	// move: a struct global seen as a '@!' borrow.
	ASSERT_EQ(fs[1]["globals"][0]["var-type"]["mutable"], true);
	// report: an array global seen as a '@' borrow.
	ASSERT_EQ(fs[2]["globals"][0]["name"], "arr");
	ASSERT_EQ(fs[2]["globals"][0]["var-type"]["mutable"], false);
}

// An array of constants is the initial data of a global's static storage; one
// with any other element, or with rows of its own, is stored in ..init.
TEST(sa, static_global_data)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/364_static_global_data.pa");
	ASSERT_TRUE(jout.is_object());
	map<string, json> data;
	map<string, int> stores;
	for (auto& s : jout["functions"].back()["body"]) {
		if (s["stmt-type"] == "var-decl") {
			const json& v = s["vars"][0];
			if (v["init"].contains("data")) data[v["name"]] = v["init"]["data"];
		} else if (s["stmt-type"] == "arr-assign") {
			const json* a = &s["target"];
			while (a->contains("array")) a = &(*a)["array"];
			stores[a->value("name", "")]++;
		}
	}
	ASSERT_EQ(data.size(), 2);
	ASSERT_EQ(data["m"]["type"]["type-name"], "int16");
	ASSERT_EQ(data["m"]["values"], json({"1", "-2", "4", "5"}));
	ASSERT_EQ(data["u"]["values"], json({"1.5", "2"}));
	ASSERT_EQ(stores["m"], 0);
	ASSERT_EQ(stores["u"], 0);
	ASSERT_EQ(stores["n"], 2);
	ASSERT_EQ(stores["o"], 2);
}

// A character literal of 128 or more takes an integer type of 32 bits or
// more; an ASCII one also a byte, and one with no type to take is an int.
TEST(sa, code_point_literal)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/338_code_point_literal.pa"));
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	const char* types[] = {"int32", "uint32", "uint8", "int64"};
	for (int i = 0; i < 4; i++)
		ASSERT_EQ(stmts[i]["vars"][0]["init"]["value-type"]["type-name"], types[i]) << i;
	const auto& args = stmts[4]["body"]["args"];
	ASSERT_EQ(args[1]["value-type"]["type-name"], "int32");
	ASSERT_EQ(args[2]["value-type"]["type-name"], "int32");
}

// A length known only at run time is read from the variable the array's
// declaration keeps it in; a row's own length is in its type.
TEST(sa, runtime_len)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/340_runtime_len.pa"));
	ASSERT_TRUE(jout.is_object());
	map<string, json> init;
	for (auto& s : jout["statements"])
		if (s["stmt-type"] == "var-decl") init[s["vars"][0]["name"]] = s["vars"][0]["init"];
	ASSERT_EQ(init["n"]["expr-type"], "convert");
	ASSERT_EQ(init["n"]["src"]["name"], "__a_d0");
	ASSERT_EQ(init["r"]["expr-type"], "lit-int");
	ASSERT_EQ(init["r"]["value"], "2");
}
