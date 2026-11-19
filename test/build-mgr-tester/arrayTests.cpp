/// palan build manager tests: arrays
///
/// @file arrayTests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include "../test-base/testBase.h"
#include "buildMgrTestUtil.h"

using namespace std;

TEST(build_mgr, array_mtrace) {
	cleanTestEnv();
	// Step 1: compile without LD_PRELOAD
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_array_mtrace_bin "
		"../test/testdata/build-mgr/033_array_mtrace.pa"), "");

	// Step 2: run the compiled binary with mtrace instrumentation
	string traceFile = "/tmp/palan_array_mtrace.log";
	execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_array_mtrace_bin");

	// Skip allocations and frees from shared libraries (e.g. libc stdio
	// buffers): whether they fall within the mtrace window varies.
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
	EXPECT_EQ(allocs, 1) << "expected 1 malloc for [64]uint8 buf, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, array_multi_var) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/034_array_multi_var.pa");
	ASSERT_EQ(output, "hello world\n");
}

TEST(build_mgr, array_while_break) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/035_array_while_break.pa");
	ASSERT_EQ(output, "0\n1\n");
}

TEST(build_mgr, array_while_continue) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/036_array_while_continue.pa");
	ASSERT_EQ(output, "1\n3\n5\n");
}

TEST(build_mgr, arr_index_fib) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/037_arr_index_fib.pa");
	ASSERT_EQ(output, "1\n1\n2\n3\n5\n8\n13\n21\n34\n55\n");
}

TEST(build_mgr, ptr_arr_transfer) {
	cleanTestEnv();

	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/038_ptr_arr_transfer.pa");
	ASSERT_EQ(output, "6\n");
}

TEST(build_mgr, 2d_array) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/039_2d_array.pa");
	ASSERT_EQ(output, "1 2 3\n4 5 6\n");
}

TEST(build_mgr, int32_index) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/041_int32_index.pa");
	ASSERT_EQ(output, "30\n10\n20\n");
}

TEST(build_mgr, embed_arr) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/042_embed_arr.pa");
	ASSERT_EQ(output, "1\n10\n26\n");
}

TEST(build_mgr, embed_arr_int32_idx) {
	cleanTestEnv();
	// int32 expression as outer row index on the scale_expr path (variable inner dim)
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/043_embed_arr_int32_idx.pa");
	ASSERT_EQ(output, "20\n30\n");
}

TEST(build_mgr, nested_owned_struct) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/056_nested_owned_struct.pa");
	ASSERT_EQ(output, "99\n");
}

TEST(build_mgr, uint_idx_var_stride) {
	// Regression: a uint32 row index into a [n]$[m]T array with a runtime
	// inner dimension used to abort before the Uint32->Int64 stride convert.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/142_uint_idx_var_stride.pa");
	ASSERT_EQ(output, "40 50 60\n");
}

TEST(build_mgr, owned_arr_embed_field_mtrace) {
	// Element types whose embedded fields are not declarable in the generated
	// allocator module: a native embed, a C struct embed, and a C union.
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_owned_arr_embed_field_mtrace_bin "
		"../test/testdata/build-mgr/193_owned_arr_embed_field_mtrace.pa"), "");

	string traceFile = "/tmp/palan_owned_arr_embed_field_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_owned_arr_embed_field_mtrace_bin");
	ASSERT_EQ(output, "3 4 5 6\n");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	// [2]L: 1 + 2 x (L + owned P) = 5; [2]itimerspec: 3; [2]pthread_mutex_t: 3
	EXPECT_EQ(allocs, 11) << "expected 11 allocs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, arr_lit_1d) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/204_arr_lit_1d.pa");
	ASSERT_EQ(output, "1 -2 3\n200 255\n1.5 2.0 10.0\n11 20 -2\n2 3 5\n9 7 8\n4 5\n");
}

TEST(build_mgr, arr_lit_2d) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/207_arr_lit_2d.pa");
	ASSERT_EQ(output, "1 3 5 -6\n8 10 12\n1.50 10.00 3.00 4.25\n3 4 6\n200 255 0 10\n20 -6 3\n10 11 12 13\n2 3 6\n2 3 4\n");
}

