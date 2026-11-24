/// semantic analyzer error case tests: arrays
///
/// @file errorArrayTests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include "../test-base/testBase.h"

using namespace std;

TEST(sa_error, embed_arr_unsized_inner) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_050_embed_arr_unsized_inner.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("an array laid out in place without a length"), string::npos);
}

TEST(sa_error, embed_arr_inner_size_mismatch) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_051_embed_arr_inner_size_mismatch.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("mismatch"), string::npos);
}

TEST(sa_error, array_size_not_integer) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_044_array_size_float.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");  // SA must fail
	ASSERT_NE(sa.find("integer"), string::npos);
}

TEST(sa_error, arr_index_float_idx) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_045_arr_index_float_idx.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");  // SA must fail
	ASSERT_NE(sa.find("Array index expression must be an integer type"), string::npos);
}

TEST(sa_error, arr_not_array_type) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_046_arr_not_array_type.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");  // SA must fail
	ASSERT_NE(sa.find("Expression is not an array (pointer) type"), string::npos);
}

TEST(sa_error, arr_assign_void) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_047_arr_assign_void.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");  // SA must fail
	ASSERT_NE(sa.find("Void function call cannot be used as a value"), string::npos);
}

TEST(sa_error, unsized_arr_var_decl) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_048_unsized_arr_var.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");  // SA must fail
	ASSERT_NE(sa.find("Unsized array type cannot be used in variable declaration"), string::npos);
}

TEST(sa_error, embed_arr_variable_inner_arg) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_052_embed_arr_variable_inner_arg.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("mismatch"), string::npos);
	ASSERT_NE(sa.find("variable"), string::npos);
}

TEST(sa_error, embed_arr_float_size) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_053_embed_arr_float_size.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("Array size expression must be an integer type"), string::npos);
}

TEST(sa_error, embed_arr_owned_sub_struct)
{
	// [n]$Outer where Outer has an owned struct field — should error
	// Covers: E_EmbedArrOwnedSubStruct in sa_embed_arr_var_decl
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_076_embed_arr_owned_struct.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("it owns storage of its own"), string::npos) << sa;
}

TEST(sa_error, write_readonly_arr_elem)
{
	// [4]@Point rpts; 42 -> rpts[0].x; — write through read-only pointer array element
	// Covers: resolveObjectChain(forWrite=true) arr-index base case, mutable:false branch (E_WriteToReadOnlyArrElem)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_077_write_readonly_arr_elem.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot write through read-only pointer"), string::npos);
}

TEST(sa_error, arr_field_size_not_constant)
{
	// type Buf { [int64(2) + 1]$int64 data; }; -- a cast is not folded as a constant expression
	// Covers: buildStructDef "arr" branch, E_ArrFieldSizeNotConstant
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_080_arr_field_size_not_constant.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":2:1: error: struct field array size must be a compile-time constant"), string::npos);
}

TEST(sa_error, embed_arr_field_owned_substruct)
{
	// type Rect { Point tl; Point br; }; type Grid { [2]$Rect cells; };
	// -- Rect has owned sub-struct fields, so [n]$Rect is not supported.
	// Covers: buildStructDef "arr" branch, struct-leaf case, E_EmbedArrOwnedSubStruct
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_081_embed_arr_field_owned_substruct.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":4:1: error: cannot embed 'Rect' with '$': it owns storage of its own"), string::npos) << sa;
}

TEST(sa_error, recursive_arr_field)
{
	// type A { [2]$A a; }; -- self-referential embedded array field
	// Covers: buildStructDef "arr" branch, struct-leaf case, E_RecursiveStruct
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_082_recursive_arr_field.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":1:1: error: struct 'A' recursively contains itself"), string::npos);
}

TEST(sa_error, write_readonly_arr_field_elem)
{
	// type Watch { [3]@Point observed; }; p -> w.observed[0]; 42 -> w.observed[0].x;
	// -- write-through to a non-mutable embed-ptr-arr struct field element
	// Covers: resolveObjectChain(forWrite=true) arr-index base case, mutable:false
	// branch (E_WriteToReadOnlyArrElem), exercised via a struct field (embed-ptr-arr)
	// rather than a plain variable array (already covered by write_readonly_arr_elem).
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_083_write_readonly_arr_field_elem.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot write through read-only pointer"), string::npos);
}

TEST(sa_error, field_inline_unsized_row)
{
	// type T { $[4]$[]int64 field; }; -- an array laid out in the struct needs every length
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_088_field_inline_unsized_row.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":1:1: error: unsupported struct field type"), string::npos);
}

TEST(sa_error, arr_lit_func_arg)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_208_arr_lit_func_arg.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":2:5: error: parameter 'a' of type '[?]int32' cannot take an array literal: only a struct or fixed-size array parameter can."), string::npos);
}

