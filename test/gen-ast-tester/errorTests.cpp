/// gen-ast error case tests
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

TEST(gen_ast_error, help) {
	cleanTestEnv();
	string out = execTestCommand("palan-gen-ast --help");
	ASSERT_NE(out.find("Usage: palan-gen-ast"), string::npos);
}

TEST(gen_ast_error, version) {
	cleanTestEnv();
	string out = execTestCommand("palan-gen-ast --version");
	ASSERT_NE(out.find("palan-gen-ast"), string::npos);
}

TEST(gen_ast_error, library_decl_misplaced) {
	for (string source : {"library a;\nlibrary b;\n", "int32 x;\nlibrary a;\n", "{ library a; }\n",
	                      "library a..b;\n", "library a.;\n", "library;\n", "library a\n"}) {
		cleanTestEnv();
		ofstream("out/m.pa") << source;
		string out = execTestCommand("palan-gen-ast out/m.pa");
		EXPECT_NE(out.find("error"), string::npos) << source << out;
	}
}

TEST(gen_ast_error, unsupported_target) {
	cleanTestEnv();
	string out = execTestCommand("palan-gen-ast --target aarch64-linux-gnu ../test/testdata/gen-ast/001_basicPattern.pa");
	ASSERT_NE(out.find("palan-gen-ast: unsupported target 'aarch64-linux-gnu'"), string::npos);
}

TEST(gen_ast_error, no_input_file) {
	cleanTestEnv();
	string out = execTestCommand("palan-gen-ast");
	ASSERT_NE(out.find("no input file"), string::npos);
}

TEST(gen_ast_error, could_not_open_file) {
	cleanTestEnv();
	string out = execTestCommand("palan-gen-ast no_such_file.pa");
	ASSERT_NE(out.find("Could not open file"), string::npos);
}

TEST(gen_ast_error, interface_only_forms_in_source) {
	struct { string source, expected; } cases[] = {
		{"int32 x = 1;\nexport func f() -> int32;\n",
		 "m.pa:2:1: error: a function without a body is accepted only in an interface file (.pai)."},
		{"type T { int32 a; };\nfunc ->(@T src, @!T dst);\n",
		 "m.pa:2:1: error: a function without a body is accepted only in an interface file (.pai)."},
		{"export type H = opaque;\n",
		 "m.pa:1:1: error: an opaque type without its contents is accepted only in an interface file (.pai)."},
		{"link \"m\";\n", "m.pa:1:1: error: 'link' is accepted only in an interface file (.pai)."},
		{"built \"0\";\n", "m.pa:1:1: error: 'built' is accepted only in an interface file (.pai)."},
		{"foo \"m\";\n", "m.pa:1:1: error: syntax error, unexpected string after 'foo'."},
	};
	for (auto& c : cases) {
		cleanTestEnv();
		ofstream("out/m.pa") << c.source;
		string out = execTestCommand("palan-gen-ast out/m.pa");
		ASSERT_NE(out.find(c.expected), string::npos) << out;
	}
}

TEST(gen_ast_error, variadic_not_last) {
	const string message = "error: a variadic parameter ('[...]T') must be the last parameter.";
	struct { string source, location; } cases[] = {
		{"func f([...]int32 xs, int32 a) { }\n", "m.pa:1:8: "},
		{"func f(int32 a, [...]int32 xs, int32 b) { }\n", "m.pa:1:17: "},
		{"@func([...]int32, int32) fp;\n", "m.pa:1:7: "},
		{"@func(int32, [...]int32, int32) fp;\n", "m.pa:1:14: "},
	};
	for (auto& c : cases) {
		cleanTestEnv();
		ofstream("out/m.pa") << c.source;
		string out = execTestCommand("palan-gen-ast out/m.pa");
		ASSERT_NE(out.find(c.location + message), string::npos) << c.source << out;
	}
	// Only a parameter is variadic.
	for (string source : {"[...]int32 x;\n", "func f() -> [...]int32 { }\n"}) {
		cleanTestEnv();
		ofstream("out/m.pa") << source;
		string out = execTestCommand("palan-gen-ast out/m.pa");
		ASSERT_NE(out.find("syntax error"), string::npos) << source << out;
	}
}

