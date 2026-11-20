/// palan build manager tests: C interop
///
/// @file cTests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include <cstdint>
#include <fstream>
#include "../test-base/testBase.h"
#include "buildMgrTestUtil.h"

using namespace std;

TEST(build_mgr, float_printf) {
	cleanTestEnv();
	// flo64 variable and float literal as printf args
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/019_float_printf.pa");
	ASSERT_EQ(output, "3.140000\n2.0\n1.500000\n");
}

TEST(build_mgr, c_float_return) {
	cleanTestEnv();
	// C function returning flo64/flo32: stored in var and used directly as arg
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/021_c_float_return.pa");
	ASSERT_EQ(output, "1.500000\n2.250000\n0.500000\n");
}

TEST(build_mgr, array_sprintf) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/032_array_sprintf.pa");
	ASSERT_EQ(output, "Hello, array! 2025\n");
}

TEST(build_mgr, cinclude_alias) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/049_cinclude_alias.pa");
	ASSERT_EQ(output, "42\n");
}

TEST(build_mgr, cinclude_typedef_size_t) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/084_cinclude_typedef_size_t.pa");
	ASSERT_EQ(output, "5\n");
}

TEST(build_mgr, cinclude_state) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/368_cinclude_state.pa");
	ASSERT_EQ(output, "8080 16\n");
}

TEST(build_mgr, null_strchr_notfound) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/105_null_strchr_notfound.pa");
	ASSERT_EQ(output, "not found\n");
}

TEST(build_mgr, null_strtok_loop) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/106_null_strtok_loop.pa");
	ASSERT_EQ(output, "a\nbb\nccc\n");
}

TEST(build_mgr, string_h_cmp) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/107_string_h_cmp.pa");
	ASSERT_EQ(output, "0\n-1\n0\n-1\n0\n0\n");
}

TEST(build_mgr, string_h_search) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/108_string_h_search.pa");
	ASSERT_EQ(output, "llo\nlo\nello\nlo\nllo\no world\nworld\nworld\n");
}

TEST(build_mgr, string_h_copy) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/109_string_h_copy.pa");
	ASSERT_EQ(output, "foobar\n\nhi\ndup-test\n");
}

TEST(build_mgr, string_h_misc) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/110_string_h_misc.pa");
	ASSERT_EQ(output, "Success\nNo such file or directory\nInterrupt\n0\n1\n3\n4\n5\n");
}

TEST(build_mgr, string_h_len) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/111_string_h_len.pa");
	ASSERT_EQ(output, "11\n5\n2\n2\n3\n");
}

TEST(build_mgr, string_h_ncopy) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/112_string_h_ncopy.pa");
	ASSERT_EQ(output, "hi\nfoobar\nhi\ndup\n");
}

TEST(build_mgr, string_h_lcmp) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/113_string_h_lcmp.pa");
	ASSERT_EQ(output, "hello 5\nfoobar 6\n5\n0\n-4\n");
}

TEST(build_mgr, string_h_mem_copy) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/114_string_h_mem_copy.pa");
	ASSERT_EQ(output, "abc\nhello\n0\n-1\n");
}

TEST(build_mgr, string_h_mem_search) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/115_string_h_mem_search.pa");
	ASSERT_EQ(output, "llo\nhel\nbarbaz\nab\n");
}

TEST(build_mgr, string_h_b_null) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/116_string_h_b_null.pa");
	ASSERT_EQ(output, "0\n-1\nhello\n0\n0\nmemchr not found\nmemmem not found\n");
}

TEST(build_mgr, ctype_h_basic) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/117_ctype_h_basic.pa");
	ASSERT_EQ(output, "8 0\n1024 0\n2 0\n2048 0\n512 0\n256 0\n32768 0\n16384 0\n4 0\n8192 0\n4096 0\n1 0\n1 0\na A\ni\nA a\n");
}

