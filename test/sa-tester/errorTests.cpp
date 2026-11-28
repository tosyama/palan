/// semantic analyzer error case tests
///
/// @file errorTests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include "../test-base/testBase.h"
#include "saTestUtil.h"

using namespace std;

TEST(sa_error, help) {
	cleanTestEnv();
	string out = execTestCommand("palan-sa --help");
	ASSERT_NE(out.find("Usage: palan-sa"), string::npos);
}

TEST(sa_error, version) {
	cleanTestEnv();
	string out = execTestCommand("palan-sa --version");
	ASSERT_NE(out.find("palan-sa"), string::npos);
}

TEST(sa_error, no_input_file) {
	cleanTestEnv();
	string out = execTestCommand("palan-sa");
	ASSERT_NE(out.find("no input file"), string::npos);
}

TEST(sa_error, block_shadow_var) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/018_block_shadow_var.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");  // must fail: variable shadowing is forbidden
}

TEST(sa_error, block_shadow_pln_func) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/019_block_shadow_pln_func.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");  // must fail: Palan function shadowing is forbidden
}

TEST(sa_error, func_inner_scope) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/021_func_inner_scope.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");  // must fail: inner is scoped to outer, not visible in caller
}

TEST(sa_error, undefined_function_with_loc) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/022_undefined_function.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":1:1: error:"), string::npos);  // loc format: file:line:col: error:
	ASSERT_NE(sa.find("Undefined function"), string::npos);
}

TEST(sa_error, undefined_variable_with_loc) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/023_undefined_variable.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":1:"), string::npos);  // loc format
	ASSERT_NE(sa.find("Undefined variable"), string::npos);
}

TEST(sa_error, return_outside_function_with_loc) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/024_return_outside_function.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":1:"), string::npos);  // loc format
	ASSERT_NE(sa.find("Return statement outside of function"), string::npos);
}

TEST(sa_error, incompatible_type_cast) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/025_incompatible_type_cast.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("Cannot cast type"), string::npos);
}

TEST(sa_error, invalid_narrowing_init) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/026_invalid_narrowing_init.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("Implicit conversion from 'int64' to 'int8'"), string::npos);
}

TEST(sa_error, export_in_block) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/027_export_in_block.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("not allowed inside a block"), string::npos);
}

TEST(sa_error, export_in_function) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/028_export_in_function.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("not allowed inside a function"), string::npos);
}

TEST(sa_error, c_export) {
	const pair<string, string> cases[] = {
		{"error_1119_c_export_overload.pa", ":2:1: error: function 'add' cannot be exported to C: it is overloaded."},
		{"error_1120_c_export_overload_after.pa", ":2:1: error: function 'add' cannot be exported to C: it is overloaded."},
		{"error_1121_c_export_rets.pa", ":1:1: error: function 'divmod' cannot be exported to C: it returns several values."},
		{"error_1122_c_export_variadic.pa", ":1:1: error: function 'sum' cannot be exported to C: it is variadic."},
		{"error_1123_c_export_default.pa", ":1:1: error: function 'scale' cannot be exported to C: it has default arguments."},
		{"error_1124_c_export_operator.pa", ":2:1: error: function '+' cannot be exported to C: it is an operator."},
		{"error_1125_spread_not_last.pa", ":3:8: error: '...' spreads an array only as the last argument of a call."},
		{"error_1126_spread_outside_call.pa", ":2:17: error: '...' spreads an array only as the last argument of a call."},
		{"error_1127_spread_mixed.pa", ":3:11: error: the spread array gives all of 'xs' of 'sum': it must be the only argument after the other parameters'."},
		{"error_1128_spread_no_fixed.pa", ":3:5: error: the spread array gives all of 'xs' of 'sum': it must be the only argument after the other parameters'."},
		{"error_1129_spread_not_variadic.pa", ":3:3: error: '...' spreads an array only into a variadic parameter '[...]T', which 'f' does not have."},
		{"error_1130_spread_c_variadic.pa", ":3:15: error: '...' spreads an array only into a variadic parameter '[...]T', which 'printf' does not have."},
		{"error_1131_spread_unsized.pa", ":2:44: error: '[?]int64' keeps no length, so it cannot be spread with '...'."},
		{"error_1132_spread_not_array.pa", ":3:8: error: '...' spreads an array, but this is 'int64'."},
		{"error_1133_spread_literal.pa", ":2:8: error: '...' spreads an array, but this is an array literal."},
		{"error_1134_spread_elem_mismatch.pa", ":3:1: error: '...' cannot spread '[3]int32' into '[...]int64': the elements must be of exactly its type, since they are not converted."},
		{"error_1135_spread_embedded.pa", ":4:1: error: '...' cannot spread '[2]$P' into '[...]P': the elements must be of exactly its type, since they are not converted."},
		{"error_1136_spread_borrow.pa", ":3:8: error: '...' takes the array as a by-value parameter does: write '...a' for a copy or '...a>>' to give it, not '@a'."},
		{"error_1137_spread_give_borrowed.pa", ":2:49: error: '>>' needs a source that owns its value: an owning variable, an owned field or element, a call result, or a pointer viewed as an owner ('p as T')."},
		{"error_1138_spread_give_variadic.pa", ":2:51: error: '>>' needs a source that owns its value: an owning variable, an owned field or element, a call result, or a pointer viewed as an owner ('p as T')."},
		{"error_1139_spread_use_after_given.pa", ":4:11: error: 'a' was given away and is NULL here; fill it with '->>' before using it."},
		{"error_1140_spread_overload_no_match.pa", ":4:1: error: No function 'pick' takes (...[3]int64); there are pick([3]int64), pick([...]int32)."},
		{"error_1141_spread_len.pa", ":2:15: error: '...' spreads an array only into a variadic parameter '[...]T', which 'len' does not have."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand("palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		EXPECT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, multi_ret_bare_return) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/029_multi_ret_bare_return.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("Function with named return values requires bare"), string::npos);
}

TEST(sa_error, single_ret_one_expr) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/030_single_ret_one_expr.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("one return expression"), string::npos);
}

TEST(sa_error, void_bare_return) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/031_void_bare_return.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("Void function requires bare"), string::npos);
}

TEST(sa_error, tuple_undefined_function) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/032_tuple_undefined_function.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("Undefined function"), string::npos);
}

TEST(sa_error, tuple_needs_multi_ret) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/033_tuple_needs_multi_ret.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("multiple return values"), string::npos);
}

TEST(sa_error, tuple_var_count_mismatch) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/034_tuple_var_count_mismatch.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("does not match return count"), string::npos);
}

TEST(sa_error, import_file_not_found) {
	cleanTestEnv();
	string ast_out = moduleAstPath("../test/testdata/sa/035_import_file_not_found.pa");
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/035_import_file_not_found.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("Could not open import file"), string::npos);
}

// Without the work tree layout there is nowhere to look for an imported AST.
TEST(sa_error, import_outside_work_tree) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	genLibSaImport();
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/058_import_unqualified.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("Could not open import file"), string::npos);
}

TEST(sa_error, void_call_as_expr) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_040_void_call_as_expr.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("Void function call cannot be used as a value"), string::npos);
}

TEST(sa_error, break_outside_loop) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_041_break_outside_loop.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":1:"), string::npos);  // loc format
	ASSERT_NE(sa.find("Break statement outside of loop"), string::npos);
}

TEST(sa_error, continue_outside_loop) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_042_continue_outside_loop.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":1:"), string::npos);  // loc format
	ASSERT_NE(sa.find("Continue statement outside of loop"), string::npos);
}

TEST(sa_error, float_modulo) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_043_float_modulo.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("'%' operator is not supported for float types"), string::npos);
}

TEST(sa_error, float_logical_op) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_049_float_logical_op.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("Logical operator operand must be an integer type"), string::npos);
}

TEST(sa_error, float_bitand) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_121_float_bitand.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("Bitwise operator operand must be an integer type"), string::npos);
}

TEST(sa_error, float_bitnot) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_122_float_bitnot.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("Bitwise operator operand must be an integer type"), string::npos);
}

TEST(sa_error, unqualified_alias_call)
{
	cleanTestEnv();
	genModuleAst("../test/testdata/sa/lib_sa_import.pa");
	string ast_out = moduleAstPath("../test/testdata/sa/error_054_unqualified_alias_call.pa");
	ASSERT_EQ(execTestCommand("palan-gen-ast ../test/testdata/sa/error_054_unqualified_alias_call.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("requires a module alias qualifier"), string::npos);
}

TEST(sa_error, chain_alias_undefined)
{
	cleanTestEnv();
	genModuleAst("../test/testdata/sa/lib_sa_import.pa");
	string ast_out = moduleAstPath("../test/testdata/sa/error_701_chain_alias_undefined.pa");
	ASSERT_EQ(execTestCommand("palan-gen-ast ../test/testdata/sa/error_701_chain_alias_undefined.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":2:16: error: Undefined function 'nosuch'."), string::npos);
}

TEST(sa_error, ambiguous_call)
{
	cleanTestEnv();
	genModuleAst("../test/testdata/sa/lib_sa_import.pa");
	genModuleAst("../test/testdata/sa/lib_sa_import2.pa");
	string ast_out = moduleAstPath("../test/testdata/sa/error_055_ambiguous_call.pa");
	ASSERT_EQ(execTestCommand("palan-gen-ast ../test/testdata/sa/error_055_ambiguous_call.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("Ambiguous function call"), string::npos);

	ast_out = moduleAstPath("../test/testdata/sa/error_683_func_value_ambiguous_import.pa");
	ASSERT_EQ(execTestCommand("palan-gen-ast ../test/testdata/sa/error_683_func_value_ambiguous_import.pa -o " + ast_out), "");
	sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":3:27: error: Ambiguous function call 'square'"), string::npos) << sa;

	ast_out = moduleAstPath("../test/testdata/sa/error_684_func_ptr_import_func_call.pa");
	ASSERT_EQ(execTestCommand("palan-gen-ast ../test/testdata/sa/error_684_func_ptr_import_func_call.pa -o " + ast_out), "");
	sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":3:11: error: call to 'square' is ambiguous: it names both a function pointer and a function."), string::npos) << sa;
}

TEST(sa_error, unknown_alias)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand("palan-gen-ast ../test/testdata/sa/error_056_unknown_alias.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("Undefined variable or module alias 'X'"), string::npos);
}

TEST(sa_error, import_block_scope_out)
{
	cleanTestEnv();
	genModuleAst("../test/testdata/sa/lib_sa_import.pa");
	string ast_out = moduleAstPath("../test/testdata/sa/error_057_import_block_scope_out.pa");
	ASSERT_EQ(execTestCommand("palan-gen-ast ../test/testdata/sa/error_057_import_block_scope_out.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("Undefined function"), string::npos);
}

TEST(sa_error, float_logical_not)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_059_float_logical_not.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("Logical operator operand must be an integer type"), string::npos);
}

TEST(sa_error, alias_unknown_method)
{
	// lib.nonexistent() — alias "lib" exists but method not found
	// Covers: findImportFuncByAlias returning nullptr
	cleanTestEnv();
	genModuleAst("../test/testdata/sa/lib_sa_import.pa");
	string ast_out = moduleAstPath("../test/testdata/sa/error_075_alias_unknown_method.pa");
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_075_alias_unknown_method.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("Undefined function"), string::npos);
}

TEST(sa_error, const_not_literal)
{
	// const NOTCONST = x; where x is a runtime variable, not a compile-time literal
	// Covers: sa_const_decl E_ConstNotCompileTimeValue branch
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_090_const_not_literal.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("must be initialized with a compile-time constant"), string::npos);
}

