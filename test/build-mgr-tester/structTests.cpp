/// palan build manager tests: structs
///
/// @file structTests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include "../test-base/testBase.h"
#include "buildMgrTestUtil.h"

using namespace std;

TEST(build_mgr, struct_basic) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/050_struct_basic.pa");
	ASSERT_EQ(output, "10 20\n");
}

TEST(build_mgr, struct_c_abi) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/051_struct_c_abi.pa");
	ASSERT_EQ(output, "42 100\n");
}

TEST(build_mgr, struct_float) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/052_struct_float.pa");
	ASSERT_EQ(output, "1.5 2.5\n");
}

TEST(build_mgr, struct_scope) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/053_struct_scope.pa");
	ASSERT_EQ(output, "42\n");
}

TEST(build_mgr, struct_multi) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/054_struct_multi.pa");
	ASSERT_EQ(output, "1 2 3 4\n");
}

TEST(build_mgr, owned_struct_alloc) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/055_owned_struct_alloc.pa");
	ASSERT_EQ(output, "10 20 30 40\n");
}

TEST(build_mgr, embed_struct_field) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/057_embed_struct_field.pa");
	ASSERT_EQ(output, "10 20\n");
}

TEST(build_mgr, struct_func_param) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/060_struct_func_param.pa");
	ASSERT_EQ(output, "42\n");
}

TEST(build_mgr, owned_struct_arr) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/061_owned_struct_arr.pa");
	ASSERT_EQ(output, "5 6 7 8\n");
}

TEST(build_mgr, embed_struct_arr) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/062_embed_struct_arr.pa");
	ASSERT_EQ(output, "30 40\n");
}

TEST(build_mgr, at_struct_arr) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/063_at_struct_arr.pa");
	ASSERT_EQ(output, "99 77\n");
}

TEST(build_mgr, at_bang_struct_arr) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/064_at_bang_struct_arr.pa");
	ASSERT_EQ(output, "42 20\n");
}

TEST(build_mgr, owned_prim_arr_field) {
	// Bucket { [3]int64 vals; }, declaration-only: proves the field's row is
	// built by __pln_init_Bucket and runs without crashing.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/069_owned_prim_arr_field.pa");
	ASSERT_EQ(output, "ok\n");
}

TEST(build_mgr, owned_struct_arr_field) {
	// Cluster { [4]Point pts; }, declaration-only: proves __pln_init_Cluster's
	// call to the row's element helpers resolves.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/070_owned_struct_arr_field.pa");
	ASSERT_EQ(output, "ok\n");
}

TEST(build_mgr, embed_prim_arr_field_access) {
	// type Buf { [4]$int64 data; }; Buf buf; 10->buf.data[0]; 20->buf.data[1];
	// 1D primitive embedded array field element access.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/071_embed_prim_arr_field_access.pa");
	ASSERT_EQ(output, "10 20\n");
}

TEST(build_mgr, embed_struct_arr_field_access) {
	// Regression: DerefLoad used to read the embedded struct's raw bytes as a
	// stored pointer instead of computing poly_ptr+offset, segfaulting at runtime.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/072_embed_struct_arr_field_access.pa");
	ASSERT_EQ(output, "10 20\n");
}

TEST(build_mgr, owned_prim_arr_field_access) {
	// type Bucket { [3]int64 vals; }; Bucket b; element read/write access.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/073_owned_prim_arr_field_access.pa");
	ASSERT_EQ(output, "1 2 3\n");
}

TEST(build_mgr, owned_struct_arr_field_access) {
	// type Point{...}; type Cluster { [4]Point pts; }; Cluster c; element access.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/074_owned_struct_arr_field_access.pa");
	ASSERT_EQ(output, "5 6\n");
}

TEST(build_mgr, embed_ptr_arr_field_access) {
	// Regression: DerefLoad on the freshly-calloc'd "nodes" field read back 0,
	// collapsing `p -> r.nodes[0];`'s store address to NULL.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/075_embed_ptr_arr_field_access.pa");
	ASSERT_EQ(output, "99\n");
}