TEST(build_mgr, null_notfound_sweep) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/118_null_notfound_sweep.pa");
	ASSERT_EQ(output, "strstr: not found\nstrpbrk: not found\nstrchrnul: not null\nstrchrnul: []\n");
}

TEST(build_mgr, cinclude_struct_arg) {
	cleanTestEnv();
	// cinclude'd "tm" resolves through the same structDefs_ path as a native
	// struct; mktime(t) receives t as a borrowed pointer.
	string output = execTestCommand("env TZ=UTC bin/palan ../test/testdata/build-mgr/119_cinclude_struct_arg.pa");
	ASSERT_EQ(output, "946684800\n");
}

TEST(build_mgr, cinclude_struct_arg_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_cinclude_struct_arg_mtrace_bin "
		"../test/testdata/build-mgr/120_cinclude_struct_arg_mtrace.pa"), "");

	string traceFile = "/tmp/palan_cinclude_struct_arg_mtrace.log";
	execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_cinclude_struct_arg_mtrace_bin");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	// tm t: 1 calloc; mktime(t) passes t as a borrowed pointer -- no extra
	// alloc/free from the C call itself.
	EXPECT_EQ(allocs, 1) << "expected 1 alloc for tm t, got " << allocs;
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, time_h_category_a) {
	cleanTestEnv();
	// timer_t is a pointer-bottomed typedef chain (flattened by a c2ast fix),
	// so NULL type-checks against it via the generic-pointer rule.
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/124_time_h_category_a.pa");
	ASSERT_EQ(output, "366\n60.000000\n1\n-1\n-1\n");
}

TEST(build_mgr, time_h_struct_tm) {
	cleanTestEnv();
	// mktime normalizes tm_wday as a side effect, so asctime/asctime_r
	// (called after) correctly print "Thu".
	string output = execTestCommand("env TZ=UTC bin/palan ../test/testdata/build-mgr/125_time_h_struct_tm.pa");
	ASSERT_EQ(output, "0\n0\n0\n1970-01-01\nThu Jan  1 00:00:00 1970\nThu Jan  1 00:00:00 1970\n");
}

TEST(build_mgr, time_h_struct_timespec) {
	cleanTestEnv();
	// itimerspec's nested timespec fields (its.it_value.tv_sec) resolve through
	// the same embed-field chain native $T structs use. timer_gettime is called
	// with an invalid handle (timer_create is out of scope) and expected to fail.
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/126_time_h_struct_timespec.pa");
	ASSERT_EQ(output, "1\n1\n1\n1\n1\n1\n");
}

TEST(build_mgr, time_h_category_c) {
	cleanTestEnv();
	// time(NULL)'s live return is only boundary-checked (>= 0); ctime/ctime_r
	// are exercised against a separately fixed epoch value for determinism.
	string output = execTestCommand("env TZ=UTC bin/palan ../test/testdata/build-mgr/127_time_h_category_c.pa");
	ASSERT_EQ(output, "1\nThu Jan  1 00:00:00 1970\nThu Jan  1 00:00:00 1970\n1\n");
}

TEST(build_mgr, time_h_category_d) {
	cleanTestEnv();
	// gmtime's glibc static-buffer aliasing (2nd call overwrites the 1st result)
	// proves @!T is a real non-owning alias, not a copy; gmtime_r's caller-owned
	// buffer is unaffected. localtime/localtime_r are timezone-dependent, so get
	// only a loose sanity check.
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/128_time_h_category_d.pa");
	ASSERT_EQ(output, "1970\n2 2\n1970\n1\n1\n");
}

TEST(build_mgr, cinclude_arr_field_access) {
	// Regression: "char name[16]"/"long vals[4]" each used to collapse to a
	// single scalar field, so vals[3] read/wrote 24 bytes past the field's
	// true end -- past the whole struct's calloc'd block.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/130_cinclude_arr_field_access.pa");
	ASSERT_EQ(output, "5 65 90 100 400\n");
}