TEST(sa_error, arg_narrowing)
{
	// A call argument used to get no type
	// check beyond ImplicitWiden -- an int64 argument to an int32 parameter
	// silently passed through and produced a bad `movq` operand-width mismatch
	// in the emitted assembly. Now diagnosed at the call site, same message as
	// a narrowing var-decl initializer.
	// Covers: convertCallArg, E_InvalidNarrowingConv
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_148_arg_narrowing.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("Implicit conversion from 'int64' to 'int32'"), string::npos);
}

TEST(sa_error, arith_op_not_numeric)
{
	// `p + 1` on a pointer operand used to
	// silently fall through to `promoted = leftType`, accepting pointer
	// arithmetic that Palan has no syntax or semantics for. usualArithConv
	// returns nullptr for a non-Prim operand, which sa_expr_arith now
	// diagnoses instead of silently accepting.
	// Covers: sa_expr_arith non-Prim operand, E_ArithOpNotNumeric
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_149_arith_op_not_numeric.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("Arithmetic operator operand must be a numeric type"), string::npos);
}

TEST(sa_error, arith_op_not_numeric_rhs)
{
	// Same check as arith_op_not_numeric above, but with the non-Prim operand
	// on the right (`1 + p`) instead of the left (`p + 1`) -- usualArithConv's
	// non-Prim guard checks both operands independently, so both orders need
	// a dedicated test.
	// Covers: sa_expr_arith non-Prim right operand, E_ArithOpNotNumeric
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_151_arith_op_not_numeric_rhs.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("Arithmetic operator operand must be a numeric type"), string::npos);
}

TEST(sa_error, assign_narrowing)
{
	// An assignment (`big -> x`) used to
	// silently insert a narrowing convert -- unlike a var-decl initializer,
	// which has always rejected this. Assignment/array-assign/return/
	// field-assign now share the initializer's strict rule.
	// Covers: convertForBinding via sa_assign_stmt, E_InvalidNarrowingConv
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_150_assign_narrowing.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("Implicit conversion from 'int64' to 'int32'"), string::npos);
}

TEST(sa_error, untyped_var_decl)
{
	// `v = 7;` is a declaration with the type omitted (reserved for future type
	// inference), not an assignment; gen-ast keeps parsing it, so SA must reject it.
	// Covers: sa_statements "not-impl" branch with untyped-var, E_VarTypeInferenceNotImpl
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_190_untyped_var_decl.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":1:1: error: variable 'v' is declared without a type"), string::npos);
}

TEST(sa_error, stmt_not_implemented)
{
	// Covers: sa_statements "not-impl" branch without untyped-var, E_StmtNotImplemented
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_191_stmt_not_implemented.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":1:1: error: this statement is not supported"), string::npos);
}

TEST(sa_error, int_literal_out_of_range)
{
	// Covers: checkIntLiteralRange for lit-int/lit-uint, via a var-decl
	// initializer and an arith operand typed from expectedType
	const pair<string, string> cases[] = {
		{"error_200_int_lit_range_int8.pa", ":1:10: error: Integer literal '300' is out of range for type 'int8'."},
		{"error_702_call_arg_lit_range.pa", ":2:5: error: Integer literal '300' is out of range for type 'int8'."},
		{"error_1359_hex_range.pa", ":1:10: error: Integer literal '511' is out of range for type 'int8'."},
		{"error_1360_hex_neg_range.pa", "'-255' is out of range for type 'int8'"},
		{"error_1364_arr_size_range_var.pa", "error: Integer literal '18446744073709551615' is out of range for type 'int64'."},
		{"error_1365_arr_size_range_uint.pa", "error: Integer literal '18446744073709551615' is out of range for type 'int64'."},
		{"error_1366_arr_size_range_hex.pa", "error: Integer literal '18446744073709551615' is out of range for type 'int64'."},
		{"error_1367_arr_size_range_field.pa", "error: Integer literal '18446744073709551615' is out of range for type 'int64'."},
		{"error_1368_arr_size_range_param.pa", "error: Integer literal '18446744073709551615' is out of range for type 'int64'."},
		{"error_1369_arr_size_range_sizeof.pa", "error: Integer literal '18446744073709551615' is out of range for type 'int64'."},
		{"error_1362_hex_const_mixed.pa", "'4278190335' is out of range for type 'int32'"},
		{"error_1363_hex_const_shift.pa", "'4278190080' is out of range for type 'int32'"},
		{"error_1361_hex_const_expr.pa", "'4278190335' is out of range for type 'int32'"},
		{"error_201_int_lit_range_uint32.pa", "'5000000000' is out of range for type 'uint32'"},
		{"error_202_int_lit_range_int64.pa", "'99999999999999999999' is out of range for type 'int64'"},
		{"error_203_int_lit_range_uint64.pa", "'18446744073709551616' is out of range for type 'uint64'"},
		{"error_204_int_lit_range_arith.pa", ":2:14: error: Integer literal '300' is out of range for type 'int8'."},
		{"error_205_int_lit_range_neg_unsigned.pa", "'-1' is out of range for type 'uint32'"},
		// An unsuffixed cinclude macro is an untyped literal, typed from its context.
		{"error_225_macro_untyped_range.pa", ":5:3: error: Integer literal '300' is out of range for type 'int8'."},
		{"error_247_int_lit_range_const.pa", ":2:10: error: Integer literal '300' is out of range for type 'int8'."},
		// A constant expression is folded first, so the sum is what must fit.
		{"error_593_int_lit_range_folded.pa", ":1:10: error: Integer literal '200' is out of range for type 'int8'."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, int_literal_typed_from_other_operand)
{
	// `300 + i` in an int8 initializer: 300 takes int64 from `i`, so the
	// error is the narrowing of the sum, not a range error on the literal.
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_206_int_lit_arith_other_operand.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":2:1: error: Implicit conversion from 'int64' to 'int8'"), string::npos);
	ASSERT_EQ(sa.find("out of range"), string::npos);
}

TEST(sa_error, bool_conversion)
{
	const pair<string, string> cases[] = {
		{"error_226_bool_lit_range.pa", ":1:10: error: Integer literal '2' is out of range for type 'bool'."},
		{"error_227_bool_from_int.pa", ":2:1: error: Implicit conversion from 'int32' to 'bool'"},
		{"error_228_bool_arg_from_int.pa", ":3:1: error: Implicit conversion from 'int8' to 'bool'"},
		{"error_229_bool_arith_result.pa", ":2:1: error: Implicit conversion from 'int32' to 'bool'"},
		{"error_230_syscall_bool_param.pa", "cannot pass through the Linux syscall ABI: 'bool'"},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, unknown_type_name)
{
	const pair<string, string> cases[] = {
		{"error_306_unknown_arr_elem.pa", ":1:1: error: unknown type 'Foo'."},
		{"error_307_unknown_ptr_slot_elem.pa", ":1:1: error: unknown type 'Foo'."},
		{"error_308_unknown_ptr_slot_unsized_elem.pa", ":1:1: error: unknown type 'Foo'."},
		{"error_309_unknown_arr2d_elem.pa", ":1:1: error: unknown type 'Foo'."},
		{"error_310_unknown_arr2d_embed_elem.pa", ":1:1: error: unknown type 'Foo'."},
		{"error_311_unknown_arr_borrow_elem.pa", ":1:1: error: unknown type 'Foo'."},
		{"error_312_unknown_arr_elem_via_alias.pa", ":2:1: error: unknown type 'Foo'."},
		{"error_314_unknown_unsized_param.pa", ":1:1: error: unknown type 'Foo'."},
		{"error_315_unknown_param.pa", ":1:1: error: unknown type 'Foo'."},
		{"error_316_unknown_arr_borrow_param.pa", ":1:1: error: unknown type 'Foo'."},
		{"error_317_unknown_named_ret.pa", ":1:1: error: unknown type 'Foo'."},
		{"error_331_unknown_ptr_field.pa", ":1:1: error: unknown type 'Foo'."},
		{"error_332_unknown_ptr_slot_field.pa", ":1:1: error: unknown type 'Foo'."},
		// A block's types are not visible from a sibling block.
		{"error_333_ptr_field_sibling_block_def.pa", ":2:2: error: unknown type 'B'."},
		// A declaration after another statement starts at its own first token.
		{"error_334_unknown_ptr_field_after_stmt.pa", ":2:1: error: unknown type 'Foo'."},
		{"error_335_unknown_param_after_stmt.pa", ":2:1: error: unknown type 'Foo'."},
		{"error_336_named_ret_init_narrowing.pa", ":1:30: error: Implicit conversion from 'int64' to 'int32' is not allowed"},
		{"error_337_named_ret_init_struct.pa", ":2:19: error: named return 'p' of a struct or array type cannot have an initializer."},
		{"error_338_tapple_assign_narrowing.pa", ":3:13: error: Implicit conversion from 'int64' to 'int32' is not allowed"},
		{"error_339_tapple_assign_count.pa", ":3:1: error: Variable count does not match return count of 'f'."},
		{"error_340_tapple_assign_single_ret.pa", ":3:1: error: Function 'g' does not have multiple return values."},
		{"error_341_tapple_decl_narrowing.pa", ":2:1: error: Implicit conversion from 'int64' to 'int32' is not allowed"},
		{"error_342_tapple_decl_struct_mismatch.pa", ":3:1: error: cannot convert 'P' to 'int32'."},
		{"error_343_tapple_decl_unknown_type.pa", ":2:1: error: unknown type 'Foo'."},
		{"error_344_transfer_non_owned_field.pa", ":3:1: error: '->>' needs a field that owns its value (T or [n]T); 'n' does not."},
		{"error_345_unsized_ret_copy.pa", ":3:1: error: cannot copy '[?]int32' into '[3]int32'"},
		{"error_346_ret_struct_borrow_decl.pa", ":3:1: error: a struct or array returned by a call has no owner to borrow from"},
		{"error_347_ret_struct_borrow_assign.pa", ":5:1: error: a struct or array returned by a call has no owner to borrow from"},
		{"error_348_ret_struct_borrow_slot.pa", ":4:1: error: a struct or array returned by a call has no owner to borrow from"},
		{"error_349_ret_struct_borrow_field.pa", ":5:1: error: a struct or array returned by a call has no owner to borrow from"},
		{"error_350_ret_struct_borrow_return.pa", ":3:18: error: a struct or array returned by a call has no owner to borrow from"},
		{"error_351_transfer_from_borrow_to_field.pa", ":4:1: error: '->>' needs a source that owns its value"},
		{"error_352_transfer_from_borrow_to_elem.pa", ":3:1: error: '->>' needs a source that owns its value"},
		{"error_353_transfer_from_param.pa", ":4:2: error: '->>' needs a source that owns its value"},
		{"error_354_transfer_from_ptr_field.pa", ":5:1: error: '->>' needs a source that owns its value"},
		{"error_1187_transfer_c_ret_to_elem.pa", ":3:1: error: '->>' needs a source that owns its value"},
		{"error_1188_transfer_c_ret_to_dict.pa", ":3:1: error: '->>' needs a source that owns its value"},
		{"error_1189_transfer_c_ret_to_field.pa", ":4:1: error: '->>' needs a source that owns its value"},
		{"error_1190_transfer_c_ret_to_var.pa", ":3:1: error: '->>' needs a source that owns its value"},
		{"error_1191_transfer_borrow_ret_to_elem.pa", ":4:1: error: '->>' needs a source that owns its value"},
		{"error_355_transfer_from_readonly_elem.pa", ":4:2: error: cannot write through read-only pointer"},
		{"error_356_transfer_to_ptr_slot.pa", ":3:1: error: '->>' cannot give ownership to a borrowed pointer slot"},
		{"error_357_transfer_to_arr_slot.pa", ":3:1: error: '->>' cannot give ownership to a borrowed pointer slot"},
		{"error_358_transfer_row_size_mismatch.pa", ":3:1: error: cannot transfer '[5]int32' into '[3]int32'"},
		{"error_361_struct_to_embed_struct_param.pa", ":4:1: error: cannot convert 'P' to '[?]$P'."},
		{"error_362_transfer_to_embed_ptr_slot.pa", ":3:1: error: '->>' cannot give ownership to a borrowed pointer slot"},
		{"error_363_unsized_rows_arg_mismatch.pa", ":3:1: error: array shape '[2][4]int32' does not match '[?][3]int32'"},
		{"error_364_unsized_rows_ret_mismatch.pa", ":2:2: error: array shape '[?][4]int32' does not match '[?][3]int32'"},
		{"error_365_unsized_rows_slot_mismatch.pa", ":3:1: error: array shape '[2][4]int32' does not match '[?][3]int32'"},
		{"error_366_unsized_rows_size_not_const.pa", ":2:1: error: the row size in '[][m]T' or '[+][m]T' must be a compile-time constant."},
		{"error_369_unsized_rows_1d_borrow.pa", ":3:1: error: array shape '@[3]int32' does not match '[?][3]int32'"},
		{"error_370_slot_arr_ret_as_owned_rows.pa", ":3:2: error: array shape '[?]@!int32' does not match '[?][?]int32'"},
		{"error_371_borrow_elems_arg_as_owned.pa", ":6:1: error: array shape '[1]@!P' does not match '[?]P'"},
		{"error_665_transfer_to_frame_var.pa", ":4:1: error: '->>' needs a variable that owns its array or struct; 'q' does not."},
		{"error_666_transfer_to_borrow_var.pa", ":4:1: error: '->>' needs a variable that owns its array or struct; 'b' does not."},
		{"error_667_transfer_to_borrow_param.pa", ":2:22: error: '->>' needs a variable that owns its array or struct; 'p' does not."},
		{"error_668_transfer_to_prim_var.pa", ":3:1: error: '->>' needs a variable that owns its array or struct; 'x' does not."},
		{"error_669_transfer_to_self.pa", ":2:1: error: '->>' cannot give 'a' to itself."},
		{"error_670_transfer_var_size_mismatch.pa", ":3:1: error: cannot transfer '[4]int32' into '[3]int32'"},
		// The variable frees by the size it was declared with.
		{"error_672_transfer_var_struct_mismatch.pa", ":5:1: error: cannot transfer 'P' into 'Q'"},
		{"error_673_transfer_frame_arr_to_var.pa", ":3:1: error: '->>' needs a source that owns its value"},
		{"error_372_owned_elems_arg_as_borrow.pa", ":5:1: error: array shape '[2]P' does not match '[?]@!P'"},
		{"error_373_slot_arr_into_owned_rows_slot.pa", ":3:1: error: array shape '[3]@!int32' does not match '[?][?]int32'"},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, pln_call_too_few_args)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_386_pln_call_too_few_args.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":2:1: error: function 'f' takes 2 argument(s), but 1 given."), string::npos);
}