TEST(build_mgr, owned_struct_arr_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_owned_struct_arr_mtrace_bin "
		"../test/testdata/build-mgr/065_owned_struct_arr_mtrace.pa"), "");

	string traceFile = "/tmp/palan_owned_struct_arr_mtrace.log";
	execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_owned_struct_arr_mtrace_bin");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	// [2]Point pts: 1 malloc (ptr array) + 2 calloc (Point elements) = 3 allocs
	EXPECT_EQ(allocs, 3) << "expected 3 allocs for [2]Point pts, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, embed_struct_arr_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_embed_struct_arr_mtrace_bin "
		"../test/testdata/build-mgr/066_embed_struct_arr_mtrace.pa"), "");

	string traceFile = "/tmp/palan_embed_struct_arr_mtrace.log";
	execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_embed_struct_arr_mtrace_bin");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	// [3]$Point pts: 1 malloc (contiguous block n*stride), 1 free
	EXPECT_EQ(allocs, 1) << "expected 1 malloc for [3]$Point pts, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, at_struct_arr_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_at_struct_arr_mtrace_bin "
		"../test/testdata/build-mgr/067_at_struct_arr_mtrace.pa"), "");

	string traceFile = "/tmp/palan_at_struct_arr_mtrace.log";
	execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_at_struct_arr_mtrace_bin");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	// [4]@Point rpts: 1 malloc (ptr array), 1 free; Point p is outside mtrace scope
	EXPECT_EQ(allocs, 1) << "expected 1 malloc for [4]@Point rpts, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, at_bang_struct_arr_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_at_bang_struct_arr_mtrace_bin "
		"../test/testdata/build-mgr/068_at_bang_struct_arr_mtrace.pa"), "");

	string traceFile = "/tmp/palan_at_bang_struct_arr_mtrace.log";
	execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_at_bang_struct_arr_mtrace_bin");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	// [4]@!Point wpts: 1 malloc (ptr array), 1 free; Point p is outside mtrace scope
	EXPECT_EQ(allocs, 1) << "expected 1 malloc for [4]@!Point wpts, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, embed_prim_arr_field_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_embed_prim_arr_field_mtrace_bin "
		"../test/testdata/build-mgr/076_embed_prim_arr_field_mtrace.pa"), "");

	string traceFile = "/tmp/palan_embed_prim_arr_field_mtrace.log";
	execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_embed_prim_arr_field_mtrace_bin");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	// Buf { [4]$int64 data; }: 1 calloc (Buf itself; data embedded in same block)
	EXPECT_EQ(allocs, 1) << "expected 1 alloc for Buf { [4]$int64 data; }, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, embed_struct_arr_field_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_embed_struct_arr_field_mtrace_bin "
		"../test/testdata/build-mgr/077_embed_struct_arr_field_mtrace.pa"), "");

	string traceFile = "/tmp/palan_embed_struct_arr_field_mtrace.log";
	execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_embed_struct_arr_field_mtrace_bin");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	// Polygon { [4]$Point pts; }: 1 calloc (Polygon itself; pts embedded in same block)
	EXPECT_EQ(allocs, 1) << "expected 1 alloc for Polygon { [4]$Point pts; }, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, embed_ptr_arr_field_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_embed_ptr_arr_field_mtrace_bin "
		"../test/testdata/build-mgr/078_embed_ptr_arr_field_mtrace.pa"), "");

	string traceFile = "/tmp/palan_embed_ptr_arr_field_mtrace.log";
	execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_embed_ptr_arr_field_mtrace_bin");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	// Ring { [4]@!Point nodes; }: 1 calloc (Ring itself; nodes are embedded ptr slots);
	// Point p is outside mtrace scope
	EXPECT_EQ(allocs, 1) << "expected 1 alloc for Ring { [4]@!Point nodes; }, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, owned_prim_arr_field_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_owned_prim_arr_field_mtrace_bin "
		"../test/testdata/build-mgr/079_owned_prim_arr_field_mtrace.pa"), "");

	string traceFile = "/tmp/palan_owned_prim_arr_field_mtrace.log";
	execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_owned_prim_arr_field_mtrace_bin");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	// Bucket { [3]int64 vals; }: Bucket calloc(1) + the field's row malloc(1) = 2
	EXPECT_EQ(allocs, 2) << "expected 2 allocs for Bucket { [3]int64 vals; }, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, owned_struct_arr_field_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_owned_struct_arr_field_mtrace_bin "
		"../test/testdata/build-mgr/080_owned_struct_arr_field_mtrace.pa"), "");

	string traceFile = "/tmp/palan_owned_struct_arr_field_mtrace.log";
	execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_owned_struct_arr_field_mtrace_bin");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	// Cluster { [2]Point pts; }: Cluster calloc(1) + the field's row:
	// ptr-array malloc(1) + 2 element callocs = 4
	EXPECT_EQ(allocs, 4) << "expected 4 allocs for Cluster { [2]Point pts; }, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, mixed_owned_and_owned_arr_field_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_mixed_owned_and_owned_arr_field_mtrace_bin "
		"../test/testdata/build-mgr/081_mixed_owned_and_owned_arr_field_mtrace.pa"), "");

	string traceFile = "/tmp/palan_mixed_owned_and_owned_arr_field_mtrace.log";
	execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_mixed_owned_and_owned_arr_field_mtrace_bin");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	// Mixed { Point single; [2]Point arr; }: Mixed calloc(1) + single-field
	// calloc(1) + the arr field's row:
	// ptr-array malloc(1) + 2 element callocs = 5
	EXPECT_EQ(allocs, 5) << "expected 5 allocs for Mixed { Point single; [2]Point arr; }, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, owned_and_embed_arr_mixed_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_owned_and_embed_arr_mixed_mtrace_bin "
		"../test/testdata/build-mgr/082_owned_and_embed_arr_mixed_mtrace.pa"), "");

	string traceFile = "/tmp/palan_owned_and_embed_arr_mixed_mtrace.log";
	execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_owned_and_embed_arr_mixed_mtrace_bin");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	// 6 = Widget calloc(1) + owned_pts arr-alloc(1 ptr-array + 2 elements) +
	// owned_vals/owned_more (shared prim-int64 allocator, dedup'd = 2 more).
	// tris/slots are embedded in Widget's own calloc block: no extra allocs.
	EXPECT_EQ(allocs, 6) << "expected 6 allocs for Widget with mixed owned/embed arr fields, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, struct_ret_div) {
	// SysV struct-by-value return through real glibc functions: div_t
	// (1 eightbyte) and ldiv_t/lldiv_t (2 eightbytes, both INTEGER class).
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/160_struct_ret_div.pa");
	ASSERT_EQ(output,
		"3 1\n"
		"3 1\n"
		"3 1\n");
}

TEST(build_mgr, struct_ret_div_mtrace) {
	// The calloc backing `div_t d` must be freed exactly once at scope exit
	// for a struct-ret'd C-function call.
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_struct_ret_div_mtrace_bin "
		"../test/testdata/build-mgr/161_struct_ret_div_mtrace.pa"), "");

	string traceFile = "/tmp/palan_struct_ret_div_mtrace.log";
	execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_struct_ret_div_mtrace_bin");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	EXPECT_EQ(allocs, 1) << "expected 1 alloc for div_t d, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, struct_field_type_alias)
{
	// Owned alias-typed fields reach the generated allocator module under
	// their resolved names.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/191_struct_field_type_alias.pa");
	ASSERT_EQ(output, "3 4 5 6 7\n");
}

TEST(build_mgr, owned_struct_arr_owned_field_mtrace) {
	// Moving each element into the array nulls the loop-local source, so the
	// generated __pln_free_L must accept NULL.
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_owned_struct_arr_owned_field_mtrace_bin "
		"../test/testdata/build-mgr/192_owned_struct_arr_owned_field_mtrace.pa"), "");

	string traceFile = "/tmp/palan_owned_struct_arr_owned_field_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_owned_struct_arr_owned_field_mtrace_bin");
	ASSERT_EQ(output, "5\n");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	// [2]L ls: 1 ptr array + 2 x (L + owned P) = 5 allocs
	EXPECT_EQ(allocs, 5) << "expected 5 allocs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, var_init_copy) {
	// A variable initialized from another must not share its storage.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/218_var_init_copy.pa");
	ASSERT_EQ(output, "3 2 1.5 2.5 10 20 6\n");
}

TEST(build_mgr, struct_borrow) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/221_struct_borrow.pa");
	ASSERT_EQ(output, "5 1 7 2 3 2\n2\n");
}

TEST(build_mgr, struct_arr_2d) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/222_struct_arr_2d.pa");
	ASSERT_EQ(output, "1 112 1\n3 4\n");
}

TEST(build_mgr, struct_arr_2d_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_struct_arr_2d_mtrace_bin "
		"../test/testdata/build-mgr/223_struct_arr_2d_mtrace.pa"), "");

	string traceFile = "/tmp/palan_struct_arr_2d_mtrace.log";
	execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_struct_arr_2d_mtrace_bin");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	// [2][3]Line: 1 outer + 2 rows + 6 Line elements + 6 owned Point fields
	EXPECT_EQ(allocs, 15) << "expected 15 allocs for [2][3]Line, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, struct_arr_lit) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/224_struct_arr_lit.pa");
	ASSERT_EQ(output, "3 1 2 1\n6 5 7 8\n1 2 3 9 4 | 10 11 6 7 5\n");
}