TEST(build_mgr, cinclude_ptr_slot_arr_field) {
	// A C "T *field[n];" is Palan's [n]@T/[n]@!T shape. Storing an address into
	// the slot is allowed despite the slot's read-only "mutable:false" default
	// (only write-through to the pointee is restricted).
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/140_cinclude_ptr_slot_arr_field.pa");
	ASSERT_EQ(output, "99\n");
}

TEST(build_mgr, sys_stat_h_s_ifdir_alias) {
	// S_IFDIR is `#define S_IFDIR __S_IFDIR`: the alias must resolve to the
	// same value as the macro it references, not be dropped from const-inlining.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/131_sys_stat_s_ifdir.pa");
	ASSERT_EQ(output, "dir-mode-ok\n");
}

TEST(build_mgr, deref_c_outparam_readback) {
	// A C out-param write through `@!T` is readable via `p[0]` -- an earlier
	// version documented this as "C side only"; that limitation is lifted.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/133_deref_c_outparam_readback.pa");
	ASSERT_EQ(output, "1\n1\n");
}

TEST(build_mgr, incomplete_struct_handle) {
	// A forward-declared-only tag from a real header is usable through a
	// non-owning pointer, which needs no layout.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/147_incomplete_struct_handle.pa");
	ASSERT_EQ(output, "0\n");
}

TEST(build_mgr, file_handle) {
	// FILE (`typedef struct _IO_FILE FILE;`) resolves as an alias for
	// _IO_FILE, so fopen/fclose signatures build instead of aborting at fromJson.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/148_file_handle.pa");
	ASSERT_EQ(output, "ok\n");
}

TEST(build_mgr, c_global_stderr) {
	// fprintf(stderr, ...) must actually write to fd 2. execTestCommand appends
	// stderr after a ":" only when non-empty, so the leading ":" here is itself
	// proof the bytes went to fd 2, not fd 1.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/149_c_global_stderr.pa");
	ASSERT_EQ(output, "out\n:err\n");
}

TEST(build_mgr, stdio_text_io) {
	// The expected string is the file's own content round-tripped through the
	// filesystem. `uint64 n = fread(...)`: fread returns size_t and a var-decl
	// initializer rejects cross-signedness narrowing.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/150_stdio_text_io.pa");
	ASSERT_EQ(output, "1:hello\n2:42 world\n3:raw n=3\nclose=0,0\n");
}

TEST(build_mgr, stdio_binary_seek) {
	// fwrite's void* parameter accepts any pntr(T). Also the repo's first use
	// of the SEEK_*/EOF constants c2ast exports from stdio.h.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/151_stdio_binary_seek.pa");
	ASSERT_EQ(output,
		"wrote=4\n"
		"size=32\n"
		"tell=16\n"
		"rec=33 n=1\n"
		"past=0 eof=1 err=0\n"
		"EOF=-1 SEEK_SET=0 SEEK_CUR=1 SEEK_END=2\n");
}

TEST(build_mgr, stdio_std_streams) {
	// stdin is read for real: execTestCommand runs via popen, so the "<"
	// redirect is honoured by the shell and inherited by the binary. The
	// trailing ":to-err\n" is execTestCommand's non-empty-stderr marker.
	cleanTestEnv();
	string output = execTestCommand(
		"bin/palan ../test/testdata/build-mgr/152_stdio_std_streams.pa"
		" < ../test/testdata/build-mgr/152_stdio_stdin_input.txt");
	ASSERT_EQ(output, "fd out=1 err=2 in=0\nin:piped-line\nto-out\n:to-err\n");
}