TEST(sa_error, pln_call_too_many_args)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_387_pln_call_too_many_args.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":2:1: error: function 'f' takes 0 argument(s), but 1 given."), string::npos);
}

TEST(sa_error, alias_call_arg_count)
{
	cleanTestEnv();
	genModuleAst("../test/testdata/sa/lib_sa_import.pa");
	string ast_out = moduleAstPath("../test/testdata/sa/error_388_alias_call_arg_count.pa");
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_388_alias_call_arg_count.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":2:11: error: function 'square' takes 1 argument(s), but 2 given."), string::npos);
}

TEST(sa_error, enum_from_literal)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_389_enum_from_literal.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":3:1: error: Implicit conversion from 'int32' to 'Color' is not allowed; write an explicit cast 'Color(...)'."), string::npos);
}

TEST(sa_error, enum_from_int)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_390_enum_from_int.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":4:1: error: Implicit conversion from 'int32' to 'Color' is not allowed; write an explicit cast 'Color(...)'."), string::npos);
}

TEST(sa_error, enum_from_other_enum)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_391_enum_from_other_enum.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":3:1: error: Implicit conversion from 'Fruit' to 'Color' is not allowed; write an explicit cast 'Color(...)'."), string::npos);
}

TEST(sa_error, enum_to_narrower)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_392_enum_to_narrower.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":3:1: error: Implicit conversion from 'Color' to 'uint8' is not allowed; write an explicit cast 'uint8(...)'."), string::npos);
}

TEST(sa_error, enum_cast_from_float)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_393_enum_cast_from_float.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":3:11: error: Cannot cast type 'flo64' to 'Color'."), string::npos);
}

TEST(sa_error, unknown_enumerator)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_394_unknown_enumerator.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":3:11: error: enum 'Color' has no enumerator 'PURPLE'."), string::npos);
}

TEST(sa_error, duplicate_enumerator)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_395_duplicate_enumerator.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":3:18: error: enumerator 'A' is already defined in enum 'E'."), string::npos);
}

TEST(sa_error, duplicate_type_name)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_396_duplicate_type_name.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":3:1: error: type 'Color' is already defined."), string::npos);
}

TEST(sa_error, enum_value_not_const)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_397_enum_value_not_const.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":3:15: error: enumerator 'A' must be given an integer constant expression."), string::npos);
}

TEST(sa_error, enum_value_overflow)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_398_enum_value_overflow.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":3:40: error: value 9223372036854775808 of enumerator 'B' is out of the int64 range."), string::npos);
}

TEST(sa_error, enum_cast_arg_count)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_399_enum_cast_arg_count.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":3:11: error: cast 'Color(...)' takes exactly one argument."), string::npos);
}

TEST(sa_error, enum_param_from_literal)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_400_enum_param_from_literal.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":4:1: error: Implicit conversion from 'int32' to 'Color' is not allowed; write an explicit cast 'Color(...)'."), string::npos);
}

TEST(sa_error, enum_value_out_of_range)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_401_enum_value_out_of_range.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":3:15: error: value 99999999999999999999 of enumerator 'A' is out of the int64 range."), string::npos);
}

TEST(sa_error, enum_name_conflicts_struct)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_402_enum_name_conflicts_struct.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":3:1: error: type 'P' is already defined."), string::npos);
}

