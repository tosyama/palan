/// palan build manager tests: modules and calls
///
/// @file moduleTests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include "../test-base/testBase.h"
#include "../../src/common/PlnDefs.h"
#include "buildMgrTestUtil.h"

using namespace std;
namespace fs = std::filesystem;

static string targetWorkDir() {
	return string(getenv("HOME")) + "/.palan/work/" PALAN_TARGET;
}

// execTestCommand gives up after 5 seconds and has one err file, so the builds
// are started directly and each writes its own output.
static void expectConcurrentRuns(const vector<pair<string, string>>& programs) {
	const int rounds = 3;
	cleanTestEnv();
	string script;
	for (int r = 0; r < rounds; r++)
		for (size_t p = 0; p < programs.size(); p++)
			script += "bin/palan ../test/testdata/build-mgr/" + programs[p].first + " > out/run"
				+ to_string(p) + "_" + to_string(r) + " 2>&1 &\n";
	script += "wait\n";
	ASSERT_EQ(system(script.c_str()), 0);
	for (int r = 0; r < rounds; r++)
		for (size_t p = 0; p < programs.size(); p++) {
			ifstream f("out/run" + to_string(p) + "_" + to_string(r));
			stringstream ss;
			ss << f.rdbuf();
			EXPECT_EQ(ss.str(), programs[p].second) << programs[p].first << " round " << r;
		}
	EXPECT_EQ(execTestCommand("find " + targetWorkDir() + " -name '*.tmp.*'"), "");
}

TEST(build_mgr, concurrent_programs_in_one_dir) {
	expectConcurrentRuns({{"391_concurrent_a.pa", "a 7\n"}, {"392_concurrent_b.pa", "b 1.5\n"}});
}

TEST(build_mgr, concurrent_programs_sharing_source) {
	expectConcurrentRuns({{"393_shared_a.pa", "a 5\n"}, {"394_shared_b.pa", "b 4 5\n"}});
}

// One file name in two directories: the declarations tell the modules apart,
// functions of one name and parameters included.
TEST(build_mgr, same_file_name_with_library) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand("bin/palan ../test/testdata/build-mgr/395_same_name_library.pa"), "11 22\n");
}

TEST(build_mgr, escaped_file_names_coexist) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand("bin/palan ../test/testdata/build-mgr/396_escaped_names.pa"), "3 4\n");
}

TEST(build_mgr, library_module_same_for_any_entry) {
	cleanTestEnv();
	string dir = fs::weakly_canonical("../test/testdata/build-mgr").string();
	string obj = targetWorkDir() + dir + "/lib_module_id/entry_demo.pa.debug.o";
	ASSERT_EQ(execTestCommand("bin/palan ../test/testdata/build-mgr/397_library_entry_demo.pa"), "15\n");
	fs::copy_file(obj, "out/entry_demo_imported.o");
	ASSERT_EQ(execTestCommand("bin/palan ../test/testdata/build-mgr/lib_module_id/entry_demo.pa"), "demo 12\n");
	ASSERT_EQ(execTestCommand("cmp out/entry_demo_imported.o " + obj), "");
}

TEST(build_mgr, module_same_at_any_location) {
	cleanTestEnv();
	string dir = fs::weakly_canonical("../test/testdata/build-mgr/lib_module_id").string();
	ASSERT_EQ(execTestCommand("bin/palan ../test/testdata/build-mgr/lib_module_id/local_type.pa"), "7\n");
	fs::copy_file("../test/testdata/build-mgr/lib_module_id/local_type.pa", "out/local_type.pa");
	ASSERT_EQ(execTestCommand("bin/palan out/local_type.pa"), "7\n");
	string moved = fs::weakly_canonical("out").string();
	ASSERT_EQ(execTestCommand("cmp " + targetWorkDir() + dir + "/local_type.pa.debug.s "
		+ targetWorkDir() + moved + "/local_type.pa.debug.s"), "");
}

TEST(build_mgr, import_basic) {
	cleanTestEnv();
	string output = execTestCommand(
		"bin/palan ../test/testdata/build-mgr/023_import_basic.pa");
	ASSERT_EQ(output, "7\n");
}

TEST(build_mgr, import_mutual) {
	cleanTestEnv();
	string output = execTestCommand(
		"bin/palan ../test/testdata/build-mgr/024_import_mutual.pa");
	ASSERT_EQ(output, "10\n13\n");
}

// The importer's own K does not size the imported alias.
TEST(build_mgr, import_alias_size) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand("bin/palan ../test/testdata/build-mgr/414_import_alias_size.pa"), "3 6 6\n");
}

TEST(build_mgr, block_import) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/045_block_import.pa");
	ASSERT_EQ(output, "4\n");
}

TEST(build_mgr, selective_import) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/046_selective_import.pa");
	ASSERT_EQ(output, "16\n");
}

TEST(build_mgr, alias_import) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/047_alias_import.pa");
	ASSERT_EQ(output, "9\n8\n");
}

// An alias does not hide a conversion, which is never written qualified.
TEST(build_mgr, literal_conversion_alias) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/409_literal_conversion_alias.pa");
	ASSERT_EQ(output, "alias\narg\n");
}

// An operator cannot be written qualified, so an alias import brings it
// unqualified; a selective import names it.
TEST(build_mgr, operator_func_import) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/381_operator_func_import.pa");
	ASSERT_EQ(output, "2 4 6\n-1 -2 -3\n");
}

TEST(build_mgr, selective_alias_import) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/048_selective_alias_import.pa");
	ASSERT_EQ(output, "25\n");
}

TEST(build_mgr, link_import) {
	// This file has no `link` clause -- imported lib_sqrt.pa is the one
	// requesting libm, proving the "libs" union is whole-program, not per-file.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/172_link_import.pa");
	ASSERT_EQ(output, "11\n");
}