TEST(build_mgr, struct_arr_lit_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_struct_arr_lit_mtrace_bin "
		"../test/testdata/build-mgr/225_struct_arr_lit_mtrace.pa"), "");

	string traceFile = "/tmp/palan_struct_arr_lit_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_struct_arr_lit_mtrace_bin");
	ASSERT_EQ(output, "2 5\n");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	// [2]Line: 1 pointer array + 2 Line elements + 2 owned Point fields
	EXPECT_EQ(allocs, 5) << "expected 5 allocs for [2]Line, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, struct_arr_borrow) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/226_struct_arr_borrow.pa");
	ASSERT_EQ(output, "9\n(30,4) 10 30 50\n(50,6)\n");
}

TEST(build_mgr, embed_struct_arr_borrow) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/227_embed_struct_arr_borrow.pa");
	ASSERT_EQ(output, "33 (2,20)(1,10)\n");
}

TEST(build_mgr, embed_struct_arr_2d) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/230_embed_struct_arr_2d.pa");
	ASSERT_EQ(output, "(8,9) 66 (110,11)(2,42) 5\n");
}

TEST(build_mgr, embed_struct_arr_2d_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_embed_struct_arr_2d_mtrace_bin "
		"../test/testdata/build-mgr/231_embed_struct_arr_2d_mtrace.pa"), "");

	string traceFile = "/tmp/palan_embed_struct_arr_2d_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_embed_struct_arr_2d_mtrace_bin");
	ASSERT_EQ(output, "5\n");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	// [2][3]$Line: 1 outer + 2 rows; the Lines sit in the rows
	EXPECT_EQ(allocs, 3) << "expected 3 allocs for [2][3]$Line, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, copy_struct) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/233_copy_struct.pa");
	ASSERT_EQ(output, "(1,2)(0,2) 12 0\n(0,2) 3 4 5 8\n3 5\n");
}

TEST(build_mgr, copy_struct_arr) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/234_copy_struct_arr.pa");
	ASSERT_EQ(output, "3 2 0\n2 3 1\n2 1\n5 6\n");
}

TEST(build_mgr, copy_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_copy_mtrace_bin "
		"../test/testdata/build-mgr/235_copy_mtrace.pa"), "");

	string traceFile = "/tmp/palan_copy_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_copy_mtrace_bin");
	ASSERT_EQ(output, "7\n");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	// Poly: itself + c + w + v (1 + 2 Points) = 6; 2 Polys, 2 [2]Poly (1 + 2 * 6),
	// 2 [2][2]Point (1 + 2 rows of 1 + 2 Points). Copies allocate nothing.
	EXPECT_EQ(allocs, 52) << "expected 52 allocs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, ret_value_across_call) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/236_ret_value_across_call.pa");
	ASSERT_EQ(output, "a=7\nt=6\n9 7\n6\n");
}

TEST(build_mgr, ret_after_free) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/237_ret_after_free.pa");
	ASSERT_EQ(output, "6 6 14 2.0 132 -1\n");
}

TEST(build_mgr, ret_after_free_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_ret_after_free_bin "
		"../test/testdata/build-mgr/238_ret_after_free_mtrace.pa"), "");

	string traceFile = "/tmp/palan_ret_after_free.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_ret_after_free_bin");
	ASSERT_EQ(output, "6 6 14 2.0 132 -1\n");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	// 1 + 1 + Poly (2) + 1 + findFirst(10): d + 3 w + findFirst(30): d + 4 w
	EXPECT_EQ(allocs, 14) << "expected 14 allocs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, tapple_struct_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_tapple_struct_mtrace_bin "
		"../test/testdata/build-mgr/245_tapple_struct_mtrace.pa"), "");

	string traceFile = "/tmp/palan_tapple_struct_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_tapple_struct_mtrace_bin");
	ASSERT_EQ(output, "3 6 4\n6 4 12\n5 10 6\n8 6 18\n7 8\n");

	string catResult = execTestCommand("cat " + traceFile);
	int allocs = 0, frees = 0;
	size_t pos = 0;
	while ((pos = catResult.find("@ ", pos)) != string::npos) {
		size_t eol = catResult.find('\n', pos);
		string line = catResult.substr(pos, eol - pos);
		bool fromSharedLib = line.find(".so.") != string::npos;
		if (!fromSharedLib && line.find(" + ") != string::npos) allocs++;
		if (!fromSharedLib && line.find(" - ") != string::npos) frees++;
		pos = (eol == string::npos) ? string::npos : eol + 1;
	}
	EXPECT_EQ(allocs, 11) << "expected 11 allocs for the received and target structs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, field_transfer_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_field_transfer_mtrace_bin "
		"../test/testdata/build-mgr/246_field_transfer_mtrace.pa"), "");

	string traceFile = "/tmp/palan_field_transfer_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_field_transfer_mtrace_bin");
	ASSERT_EQ(output, "3 5 7 9\n");

	string catResult = execTestCommand("cat " + traceFile);
	int allocs = 0, frees = 0;
	size_t pos = 0;
	while ((pos = catResult.find("@ ", pos)) != string::npos) {
		size_t eol = catResult.find('\n', pos);
		string line = catResult.substr(pos, eol - pos);
		bool fromSharedLib = line.find(".so.") != string::npos;
		if (!fromSharedLib && line.find(" + ") != string::npos) allocs++;
		if (!fromSharedLib && line.find(" - ") != string::npos) frees++;
		pos = (eol == string::npos) ? string::npos : eol + 1;
	}
	// The fields' own allocations are released when '->>' replaces them.
	EXPECT_EQ(allocs, 15) << "expected 15 allocs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, transfer_from_slot_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_transfer_from_slot_mtrace_bin "
		"../test/testdata/build-mgr/249_transfer_from_slot_mtrace.pa"), "");

	string traceFile = "/tmp/palan_transfer_from_slot_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_transfer_from_slot_mtrace_bin");
	ASSERT_EQ(output, "5 7 9 3\n");

	string catResult = execTestCommand("cat " + traceFile);
	int allocs = 0, frees = 0;
	size_t pos = 0;
	while ((pos = catResult.find("@ ", pos)) != string::npos) {
		size_t eol = catResult.find('\n', pos);
		string line = catResult.substr(pos, eol - pos);
		bool fromSharedLib = line.find(".so.") != string::npos;
		if (!fromSharedLib && line.find(" + ") != string::npos) allocs++;
		if (!fromSharedLib && line.find(" - ") != string::npos) frees++;
		pos = (eol == string::npos) ? string::npos : eol + 1;
	}
	// The source slots are nulled, so their owners' frees skip what was given away.
	EXPECT_EQ(allocs, 19) << "expected 19 allocs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, elem_transfer_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_elem_transfer_mtrace_bin "
		"../test/testdata/build-mgr/250_elem_transfer_mtrace.pa"), "");

	string traceFile = "/tmp/palan_elem_transfer_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_elem_transfer_mtrace_bin");
	ASSERT_EQ(output, "3 5 7 9\n");

	string catResult = execTestCommand("cat " + traceFile);
	int allocs = 0, frees = 0;
	size_t pos = 0;
	while ((pos = catResult.find("@ ", pos)) != string::npos) {
		size_t eol = catResult.find('\n', pos);
		string line = catResult.substr(pos, eol - pos);
		bool fromSharedLib = line.find(".so.") != string::npos;
		if (!fromSharedLib && line.find(" + ") != string::npos) allocs++;
		if (!fromSharedLib && line.find(" - ") != string::npos) frees++;
		pos = (eol == string::npos) ? string::npos : eol + 1;
	}
	// What each element or row held is released when '->>' replaces it.
	EXPECT_EQ(allocs, 28) << "expected 28 allocs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, struct_ret_copy_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_struct_ret_copy_mtrace_bin "
		"../test/testdata/build-mgr/247_struct_ret_copy_mtrace.pa"), "");

	string traceFile = "/tmp/palan_struct_ret_copy_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_struct_ret_copy_mtrace_bin");
	ASSERT_EQ(output, "3 5 3 3 3 3\n");

	string catResult = execTestCommand("cat " + traceFile);
	int allocs = 0, frees = 0;
	size_t pos = 0;
	while ((pos = catResult.find("@ ", pos)) != string::npos) {
		size_t eol = catResult.find('\n', pos);
		string line = catResult.substr(pos, eol - pos);
		bool fromSharedLib = line.find(".so.") != string::npos;
		if (!fromSharedLib && line.find(" + ") != string::npos) allocs++;
		if (!fromSharedLib && line.find(" - ") != string::npos) frees++;
		pos = (eol == string::npos) ? string::npos : eol + 1;
	}
	EXPECT_EQ(allocs, 10) << "expected 10 allocs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