TEST(build_mgr, arr_lit_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_arr_lit_mtrace_bin "
		"../test/testdata/build-mgr/205_arr_lit_mtrace.pa"), "");

	string traceFile = "/tmp/palan_arr_lit_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_arr_lit_mtrace_bin");
	ASSERT_EQ(output, "8\n");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	EXPECT_EQ(allocs, 2) << "expected 2 allocs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, arr_lit_2d_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_arr_lit_2d_mtrace_bin "
		"../test/testdata/build-mgr/208_arr_lit_2d_mtrace.pa"), "");

	string traceFile = "/tmp/palan_arr_lit_2d_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_arr_lit_2d_mtrace_bin");
	ASSERT_EQ(output, "10\n");

	// [2][3]int32: row table + 2 rows; []$[3]int32: one contiguous block
	auto [allocs, frees] = parseMtraceLog(traceFile);
	EXPECT_EQ(allocs, 4) << "expected 4 allocs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, arr_borrow) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/219_arr_borrow.pa");
	ASSERT_EQ(output, "161\n7 8\n8\n5 6\n8\n9 8\n1 9\n");
}

TEST(build_mgr, prim_ptr_slot_arr) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/228_prim_ptr_slot_arr.pa");
	ASSERT_EQ(output, "11 20 11\n");
}

TEST(build_mgr, ptr_slot_arr_borrow) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/229_ptr_slot_arr_borrow.pa");
	ASSERT_EQ(output, "(13,4)(11,2) 13 106 7\n");
}

TEST(build_mgr, copy_arr) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/232_copy_arr.pa");
	ASSERT_EQ(output, "1 1 3 11 1\n4 7 6\n3\n");
}

TEST(build_mgr, owned_arr_field_elem_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_owned_arr_field_elem_bin "
		"../test/testdata/build-mgr/239_owned_arr_field_elem_mtrace.pa"), "");

	string traceFile = "/tmp/palan_owned_arr_field_elem.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_owned_arr_field_elem_bin");
	ASSERT_EQ(output, "3 4\n");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	// W: itself + w = 2; [2]W (1 + 2 * 2), [2][2]W (1 + 2 rows of 1 + 2 * 2)
	EXPECT_EQ(allocs, 16) << "expected 16 allocs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, unsized_arr_rows) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/253_unsized_arr_rows.pa");
	ASSERT_EQ(output, "0 1 2\n10 11 12\n5 6\n7 8\n12\n");
}

TEST(build_mgr, embed_row_copy) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/254_embed_row_copy.pa");
	ASSERT_EQ(output, "0 1 2 3\n10 11 12 13\n3\n");
}

TEST(build_mgr, row_borrow) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/255_row_borrow.pa");
	ASSERT_EQ(output, "46 13 46\n406 3 200 806\n77 101 3\n10\n5 6\n");
}

TEST(build_mgr, arr_lit_assign_mtrace)
{
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_arr_lit_assign_bin "
		"../test/testdata/build-mgr/274_arr_lit_assign_mtrace.pa"), "");

	string traceFile = "/tmp/palan_arr_lit_assign_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_arr_lit_assign_bin");
	// Every value is read before any element is written (swaps), and the index
	// in a row target is evaluated once (cnt).
	ASSERT_EQ(output, "2 1 4 3\n1.00 0.00 0.50 0.25\n1 2 3 4 8 0 9 10\n0 5 6 cnt=1\n"
	                  "1 6 0 0 7 8 cnt=2\n2 3 6 3 4\n");

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

TEST(build_mgr, owned_rows_ro_slots) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/292_owned_rows_ro_slots.pa");
	ASSERT_EQ(output, "3 4\n7\n");
}

TEST(build_mgr, varr_basic) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/325_varr_basic.pa");
	ASSERT_EQ(output, "7 8\n10 20 30\n2 5 9\nAA\n3 4\n6\n");
}

TEST(build_mgr, varr_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_varr_mtrace_bin "
		"../test/testdata/build-mgr/326_varr_mtrace.pa"), "");

	string traceFile = "/tmp/palan_varr_mtrace.log";
	execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_varr_mtrace_bin");

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
	// Three heap headers, and data for the three arrays with a capacity.
	EXPECT_EQ(allocs, 6) << "expected 6 allocs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, varr_builtins) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/327_varr_builtins.pa");
	ASSERT_EQ(output, "12 12\n200 11\n0 1 2 30 40 \n2.5 1.5 1\n7 19 20\n");
}

