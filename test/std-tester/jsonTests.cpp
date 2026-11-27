/// std/json tests
///
/// @file jsonTests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <vector>
#include "../test-base/testBase.h"

using namespace std;

static void expectBalanced(const string& log_path) {
	ifstream log(log_path);
	int allocs = 0, frees = 0;
	for (string line; getline(log, line);) {
		if (line.find(" + ") != string::npos) allocs++;
		else if (line.find(" - ") != string::npos) frees++;
	}
	EXPECT_GT(allocs, 0);
	EXPECT_EQ(allocs, frees);
}

// The tests in json.pa cover parsing; run under mtrace, they also show that neither a parsed
// value nor a failed parse leaks.
TEST(std_json, top_level_tests) {
	cleanTestEnv();
	ASSERT_EQ(execTestCommand("bin/palan -o out/json_tests ../src/std/json.pa"), "");
	ASSERT_EQ(execTestCommand("env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=out/json_tests.log out/json_tests"), "");
	expectBalanced("out/json_tests.log");
}

TEST(std_json, json_test_suite) {
	cleanTestEnv();
	const string dir = "../test/testdata/std/JSONTestSuite/test_parsing";
	vector<string> paths;
	for (auto& e : filesystem::directory_iterator(dir)) paths.push_back(e.path().string());
	sort(paths.begin(), paths.end());
	string files;
	for (auto& p : paths) files += " " + p;
	ASSERT_EQ(execTestCommand("bin/palan -o out/json_suite ../test/testdata/std/json_suite.pa"), "");
	ASSERT_EQ(execTestCommand("env LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libc_malloc_debug.so "
		"MALLOC_TRACE=out/json_suite.log out/json_suite" + files), "318 cases, 0 failed\n");
	expectBalanced("out/json_suite.log");
}

TEST(std_json, imported_from_the_build_tree) {
	cleanTestEnv();
	ofstream("out/uses.pa") << "import <std/json>;\nimport <std/utf8>;\ncinclude <stdio.h>;\n"
		"jsonresult r = parseJson(\"{\\\"a\\\": [1, 2.5, \\\"x\\\"]}\");\n"
		"if r is @v = value && v is @m = members && m[\"a\"] is @xs = items {\n"
		"\tif xs[0] is copy i && xs[1] is copy f && xs[2] is @s { printf(\"%ld %g %s\\n\", i, f, s -> cstr()); }\n"
		"}\n"
		"jsonresult e = parseJson(\"[1,\\n tru]\");\n"
		"if e is @err = error { printf(\"%s %ld %ld:%ld\\n\", err.message -> cstr(), err.pos, err.line, err.col); }\n"
		"json j = [true, {\"k\": 3}];\n"
		"if j is @t = items && t[1] is @d = members && d[\"k\"] is copy n = i { printf(\"%ld\\n\", n); }\n"
		"5 -> j[1][\"m\"];\n"
		"if j[1][\"m\"] is copy m = i { printf(\"%ld\\n\", m); }\n";
	ASSERT_EQ(execTestCommand("bin/palan out/uses.pa"),
		"1 2.5 x\nunexpected character 8 2:5\n3\n5\n");
}

TEST(std_json, runtime_errors) {
	struct { const char* code; const char* error; } cases[] = {
		{"flo64 z = 0.0;\njson j = json(z / z);\nutf8 t = toUtf8(@j);\n", "json number is NaN or infinite"},
		{"flo64 f = 1e308;\njson j = json(f * 10.0);\nutf8 t = toUtf8(@j);\n", "json number is NaN or infinite"},
		{"json j = \"1\";\nflo64 f = num(@j);\n", "num of a json that is not a number"},
		{"json j = [1];\njson k = j[\"a\"];\n", "json indexed by a key is not members"},
		{"json j = {};\njson k = j[\"a\"];\n", "key \"a\" is not in the dictionary"},
		{"json j = {};\njson k = j[0];\n", "json indexed by a position is not items"},
		{"json j = [1];\njson k = j[1];\n", "json position out of range"},
		{"json j = [1];\njson k = j[-1];\n", "json position out of range"},
		{"json j = [1];\n2 -> j[1];\n", "json position out of range"},
		{"json j = 1;\n2 -> j[\"a\"];\n", "json indexed by a key is not members"},
		{"json j;\n2 -> j[0];\n", "json indexed by a position is not items"},
	};
	for (auto& c : cases) {
		cleanTestEnv();
		ofstream("out/fails.pa") << "import <std/json>;\nimport <std/utf8>;\n" << c.code;
		ASSERT_EQ(execTestCommand("bin/palan -o out/fails_bin out/fails.pa"), "") << c.code;
		// ulimit -c 0 keeps the program's core dump out of build/.
		string out = execTestCommand("ulimit -c 0; out/fails_bin 2>out/stderr; echo status $?; cat out/stderr");
		string expected = string("status 134\nruntime error: ") + c.error + "\n";
		EXPECT_EQ(out.substr(0, expected.size()), expected) << c.code;
	}
}