// A returned struct passed as an argument is released right after the call,
// including where the call runs conditionally ('&&', a loop condition).
TEST(build_mgr, struct_ret_temp_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_struct_ret_temp_mtrace_bin "
		"../test/testdata/build-mgr/248_struct_ret_temp_mtrace.pa"), "");

	string traceFile = "/tmp/palan_struct_ret_temp_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_struct_ret_temp_mtrace_bin");
	ASSERT_EQ(output, "3 4 5 6\n103 103\nyes\n3 3 13\n");

	string catResult = execTestCommand("cat " + traceFile);
	int allocs = 0, frees = 0;
	size_t pos = 0;
	while ((pos = catResult.find("@ ", pos)) != string::npos) {
		size_t eol = catResult.find('\n', pos);
		string line = catResult.substr(pos, eol - pos);
		bool fromSharedLib = line.find(".so.") != string::npos;
		if (!fromSharedLib && line.find(" + ") != string::npos) allocs++;
		if (!fromSharedLib && line.find(" - ") != string::npos) frees++;
		pos = (eol == string::npos) ? string::npos : eol + 1;
	}
	EXPECT_EQ(allocs, 3) << "expected 3 allocs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, unsized_struct_arr) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/251_unsized_struct_arr.pa");
	ASSERT_EQ(output, "15 6 7 13\n");
}

TEST(build_mgr, assign_chain) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/256_assign_chain.pa");
	// `c[2] + 1 -> i -> c[i]` indexes with the i just stored.
	ASSERT_EQ(output, "7 7\n3 3 3 3 3\n9 9 9 9\n4 4\n");
}

TEST(build_mgr, assign_chain_transfer_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_assign_chain_transfer_mtrace_bin "
		"../test/testdata/build-mgr/257_assign_chain_transfer_mtrace.pa"), "");

	string traceFile = "/tmp/palan_assign_chain_transfer_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_assign_chain_transfer_mtrace_bin");
	ASSERT_EQ(output, "4 6 5\n");

	string catResult = execTestCommand("cat " + traceFile);
	int allocs = 0, frees = 0;
	size_t pos = 0;
	while ((pos = catResult.find("@ ", pos)) != string::npos) {
		size_t eol = catResult.find('\n', pos);
		string line = catResult.substr(pos, eol - pos);
		bool fromSharedLib = line.find(".so.") != string::npos;
		if (!fromSharedLib && line.find(" + ") != string::npos) allocs++;
		if (!fromSharedLib && line.find(" - ") != string::npos) frees++;
		pos = (eol == string::npos) ? string::npos : eol + 1;
	}
	EXPECT_EQ(allocs, 14) << "expected 14 allocs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, struct_field_const_size) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/265_struct_field_const_size.pa");
	ASSERT_EQ(output, "2 20 200 48\n");
}

TEST(build_mgr, struct_ptr_elem_copy) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/268_struct_ptr_elem_copy.pa");
	ASSERT_EQ(output, "1 2\n5 6\n0 11\n");
}

TEST(build_mgr, struct_lit_init_assign) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/269_struct_lit_init_assign.pa");
	ASSERT_EQ(output, "1 2 3 / 4 5 6 / 1 2 7\n2 1 / 9 3\n5 1\n5 4\n3 4\n");
}

