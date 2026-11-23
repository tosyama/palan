/// std/utf8 tests
///
/// @file utf8Tests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include <fstream>
#include "../test-base/testBase.h"

using namespace std;

TEST(std_utf8, top_level_tests) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand("bin/palan ../src/std/utf8.pa"), "");
}

TEST(std_graphemeprops, top_level_tests) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand("bin/palan ../src/std/graphemeprops.pa"), "");
}

// Every case of Unicode's own test data, of the version the property table is made from.
TEST(std_utf8, grapheme_break_conformance) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand("bin/palan ../test/testdata/std/grapheme_break.pa"
		" ../test/testdata/std/GraphemeBreakTest.txt"), "853 cases, 0 failed\n");
}

TEST(std_utf8, imported_from_the_build_tree) {
	cleanTestEnv();
	ofstream("out/uses.pa") << "import <std/utf8>;\ncinclude <stdio.h>;\n"
		"utf8 s = utf8(\"h\\xC3\\xA9\", utf8mode.KEEP_INVALID);\ns << \"!\\xFF\";\n"
		"utf8 t;\ns -> t;\nint64 n = 0;\nfor copy c in t { n + 1 -> n; }\n"
		"printf(\"%s %ld %ld %d %ld %d\\n\", s -> cstr(), s -> byteLen(), s -> charCount(), s.hadInvalid,\n"
		"\tn, t == \"h\\u{E9}!\\u{FFFD}\");\n"
		"for copy g, i in utf8(\"e\\u{301}x\") -> graphemes() { printf(\"%ld:%s \", i, g -> cstr()); }\n"
		"printf(\"%ld\\n\", t -> graphemeCount());\n";
	ASSERT_EQ(execTestCommand("bin/palan out/uses.pa"), "h\xC3\xA9!\xFF 5 4 1 4 1\n0:e\xCC\x81 1:x 4\n");
}