TEST(build_mgr, arr_len) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/331_arr_len.pa");
	ASSERT_EQ(output, "10 20 30 10 20 30 \n5 6 2\n3 4\n");
}

TEST(build_mgr, len_borrow) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/366_len_borrow.pa");
	ASSERT_EQ(output, "6 2 2\n");
}

TEST(build_mgr, varr_builtins_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_varr_builtins_mtrace_bin "
		"../test/testdata/build-mgr/328_varr_builtins_mtrace.pa"), "");

	string traceFile = "/tmp/palan_varr_builtins_mtrace.log";
	execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_varr_builtins_mtrace_bin");

	string catResult = execTestCommand("cat " + traceFile);
	int allocs = 0, frees = 0;
	size_t pos = 0;
	while ((pos = catResult.find("@ ", pos)) != string::npos) {
		size_t eol = catResult.find('\n', pos);
		string line = catResult.substr(pos, eol - pos);
		bool fromSharedLib = line.find(".so.") != string::npos;
		// mtrace logs a realloc as a free ("<") and an allocation (">").
		if (!fromSharedLib && (line.find(" + ") != string::npos || line.find(" > ") != string::npos)) allocs++;
		if (!fromSharedLib && (line.find(" - ") != string::npos || line.find(" < ") != string::npos)) frees++;
		pos = (eol == string::npos) ? string::npos : eol + 1;
	}
	// The header, the data as push grows it to 4, 8, 16 and 32, and as resize(100) grows it to 100.
	EXPECT_EQ(allocs, 6) << "expected 6 allocs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, varr_borrow) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/332_varr_borrow.pa");
	ASSERT_EQ(output, "4 18 18\n18\n5 1 2\n5 16\n");
}

TEST(build_mgr, varr_borrow_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_varr_borrow_mtrace_bin "
		"../test/testdata/build-mgr/333_varr_borrow_mtrace.pa"), "");

	string traceFile = "/tmp/palan_varr_borrow_mtrace.log";
	execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_varr_borrow_mtrace_bin");

	string catResult = execTestCommand("cat " + traceFile);
	int allocs = 0, frees = 0;
	size_t pos = 0;
	while ((pos = catResult.find("@ ", pos)) != string::npos) {
		size_t eol = catResult.find('\n', pos);
		string line = catResult.substr(pos, eol - pos);
		bool fromSharedLib = line.find(".so.") != string::npos;
		if (!fromSharedLib && (line.find(" + ") != string::npos || line.find(" > ") != string::npos)) allocs++;
		if (!fromSharedLib && (line.find(" - ") != string::npos || line.find(" < ") != string::npos)) frees++;
		pos = (eol == string::npos) ? string::npos : eol + 1;
	}
	// Only the owner frees: the header, and the data as the borrows grow it to 4, 8 and 16.
	EXPECT_EQ(allocs, 4) << "expected 4 allocs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, cond_return_owned_mtrace) {
	// A variable returned on one path is still freed on the others.
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_cond_return_owned_bin "
		"../test/testdata/build-mgr/335_cond_return_owned.pa"), "");

	string traceFile = "/tmp/palan_cond_return_owned.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_cond_return_owned_bin");
	ASSERT_EQ(output, "2 1 3 1\n");

	string catResult = execTestCommand("cat " + traceFile);
	int allocs = 0, frees = 0;
	size_t pos = 0;
	while ((pos = catResult.find("@ ", pos)) != string::npos) {
		size_t eol = catResult.find('\n', pos);
		string line = catResult.substr(pos, eol - pos);
		bool fromSharedLib = line.find(".so.") != string::npos;
		if (!fromSharedLib && (line.find(" + ") != string::npos || line.find(" > ") != string::npos)) allocs++;
		if (!fromSharedLib && (line.find(" - ") != string::npos || line.find(" < ") != string::npos)) frees++;
		pos = (eol == string::npos) ? string::npos : eol + 1;
	}
	EXPECT_EQ(allocs, 8) << "expected 8 allocs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, varr_copy_return_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_varr_copy_return_bin "
		"../test/testdata/build-mgr/336_varr_copy_return.pa"), "");

	string traceFile = "/tmp/palan_varr_copy_return.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_varr_copy_return_bin");
	ASSERT_EQ(output,
		"1 2 3 (3)\n4 5 6 7 (4)\n1 2 3 (3)\n3 2 (2)\n1 2 3 (3)\n"
		"0 10 20 (3)\n0 10 (2)\n0 10 20 30 (4)\n5\n7 8 (2)\n1 2 3 (3)\n9 9 (2)\n"
		"0 0 0 (3)\n0 (1)\n5\n"
		"106 3\n106 122\n130 102\n"
		"1 2 3 (3)\n0 (1)\n");

	string catResult = execTestCommand("cat " + traceFile);
	int allocs = 0, frees = 0;
	size_t pos = 0;
	while ((pos = catResult.find("@ ", pos)) != string::npos) {
		size_t eol = catResult.find('\n', pos);
		string line = catResult.substr(pos, eol - pos);
		bool fromSharedLib = line.find(".so.") != string::npos;
		if (!fromSharedLib && (line.find(" + ") != string::npos || line.find(" > ") != string::npos)) allocs++;
		if (!fromSharedLib && (line.find(" - ") != string::npos || line.find(" < ") != string::npos)) frees++;
		pos = (eol == string::npos) ? string::npos : eol + 1;
	}
	EXPECT_EQ(allocs, 47) << "expected 47 allocs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