// Struct-type named returns are allocated at function entry and handed to the
// caller on every path, including an early return.
TEST(build_mgr, named_ret_struct_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_named_ret_struct_mtrace_bin "
		"../test/testdata/build-mgr/270_named_ret_struct_mtrace.pa"), "");

	string traceFile = "/tmp/palan_named_ret_struct_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_named_ret_struct_mtrace_bin");
	ASSERT_EQ(output, "3 4\n4 8 12\n5 50 6\n1 9\n1 8\n");

	string catResult = execTestCommand("cat " + traceFile);
	int allocs = 0, frees = 0;
	size_t pos = 0;
	while ((pos = catResult.find("@ ", pos)) != string::npos) {
		size_t eol = catResult.find('\n', pos);
		string line = catResult.substr(pos, eol - pos);
		bool fromSharedLib = line.find(".so.") != string::npos;
		if (!fromSharedLib && line.find(" + ") != string::npos) allocs++;
		if (!fromSharedLib && line.find(" - ") != string::npos) frees++;
		pos = (eol == string::npos) ? string::npos : eol + 1;
	}
	EXPECT_EQ(allocs, 5) << "expected 5 allocs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, struct_init_from_call_mtrace)
{
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_struct_init_from_call_bin "
		"../test/testdata/build-mgr/272_struct_init_from_call_mtrace.pa"), "");

	string traceFile = "/tmp/palan_struct_init_from_call_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_struct_init_from_call_bin");
	// s copies p before bump(@!p) runs.
	ASSERT_EQ(output, "3 4\n4 8 12 3\n3 4\n1 2 3 \n");

	string catResult = execTestCommand("cat " + traceFile);
	int allocs = 0, frees = 0;
	size_t pos = 0;
	while ((pos = catResult.find("@ ", pos)) != string::npos) {
		size_t eol = catResult.find('\n', pos);
		string line = catResult.substr(pos, eol - pos);
		bool fromSharedLib = line.find(".so.") != string::npos;
		if (!fromSharedLib && line.find(" + ") != string::npos) allocs++;
		if (!fromSharedLib && line.find(" - ") != string::npos) frees++;
		pos = (eol == string::npos) ? string::npos : eol + 1;
	}
	EXPECT_EQ(allocs, 9) << "expected 9 allocs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, return_struct_lit_mtrace)
{
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_return_struct_lit_bin "
		"../test/testdata/build-mgr/273_return_struct_lit_mtrace.pa"), "");

	string traceFile = "/tmp/palan_return_struct_lit_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_return_struct_lit_bin");
	// The returned literal reads t before t is freed.
	ASSERT_EQ(output, "3 15\n0 0\n0 0 1 1 2 1 2 4 1 \n");

	string catResult = execTestCommand("cat " + traceFile);
	int allocs = 0, frees = 0;
	size_t pos = 0;
	while ((pos = catResult.find("@ ", pos)) != string::npos) {
		size_t eol = catResult.find('\n', pos);
		string line = catResult.substr(pos, eol - pos);
		bool fromSharedLib = line.find(".so.") != string::npos;
		if (!fromSharedLib && line.find(" + ") != string::npos) allocs++;
		if (!fromSharedLib && line.find(" - ") != string::npos) frees++;
		pos = (eol == string::npos) ? string::npos : eol + 1;
	}
	EXPECT_EQ(allocs, 13) << "expected 13 allocs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, struct_lit_arr_field_mtrace)
{
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_struct_lit_arr_field_bin "
		"../test/testdata/build-mgr/275_struct_lit_arr_field_mtrace.pa"), "");

	string traceFile = "/tmp/palan_struct_lit_arr_field_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_struct_lit_arr_field_bin");
	ASSERT_EQ(output, "1.0 2.0 3.0 1 2 3 4 5 6 7 8 9 10\n"
	                  "3.0 2.0 1.0 4 3 2 1 8 7 6 5 10 9\n"
	                  "0.5 1.0 2.0 7 1 2 7 7 3 4 7 7 5\n"
	                  "2.0 2.0 2.0 2 2 2 2 2 2 2 2 2 2\n");

	string catResult = execTestCommand("cat " + traceFile);
	int allocs = 0, frees = 0;
	size_t pos = 0;
	while ((pos = catResult.find("@ ", pos)) != string::npos) {
		size_t eol = catResult.find('\n', pos);
		string line = catResult.substr(pos, eol - pos);
		bool fromSharedLib = line.find(".so.") != string::npos;
		if (!fromSharedLib && line.find(" + ") != string::npos) allocs++;
		if (!fromSharedLib && line.find(" - ") != string::npos) frees++;
		pos = (eol == string::npos) ? string::npos : eol + 1;
	}
	EXPECT_EQ(allocs, 25) << "expected 25 allocs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, struct_lit_struct_expr_mtrace)
{
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_struct_lit_struct_expr_bin "
		"../test/testdata/build-mgr/276_struct_lit_struct_expr_mtrace.pa"), "");

	string traceFile = "/tmp/palan_struct_lit_struct_expr_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_struct_lit_struct_expr_bin");
	ASSERT_EQ(output, "1 2 3 4\n"
	                  "3 4 1 2\n"
	                  "5 6 1 2\n"
	                  "1 2 9 10\n"
	                  "1 2 7 8\n"
	                  "7 8 11 12\n"
	                  "30 30 30 2 1 2\n");

	string catResult = execTestCommand("cat " + traceFile);
	int allocs = 0, frees = 0;
	size_t pos = 0;
	while ((pos = catResult.find("@ ", pos)) != string::npos) {
		size_t eol = catResult.find('\n', pos);
		string line = catResult.substr(pos, eol - pos);
		bool fromSharedLib = line.find(".so.") != string::npos;
		if (!fromSharedLib && line.find(" + ") != string::npos) allocs++;
		if (!fromSharedLib && line.find(" - ") != string::npos) frees++;
		pos = (eol == string::npos) ? string::npos : eol + 1;
	}
	EXPECT_EQ(allocs, 38) << "expected 38 allocs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, return_unowned_struct_mtrace)
{
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_return_unowned_struct_bin "
		"../test/testdata/build-mgr/277_return_unowned_struct_mtrace.pa"), "");

	string traceFile = "/tmp/palan_return_unowned_struct_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_return_unowned_struct_bin");
	ASSERT_EQ(output, "20 21\n"
	                  "20 21\n"
	                  "24 25\n"
	                  "3 4\n"
	                  "7 8\n"
	                  "28 29\n"
	                  "24 25\n"
	                  "9 10\n"
	                  "11 12\n"
	                  "20 21\n"
	                  "24 25\n"
	                  "28 29\n");

	string catResult = execTestCommand("cat " + traceFile);
	int allocs = 0, frees = 0;
	size_t pos = 0;
	while ((pos = catResult.find("@ ", pos)) != string::npos) {
		size_t eol = catResult.find('\n', pos);
		string line = catResult.substr(pos, eol - pos);
		bool fromSharedLib = line.find(".so.") != string::npos;
		if (!fromSharedLib && line.find(" + ") != string::npos) allocs++;
		if (!fromSharedLib && line.find(" - ") != string::npos) frees++;
		pos = (eol == string::npos) ? string::npos : eol + 1;
	}
	EXPECT_EQ(allocs, 22) << "expected 22 allocs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, embed_field_value_mtrace)
{
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_embed_field_value_bin "
		"../test/testdata/build-mgr/278_embed_field_value_mtrace.pa"), "");

	string traceFile = "/tmp/palan_embed_field_value_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_embed_field_value_bin");
	ASSERT_EQ(output, "1 2\n"
	                  "1 2\n"
	                  "3\n"
	                  "3 4\n"
	                  "1 2\n"
	                  "3 4\n"
	                  "3 4\n");

	string catResult = execTestCommand("cat " + traceFile);
	int allocs = 0, frees = 0;
	size_t pos = 0;
	while ((pos = catResult.find("@ ", pos)) != string::npos) {
		size_t eol = catResult.find('\n', pos);
		string line = catResult.substr(pos, eol - pos);
		bool fromSharedLib = line.find(".so.") != string::npos;
		if (!fromSharedLib && line.find(" + ") != string::npos) allocs++;
		if (!fromSharedLib && line.find(" - ") != string::npos) frees++;
		pos = (eol == string::npos) ? string::npos : eol + 1;
	}
	EXPECT_EQ(allocs, 8) << "expected 8 allocs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