TEST(build_mgr, output_name_injection) {
	// The -o argument used to be concatenated unquoted into ld's shell command
	// line. The marker file must NOT exist (injection closed); a file with the
	// exact literal name must exist (the argument reached ld verbatim).
	cleanTestEnv();
	execTestCommand("rm -f PWNED");
	string output = execTestCommand(
		"bin/palan -o 'out/b_$(touch PWNED)' ../test/testdata/build-mgr/001_helloworld.pa");
	ASSERT_EQ(output, "");
	ASSERT_FALSE(fs::exists("PWNED"));
	ASSERT_TRUE(fs::exists("out/b_$(touch PWNED)"));
}

TEST(build_mgr, source_path_injection) {
	// The input .pa path used to flow unquoted through fs::weakly_canonical
	// into every pipeline stage's command line. Metacharacter filenames aren't
	// checked into git, so the hostile name is created at runtime.
	cleanTestEnv();
	execTestCommand("rm -f PWNED");
	execTestCommand(
		"cp ../test/testdata/build-mgr/174_shell_metachar_source.pa 'out/inj_$(touch PWNED).pa'");
	string output = execTestCommand("bin/palan 'out/inj_$(touch PWNED).pa'");
	ASSERT_EQ(output, "Hello World!\n");
	ASSERT_FALSE(fs::exists("PWNED"));
}

TEST(build_mgr, import_path_injection) {
	// An import path read back out of ast.json used to be concatenated
	// unquoted into the next palan-gen-ast shell command line. A nonexistent
	// metacharacter path is rejected earlier by fs::exists(), so the imported
	// file must actually exist under the hostile name to reach that code path.
	cleanTestEnv();
	execTestCommand("rm -f PWNED");
	execTestCommand("cp ../test/testdata/build-mgr/175_import_path_injection.pa out/");
	execTestCommand(
		"printf 'export func add(int32 a, int32 b) -> int32 { return a + b; }\\n' "
		"> 'out/$(touch PWNED).pa'");
	string output = execTestCommand("bin/palan out/175_import_path_injection.pa");
	ASSERT_EQ(output, "7\n");
	ASSERT_FALSE(fs::exists("PWNED"));
}

static void writeModule(const string& path, const string& source) {
	fs::create_directories(fs::path(path).parent_path());
	ofstream(path) << source;
}

static const string printF = "cinclude <stdio.h>;\nprintf(\"%d\\n\", f());\n";

// The relative import inside real.pa means next to the real file, not the link.
TEST(build_mgr, import_symlink_file) {
	cleanTestEnv();
	writeModule("out/b/real.pa", "import \"sib.pa\";\nexport func f() -> int32 { return g(); }\n");
	writeModule("out/b/sib.pa", "export func g() -> int32 { return 7; }\n");
	writeModule("out/a/main.pa", "import \"link.pa\";\n" + printF);
	fs::create_symlink("../b/real.pa", "out/a/link.pa");
	ASSERT_EQ(execTestCommand("bin/palan out/a/main.pa"), "7\n");
}

// A type's '->' comes with the type: main.pa reaches T only through Q, and
// copying a Q into an existing one still calls it.
TEST(build_mgr, copy_func_reaches_with_its_type) {
	cleanTestEnv();
	writeModule("out/t.pa", "cinclude <stdio.h>;\nexport type T { [+]int32 v; int32 mode; };\n"
		"func ->(@T src, @!T dst) { src.v -> dst.v; printf(\"keeps %d \", dst.mode); }\n");
	writeModule("out/q.pa", "import \"t.pa\";\nexport type Q { T t; int32 n; };\n"
		"export func mkq(int32 m) -> Q q { m -> q.t.mode; m -> q.n; }\n");
	writeModule("out/main.pa", "cinclude <stdio.h>;\nimport \"q.pa\";\n"
		"Q p = mkq(1); Q q = mkq(2);\np -> q;\np.t -> q.t;\nprintf(\"%d %d\\n\", q.t.mode, q.n);\n");
	ASSERT_EQ(execTestCommand("bin/palan out/main.pa"), "keeps 2 keeps 2 2 1\n");
}

TEST(build_mgr, import_symlink_dir) {
	cleanTestEnv();
	writeModule("out/real/lib.pa", "export func f() -> int32 { return 7; }\n");
	writeModule("out/a/main.pa", "import \"linked/lib.pa\";\n" + printF);
	fs::create_directory_symlink("../real", "out/a/linked");
	ASSERT_EQ(execTestCommand("bin/palan out/a/main.pa"), "7\n");
}

TEST(build_mgr, import_one_file_two_spellings) {
	cleanTestEnv();
	writeModule("out/real.pa", "export func f() -> int32 { return 7; }\n");
	fs::create_symlink("real.pa", "out/link.pa");
	writeModule("out/main.pa", "import \"link.pa\";\nimport \"real.pa\";\nimport \"./real.pa\";\n" + printF);
	ASSERT_EQ(execTestCommand("bin/palan out/main.pa"), "7\n");
}

TEST(build_mgr, import_cycle_across_dirs) {
	cleanTestEnv();
	writeModule("out/d1/a.pa", "import \"../d2/b.pa\";\nexport func one() -> int32 { return 1; }\n"
		"cinclude <stdio.h>;\nprintf(\"%d\\n\", two());\n");
	writeModule("out/d2/b.pa", "import \"./../d1/a.pa\";\nimport \"./b.pa\";\n"
		"export func two() -> int32 { return one() + 1; }\n");
	ASSERT_EQ(execTestCommand("bin/palan out/d1/a.pa"), "2\n");
}

TEST(build_mgr, import_absolute_path) {
	cleanTestEnv();
	writeModule("out/lib/abs.pa", "export func f() -> int32 { return 7; }\n");
	writeModule("out/main.pa", "import \"" + fs::weakly_canonical("out/lib/abs.pa").string() + "\";\n" + printF);
	ASSERT_EQ(execTestCommand("bin/palan out/main.pa"), "7\n");
}

TEST(build_mgr, import_syscall) {
	// The library module exports no code at all, only a prototype, so this also
	// covers assembling and linking an object with no callable symbol.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/181_import_syscall.pa");
	ASSERT_EQ(output, "import syscall\nsecond\nn==7\n");
}

