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
		{"444_dict_missing_key", "--debug", "key \"b\" is not in the dictionary"},
		{"444_dict_missing_key", "--release", "key \"b\" is not in the dictionary"},
		{"474_union_wrong_variant", "--debug", "a union read as a variant it does not hold"},
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

// Unchecked, the contents are read as the other variant's: making the union
// a variant zeroes them, so `true` reads as 1.
TEST(build_mgr, union_wrong_variant_release) {
	cleanTestEnv();
	string bin = "/tmp/palan_474_union_wrong_variant";
	ASSERT_EQ(execTestCommand("bin/palan --release -o " + bin + " ../test/testdata/build-mgr/474_union_wrong_variant.pa"), "");
	EXPECT_EQ(execTestCommand(bin), "1\n");
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
		"2 3 12\n3 1 3 2 3\n1 3\n2 13 13\n2\n5 6\n2 6 5\n", 35);
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

// What the range's call was given lives until the loop ends, also by a
// 'return' out of it, and the arguments are evaluated in the order written.
TEST(build_mgr, for_range_temps_mtrace) {
	expectMtraceRun("451_for_range_temps_mtrace",
		"11 12 13 22 23 65 66 \n33 42 43 51 61 \n1 3 11 12 71\n2 3 3 4 3\n82 92\n", 61);
}

TEST(build_mgr, dict_global) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand("bin/palan ../test/testdata/build-mgr/452_dict_global.pa"), "1 2 4 2 1\n");
}

// Initializers that static data cannot hold.
TEST(build_mgr, dynamic_global_data) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand("bin/palan ../test/testdata/build-mgr/454_dynamic_global_data.pa"), "1 4 1 4 11\n");
}

// Opaque values held in fields, rows and `[+]`, returned among several values,
// and copied and converted where their contents are visible.
TEST(build_mgr, opaque_mtrace) {
	expectMtraceRun("415_opaque_mtrace", "5 5\nclose 5\n5\n3\nclose 9\nclose 8\nclose 7\nclose 6\nclose 5\n", 27);
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

// A dictionary is allocated, copied, moved, passed and returned as a `[+]T` is,
// its header from std/rt; every header is freed once.
TEST(build_mgr, dict_mtrace) {
	expectMtraceRun("442_dict_mtrace", "ok\n", 10);
}

// The values of numbers and structs are added, replaced, read, removed and
// compacted; a value is evaluated before its key is added.
TEST(build_mgr, dict_index_mtrace) {
	expectMtraceRun("443_dict_index_mtrace",
		"13 5 2 3\n1 0 0\n4 99\n3\n4 36 39\n105 4 7 9\n4 1 3 105 4\n", 90);
}

// A 'for' runs in the order the keys were added, past removed ones and after
// compacting, over a dictionary a call returns too.
TEST(build_mgr, dict_for_mtrace) {
	expectMtraceRun("445_dict_for_mtrace",
		"e=5 a=1 d=4 b=2 \ne=30 a=22 d=8 b=4 64\ne=30 a=22 d=8 b=4 k18=18 k19=19 \none=1 two=2 three=3 \n1 11 2 21 \n", 58);
}

// A literal initializes, replaces with '->', is returned and is given as an
// argument; a key may be a string constant, and overloads tell a dictionary
// literal from a struct one.
TEST(build_mgr, dict_lit_mtrace) {
	expectMtraceRun("446_dict_lit_mtrace",
		"a=1 kk=2 c=3 \nz=9 a=3 \nlit=4 \nchained=5 \n2\n1 2\n2 3 11 3\n9 1\n6 7\n", 62);
}

// Values of every kind a `[+]T` holds: removed ones are released at once, and
// a copy keeps the positions of the removed keys, so nothing is freed twice.
TEST(build_mgr, dict_values_mtrace) {
	expectMtraceRun("447_dict_values_mtrace",
		"1 3 3 30\n9 4 5\n4 12\n1 4 2\n5 2 0\none 101 2\nb 103 4\n11 8\nclose 1\n1 2\nclose 2\n", 146);
}

// A dictionary as a field (initialized, given by a struct literal, copied with
// its struct) and as an element of a fixed and of a variable-length array.
TEST(build_mgr, dict_holders_mtrace) {
	expectMtraceRun("448_dict_holders_mtrace",
		"1 5 2 7\n1 1\n9 1\n2\n0 1\n1 6 4 2\n2\n2\n3 0 3\n1 1\n1 2\n", 127);
}

// '{}' where a value goes: an initializer, a field, nested, an argument, a return.
TEST(build_mgr, empty_dict_lit_mtrace) {
	expectMtraceRun("449_empty_dict_lit_mtrace", "1 1 1 0 0 1\n0 0 1 3\n", 33);
}

// Overloads told apart by a dictionary, one passed and returned by value, and
// rows and structs laid out in place in dictionaries.
TEST(build_mgr, dict_calls_mtrace) {
	expectMtraceRun("453_dict_calls_mtrace", "4 1\n7 100\n2 11\n1\n1 2\n2 6\n", 50);
}

// Fields borrowing arrays and dictionaries: bound, read, written through, passed
// on, iterated, and copied with their struct; nothing is freed through them.
TEST(build_mgr, borrow_fields_mtrace) {
	expectMtraceRun("455_borrow_fields_mtrace",
		"10 3 7 40 e\n3 30 11 12 1 3\n10 2 3 k=7 40 5 6 \n5 5\n", 37);
}

// Borrow fields of an imported struct, in a module that declares no such array.
TEST(build_mgr, imported_borrow_fields) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand("bin/palan ../test/testdata/build-mgr/456_imported_borrow_fields.pa"), "2 5 2 4\n2\n");
}