// A borrow a call returns may point to a temp struct given to it, so the temp
// lives until its full expression is done; inside a condition that is each
// short-circuit operand, and each evaluation of a loop condition.
TEST(build_mgr, call_temp_borrow_mtrace)
{
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_call_temp_borrow_bin "
		"../test/testdata/build-mgr/279_call_temp_borrow_mtrace.pa"), "");

	string traceFile = "/tmp/palan_call_temp_borrow_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_call_temp_borrow_bin");
	ASSERT_EQ(output, "76\n"
	                  "38\n"
	                  "76\n"
	                  "4 6 8\n"
	                  "and\n"
	                  "8\n");

	string catResult = execTestCommand("cat " + traceFile);
	int allocs = 0, frees = 0;
	size_t pos = 0;
	while ((pos = catResult.find("@ ", pos)) != string::npos) {
		size_t eol = catResult.find('\n', pos);
		string line = catResult.substr(pos, eol - pos);
		bool fromSharedLib = line.find(".so.") != string::npos;
		if (!fromSharedLib && line.find(" + ") != string::npos) allocs++;
		if (!fromSharedLib && line.find(" - ") != string::npos) frees++;
		pos = (eol == string::npos) ? string::npos : eol + 1;
	}
	EXPECT_EQ(allocs, 3) << "expected 3 allocs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

// A literal argument is a temp of the parameter's type, released like a
// returned struct given as an argument.
TEST(build_mgr, lit_arg_mtrace)
{
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_lit_arg_bin "
		"../test/testdata/build-mgr/280_lit_arg_mtrace.pa"), "");

	string traceFile = "/tmp/palan_lit_arg_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_lit_arg_bin");
	ASSERT_EQ(output, "2.5 4.5 6.5\n"
	                  "6.0 4.0\n"
	                  "3.0\n"
	                  "9 11\n"
	                  "and\n");

	string catResult = execTestCommand("cat " + traceFile);
	int allocs = 0, frees = 0;
	size_t pos = 0;
	while ((pos = catResult.find("@ ", pos)) != string::npos) {
		size_t eol = catResult.find('\n', pos);
		string line = catResult.substr(pos, eol - pos);
		bool fromSharedLib = line.find(".so.") != string::npos;
		if (!fromSharedLib && line.find(" + ") != string::npos) allocs++;
		if (!fromSharedLib && line.find(" - ") != string::npos) frees++;
		pos = (eol == string::npos) ? string::npos : eol + 1;
	}
	EXPECT_EQ(allocs, 16) << "expected 16 allocs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, lit_arg_struct_arr_mtrace)
{
	// '@[2]$P' takes one block of inline structs; '@[2]P' a slot array plus a
	// struct per element.
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_lit_struct_arr_bin "
		"../test/testdata/build-mgr/283_lit_arg_struct_arr_mtrace.pa"), "");

	string traceFile = "/tmp/palan_lit_struct_arr_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_lit_struct_arr_bin");
	ASSERT_EQ(output, "4 8\n");

	string catResult = execTestCommand("cat " + traceFile);
	int allocs = 0, frees = 0;
	size_t pos = 0;
	while ((pos = catResult.find("@ ", pos)) != string::npos) {
		size_t eol = catResult.find('\n', pos);
		string line = catResult.substr(pos, eol - pos);
		bool fromSharedLib = line.find(".so.") != string::npos;
		if (!fromSharedLib && line.find(" + ") != string::npos) allocs++;
		if (!fromSharedLib && line.find(" - ") != string::npos) frees++;
		pos = (eol == string::npos) ? string::npos : eol + 1;
	}
	EXPECT_EQ(allocs, 4) << "expected 4 allocs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, frame_mtrace) {
	expectFrameMtrace("300_frame_mtrace",
		"7\n2 6\n1 9 0\n6 10\n7 5\n3\n5\n8\n4\n3 4\n11\n");
}

TEST(build_mgr, frame_owned_elems_mtrace) {
	expectFrameMtrace("302_frame_owned_elems_mtrace", "10\n5\n7\n9\n3\n");
}

TEST(build_mgr, expr_rooted_access) {
	cleanTestEnv();
	// Indexing, field access and their writes rooted in a cast, a parenthesized
	// expression or a call returning a pointer.
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/308_expr_rooted_access.pa");
	ASSERT_EQ(output, "7 8 8\n10\n10 11\n12\n1970\n1\n");
}

TEST(build_mgr, store_eval_once) {
	cleanTestEnv();
	// Each call in a chain's targets runs once, and a value runs before the
	// calls in its target.
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/309_store_eval_once.pa");
	ASSERT_EQ(output, "i1 v i2 s7 5 6 5 7\ng i0 g v g i1 v 8 9 10 10\nw g i1 11 11 12 13 14\n");
}

TEST(build_mgr, store_eval_once_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_store_eval_once_mtrace_bin "
		"../test/testdata/build-mgr/310_store_eval_once_mtrace.pa"), "");

	string traceFile = "/tmp/palan_store_eval_once_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_store_eval_once_mtrace_bin");
	ASSERT_EQ(output, "i1 i0 i1 2 3 4\n");

	string catResult = execTestCommand("cat " + traceFile);
	int allocs = 0, frees = 0;
	size_t pos = 0;
	while ((pos = catResult.find("@ ", pos)) != string::npos) {
		size_t eol = catResult.find('\n', pos);
		string line = catResult.substr(pos, eol - pos);
		bool fromSharedLib = line.find(".so.") != string::npos;
		if (!fromSharedLib && line.find(" + ") != string::npos) allocs++;
		if (!fromSharedLib && line.find(" - ") != string::npos) frees++;
		pos = (eol == string::npos) ? string::npos : eol + 1;
	}
	EXPECT_EQ(allocs, 8) << "expected 8 allocs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, struct_param_owned_mtrace) {
	// A struct parameter owns a copy of its argument, or the struct a call
	// returned, and frees it unless it gives it away.
	expectMtraceRun("373_struct_param_owned_mtrace", "11 1\n15 17\n8 2\n1 1 2\n", 15);
}

TEST(build_mgr, give_take_mtrace) {
	// '>>' moves a value into a parameter owning it or gives it to C, which
	// realloc hands back for '<<=' to take over; nothing leaks or is freed twice.
	expectMtraceRun("374_give_take_mtrace", "5 1\n4\n3\n10\n102\n7 9 1 5 4 1\n", 22);
}

TEST(build_mgr, give_then_borrow_mtrace) {
	// An owner given away with '>>' is set to NULL only just before the call,
	// so the call's other arguments, before or after it, still see it.
	expectMtraceRun("384_give_then_borrow_mtrace", "12 1\n26\n60 1\n48\n55\n", 13);
}

