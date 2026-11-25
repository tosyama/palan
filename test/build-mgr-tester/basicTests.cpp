/// palan build manager tests
///
/// @file basicTests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include "../test-base/testBase.h"
#include "buildMgrTestUtil.h"

using namespace std;

TEST(build_mgr, helloworld) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/001_helloworld.pa");
	ASSERT_EQ(output, "Hello World!\n");
}

TEST(build_mgr, str_lit_bytes) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/401_str_lit_bytes.pa");
	ASSERT_EQ(output, "78 ff 79 00 7a 3\n5c 22 09 0d 01 e2 82 ac ff c3 a9 11\n");
}

TEST(build_mgr, code_point_literal) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/404_code_point_literal.pa");
	ASSERT_EQ(output, "233 128512 8364\n\xE2\x82\xAC\xC3\xA9 3\n");
}

TEST(build_mgr, explicit_target) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan --target x86_64-linux-gnu ../test/testdata/build-mgr/001_helloworld.pa");
	ASSERT_EQ(output, "Hello World!\n");
}

TEST(build_mgr, basic_expr) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/101_basic_expr.pa");
	ASSERT_EQ(output, "10\n30\n10\n100\n42\n100 100 50 50\n7\n1 1 0\n0 0 1\n");
}

TEST(build_mgr, func_def) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/102_func_def.pa");
	ASSERT_EQ(output, "7\n42\n15\n16\n99\n3 5\n5 10\n3 5\nhello\n3 5\n");
}

TEST(build_mgr, block) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/103_block.pa");
	ASSERT_EQ(output, "10 20\n10\n6\nhello from block\n14\n1 2 3\n1 2\n1\n");
}

TEST(build_mgr, const_decl_basic) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/104_const_decl_basic.pa");
	ASSERT_EQ(output, "100\n101\nhello\n");
}

TEST(build_mgr, fibonacci) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/006_fibonacci.pa");
	ASSERT_EQ(output, "55\n");
}

TEST(build_mgr, abs_gcd_lcm) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/007_abs_gcd_lcm.pa");
	ASSERT_EQ(output, "abs=42 gcd=4 lcm=12 abs=7 gcd=6 lcm=30\n");
}

TEST(build_mgr, register_spill) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/008_register_spill.pa");
	// Sub/Mul/Neg/Cmp dst spilled to stack when all callee-saved regs exhausted
	ASSERT_EQ(output, "12\n26\n2\n9\n");
}

TEST(build_mgr, seven_param_palan_func) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/009_seven_param_palan_func.pa");
	// 7th param passes on stack (RegAlloc lines 94, 155-158; X86CodeGen stack-arg path)
	ASSERT_EQ(output, "28\n");
}

TEST(build_mgr, divmod_rdx_conflict) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/010_divmod_rdx_conflict.pa");
	// vreg desired for %rdx whose live range spans a Div → conflict → callee-saved (RegAlloc lines 252-254)
	ASSERT_EQ(output, "17\n");
}

TEST(build_mgr, param_loop_call_arg) {
	cleanTestEnv();
	// Parameter n is used only as call arg inside loop (not Cmp operand), so
	// only the loop extension of its live range makes it cross the call.
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/014_param_loop_call_arg.pa");
	ASSERT_EQ(output, "42\n42\n42\n");
}

TEST(build_mgr, loop_live_range) {
	// Values defined before a loop must stay live until its backward jump:
	// a spilled param's stack slot and a call-arg-only local were both
	// clobbered on the second iteration.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/210_loop_live_range.pa");
	ASSERT_EQ(output, "f=60\nf=60\nf=60\nk=7\nk=7\nk=7\n");
}

TEST(build_mgr, rdx_divmod_conflict) {
	cleanTestEnv();
	// r (CallPln result, desired %rdx) spans a Div before use as 3rd printf arg;
	// without the conflict check idivq would clobber %rdx, giving "3 1" not "3 10".
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/015_rdx_divmod_conflict.pa");
	ASSERT_EQ(output, "3 10\n");
}

TEST(build_mgr, collatz) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/012_collatz.pa");
	ASSERT_EQ(output, "collatz(27) = 111\ncollatz(871) = 178\n");
}

