/// palan build manager tests: installation and uninstallation
///
/// @file installTests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <set>
#include "../test-base/testBase.h"
#include "../../src/common/PlnDefs.h"

using namespace std;
namespace fs = std::filesystem;

static const string VERSION = PALAN_VERSION;
static const string HELLO = "../test/testdata/build-mgr/001_helloworld.pa";

static string linkTo(const string& version)
{
	return "../lib/palan/" + version + "/tools/palan";
}

// Run with system(): copying the tools can outlast execTestCommand's timeout.
static int install(const string& prefix, const string& destdir = "")
{
	string env = destdir.empty() ? "" : "DESTDIR=" + destdir + " ";
	return system((env + "cmake --install . --prefix " + prefix + " > out/install.log 2>&1").c_str());
}

static string outPath(const string& name)
{
	return (fs::current_path() / "out" / name).string();
}

static string usesStd()
{
	string path = outPath("uses_std.pa");
	ofstream(path) << "import <std/utf8>;\ncinclude <stdio.h>;\nutf8 s = \"\\xE2\\x82\\xAC\";\n"
		"printf(\"%ld\\n\", s -> byteLen());\n";
	return path;
}

static set<string> filesUnder(const fs::path& root)
{
	set<string> files;
	for (auto& e : fs::recursive_directory_iterator(root))
		if (!fs::is_directory(e.symlink_status()))
			files.insert(e.path().lexically_relative(root).string());
	return files;
}

static set<string> expectedFiles()
{
	string lib = "lib/palan/" + VERSION + "/";
	string doc = "share/doc/palan/" + VERSION + "/";
	return {
		"bin/palan",
		lib + "tools/palan",
		lib + "tools/palan-gen-ast",
		lib + "tools/palan-c2ast",
		lib + "tools/palan-sa",
		lib + "tools/palan-codegen",
		lib + "x86_64-linux-gnu/c2ast/predefined.h",
		lib + "x86_64-linux-gnu/std/utf8.pai",
		lib + "x86_64-linux-gnu/std/utf8.a",
		lib + "x86_64-linux-gnu/std/rt.pai",
		lib + "x86_64-linux-gnu/std/rt.a",
		lib + "src/std/utf8.pa",
		lib + "src/std/rt.pa",
		lib + "uninstall",
		doc + "README.md",
		doc + "LICENSE",
		doc + "doc/ASTSpec.md",
		doc + "doc/PalanReference.md",
		doc + "doc/PalanStdLibReference.md",
		doc + "doc/SASpec.md",
		doc + "doc/SpecAndDesign.md",
	};
}

// A copy stands in for another installed version: the uninstall script finds its
// version from where it is, and a test build has only one version to install.
static void addVersion(const string& root, const string& version)
{
	fs::copy(root + "/lib/palan/" + VERSION, root + "/lib/palan/" + version,
		fs::copy_options::recursive);
	fs::copy(root + "/share/doc/palan/" + VERSION, root + "/share/doc/palan/" + version,
		fs::copy_options::recursive);
}

static void pointLinkAt(const string& root, const string& version)
{
	fs::remove(root + "/bin/palan");
	fs::create_symlink(linkTo(version), root + "/bin/palan");
}

static string uninstall(const string& root, const string& version)
{
	return execTestCommand(root + "/lib/palan/" + version + "/uninstall");
}

TEST(install, layout_and_run) {
	cleanTestEnv();
	string root = outPath("root");
	ASSERT_EQ(install(root), 0);

	ASSERT_EQ(filesUnder(root), expectedFiles());
	ASSERT_EQ(fs::read_symlink(root + "/bin/palan").string(), linkTo(VERSION));

	ASSERT_EQ(execTestCommand(root + "/bin/palan --version"), "palan " + VERSION + "\n");
	ASSERT_EQ(execTestCommand(root + "/bin/palan " + HELLO), "Hello World!\n");
}