TEST(sa_error, type_name_conflict)
{
	const pair<string, string> cases[] = {
		{"error_526_struct_name_conflicts_c_struct.pa", ":1:1: error: type 'tm' is already defined."},
		{"error_527_struct_name_conflicts_c_enum.pa", ":1:1: error: type 'Color' is already defined."},
		{"error_528_alias_name_conflicts_c_typedef.pa", ":1:1: error: type 'time_t' is already defined."},
		{"error_529_struct_name_duplicate.pa", ":2:1: error: type 'P' is already defined."},
		{"error_530_alias_name_conflicts_struct.pa", ":2:1: error: type 'P' is already defined."},
		{"error_531_c_struct_name_conflict_nested.pa", ":3:2: error: type 'tm' is already defined."},
		{"error_532_c_typedef_name_conflict_nested.pa", ":3:2: error: type 'time_t' is already defined."},
		{"error_533_import_type_conflicts_c_struct.pa", ":1:1: error: type 'Vec' is already defined."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		genModuleAst("../test/testdata/sa/lib_sa_types2.pa");
		string ast_out = moduleAstPath("../test/testdata/sa/" + file);
		ASSERT_EQ(execTestCommand(
			"palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, enum_field_from_int)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_405_enum_field_from_int.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":4:1: error: Implicit conversion from 'int32' to 'Color' is not allowed"), string::npos);
}

TEST(sa_error, sizeof)
{
	const pair<string, string> cases[] = {
		{"error_406_sizeof_unknown_type.pa", ":1:12: error: unknown type 'Foo'."},
		{"error_407_sizeof_incomplete_struct.pa", ":2:12: error: struct 'DIR' is only forward-declared"},
		{"error_408_sizeof_unsized_arr.pa", ":1:12: error: every array size in the type of 'sizeof' must be a compile-time constant."},
		{"error_409_sizeof_arr_size_not_const.pa", ":2:12: error: every array size in the type of 'sizeof' must be a compile-time constant."},
		{"error_410_sizeof_embed_prim.pa", ":1:12: error: '$' applies only to a struct element or a '$[m]' row, not 'int32'."},
		{"error_411_sizeof_embed_ptr.pa", ":1:12: error: '$' applies only to a struct element or a '$[m]' row, not '@int32'."},
		{"error_573_sizeof_embed_unknown.pa", ":1:12: error: unknown type 'Foo'."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, expr_not_implemented)
{
	const pair<string, string> cases[] = {
		{"error_412_expr_not_impl_var_init.pa", ":1:11: error: this expression is not supported"},
		{"error_413_expr_not_impl_sizeof.pa", ":1:12: error: this expression is not supported"},
		{"error_414_expr_not_impl_nested_arg.pa", ":2:16: error: this expression is not supported"},
		{"error_415_expr_not_impl_return.pa", ":3:9: error: this expression is not supported"},
		{"error_416_expr_not_impl_assign.pa", ":2:1: error: this expression is not supported"},
		{"error_417_expr_not_impl_stmt.pa", ":1:1: error: this expression is not supported"},
		{"error_523_expr_not_impl_sizeof_nested.pa", ":1:12: error: this expression is not supported"},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

// None of these functions is called: the signature alone must be rejected.
TEST(sa_error, sig_not_implemented)
{
	const string untyped = "is declared without a type; type inference is not supported yet";
	const pair<string, string> cases[] = {
		{"error_544_param_untyped_default.pa", ":1:8: error: variable 'x' " + untyped},
		{"error_547_param_undeclarable.pa", ":1:8: error: this parameter is not supported in this version."},
		{"error_549_syscall_param_default.pa", ":1:11: error: variable 'x' " + untyped},
		{"error_550_inner_func_param.pa", ":2:13: error: this parameter is not supported in this version."},
		{"error_552_ret_untyped.pa", ":1:13: error: this return value is not supported in this version."},
		{"error_941_take_undeclarable.pa", ":2:1: error: this statement is not supported in this version."},
		{"error_942_transfer_to_expr.pa", ":3:1: error: this expression is not supported in this version."},
		{"error_944_unknown_field_overload_arg.pa", ":5:3: error: struct 'P' has no field 'nofield'."},
		{"error_555_import_not_impl_sig.pa", "lib_sa_not_impl_sig.pa:1:15: error: this parameter is not supported in this version."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		genModuleAst("../test/testdata/sa/lib_sa_not_impl_sig.pa");
		string ast_out = moduleAstPath("../test/testdata/sa/" + file);
		ASSERT_EQ(execTestCommand(
			"palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, method_call)
{
	const string notMethod = "cannot be called as a method: its first parameter must be a writable borrow ('@!').";
	const pair<string, string> cases[] = {
		{"error_418_method_ro_first_param.pa", ":4:1: error: 'f' " + notMethod},
		{"error_419_method_value_first_param.pa", ":3:1: error: 'f' " + notMethod},
		{"error_420_method_no_param.pa", ":4:1: error: 'f' " + notMethod},
		{"error_421_method_c_const_first_param.pa", ":3:11: error: 'strlen' " + notMethod},
		{"error_422_method_recv_not_addressable.pa", ":4:1: error: cannot take the address of this expression"},
		{"error_423_method_recv_ro_borrow.pa", ":4:2: error: cannot bind a read-only pointer '@T' to a mutable pointer '@!T'."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, import_type)
{
	const pair<string, string> cases[] = {
		{"error_424_import_type_unqualified.pa", ":2:1: error: type 'Vec' requires a module alias qualifier: 'V.Vec'."},
		{"error_425_import_enum_unqualified.pa", ":2:17: error: type 'Color' requires a module alias qualifier: 'V.Color'."},
		{"error_426_import_type_not_selected.pa", ":2:1: error: unknown type 'Vec'."},
		{"error_427_import_type_private_dep.pa", ":2:1: error: unknown type 'Inner'."},
		{"error_428_import_type_local_conflict.pa", ":2:1: error: type 'Vec' is already defined."},
		// The imported module's own path and line, not the importer's.
		{"error_430_import_bad_struct_def.pa", "lib_sa_bad_struct.pa:2:1: error: unsupported struct field type."},
		// A block's import makes its types writable only in that block.
		{"error_568_block_import_type_outside.pa", ":4:1: error: unknown type 'Vec'."},
		{"error_569_block_import_alias_outside.pa", ":4:1: error: unknown type 'MyInt'."},
		{"error_570_block_import_enum_outside.pa", ":4:11: error: Undefined variable 'Color'."},
		{"error_571_block_import_qualified_outside.pa", ":4:1: error: unknown type 'V.Vec'."},
		// PP is lib's P, a different type from main's P of the same name.
		{"error_641_import_alias_private_type.pa", ":4:1: error: cannot convert '@!P' to '@!P'."},
		// An import making a name writable still clashes with a local type of that name.
		{"error_642_block_local_then_import.pa", ":3:2: error: type 'Vec' is already defined."},
		{"error_643_top_local_block_import.pa", ":3:2: error: type 'Vec' is already defined."},
		{"error_644_top_alias_block_import.pa", ":3:2: error: type 'MyInt' is already defined."},
		{"error_645_hidden_after_block_local.pa", ":6:1: error: type 'Vec' requires a module alias qualifier: 'V.Vec'."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		for (string lib : {"lib_sa_types", "lib_sa_types2", "lib_sa_bad_struct", "lib_sa_private_alias"})
			genModuleAst("../test/testdata/sa/" + lib + ".pa");
		string ast_out = moduleAstPath("../test/testdata/sa/" + file);
		ASSERT_EQ(execTestCommand(
			"palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, circular_type_decl)
{
	const pair<string, string> cases[] = {
		// Reported where the cycle closes: A, reached from the import of B.
		{"error_433_import_mutual_embed.pa", "error_433_import_mutual_embed.pa:2:1: error: type 'B' depends on itself: B -> A -> B."},
		{"error_434_owned_cycle.pa", ":2:1: error: type 'P' depends on itself: P -> Q -> P."},
		{"error_1370_embed_varr_cycle.pa", ":1:1: error: type 'A' depends on itself: A -> A."},
		{"error_1371_fixed_arr_cycle.pa", ":1:21: error: type 'A' depends on itself: A -> B -> A."},
		{"error_1372_init_elem_cycle.pa", ":1:1: error: type 'A' depends on itself: A -> A."},
		{"error_1373_arr_borrow_cycle.pa", ":1:1: error: type 'A' depends on itself: A -> A."},
		{"error_435_alias_cycle.pa", ":2:1: error: type 'X' depends on itself: X -> Y -> X."},
		{"error_879_top_const_var_cycle.pa", ":1:1: error: type 'N' depends on itself: N -> a -> N."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		for (string f : {"lib_sa_mutual_cycle", "error_433_import_mutual_embed"})
			genModuleAst("../test/testdata/sa/" + f + ".pa");
		string ast_out = moduleAstPath("../test/testdata/sa/" + file);
		ASSERT_EQ(execTestCommand(
			"palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, alias_cast)
{
	const pair<string, string> cases[] = {
		{"error_436_cast_alias_struct.pa", ":3:11: error: cannot cast to 'PT': only a pointer ('@!X') can be viewed as an owner"},
		{"error_437_cast_alias_array.pa", ":2:11: error: cannot cast to 'R': only a pointer ('@!X') can be viewed as an owner"},
		{"error_438_cast_opaque.pa", ":2:11: error: cannot cast to 'FILE': only a pointer ('@!X') can be viewed as an owner"},
		{"error_439_cast_alias_arg_count.pa", ":2:8: error: cast 'U8(...)' takes exactly one argument."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, as_cast)
{
	const pair<string, string> cases[] = {
		{"error_579_cast_owned_arr.pa", ":2:14: error: cannot cast to 'array': only a pointer ('@!X') can be viewed as an owner"},
		{"error_938_cast_unsized_arr.pa", ":2:12: error: cannot cast to 'array': only a primitive"},
		{"error_939_cast_embed.pa", ":3:8: error: cannot cast to 'embed': only a primitive"},
		{"error_580_cast_ptr_to_narrow_int.pa", ":3:11: error: Cannot cast type '@!int32' to 'int32'."},
		{"error_581_cast_int_to_ptr.pa", ":2:13: error: Cannot cast type 'int32' to '@!int32'."},
		{"error_582_cast_int_to_borrowed_arr.pa", ":2:16: error: Cannot cast type 'uint64' to '@![4]int32'."},
		{"error_583_cast_struct_value.pa", ":3:7: error: cannot cast to 'P': only a pointer ('@!X') can be viewed as an owner"},
		{"error_584_cast_struct_to_ptr.pa", ":3:13: error: Cannot cast type 'P' to '@!uint8'."},
		{"error_585_cast_enum_to_ptr.pa", ":3:13: error: Cannot cast type 'Big' to '@!int32'."},
		{"error_586_cast_ptr_to_enum.pa", ":3:9: error: Cannot cast type '@!int32' to 'Big'."},
		{"error_688_cast_ptr_to_float.pa", ":3:11: error: Cannot cast type '@int32' to 'flo64'."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, block_decl_scope)
{
	const pair<string, string> cases[] = {
		{"error_556_block_type_outside.pa", ":4:1: error: unknown type 'Foo'."},
		{"error_557_block_const_outside.pa", ":4:11: error: Undefined variable 'N'."},
		{"error_558_block_alias_outside.pa", ":4:1: error: unknown type 'I'."},
		{"error_559_block_enum_outside.pa", ":4:11: error: Undefined variable 'E'."},
		{"error_560_block_type_shadow_top.pa", ":3:2: error: type 'Foo' is already defined."},
		{"error_561_block_type_shadow_c.pa", ":3:2: error: type 'FILE' is already defined."},
		{"error_562_block_type_shadow_outer_block.pa", ":4:3: error: type 'Foo' is already defined."},
		{"error_563_block_const_shadow.pa", ":4:3: error: Constant 'N' is already defined."},
		{"error_564_block_dup_decl.pa", ":3:2: error: Constant 'Foo' is already defined."},
		// A C type a block's cinclude brings in clashes with the block's own.
		{"error_565_block_type_then_cinclude.pa", ":3:2: error: type 'tm' is already defined."},
		{"error_696_prim_name_type.pa", ":1:1: error: type 'int32' is already defined."},
		{"error_697_block_prim_name_type.pa", ":2:2: error: type 'int32' is already defined."},
		{"error_698_alias_then_cinclude.pa", ":3:2: error: type 'A' is already defined."},
		{"error_699_alias_then_cinclude_typedef.pa", ":3:2: error: type 'B' is already defined."},
		{"error_566_block_type_display_name.pa", ":4:2: error: cannot convert 'Foo' to 'int32'."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, const_name_dup)
{
	const pair<string, string> cases[] = {
		{"error_630_top_const_dup.pa", ":2:1: error: Constant 'N' is already defined."},
		{"error_631_top_type_then_const.pa", ":2:1: error: Constant 'M' is already defined."},
		{"error_632_top_const_then_alias.pa", ":2:1: error: type 'M' is already defined."},
		{"error_633_top_alias_dup.pa", ":2:1: error: type 'A' is already defined."},
		{"error_634_top_var_then_const.pa", ":2:1: error: Constant 'K' is already defined."},
		{"error_635_top_const_then_var.pa", ":2:1: error: Variable 'K' is already declared."},
		{"error_636_top_const_param.pa", ":2:1: error: Variable 'N' is already declared."},
		{"error_637_block_const_outer_var.pa", ":3:2: error: Constant 'K' is already defined."},
		{"error_638_block_var_then_const.pa", ":3:2: error: Constant 'K' is already defined."},
		{"error_639_top_const_c_type.pa", ":2:1: error: Constant 'time_t' is already defined."},
		{"error_640_block_tapple_then_const.pa", ":6:2: error: Constant 'K' is already defined."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, named_ret_single_return_value)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_449_named_ret_single_return_value.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":2:2: error: Function with named return values requires bare 'return;' statement."), string::npos) << sa;
}

TEST(sa_error, named_ret_lit_return_value)
{
	const char* files[] = {
		"error_1195_named_ret_varr_lit.pa",
		"error_1196_named_ret_dict_lit.pa",
		"error_1197_named_ret_empty_dict.pa",
		"error_1198_named_ret_fixed_arr_lit.pa",
	};
	for (auto file : files) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			string("palan-gen-ast ../test/testdata/sa/") + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(":3:2: error: Function with named return values requires bare 'return;' statement."), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, lit_arg)
{
	const pair<string, string> cases[] = {
		{"error_495_lit_arg_needs_at.pa", ":3:1: error: a struct given to a '@T'/'@!T' pointer must be written as '@name' or '@!name' ('@f()' or '@!f()' for a call returning it, '@{..}', '@!{..}', '@[..]' or '@![..]' for a literal)."},
		{"error_496_arr_lit_arg_needs_at.pa", ":2:1: error: an array given to a borrowed array ('@[n]T'/'@![n]T') or a '@T'/'@!T' pointer must be written as '@name' or '@!name' ('@[..]' or '@![..]' for a literal)."},
		{"error_497_lit_arg_count.pa", ":2:5: error: array 'a' has size 3 but its array literal has 2 elements."},
		{"error_498_lit_arg_field_count.pa", ":3:5: error: struct 'P' has 2 fields but its literal has 1 values."},
		{"error_499_dict_lit_arr_arg.pa", ":2:5: error: parameter 'a' of type '@[3]int32' cannot take a '{name: value}' literal: only a struct or dictionary parameter can."},
		{"error_500_lit_arg_c_func.pa", ":2:10: error: a literal cannot be passed to C function 'memset': only a by-value struct parameter takes one."},
		{"error_501_lit_arg_unsized.pa", ":2:5: error: parameter 'a' of type '[?]int32' cannot take an array literal: only a struct or fixed-size array parameter can."},
		{"error_502_lit_arg_ptr_slot.pa", ":4:5: error: array 'a' cannot be given an array literal: only numeric and struct element types are supported."},
		{"error_503_lit_borrow_not_arg.pa", ":3:9: error: an array literal can only be used as"},
		{"error_504_lit_arg_borrow_escapes.pa", ":4:1: error: a borrow returned by a call given a temporary struct may point to that struct"},
		{"error_505_lit_arg_read_only.pa", ":4:1: error: cannot bind a read-only pointer '@T' to a mutable pointer '@!T'."},
		{"error_614_arr_lit_scalar_param.pa", ":2:4: error: parameter 'a' of type 'int32' cannot take an array literal: only a struct or fixed-size array parameter can."},
		{"error_615_arr_lit_ptr_param.pa", ":2:6: error: parameter 'p' of type '@!int32' cannot take an array literal: only a struct or fixed-size array parameter can."},
		{"error_616_lit_arg_c_variadic.pa", ":2:16: error: a literal cannot be passed to C function 'printf': only a by-value struct parameter takes one."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, assign_in_expr)
{
	const string msg = "error: an assignment cannot be used inside an expression";
	const pair<string, string> cases[] = {
		{"error_506_assign_in_init.pa", ":3:11: " + msg},
		{"error_507_assign_in_arg.pa", ":3:3: " + msg},
		{"error_508_assign_in_cond.pa", ":3:5: " + msg},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, void_call_as_value)
{
	const pair<string, string> cases[] = {
		{"error_509_void_call_arg.pa", ":3:13: error: Void function call cannot be used as a value."},
		{"error_510_void_call_variadic_arg.pa", ":3:16: error: Void function call cannot be used as a value."},
		{"error_654_void_value_call_target.pa", ":4:1: error: Void function call cannot be used as a value."},
		{"error_656_void_index_store.pa", ":3:10: error: Void function call cannot be used as a value."},
		{"error_657_void_index_load.pa", ":4:5: error: Void function call cannot be used as a value."},
		{"error_658_void_struct_elem_copy.pa", ":4:1: error: Void function call cannot be used as a value."},
		{"error_659_void_operand.pa", ":2:11: error: Void function call cannot be used as a value."},
		{"error_660_void_logical_not.pa", ":2:12: error: Void function call cannot be used as a value."},
		{"error_661_void_cond.pa", ":2:4: error: Void function call cannot be used as a value."},
		{"error_662_void_return.pa", ":3:9: error: Void function call cannot be used as a value."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, chain_call)
{
	const pair<string, string> cases[] = {
		{"error_511_chain_type.pa", ":4:6: error: cannot convert 'V' to 'int32'."},
		{"error_512_chain_too_many.pa", ":3:6: error: function 'add' takes 2 argument(s), but 3 given."},
		{"error_513_chain_too_few.pa", ":3:17: error: function 'show' takes 3 argument(s), but 2 given."},
		{"error_514_chain_mut_needs_at.pa", ":4:6: error: a struct given to a '@T'/'@!T' pointer must be written as '@name' or '@!name'"},
		{"error_515_chain_multi_to_mut.pa", ":3:17: error: cannot convert 'int32' to '@!int32'."},
		{"error_516_chain_void_value.pa", ":3:1: error: Void function call cannot be used as a value."},
		{"error_518_chain_method_ro_recv.pa", ":4:7: error: cannot bind a read-only pointer '@T' to a mutable pointer '@!T'."},
		{"error_519_chain_multi_to_tapple.pa", ":3:1: error: the return values of a '->' chain whose value is evaluated first"},
		{"error_520_chain_multi_borrow_escapes.pa", ":4:1: error: a borrow returned by a call given a temporary struct may point to that struct"},
		{"error_719_chain_method_not_mutable.pa", ":5:6: error: 'getx' cannot be called as a method: its first parameter must be a writable borrow ('@!')."},
		{"error_720_chain_method_too_many.pa", ":4:6: error: function 'set' takes 2 argument(s), but 3 given."},
		{"error_721_chain_method_type.pa", ":4:6: error: cannot convert 'V' to 'int32'."},
		{"error_722_chain_method_ahead_to_tapple.pa", ":5:1: error: the return values of a '->' chain whose value is evaluated first"},
		{"error_723_chain_method_borrow_escapes.pa", ":6:10: error: a borrow returned by a call given a temporary struct may point to that struct"},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, const_expr)
{
	const string overflow = "error: constant expression overflows its 64-bit range.";
	const string divZero = "error: division by zero in a constant expression.";
	const pair<string, string> cases[] = {
		{"error_587_const_expr_overflow.pa", ":1:11: " + overflow},
		{"error_588_const_expr_uint_overflow.pa", ":1:11: " + overflow},
		{"error_589_const_expr_div_zero.pa", ":1:11: " + divZero},
		{"error_590_const_expr_flo_div_zero.pa", ":1:11: " + divZero},
		{"error_591_const_expr_flo_bitop.pa", ":1:11: error: Bitwise operator operand must be an integer type."},
		{"error_592_const_expr_flo_mod.pa", ":1:11: error: '%' operator is not supported for float types."},
		{"error_597_shift_float.pa", ":2:11: error: Bitwise operator operand must be an integer type."},
		{"error_598_const_shift_float.pa", ":1:11: error: Bitwise operator operand must be an integer type."},
		{"error_674_const_expr_flo_bitnot.pa", ":1:11: error: Bitwise operator operand must be an integer type."},
		{"error_675_const_expr_uint_neg.pa", ":1:11: " + overflow},
		{"error_676_const_expr_uint_add_overflow.pa", ":1:11: " + overflow},
		{"error_677_const_expr_uint_mul_overflow.pa", ":1:11: " + overflow},
		{"error_678_const_expr_uint_neg_operand.pa", ":1:11: " + overflow},
		{"error_679_const_expr_int_min_div.pa", ":1:11: " + overflow},
		{"error_680_const_expr_flo_overflow.pa", ":1:11: " + overflow},
		{"error_681_const_expr_mul_overflow.pa", ":1:11: " + overflow},
		{"error_682_const_expr_sub_overflow.pa", ":1:11: " + overflow},
		// A literal past 64 bits is caught while folding, before its context types it.
		{"error_695_const_expr_lit_overflow.pa", ":1:11: " + overflow},
		{"error_689_const_expr_uint_mod_zero.pa", ":1:11: " + divZero},
		{"error_690_const_expr_str_operand.pa", ":2:11: error: Arithmetic operator operand must be a numeric type."},
		{"error_691_shift_flo_count.pa", ":2:11: error: Bitwise operator operand must be an integer type."},
		// A shift computes in int32 for a bool target, which then rejects the narrowing.
		{"error_692_shift_bool_target.pa", ":2:1: error: Implicit conversion from 'int32' to 'bool' is not allowed"},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, overload)
{
	const pair<string, string> cases[] = {
		// Both fit only by widening, and neither matches exactly.
		{"error_703_overload_ambiguous.pa", ":4:1: error: Ambiguous function call 'f': it can mean f(int32), f(int64)."},
		{"error_825_overload_narrowed_ambiguous.pa", ":5:1: error: Ambiguous function call 'f': it can mean f(@int32, @!int32), f(@!int32, @int32)."},
		// A literal matches exactly only its default type, int64.
		{"error_704_overload_literal_ambiguous.pa", ":3:1: error: Ambiguous function call 'g': it can mean g(int32), g(flo32)."},
		{"error_705_overload_no_match.pa", ":5:1: error: No function 'f' takes (@P); there are f(int32), f(int64)."},
		// The return type does not tell overloads apart.
		{"error_706_overload_same_params.pa", ":3:1: error: Function 'f' is already defined."},
		{"error_707_overload_method_no_mutable.pa", ":5:1: error: 'm' cannot be called as a method: its first parameter must be a writable borrow ('@!')."},
		{"error_708_overload_func_name_type.pa", ":3:25: error: 'f' names f(int32), f(int64); it must be given where a function pointer type chooses one."},
		// The receivers are declared before the arguments choose a function.
		{"error_709_overload_multi_ret_differ.pa", ":4:22: error: Ambiguous function call 'dm': it can mean dm(int32), dm(int64)."},
		// An argument that fits no function is shown as what it would match.
		{"error_710_overload_no_match_literal.pa", ":3:1: error: No function 'pp' takes (int64); there are pp(@int32), pp(@int64)."},
		{"error_711_overload_no_match_func_name.pa", ":5:1: error: No function 'f' takes (function); there are f(int32), f(int64)."},
		{"error_712_overload_no_match_chained.pa", ":5:6: error: No function 'f' takes (P); there are f(int32), f(int64)."},
		{"error_713_overload_no_match_variadic.pa", ":5:1: error: No function 'printf' takes (@P); there are printf(@int8, ...), printf(int32)."},
		{"error_714_overload_func_name_none.pa", ":3:27: error: 'dm' names dm(int32), dm(int64); it must be given where a function pointer type chooses one."},
		{"error_715_overload_multi_ret_none.pa", ":3:1: error: Function 'f' does not have multiple return values."},
		{"error_716_overload_no_match_chained_literal.pa", ":3:6: error: No function 'pp' takes (int64); there are pp(@int32), pp(@int64)."},
		{"error_717_overload_no_match_aggregate.pa", ":3:1: error: No function 'f' takes (literal); there are f(int32), f(int64)."},
		{"error_718_overload_no_match_no_args.pa", ":3:1: error: No function 'printf' takes (); there are printf(@int8, ...), printf(int32)."},
		// A parameter is shown as written: '@[]T' is the same type as '@T'.
		{"error_1046_overload_no_match_borrowed_array.pa", ":5:1: error: No function 'f' takes (@!T, T); there are f(@!T, @[]uint8), f(@!T, @[4]int32, [...]@[]uint8)."},
		{"error_1048_overload_defaults_shown.pa", ":4:13: error: No function 'f' takes (@[2]int32); there are f(int32, bool = true, @[]uint8 = \"x\", Color = Color.GREEN, int32 = 3), f(@[]uint8, bool = false, Color = Color.RED)."},
		{"error_1049_arg_count_with_defaults.pa", ":2:1: error: function 'f' takes 1 to 2 argument(s), but 3 given."},
		{"error_1053_wide_char_int8.pa", ":1:10: error: character literal U+00E9 needs an integer type of 32 bits or more, not 'int8': no byte of UTF-8 text holds it."},
		{"error_1054_wide_char_uint16.pa", ":1:12: error: character literal U+00E9 needs an integer type of 32 bits or more, not 'uint16': no byte of UTF-8 text holds it."},
		{"error_1055_wide_char_bool.pa", ":1:10: error: character literal U+00E9 needs an integer type of 32 bits or more, not 'bool': no byte of UTF-8 text holds it."},
		{"error_1056_variadic_too_few.pa", ":2:1: error: function 'f' takes at least 1 argument(s), but 0 given."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, implicit_args)
{
	const pair<string, string> cases[] = {
		{"error_833_args_redecl.pa", ":1:1: error: Variable 'args' is already declared."},
		{"error_834_args_redecl_block.pa", ":2:2: error: Variable 'args' is already declared."},
		{"error_835_args_const.pa", ":1:1: error: Constant 'args' is already defined."},
		{"error_836_args_addr_of.pa", ":1:15: error: 'args' is already a borrow ('@T'/'@[n]T'/'@[+]T'); pass it by name."},
		{"error_837_args_resize.pa", ":1:1: error: cannot bind a read-only pointer '@T' to a mutable pointer '@!T'."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, for_loop)
{
	const pair<string, string> cases[] = {
		{"error_838_for_not_array.pa", ":1:28: error: 'for' takes an array, a dictionary, or a struct whose type has 'iter' and 'next', to iterate over."},
		{"error_839_for_index_assign.pa", ":1:34: error: cannot assign to 'i': the index or key of a 'for' loop is read-only."},
		{"error_840_for_index_addr_of.pa", ":1:46: error: cannot assign to 'i': the index or key of a 'for' loop is read-only."},
		{"error_841_for_unsized.pa", ":1:35: error: 'len' cannot tell this array's length: an array declared with its size, or a variable-length array, keeps one, but a '[]T' does not."},
		{"error_843_for_ro_write.pa", ":1:27: error: cannot bind a read-only pointer '@T' to a mutable pointer '@!T'."},
		{"error_845_for_dup_name.pa", ":1:17: error: Variable 'a' is already declared."},
		{"error_846_for_push.pa", ":1:37: error: cannot change what a 'for' loop iterates over ('v')."},
		{"error_847_for_copy_varr.pa", ":1:49: error: cannot change what a 'for' loop iterates over ('v')."},
		{"error_848_for_transfer_fixed.pa", ":1:43: error: cannot change what a 'for' loop iterates over ('c')."},
		{"error_849_for_transfer_source.pa", ":1:49: error: cannot change what a 'for' loop iterates over ('v')."},
		{"error_850_for_container_borrow.pa", ":3:30: error: cannot change what a 'for' loop iterates over ('s.arr')."},
		{"error_851_for_borrow_arg.pa", ":2:42: error: cannot change what a 'for' loop iterates over ('p')."},
		{"error_852_for_borrow_decl.pa", ":1:42: error: cannot change what a 'for' loop iterates over ('p')."},
		{"error_853_for_var_index.pa", ":1:52: error: cannot change what a 'for' loop iterates over ('vv[0]')."},
		{"error_1086_for_same_const_index.pa", ":2:39: error: cannot change what a 'for' loop iterates over ('vv[0]')."},
		{"error_854_for_field_transfer.pa", ":2:33: error: cannot change what a 'for' loop iterates over ('s.arr')."},
		{"error_855_for_elem_copy.pa", ":1:51: error: cannot change what a 'for' loop iterates over ('vv[0]')."},
		// '@a' is the element itself, written through a read-only view of the range.
		{"error_869_for_ro_elem_write.pa", ":1:33: error: cannot write through read-only pointer '@T'"},
		{"error_870_for_ro_elem_write_owned.pa", ":2:18: error: cannot write through read-only pointer '@T'"},
		{"error_871_for_elem_struct_by_name.pa", ":4:17: error: a struct given to a '@T'/'@!T' pointer must be written as '@name' or '@!name'"},
		// A read-only '@row' makes the row's elements read-only too.
		{"error_872_for_ro_row_inner_for.pa", ":2:21: error: cannot bind a read-only pointer '@T' to a mutable pointer '@!T'"},
		{"error_873_for_ro_row_write.pa", ":2:17: error: cannot write through read-only pointer '@T'"},
		{"error_874_for_ro_row_write_borrow.pa", ":1:43: error: cannot write through read-only pointer '@T'"},
		{"error_875_for_index_not_integer.pa", ":2:1: error: the index of a 'for' loop must be of an integer type, not 'flo64'."},
		{"error_876_for_index_too_narrow.pa", ":2:1: error: the index type 'int8' cannot hold every index of an array of length 129."},
		{"error_877_for_typed_index_assign.pa", ":2:28: error: cannot assign to 'i': the index or key of a 'for' loop is read-only."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, ro_ptr_slot)
{
	const pair<string, string> cases[] = {
		{"error_856_ro_ptr_slot_write.pa", ":4:1: error: cannot write through read-only pointer '@T'"},
		{"error_857_ro_ptr_slot_row_write.pa", ":4:1: error: cannot write through read-only pointer '@T'"},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, unqualified_enumerator)
{
	const pair<string, string> cases[] = {
		// The enum's own name, not an alias of it, even one sorting first.
		{"error_865_unqualified_enumerator.pa", ":1:11: error: Undefined variable 'Q'; write 'A.Q', 'B.Q' or 'C.Q' for the enumerator."},
		{"error_866_unqualified_c_enumerator.pa", ":2:11: error: Undefined variable 'MODE_X'; write 'Mode.MODE_X' for the enumerator."},
		{"error_867_unqualified_import_enumerator.pa", ":2:11: error: Undefined variable 'Red'; write 'V.Color.Red' for the enumerator."},
		// Color is brought along by Vec only, so source can't write it here.
		{"error_868_unqualified_hidden_enumerator.pa", ":2:11: error: Undefined variable 'Red'.\n"},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		genModuleAst("../test/testdata/sa/lib_sa_types.pa");
		string ast_out = moduleAstPath("../test/testdata/sa/" + file);
		ASSERT_EQ(execTestCommand(
			"palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, for_struct)
{
	const string iterShape = "error: 'for' over 'bag' needs 'func iter(@bag s) -> I' or 'func iter(@!bag s) -> I', "
	                         "I a struct type, and 'func next(@!I it) -> bool ok, E'.";
	const string nextShape = "error: 'for' needs 'func next(@!bagit it) -> bool ok, E' for the iterator 'bagit'.";
	const string change = "error: cannot change what a 'for' loop iterates over ";
	const pair<string, string> cases[] = {
		{"error_1016_for_no_iter.pa", ":4:15: " + iterShape},
		{"error_1017_for_iter_both.pa", ":6:15: error: 'for' over 'bag' finds both 'iter(@bag)' and 'iter(@!bag)'; a type has one of them."},
		{"error_1018_for_no_next.pa", ":5:15: " + nextShape},
		{"error_1019_for_next_shape.pa", ":6:15: " + nextShape},
		{"error_1020_for_iter_prim.pa", ":5:15: " + iterShape},
		{"error_1021_for_elem_value_borrow.pa", ":6:5: error: '@c' needs 'next' to return the element as a borrow '@E' or '@!E', but it returns 'int32'."},
		{"error_1022_for_elem_ro_mut.pa", ":6:5: error: '@!c' needs 'next' to return the element as a borrow '@!E', but it returns '@int32'."},
		{"error_1023_for_iter_mut_readonly.pa", ":8:18: error: cannot bind a read-only pointer '@T' to a mutable pointer '@!T'."},
		{"error_1024_for_struct_borrow_arg.pa", ":9:23: " + change + "('g')."},
		{"error_1025_for_struct_method.pa", ":9:19: " + change + "('g')."},
		{"error_1026_for_struct_append.pa", ":9:19: " + change + "('g')."},
		{"error_1027_for_struct_copy_into.pa", ":10:19: " + change + "('g')."},
		{"error_1028_for_struct_move_out.pa", ":10:19: " + change + "('g')."},
		{"error_1029_for_struct_field_range.pa", ":11:22: " + change + "('x.in')."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, const_used_before_known)
{
	const pair<string, string> cases[] = {
		{"error_878_top_const_before_var.pa", ":3:10: error: const 'N' cannot be used here: its value uses 'a', which is not a top-level variable declared before the const."},
		{"error_880_top_const_cglobal.pa", ":3:11: error: const 'N' cannot be used here: its value uses 'stdin', which is not a top-level variable declared before the const."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, global_var)
{
	const string notWritable = "a global variable is written only where a 'global' statement permits it";
	const pair<string, string> cases[] = {
		{"error_881_global_in_func.pa", ":2:2: error: 'global' is allowed only at the top level of a module."},
		{"error_882_global_in_block.pa", ":2:2: error: 'global' is allowed only at the top level of a module."},
		{"error_937_permit_in_block.pa", ":2:3: error: 'global' is allowed only at the top level of a module."},
		{"error_883_permit_not_global.pa", ":2:1: error: 'x' is not a global variable: declare it with 'global', import it, or cinclude the C global first."},
		{"error_884_permit_not_func.pa", ":1:24: error: 'y' is not a function at the top level of this module."},
		{"error_885_permit_duplicate.pa", ":2:14: error: 'f' is already permitted to use 'x'."},
		{"error_889_global_read_only.pa", ":3:2: error: cannot write to 'x': " + notWritable},
		{"error_890_global_replace.pa", ":3:2: error: cannot assign to 'a': a function writes the elements or fields of an array or struct global variable, but does not replace it."},
		{"error_891_global_read_only_elem.pa", ":3:2: error: cannot write through read-only pointer"},
		{"error_892_c_global_read_only_func.pa", ":4:2: error: cannot write to 'stderr': " + notWritable},
		{"error_894_global_param_clash.pa", ":2:1: error: Variable 'x' is already declared."},
		{"error_896_global_addr_read_only.pa", ":3:14: error: cannot write to 'x': " + notWritable},
		{"error_1069_global_init_top_var.pa", ":2:18: error: the initializer of global 'x' cannot use top-level variable 'n': globals are initialized before the top level runs."},
		{"error_1070_global_init_args.pa", ":1:18: error: the initializer of global 'n' cannot use top-level variable 'args': globals are initialized before the top level runs."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = moduleAstPath("../test/testdata/sa/" + file);
		ASSERT_EQ(execTestCommand(
			"palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, import_global)
{
	const string notWritable = "a global variable is written only where a 'global' statement permits it";
	const pair<string, string> cases[] = {
		{"error_1071_import_global_not_permitted.pa", ":2:28: error: Undefined variable 'count'."},
		{"error_1072_import_global_read_only.pa", ":3:12: error: cannot write to 'count': " + notWritable},
		{"error_1073_import_global_alias_read_only.pa", ":3:12: error: cannot write to 'G.count': " + notWritable},
		{"error_1074_permit_unknown_alias.pa", ":1:1: error: Undefined variable or module alias 'X'."},
		{"error_1075_permit_unknown_qualified.pa", ":2:1: error: 'G.nothing' is not a global variable"},
		{"error_1076_import_global_clash.pa", ":2:1: error: Variable 'count' is already declared."},
		{"error_1077_import_global_twice_name.pa", ":2:1: error: Variable 'count' is already declared."},
		{"error_1078_import_global_unqualified_under_alias.pa", ":2:1: error: Undefined variable 'count'."},
		{"error_1079_block_import_global_outside.pa", ":2:1: error: Undefined variable 'count'."},
		{"error_1080_func_import_global_outside.pa", ":2:28: error: Undefined variable 'count'."},
		{"error_1081_import_global_replace.pa", ":3:12: error: cannot assign to 'G.arr': a function writes the "
		 "elements or fields of an array or struct global variable, but does not replace it."},
		{"error_1082_export_global_run_size.pa", ":2:1: error: the sizes of exported global 'a' must be constants "
		 "or its array literal's: an importer cannot know them otherwise."},
		{"error_1083_import_hidden_global.pa", ":2:1: error: Undefined variable 'hidden'."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		genModuleAst("../test/testdata/sa/lib_sa_global.pa");
		genModuleAst("../test/testdata/sa/lib_sa_global2.pa");
		string ast_out = moduleAstPath("../test/testdata/sa/" + file);
		ASSERT_EQ(execTestCommand(
			"palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, give_and_take)
{
	const string nonOwner = " needs a source that owns its value: an owning variable, an owned field or element, "
	                        "a call result, or a pointer viewed as an owner ('p as T').";
	const string toParam = " cannot take a value given away with '>>': only a parameter that owns it can.";
	const string viewTarget = ": '->> loc as T' needs a location holding a writable pointer ('@!X') "
	                          "and an owned type T (a struct, '[n]T' or '[+]T').";
	const string castGiven = ": a value given away with '>>' cannot be cast: it is given as the type it has.";
	const pair<string, string> cases[] = {
		{"error_553_arg_move_owner.pa", ":3:3: error: '>>'" + nonOwner},
		{"error_554_c_arg_move_owner.pa", ":3:16: error: '>>'" + nonOwner},
		{"error_517_chain_transfer.pa", ":3:1: error: '->>'" + nonOwner},
		{"error_897_transfer_not_arg.pa", ":3:7: error: '>>' gives a value away only as a call argument, to a parameter that owns it."},
		{"error_898_transfer_to_borrow_param.pa", ":4:1: error: parameter 'q' of 'f'" + toParam},
		// A C function's pointer parameter borrows; only a cast of the function hands ownership over.
		{"error_899_transfer_to_c_ptr_param.pa", ":4:1: error: parameter '__ptr' of 'free'" + toParam},
		{"error_900_transfer_to_variadic.pa", ":4:1: error: parameter '#2' of 'printf'" + toParam},
		{"error_901_transfer_cast.pa", ":4:3: error" + castGiven},
		{"error_902_transfer_from_frame_var.pa", ":4:20: error: '>>'" + nonOwner},
		{"error_903_take_from_read_only_view.pa",
		 ":4:1: error: '<<=' cannot move from a read-only pointer viewed as an owner ('@X as T')"},
		{"error_904_take_shape_mismatch.pa", ":3:1: error: cannot transfer 'Q' into '[2]int32'"},
		{"error_905_take_into_frame_var.pa", ":3:1: error: '<<=' needs a variable that owns its storage; 'r' is laid out in place."},
		{"error_906_take_into_prim.pa", ":2:1: error: '<<=' needs a variable that owns its array or struct; 'm' does not."},
		{"error_907_take_from_non_owner.pa", ":3:1: error: '<<='" + nonOwner},
		{"error_908_take_from_prim.pa", ":3:1: error: '<<='" + nonOwner},
		{"error_940_owner_view_from_non_owner.pa", ":3:1: error: '->>'" + nonOwner},
		{"error_909_call_non_func_ptr.pa", ":2:1: error: 'int32' is not a function pointer and cannot be called."},
		{"error_910_keep_param_count.pa",
		 ":2:13: error: the cast's function type has 2 parameters, but '@func(uint64) -> @!void' has 1."},
		{"error_911_keep_from_non_func.pa",
		 ":2:12: error: '_' keeps the type of the cast's source at its place, but 'int64' has no such place."},
		{"error_912_owner_view_target_read_only.pa", ":4:1: error: '->>' cannot give a value to '@int8'" + viewTarget},
		{"error_913_owner_view_target_not_ptr.pa", ":3:1: error: '->>' cannot give a value to 'int64'" + viewTarget},
		{"error_914_owner_view_target_not_owned.pa", ":3:1: error: '->>' cannot give a value to '@!int8'" + viewTarget},
		{"error_915_owner_view_target_shape.pa", ":3:1: error: cannot transfer '[4]int8' into '[4]int32'"},
		// A `[]T` parameter keeps no length, nor does a `[+]T` take one from the array it copies.
		{"error_916_copy_from_unsized_borrow.pa", ":4:2: error: cannot copy '[?]int32' into '[4]int32'"},
		{"error_918_operator_func_arity.pa", ":2:1: error: operator function '%' must take two parameters."},
		{"error_919_operator_func_no_struct.pa",
		 ":1:1: error: operator function '*' must take a struct ('T' or '@T') parameter."},
		{"error_920_operator_func_writable_param.pa",
		 ":2:1: error: parameter 'a' of operator function '+' cannot be '@!': an operand is never passed for writing."},
		{"error_921_no_operator_func.pa", ":3:10: error: No operator function '/' takes (vec2, vec2)."},
		// An operand cannot be written so as to choose between a borrow and a copy.
		{"error_922_operator_func_ambiguous.pa", ":5:10: error: Ambiguous function call '-': it can mean -(@vec2), -(vec2)."},
		{"error_923_operator_func_no_match.pa",
		 ":4:10: error: No function '+' takes (vec2, flo64); there are +(@vec2, @vec2)."},
		// A call left unchosen while its operator's function is chosen reports as itself.
		{"error_1090_overload_inner_call_ambiguous.pa",
		 ":9:3: error: Ambiguous function call 'h': it can mean h(int32), h(flo64)."},
		{"error_951_compare_func_ret.pa", ":2:1: error: operator function '==' must return 'bool'."},
		{"error_952_compare_func_arity.pa", ":2:1: error: operator function '<' must take two parameters."},
		{"error_953_compare_func_writable_param.pa",
		 ":2:1: error: parameter 'a' of operator function '==' cannot be '@!': an operand is never passed for writing."},
		{"error_954_compare_func_no_struct.pa",
		 ":1:1: error: operator function '<' must take a struct ('T' or '@T') parameter."},
		// Two structs once compared by address, silently.
		{"error_955_struct_compare_no_func.pa", ":4:13: error: No operator function '==' takes (ver, ver)."},
		// A derived '>' looks for '<' with the operands swapped.
		{"error_956_derived_compare_no_func.pa", ":4:14: error: No operator function '<' takes (ver, int64)."},
		{"error_957_c_struct_ptr_compare.pa",
		 ":2:13: error: No operator function '==' takes (@!FILE, @!FILE)."},
		{"error_958_wide_char_uint8.pa", ":1:11: error: character literal U+00E9 needs an integer type of 32 bits or more, not 'uint8': no byte of UTF-8 text holds it."},
		// A comparison types no literal by the other operand, so it is checked there.
		{"error_959_wide_char_compare_byte.pa", ":1:48: error: character literal U+00E9 needs an integer type of 32 bits or more, not 'uint8': no byte of UTF-8 text holds it."},
		{"error_960_wide_char_int16_arg.pa", ":2:3: error: character literal U+20AC needs an integer type of 32 bits or more, not 'int16': no byte of UTF-8 text holds it."},
		// A candidate fitting through its defaults is never ranked above another.
		{"error_961_default_ambiguous.pa", ":3:1: error: Ambiguous function call 'f': it can mean f(int32), f(int32, int32 = 0)."},
		{"error_962_default_ambiguous_widen.pa", ":3:1: error: Ambiguous function call 'f': it can mean f(int32, int32 = 0), f(int64)."},
		{"error_963_default_not_last.pa", ":1:1: error: parameter 'c' needs a default value: it follows a parameter that has one, and parameters with default values come last."},
		{"error_964_default_var.pa", ":2:18: error: the default value of parameter 'a' must be a compile-time constant: a number, 'true' or 'false', an enum value, a const, or a string literal."},
		{"error_965_default_call.pa", ":2:18: error: the default value of parameter 'a' must be a compile-time constant: a number, 'true' or 'false', an enum value, a const, or a string literal."},
		{"error_966_default_struct.pa", ":2:15: error: the default value of parameter 'p' must be a compile-time constant: a number, 'true' or 'false', an enum value, a const, or a string literal."},
		{"error_967_operator_func_default.pa", ":2:21: error: parameter 'b' of operator function '+' cannot have a default value: an operator is always given its operands."},
		{"error_968_default_too_few.pa", ":2:1: error: function 'f' takes 1 to 2 argument(s), but 0 given."},
		{"error_969_default_too_many.pa", ":2:1: error: function 'f' takes 1 to 2 argument(s), but 3 given."},
		{"error_970_default_out_of_range.pa", ":1:17: error: Integer literal '300' is out of range for type 'int8'."},
		{"error_971_default_type.pa", ":1:18: error: cannot convert '@uint8' to 'int32'."},
		// A function pointer's type has no defaults.
		{"error_972_default_func_ptr.pa", ":3:1: error: function 'fp' takes 2 argument(s), but 1 given."},
		{"error_973_default_narrowing.pa", ":1:18: error: Implicit conversion from 'flo64' to 'int32' is not allowed"},
		{"error_974_variadic_elem_arr.pa", ":1:1: error: a variadic parameter's elements cannot be an array by value ('[n]T', '[]T', '[+]T') or '$T': each argument is given as a parameter is, so an array must be borrowed ('@[]T')."},
		{"error_975_variadic_elem_embed.pa", ":1:1: error: a variadic parameter's elements cannot be an array by value ('[n]T', '[]T', '[+]T') or '$T': each argument is given as a parameter is, so an array must be borrowed ('@[]T')."},
		{"error_976_variadic_with_default.pa", ":1:1: error: function 'f' cannot have both default values and a variadic parameter: an argument could be taken for either."},
		{"error_977_operator_func_variadic.pa", ":2:1: error: operator function '+' cannot take a variadic parameter: an operator has fixed operands."},
		{"error_978_syscall_variadic.pa", ":1:1: error: syscall 's' cannot take a variadic parameter."},
		// A candidate fitting through its variadic parameter is never ranked above another.
		{"error_979_variadic_ambiguous.pa", ":3:1: error: Ambiguous function call 'f': it can mean f(int32), f(int32, [...]int32)."},
		{"error_980_variadic_too_few.pa", ":2:1: error: function 'f' takes at least 1 argument(s), but 0 given."},
		{"error_981_variadic_elem_mismatch.pa", ":2:1: error: Implicit conversion from 'flo64' to 'int32' is not allowed"},
		// The array is the caller's; only its elements are the function's.
		{"error_982_variadic_move_array.pa", ":3:2: error: '->>' needs a source that owns its value: an owning variable, an owned field or element, a call result, or a pointer viewed as an owner ('p as T')."},
		{"error_983_variadic_func_ptr.pa", ":2:1: error: cannot convert '@func([...]int64)' to '@func(@int64)'."},
		{"error_984_variadic_c_callback.pa", ":4:1: error: cannot convert '@func(@void, [...]@void) -> int32' to '@func(@void, @void) -> int32'."},
		{"error_986_variadic_take_array.pa", ":2:2: error: '<<=' needs a source that owns its value: an owning variable, an owned field or element, a call result, or a pointer viewed as an owner ('p as T')."},
		{"error_924_keep_param_count_nested.pa",
		 ":3:2: error: the cast's function type has 2 parameters, but '@func(int32)' has 1."},
		{"error_925_given_twice.pa",
		 ":4:8: error: 's' is given away with '>>' and set to NULL just before the call, but this later argument of the same call may change it."},
		{"error_926_given_container_borrowed.pa",
		 ":6:18: error: 's.p' is given away with '>>' and set to NULL just before the call, but this later argument of the same call may change it."},
		{"error_927_given_index_borrowed.pa",
		 ":6:15: error: 'arr[..]' is given away with '>>' and set to NULL just before the call, but this later argument of the same call may change it."},
		{"error_928_given_receiver.pa",
		 ":6:10: error: 's.p' is given away with '>>' and set to NULL just before the call, but this later argument of the same call may change it."},
		{"error_929_given_global_call.pa",
		 ":5:9: error: 'gs' is given away with '>>' and set to NULL just before the call, but this later argument of the same call may change it."},
		{"error_930_given_path_overlap.pa",
		 ":5:8: error: 's' is given away with '>>' and set to NULL just before the call, but this later argument of the same call may change it."},
		{"error_931_given_global_in_func.pa",
		 ":4:37: error: 'arr[..]' is given away with '>>' and set to NULL just before the call, but this later argument of the same call may change it."},
		// Only a C function's cast may change ownership: any other function frees by its signature.
		{"error_932_func_cast_owned_to_borrow.pa",
		 ":4:2: error: cannot cast '@func(P)' to '@func(@P)': only a C function can be cast to take or give ownership."},
		{"error_933_func_cast_borrow_to_owned.pa",
		 ":4:2: error: cannot cast '@func(@P)' to '@func(P)': only a C function can be cast to take or give ownership."},
		{"error_934_func_cast_owned_ret.pa",
		 ":3:9: error: cannot cast '@func() -> P' to '@func() -> @P': only a C function can be cast to take or give ownership."},
		{"error_935_func_ptr_var_cast_ownership.pa",
		 ":4:15: error: cannot cast '@func(P)' to '@func(@P)': only a C function can be cast to take or give ownership."},
		{"error_936_func_cast_nested_ownership.pa",
		 ":4:2: error: cannot cast '@func(@func(P))' to '@func(@func(@P))': only a C function can be cast to take or give ownership."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, expr_stmt_not_call) {
	const vector<pair<string, string>> cases = {
		{"error_1341_expr_stmt_arith.pa", ":2:1: error: the value of this statement is not used"},
		{"error_1342_expr_stmt_id.pa", ":2:13: error: the value of this statement is not used"},
		{"error_1343_expr_stmt_elem.pa", ":2:1: error: the value of this statement is not used"},
		{"error_1344_expr_stmt_field.pa", ":3:5: error: the value of this statement is not used"},
		{"error_1345_expr_stmt_dict.pa", ":2:1: error: the value of this statement is not used"},
		{"error_1346_expr_stmt_call_operand.pa", ":2:1: error: the value of this statement is not used"},
		{"error_1347_expr_stmt_call_cast.pa", ":2:1: error: the value of this statement is not used"},
		{"error_1348_expr_stmt_call_operand_stmts.pa", ":3:1: error: the value of this statement is not used"},
		{"error_1349_expr_stmt_folded.pa", ":2:1: error: the value of this statement is not used"},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, union) {
	const vector<pair<string, string>> cases = {
		{"error_1376_union_dup_variant.pa", ":1:31: error: variant 'i' is already defined in union 'U'."},
		{"error_1381_union_self_embed.pa", ":3:2: error: union 'U' recursively contains itself."},
		{"error_1382_union_owned_cycle.pa", ":2:1: error: type 'U' depends on itself: U -> V -> U."},
		{"error_1377_union_no_contents.pa", ":3:11: error: variant 'Null' of union 'U' has no contents to access."},
		{"error_1384_union_unknown_variant.pa", ":2:7: error: union 'U' has no variant 'k'."},
		{"error_1385_union_variant_needs_value.pa", ":2:7: error: 'U.i' has contents, which are given as 'U.i(value)'."},
		{"error_1386_union_variant_no_value.pa", ":2:7: error: 'U.Null' has no contents: it is written 'U.Null', without '()'."},
		{"error_1387_union_unknown_field.pa", ":3:11: error: union 'U' has no variant 'k'."},
		{"error_1378_is_not_union.pa", ":2:10: error: 'is' tests a union's variant, not a value of type 'int32'."},
		{"error_1390_is_bind_place.pa", ":3:9: error: 'i' can be bound only in the condition of an 'if' or 'while', or on the left of '&&' there."},
		{"error_1391_is_bind_not_cond.pa", ":3:15: error: 'i' can be bound only in the condition"},
		{"error_1392_is_bind_no_contents.pa", ":3:9: error: variant 'Null' of union 'U' has no contents to access."},
		{"error_1393_is_bind_readonly.pa", ":3:10: error: cannot bind a read-only pointer '@T' to a mutable pointer '@!T'."},
		{"error_1394_is_change_variant.pa", ":5:2: error: cannot change the variant of a union whose contents 'is' binds ('j')."},
		// The index is not known, so any element may be the one tested.
		{"error_1395_is_change_container.pa", ":5:3: error: cannot change the variant of a union whose contents 'is' binds ('s.us[]')."},
		{"error_1396_is_give_binding.pa", ":5:2: error: cannot change the variant of a union whose contents 'is' binds ('a')."},
		{"error_1397_is_bind_else.pa", ":4:23: error: Undefined variable 'i'."},
		{"error_1398_is_borrow_whole.pa", ":5:4: error: cannot change the variant of a union whose contents 'is' binds ('j')."},
		{"error_1399_is_bind_taken.pa", ":4:9: error: Variable 'i' is already declared."},
		{"error_1379_if_is_not_union.pa", ":2:4: error: 'is' tests a union's variant, not a value of type 'int32'."},
		{"error_1401_if_is_missing.pa", ":3:1: error: 'if ... is { }' does not handle 'a' of union 'U': add an arm or 'else'."},
		{"error_1402_if_is_else_unreachable.pa", ":3:1: error: 'else' is never reached: every variant of union 'U' has an arm."},
		{"error_1403_if_is_repeated.pa", ":3:20: error: variant 'Null' is already handled by another arm of this 'if ... is { }'."},
		{"error_1404_if_is_change_variant.pa", ":5:7: error: cannot change the variant of a union whose contents 'is' binds ('j')."},
		{"error_1405_if_is_bind_other_arm.pa", ":5:9: error: Undefined variable or module alias 'a'."},
		{"error_1406_union_lit_no_conversion.pa", ":2:7: error: a '{..}' literal does not convert to union 'U': no function 'U' takes it alone."},
		{"error_1407_union_lit_ambiguous.pa", ":4:7: error: Ambiguous function call 'J': it can mean J([+]J), J([+]int64)."},
		// Only a function taking bool converts true: a bool widens to any number.
		{"error_1408_bool_conv_needs_bool.pa", ":3:1: error: cannot copy 'bool' into 'big'"},
		// A bare literal is not a '@S': no overload takes it.
		{"error_1409_lit_arg_fits_no_overload.pa", ":4:11: error: No function 'g' takes (literal); there are g(@S), g(int64)."},
		{"error_1426_is_unknown_variant.pa", ":3:4: error: union 'U' has no variant 'k'."},
		{"error_1427_if_is_bind_readonly.pa", ":2:35: error: cannot bind a read-only pointer '@T' to a mutable pointer '@!T'."},
		// The union tested is reached through a borrow the temporary may hold.
		{"error_1428_is_temp_escapes.pa", ":6:4: error: a borrow returned by a call given a temporary struct may point to that struct"},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, subscript_func) {
	const string sig = "error: '[]' must be 'func [](@T t, K k) -> @E' (or 'E') to read, or 'func [](@!T t, K k) -> @!E' to write";
	const string putSig = "error: '->[]' must be 'func ->[](V v, @!T t, K k)'";
	const string tested = "error: cannot change the variant of a union whose contents 'is' binds ('u').";
	const vector<pair<string, string>> cases = {
		{"error_1410_subscript_arity.pa", ":2:1: " + sig},
		{"error_1411_subscript_write_ret.pa", ":2:1: " + sig},
		{"error_1412_subscript_by_value.pa", ":2:1: " + sig},
		{"error_1413_put_readonly.pa", ":2:1: " + putSig},
		{"error_1414_put_ret.pa", ":2:1: " + putSig},
		// A read never needs t writable, so it takes only a '[]' of '@T'.
		{"error_1415_subscript_read_needs_ro.pa", ":4:11: error: No operator function '[]' takes (@T, int64)."},
		{"error_1416_subscript_write_readonly.pa", ":4:21: error: cannot write through read-only pointer '@T'"},
		// Not the pointer's subscript, though p is a borrow.
		{"error_1417_subscript_no_fit.pa", ":3:32: error: No function '[]' takes (@T, int64); there are [](@T, @[]uint8)."},
		// A type with '->[]' stores only through it, not through '[]'.
		{"error_1418_put_no_fit.pa", ":5:1: error: No function '->[]' takes (int64, @!T, int64); there are ->[](int64, @!T, @[]uint8)."},
		{"error_1419_subscript_default.pa", ":2:25: error: parameter 'i' of operator function '[]' cannot have a default value"},
		{"error_1420_subscript_move_whole.pa", ":6:1: error: cannot assign to a struct element as a whole"},
		{"error_1421_put_tested_union.pa", ":4:14: " + tested},
		{"error_1422_subscript_temp_base.pa", ":4:13: error: a struct or array returned by a call has no owner to access it through"},
		{"error_1423_subscript_tested_union.pa", ":4:28: " + tested},
		// A '[]' returning by value returns a temp, as any call does.
		{"error_1424_subscript_value_field.pa", ":5:11: error: a struct or array returned by a call has no owner"},
		{"error_1425_subscript_value_borrow.pa", ":6:4: error: a struct or array returned by a call has no owner"},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}