// Forms with no plan in the language are not parsed at all.
TEST(gen_ast_error, unplanned_forms) {
	struct { string source, expected; } cases[] = {
		{"interface I { func f(); };\n", "m.pa:1:13: error: syntax error"},
		{"type T : I as A { int32 x; };\n", "m.pa:1:8: error: syntax error"},
		{"type T { int32 x; func A.f() { } };\n", "m.pa:1:19: error: syntax error"},
		{"construct T(int32 x) { }\n", "m.pa:1:12: error: syntax error"},
		{"f = func (int32 x) { };\n", "m.pa:1:5: error: syntax error"},
		{"x++;\n", "m.pa:1:3: error: syntax error"},
		{"type X;\n", "m.pa:1:7: error: syntax error"},
		{"int32 >>b;\n", "m.pa:1:9: error: syntax error"},
		{"func f(int32 >>b) { }\n", "m.pa:1:14: error: syntax error"},
		{"(int32 a, b) = (3, 5);\n", "m.pa:1:18: error: syntax error"},
		{"(3, 5) -> (a, b);\n", "m.pa:1:3: error: syntax error"},
		{"f() -> (a, -);\n", "m.pa:1:13: error: syntax error"},
		{"f() ->> (a, b);\n", "m.pa:1:11: error: syntax error"},
		{"[#]int32 a = [1];\n", "m.pa:1:2: error: Unexpected character '#'."},
		{"[#3]int32 a;\n", "m.pa:1:2: error: Unexpected character '#'."},
		{"k(@int32(1));\n", "m.pa:1:4: error: 'int32(...)' is a conversion, which cannot follow '@', '@!', '->' or '->>'."},
		{"k(@int32(1).f());\n", "m.pa:1:4: error: 'int32(...)' is a conversion, which cannot follow '@', '@!', '->' or '->>'."},
		{"x -> int32();\n", "m.pa:1:6: error: 'int32(...)' is a conversion, which cannot follow '@', '@!', '->' or '->>'."},
	};
	for (auto& c : cases) {
		cleanTestEnv();
		ofstream("out/m.pa") << c.source;
		string out = execTestCommand("palan-gen-ast out/m.pa");
		ASSERT_NE(out.find(c.expected), string::npos) << c.source << out;
	}

	// The words those forms used are names again.
	cleanTestEnv();
	ofstream("out/m.pa") << "int32 interface = 1, construct = 2;\n";
	string out = execTestCommand("palan-gen-ast out/m.pa");
	ASSERT_EQ(out.find("error"), string::npos) << out;
}

// Several values come only from a call, into places that store them, and go no further.
TEST(gen_ast_error, receive_misuse) {
	struct { string source, expected; } cases[] = {
		{"(int32 a, b) = x;\n", "m.pa:1:16: error: several values are received only from a call."},
		{"x -> (a, b);\n", "m.pa:1:1: error: several values are received only from a call."},
		{"f() -> (a, b + 1);\n", "m.pa:1:12: error: a received value is stored only into a variable, an element or a field."},
		{"f() -> (a, b) -> g();\n",
		 "m.pa:1:15: error: values received into several targets cannot be passed on with '->' or '->>'."},
		{"f() -> (a, b) -> c;\n",
		 "m.pa:1:15: error: values received into several targets cannot be passed on with '->' or '->>'."},
		{"f() -> (a, b) ->> c;\n",
		 "m.pa:1:15: error: values received into several targets cannot be passed on with '->' or '->>'."},
		{"f() -> (a, b) ->> c as [+]int32;\n",
		 "m.pa:1:15: error: values received into several targets cannot be passed on with '->' or '->>'."},
		{"f() -> (a, b) -> (c, d);\n",
		 "m.pa:1:15: error: values received into several targets cannot be passed on with '->' or '->>'."},
	};
	for (auto& c : cases) {
		cleanTestEnv();
		ofstream("out/m.pa") << c.source;
		string out = execTestCommand("palan-gen-ast out/m.pa");
		ASSERT_NE(out.find(c.expected), string::npos) << c.source << out;
	}
}