TEST(build_mgr, file_handle_no_autofree_mtrace) {
	// Scope exit must not free a `@!FILE` handle. The log's two allocations are
	// both libc.so.6 frames that parseMtraceLog skips; a control case (an owned
	// [4]int64) does produce a matching alloc/free, so frees==0 is a real signal.
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_file_handle_no_autofree_mtrace_bin "
		"../test/testdata/build-mgr/153_file_handle_no_autofree_mtrace.pa"), "");

	string traceFile = "/tmp/palan_file_handle_no_autofree_mtrace.log";
	execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_file_handle_no_autofree_mtrace_bin");

	auto [allocs, frees] = parseMtraceLog(traceFile);
	EXPECT_EQ(allocs, 0) << "fopen's allocation belongs to libc, not to Palan; got " << allocs;
	EXPECT_EQ(frees, 0) << "@!FILE must not be freed at scope exit; got " << frees << " free(s)";
}

TEST(build_mgr, stat_file_types) {
	// The mkfifo'd path is checked with lstat only: open()'ing a FIFO with no
	// peer would hang until execTestCommand's 5-second SIGKILL timeout.
	cleanTestEnv();
	execTestCommand("rm -f /tmp/pln_156_reg.txt /tmp/pln_156_link /tmp/pln_156_fifo");
	execTestCommand("ln -s /tmp/pln_156_reg.txt /tmp/pln_156_link");
	execTestCommand("mkfifo /tmp/pln_156_fifo");
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/156_stat_file_types.pa");
	ASSERT_EQ(output,
		"dir=1\n"
		"chr=1\n"
		"reg=1 size=10 mtime_nonzero=1\n"
		"lnk=1 target_reg=1\n"
		"fifo=1\n"
		"S_ISDIR=1 S_ISREG=0\n");
}

TEST(build_mgr, function_macro) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/440_function_macro.pa");
	ASSERT_EQ(output, "7 7\nx 5\n3 2\n3 1 12\n42 5\n9\n");
}

TEST(build_mgr, stat_mode_bits) {
	// umask(0) makes mkdir's permission bits deterministic regardless of the
	// caller's inherited umask; chmod's bits are unaffected by umask either way.
	cleanTestEnv();
	execTestCommand("rm -rf /tmp/pln_157_dir");
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/157_stat_mode_bits.pa");
	ASSERT_EQ(output,
		"mkdir_perm=504\n"
		"chmod_perm=488\n"
		"umask_roundtrip=0\n"
		"consts=448 56 7 511 4095\n");
}

TEST(build_mgr, strtol_endptr) {
	// `@!` on a pointer-typed local (not just primitive) produces pntr-of-pntr,
	// letting strtol's `char **endptr` idiom be written as `@!end` where
	// `end` is `@!int8`.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/159_strtol_endptr.pa");
	ASSERT_EQ(output, "42 abc\n");
}

TEST(build_mgr, alias_call_variadic_promote) {
	// Regression: sa_expr_member_call duplicated sa_expr_call's argument loop
	// without the variadic-promotion step, so an aliased call (`S.printf(...)`)
	// silently passed flo32 where va_arg reads flo64; the unaliased form
	// already promoted correctly.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/162_alias_call_variadic_promote.pa");
	ASSERT_EQ(output, "1.500000\n");
}

TEST(build_mgr, non_executable_stack) {
	// Palan bypasses the C driver, so palan-codegen must emit .note.GNU-stack
	// itself -- without it, the stack goes RWE as soon as any note-carrying
	// object (e.g. libc_nonshared.a's atexit) joins the link.
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_gnu_stack_bin "
		"../test/testdata/build-mgr/001_helloworld.pa"), "");

	string segs = execTestCommand("readelf -lW /tmp/palan_gnu_stack_bin");
	ASSERT_NE(segs.find("GNU_STACK"), string::npos);
	ASSERT_EQ(segs.find("RWE"),       string::npos);
}

TEST(build_mgr, qsort_callback) {
	// glibc's qsort calls a Palan function through the func-ref/LeaLabel
	// address. Comparator uses a typed pointee (@int32); bsearch_callback
	// below covers the C-idiomatic @void spelling.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/163_qsort_callback.pa");
	ASSERT_EQ(output, "1 3 4 5\n");
}