TEST(build_mgr, import_syscall_alias) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/182_import_syscall_alias.pa");
	ASSERT_EQ(output, "alias syscall\nn==14\n");
}

TEST(build_mgr, import_syscall_macro) {
	// The exported syscall's macro-derived number is already folded in the
	// library module's own ast.json (ast.export gets the same walk as the
	// library's own statements), so the importing module needs no cinclude
	// of its own to resolve it.
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/184_import_syscall_macro.pa");
	ASSERT_EQ(output, "import macro syscall\nsecond\nn==7\n");
}

TEST(build_mgr, import_float)
{
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/201_import_float.pa");
	ASSERT_EQ(output, "25.000000 1.500000\n");
}

TEST(build_mgr, clean) {
	cleanTestEnv();
	// Its own work directories, so the work files the other tests reuse stay.
	fs::create_directories("out/home/.palan/work/" PALAN_TARGET);
	ASSERT_EQ(execTestCommand("HOME=out/home bin/palan --clean"), "");
	ASSERT_FALSE(fs::exists("out/home/.palan/work/" PALAN_TARGET));

	fs::create_directories("out/work/" PALAN_TARGET);
	fs::create_directories("out/work/mine");
	ASSERT_EQ(execTestCommand("PALAN_WORK=out/work bin/palan --clean"), "");
	ASSERT_FALSE(fs::exists("out/work/" PALAN_TARGET));
	ASSERT_TRUE(fs::exists("out/work/mine"));
}

TEST(build_mgr, palan_work) {
	cleanTestEnv();
	string out = execTestCommand("PALAN_WORK=out/work bin/palan ../test/testdata/build-mgr/001_helloworld.pa");
	ASSERT_EQ(out, "Hello World!\n");
	ASSERT_TRUE(fs::exists("out/work/" PALAN_TARGET + fs::absolute("../test/testdata/build-mgr/001_helloworld.pa").lexically_normal().string() + ".ast.json"));
}

TEST(build_mgr, method_call) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/262_method_call.pa");
	ASSERT_EQ(output, "121 5\n7\n1 1\n");
}

TEST(build_mgr, import_type) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/263_import_type.pa");
	ASSERT_EQ(output, "25.0\n3.0 7 5\n1.0 0 5\n");
}

TEST(build_mgr, import_type_field) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/264_import_type_field.pa");
	ASSERT_EQ(output, "1.0 2.0 7 3.0 5 4.0\n");
}

TEST(build_mgr, import_mutual_type) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/266_import_mutual_type.pa");
	ASSERT_EQ(output, "4.5 4.0 2 9\n");
}

TEST(build_mgr, chain_call_mtrace)
{
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_chain_call_bin "
		"../test/testdata/build-mgr/281_chain_call_mtrace.pa"), "");

	string traceFile = "/tmp/palan_chain_call_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_chain_call_bin");
	ASSERT_EQ(output, "0.6 0.0 0.8\n"
	                  "1.4\n"
	                  "9.0\n"
	                  "105\n"
	                  "2\n");

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

TEST(build_mgr, chain_lit_mtrace)
{
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_chain_lit_bin "
		"../test/testdata/build-mgr/282_chain_lit_mtrace.pa"), "");

	string traceFile = "/tmp/palan_chain_lit_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_chain_lit_bin");
	ASSERT_EQ(output, "2.0 4.0 6.0\n"
	                  "18.0\n"
	                  "7\n");

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

TEST(build_mgr, block_type_scope_mtrace) {
	// Same-named block-local structs of different layouts each get their own
	// allocator.
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_block_type_scope_bin "
		"../test/testdata/build-mgr/285_block_type_scope_mtrace.pa"), "");

	string traceFile = "/tmp/palan_block_type_scope_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_block_type_scope_bin");
	ASSERT_EQ(output, "5 2\n7\n");

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
	EXPECT_GT(allocs, 0);
	EXPECT_EQ(allocs, frees)
		<< "malloc/free not balanced: " << allocs << " allocs, " << frees << " frees";
}

TEST(build_mgr, import_private_type) {
	cleanTestEnv();
	// Each module's P gets its own allocators: copying lib's [2]P with main's
	// 4-byte copy would print "5 3 0".
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/303_import_private_type.pa");
	ASSERT_EQ(output, "5 3 4\n");
}

TEST(build_mgr, import_private_dep_type) {
	cleanTestEnv();
	// Q brings lib's unexported P along; main's own P does not clash with it.
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/304_import_private_dep_type.pa");
	ASSERT_EQ(output, "6 7\n");
}

TEST(build_mgr, import_private_alias_type) {
	cleanTestEnv();
	// PP names lib's P wherever it is written, not main's P of the same name.
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/305_import_private_alias_type.pa");
	ASSERT_EQ(output, "8 9\n");
}

TEST(build_mgr, qualified_type_entries) {
	cleanTestEnv();
	// V.P reaches lib's L through an alias chain, a parameter, a tapple, a
	// cast and sizeof alike, never main's L.
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/306_qualified_type_entries.pa");
	ASSERT_EQ(output, "5 6 2 8 16\n");
}

TEST(build_mgr, import_hidden_type_name_reuse) {
	cleanTestEnv();
	// main's R coexists with lib's R, written only as V.R or reached through S.
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/307_import_hidden_type_name_reuse.pa");
	ASSERT_EQ(output, "1 2 30000000000\n7 5\n");
}

TEST(build_mgr, overload_symbols) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/320_overload_symbols.pa");
	ASSERT_EQ(output, "int 3\nfloat 2.5\n8 3.0\n1 3 4 5 1\n");
}

TEST(build_mgr, overload_modules) {
	cleanTestEnv();
	// A function exported under a libc name does not take the place of the
	// libc one, and two modules can export the same function.
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/321_overload_modules.pa");
	ASSERT_EQ(output, "8 3.0 12\n1 2\n5 100\n");
}