// '$' is a property of a local declaration: a parameter or return value is never in the caller's frame.
TEST(gen_ast_error, frame_param_or_ret) {
	const string message = "error: '$' places a local variable in the stack frame; a parameter or return value cannot have it.";
	struct { string source, location; } cases[] = {
		{"func f($int32 x) { }\n", "m.pa:1:1: "},
		{"type P { int32 x; };\nfunc f(int32 a, $P p) { }\n", "m.pa:2:1: "},
		{"type P { int32 x; };\nfunc f($P a, b) { }\n", "m.pa:2:1: "},
		{"func f($[4]int32 a) { }\n", "m.pa:1:1: "},
		{"type P { int32 x; };\nfunc f() -> $P r { }\n", "m.pa:2:1: "},
		{"type P { int32 x; };\nfunc f() -> $P { }\n", "m.pa:2:13: "},
		{"func outer() {\n\tfunc inner($int32 x) { }\n}\n", "m.pa:2:2: "},
		{"syscall s($int32 a) -> int64 = 1;\n", "m.pa:1:1: "},
		{"syscall s(int64 a) -> $int64 = 1;\n", "m.pa:1:23: "},
	};
	for (auto& c : cases) {
		cleanTestEnv();
		ofstream("out/m.pa") << c.source;
		string out = execTestCommand("palan-gen-ast out/m.pa");
		ASSERT_NE(out.find(c.location + message), string::npos) << c.source << out;
	}
}

TEST(gen_ast_error, dict_key_type) {
	const string message = "error: a dictionary type is written '[\"\"]T': its key is a C string, which has no type name to write.";
	struct { string source, location; } cases[] = {
		{"[\"a\"]int32 d;\n", "m.pa:1:2: "},
		{"type P { [\"key\"]int32 m; };\n", "m.pa:1:11: "},
	};
	for (auto& c : cases) {
		cleanTestEnv();
		ofstream("out/m.pa") << c.source;
		string out = execTestCommand("palan-gen-ast out/m.pa");
		ASSERT_NE(out.find(c.location + message), string::npos) << c.source << out;
	}
}

TEST(gen_ast_error, interface_version) {
	cleanTestEnv();
	ofstream("out/old.pai") << "built \"0.0.0\";\ntarget \"" PALAN_TARGET "\";\nexport func f();\n";
	string out = execTestCommand("palan-gen-ast out/old.pai");
	ASSERT_NE(out.find("old.pai:1:1: error: this interface was built by version 0.0.0"), string::npos) << out;

	ofstream("out/none.pai") << "export func f();\n";
	out = execTestCommand("palan-gen-ast out/none.pai");
	ASSERT_NE(out.find("none.pai:1:1: error: an interface file needs a 'built' statement."), string::npos) << out;
}

TEST(gen_ast_error, interface_target) {
	cleanTestEnv();
	ofstream("out/other.pai") << "built \"" PALAN_VERSION "\";\ntarget \"aarch64-linux-gnu\";\nexport func f();\n";
	string out = execTestCommand("palan-gen-ast out/other.pai");
	ASSERT_NE(out.find("other.pai:2:1: error: this interface is for the target aarch64-linux-gnu"), string::npos) << out;

	ofstream("out/none.pai") << "built \"" PALAN_VERSION "\";\nexport func f();\n";
	out = execTestCommand("palan-gen-ast out/none.pai");
	ASSERT_NE(out.find("none.pai:1:1: error: an interface file needs a 'target' statement."), string::npos) << out;

	ofstream("out/m.pa") << "target \"x\";\n";
	out = execTestCommand("palan-gen-ast out/m.pa");
	ASSERT_NE(out.find("m.pa:1:1: error: 'target' is accepted only in an interface file (.pai)."), string::npos) << out;
}

TEST(gen_ast_error, cinclude_export_not_top_level) {
	const string message = "error: an 'export' clause is accepted only on a top-level cinclude.";
	struct { string source, location; } cases[] = {
		{"{\n\tcinclude <stdio.h> export FILE;\n}\n", "m.pa:2:2: "},
		{"func f() {\n\tcinclude <stdio.h> export FILE;\n}\n", "m.pa:2:2: "},
		{"if 1 < 2 {\n\tcinclude <stdio.h> export FILE;\n}\n", "m.pa:2:2: "},
	};
	for (auto& c : cases) {
		cleanTestEnv();
		ofstream("out/m.pa") << c.source;
		string out = execTestCommand("palan-gen-ast out/m.pa");
		ASSERT_NE(out.find(c.location + message), string::npos) << c.source << out;
	}
}