// Owned struct elements are built, freed and copied as the length changes,
// and a popped one is the caller's.
TEST(build_mgr, varr_struct_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_varr_struct_bin "
		"../test/testdata/build-mgr/337_varr_struct_mtrace.pa"), "");

	string traceFile = "/tmp/palan_varr_struct.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_varr_struct_bin");
	ASSERT_EQ(output,
		"3 1 4 5\n5 0 9\n1 3 4\n"
		"4 9\n41\n20 21\n2 7\n"
		"2 9 1 2\n2 5 101\n3 4 0\n");

	string catResult = execTestCommand("cat " + traceFile);
	int allocs = 0, frees = 0;
	size_t pos = 0;
	while ((pos = catResult.find("@ ", pos)) != string::npos) {
		size_t eol = catResult.find('\n', pos);
		string line = catResult.substr(pos, eol - pos);
		bool fromSharedLib = line.find(".so.") != string::npos;
		if (!fromSharedLib && (line.find(" + ") != string::npos || line.find(" > ") != string::npos)) allocs++;
		if (!fromSharedLib && (line.find(" - ") != string::npos || line.find(" < ") != string::npos)) frees++;
		pos = (eol == string::npos) ? string::npos : eol + 1;
	}
	EXPECT_EQ(allocs, 63) << "expected 63 allocs, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, arrow_precedence) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/334_arrow_precedence.pa");
	ASSERT_EQ(output, "7 8 9 1\n");
}

// A release build leaves out the checks of the writer's errors, not that of
// an allocation: a negative size then runs out of memory.
TEST(build_mgr, runtime_errors) {
	struct { const char* file; const char* mode; const char* error; } cases[] = {
		{"329_varr_pop_empty", "--debug", "pop of an empty array"},
		{"330_varr_resize_negative", "--debug", "negative array size"},
		{"436_array_negative_size", "--debug", "negative array size"},
		{"436_array_negative_size", "--release", "out of memory"},
		{"437_array_out_of_memory", "--debug", "out of memory"},
		{"437_array_out_of_memory", "--release", "out of memory"},
	};
	for (auto& c : cases) {
		cleanTestEnv();
		string bin = string("/tmp/palan_") + c.file;
		ASSERT_EQ(execTestCommand(string("bin/palan ") + c.mode + " -o " + bin + " ../test/testdata/build-mgr/" + c.file + ".pa"), "");
		// ulimit -c 0 keeps the program's core dump out of build/.
		// The shell's own report of the abort follows what the program wrote.
		string out = execTestCommand("ulimit -c 0; " + bin + " 2>out/stderr; echo status $?; cat out/stderr");
		string expected = string("status 134\nruntime error: ") + c.error + "\n";
		EXPECT_EQ(out.substr(0, expected.size()), expected) << c.file << " " << c.mode;
	}
}