TEST(build_mgr, fizzbuzz) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/013_fizzbuzz.pa");
	ASSERT_EQ(output, "1\n2\nFizz\n4\nBuzz\nFizz\n7\n8\nFizz\nBuzz\n11\nFizz\n13\n14\nFizzBuzz\n16\n17\nFizz\n19\nBuzz\n");
}

TEST(build_mgr, narrow_types) {
	cleanTestEnv();
	// Covers: var-decl without init, int8/int16 arithmetic (add/sub/mul/neg/cmp),
	// named single-return function with explicit bare return.
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/017_narrow_types.pa");
	ASSERT_EQ(output, "0\n13 7 -3 0\n107 93 700 -7 0\n42\n");
}

TEST(build_mgr, print_primes) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/016_print_primes.pa");
	ASSERT_EQ(output, "2\n3\n5\n7\n11\n13\n17\n19\n---\n1\n2\n4\n5\n7\n8\n10\n");
}

TEST(build_mgr, float_basics) {
	cleanTestEnv();
	// flo64/flo32 var-decl/init (float and int literals), float-to-int cast for output
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/018_float_basics.pa");
	ASSERT_EQ(output, "3\n2\n5\n3\n");
}

TEST(build_mgr, float_mixed_args) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/020_float_mixed_args.pa");
	// Case 1: interleaved int/float args (all registers)
	// Case 2: int overflow to stack, float in xmm
	// Case 3: float overflow to stack (9 floats)
	ASSERT_EQ(output,
		"10 3.500000 20\n"
		"1 2 3 4 5 6 7 0.500000\n"
		"1.000000 2.000000 3.000000 4.000000 5.000000 6.000000 7.000000 8.000000 9.000000\n");
}

TEST(build_mgr, int_to_float_implicit) {
	cleanTestEnv();
	// int32/int64 variables assigned to flo64 without explicit cast
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/022_int_to_float_implicit.pa");
	ASSERT_EQ(output, "7.000000\n-100.000000\n");
}

TEST(build_mgr, int_convert_extra) {
	cleanTestEnv();
	// int8/16 widening and int32/16->int8 narrowing
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/023_int_convert_extra.pa");
	ASSERT_EQ(output, "100 100 300 -56 44\n");
}

TEST(build_mgr, float32_convert) {
	cleanTestEnv();
	// flo64<->flo32, flo32->int, int->flo32, int8/16->flo32/64
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/024_float32_convert.pa");
	ASSERT_EQ(output, "3 3 3\n10.000000 5.000000 12.000000\n5.000000\n42.000000\n");
}

TEST(build_mgr, uint_convert) {
	cleanTestEnv();
	// uint widening/narrowing and uint->float implicit
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/025_uint_convert.pa");
	ASSERT_EQ(output, "200 200 200\n200\n200.000000 200.000000\n300.000000 70000.000000\n");
}

TEST(build_mgr, float_arith) {
	cleanTestEnv();
	// flo64 and flo32 arithmetic: +, -, *, /
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/026_float_arith.pa");
	ASSERT_EQ(output, "5.000000 1.000000 6.000000 1.500000\n5.000000 1.000000 6.000000 1.500000\n");
}

TEST(build_mgr, float_cmp) {
	cleanTestEnv();
	// flo64 comparison: <, >, ==
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/027_float_cmp.pa");
	ASSERT_EQ(output, "1\n0\n0\n");
}

// Float values in XMM registers: across calls, in loops, under register
// pressure, as params and multiple returns, and an address-taken local.
TEST(build_mgr, float_regs) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/382_float_regs.pa");
	ASSERT_EQ(output, "22.5\n38\n8.38411\n6\n7 12\n63\n3 0.75\n");
}

TEST(build_mgr, float_neg) {
	cleanTestEnv();
	// flo64 unary negation: literal neg and variable neg via ->
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/028_float_neg.pa");
	ASSERT_EQ(output, "-3.500000\n-2.000000\n");
}

TEST(build_mgr, float_newton) {
	cleanTestEnv();
	// Newton's method sqrt(2) using float arith, neg, if, while
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/029_float_newton.pa");
	ASSERT_EQ(output, "sqrt(2) = 1.414214\n");
}

TEST(build_mgr, float_int_mixed) {
	cleanTestEnv();
	// Covers both operand orders: usualArithConv's float tie-break is not
	// commutative in the code path taken (left vs right operand).
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/030_float_int_mixed.pa");
	ASSERT_EQ(output, "5.000000\n30.000000\n4.000000\n5.000000\n5.000000\n");
}