TEST(gen_ast_error, export_import_not_top_level) {
	const string message = "error: 'export import' is accepted only at the top level.";
	struct { string source, location; } cases[] = {
		{"{\n\texport import \"x.pa\";\n}\n", "m.pa:2:2: "},
		{"func f() {\n\texport import <std/utf8>;\n}\n", "m.pa:2:2: "},
	};
	for (auto& c : cases) {
		cleanTestEnv();
		ofstream("out/m.pa") << c.source;
		string out = execTestCommand("palan-gen-ast out/m.pa");
		ASSERT_NE(out.find(c.location + message), string::npos) << c.source << out;
	}
	cleanTestEnv();
	ofstream("out/m.pa") << "export import \"x.pa\" as X;\n";
	string out = execTestCommand("palan-gen-ast out/m.pa");
	ASSERT_NE(out.find("m.pa:1:1: error: 'export import' cannot take an alias: the alias is a name of this file only."),
		string::npos) << out;
}

TEST(gen_ast_error, interface_cannot_state) {
	struct { string source, expected; } cases[] = {
		{"export func f([3][]int32 a) { }\n", "error: an interface cannot state this parameter or return."},
		{"export func f() -> [3][]int32 { }\n", "error: an interface cannot state this type."},
		{"const N = {x: 1};\n", "error: an interface cannot state this expression."},
	};
	for (auto& c : cases) {
		cleanTestEnv();
		ofstream("out/m.pa") << c.source;
		string out = execTestCommand("palan-gen-ast out/m.pa -o out/m.json --interface out/m.pai");
		ASSERT_NE(out.find(c.expected), string::npos) << c.source << out;
	}
}

TEST(gen_ast_error, library_import_form) {
	for (string path : {"stdio.h", "utf8", "../x", "/x/y", "x/", "x//y", "x/./y"}) {
		cleanTestEnv();
		ofstream("out/m.pa") << "\nimport <" << path << ">;\n";
		string out = execTestCommand("palan-gen-ast out/m.pa -o out/m.json");
		string expected = "m.pa:2:8: error: '<" + path + ">' does not name a library module";
		ASSERT_NE(out.find(expected), string::npos) << path << out;
	}
	cleanTestEnv();
	ofstream("out/m.pa") << "cinclude <stdio.h>;\nimport <a/b/c>;\n";
	ASSERT_EQ(execTestCommand("palan-gen-ast out/m.pa -o out/m.json"), "");
}

TEST(gen_ast_error, libpath_in_interface) {
	cleanTestEnv();
	ofstream("out/m.pai") << "built \"" PALAN_VERSION "\";\ntarget \"" PALAN_TARGET "\";\nlibpath \"x\";\n";
	string out = execTestCommand("palan-gen-ast out/m.pai -o out/m.json");
	ASSERT_NE(out.find("m.pai:3:1: error: an interface file cannot hold 'libpath'"), string::npos) << out;
}

TEST(gen_ast_error, interface_file_not_writable) {
	cleanTestEnv();
	ofstream("out/m.pa") << "export func f() { }\n";
	string out = execTestCommand("palan-gen-ast out/m.pa -o out/m.json --interface /nonexistent/dir/m.pai");
	ASSERT_NE(out.find("Could not open output file"), string::npos);
}

TEST(gen_ast_error, could_not_open_output_file) {
	cleanTestEnv();
	string out = execTestCommand(
		"palan-gen-ast ../test/testdata/gen-ast/001_basicPattern.pa -o /nonexistent/dir/out.json");
	ASSERT_NE(out.find("Could not open output file"), string::npos);
}

TEST(gen_ast_error, syntax_error_with_loc) {
	cleanTestEnv();
	string out = execTestCommand(
		"palan-gen-ast ../test/testdata/gen-ast/error_001_syntax_error.pa");
	ASSERT_NE(out.find(":1:"), string::npos);   // loc format
	ASSERT_NE(out.find("error:"), string::npos);
}

TEST(gen_ast_error, c2ast_failed) {
	// cinclude of a header palan-c2ast cannot read must be a fatal,
	// diagnosed error rather than a silent no-op.
	// execute_c2ast() throws on failure (same runtime_error + main.cpp
	// catch path as E_CouldNotOpenFile), so there is no source location
	// prefix -- just the message, same style as could_not_open_file above.
	cleanTestEnv();
	string out = execTestCommand(
		"palan-gen-ast ../test/testdata/gen-ast/error_002_c2ast_failed.pa");
	ASSERT_NE(out.find("Failed to read C header"), string::npos);
}