TEST(build_mgr, bsearch_callback) {
	// Comparator uses C's `const void *` signature (@void, read-only rebinding
	// to @int32); bsearch's `void *` result binds to a typed pointer -- a miss
	// comparing equal to NULL proves glibc consumes the comparator's return
	// value, not just calls it.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/164_bsearch_callback.pa");
	ASSERT_EQ(output,
		"4\n"
		"1\n");
}

TEST(build_mgr, atexit_callback) {
	// Two handlers prove LIFO dispatch order (C11 7.22.4.2) -- evidence they go
	// through glibc's __cxa_atexit registry, not invoked incidentally. Requires
	// the entry object's own __dso_handle. Both touch only .rodata strings:
	// _start frees its owned locals before `call exit`, so reading a Palan
	// local here would be a use-after-free.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/165_atexit_callback.pa");
	ASSERT_EQ(output,
		"hello\n"
		"last\n"
		"bye\n");
}

TEST(build_mgr, limits_constants) {
	// Regression: sa_cinclude()'s old "return if no functions" early-return
	// sat before constants-registration, so a function-less header's
	// (limits.h has zero C functions) constants were silently never registered.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/166_limits_constants.pa");
	ASSERT_EQ(output, "2147483647\n");
}

TEST(build_mgr, stdint) {
	// stdint.h declares zero C functions, only typedefs. Before the dedicated
	// typedefs section, int32_t was invisible: nothing carried its name-hint
	// through the old signature-piggyback path.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/167_stdint.pa");
	ASSERT_EQ(output, "5\n");
}

TEST(build_mgr, stdint_types) {
	// Complements stdint's minimal repro: multiple typedef'd types used
	// together plus one cross-type (int32_t -> int64_t) assignment.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/169_stdint_types.pa");
	ASSERT_EQ(output, "1000000 2000000 300000000 400000000 1000000\n");
}

TEST(build_mgr, typedef_chain) {
	// A multi-level chain (A->B->C->int32), a struct-bottomed typedef, and an
	// anonymous-struct typedef all register via the unconditional ast.typedefs
	// loop. The header also carries an unreferenced pointer-bottomed typedef
	// (P), proving the others still register when a header mixes one in.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/168_typedef_chain.pa");
	ASSERT_EQ(output, "1 1 1 3 7\n");
}

TEST(build_mgr, all_headers) {
	// cinclude all 13 supported headers simultaneously. A manual audit found
	// zero E_ConflictingTypedef diagnostics now that typedefs register
	// unconditionally rather than only when referenced by a function signature
	// -- this pins that result as a standing regression guard.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/170_all_headers.pa");
	ASSERT_EQ(output, "7\n");
}

TEST(build_mgr, link_math) {
	// build-mgr unions "libs" from every module's sa.json into -l<name> for
	// ld, so a cinclude `link` clause actually makes sqrt (in libm) linkable.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/171_link_math.pa");
	ASSERT_EQ(output, "1.414214\n");
}

TEST(build_mgr, link_obj) {
	cleanTestEnv();
	ASSERT_EQ(runWithCObj("295_link_obj"), "6\n");
}

// C calls the functions by name, and Palan calls them past the header's declarations.
TEST(build_mgr, c_export) {
	cleanTestEnv();
	ASSERT_EQ(runWithCObj("431_c_export"), "80 6\n30 11\n");
}

TEST(build_mgr, link_failure) {
	for (string opt : {"", "-o out/u "}) {
		cleanTestEnv();
		ofstream("out/u.h") << "int nowhere(void);\n";
		ofstream("out/u.pa") << "cinclude \"u.h\";\nnowhere();\n";
		string output = execTestCommand("bin/palan " + opt + "out/u.pa");
		EXPECT_TRUE(output.starts_with("return1:")) << opt << output;
		EXPECT_NE(output.find("undefined reference to `nowhere'"), string::npos) << opt << output;
	}
}