// A struct literal's value for a borrow field, and an imported struct that
// points to a C struct with a field of an unsupported type.
TEST(build_mgr, borrow_field_lit) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand("bin/palan ../test/testdata/build-mgr/457_borrow_field_lit.pa"), "4 9 3\n");
}

// Function pointers whose arrays have the functions' shapes.
TEST(build_mgr, func_ptr_arr_shape) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand("bin/palan ../test/testdata/build-mgr/458_func_ptr_arr_shape.pa"), "6 8 1\n");
}

// Function pointers taking and returning borrowed arrays and dictionaries and
// owned dictionaries, also through an imported struct's fields.
TEST(build_mgr, func_ptr_arrs_mtrace) {
	expectMtraceRun("459_func_ptr_arrs_mtrace", "1 2 5\n5 5 5 3\n8 9 2 5\n5 2 3 7\n", 34);
}

// Arrays and dictionaries lent read-only where the elements' owner differs.
TEST(build_mgr, lend_elems_mtrace) {
	expectMtraceRun("460_lend_elems_mtrace", "6 3 3 3 4 4 6 3 3 5 5 8\n", 46);
}

TEST(build_mgr, null_borrow_mtrace) {
	expectMtraceRun("461_null_borrow_mtrace",
		"-1 -2 -3 -4\n-1 1\n6 7 1\n6 1\n1 1 1\n8 3 4\n1 1 1\n1 1 1 1\n1 1 1 1\n7 5 9\n1 1\n-1 6\n", 20);
}

TEST(build_mgr, nested_borrow_mtrace) {
	expectMtraceRun("462_nested_borrow_mtrace",
		"3 2 36 21\n20 1 1\n7 50 3\n4\n30 10\n7\n7 1 0\n5 4 v 4\nw 4\n1 8 20\n2 8\n3\n9\n118\n", 46);
}

TEST(build_mgr, unsized_rows_borrow_mtrace) {
	expectMtraceRun("463_unsized_rows_borrow_mtrace",
		"6 8 6\n60\n60 8 2 1\n60\n8\n60\n5\n7 6 90 90\n6 60 6\n", 26);
}

TEST(build_mgr, row_layouts) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand("bin/palan ../test/testdata/build-mgr/464_row_layouts.pa"),
		"7 6 7\n7 6 7\n8\n6 7\n");
}

// Types in two modules, one type and block-scoped types in pointer cycles.
TEST(build_mgr, ptr_cycle_mtrace) {
	expectMtraceRun("465_ptr_cycle_mtrace", "7 7 5 7 12\n4 2 1 5\n7 1\n5\n9\n", 27);
}

// A type holding itself through a '[+]T' or a dictionary.
TEST(build_mgr, recursive_type_mtrace) {
	expectMtraceRun("470_recursive_type_mtrace",
		"13 4 2\n3 50\n2 1\n13\n7\n3 3\n9 5\n3\n8 6 8 8\n102 1\n4 3\n4\n", 341);
}

// Across importing files, through an alias, in a block and in an opaque type.
TEST(build_mgr, recursive_modules_mtrace) {
	expectMtraceRun("471_recursive_modules_mtrace", "3 2 3\n4\n3\n5 1\n", 79);
}

// Overloads differing only in lengths, rows, elements' owners or permissions.
TEST(build_mgr, overload_shape) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand("bin/palan ../test/testdata/build-mgr/466_overload_shape.pa"),
		"3 4 4 3 4 4 3\n1 2 1 2 1 2 2\n1 2 1 1 2\n1 1 2 1 2 2\n1 1 2 1\n1\n");
}

// Borrowed arrays through '@void', function pointers and fields; copies of dictionaries.
TEST(build_mgr, borrow_bindings) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand("bin/palan ../test/testdata/build-mgr/467_borrow_bindings.pa"),
		"3 2 1\n1\n2\n2 1 1\n");
}

// A string initializes a uint8 array, as C sizes it, through every way a value meets one.
TEST(build_mgr, str_lit_bytes_mtrace) {
	expectMtraceRun("483_str_lit_bytes_mtrace",
		"hello 6 ho 5 abc 0 8\nxyz 4 plus 5 hi hello\nbye wxyz abcdef 7 row\nfield 3 one\nval other\n"
		"6 128 195 169 0 122\nhex 6\nuni 4\n99\nglob 0\n", 32);
}