TEST(gen_ast_error, link_keyword_typo) {
	// The token after import_as, when present, must
	// spell "link" -- a misspelling is a diagnosed error (E_ExpectedLinkKeyword),
	// not silently mis-parsed as something else. Thrown from a parser action
	// (same runtime_error + main.cpp catch path as c2ast_failed above), so no
	// source location prefix.
	cleanTestEnv();
	string out = execTestCommand(
		"palan-gen-ast ../test/testdata/gen-ast/error_003_link_keyword_typo.pa");
	ASSERT_NE(out.find("Expected 'link' keyword"), string::npos);
}

TEST(gen_ast_error, cinclude_sys_path_injection) {
	// execute_c2ast used to build a shell command string for a <...> cinclude
	// path, letting shell metacharacters in the path escape the intended
	// palan-c2ast argv (e.g. injecting a `touch PWNED`); now the literal
	// string is passed as a single argv element, so it is just not a header
	// palan-c2ast can find.
	cleanTestEnv();
	execTestCommand("rm -f PWNED");
	string out = execTestCommand(
		"palan-gen-ast ../test/testdata/gen-ast/error_004_cinclude_sys_injection.pa");
	ASSERT_NE(out.find("Failed to read C header"), string::npos);
	ASSERT_FALSE(fs::exists("PWNED"));
}

TEST(gen_ast_error, syscall_as_identifier) {
	// Unlike "link" (a context-dependent keyword), "syscall" is a reserved
	// word everywhere, so using it as an identifier must be a diagnosed
	// syntax error. Goes through bison's error path, so it has the usual
	// ":line:" location prefix (fixture's offending line is line 3, after
	// two leading comment lines).
	cleanTestEnv();
	string out = execTestCommand(
		"palan-gen-ast ../test/testdata/gen-ast/error_006_syscall_as_identifier.pa");
	ASSERT_NE(out.find(":3:"), string::npos);
	ASSERT_NE(out.find("error:"), string::npos);
}

TEST(gen_ast_error, true_as_identifier) {
	cleanTestEnv();
	string out = execTestCommand(
		"palan-gen-ast ../test/testdata/gen-ast/error_013_true_as_identifier.pa");
	ASSERT_NE(out.find(":2:7: error: syntax error"), string::npos) << out;
}

TEST(gen_ast_error, unknown_char) {
	// flex echoed unmatched bytes to stdout ahead of the AST JSON. The
	// "return1::" prefix (empty stdout between the colons) checks it no longer does.
	cleanTestEnv();
	string out = execTestCommand(
		"palan-gen-ast ../test/testdata/gen-ast/error_007_unknown_char.pa");
	ASSERT_EQ(out.rfind("return1::", 0), 0u) << out;
	ASSERT_NE(out.find(":3:13: error: Unexpected character '`'."), string::npos) << out;
}

TEST(gen_ast_error, unknown_char_nonascii) {
	// Reported inside a declaration, where the GLR parser stack is split: the
	// error must still be reported exactly once.
	cleanTestEnv();
	string out = execTestCommand(
		"palan-gen-ast ../test/testdata/gen-ast/error_008_unknown_char_nonascii.pa");
	ASSERT_NE(out.find(":2:10: error: Unexpected character '\\xC3'."), string::npos) << out;
	ASSERT_EQ(out.find("syntax error"), string::npos) << out;
}

TEST(gen_ast_error, invalid_char_literal) {
	cleanTestEnv();
	const char* cases[][2] = {
		{"error_009_char_literal_empty.pa", ":2:11: error: Invalid character literal ''."},
		{"error_010_char_literal_multi.pa",
		 ":2:11: error: Character literal 'ab' holds more than one code point; write it as a string literal."},
		{"error_011_char_literal_escape.pa", ":2:11: error: Invalid character literal '\\q'."},
		{"error_012_char_literal_invalid_utf8.pa",
		 ":2:11: error: Character literal is not valid UTF-8; write bytes in a string literal with '\\xHH'."},
		{"error_028_char_literal_combining.pa",
		 ":2:11: error: Character literal 'e\xCC\x81' holds more than one code point; write it as a string literal."},
	};
	for (auto& c : cases) {
		string out = execTestCommand(string("palan-gen-ast ../test/testdata/gen-ast/") + c[0]);
		EXPECT_EQ(out.rfind("return1::", 0), 0u) << out;
		EXPECT_NE(out.find(c[1]), string::npos) << out;
	}
}