TEST(build_mgr, c_struct_byval) {
	// Each shape is checked against gcc's own code on the other side: the
	// callee prints what it received and returns a changed copy, and the
	// caller's struct stays unchanged.
	cleanTestEnv();
	ASSERT_EQ(runWithCObj("296_struct_byval"),
		"i8 1 2\n11 12 / 1 2\n"
		"i16 3 4\n13 14\n"
		"f2 1.5 2.5\n2.5 3.5\n"
		"d2 1.5 2.5\n2.5 3.5\n"
		"mix 0.5 7\n1.5 8\n"
		"mix2 5 0.5\n6 1.5\n"
		"b3 1 2 3\n11 2 13\n"
		"b5 1 2 3 4 5\n11 2 3 4 15\n"
		"b6 1000 2000 3000\n1001 2000 3001\n"
		"b7 1 2 3 4 5 6 7\n11 2 3 4 5 6 17\n"
		"b11 1 9 11\n11 9 21\n"
		"big 1 2 3\n11 2 13 / 1\n"
		"big20 1 20\n11 30\n"
		"u 3f800000\n3f800001\n"
		"int_regs_full 1 2 3 4 5 3 4 7\n"
		"flt_regs_full 1 2 3 4 5 6 7 1.5 2.5 9\n"
		"variadic_after 0.5 7 2.25 9\n");
}

TEST(build_mgr, c_flo80) {
	// long double on the stack after the registers, in a struct by value, and
	// alone in a returned struct, against gcc's code; C calls a cexport back.
	cleanTestEnv();
	ASSERT_EQ(runWithCObj("439_c_flo80"),
		"late 1 7 0.50 9 0.25\n17.750\n"
		"mix 3 1.50\n4 1.75 / 3 1.50\n"
		"1.2500\n"
		"0.66666666666666666668\n");
}

TEST(build_mgr, c_packed_aligned) {
	// gcc's side prints its own layout and what it received through each path.
	cleanTestEnv();
	ASSERT_EQ(runWithCObj("315_c_packed_aligned"),
		"Pk 13 1 5, Al 32, Fa 24 8 12, TPk 3, Outer 40 1 14\n"
		"pk 1 2 3\n11 12 13\n"
		"2 12 102\n13\n"
		"20 21 32\n"
		"fa 4 5 12\n"
		"tpk 6 7\n"
		"outer 8 9 10 11 13\n");
}

TEST(build_mgr, c_bit_field) {
	// Bs's bit-field shares an eightbyte with a float, passed by value.
	cleanTestEnv();
	ASSERT_EQ(runWithCObj("356_c_bitfield"),
		"Bf 12 4 8, Bp 7 6, Bl 16 8, Bs 12 8\n"
		"bf 1 9 33 2 -2 3\n"
		"1 2 3 / 4 8\n"
		"bp 4 0 0 5\n6\n"
		"bl 0 0 6\n8\n"
		"bs 1.50 0 2.50\n1.25 3.50\nbs 1.25 7 3.50\n"
		"9 33 -2\n"
		"bf 1 5 63 2 3 3\n"
		"bf 1 4 63 2 -3 3\n4 -3\n"
		"bp 4 -1000 0 5\n-1000\n"
		"bl -5 1000 6\n-5 1000\n"
		"bs 0.50 -100 0.75\n-100 7\n");
}

TEST(build_mgr, epoll_packed) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand("bin/palan ../test/testdata/build-mgr/316_epoll_packed.pa"),
		"1 1 1234605616436508552\n");
}

TEST(build_mgr, c_anon_member) {
	cleanTestEnv();
	ASSERT_EQ(runWithCObj("298_c_anon_member"),
		"1 2 3 4\n5 2.5 6\n"
		"v 1 2 3.5\n11 12 13.5\n"
		"f 1.5 2.5\n2.5 3.5\n");
}

TEST(build_mgr, cinclude_macro_state) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/299_cinclude_macro_state.pa");
	ASSERT_EQ(output, "ok\n100 3 2\n");
}