TEST(build_mgr, nested_varr_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_nested_varr_bin "
		"../test/testdata/build-mgr/339_nested_varr_mtrace.pa"), "");

	string traceFile = "/tmp/palan_nested_varr.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_nested_varr_bin");
	ASSERT_EQ(output,
		"[1][][23]\n[1][456]\n[1][456]\n[17]\n[179][]\n[179][]\n"
		"[01][12][23]\n9\n[8][]\n2 1\n0\n1 3 2\n2 5\n");
	expectBalancedMtrace(traceFile, 77);
}

TEST(build_mgr, arr_of_varr_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_arr_of_varr_bin "
		"../test/testdata/build-mgr/340_arr_of_varr_mtrace.pa"), "");

	string traceFile = "/tmp/palan_arr_of_varr.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_arr_of_varr_bin");
	ASSERT_EQ(output, "2 0 1\n2 0 1\n0 0 2\n1 2 0\n1 2 0\n3 4\n2\n");
	expectBalancedMtrace(traceFile, 25);
}

// A popped row is freed when discarded and taken over by a declaration.
TEST(build_mgr, varr_row_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_varr_row_mtrace_bin "
		"../test/testdata/build-mgr/341_varr_row_mtrace.pa"), "");

	string traceFile = "/tmp/palan_varr_row_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_varr_row_mtrace_bin");
	ASSERT_EQ(output, 
		"[1 2 3 4][5 6 7 8][9 10 11 12]\n12 1 0\n42\n\n[1 2 3 9][1 2 3 4][5 6 7 8]\n"
		"[1 2 3 4]\n9 5\n3 7 3\n3\n7 4 1\n");
	expectBalancedMtrace(traceFile, 40);
}

// Rows laid out in place: owned elements in them are built, dropped and
// moved out by pop with the row.
TEST(build_mgr, evarr_row_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_evarr_row_mtrace_bin "
		"../test/testdata/build-mgr/342_evarr_row_mtrace.pa"), "");

	string traceFile = "/tmp/palan_evarr_row_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_evarr_row_mtrace_bin");
	ASSERT_EQ(output, "7 3\n12 9 0 4\n77 10\n2 2 3\n5 7 0 3 1\n5 1 2\n4 3\n");
	expectBalancedMtrace(traceFile, 80);
}

TEST(build_mgr, varr_row_owned_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_varr_row_owned_mtrace_bin "
		"../test/testdata/build-mgr/343_varr_row_owned_mtrace.pa"), "");

	string traceFile = "/tmp/palan_varr_row_owned_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_varr_row_owned_mtrace_bin");
	ASSERT_EQ(output, "5 7 3 7\n4 5 1\n2 1 0\n6 1 1\n");
	expectBalancedMtrace(traceFile, 76);
}

// A fixed array of rows of a run-time length and `[+][n]T` share the row
// helpers, which take the row length as an argument.
TEST(build_mgr, shared_row_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_shared_row_mtrace_bin "
		"../test/testdata/build-mgr/344_shared_row_mtrace.pa"), "");

	string traceFile = "/tmp/palan_shared_row_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_shared_row_mtrace_bin");
	ASSERT_EQ(output, "10 6 6\n");
	expectBalancedMtrace(traceFile, 27);
}

TEST(build_mgr, nested_fixed_mtrace) {
	expectMtraceRun("345_nested_fixed_mtrace", "7 7 8 8\n5 6 1\n7\n7 9 3\n", 63);
}

// `[+][3]int32` and `[+][4]int32` share their functions, which take the length.
TEST(build_mgr, varr_nested_rows_mtrace) {
	expectMtraceRun("346_varr_nested_rows_mtrace", "2 9 1 3 4\n4 6 7\n6 5 7\n8\n", 94);
}

TEST(build_mgr, embed_rows_mtrace) {
	expectMtraceRun("347_embed_rows_mtrace", "5 6 7\n9 2\n4 1\n8 1\n30\n11\n6 6 2\n", 178);
}

// An owned `[n]T` parameter frees its copy, a returned one is the caller's,
// and a borrow, a field, a row or a `$` array is returned as a copy.
TEST(build_mgr, arr_return_param_mtrace) {
	expectMtraceRun("348_arr_return_param_mtrace",
		"1 2 3\n101 2 3\n110 11 12\n1 2 3\n8 8 8\n4 5 6\n4 5 6\n6 6 6\n1 0\n7 8 9\n9\n"
		"104 5 6\n101 2 3\n3\n20 21 22\n", 35);
}