TEST(install, root_can_move) {
	cleanTestEnv();
	string root = outPath("root");
	string moved = outPath("moved");
	ASSERT_EQ(install(root), 0);
	fs::rename(root, moved);

	ASSERT_EQ(execTestCommand(moved + "/bin/palan " + HELLO), "Hello World!\n");
	ASSERT_EQ(execTestCommand(moved + "/bin/palan " + usesStd()), "3\n");
}

// `std` comes from the tools' own version, and a libpath holding one cannot stand in for it.
TEST(install, std_library_is_found_with_the_tools) {
	cleanTestEnv();
	string root = outPath("root");
	ASSERT_EQ(install(root), 0);
	EXPECT_EQ(execTestCommand(root + "/bin/palan " + usesStd()), "3\n");

	fs::create_directories("out/src");
	ofstream("out/src/two.pa") << "library std;\nexport func two() -> int32 { return 2; }\n";
	ASSERT_EQ(execTestCommand(root + "/bin/palan --lib -o out/decoy out/src/two.pa"), "");

	string decoyed = outPath("decoyed.pa");
	ofstream(decoyed) << "libpath \"decoy\";\nimport <std/two>;\n";
	EXPECT_EQ(execTestCommand(root + "/bin/palan " + decoyed),
		"return1::palan: '" + decoyed + "' imports <std/two>, which no libpath has\n");
}

TEST(install, destdir) {
	cleanTestEnv();
	string stage = outPath("stage");
	ASSERT_EQ(install("/usr", stage), 0);

	ASSERT_EQ(filesUnder(stage + "/usr"), expectedFiles());
	ASSERT_EQ(fs::read_symlink(stage + "/usr/bin/palan").string(), linkTo(VERSION));
	ASSERT_EQ(execTestCommand(stage + "/usr/bin/palan " + HELLO), "Hello World!\n");
}

TEST(install, uninstall_versions) {
	cleanTestEnv();
	string root = outPath("root");
	ASSERT_EQ(install(root), 0);
	// 99.9.10 sorts before 99.9.9 as text, so these tell version order from it.
	addVersion(root, "99.9.9");
	addVersion(root, "99.9.10");
	addVersion(root, "99.9.11");
	pointLinkAt(root, "99.9.11");

	ASSERT_EQ(uninstall(root, "99.9.11"), "");
	ASSERT_FALSE(fs::exists(root + "/lib/palan/99.9.11"));
	ASSERT_FALSE(fs::exists(root + "/share/doc/palan/99.9.11"));
	ASSERT_EQ(fs::read_symlink(root + "/bin/palan").string(), linkTo("99.9.10"));
	ASSERT_EQ(execTestCommand(root + "/bin/palan " + HELLO), "Hello World!\n");

	ASSERT_EQ(uninstall(root, "99.9.9"), "");
	ASSERT_FALSE(fs::exists(root + "/lib/palan/99.9.9"));
	ASSERT_EQ(fs::read_symlink(root + "/bin/palan").string(), linkTo("99.9.10"));

	ASSERT_EQ(uninstall(root, "99.9.10"), "");
	ASSERT_EQ(fs::read_symlink(root + "/bin/palan").string(), linkTo(VERSION));

	ASSERT_EQ(uninstall(root, VERSION), "");
	ASSERT_TRUE(fs::is_empty(root));
}

TEST(install, uninstall_outside_layout) {
	cleanTestEnv();
	string root = outPath("root");
	ASSERT_EQ(install(root), 0);
	string dir = outPath("elsewhere/a/b");
	fs::create_directories(dir);
	fs::copy(root + "/lib/palan/" + VERSION + "/uninstall", dir + "/uninstall");

	string output = execTestCommand(dir + "/uninstall");
	ASSERT_NE(output.find("is not in"), string::npos);
	ASSERT_TRUE(fs::exists(dir + "/uninstall"));
}