TEST(build_mgr, fopencookie) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/297_fopencookie.pa");
	ASSERT_EQ(output, "[hello 42]\n");
}

TEST(build_mgr, math_functions) {
	// Broad libm coverage (sqrt/pow/sin/.../hypot) run through the `link`
	// clause, including a variable and a nested expression as arguments.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/173_math_functions.pa");
	ASSERT_EQ(output,
		"1.414214\n"
		"1024.000000\n"
		"0.644218\n"
		"0.764842\n"
		"0.842288\n"
		"4.481689\n"
		"2.014903\n"
		"3.000000\n"
		"4.000000\n"
		"5.500000\n"
		"1.500000\n"
		"0.643501\n"
		"5.000000\n"
		"6.250000\n"
		"5.000000\n");
}

TEST(build_mgr, syscall_write) {
	// Also exercises calling a return-declaring syscall as a bare statement.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/176_syscall_write.pa");
	ASSERT_EQ(output, "hello, syscall\nsecond line\nwrote 12\n");
}

TEST(build_mgr, syscall_read) {
	// Same buffer crosses both pointer permissions: @!void into read, @void into write.
	cleanTestEnv();
	string output = execTestCommand(
		"bin/palan ../test/testdata/build-mgr/177_syscall_read.pa"
		" < ../test/testdata/build-mgr/177_syscall_read_input.txt");
	ASSERT_EQ(output, "syscall-read-line\nn==w\n");
}

TEST(build_mgr, syscall_getpid_matches_libc) {
	// Declared sys_getpid, not getpid: a cinclude'd C function resolves first
	// with no collision diagnostic, which would compare libc against itself.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/178_syscall_getpid.pa");
	ASSERT_EQ(output, "same=1 positive=1\n");
}

TEST(build_mgr, syscall_exit_status) {
	// execTestCommand exposes no numeric exit code, so `echo $?` reports it;
	// braces keep palan's stderr inside execTestCommand's " 2>out/err" redirect.
	cleanTestEnv();
	string output = execTestCommand(
		"{ bin/palan ../test/testdata/build-mgr/179_syscall_exit.pa; echo $?; }");
	ASSERT_EQ(output, "before-exit\n7\n");
}

TEST(build_mgr, syscall_negative_errno) {
	// Raw %rax stays -9 (EBADF): Palan does not translate it into libc errno.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/180_syscall_errno.pa");
	ASSERT_EQ(output, "rc=-9\n");
}

TEST(build_mgr, syscall_macro_number) {
	// A cinclude'd macro constant (SYS_write) used directly as a syscall
	// number, folded by gen-ast before it ever reaches SA.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/183_syscall_macro_number.pa");
	ASSERT_EQ(output, "macro syscall\nsecond\nn==7\n");
}

TEST(build_mgr, macro_array_size) {
	// A macro constant used as an array size, both for a local array
	// declaration and a struct field array declaration.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/185_macro_array_size.pa");
	ASSERT_EQ(output, "1 2 3 10 20 30\n");
}

TEST(build_mgr, macro_name_collision) {
	// A macro constant silently wins over a same-named variable, function
	// parameter, and Palan const -- every reference to the colliding name
	// resolves to the macro's value, ignoring the local declaration.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/186_macro_name_collision.pa");
	ASSERT_EQ(output, "7 7 7\n");
}

TEST(build_mgr, macro_const_suffix)
{
	// Same shape as ncurses' A_* attributes: a typedef cast of a suffixed literal, shifted.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/211_macro_const_suffix.pa");
	ASSERT_EQ(output, "262144 2097152 2359296 18446744073709551615 4294967040\n");
}

TEST(build_mgr, macro_const_untyped)
{
	// Unsuffixed macros take the parameter's type like source literals, so
	// ncurses' init_pair(1, COLOR_RED, COLOR_BLACK) needs no casts to short.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/212_macro_const_untyped.pa");
	ASSERT_EQ(output, "1 -1 -1\n40000 1\n");
}