TEST(build_mgr, give_path_call_once_mtrace) {
	// The calls on the path to an owner given away run once, though the
	// owner is read and later reset through it.
	expectMtraceRun("385_give_path_call_once_mtrace", "1\n3\n2 4 3 2 1\n", 7);
}

TEST(build_mgr, func_cast_ownership_mtrace) {
	// A C function cast to own a parameter keeps what '>>' gives it until the
	// Palan callback owning it frees it, and a pointer in a read-only struct's
	// field is taken over by viewing it as its owner.
	expectMtraceRun("375_func_cast_ownership_mtrace", "work 3 2\n1\n2 1\n", 5);
}

TEST(build_mgr, runtime_size_ownership_mtrace) {
	// Arrays sized at run time, of structs and rows too, are moved, given, taken
	// over and copied by the lengths their receiving variable keeps.
	expectMtraceRun("376_runtime_size_ownership_mtrace", "1 4 3 10\n3:1 1:1 \n7 9\n5 5\n", 43);
}

TEST(build_mgr, runtime_size_move_copy_mtrace) {
	// A variable given an array takes its size; a row, a field, a constant-size
	// variable or a parameter keeps its own, given an array through a cast. A
	// copy goes as far as the shorter at each level, rows in place one by one.
	expectMtraceRun("429_runtime_size_move_copy_mtrace", "2 7 4 5\n6 8 9 3\n4 2 2\n11 7 6 2\n", 42);
}

TEST(build_mgr, given_flow_mtrace) {
	// Filled again before each use, or given only on a path that leaves.
	expectMtraceRun("430_given_flow_mtrace", "6 3 3 8\n11 1 1\n", 10);
}

TEST(build_mgr, owner_view_target_mtrace) {
	// A block given to a C pointer viewed as its owner is held there until it is
	// taken back over, sized at run time on both sides.
	expectMtraceRun("377_owner_view_target_mtrace", "16 7\n3 1\n", 3);
}

TEST(build_mgr, store_value_first) {
	cleanTestEnv();
	// A call in the target that writes the value through a pointer it was
	// given -- by '@', a []T parameter, a raw pointer or an integer
	// cast, even one given later in a loop -- does not change what is stored.
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/311_store_value_first.pa");
	ASSERT_EQ(output, "5 5\n1 2\n1 1\n1 2\n1 11\n1 101\n1 1001\n3 9\ng i 1 2\n"
	                  "1 2 27\ni i i 6 4 1\ni 2 2\n");
}

TEST(build_mgr, store_value_first_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_store_value_first_mtrace_bin "
		"../test/testdata/build-mgr/312_store_value_first_mtrace.pa"), "");

	string traceFile = "/tmp/palan_store_value_first_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_store_value_first_mtrace_bin");
	ASSERT_EQ(output, "3 4\n5 4 9\n");

	string catResult = execTestCommand("cat " + traceFile);
	int allocs = 0, frees = 0;
	size_t pos = 0;
	while ((pos = catResult.find("@ ", pos)) != string::npos) {
		size_t eol = catResult.find('\n', pos);
		string line = catResult.substr(pos, eol - pos);
		bool fromSharedLib = line.find(".so.") != string::npos;
		if (!fromSharedLib && line.find(" + ") != string::npos) allocs++;
		if (!fromSharedLib && line.find(" - ") != string::npos) frees++;
		pos = (eol == string::npos) ? string::npos : eol + 1;
	}
	EXPECT_EQ(allocs, 15) << "expected 15 allocs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, uninit_var_decl) {
	cleanTestEnv();
	// A variable declared without an initializer keeps its declared width
	// when it is later assigned, read, addressed or redeclared in a loop.
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/313_uninit_var_decl.pa");
	ASSERT_EQ(output, "11 12\n12 13\n-2 301 4000000000 3.0\n8\n0 1 2 3 4 5 \n");
}

TEST(build_mgr, tapple_frame_mtrace) {
	// A `$` receiver gets the returned struct's contents, owned fields
	// included, and the returned block is freed.
	expectFrameMtrace("318_tapple_frame_mtrace", "3 10\n5 6 9 7\n8 9\n");
}

TEST(build_mgr, var_transfer_mtrace) {
	// The target's old value is freed and the source left NULL, so every
	// block is freed exactly once.
	expectFrameMtrace("319_var_transfer_mtrace", "4\n5\n3 4\n7\n9\n7\n0\n5\n9\n");
}

// The value is computed before the target releases what it held, so a value
// that reads the target (`f(v) ->> v`) sees it intact.
TEST(build_mgr, transfer_into_source_mtrace) {
	expectMtraceRun("441_transfer_into_source_mtrace", "4 7\n3\n5 3\n3\n", 24);
}

// `[n]arr_P` and `[m][n]P` once shared the helper key `arr_arr_P`, so one of
// them got no helpers and the program failed to link.
TEST(build_mgr, alloc_key_struct_names) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/338_alloc_key_struct_names.pa");
	ASSERT_EQ(output, "3 4\n");
}

// A field holds any array a variable can: built, copied, moved into and
// freed with its struct, on the heap, in a frame and as an array element.
TEST(build_mgr, struct_multilevel_field_mtrace) {
	expectMtraceRun("349_struct_multilevel_field_mtrace",
		"5 9 6 8 2 3 5\n1 2\n3 4\n11\n3 1\n", 67);
}

// A `[+]T` field's header is built with its struct, which copies, moves and
// frees it, and a struct literal sets its length.
TEST(build_mgr, struct_varr_field_mtrace) {
	expectMtraceRun("351_struct_varr_field_mtrace",
		"3 5 1 1\n20 9 39\n4 5\n2 2 5\n8 3 9 1\n2 9\n3 2 9 6 5\n1 2 3 3\n", 73);
}

// A field's `$[n]...` lies in its struct, rows, structs and pointer slots
// alike, and is copied with it; an owned array field beside it is copied deep.
TEST(build_mgr, field_inline_arr_mtrace) {
	expectMtraceRun("354_field_inline_arr_mtrace", "88 6 30 20 20 11 31\n7 31\n5\n", 5);
}

// Owned slots laid out in a struct are built, copied and freed with it.
TEST(build_mgr, field_inline_owned_mtrace) {
	expectMtraceRun("355_field_inline_owned_mtrace", "25 37 2 7 2\n2 9 2\n2 1\n", 83);
}

// Arrays of elements laid out alike share their functions, which store and
// return a narrow integer by its width; its reader extends it by its own type.
TEST(build_mgr, layout_share_mtrace) {
	expectMtraceRun("350_layout_share_mtrace",
		"-7 4000000001 1 1\n-128 255 0 -2 254\n2.25 1\n5 6 2.5 4000000000 1\n"
		"2 6 0.25 4000000000\n-3 252 0 0\n", 49);
}