TEST(build_mgr, nested_func_symbols) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/322_nested_func_symbols.pa");
	ASSERT_EQ(output, "5000\n60000\n1 2 430\n");
}

TEST(build_mgr, chain_method_call) {
	cleanTestEnv();
	string output = execTestCommand("bin/palan ../test/testdata/build-mgr/323_chain_method_call.pa");
	// The chained value is evaluated before the receiver's index.
	ASSERT_EQ(output, "324\n1 0 10\n8 7\n11\n8\n10 10 10\n");
}

TEST(build_mgr, chain_method_mtrace) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"bin/palan -o /tmp/palan_chain_method_bin "
		"../test/testdata/build-mgr/324_chain_method_mtrace.pa"), "");

	string traceFile = "/tmp/palan_chain_method_mtrace.log";
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " "
		"/tmp/palan_chain_method_bin");
	ASSERT_EQ(output, "30\n34\n40 40 2\n");

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

// The entry is chosen at link time: an imported module's top level runs only
// when that module is the entry, and its object is the same either way.
TEST(build_mgr, entry_at_link) {
	cleanTestEnv();
	string dir = fs::weakly_canonical("../test/testdata/build-mgr").string();
	string libObj = targetWorkDir() + dir + "/lib_entry_demo.pa.debug.o";

	ASSERT_EQ(execTestCommand("bin/palan ../test/testdata/build-mgr/358_entry_at_link.pa"), "15\n");
	fs::copy_file(libObj, "out/lib_entry_demo_imported.o");
	ASSERT_EQ(execTestCommand("bin/palan ../test/testdata/build-mgr/lib_entry_demo.pa"), "demo 12\n");
	ASSERT_EQ(execTestCommand("cmp out/lib_entry_demo_imported.o " + libObj), "");
}

TEST(build_mgr, implicit_args) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand("bin/palan -o out/args_bin ../test/testdata/build-mgr/359_args.pa"), "");
	ASSERT_EQ(execTestCommand("out/args_bin"), "1\nrebound\n");
	ASSERT_EQ(execTestCommand("out/args_bin x 'y z'"), "3\n1:x\n2:y z\nrebound\n");

	// An imported module's top level reads `args` only when it is the entry.
	ASSERT_EQ(execTestCommand("bin/palan ../test/testdata/build-mgr/360_args_import.pa"), "hello main\n");
	ASSERT_EQ(execTestCommand("bin/palan -o out/lib_args_bin ../test/testdata/build-mgr/lib_args_demo.pa"), "");
	ASSERT_EQ(execTestCommand("out/lib_args_bin a b"), "hello a\nhello b\n");
}

// Without -o, everything after the source goes to the program, options too,
// and the executable is kept in the working directory, not the current one.
TEST(build_mgr, run_args) {
	cleanTestEnv();
	string dir = fs::weakly_canonical("../test/testdata/build-mgr").string();
	string bin = targetWorkDir() + dir + "/359_args.pa.debug.d/out";
	fs::remove(bin);
	ASSERT_EQ(execTestCommand("bin/palan ../test/testdata/build-mgr/359_args.pa x -o 'y z'"),
		"4\n1:x\n2:-o\n3:y z\nrebound\n");
	EXPECT_TRUE(fs::exists(bin));
	EXPECT_FALSE(fs::exists("a.out"));
	ASSERT_EQ(execTestCommand("bin/palan ../test/testdata/build-mgr/lib_args_demo.pa a"), "hello a\n");
}

// The header of `args` is in the frame: only the copy is allocated and freed.
TEST(build_mgr, implicit_args_mtrace) {
	expectMtraceRun("361_args_mtrace", "4 a end\n", 2, " a b");
}

// A callback C calls with fixed arguments (qsort's comparator) reaches what it
// needs through globals.
TEST(build_mgr, global_var) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand("bin/palan ../test/testdata/build-mgr/370_global.pa"),
		"1 3 5\n5 3 1\n1 13 4\n7\n");
}

// What a global owns lives as long as the process: only the value it was
// moved away from is freed.
TEST(build_mgr, global_var_mtrace) {
	cleanTestEnv();
	string bin = "/tmp/palan_global_mtrace_bin";
	ASSERT_EQ(execTestCommand("bin/palan -o " + bin + " ../test/testdata/build-mgr/371_global_mtrace.pa"), "");
	string traceFile = "/tmp/palan_global_mtrace.log";
	ASSERT_EQ(execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " " + bin), "6\n15\n");
	auto [allocs, frees] = parseMtraceLog(traceFile);
	EXPECT_EQ(allocs, 2);
	EXPECT_EQ(frees, 1);
}

// An imported module's globals are initialized before the entry's top level runs.
TEST(build_mgr, import_global_var) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand("bin/palan ../test/testdata/build-mgr/372_import_global.pa"),
		"lib_global init\nmain\n11\n12\n");
}

// A module imported twice is initialized once, after the modules it imports.
TEST(build_mgr, init_order) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand("bin/palan ../test/testdata/build-mgr/416_init_order.pa"),
		"init b\ninit a\n8 7\n");
}

// In a circle, the module reached second sees the first's globals still zero.
TEST(build_mgr, init_circle) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand("bin/palan ../test/testdata/build-mgr/417_init_circle.pa"),
		"b sees 0\na sees 2\n1 2\n");
}

TEST(build_mgr, global_before_decl) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand("bin/palan ../test/testdata/build-mgr/418_global_before_decl.pa"), "5\n1\n");
}

// The first import's initializer starts tracing, so the owned global of a
// module imported twice shows its one allocation, never freed.
TEST(build_mgr, import_global_var_mtrace) {
	cleanTestEnv();
	string bin = "/tmp/palan_import_global_mtrace_bin";
	ASSERT_EQ(execTestCommand("bin/palan -o " + bin + " ../test/testdata/build-mgr/419_import_global_mtrace.pa"), "");
	string traceFile = "/tmp/palan_import_global_mtrace.log";
	ASSERT_EQ(execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " " + bin), "6 12\n");
	auto [allocs, frees] = parseMtraceLog(traceFile);
	EXPECT_EQ(allocs, 1);
	EXPECT_EQ(frees, 0);
}