TEST(build_mgr, cond_width)
{
	// A condition is tested at its own width: a uint8 argument's upper bits
	// are not clean, and 2^32 is nonzero only above bit 31.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/213_cond_width.pa");
	ASSERT_EQ(output, "0 1\n");
}

TEST(build_mgr, c_narrow_arg)
{
	// printf reads each narrow argument as a full int, in registers and on the stack.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/215_c_narrow_arg.pa");
	ASSERT_EQ(output, "-5 200 -300 60000 7 -6 250 -30000\n");
}

TEST(build_mgr, c_bool)
{
	// libc functions redeclared with _Bool check the C ABI without needing ncurses.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/216_c_bool.pa");
	ASSERT_EQ(output, "1 1 0\n1 0\ncond\n");
}

TEST(build_mgr, c_union_rw)
{
	// Both members of a cinclude'd union alias the same bytes: a write through
	// one is visible through the other (little-endian 258 = 0x0102).
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/187_c_union_rw.pa");
	ASSERT_EQ(output, "2 1\n259\n");
}

TEST(build_mgr, pthread_create_join)
{
	// The thread entry is a Palan function passed as C's void *(*)(void *).
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/188_pthread_create_join.pa");
	ASSERT_EQ(output, "42\n");
}

TEST(build_mgr, pthread_mutex)
{
	// A lost update between the two threads would show as a count below 200000.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/189_pthread_mutex.pa");
	ASSERT_EQ(output, "200000\n");
}

TEST(build_mgr, pthread_cond)
{
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/190_pthread_cond.pa");
	ASSERT_EQ(output, "ready=1\n");
}

TEST(build_mgr, c_enum) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/259_c_enum.pa");
	ASSERT_EQ(output, "2 1 x 100\n-1\n");
}

TEST(build_mgr, cross_cinclude_typedef) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/260_cross_cinclude_typedef.pa");
	ASSERT_EQ(output, "ok\n5\n");
}

// A C array field of any dimensions lies in its struct, rows and structs
// included, and is copied with it.
TEST(build_mgr, c_2d_arr_field) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/353_c_2d_arr_field.pa");
	ASSERT_EQ(output, "6 7 ab 112\n");
}

TEST(build_mgr, c_linked_list) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/286_c_linked_list.pa");
	ASSERT_EQ(output, "1 20 30\n");
}

TEST(build_mgr, c_ptr_typedef_timer) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/287_c_ptr_typedef_timer.pa");
	ASSERT_EQ(output, "0\n0\n0\n1\n0\n");
}

TEST(build_mgr, select_fd_set) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/289_select_fd_set.pa");
	ASSERT_EQ(output, "1\n1\n");
}

TEST(build_mgr, file_layout) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/290_file_layout.pa");
	ASSERT_EQ(output, "1\n");
}

TEST(build_mgr, frame_c) {
	cleanTestEnv();
	ASSERT_EQ(runWithCObj("301_frame_c"), "3 4\n14\n33\nAAAAAAA\n");
}

TEST(build_mgr, inet_ntoa) {
	cleanTestEnv();
	// The socket headers reach a GNU named variadic macro (`MEMBERS...`).
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/314_inet_ntoa.pa");
	ASSERT_EQ(output, "127.0.0.1 2130706433\n2\n");
}

TEST(build_mgr, cinclude_after_block) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/426_cinclude_after_block.pa");
	ASSERT_EQ(output, "in f\nin g -1\ntop\n");
}

TEST(build_mgr, c_func_alias_macro) {
	cleanTestEnv();
	// Each name is a glibc macro for another symbol; the old sscanf would read
	// "%as" as a GNU allocated string.
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/317_c_func_alias_macro.pa");
	ASSERT_EQ(output, "1 8\n0 No such file or directory\nb\n");
}