TEST(gen_ast_error, invalid_code_point_escape) {
	const string rule = "': '\\u{X}' takes 1 to 6 hex digits, up to 10FFFF and not a surrogate (D800 to DFFF).";
	const pair<string, string> cases[] = {
		{"error_029_code_point_surrogate.pa", ":1:11: error: Invalid code point escape '\\u{D800}" + rule},
		{"error_030_code_point_too_large.pa", ":1:11: error: Invalid code point escape '\\u{110000}" + rule},
		{"error_031_code_point_no_digits.pa", ":1:11: error: Invalid code point escape '\\u{}" + rule},
		{"error_032_code_point_seven_digits.pa", ":1:11: error: Invalid code point escape '\\u{0000041}" + rule},
		{"error_033_code_point_unclosed.pa", ":1:11: error: Invalid code point escape '\\u{41" + rule},
		{"error_034_code_point_no_brace.pa", ":1:11: error: Invalid code point escape '\\u" + rule},
		{"error_035_field_marker.pa", ":1:17: error: Expected 'private' or 'readonly' before the field's type, but found 'publik'."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string out = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/" + file);
		EXPECT_EQ(out.rfind("return1::", 0), 0u) << out;
		EXPECT_NE(out.find(expected), string::npos) << out;
	}
}

TEST(gen_ast_error, invalid_number_literal) {
	cleanTestEnv();
	const char* cases[][2] = {
		{"error_014_number_literal_bare_exponent.pa", ":2:12: error: Invalid number literal '1e'."},
		{"error_015_number_literal_int_suffix.pa", ":2:12: error: Invalid number literal '100px'."},
		{"error_016_number_literal_float_suffix.pa", ":2:12: error: Invalid number literal '1.5f'."},
		{"error_017_number_literal_exponent_suffix.pa", ":2:12: error: Invalid number literal '1e10x'."},
		{"error_036_hex_no_digits.pa", ":2:12: error: Invalid number literal '0x'."},
		{"error_037_hex_bad_digit.pa", ":2:12: error: Invalid number literal '0xG'."},
		{"error_038_hex_fraction.pa", ":2:12: error: Invalid number literal '0x1.5'."},
		{"error_039_hex_too_large.pa", ":2:12: error: Hexadecimal literal '0x10000000000000000' does not fit in 64 bits."},
	};
	for (auto& c : cases) {
		string out = execTestCommand(string("palan-gen-ast ../test/testdata/gen-ast/") + c[0]);
		EXPECT_EQ(out.rfind("return1::", 0), 0u) << out;
		EXPECT_NE(out.find(c[1]), string::npos) << out;
	}
}

TEST(gen_ast_error, syscall_named_return) {
	cleanTestEnv();
	const string msg = "error: a syscall must declare its return with '-> type'; named and "
		"multiple return values are not supported.";
	const char* cases[][2] = {
		{"error_018_syscall_named_return.pa", ":1:18: "},
		{"error_019_syscall_multi_return.pa", ":1:18: "},
		{"error_020_syscall_multi_return_inherit.pa", ":1:18: "},
	};
	for (auto& c : cases) {
		string out = execTestCommand(string("palan-gen-ast ../test/testdata/gen-ast/") + c[0]);
		EXPECT_EQ(out.rfind("return1::", 0), 0u) << out;
		EXPECT_NE(out.find(c[1] + msg), string::npos) << out;
	}
}

TEST(gen_ast_error, for_expected_in) {
	cleanTestEnv();
	string out = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/error_021_for_expected_in.pa");
	EXPECT_EQ(out.rfind("return1::", 0), 0u) << out;
	EXPECT_NE(out.find(":1:12: error: syntax error, unexpected identifier, expecting in or ','"), string::npos) << out;
}

// 'in' is reserved so a 'for' range never reads as a typed index.
TEST(gen_ast_error, in_reserved) {
	cleanTestEnv();
	ofstream("out/m.pa") << "int32 in = 1;\n";
	string out = execTestCommand("palan-gen-ast out/m.pa");
	EXPECT_NE(out.find("m.pa:1:7: error: syntax error, unexpected in"), string::npos) << out;
}

// '{}' at the start of a statement is an empty block, so it is never the source of '->'.
TEST(gen_ast_error, empty_braces_statement) {
	cleanTestEnv();
	ofstream("out/m.pa") << "[\"\"]int32 d;\n{} -> d;\n";
	string out = execTestCommand("palan-gen-ast out/m.pa");
	EXPECT_NE(out.find("m.pa:2:4: error: syntax error, unexpected ->"), string::npos) << out;
}

TEST(gen_ast_error, for_elem_mode) {
	const pair<string, string> cases[] = {
		{"error_022_for_bare_elem.pa", ":1:5: error: The loop variable 'a' needs 'copy', '@' or '@!': 'copy a' copies each element, '@a'/'@!a' is the element itself."},
		{"error_023_for_expected_copy.pa", ":1:5: error: Expected 'copy', '@' or '@!' before the loop variable, but found 'cpy'."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string out = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/" + file);
		EXPECT_EQ(out.rfind("return1::", 0), 0u) << out;
		EXPECT_NE(out.find(expected), string::npos) << out;
	}
}

// Only '==' and '<' are defined; the other comparisons are derived from them.
TEST(gen_ast_error, derived_compare_func) {
	const pair<string, string> cases[] = {
		{"error_024_derived_compare_func_ne.pa", ":2:6: error: '!=' cannot be defined: it is derived from '=='."},
		{"error_025_derived_compare_func_ge.pa", ":2:6: error: '>=' cannot be defined: it is derived from '<'."},
		{"error_026_derived_compare_func_gt.pa", ":2:6: error: '>' cannot be defined: it is derived from '<'."},
		{"error_027_derived_compare_func_le.pa", ":2:6: error: '<=' cannot be defined: it is derived from '<'."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string out = execTestCommand("palan-gen-ast ../test/testdata/gen-ast/" + file);
		EXPECT_EQ(out.rfind("return1::", 0), 0u) << out;
		EXPECT_NE(out.find(expected), string::npos) << out;
	}
}

TEST(gen_ast_error, cinclude_local_path_injection) {
	// Same shell-metacharacter-injection risk as cinclude_sys_path_injection
	// above, for the "..." local-path branch (fs::path(base_dir) / resolved).
	cleanTestEnv();
	execTestCommand("rm -f PWNED");
	string out = execTestCommand(
		"palan-gen-ast ../test/testdata/gen-ast/error_005_cinclude_local_injection.pa");
	ASSERT_NE(out.find("Failed to read C header"), string::npos);
	ASSERT_FALSE(fs::exists("PWNED"));
}

TEST(gen_ast_error, nesting_too_deep) {
	cleanTestEnv();
	const int depth = 4000;
	string src = "func f() {\nint32 a = 0;\n";
	for (int i = 0; i < depth; i++) src += "while 1 < 2 {\n";
	src += "a + 1 -> a;\n" + string(depth, '}') + "\n}\n";
	ofstream("out/nesting_too_deep.pa") << src;

	string out = execTestCommand("palan-gen-ast out/nesting_too_deep.pa");
	EXPECT_EQ(out.rfind("return1::", 0), 0u) << out.substr(0, 200);
	EXPECT_NE(out.find("error: memory exhausted"), string::npos) << out.substr(0, 200);
}

TEST(gen_ast_error, union_syntax) {
	const pair<string, string> cases[] = {
		{"int32 union = 1;\n", ":1:7: error: syntax error, unexpected union"},
		{"int32 is = 1;\n", ":1:10: error: syntax error, unexpected '=', expecting identifier or @! or '@'"},
		{"type U union { };\n", ":1:16: error: syntax error, unexpected '}'"},
		{"type U union { int32 i = 1; };\n", ":1:24: error: syntax error, unexpected '='"},
		{"type U union { private int32 i; };\n", ":1:30: error: syntax error, unexpected identifier, expecting ';'"},
		{"int32 j;\nbool b = j is cpy i;\n", ":2:15: error: Expected 'copy', '@' or '@!' before 'i', but found 'cpy'."},
		{"int32 j;\nif j is { else { } i { } }\n", ":2:11: error: syntax error, unexpected else"},
		{"int32 j;\nif j is { i { } } else { }\n", ":2:19: error: syntax error, unexpected else"},
		{"int32 j;\nwhile j is { i { } }\n", ":2:12: error: syntax error, unexpected '{'"},
	};
	for (auto& [src, expected] : cases) {
		cleanTestEnv();
		ofstream("out/m.pa") << src;
		string out = execTestCommand("palan-gen-ast out/m.pa");
		EXPECT_NE(out.find("m.pa" + expected), string::npos) << src << out;
	}
}