TEST(build_mgr, flo80) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/438_flo80.pa");
	ASSERT_EQ(output,
		"0.33333333333333333334 0.10000000000000000000 0.33333333333333333334 0.33333333333333331483\n"
		"0.50 -0.50 0.12\n"
		"1 1 0 0\n"
		"1.414214\n"
		"-3.0 250.0 -300.0 60000.0 -70000.0 4000000000.0 -5000000000.0 18446744073709551615.0 1.5 2.5\n"
		"-7 0 -7 0 -7 4294967295 -7 18446744073709551615 -7.75 -7.75\n"
		"2.00\n"
		"11.0 5 9.0\n"
		"16.875\n"
		"-4.50\n"
		"4.50\n"
		"1 2 3 4 5 6 1.00 8 0.50 10\n"
		"2.50 7 6.00\n"
		"5.00 5.00 8.00\n"
		"-4999999999.0\n"
		"-6.25 -15.50\n");
}

TEST(build_mgr, float32_cmp) {
	cleanTestEnv();
	// flo32 comparison: !=, ==, <, > (exercises ucomiss and setne)
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/031_float32_cmp.pa");
	ASSERT_EQ(output, "1\n0\n1\n0\n");
}

TEST(build_mgr, logical_ops) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/040_logical_ops.pa");
	ASSERT_EQ(output, "1\n0\n1\n1\n");
}

TEST(build_mgr, uint_literal) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/044_uint_literal.pa");
	ASSERT_EQ(output, "18446744073709551615\n");
}

TEST(build_mgr, type_alias) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/083_type_alias.pa");
	ASSERT_EQ(output, "5\n42\n");
}

TEST(build_mgr, toplevel_call_named_return_struct) {
	cleanTestEnv();
	// Regression: a top-level call with a struct-typed @!T named return crashed
	// palan-sa ("unknown prim type-name: Point") before signature struct-normalization.
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/129_toplevel_call_named_return_struct.pa");
	ASSERT_EQ(output, "20\n");
}

TEST(build_mgr, sign_cross_convert) {
	// Regression: emitConvert had no signed<->unsigned branches, so every case
	// below used to abort with rc=134. Covers the full cross-signedness matrix.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/141_sign_cross_convert.pa");
	ASSERT_EQ(output,
		"7 7 7 7\n"
		"255 255 255 65535 65535 4294967295\n"
		"65535 4294967295 18446744073709551615 4294967295 18446744073709551615 18446744073709551615\n"
		"-1 -1 -1 -1\n"
		"255 65535 4294967295 18446744073709551615\n"
		"-1 -1 -1 -1 -1 -1\n"
		"255 255 255 65535 65535 4294967295\n");
}

TEST(build_mgr, uint_narrow_arith) {
	// Regression: arithmetic mnemonic tables fell through to the 64-bit default
	// for Uint8/16/32 while sizedRegName already sized them narrower -- e.g.
	// `uint32 a + uint32 b` emitted `movl` into a 32-bit reg then `addq`, which
	// the assembler rejects.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/144_uint_narrow_arith.pa");
	ASSERT_EQ(output, "4 240 0\n4 65520 65476 0\n4 4294967280 4294967236 0\n");
}

TEST(build_mgr, uint_lit_narrow) {
	// Regression: a `u`-suffixed literal (lit-uint) deserialized with no type
	// field, so codegen always emitted a 64-bit MovImm regardless of the
	// declared width, and lowerVarDeclStmt never routed it through InitVar --
	// reassignment produced mismatched widths. Uint64 and unsuffixed literals
	// (lit-int, SA-retyped) never exposed this.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/145_uint_lit_narrow.pa");
	ASSERT_EQ(output, "44\n50\n4464\n12345\n14745824\n100\n");
}

TEST(build_mgr, bitwise_ops) {
	// `&` `|` `^` `~` were entirely unimplemented -- `&` parsed
	// but returned "not-impl" in SA, `|`/`^` weren't even lexed, `~` didn't exist.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/146_bitwise_ops.pa");
	ASSERT_EQ(output, "8 14 6 -13\n1\n61440 61503 63\n");
}

