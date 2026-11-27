/// build-mgr error case tests
///
/// @file errorTests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include "../test-base/testBase.h"
#include "../../src/common/PlnDefs.h"

using namespace std;
namespace fs = std::filesystem;

TEST(build_mgr_error, help) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan --help");
	ASSERT_NE(out.find("Usage: palan"), string::npos);
}

TEST(build_mgr_error, version) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan --version");
	ASSERT_NE(out.find("palan"), string::npos);
}

TEST(build_mgr_error, wrong_number_of_args) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan");
	ASSERT_NE(out.find("wrong number of arguments"), string::npos);
	// With -o the program is not run, so it takes no arguments.
	out = execTestCommand("bin/palan -o out/x ../test/testdata/build-mgr/359_args.pa x");
	ASSERT_NE(out.find("wrong number of arguments"), string::npos);
}

TEST(build_mgr_error, could_not_open_file) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan no_such_file.pa");
	ASSERT_NE(out.find("could not open file"), string::npos);
}

TEST(build_mgr_error, no_work_dir) {
	cleanTestEnv();
	string out = execTestCommand("env -u HOME -u PALAN_WORK bin/palan ../test/testdata/build-mgr/001_helloworld.pa");
	ASSERT_NE(out.find("palan: neither PALAN_WORK nor HOME is set"), string::npos);
	out = execTestCommand("env -u HOME PALAN_WORK= bin/palan --clean");
	ASSERT_NE(out.find("palan: neither PALAN_WORK nor HOME is set"), string::npos);
}

TEST(build_mgr_error, could_not_create_work_dir) {
	cleanTestEnv();
	ofstream("out/file").close();
	string out = execTestCommand("PALAN_WORK=out/file bin/palan ../test/testdata/build-mgr/001_helloworld.pa");
	ASSERT_NE(out.find("palan: could not create directory 'out/file/" PALAN_TARGET "'"), string::npos);
}

TEST(build_mgr_error, unsupported_target) {
	cleanTestEnv();
	// Reported ahead of the missing source: nothing runs for such a target.
	string out = execTestCommand("bin/palan --target aarch64-linux-gnu no_such_file.pa");
	ASSERT_NE(out.find("palan: unsupported target 'aarch64-linux-gnu'"), string::npos);
}

TEST(build_mgr_error, block_import_scope_out) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_045_block_import_scope.pa");
	ASSERT_NE(out.find("Undefined function"), string::npos);
}

TEST(build_mgr_error, same_module_identity) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_064_same_identity.pa");
	ASSERT_NE(out.find("plain_b/util.pa' has the same module identity 'util' as '"), string::npos) << out;
	ASSERT_NE(out.find("plain_a/util.pa'"), string::npos) << out;
}

TEST(build_mgr_error, allocators_identity) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_065_allocators_identity.pa");
	ASSERT_NE(out.find("has the same module identity '__allocators' as '"), string::npos) << out;
}

// A selective `export import` passes on only the names it lists.
TEST(build_mgr_error, reexport_unlisted) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_066_reexport_unlisted.pa");
	ASSERT_NE(out.find("error_066_reexport_unlisted.pa:2:11: error: Undefined function 'hiddenC'."), string::npos) << out;
}

TEST(build_mgr_error, unqualified_alias_call) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_046_unqualified_alias.pa");
	ASSERT_NE(out.find("requires a module alias qualifier"), string::npos);
}

TEST(build_mgr_error, ambiguous_call) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_047_ambiguous_call.pa");
	ASSERT_NE(out.find("Ambiguous function call"), string::npos);
}

TEST(build_mgr_error, unknown_struct_type) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_048_unknown_struct.pa");
	ASSERT_NE(out.find("unknown type"), string::npos);
}

TEST(build_mgr_error, unknown_field) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_049_unknown_field.pa");
	ASSERT_NE(out.find("has no field"), string::npos);
}

TEST(build_mgr_error, recursive_embed) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_050_recursive_embed.pa");
	ASSERT_NE(out.find(":1:1: error: struct 'A' recursively contains itself"), string::npos);
}

TEST(build_mgr_error, unsupported_field) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_051_unsupported_field.pa");
	ASSERT_NE(out.find(":1:1: error: unsupported struct field type"), string::npos);
}

TEST(build_mgr_error, inline_as_value) {
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_052_inline_as_value.pa");
	ASSERT_NE(out.find(":5:1: error: cannot convert 'Point' to 'int64'."), string::npos);
}

TEST(build_mgr_error, cinclude_arr_field_cast_size) {
	// A non-literal array size-expr ("(int)4.0" resolves to "cast", not
	// "lit-int") registers the struct as incomplete, a diagnostic rather than a crash.
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_054_cinclude_arr_field_cast_size.pa");
	ASSERT_NE(out.find("struct 'CastSized' has no known layout"), string::npos);
}