// Both modules read and write one counter: the importer's top level freely,
// the functions a `global` statement permits, and its own global's initializer.
TEST(build_mgr, export_global) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand("bin/palan ../test/testdata/build-mgr/420_export_global.pa"),
		"lib 1111\nmain 1111 1111 20\n");
}

TEST(build_mgr, export_global_alias) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand("bin/palan ../test/testdata/build-mgr/421_export_global_alias.pa"), "23 23\n");
}

// Replacing an imported owned global frees what it held, once; what it
// owns afterwards lives on.
TEST(build_mgr, export_global_mtrace) {
	cleanTestEnv();
	string bin = "/tmp/palan_export_global_mtrace_bin";
	ASSERT_EQ(execTestCommand("bin/palan -o " + bin + " ../test/testdata/build-mgr/422_export_global_mtrace.pa"), "");
	string traceFile = "/tmp/palan_export_global_mtrace.log";
	ASSERT_EQ(execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=" + traceFile + " " + bin), "18 3 5\n");
	auto [allocs, frees] = parseMtraceLog(traceFile);
	EXPECT_EQ(allocs, 5);
	EXPECT_EQ(frees, 2);
}

TEST(build_mgr, import_global_in_func) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand("bin/palan ../test/testdata/build-mgr/423_import_global_in_func.pa"), "15\n");
}

TEST(build_mgr, global_after_unrelated_call) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand("bin/palan ../test/testdata/build-mgr/389_global_after_unrelated_call.pa"), "x\n1\n");
}

static string reusePath(const string& name) {
	return targetWorkDir() + (fs::current_path() / "out" / name).string();
}

static fs::file_time_type objTime(const string& source) {
	return fs::last_write_time(reusePath(source) + ".debug.o");
}

// writeOldSource's times go back, which a work file left by an earlier run would hide.
static void cleanReuseEnv() {
	cleanTestEnv();
	fs::remove_all(reusePath(""));
}

// Backdated, or a build finishing within one clock tick of the write would see
// its result as not newer and compile again.
static void writeOldSource(const string& name, const string& content) {
	string path = "out/" + name;
	ofstream(path) << content;
	fs::last_write_time(path, fs::file_time_type::clock::now() - chrono::minutes(1));
}

static void writeImportChain() {
	cleanReuseEnv();
	writeOldSource("c.pa", "export func c() -> int32 { return 1; }\n");
	writeOldSource("b.pa", "import \"c.pa\";\nexport func b() -> int32 { return c(); }\n");
	writeOldSource("a.pa", "cinclude <stdio.h>;\nimport \"b.pa\";\nprintf(\"%d\\n\", b());\n");
}

struct ChainTimes {
	fs::file_time_type a = objTime("a.pa"), b = objTime("b.pa"), c = objTime("c.pa");
};

TEST(build_mgr, reuse_nothing_changed) {
	writeImportChain();
	ASSERT_EQ(execTestCommand("bin/palan out/a.pa"), "1\n");
	ChainTimes before;
	auto astTime = fs::last_write_time(reusePath("a.pa") + ".ast.json");
	ASSERT_EQ(execTestCommand("bin/palan out/a.pa"), "1\n");
	ChainTimes after;
	EXPECT_EQ(after.a, before.a);
	EXPECT_EQ(after.b, before.b);
	EXPECT_EQ(after.c, before.c);
	EXPECT_EQ(fs::last_write_time(reusePath("a.pa") + ".ast.json"), astTime);
}

TEST(build_mgr, reuse_own_source_changed) {
	writeImportChain();
	ASSERT_EQ(execTestCommand("bin/palan out/a.pa"), "1\n");
	ChainTimes before;
	ofstream("out/a.pa") << "cinclude <stdio.h>;\nimport \"b.pa\";\nprintf(\"%d\\n\", b() + 1);\n";
	ASSERT_EQ(execTestCommand("bin/palan out/a.pa"), "2\n");
	ChainTimes after;
	EXPECT_NE(after.a, before.a);
	EXPECT_EQ(after.b, before.b);
	EXPECT_EQ(after.c, before.c);
}

TEST(build_mgr, reuse_import_changed) {
	writeImportChain();
	ASSERT_EQ(execTestCommand("bin/palan out/a.pa"), "1\n");
	ChainTimes before;
	ofstream("out/b.pa") << "import \"c.pa\";\nexport func b() -> int32 { return c() + 1; }\n";
	ASSERT_EQ(execTestCommand("bin/palan out/a.pa"), "2\n");
	ChainTimes after;
	EXPECT_NE(after.a, before.a);
	EXPECT_NE(after.b, before.b);
	EXPECT_EQ(after.c, before.c);
}

TEST(build_mgr, reuse_indirect_import_changed) {
	writeImportChain();
	ASSERT_EQ(execTestCommand("bin/palan out/a.pa"), "1\n");
	ChainTimes before;
	ofstream("out/c.pa") << "export func c() -> int32 { return 5; }\n";
	ASSERT_EQ(execTestCommand("bin/palan out/a.pa"), "5\n");
	ChainTimes after;
	EXPECT_NE(after.a, before.a);
	EXPECT_NE(after.b, before.b);
	EXPECT_NE(after.c, before.c);
}

// The caller holds the default, so changing it compiles the caller again.
TEST(build_mgr, reuse_default_changed) {
	cleanReuseEnv();
	writeOldSource("c.pa", "export func c(int32 k = 1) -> int32 { return k; }\n");
	writeOldSource("b.pa", "import \"c.pa\";\nexport func b() -> int32 { return c(); }\n");
	writeOldSource("a.pa", "cinclude <stdio.h>;\nimport \"b.pa\";\nprintf(\"%d\\n\", b());\n");
	ASSERT_EQ(execTestCommand("bin/palan out/a.pa"), "1\n");
	ChainTimes before;
	ofstream("out/c.pa") << "export func c(int32 k = 5) -> int32 { return k; }\n";
	ASSERT_EQ(execTestCommand("bin/palan out/a.pa"), "5\n");
	ChainTimes after;
	EXPECT_NE(after.b, before.b);
}