TEST(sa_error, arr_lit_init)
{
	const pair<string, string> cases[] = {
		{"error_209_arr_lit_count_mismatch.pa", ":1:1: error: array 'a' has size 3 but its array literal has 2 elements."},
		{"error_210_arr_lit_size_not_const.pa", ":2:1: error: array 'a' given an array literal must have a compile-time constant size."},
		{"error_211_arr_lit_dim_mismatch.pa", ":1:15: error: the array literal's dimensions do not match array 'a'."},
		{"error_212_arr_lit_elem_type.pa", ":2:11: error: a value of struct 'P' must be written as '[...]' or '{name: value, ...}'."},
		{"error_213_arr_lit_elem_narrowing.pa", ":2:17: error: Implicit conversion from 'int16' to 'int8' is not allowed"},
		{"error_214_arr_lit_elem_range.pa", ":1:17: error: Integer literal '300' is out of range for type 'int8'."},
		// Elements are analyzed before the array is declared.
		{"error_215_arr_lit_self_ref.pa", ":1:18: error: Undefined variable 'a'."},
		{"error_216_arr_lit_ptr_elem.pa", ":1:1: error: array 'a' cannot be given an array literal: only numeric and struct element types are supported."},
		{"error_217_arr_lit_unknown_elem.pa", ":1:1: error: unknown type 'Foo'."},
		{"error_218_arr_lit_void_elem.pa", ":2:15: error: Void function call cannot be used as a value."},
		{"error_219_arr_lit_row_size.pa", ":1:24: error: array 'm' has rows of size 3 but a row of its array literal has 2 elements."},
		{"error_220_arr_lit_row_size_declared.pa", ":1:19: error: array 'm' has rows of size 3 but a row of its array literal has 2 elements."},
		{"error_221_arr_lit_row_count.pa", ":1:1: error: array 'm' has size 3 but its array literal has 2 elements."},
		{"error_222_arr_lit_2d_given_1d.pa", ":1:18: error: the array literal's dimensions do not match array 'm'."},
		{"error_224_arr_lit_row_size_not_const.pa", ":2:1: error: array 'm' given an array literal must have a compile-time constant size."},
		{"error_375_struct_lit_void_field.pa", ":3:16: error: Void function call cannot be used as a value."},
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

TEST(sa_error, arr_borrow)
{
	const pair<string, string> cases[] = {
		{"error_231_arr_borrow_write_ro.pa", ":1:26: error: cannot write through read-only pointer"},
		{"error_232_arr_borrow_write_ro_embed.pa", ":1:27: error: cannot write through read-only pointer"},
		{"error_233_arr_borrow_ro_to_mut.pa", ":2:1: error: cannot bind a read-only pointer"},
		{"error_234_arr_borrow_no_addr_of.pa", ":2:1: error: an array given to a borrowed array ('@[n]T'/'@![n]T') or a '@T'/'@!T' pointer must be written as"},
		{"error_235_arr_borrow_size_mismatch.pa", "array shape '[3][4]int32' does not match the borrowed array type '@[3][5]int32'."},
		{"error_236_arr_borrow_runtime_size.pa", "array shape '[?]int32' does not match the borrowed array type '@[2]int32'."},
		{"error_237_arr_borrow_embed_mismatch.pa", "array shape '[2]$[3]int32' does not match the borrowed array type '@[2][3]int32'."},
		{"error_238_arr_borrow_size_not_const.pa", ":2:1: error: every size in a borrowed array type"},
		{"error_239_addr_of_struct_arr.pa", "array shape '[2]P' does not match the borrowed array type '@[2]int32'."},
		{"error_241_arr_borrow_mut_of_ro.pa", "cannot bind a read-only pointer"},
		{"error_242_arr_borrow_local_no_addr_of.pa", ":2:1: error: an array given to a borrowed array ('@[n]T'/'@![n]T') or a '@T'/'@!T' pointer must be written as"},
		{"error_243_arr_borrow_assign_mismatch.pa", ":4:1: error: array shape '[3]int32' does not match"},
		{"error_245_ptr_to_arr_borrow.pa", ":1:1: error: a borrow of a sized array ('@[n]T'/'@![n]T') cannot be an array's or a dictionary's element"},
		{"error_858_ro_unsized_borrow_write.pa", ":1:22: error: cannot write through read-only pointer '@T'"},
		{"error_859_unsized_borrow_no_addr_of.pa", ":3:1: error: an array given to a borrowed array ('@[n]T'/'@![n]T') or a '@T'/'@!T' pointer must be written as"},
		{"error_860_unsized_borrow_ro_to_mut.pa", ":2:1: error: cannot bind a read-only pointer"},
		{"error_246_arr_borrow_embed_prim.pa", ":1:1: error: '$' applies only to a struct element or a '$[m]' row, not 'int32'."},
		{"error_271_struct_arr_borrow_write_ro.pa", ":3:7: error: cannot write through read-only pointer"},
		{"error_272_struct_arr_borrow_mut_elem_of_ro.pa", ":3:10: error: cannot write through read-only pointer"},
		{"error_273_struct_arr_borrow_elem_no_addr_of.pa", ":3:2: error: a struct given to a '@T'/'@!T' pointer must be written as"},
		{"error_274_struct_arr_borrow_ptr_slots.pa", "array shape '[2]@P' does not match the borrowed array type '@![2]P'."},
		{"error_275_struct_arr_borrow_size_mismatch.pa", "array shape '[2][4]P' does not match the borrowed array type '@[2][3]P'."},
		{"error_276_embed_struct_arr_borrow_write_ro.pa", ":3:7: error: cannot write through read-only pointer"},
		{"error_277_embed_struct_arr_borrow_mut_elem_of_ro.pa", ":3:10: error: cannot write through read-only pointer"},
		{"error_278_embed_struct_arr_borrow_owned_arr.pa", "array shape '[2]P' does not match the borrowed array type '@[2]$P'."},
		{"error_279_struct_arr_borrow_embed_arr.pa", "array shape '[2]$P' does not match the borrowed array type '@[2]P'."},
		{"error_280_embed_struct_arr_borrow_size_mismatch.pa", "array shape '[3]$P' does not match the borrowed array type '@[2]$P'."},
		{"error_281_embed_struct_arr_field_access.pa", ":3:11: error: field access on non-struct variable."},
		{"error_303_arr_borrow_addr_of_param.pa", ":1:25: error: 'g' is already a borrow"},
		{"error_304_arr_borrow_addr_of_local.pa", ":3:16: error: 'g' is already a borrow"},
		{"error_305_arr_borrow_row_by_name.pa", ":3:1: error: an array given to a borrowed array"},
		{"error_819_arr_to_ptr_param_no_addr_of.pa", ":3:1: error: an array given to a borrowed array ('@[n]T'/'@![n]T') or a '@T'/'@!T' pointer must be written as"},
		{"error_820_arr_to_ptr_slot_no_addr_of.pa", ":3:1: error: an array given to a borrowed array ('@[n]T'/'@![n]T') or a '@T'/'@!T' pointer must be written as"},
		{"error_821_arr_to_ptr_var_no_addr_of.pa", ":2:1: error: an array given to a borrowed array ('@[n]T'/'@![n]T') or a '@T'/'@!T' pointer must be written as"},
		{"error_822_row_to_ptr_slot_no_addr_of.pa", ":3:1: error: an array given to a borrowed array ('@[n]T'/'@![n]T') or a '@T'/'@!T' pointer must be written as"},
		{"error_823_runtime_arr_to_ptr_no_addr_of.pa", ":4:2: error: an array given to a borrowed array ('@[n]T'/'@![n]T') or a '@T'/'@!T' pointer must be written as"},
		{"error_824_arr_field_to_ptr_no_addr_of.pa", ":4:1: error: an array given to a borrowed array ('@[n]T'/'@![n]T') or a '@T'/'@!T' pointer must be written as"},
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

TEST(sa_error, copy)
{
	const pair<string, string> cases[] = {
		{"error_207_arr_var_init_not_literal.pa", ":1:1: error: cannot copy 'int64' into '[3]int32'"},
		{"error_296_copy_size_mismatch.pa", ":3:1: error: cannot copy '[4]int32' into '[3]int32'"},
		{"error_655_copy_unsized_row_call_target.pa", ":4:2: error: cannot copy '@!int32' into '[2]int32'"},
		{"error_298_copy_elem_type.pa", ":2:1: error: cannot copy '[3]int32' into '[3]int64'"},
		{"error_299_copy_struct_type.pa", ":5:1: error: cannot copy 'P' into 'Q'"},
		{"error_300_copy_into_ro_elem.pa", ":3:2: error: cannot write through read-only pointer '@T'"},
		{"error_301_copy_ptr_slots_into_structs.pa", ":4:1: error: cannot copy '[2]@P' into '[2]P'"},
		{"error_374_arr_copy_void_init.pa", ":2:14: error: Void function call cannot be used as a value."},
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

TEST(sa_error, embed_elem_not_struct)
{
	const pair<string, string> cases[] = {
		{"error_318_embed_prim_2d_var.pa", ":1:1: error: '$' applies only to a struct element or a '$[m]' row, not 'int32'."},
		{"error_319_embed_prim_row_elem.pa", ":1:1: error: '$' applies only to a struct element or a '$[m]' row, not 'int32'."},
		{"error_320_embed_prim_alias.pa", ":2:1: error: '$' applies only to a struct element or a '$[m]' row, not 'int32'."},
		{"error_321_embed_prim_param.pa", ":1:1: error: '$' applies only to a struct element or a '$[m]' row, not 'int32'."},
		{"error_322_embed_ptr_elem.pa", ":1:1: error: '$' applies only to a struct element or a '$[m]' row, not '@int32'."},
		{"error_572_embed_prim_field.pa", ":1:1: error: '$' applies only to a struct element or a '$[m]' row, not 'int32'."},
		{"error_808_embed_ptr_field.pa", ":1:1: error: '$' applies only to a struct element or a '$[m]' row, not '@int32'."},
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

TEST(sa_error, field_arr_level_errors)
{
	// An owned field's every level is checked as the field's own.
	const pair<string, string> cases[] = {
		{"error_324_field_2d_embed_prim.pa", ":1:1: error: '$' applies only to a struct element or a '$[m]' row, not 'int32'."},
		{"error_804_field_row_size_not_const.pa", ":3:2: error: struct field array size must be a compile-time constant integer; 'n' is not a const."},
		{"error_805_field_unsized_row.pa", ":1:1: error: unsupported struct field type."},
		{"error_807_field_sized_varr_elem.pa", ":1:1: error: a variable-length array element is written '[+]T', without a size: it starts empty."},
		{"error_809_field_sized_varr.pa", ":1:1: error: a variable-length array element is written '[+]T', without a size: it starts empty."},
		{"error_810_field_embed_varr_rows.pa", ":1:1: error: cannot embed '[+]int32' with '$': it owns storage of its own; use '[+]int32' without '$'."},
		{"error_811_field_inline_embed_owning.pa", ":2:1: error: cannot embed 'O' with '$': it owns storage of its own; use 'O' without '$'."},
		{"error_813_field_embed_prim.pa", ":1:1: error: '$' applies only to a struct element or a '$[m]' row, not 'int32'."},
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

TEST(sa_error, row_borrow_readonly)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_377_row_borrow_readonly.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("cannot write through read-only pointer"), string::npos);
}

TEST(sa_error, row_borrow_size)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_378_row_borrow_size.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("array shape '[3]int32' does not match the borrowed array type '@[4]int32'"), string::npos);
}

TEST(sa_error, row_borrow_runtime_size)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_379_row_borrow_runtime_size.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("array shape '[?]int32' does not match the borrowed array type '@[4]int32'"), string::npos);
}

TEST(sa_error, row_by_name)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_380_row_by_name.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("must be written as '@name' or '@!name'"), string::npos);
}

TEST(sa_error, arr_lit_assign)
{
	const pair<string, string> cases[] = {
		{"error_454_arr_lit_assign_count.pa", ":3:2: error: array 'albedo' has size 4 but its array literal has 2 elements."},
		{"error_455_arr_lit_assign_row_size.pa", ":2:7: error: array 'g' has rows of size 2 but a row of its array literal has 3 elements."},
		{"error_456_arr_lit_assign_size_not_const.pa", ":3:1: error: array 'a' given an array literal must have a compile-time constant size."},
		{"error_457_arr_lit_assign_param.pa", ":2:2: error: array 'p' given an array literal must have a compile-time constant size."},
		{"error_458_arr_lit_assign_borrow.pa", ":2:2: error: an array literal cannot be given to the borrowed array 'b'; '->' to a borrow rebinds it, so write to its elements instead."},
		{"error_459_arr_lit_assign_ptr_elem.pa", ":3:1: error: array 'a' cannot be given an array literal: only numeric and struct element types are supported."},
		{"error_460_arr_lit_assign_transfer.pa", ":3:1: error: '->>' needs a source that owns its value: an owning variable, an owned field or element, a call result, or a pointer viewed as an owner ('p as T')."},
		{"error_461_arr_lit_assign_readonly_field.pa", ":3:12: error: cannot write through read-only pointer '@T'; use '@!T' for mutable."},
		{"error_462_addr_of_readonly_arr_field.pa", ":3:19: error: cannot write through read-only pointer '@T'; use '@!T' for mutable."},
		{"error_463_arr_lit_assign_dict.pa", ":2:1: error: a '{name: value}' literal can only be used as a struct or dictionary value"},
		{"error_464_struct_lit_arr_field_count.pa", ":2:8: error: array 'albedo' has size 4 but its array literal has 2 elements."},
		{"error_465_struct_lit_arr_field_not_lit.pa", ":3:8: error: the array literal's dimensions do not match array 'v'."},
		{"error_466_struct_lit_arr_field_dict.pa", ":2:8: error: a '{name: value}' literal can only be used as a struct or dictionary value"},
		{"error_467_struct_lit_ptr_arr_field.pa", ":3:8: error: array 'ps' cannot be given an array literal: only numeric and struct element types are supported."},
		{"error_468_struct_lit_not_struct.pa", ":3:8: error: a value of struct 'P' must be written as '[...]' or '{name: value, ...}'."},
		{"error_469_struct_lit_other_struct.pa", ":4:12: error: cannot copy 'R' into 'P'"},
		{"error_470_embed_field_to_borrow.pa", ":4:1: error: a struct given to a '@T'/'@!T' pointer must be written as '@name' or '@!name' ('@f()' or '@!f()' for a call returning it, '@{..}', '@!{..}', '@[..]' or '@![..]' for a literal)."},
		{"error_471_display_struct_value.pa", ":3:1: error: cannot convert 'P' to 'int64'."},
		{"error_472_display_struct_param.pa", ":3:1: error: cannot convert 'int64' to 'P'."},
		{"error_473_display_arr.pa", ":2:1: error: cannot convert '[3]int32' to 'int64'."},
		{"error_474_display_arr_2d.pa", ":2:1: error: cannot convert '[2][3]int32' to 'int64'."},
		{"error_475_display_arr_contig.pa", ":2:1: error: cannot convert '[2]$[3]int32' to 'int64'."},
		{"error_476_display_struct_arr.pa", ":3:1: error: cannot convert '[2]P' to 'int64'."},
		{"error_477_display_embed_struct_arr.pa", ":3:1: error: cannot convert '[2]$P' to 'int64'."},
		{"error_478_display_slot_arr.pa", ":2:1: error: cannot convert '[3]@!int32' to 'int64'."},
		{"error_479_display_arr_borrow.pa", ":2:2: error: cannot convert '@[2][3]int32' to 'int64'."},
		{"error_480_display_arr_borrow_row.pa", ":2:2: error: cannot convert '@[3]int32' to 'int64'."},
		{"error_481_display_cast_struct.pa", ":3:11: error: Cannot cast type 'P' to 'int64'."},
		{"error_482_call_borrow_needs_at.pa", ":4:11: error: a struct given to a '@T'/'@!T' pointer must be written as '@name' or '@!name' ('@f()' or '@!f()' for a call returning it, '@{..}', '@!{..}', '@[..]' or '@![..]' for a literal)."},
		{"error_483_call_borrow_decl.pa", ":3:8: error: a struct or array returned by a call has no owner to borrow from"},
		{"error_484_call_borrow_ro_to_mut.pa", ":4:1: error: cannot bind a read-only pointer '@T' to a mutable pointer '@!T'."},
		{"error_485_call_borrow_not_struct.pa", ":3:3: error: cannot take the address of this expression"},
		{"error_486_temp_borrow_decl.pa", ":5:1: error: a borrow returned by a call given a temporary struct may point to that struct"},
		{"error_487_temp_borrow_assign.pa", ":6:1: error: a borrow returned by a call given a temporary struct may point to that struct"},
		{"error_488_temp_borrow_return.pa", ":5:29: error: a borrow returned by a call given a temporary struct may point to that struct"},
		{"error_489_temp_borrow_field.pa", ":7:1: error: a borrow returned by a call given a temporary struct may point to that struct"},
		{"error_490_temp_borrow_slot.pa", ":6:1: error: a borrow returned by a call given a temporary struct may point to that struct"},
		{"error_491_temp_borrow_nested.pa", ":5:1: error: a borrow returned by a call given a temporary struct may point to that struct"},
		{"error_492_temp_borrow_prim.pa", ":6:1: error: a borrow returned by a call given a temporary struct may point to that struct"},
		{"error_493_temp_borrow_tapple.pa", ":6:1: error: a borrow returned by a call given a temporary struct may point to that struct"},
		{"error_494_member_call_borrow_not_arg.pa", ":4:9: error: a struct or array returned by a call has no owner to borrow from"},
		{"error_653_temp_borrow_pinned.pa", ":6:1: error: a borrow returned by a call given a temporary struct may point to that struct"},
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

TEST(sa_error, varr)
{
	const pair<string, string> cases[] = {
		{"error_726_varr_copy_elem_mismatch.pa", ":2:1: error: cannot copy '[+]int32' into '[+]int64': both need the same element type and layout, and sizes, the same where both are known at compile time."},
		{"error_727_varr_transfer_elem_mismatch.pa", ":3:1: error: cannot transfer '[+]int32' into '[+]int64': both need the same element type and layout, and the same sizes where both are known at compile time."},
		{"error_728_varr_embed_owning_struct.pa", ":3:1: error: cannot embed 'Q' with '$': it owns storage of its own; use 'Q' without '$'."},
		{"error_729_varr_lit_over_len.pa", ":1:1: error: array 'v' has size 2 but its array literal has 3 elements."},
		{"error_730_varr_lit_len_not_const.pa", ":2:1: error: array 'v' given an array literal must have a compile-time constant size."},
		{"error_731_varr_to_ptr.pa", ":2:1: error: cannot convert '[+]int32' to '@int32'."},
		{"error_732_varr_push_readonly.pa", ":2:1: error: cannot bind a read-only pointer '@T' to a mutable pointer '@!T'."},
		{"error_733_varr_len_method.pa", ":2:11: error: 'len' cannot be called as a method: its first parameter must be a writable borrow ('@!')."},
		{"error_734_varr_push_no_match.pa", ":4:1: error: No function 'push' takes (@![+]int32, int64); there are push(@![+]flo64, flo64), push(@![+]int32, int32)."},
		{"error_735_varr_borrow_needs_addr.pa", ":2:11: error: a variable-length array given to '@[+]T'/'@![+]T' must be written as '@name' or '@!name'."},
		{"error_736_arr_len_needs_addr.pa", ":2:11: error: an array given to 'len' must be written as '@name'."},
		{"error_1084_len_not_array.pa", ":2:15: error: 'len' takes an array or a dictionary; this value is 'int32'."},
		{"error_1085_len_not_array_chain.pa", ":2:11: error: 'len' takes an array or a dictionary; this value is 'flo64'."},
		{"error_861_len_addr_of_borrow.pa", ":3:15: error: 'h' is already a borrow ('@T'/'@[n]T'/'@[+]T'); pass it by name."},
		{"error_862_varr_borrow_addr_of.pa", ":4:3: error: 'h' is already a borrow ('@T'/'@[n]T'/'@[+]T'); pass it by name."},
		{"error_863_len_addr_of_varr_borrow.pa", ":3:15: error: 'h' is already a borrow ('@T'/'@[n]T'/'@[+]T'); pass it by name."},
		{"error_864_varr_borrow_to_sized_borrow.pa", ":3:1: error: array shape '@[+]int64' does not match the borrowed array type '@[3]int64'."},
		{"error_737_arr_len_unsized_param.pa", ":2:2: error: 'len' cannot tell this array's length: an array declared with its size, or a variable-length array, keeps one, but a '[]T' does not."},
		{"error_739_varr_len_const.pa", ":2:1: error: const 'N' must be initialized with a compile-time constant."},
		{"error_740_varr_borrow_push_readonly.pa", ":1:23: error: cannot bind a read-only pointer '@T' to a mutable pointer '@!T'."},
		{"error_741_varr_borrow_write_readonly.pa", ":1:23: error: cannot write through read-only pointer '@T'; use '@!T' for mutable."},
		{"error_742_varr_local_borrow_needs_addr.pa", ":2:1: error: a variable-length array given to '@[+]T'/'@![+]T' must be written as '@name' or '@!name'."},
		{"error_743_varr_borrow_elem_mismatch.pa", ":3:1: error: cannot convert '@[+]int32' to '@[+]int64'."},
		{"error_744_varr_to_sized_borrow.pa", ":3:1: error: array shape '[+]int32' does not match the borrowed array type '@[3]int32'."},
		{"error_745_varr_readonly_to_unsized.pa", ":2:23: error: cannot bind a read-only pointer '@T' to a mutable pointer '@!T'."},
		{"error_746_varr_builtin_redefined.pa", ":1:1: error: Function 'push' is already defined."},
		{"error_747_varr_builtin_redefined_after.pa", ":2:1: error: Function 'push' is already defined."},
		{"error_748_arr_len_redefined.pa", ":1:1: error: Function 'len' is already defined."},
		{"error_826_varr_ptr_push_struct_no_addr.pa", ":4:1: error: a struct given to a '@T'/'@!T' pointer must be written as"},
		{"error_827_varr_ptr_push_arr_no_addr.pa", ":3:1: error: an array given to a borrowed array ('@[n]T'/'@![n]T') or a '@T'/'@!T' pointer must be written as"},
		{"error_828_varr_ptr_push_upgrade.pa", ":4:1: error: cannot bind a read-only pointer '@T' to a mutable pointer '@!T'."},
		{"error_829_varr_ptr_borrow_slot_write_ro.pa", ":2:28: error: cannot write through read-only pointer '@T'"},
		{"error_830_varr_ptr_write_through_ro.pa", ":2:25: error: cannot write through read-only pointer array element"},
		{"error_831_varr_ptr_borrow_narrow_mut.pa", ":4:1: error: array shape '@![+]@!P' does not match '[+]@P'"},
		{"error_832_varr_ptr_copy_upgrade.pa", ":3:1: error: cannot copy '[+]@P' into '[+]@!P'"},
		{"error_750_varr_borrow_sized.pa", ":1:1: error: a borrowed variable-length array type is written '@[+]T' or '@![+]T', without a size."},
		{"error_751_arrow_after_cmp.pa", ":2:5: error: '->' binds tighter than '<'; parenthesize the operation to store its result: '(a < b) -> c'."},
		{"error_752_arrow_after_logical.pa", ":2:7: error: '->' binds tighter than '&&'; parenthesize the operation to store its result: '(a && b) -> c'."},
		{"error_753_varr_transfer_from_frame.pa", ":3:1: error: '->>' needs a source that owns its value: an owning variable, an owned field or element, a call result, or a pointer viewed as an owner ('p as T')."},
		{"error_945_transfer_from_embed_varr.pa", ":4:1: error: '->>' needs a source that owns its value: an owning variable, an owned field or element, a call result, or a pointer viewed as an owner ('p as T')."},
		{"error_946_transfer_to_embed_varr.pa", ":4:1: error: '->>' needs a field that owns its value (T or [n]T); 'bytes' does not."},
		{"error_754_varr_param_elem_mismatch.pa", ":3:1: error: cannot convert '[+]int64' to '[+]int32'."},
		{"error_755_varr_named_ret_init.pa", ":1:26: error: named return 'r' of a struct or array type cannot have an initializer."},
		{"error_756_varr_return_borrow_mismatch.pa", ":2:2: error: cannot copy '@[+]int64' into '[+]int32': both need the same element type and layout, and sizes, the same where both are known at compile time."},
		{"error_757_varr_lit_transfer.pa", ":2:1: error: '->>' needs a source that owns its value: an owning variable, an owned field or element, a call result, or a pointer viewed as an owner ('p as T')."},
		{"error_758_varr_copy_struct_layout.pa", ":3:1: error: cannot copy '[2]$P' into '[+]P': both need the same element type and layout, and sizes, the same where both are known at compile time."},
		{"error_759_varr_embed_varr.pa", ":1:1: error: cannot embed '[+]int32' with '$': it owns storage of its own; use '[+]int32' without '$'."},
		{"error_760_arr_embed_varr.pa", ":1:1: error: cannot embed '[+]int32' with '$': it owns storage of its own; use '[+]int32' without '$'."},
		{"error_762_varr_elem_sized.pa", ":1:1: error: a variable-length array element is written '[+]T', without a size: it starts empty."},
		{"error_763_nested_varr_lit_count.pa", ":1:1: error: array 'a' has size 3 but its array literal has 2 elements."},
		{"error_764_arr_lit_empty_size.pa", ":1:1: error: array 'a' cannot take its size from an empty array literal '[]'."},
		{"error_765_unsized_varr_elem_param.pa", ":1:1: error: an unsized array of variable-length arrays ('[][+]T') is not supported in this version; borrow the array as '@[n][+]T' or '@[+][+]T'."},
		{"error_766_nested_varr_lit_dim.pa", ":2:7: error: the array literal's dimensions do not match array 'm'."},
		{"error_767_varr_elem_borrow_mismatch.pa", ":3:1: error: cannot convert '@[3][+]int64' to '@[3][+]int32'."},
		{"error_768_nested_varr_readonly_push.pa", ":2:7: error: cannot write through read-only pointer '@T'; use '@!T' for mutable."},
		{"error_769_borrow_embed_varr.pa", ":1:1: error: cannot embed '[+]int32' with '$': it owns storage of its own; use '[+]int32' without '$'."},
		{"error_770_arr_varr_elem_sized.pa", ":1:1: error: a variable-length array element is written '[+]T', without a size: it starts empty."},
		{"error_771_arr_varr_elem_sized_param.pa", ":1:1: error: a variable-length array element is written '[+]T', without a size: it starts empty."},
		{"error_772_nested_varr_readonly_write.pa", ":2:2: error: cannot write through read-only pointer '@T'; use '@!T' for mutable."},
		{"error_773_arr_varr_readonly_write.pa", ":2:2: error: cannot write through read-only pointer '@T'; use '@!T' for mutable."},
		{"error_774_nested3_varr_readonly_write.pa", ":2:2: error: cannot write through read-only pointer '@T'; use '@!T' for mutable."},
		{"error_778_varr_row_size_not_const.pa", ":2:1: error: the row size in '[][m]T' or '[+][m]T' must be a compile-time constant."},
		{"error_779_varr_row_varr_sized.pa", ":1:1: error: a variable-length array element is written '[+]T', without a size: it starts empty."},
		{"error_780_varr_row_push_size.pa", ":3:1: error: cannot convert '[3]int32' to '[4]int32'."},
		{"error_782_varr_row_lit_count.pa", ":1:26: error: array 'a' has rows of size 2 but a row of its array literal has 3 elements."},
		{"error_783_varr_row_copy_layout.pa", ":2:1: error: cannot copy '[+][4]int32' into '[+]$[4]int32': both need the same element type and layout, and sizes, the same where both are known at compile time."},
		{"error_784_varr_row_readonly_write.pa", ":2:2: error: cannot write through read-only pointer '@T'; use '@!T' for mutable."},
		{"error_785_varr_row_pop_size.pa", ":2:1: error: cannot copy '[4]int32' into '[3]int32': both need the same element type and layout, and sizes, the same where both are known at compile time."},
		{"error_786_varr_row_pop_index.pa", ":2:11: error: a struct or array returned by a call has no owner to access it through; receive it into a variable with '->' first."},
		{"error_787_varr_row_embed_owning.pa", ":3:1: error: cannot embed 'P' with '$': it owns storage of its own; use 'P' without '$'."},
		{"error_788_varr_row_pop_borrow.pa", ":2:15: error: a struct or array returned by a call has no owner to borrow from; receive it into a variable with '->' and borrow it with '@name' or '@!name' (only a call argument can borrow it directly, as '@f()' or '@!f()')."},
		{"error_789_embed_varr.pa", ":1:1: error: cannot embed '[+]int32' with '$': it owns storage of its own; use '[+]int32' without '$'."},
		{"error_790_embed_varr_deep.pa", ":1:1: error: cannot embed '[+]int32' with '$': it owns storage of its own; use '[+]int32' without '$'."},
		{"error_791_unsized_rows_of_varr.pa", ":1:1: error: an unsized array of variable-length arrays ('[][+]T') is not supported in this version; borrow the array as '@[n][+]T' or '@[+][+]T'."},
		{"error_793_varr_borrow_row_size.pa", ":3:1: error: array shape '@![+][4]int32' does not match '[+][3]int32': its elements must have the same size and layout, and be owned by both or by neither."},
		{"error_794_arr_param_size_mismatch.pa", ":3:1: error: cannot convert '[4]int32' to '[3]int32'."},
		{"error_795_arr_param_elem_mismatch.pa", ":3:1: error: cannot convert '[3]int16' to '[3]int32'."},
		{"error_797_arr_return_size_mismatch.pa", ":3:2: error: cannot transfer '[3]int32' into '[4]int32': both need the same element type and layout, and the same sizes where both are known at compile time."},
		{"error_798_arr_return_call_size_mismatch.pa", ":3:2: error: cannot copy '[3]int32' into '[4]int32': both need the same element type and layout, and sizes, the same where both are known at compile time."},
		{"error_799_arr_return_borrow_size_mismatch.pa", ":2:2: error: cannot copy '@[4]int32' into '[3]int32': both need the same element type and layout, and sizes, the same where both are known at compile time."},
		{"error_800_arr_sig_size_not_const.pa", ":1:1: error: every size in an array parameter or return type must be a compile-time constant."},
		{"error_801_arr_named_ret_init.pa", ":1:26: error: named return 'r' of a struct or array type cannot have an initializer."},
		{"error_802_arr_return_lit_count.pa", ":2:2: error: array '[3]int32' has size 3 but its array literal has 2 elements."},
		{"error_803_arr_param_lit_count.pa", ":2:3: error: array 'x' has size 3 but its array literal has 2 elements."},
		{"error_1047_varr_borrow_row_vs_struct.pa", ":5:1: error: array shape '@![+][2]$F' does not match '[+]F': its elements must have the same size and layout, and be owned by both or by neither."},
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

TEST(sa_error, owned_rows_c_ptrptr)
{
	// A row the array owns is not a '@!T' slot, whether or not a size is known.
	const string mismatch = "does not match '[?]@!uint8': its elements must have the same size and layout";
	const pair<string, string> cases[] = {
		{"error_594_owned_rows_c_ptrptr.pa", ":3:1: error: array shape '@![1][4]uint8' " + mismatch},
		{"error_595_owned_unsized_rows_c_ptrptr.pa", ":4:1: error: array shape '@![1][?]uint8' " + mismatch},
		{"error_596_owned_rows_runtime_outer_c_ptrptr.pa", ":4:1: error: array shape '@![?][4]uint8' " + mismatch},
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

TEST(sa_error, run_size_move)
{
	const string runSize = "its size cannot change there, and a size known only at run time cannot be shown to match it";
	const string resized = "can only be given away: '<<=', '->>', '>>' or 'return'.";
	const pair<string, string> cases[] = {
		{"error_1096_move_run_size_to_const_var.pa", ":4:1: error: cannot transfer '[?]int32' into '[4]int32': " + runSize},
		{"error_1097_move_run_size_to_row.pa", ":5:1: error: cannot transfer '[?]int32' into '[?]int32': " + runSize},
		{"error_1098_move_const_to_run_size_row.pa", ":4:1: error: cannot transfer '[2]int32' into '[?]int32': " + runSize},
		{"error_1099_move_run_size_to_field.pa", ":5:1: error: cannot transfer '[?]int32' into '[4]int32': " + runSize},
		{"error_1100_move_run_size_to_param.pa", ":4:1: error: cannot transfer '[?]int32' into '[4]int32': " + runSize},
		{"error_1101_take_run_size_into_const.pa", ":3:1: error: cannot transfer '[?]int32' into '[4]int32': " + runSize},
		{"error_1102_return_run_size.pa", ":4:2: error: cannot transfer '[?]int32' into '[4]int32': " + runSize},
		{"error_1103_resize_cast_copy.pa", ":4:14: error: an array cast to '[?]int32' " + resized},
		{"error_1104_resize_cast_read.pa", ":3:12: error: an array cast to '[4]int32' " + resized},
		{"error_1105_resize_cast_elem_mismatch.pa", ":4:1: error: cannot transfer '[?]int32' into '[4]int64': both need the same element type and layout"},
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

TEST(sa_error, use_after_given)
{
	const string given = " error: 'a' was given away and is NULL here; fill it with '->>' before using it.";
	const pair<string, string> cases[] = {
		{"error_1106_given_len.pa", ":4:16:" + given},
		{"error_1107_given_read.pa", ":4:11:" + given},
		{"error_1108_given_copy_into.pa", ":5:1:" + given},
		{"error_1109_given_again.pa", ":5:1:" + given},
		{"error_1110_given_by_arg.pa", ":4:6:" + given},
		{"error_1111_given_by_take.pa", ":2:25:" + given},
		{"error_1112_given_param.pa", ":5:9: error: 'p' was given away"},
		{"error_1113_given_in_if.pa", ":9:6:" + given},
		// Reached from the loop's head after the previous iteration gave it.
		{"error_1114_given_prev_iteration.pa", ":5:7:" + given},
		{"error_1115_given_break.pa", ":11:16:" + given},
		{"error_1116_given_continue.pa", ":7:6:" + given},
		{"error_1117_given_in_cond.pa", ":3:12:" + given},
		{"error_1118_given_for_range.pa", ":5:14:" + given},
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

TEST(sa_error, dict)
{
	const pair<string, string> cases[] = {
		{"error_1153_dict_field.pa", ":1:1: error: cannot embed '[\"\"]int32' with '$': it owns storage of its own"},
		{"error_1154_dict_frame_var.pa", ":2:2: error: '$' on a variable applies only to a struct or an array, not '[\"\"]int32'."},
		{"error_1156_dict_borrow_no_addr.pa", ":3:1: error: a dictionary given to '@[\"\"]T'/'@![\"\"]T' must be written as '@name' or '@!name' ('@{..}' or '@!{..}' for a literal)."},
		{"error_1157_dict_borrow_var_no_addr.pa", ":2:1: error: a dictionary given to '@[\"\"]T'/'@![\"\"]T' must be written as '@name' or '@!name' ('@{..}' or '@!{..}' for a literal)."},
		{"error_1158_dict_copy_elem_mismatch.pa", ":2:1: error: cannot copy '[\"\"]int32' into '[\"\"]int64'"},
		{"error_1159_dict_write_through_borrow.pa", ":2:2: error: cannot write through read-only pointer '@T'"},
		{"error_1160_dict_value_type.pa", ":2:1: error: cannot embed 'P' with '$': it owns storage of its own"},
		{"error_1161_dict_key_type.pa", ":2:11: error: cannot convert 'int64' to '@uint8'."},
		{"error_1162_dict_builtin_redefined.pa", ":1:1: error: Function 'has' is already defined."},
		{"error_1163_dict_field_write_through_borrow.pa", ":3:7: error: cannot write through read-only pointer '@T'"},
		{"error_1164_dict_for_remove.pa", ":2:22: error: cannot change what a 'for' loop iterates over ('d')."},
		{"error_1165_dict_for_clear.pa", ":2:21: error: cannot change what a 'for' loop iterates over ('d')."},
		{"error_1166_dict_for_store.pa", ":2:19: error: cannot change what a 'for' loop iterates over ('d')."},
		{"error_1167_dict_for_store_key.pa", ":2:22: error: cannot change what a 'for' loop iterates over ('d')."},
		{"error_1168_dict_for_store_borrow.pa", ":1:39: error: cannot change what a 'for' loop iterates over ('d')."},
		{"error_1169_dict_for_transfer.pa", ":3:15: error: cannot change what a 'for' loop iterates over ('d')."},
		{"error_1170_dict_for_pass.pa", ":3:17: error: cannot change what a 'for' loop iterates over ('d')."},
		{"error_1171_dict_for_key_assign.pa", ":2:22: error: cannot assign to 'k': the index or key of a 'for' loop is read-only."},
		{"error_1172_dict_for_key_typed.pa", ":2:1: error: the key of a 'for' loop over a dictionary is '@[]uint8', which is not written."},
		{"error_1173_dict_for_write_through_borrow.pa", ":1:28: error: cannot bind a read-only pointer '@T' to a mutable pointer '@!T'."},
		{"error_1174_dict_lit_key_var.pa", ":2:16: error: 'n' is not a string constant: a key of a dictionary literal is a string or a string constant."},
		{"error_1175_dict_lit_key_int_const.pa", ":2:16: error: 'N' is not a string constant: a key of a dictionary literal is a string or a string constant."},
		{"error_1176_dict_lit_key_undefined.pa", ":1:24: error: 'zz' is not a string constant: a key of a dictionary literal is a string or a string constant."},
		{"error_1177_dict_lit_dup_key.pa", ":1:32: error: key \"a\" appears more than once in the dictionary literal."},
		{"error_1178_dict_lit_dup_const_key.pa", ":2:24: error: key \"a\" appears more than once in the dictionary literal."},
		{"error_1179_dict_lit_overload_ambiguous.pa", ":5:1: error: Ambiguous function call 'f': it can mean f(@[\"\"]int32), f(@P)."},
		{"error_1180_struct_lit_string_key.pa", ":2:8: error: a value of struct 'P' names its fields; a string key is for a dictionary."},
		{"error_1181_dict_lit_transfer.pa", ":2:1: error: '->>' needs a source that owns its value"},
		{"error_1182_arr_lit_dict_arg.pa", ":2:3: error: parameter 'd' of type '@[\"\"]int32' cannot take an array literal"},
		{"error_1183_dict_for_value_change.pa", ":2:24: error: cannot change what a 'for' loop iterates over ('a[\"x\"]')"},
		{"error_1184_dict_lit_row_size.pa", ":1:24: error: array 'r' has rows of size 3 but a row of its array literal has 2 elements."},
		{"error_1185_dict_embedded_elem.pa", ":2:2: error: cannot embed '[\"\"]int32' with '$': it owns storage of its own"},
		{"error_1186_dict_for_row_change.pa", ":3:23: error: cannot change what a 'for' loop iterates over ('a[0]')"},
		{"error_1192_empty_dict_lit_array.pa", ":1:14: error: a '{name: value}' literal can only be used as a struct or dictionary value"},
		{"error_1193_empty_lit_missing_field.pa", ":2:7: error: the literal of struct 'P' is missing field 'x'."},
		{"error_1194_empty_lit_overload_ambiguous.pa", ":4:1: error: Ambiguous function call 'f': it can mean f(P), f([\"\"]int32)."},
		{"error_1199_dict_copy_to_varr.pa", ":3:1: error: cannot copy '[\"\"]int32' into '[+]int32'"},
		{"error_1200_dict_to_varr_borrow.pa", ":3:1: error: cannot convert '[\"\"]int32' to '@[+]int32'."},
		{"error_1201_arr_lit_overload_dict.pa", ":4:1: error: Ambiguous function call 'f': it can mean f(@[2]int32), f(@[3]int32)."},
		{"error_1202_return_nothing_for_block.pa", ":1:24: error: Single-return function requires exactly one return expression."},
		{"error_1203_return_two_for_block.pa", ":1:37: error: Single-return function requires exactly one return expression."},
		{"error_1204_chained_varr_dict_overloads.pa", ":4:16: error: No function 'f' takes ([+]int32); there are f(@[\"\"]int32), f(@[\"\"]flo64)."},
		{"error_1205_borrow_field_shape.pa", ":2:29: error: array shape '@![4]int32' does not match the borrowed array type '@![3]int32'."},
		{"error_1206_borrow_field_no_addr.pa", ":2:29: error: an array given to a borrowed array"},
		{"error_1207_borrow_field_lit.pa", ":2:17: error: an array literal cannot be given to the borrowed array 'a'"},
		{"error_1208_dict_borrow_field_write.pa", ":2:17: error: cannot write through read-only pointer '@T'"},
		{"error_1209_borrow_field_len_addr.pa", ":2:36: error: 'a' is already a borrow"},
		{"error_1210_borrow_field_lit_shape.pa", ":2:34: error: array shape '[4]int32' does not match the borrowed array type '@[3]int32'."},
		{"error_1211_borrow_field_cycle.pa", ":2:1: error: type 'A' depends on itself: A -> B -> A."},
		{"error_1212_c_arr_ptr_field.pa", ":2:39: error: cannot access field 'rows' of 'CA': its type uses 'array'"},
		{"error_1213_c_arr_ptr_slots.pa", ":2:33: error: cannot access field 'slots' of 'CA': its type uses 'array'"},
		{"error_1228_varr_elems_owner.pa", "error: array shape '@![+]@P' does not match '[+]P'"},
		{"error_1229_varr_elems_owner_assign.pa", "error: array shape '@![+]@P' does not match '[+]P'"},
		{"error_1230_varr_elems_owner_arg.pa", "error: array shape '@![+]@P' does not match '[+]P'"},
		{"error_1231_varr_elems_owner_field.pa", "error: array shape '@![+]@P' does not match '[+]P'"},
		{"error_1232_varr_elems_owner_lit.pa", "error: array shape '@![+]@P' does not match '[+]P'"},
		{"error_1233_dict_elems_owner.pa", "error: array shape '@![\"\"]@P' does not match '[\"\"]P'"},
		{"error_1234_dict_elems_owner_rev.pa", "error: array shape '@![\"\"]P' does not match '[\"\"]@P'"},
		{"error_1235_dict_elems_permission.pa", "error: array shape '@![\"\"]@P' does not match '[\"\"]@!P'"},
		{"error_1236_dict_elem_rows.pa", "error: array shape '@[\"\"][3]int32' does not match '[\"\"][4]int32'"},
		{"error_1237_nested_varr_elems_owner.pa", "error: array shape '@![+][+]@P' does not match '[+][+]P'"},
		{"error_1238_arr_of_varr_elems_owner.pa", "error: array shape '[2][+]@P' does not match the borrowed array type '@![2][+]P'"},
		{"error_1239_unsized_elems_owner.pa", "error: array shape '@![2]@P' does not match '[?]P'"},
		{"error_1240_unsized_rows_var.pa", "error: array shape '@@[3]int32' does not match '[?]$[4]int32'"},
		{"error_1241_unsized_rows_ret.pa", "error: array shape '@@[3]int32' does not match '[?]$[4]int32'"},
		{"error_1242_func_cast_elems_owner.pa", "error: cannot cast '@func([3]@P)' to '@func([3]P)'"},
		{"error_1243_func_cast_varr_elems_owner.pa", "error: cannot cast '@func([+]@P)' to '@func([+]P)'"},
		{"error_1244_func_cast_owned_kind.pa", "error: cannot cast '@func(Q)' to '@func(P)'"},
		{"error_1245_null_owned_varr.pa", "error: cannot copy '[?]void' into '[+]int32'"},
		{"error_1246_null_owned_dict.pa", "error: cannot copy '[?]void' into '[\"\"]int32'"},
		{"error_1247_null_owned_arg.pa", "error: cannot convert '[?]void' to '[+]int32'"},
		{"error_1248_null_owned_ret.pa", "error: cannot convert '[?]void' to '[+]int32'"},
		{"error_1249_null_overload.pa", "error: Ambiguous function call 'f': it can mean f(@P), f(@[+]int32)."},
		{"error_1250_null_other_ptr.pa", "error: cannot convert '[?]void' to '@[+]int32'"},
		{"error_1251_null_other_ptr_arr.pa", "error: an array given to a borrowed array"},
		{"error_1252_nested_arr_borrow_varr.pa", "error: a borrow of a sized array"},
		{"error_1253_nested_arr_borrow_dict.pa", "error: a borrow of a sized array"},
		{"error_1254_nested_arr_borrow_var.pa", "error: a borrow of a sized array"},
		{"error_1255_nested_arr_borrow_field.pa", "error: a borrow of a sized array"},
		{"error_1256_nested_arr_borrow_param.pa", "error: a borrow of a sized array"},
		{"error_1257_nested_arr_borrow_borrowed.pa", "error: a borrow of a sized array"},
		{"error_1258_nested_arr_borrow_ptr_var.pa", "error: a borrow of a sized array"},
		{"error_1259_nested_arr_borrow_ptr_field.pa", "error: a borrow of a sized array"},
		{"error_1260_nested_arr_borrow_func_sig.pa", "error: a borrow of a sized array"},
		{"error_1261_embed_under_ptr_func_sig.pa", "error: '$' cannot be what '@' or '@!' points to."},
		{"error_1262_embed_under_ptr_param.pa", "error: '$' cannot be what '@' or '@!' points to."},
		{"error_1263_embed_under_ptr_elem.pa", "error: '$' cannot be what '@' or '@!' points to."},
		{"error_1264_embed_under_ptr_field.pa", "error: '$' cannot be what '@' or '@!' points to."},
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

TEST(sa_error, unsized_rows) {
	const vector<pair<string, string>> cases = {
		{"error_1265_embed_rows_borrowed_param.pa", "error: an array laid out in place without a length"},
		{"error_1266_embed_rows_var.pa", "error: an array laid out in place without a length"},
		{"error_1267_embed_rows_elem.pa", "error: an array laid out in place without a length"},
		{"error_1268_embed_rows_dict_elem.pa", "error: an array laid out in place without a length"},
		{"error_1269_embed_rows_field.pa", "error: an array laid out in place without a length"},
		{"error_1270_embed_rows_func_sig.pa", "error: an array laid out in place without a length"},
		{"error_1271_embed_rows_ret.pa", "error: an array laid out in place without a length"},
		{"error_1272_embed_rows_inner.pa", "error: an array laid out in place without a length"},
		{"error_1273_embed_rows_dict.pa", "error: an array laid out in place without a length"},
		{"error_1274_embed_rows_run_size.pa", "must be a compile-time constant."},
		{"error_1275_unsized_rows_write.pa", "cannot write through read-only pointer"},
		{"error_1276_embed_rows_const_mismatch.pa", "Embedded array inner size mismatch: expected 3, got 4."},
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

TEST(sa_error, row_layout_mismatch) {
	const vector<pair<string, string>> cases = {
		{"error_1277_row_layout_param.pa", "does not match '"},
		{"error_1278_row_layout_value_param.pa", "does not match '"},
		{"error_1279_row_layout_sized_row_param.pa", "does not match '"},
		{"error_1280_row_layout_assign.pa", "does not match '"},
		{"error_1281_row_layout_ptr_ptr.pa", "does not match '"},
		{"error_1282_row_layout_rows_to_embed.pa", "does not match '"},
		{"error_1283_row_layout_slot.pa", "does not match '"},
		{"error_1284_row_layout_field.pa", "does not match '"},
		{"error_1285_row_layout_borrow_var.pa", "does not match '"},
		{"error_1286_row_layout_run_size.pa", "does not match '"},
		{"error_1287_row_layout_varr.pa", "does not match '"},
		{"error_1288_row_layout_func_ptr.pa", "cannot convert '@func("},
		{"error_1289_row_layout_slots.pa", "does not match the borrowed array type"},
		{"error_1290_row_layout_slots_embed_size.pa", "does not match the borrowed array type"},
		{"error_1291_row_layout_slots_row_size.pa", "does not match the borrowed array type"},
		{"error_1292_row_layout_slots_ptr_elems.pa", "does not match the borrowed array type"},
		{"error_1293_row_layout_lent_rows.pa", "does not match the borrowed array type"},
		{"error_1294_row_layout_varr_elems.pa", "does not match '"},
		{"error_1295_row_layout_dict_elems.pa", "does not match '"},
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