// A returned struct is written into storage its receiver provides: a frame
// temp, a `$` variable, a heap variable, an owning parameter's block, or the
// caller's own return slot; in registers or through a hidden pointer.
TEST(build_mgr, struct_ret_dest_mtrace) {
	expectMtraceRun("378_struct_ret_dest_mtrace",
		"42 9\n10 28\n3 1.5 7 15\n5 6 4 3\n3 8 9\n9 10\n11 12\n", 33);
}

// A C function's struct return is received wherever a Palan one is.
TEST(build_mgr, c_struct_ret_mtrace) {
	expectMtraceRun("379_c_struct_ret_mtrace", "3 1 14 2\n4 3 3\n4 2 2\n", 5);
}

// Struct returns whose SSE eightbytes are one flo64, two flo32, or a flo64
// array element.
TEST(build_mgr, struct_ret_flo64) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/383_struct_ret_flo64.pa");
	ASSERT_EQ(output, "1.5 2.5 5\n-4 -3.75\n0.75 -0.75\n0.5 1.5\n");
}

TEST(build_mgr, operator_func_mtrace) {
	expectMtraceRun("380_operator_func_mtrace",
		"1.5 2.5 3.5\n0.5 1.5 2.5\n-1 -2 -3\n-1.5 -2.5 -3.5\n1.5 3 4.5\n0 -1 -2\n3\n2.5 4.5 6.5\n", 4);
}

// A call whose functions return a struct or a number is told by its argument,
// so the operator over it is a struct the overloaded 'g' takes.
TEST(build_mgr, overload_inner_call_mtrace) {
	expectMtraceRun("425_overload_inner_call_mtrace", "vec2 2 4\nflo64 5\n", 0);
}

// '<<' chains through the borrow each call returns; a returned or converted
// right operand is released after the statement.
TEST(build_mgr, append_mtrace) {
	expectMtraceRun("410_append_mtrace", "abcde\nabcderetT\nabcderetT!?\nabcderetT!?x\nfg\n", 24);
}

TEST(build_mgr, copy_func_mtrace) {
	expectMtraceRun("411_copy_func_mtrace",
		"->2 | 2 1\n->2 ->1 | 1 1\n->2 | self 1\n->2 | 3\nnew 2 2 4\n->7 ->8 | 8 6 5\n->8 | field\n->1 ->2 | 9\n"
		"->8 | 8 2 2\n->5 ->6 | 5 6 2\n->7 F4 F5 F6 | 11 4 1 7 42 3\nF3 F4 | 4 2\nF4 | 4 9 2\nF3 F4 | 2 1\n"
		"->1 ->2 ->0 ->3 ->4 K7 K0 K8 | 5 7 3 4\n->1 ->2 ->0 ->3 ->4 K7 K0 K8 K0 K4 K1 K3 L5 | 6 7 9 2 4\n->1 | 1 2\nK0 | 5 0\n", 169);
}

TEST(build_mgr, compare_func_mtrace) {
	expectMtraceRun("403_compare_func_mtrace",
		"1 0 0\n1 0 1 0\n0 1 1 1\n1 0\n1 1\n0 1\n1 2 0\n3 4 1\n", 4);
}

// Struct elements are copies the callee owns: those left are released at its
// exit, one moved out is not, and the caller releases none of them.
TEST(build_mgr, variadic_struct_mtrace) {
	expectMtraceRun("407_variadic_struct_mtrace", "10\n5 1 2\n8\n12\n14\n", 33);
}

// An element moved out leaves NULL, which a copy keeps; one copied into a
// NULL slot is built there first, and so copied whole.
TEST(build_mgr, copy_null_elem_mtrace) {
	expectMtraceRun("432_copy_null_elem_mtrace", "1 2 -1\n1 1\n1\n1 5\n1\n7\n1 2 1\n3\n", 69);
}

// '...a' passes a copy the callee may write, '...a>>' the array itself, whose
// block alone the caller frees after the call.
TEST(build_mgr, spread_arg_mtrace) {
	expectMtraceRun("433_spread_arg_mtrace", "n=3 7\n1\nn=3 6\nn=3 6\nn=4 6\nn=2 11\nn=2 11\nn=3 6\nn=2 3\n7\nn=3 11\nn=3 6\nn=2 42\nx, y\nn=3 6\n1\nn=2 11\nn=4 6\n1 1\n", 25);
}

// Struct elements are the callee's whether copied or given; a NULL one is
// passed on as NULL.
TEST(build_mgr, spread_struct_mtrace) {
	expectMtraceRun("434_spread_struct_mtrace", "3\n2 1 2\n3\n3\n1\n7\n4 1\n15\n4\n111\n", 99);
}

// Each converted literal is released where a struct the call returns would be.
TEST(build_mgr, literal_conversion_mtrace) {
	expectMtraceRun("408_literal_conversion_mtrace",
		"init copy borrow owned v1 v2 ret cmp gt chained opret field field2 e2 e1 \n", 64);
}

TEST(build_mgr, numeric_literal_conversion_mtrace) {
	expectMtraceRun("428_numeric_literal_conversion_mtrace", "12 15 -12 i3 30 eq 7 10 2 \n", 33);
}

TEST(build_mgr, chain_ahead_of_receiver_mtrace) {
	expectMtraceRun("386_chain_ahead_of_receiver_mtrace", "5\n1 4\n1 4\n1 5\n4\n4\n4\n", 10);
}

// A `[+]T` header laid out in a struct, directly or in a struct laid out in
// it, is built, copied and freed with the struct wherever the struct lives.
TEST(build_mgr, field_embed_varr_mtrace) {
	expectMtraceRun("399_field_embed_varr_mtrace", "Hi 3\nHi Hi!\n9 A 0 2\nA N 5 1\n3 5\n", 35);
}

// Field initializers are in place wherever a struct comes to exist: a variable,
// a field, an element (laid out in place too) or one `resize` adds; a named
// literal takes them for the fields it leaves out.
TEST(build_mgr, field_init_mtrace) {
	string p = "7 200 1 2.0 2\n";
	expectMtraceRun("400_field_init_mtrace",
		p + p + "hi 3 1\n1 -2 3 5 6 2 8\n" + p + p + p + p + "hi 0\n" + p + p
		+ p + p + p + p + "8 200 1 2.0 2\n" + p + "7 1 1 2.0 2\n3 4 0 0.5 1\n"
		+ "9 200 1 2.0 2\n3 200 1 2.0 0\n", 38);
}

// An initializer gives a struct, an array of structs or of rows, or NULL; a
// struct laid out in another stores its own, its type in another module too.
TEST(build_mgr, field_init_values_mtrace) {
	string r = "1 9 2 3 4 1 9\n5 9 7 8 9 9\n3 4 2 3 3 6 1\n";
	expectMtraceRun("435_field_init_values_mtrace", r + r + r + "3 3 7 1 5\n2 9 3\n3 3 9 1 8\n", 103);
}