TEST(build_mgr, reuse_mutual_import) {
	cleanReuseEnv();
	writeOldSource("m1.pa", "cinclude <stdio.h>;\nimport \"m2.pa\";\nexport func f1() -> int32 { return 1; }\nprintf(\"%d\\n\", f2());\n");
	writeOldSource("m2.pa", "import \"m1.pa\";\nexport func f2() -> int32 { return f1() + 1; }\n");
	ASSERT_EQ(execTestCommand("bin/palan out/m1.pa"), "2\n");
	auto m1 = objTime("m1.pa");
	auto m2 = objTime("m2.pa");
	ASSERT_EQ(execTestCommand("bin/palan out/m1.pa"), "2\n");
	EXPECT_EQ(objTime("m1.pa"), m1);
	EXPECT_EQ(objTime("m2.pa"), m2);
}

// A header counts wherever it is reached from: through another header, or a
// `cinclude` inside a function body.
TEST(build_mgr, reuse_header_changed) {
	cleanReuseEnv();
	writeOldSource("nested.h", "typedef int nested_t;\n");
	writeOldSource("top.h", "#include \"nested.h\"\n");
	writeOldSource("body.h", "typedef int body_t;\n");
	writeOldSource("m.pa", "cinclude \"top.h\";\nexport func f() -> int32 { cinclude \"body.h\"; return 1; }\n");
	writeOldSource("main.pa", "cinclude <stdio.h>;\nimport \"m.pa\";\nprintf(\"%d\\n\", f());\n");
	ASSERT_EQ(execTestCommand("bin/palan out/main.pa"), "1\n");
	for (const char* header : {"nested.h", "body.h"}) {
		auto m = objTime("m.pa");
		auto importer = objTime("main.pa");
		ofstream("out/" + string(header)) << "typedef int changed_t;\n";
		ASSERT_EQ(execTestCommand("bin/palan out/main.pa"), "1\n");
		EXPECT_NE(objTime("m.pa"), m) << header;
		EXPECT_NE(objTime("main.pa"), importer) << header;
	}

	ASSERT_TRUE(fs::remove("out/nested.h"));
	EXPECT_NE(execTestCommand("bin/palan out/main.pa").find("nested.h"), string::npos);
}

TEST(build_mgr, reuse_tools_changed) {
	writeImportChain();
	// Run with system(): copying the tools can outlast execTestCommand's timeout.
	ASSERT_EQ(system("cp -r lib/palan/dev out/dev"), 0);
	const string palan = "out/dev/tools/palan out/a.pa";
	ASSERT_EQ(execTestCommand(palan), "1\n");
	ChainTimes built;
	ASSERT_EQ(execTestCommand(palan), "1\n");
	EXPECT_EQ(objTime("c.pa"), built.c);

	auto touchedRebuilds = [&](const string& file) {
		auto before = objTime("c.pa");
		fs::last_write_time(file, fs::file_time_type::clock::now());
		EXPECT_EQ(execTestCommand(palan), "1\n") << file;
		return objTime("c.pa") != before;
	};
	EXPECT_TRUE(touchedRebuilds("out/dev/tools/palan-sa"));
	EXPECT_TRUE(touchedRebuilds("out/dev/" PALAN_TARGET "/c2ast/predefined.h"));
}

TEST(build_mgr, reuse_shared_by_entries) {
	cleanReuseEnv();
	writeOldSource("shared.pa", "cinclude <stdio.h>;\nexport func s() -> int32 { return 7; }\nprintf(\"test\\n\");\n");
	writeOldSource("e1.pa", "cinclude <stdio.h>;\nimport \"shared.pa\";\nprintf(\"%d\\n\", s());\n");
	writeOldSource("e2.pa", "cinclude <stdio.h>;\nimport \"shared.pa\";\nprintf(\"%d\\n\", s() + 1);\n");
	ASSERT_EQ(execTestCommand("bin/palan out/e1.pa"), "7\n");
	auto shared = objTime("shared.pa");
	ASSERT_EQ(execTestCommand("bin/palan out/e2.pa"), "8\n");
	ASSERT_EQ(execTestCommand("bin/palan out/shared.pa"), "test\n");
	EXPECT_EQ(objTime("shared.pa"), shared);
}

TEST(build_mgr, reuse_error_reported_every_run) {
	cleanReuseEnv();
	writeOldSource("bad.pa", "int32 x = undefined_name;\n");
	string first = execTestCommand("bin/palan out/bad.pa");
	EXPECT_NE(first.find("error"), string::npos) << first;
	EXPECT_EQ(execTestCommand("bin/palan out/bad.pa"), first);
}

TEST(build_mgr, reuse_rebuilds_missing_sa) {
	writeImportChain();
	ASSERT_EQ(execTestCommand("bin/palan out/a.pa"), "1\n");
	auto before = objTime("c.pa");
	ASSERT_TRUE(fs::remove(reusePath("c.pa") + ".debug.sa.json"));
	ASSERT_EQ(execTestCommand("bin/palan out/a.pa"), "1\n");
	EXPECT_NE(objTime("c.pa"), before);
}

TEST(build_mgr, reuse_allocators) {
	cleanReuseEnv();
	writeOldSource("alloc.pa", "cinclude <stdio.h>;\n[+]int32 v = [1, 2];\nprintf(\"%d\\n\", int32(len(@v)));\n");
	ASSERT_EQ(execTestCommand("bin/palan out/alloc.pa"), "2\n");
	string allocators = reusePath("alloc.pa") + ".debug.d/" PALAN_ALLOCATORS_MODULE ".pa.debug.o";
	auto before = fs::last_write_time(allocators);

	ofstream("out/alloc.pa") << "cinclude <stdio.h>;\n[+]int32 v = [1, 2, 3];\nprintf(\"%d\\n\", int32(len(@v)));\n";
	ASSERT_EQ(execTestCommand("bin/palan out/alloc.pa"), "3\n");
	EXPECT_EQ(fs::last_write_time(allocators), before);

	ofstream("out/alloc.pa") << "cinclude <stdio.h>;\n[+]int64 v = [1, 2];\nprintf(\"%d\\n\", int32(len(@v)));\n";
	ASSERT_EQ(execTestCommand("bin/palan out/alloc.pa"), "2\n");
	EXPECT_NE(fs::last_write_time(allocators), before);
}