TEST(build_mgr_error, c_unsupported_sig) {
	// Regression: an unrepresentable C type on a signature used to abort via an
	// uncaught fromJson throw (rc=134); now diagnosed at the call.
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_055_c_unsupported_sig.pa");
	ASSERT_NE(out.find("cannot call C function 'cabs'"), string::npos) << out;
	ASSERT_NE(out.find("'double _Complex'"), string::npos);
	ASSERT_EQ(out.find("return0:"), string::npos);  // not killed by a signal (no abort)
}

TEST(build_mgr_error, dirent_owned_dir) {
	// DIR resolves to `struct __dirstream`, which dirent.h only
	// forward-declares; the diagnostic names it by its typedef.
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_056_dirent_owned_dir.pa");
	ASSERT_NE(out.find("struct 'DIR' is only forward-declared"), string::npos);
	ASSERT_EQ(out.find("return0:"), string::npos);  // not killed by a signal (no abort)
}

TEST(build_mgr_error, stdio_size_t_narrowing) {
	// fwrite/fread/strlen return size_t; `int64 n = fwrite(...)` crosses
	// signedness and a var-decl initializer refuses it (the loose `-> n` form
	// converts instead). Pins the rule at the C-ABI boundary.
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_057_stdio_size_t_narrowing.pa");
	ASSERT_NE(out.find("Implicit conversion from 'uint64' to 'int64'"), string::npos);
	ASSERT_EQ(out.find("return0:"), string::npos);  // not killed by a signal (no abort)
}

TEST(build_mgr_error, dirent_incomplete_field) {
	// Same requireCompleteStruct diagnostic as dirent_owned_dir above, but
	// reached via field access (`d.fd`) through a `@!DIR` handle.
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_058_dirent_incomplete_field.pa");
	ASSERT_NE(out.find("struct 'DIR' is only forward-declared"), string::npos);
	ASSERT_EQ(out.find("return0:"), string::npos);  // not killed by a signal (no abort)
}

TEST(build_mgr_error, stat_func_macro) {
	// FD_SET's body assigns, which has no Palan expression.
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_059_stat_func_macro.pa");
	ASSERT_NE(out.find(":4:1: error: C macro 'FD_SET' uses an assignment, which Palan cannot translate."), string::npos) << out;
	ASSERT_EQ(out.find("return0:"), string::npos);  // not killed by a signal (no abort)
}

TEST(build_mgr_error, arg_narrowing) {
	// Regression: an int64 argument to an int32 parameter used to pass through
	// unchecked, producing a bad `movq` width mismatch at the assembler stage.
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_060_arg_narrowing.pa");
	ASSERT_NE(out.find("Implicit conversion from 'int64' to 'int32'"), string::npos);
	ASSERT_EQ(out.find("return0:"), string::npos);  // not killed by a signal (no abort)
}

TEST(build_mgr_error, cinclude_not_found) {
	// Regression: gen-ast used to swallow a failed header read and let palan
	// exit 0 as if the header were empty.
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_061_cinclude_not_found.pa");
	ASSERT_NE(out.find("Failed to read C header"), string::npos);
	ASSERT_EQ(out.find("return0:"), string::npos);  // not killed by a signal (no abort)
}

TEST(build_mgr_error, missing_lib) {
	// A `link` clause naming a library ld can't find is not diagnosed by the
	// compiler: reproducing ld's search path belongs to ld, not Palan.
	cleanTestEnv();
	string out = execTestCommand("bin/palan ../test/testdata/build-mgr/error_062_missing_lib.pa");
	ASSERT_NE(out.find("cannot find -lnosuchlib"), string::npos);
}

TEST(build_mgr_error, assembler_not_found) {
	// `as` is the only PATH-resolved tool on the happy path (the other stages
	// launch by absolute /proc/self/exe path), so hiding PATH only breaks it.
	cleanTestEnv();
	string out = execTestCommand(
		"env PATH=/nonexistent bin/palan ../test/testdata/build-mgr/001_helloworld.pa");
	ASSERT_NE(out.find("failed to execute 'as'"), string::npos);
}

TEST(build_mgr_error, child_killed_by_signal) {
	// palan must exit -1 (255) on PlnSpawnStatus::Signaled, not crash itself.
	// ulimit -c 0 keeps the compiled program's core dump out of build/.
	cleanTestEnv();
	string out = execTestCommand(
		"ulimit -c 0; bin/palan ../test/testdata/build-mgr/error_063_abort.pa");
	ASSERT_NE(out.find("return1:"), string::npos);
}