TEST(build_mgr, call_arg_in_func_body) {
	// Complements param_loop_call_arg's loop-region case with the straight-line
	// case: a genuine 2-cycle swap, and a 3-arg call whose bystander resolves
	// before the 2-cycle, exercising emitSafeRegMoves' cycle-break path when
	// the cycle isn't at index 0.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/154_call_arg_in_func_body.pa");
	ASSERT_EQ(output, "direct=7\nviacopy=7\ntwo=7 8\ng=22 11\ng4=1 3 2\n");
}

TEST(build_mgr, neg_lit_narrow_init) {
	// A negated literal now adopts the initializer's expected type instead of
	// always widening to int64/flo64 first and tripping narrowing diagnostics.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/155_neg_lit_narrow_init.pa");
	ASSERT_EQ(output, "-1 -1.500000 -200\n");
}

TEST(build_mgr, usual_arith_conv) {
	// Regression: a mixed signed/unsigned pair silently fell through
	// typeCompat's ExplicitCast with no convert node, in a binary operator
	// (m & big) and a call argument (uint32 -> int64 param).
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/158_usual_arith_conv.pa");
	ASSERT_EQ(output, "255\n4294967295\n");
}

TEST(build_mgr, bool_type)
{
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/214_bool.pa");
	ASSERT_EQ(output, "1 0 1 0\n0 1\n2 1 -1 -2\n1\n0 1\n3 1\n");
}

TEST(build_mgr, int_literal_range)
{
	// A uint64 literal above INT64_MAX used to crash codegen's stoll.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/194_int_literal_range.pa");
	ASSERT_EQ(output, "18446744073709551600 -128 127\n");
}

TEST(build_mgr, hex_literal)
{
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/469_hex_literal.pa");
	ASSERT_EQ(output, "-1 -1 4294967295 4294967295 -1 18446744073709551615 -128\n"
		"-2147483648 2147483647 -128 -2 -3\neq\neq\neq\n-16777216 -1\n"
		"-16777216 4278190335 16 255 -1\n-16776962 -16776961\neq\n");
}

TEST(build_mgr, int64_imm_range)
{
	// mov to memory only encodes a sign-extended imm32, so wider values
	// failed to assemble.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/195_int64_imm_range.pa");
	ASSERT_EQ(output, "3000000000 -9223372036854775807 9223372036854775808\n-3000000000\n");
}

TEST(build_mgr, unsigned_cmp)
{
	// Unsigned comparisons used signed setCC, so a value with its top bit set
	// compared as negative.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/196_unsigned_cmp.pa");
	ASSERT_EQ(output, "001101\n001101\n001101\n001101\n110001\nif ok\n6\n");
}

TEST(build_mgr, stack_arg_narrow_int)
{
	// Stack-passed arguments narrower than 64 bits were stored with movq,
	// which fails to assemble for a sized register source.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/197_stack_arg_narrow_int.pa");
	ASSERT_EQ(output, "1 2 3 4 5 -4 4000000001\n"
	                  "1 2 3 4 5 -299 -69999 -5\n"
	                  "1 2 3 4 5 -300 -70000 4000000000\n"
	                  "-699990393\n-700000493\n");
}

TEST(build_mgr, mul_8bit)
{
	// 8-bit multiply was emitted as imulq on byte registers, which fails to assemble.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/198_mul_8bit.pa");
	ASSERT_EQ(output, "-15 44 -128\n144 255\n44 144\n");
}

TEST(build_mgr, divmod_width_sign)
{
	// Div/Mod always used movq+cqto+idivq: narrower operands failed to assemble
	// and unsigned operands were divided as signed.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/199_divmod_width_sign.pa");
	ASSERT_EQ(output, "-3 -1 -128 0\n-4285 -5\n-666666666 -2\n-1285714285714285714 -2\n"
	                  "35 5 9285 5\n571428571 3\n1844674407370955160 0\n-128 500000000\n");
}

TEST(build_mgr, pln_float_call)
{
	// Palan calls passed every arg in intArgs[j] and returned in %rax regardless
	// of type, so float params/returns failed to assemble.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/200_pln_float_call.pa");
	ASSERT_EQ(output, "2.000000\n4.500000 2.500000\n21.750000\n"
	                  "9.000000 10.000000 600 700 800\n3655.000000\n"
	                  "x=1.250000\n2.500000\n3 1.500000 2 1.000000\n0.500000\n");
}