static string fileText(const string& path) {
	ifstream f(path);
	stringstream ss;
	ss << f.rdbuf();
	return ss.str();
}

TEST(build_mgr, modes_kept_apart) {
	cleanReuseEnv();
	writeOldSource("mode.pa", "cinclude <stdio.h>;\n[+]int32 v = [1, 2];\nv.resize(3);\n"
		"printf(\"%d\\n\", v.pop());\n");
	ASSERT_EQ(execTestCommand("bin/palan out/mode.pa"), "0\n");
	auto debugObj = objTime("mode.pa");
	auto astTime = fs::last_write_time(reusePath("mode.pa") + ".ast.json");
	ASSERT_EQ(execTestCommand("bin/palan -o out/mode_bin out/mode.pa"), "");
	ASSERT_EQ(execTestCommand("out/mode_bin"), "0\n");
	EXPECT_TRUE(fs::exists(reusePath("mode.pa") + ".release.o"));
	EXPECT_EQ(objTime("mode.pa"), debugObj);
	EXPECT_EQ(fs::last_write_time(reusePath("mode.pa") + ".ast.json"), astTime);
}

TEST(build_mgr, mode_decides_checks) {
	cleanReuseEnv();
	writeOldSource("mode.pa", "cinclude <stdio.h>;\n[+]int32 v = [1, 2];\nv.resize(3);\n"
		"printf(\"%d\\n\", v.pop());\n");
	auto hasChecks = [](const string& mode) {
		string text = fileText(reusePath("mode.pa") + "." + mode + ".d/" PALAN_ALLOCATORS_MODULE ".pa");
		bool pop = text.find("if h.len == 0 { __pln_pop_empty(); }") != string::npos;
		bool resize = text.find("if n < 0 { __pln_negative_size(); }") != string::npos;
		EXPECT_EQ(pop, resize) << text;
		return pop;
	};
	ASSERT_EQ(execTestCommand("bin/palan out/mode.pa"), "0\n");
	EXPECT_TRUE(hasChecks("debug"));
	ASSERT_EQ(execTestCommand("bin/palan -o out/mode_bin out/mode.pa"), "");
	EXPECT_FALSE(hasChecks("release"));

	cleanReuseEnv();
	writeOldSource("mode.pa", "cinclude <stdio.h>;\n[+]int32 v = [1, 2];\nv.resize(3);\n"
		"printf(\"%d\\n\", v.pop());\n");
	ASSERT_EQ(execTestCommand("bin/palan --release out/mode.pa"), "0\n");
	EXPECT_FALSE(hasChecks("release"));
	ASSERT_EQ(execTestCommand("bin/palan --debug -o out/mode_bin out/mode.pa"), "");
	EXPECT_TRUE(hasChecks("debug"));
}

static string libBuild(const string& file) {
	return "bin/palan --lib -o out/lib ../test/testdata/build-mgr/lib_build/" + file;
}

static const char libOut[] = "out/lib/";

TEST(build_mgr, lib_interface_merges_what_the_main_publishes) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(libBuild("demo.pa")), "");
	EXPECT_EQ(fileText(string(libOut) + "demo/demo.pai"),
		"library demo;\n"
		"built \"" PALAN_VERSION "\";\ntarget \"" PALAN_TARGET "\";\n"
		"link \"m\";\n"
		"cinclude <stdio.h> export FILE;\n"
		"type Dup {\n\tint16 keep;\n};\n"
		"type Used {\n\tint32 q;\n\t[2]int32 owned;\n};\n"
		"const C = 2;\n"
		"type OnlyInPrivate {\n\tint32 i;\n};\n"
		"type Priv {\n\tint32 x;\n\t@OnlyInPrivate i;\n};\n"
		"export type Color enum {\n\tRed,\n\tBlue,\n};\n"
		"export type T {\n\t[C]int32 a;\n\tBytes b;\n\t@Priv p;\n\t@FILE f;\n\t@Dup d;\n};\n"
		"export type W {\n\t@T t;\n\t[C]int32 c;\n\t@Dup d;\n};\n"
		"const __m1_C = 4;\n"
		"export type Bytes = [__m1_C]int8;\n"
		"export func value(@Used u, Color c) -> int32;\n"
		"export func many() -> [+]T;\n");
}

TEST(build_mgr, lib_archive_links_without_the_source) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(libBuild("demo.pa")), "");
	string archive = string(libOut) + "demo/demo.a";
	EXPECT_EQ(execTestCommand("ar t " + archive + " | sort"),
		"demo.demo.__allocators.o\ndemo.demo.o\ndemo.shapes.__allocators.o\ndemo.shapes.o\n");
	// A program's first use of the archive: only the stub is not from it.
	ASSERT_EQ(execTestCommand("lib/palan/dev/tools/palan-codegen --entry demo.demo..init --entry demo.demo..main --target " PALAN_TARGET " -o out/entry.s"), "");
	ASSERT_EQ(execTestCommand("as out/entry.s -o out/entry.o"), "");
	ASSERT_EQ(execTestCommand("ld out/entry.o " + archive + " lib/palan/dev/" PALAN_TARGET "/std/rt.a -lc -lm -dynamic-linker /lib64/ld-linux-x86-64.so.2 -o out/demo"), "");
	string output = execTestCommand(
		"env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so MALLOC_TRACE=out/trace.log out/demo");
	ASSERT_EQ(output, "len 2 1 q 7\n");
	expectBalancedMtrace("out/trace.log", 15);
}