// A `[+]@T` frees its header and slots, never what they point to.
TEST(build_mgr, varr_ptr_slot_mtrace) {
	expectMtraceRun("357_varr_ptr_slot_mtrace",
		"3 6\n101 102 103\n102 2\n5 103 101\n5 7\n102 2 7\nHi 1\n", 21);
}

// Rows of pointer slots are rows of int64 to their helpers.
TEST(build_mgr, ptr_slot_rows_mtrace) {
	expectMtraceRun("427_ptr_slot_rows_mtrace",
		"2 3 12\n3 1 3 2 3\n1 3\n2 13 13\n2\n5 6\n", 31);
}

TEST(build_mgr, for_loop) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand("bin/palan -o out/for_bin ../test/testdata/build-mgr/362_for.pa"), "");
	ASSERT_EQ(execTestCommand("out/for_bin x 'y z'"),
		"40\n11 104\n0:1 1:2 2:3 \n1 2 3 4 5 6 \n60\n0=1 1=2 2=3 \n(1,2)(3,4)(0,2)(0,4)\n"
		"0 out/for_bin\n1 x\n2 y z\n");
}

// Each element copy is freed at the end of its iteration, by continue and
// break too, and a returned array after the loop.
TEST(build_mgr, for_loop_mtrace) {
	expectMtraceRun("363_for_mtrace", "(1,2) 7 8 1 2 3\n", 36);
}

TEST(build_mgr, for_loop_index_type) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand("bin/palan -o out/for_idx_bin ../test/testdata/build-mgr/367_for_index_type.pa"), "");
	ASSERT_EQ(execTestCommand("out/for_idx_bin"),
		"(0,0)1(4,0)3(0,1)4(4,1)6\n254:254 255:255 0:256 1:257 \n");
}

// The body may change an element at another constant index of the range's array.
TEST(build_mgr, for_loop_const_index) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand("bin/palan -o out/for_cidx_bin ../test/testdata/build-mgr/424_for_const_index.pa"), "");
	ASSERT_EQ(execTestCommand("out/for_cidx_bin"), "10 20 \n4\n");
}

// '@!e' writes the element 'next' returns a borrow of; a range a call returns
// lives until the loop ends; 'iter' and 'next' are found under an alias.
TEST(build_mgr, for_struct) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand("bin/palan -o out/for_struct_bin ../test/testdata/build-mgr/412_for_struct.pa"), "");
	ASSERT_EQ(execTestCommand("out/for_struct_bin"),
		"10,2 7,8 50,6 0:2 1:8 2:6 \n4 6 4 5 \n0:2 1:3 2:4 \n");
}

// Each element 'next' returns by value is freed at the end of its iteration,
// also by 'continue', 'break' and the last call returning false.
TEST(build_mgr, for_struct_mtrace) {
	expectMtraceRun("413_for_struct_mtrace", "1 2 3 2 \n", 34);
}

// Opaque values held in fields, rows and `[+]`, returned among several values,
// and copied and converted where their contents are visible.
TEST(build_mgr, opaque_mtrace) {
	expectMtraceRun("415_opaque_mtrace", "5 5\n3\nclose 9\nclose 8\nclose 7\nclose 6\nclose 5\n", 24);
}

TEST(build_mgr, top_var_const_in_sig) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/369_top_var_const.pa");
	ASSERT_EQ(output, "3 10.20\n");
}

TEST(build_mgr, multi_ret_into_frame_array_mtrace) {
	expectMtraceRun("387_multi_ret_into_frame_array_mtrace", "6 8\n", 1);
}

TEST(build_mgr, give_row_const_index_mtrace) {
	expectMtraceRun("388_give_row_const_index_mtrace", "6\n", 3);
}

TEST(build_mgr, arr_borrow_return) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/402_arr_borrow_return.pa");
	ASSERT_EQ(output, "hi hi\nhi 105\n7 4\n4\nok\nhi\n");
}

// `len` and `for` read the lengths an array sized at run time keeps: its
// own, a row's, and those of the copy a `for` iterates over.
TEST(build_mgr, runtime_len_mtrace) {
	expectMtraceRun("405_runtime_len_mtrace",
		"len 3 4 4 3\n0 1 2 3 \n10 11 12 13 \n20 21 22 23 \n0 1 2 \n10 11 12 13 \n2 0\n", 12);
}