TEST(build_mgr, multiret_recv_swap)
{
	// Results were copied to their dsts in reverse order, clobbering a return
	// register still holding a later-copied result when dsts sit in arg registers.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/202_multiret_recv_swap.pa");
	ASSERT_EQ(output, "2 1\n3 1 2\n20 2.500000 10\n");
}

TEST(build_mgr, int_lit_float_ctx) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/203_int_lit_float_ctx.pa");
	ASSERT_EQ(output, "2.0 2.5 3.0\n3.0 2.0\n7.0\n");
}

TEST(build_mgr, mixed_type_var_decl) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/206_mixed_type_var_decl.pa");
	ASSERT_EQ(output, "7 5000000000\n3\n1 4\n5 6\n");
}

TEST(build_mgr, uint64_float_convert) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/209_uint64_float_convert.pa");
	ASSERT_EQ(output,
		"0 9007199254740992 9223372036854775808 18446744073709551616 9223372036854777856\n"
		"0 9007199254740992 9223372036854775808 18446744073709551616\n"
		"18446744073709551616 9223372036854775808 9223372036854775808.0\n"
		"18000000000000000000 12345 9223372036854775808\n"
		"18446744073709549568\n");
}

TEST(build_mgr, char_literal) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/217_char_literal.pa");
	ASSERT_EQ(output, "quit 113\n98 10 92\n");
}

TEST(build_mgr, const_typed_context) {
	// A const is typed by its reference context, like the literal it names.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/220_const_typed_context.pa");
	ASSERT_EQ(output, "x 22 40 23 20.0 10.0\n");
}

TEST(build_mgr, named_ret_init) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/240_named_ret_init.pa");
	ASSERT_EQ(output, "5 21 7 3.0\n3 4\n");
}

TEST(build_mgr, tapple_assign) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/241_tapple_assign.pa");
	ASSERT_EQ(output, "4 2\n2 1\n2 2\n");
}

TEST(build_mgr, tapple_decl_convert) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/244_tapple_decl_convert.pa");
	ASSERT_EQ(output, "4 2.0\n");
}

TEST(build_mgr, bool_cmp_result) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/242_bool_cmp_result.pa");
	ASSERT_EQ(output, "1 1 0\n0\n6\n1 2 0\n1 0 1\n");
}

TEST(build_mgr, bool_literal) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/243_bool_literal.pa");
	ASSERT_EQ(output, "1 0 0 1\n1\n1 2 1 -1\n1 1\n");
}

TEST(build_mgr, enum_type) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/258_enum.pa");
	ASSERT_EQ(output, "0 6 6 1\n-3 5000000000 5 11\n6 -3 3\n6 5 5\n");
}

TEST(build_mgr, sizeof) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/261_sizeof.pa");
	ASSERT_EQ(output, "56 32 16\n");
}

TEST(build_mgr, alias_cast) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/267_alias_cast.pa");
	ASSERT_EQ(output, "44 2 1\n");
}

TEST(build_mgr, float_exponent_no_point) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/271_float_exponent_no_point.pa");
	ASSERT_EQ(output, "10000000000.0 0.001 200000.0 600.0\n");
}

TEST(build_mgr, as_cast) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/288_as_cast.pa");
	ASSERT_EQ(output, "2 1\nok\n7 1\n9\n40 0\n");
}

TEST(build_mgr, const_expr) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/291_const_expr.pa");
	ASSERT_EQ(output, "28 5 4.5\n");
}

TEST(build_mgr, shift) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/293_shift.pa");
	ASSERT_EQ(output, "2147483644 144 2 2\n15 40\n-1 -3 -56 -56\n3 5 116\n-64 0 -2305843009213693952\n");
}

TEST(build_mgr, float_neg_on_stack) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand("bin/palan ../test/testdata/build-mgr/390_float_neg_on_stack.pa"),
		"1\n2\n3\n4\n5\n6\n-2.500000\n");
}

// Any number of arguments, string literals for borrowed arrays, through a
// function pointer and after `->`; a function taking no variadic argument is
// chosen when the other one does not fit.
TEST(build_mgr, variadic) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/406_variadic.pa");
	ASSERT_EQ(output, "n=3 11\nn=0 10\n0:a 1:bc 2:def \n\nn=2 102\nn=1 15\n1 2\n");
}