// Same object file names in one archive: every module's allocators, and a file name of two directories.
TEST(build_mgr, lib_members_of_one_file_name) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(libBuild("shared_name_a.pa")), "");
	EXPECT_EQ(execTestCommand("ar t " + string(libOut) + "demo/a/shared_name_a.a | sort"),
		"demo.a.shared_name_a.o\ndemo.same.__allocators.o\ndemo.same.o\ndemo.sub.same.__allocators.o\ndemo.sub.same.o\n");
}

TEST(build_mgr, lib_leaving_out_a_private_type_keeps_the_symbols) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(libBuild("private_order.pa")), "");
	EXPECT_EQ(fileText(string(libOut) + "demo/private_order.pai"),
		"library demo;\nbuilt \"" PALAN_VERSION "\";\ntarget \"" PALAN_TARGET "\";\ntype B {\n\tint32 b;\n};\nexport func get(@B b) -> int32;\n");
}

TEST(build_mgr, lib_local_header_used_only_in_bodies) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(libBuild("body_header.pa")), "");
	EXPECT_EQ(fileText(string(libOut) + "demo/body_header.pai"),
		"library demo;\nbuilt \"" PALAN_VERSION "\";\ntarget \"" PALAN_TARGET "\";\nexport func one() -> int32;\n");
}

// A named return's initial value belongs to the body, not the interface.
TEST(build_mgr, lib_named_return_initial_value) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(libBuild("named_ret_init.pa")), "");
	EXPECT_EQ(fileText(string(libOut) + "demo/named_ret_init.pai"),
		"library demo;\nbuilt \"" PALAN_VERSION "\";\ntarget \"" PALAN_TARGET "\";\nexport func count() -> int32 n;\n");
}

// The other module's array stays for the const its public alias is sized by; the unused one does not.
TEST(build_mgr, lib_const_from_an_array_through_a_selective_import) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(libBuild("sized.pa")), "");
	EXPECT_EQ(fileText(string(libOut) + "demo/sized.pai"),
		"library demo;\nbuilt \"" PALAN_VERSION "\";\ntarget \"" PALAN_TARGET "\";\n"
		"export type Rows {\n\tRow a;\n};\n"
		"[3]int32 table;\nconst K = len(@table);\n"
		"export type Row = [K]int32;\n"
		"export func first(@Rows r) -> int32;\n");
}

// Another module's const of the same name gives way, as it does for a type's size.
TEST(build_mgr, lib_parameter_defaults) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand(libBuild("defaults.pa")), "");
	EXPECT_EQ(fileText(string(libOut) + "demo/defaults.pai"),
		"library demo;\nbuilt \"" PALAN_VERSION "\";\ntarget \"" PALAN_TARGET "\";\n"
		"const K = 2;\n"
		"export type Mode enum {\n\tFast,\n\tSlow,\n};\n"
		"export type Box {\n\tArr a;\n};\n"
		"const __m1_K = 3;\n"
		"export type Arr = [__m1_K]int32;\n"
		"export func put(@!Box b, int32 v = K * 10, Mode m = Mode.Slow, @[]uint8 tag = \"none\");\n");
}

TEST(build_mgr, lib_library_module_has_its_own_allocators) {
	expectMtraceRun("398_library_allocators", "1 3\n", 13);
	string dir = fs::weakly_canonical("../test/testdata/build-mgr").string();
	EXPECT_TRUE(fs::exists(targetWorkDir() + dir + "/lib_build/shapes.pa.release.alloc/" PALAN_ALLOCATORS_MODULE ".pa.release.o"));
	EXPECT_TRUE(fs::exists(targetWorkDir() + dir + "/398_library_allocators.pa.release.d/" PALAN_ALLOCATORS_MODULE ".pa.release.o"));
}

TEST(build_mgr, lib_errors) {
	string lib_build = fs::weakly_canonical("../test/testdata/build-mgr/lib_build").string() + "/";
	cleanTestEnv();
	EXPECT_EQ(execTestCommand("bin/palan --lib ../test/testdata/build-mgr/lib_build/demo.pa"),
		"return1::palan: --lib needs an output directory (-o)\n");
	EXPECT_EQ(execTestCommand(libBuild("err_no_decl.pa")),
		"return1::palan: '" + lib_build + "err_no_decl.pa' has no library declaration, which every module of a library needs\n");
	EXPECT_EQ(execTestCommand(libBuild("err_inner_no_decl.pa")),
		"return1::palan: '" + lib_build + "plain.pa' has no library declaration, which every module of a library needs\n");
	EXPECT_EQ(execTestCommand(libBuild("err_package.pa")),
		"return1::palan: '" + lib_build + "other_package.pa' belongs to library 'other', not 'demo'\n");
	EXPECT_EQ(execTestCommand(libBuild("err_local_header.pa")),
		"return1::" + lib_build + "err_local_header.pa:2:1: error: a library's public declaration cannot use a C type of the local header 'local_type.h': its reader would not find the header; wrap the type in an opaque type to publish it.\n");
	EXPECT_NE(execTestCommand(libBuild("err_no_export_clause.pa")).find("FILE"), string::npos);
	string internal = "': declare it in the main source.\n";
	EXPECT_EQ(execTestCommand(libBuild("err_internal_type.pa")),
		"return1::" + lib_build + "err_internal_type.pa:3:1: error: a library's public declaration cannot use the type 'K.Kept' of the internal module '" + lib_build + "kept.pa" + internal);
	EXPECT_EQ(execTestCommand(libBuild("err_internal_field.pa")),
		"return1::" + lib_build + "err_internal_field.pa:3:1: error: a library's public declaration cannot use the type 'K.Kept' of the internal module '" + lib_build + "kept.pa" + internal);
	EXPECT_EQ(execTestCommand(libBuild("err_private_alias.pa")),
		"return1::" + lib_build + "private_alias.pa:3:1: error: a library's public declaration cannot use the type 'Hidden' of the internal module '" + lib_build + "private_alias.pa" + internal);
	EXPECT_FALSE(fs::exists("out/lib"));
}
