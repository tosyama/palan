/// c2ast tests: preprocessor
///
/// @file preprocessorTests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include <algorithm>
#include <fstream>
#include <set>
#include "../test-base/testBase.h"
#include "../../lib/json/single_include/nlohmann/json.hpp"

using json = nlohmann::json;
using namespace std;

TEST(c2ast, va_args) {
    cleanTestEnv();
    string output;
    output = execTestCommand("palan-c2ast -d ../test/testdata/c2ast/001_va_args.h");
    ASSERT_EQ(output, "int testproc(){mylog(\"hello\",1,2);}");
}

TEST(c2ast, named_va_args) {
    cleanTestEnv();
    string output;
    output = execTestCommand("palan-c2ast -d ../test/testdata/c2ast/046_named_va_args.h");
    ASSERT_EQ(output, "int testproc(){mylog(\"none\",);mylog(\"one\",1);mylog(\"many\",1,(2,3),4);"
        "\"a , b\";\"c , d\";}union {struct {int x;int y;}a;struct tg{int x;int y;}b;}g;");
}

TEST(c2ast, func_macro_no_paren) {
    // A function-like macro's name not followed by '(' is not an invocation.
    cleanTestEnv();
    string output;
    output = execTestCommand("palan-c2ast -d ../test/testdata/c2ast/048_func_macro_no_paren.h");
    ASSERT_EQ(output, "int testproc(){returnF;returnF+1;returnF;return(2+1);return(3+1);}");
    output = execTestCommand("palan-c2ast ../test/testdata/c2ast/048_func_macro_no_paren.h");
    ASSERT_NE(output.find("{\"name\":\"K\"}"), string::npos);
}

TEST(c2ast, token_paste_nonid) {
    cleanTestEnv();
    string output;
    output = execTestCommand("palan-c2ast -d ../test/testdata/c2ast/002_token_paste.h");
    ASSERT_EQ(output, "int testproc(){returnval1;return1val;return12;returnint_t;}");
}

TEST(c2ast, pragma_once) {
    cleanTestEnv();
    string output;
    output = execTestCommand("palan-c2ast -d ../test/testdata/c2ast/003_pragma_once_main.h");
    ASSERT_EQ(output, "int result=1;");
}

TEST(c2ast, char_const_in_if) {
    cleanTestEnv();
    string output;
    output = execTestCommand("palan-c2ast -d ../test/testdata/c2ast/004_char_const.h");
    ASSERT_EQ(output, "int char_result=1;");
}

TEST(c2ast, token_paste_keyword) {
    cleanTestEnv();
    string output;
    output = execTestCommand("palan-c2ast -d ../test/testdata/c2ast/005_token_paste_kw.h");
    ASSERT_EQ(output, "int x;");
}

TEST(c2ast, stringizing) {
    cleanTestEnv();
    string output;
    output = execTestCommand("palan-c2ast -d ../test/testdata/c2ast/007_stringify.h");
    ASSERT_EQ(output, "int n=\"hello\";char *s=\"1 + 2\";");
}

TEST(c2ast, warning_directive) {
    cleanTestEnv();
    string output;
    output = execTestCommand("palan-c2ast -d ../test/testdata/c2ast/006_warning.h");
    ASSERT_TRUE(output.find("int warning_test=1;") != string::npos);
    ASSERT_TRUE(output.find("warning: this is a test warning") != string::npos);
}

TEST(c2ast, include_macro) {
    cleanTestEnv();
    string output;
    output = execTestCommand("palan-c2ast -d ../test/testdata/c2ast/008_include_macro.h");
    ASSERT_EQ(output, "int macro_include_result=1;");
}

TEST(c2ast, include_next_warning) {
    cleanTestEnv();
    string output;
    output = execTestCommand("palan-c2ast -d ../test/testdata/c2ast/010_include_next.h");
    ASSERT_TRUE(output.find("int include_next_result=1;") != string::npos);
    ASSERT_TRUE(output.find("warning: '#include_next' is not supported, ignored") != string::npos);
}

TEST(c2ast, line_directive_warning_once) {
    cleanTestEnv();
    string output;
    output = execTestCommand("palan-c2ast -d ../test/testdata/c2ast/011_line_directive.h");
    ASSERT_TRUE(output.find("int line_directive_result=1;") != string::npos);
    // warning appears exactly once despite two #line directives
    size_t first = output.find("warning: '#line' is not supported");
    ASSERT_NE(first, string::npos);
    ASSERT_EQ(output.find("warning: '#line' is not supported", first + 1), string::npos);
}

TEST(c2ast, unknown_directive_warning) {
    cleanTestEnv();
    string output;
    output = execTestCommand("palan-c2ast -d ../test/testdata/c2ast/012_unknown_directive.h");
    ASSERT_TRUE(output.find("int unknown_dir_result=1;") != string::npos);
    ASSERT_TRUE(output.find("warning: unsupported directive '#ident'") != string::npos);
}

TEST(c2ast, include_macro_bracket) {
    cleanTestEnv();
    string output;
    output = execTestCommand("palan-c2ast -d ../test/testdata/c2ast/009_include_macro_bracket.h");
    ASSERT_TRUE(output.find("int printf(const char") != string::npos);
}

TEST(c2ast, relational_ops) {
    cleanTestEnv();
    string output = execTestCommand("palan-c2ast ../test/testdata/c2ast/014_relational_ops.h");
    json ast = json::parse(output);
    auto& functions = ast["ast"]["functions"];
    bool found = false;
    for (auto& f : functions) if (f["name"] == "f") found = true;
    ASSERT_TRUE(found);
}

TEST(c2ast, macro_const_simple) {
    cleanTestEnv();
    string output = execTestCommand("palan-c2ast ../test/testdata/c2ast/018_macro_const_simple.h");
    json ast = json::parse(output);
    auto& constants = ast["ast"]["constants"];

    auto find_const = [&](const string& name) -> json* {
        for (auto& c : constants)
            if (c["name"] == name) return &c;
        return nullptr;
    };

    json* magic = find_const("MAGIC");
    ASSERT_NE(magic, nullptr);
    ASSERT_EQ((*magic)["value"], "42");
    // Unsuffixed and uncast: untyped, like a Palan source literal.
    ASSERT_FALSE(magic->contains("value-type"));

    // An additive expression: folded now that binary operators are evaluated.
    json* complex_ = find_const("COMPLEX");
    ASSERT_NE(complex_, nullptr);
    ASSERT_EQ((*complex_)["value"], "3");
    ASSERT_FALSE(complex_->contains("value-type"));
}

TEST(c2ast, macro_const_null) {
    cleanTestEnv();
    string output = execTestCommand("palan-c2ast ../test/testdata/c2ast/019_macro_const_null.h");
    json ast = json::parse(output);
    auto& constants = ast["ast"]["constants"];

    json* null_const = nullptr;
    for (auto& c : constants)
        if (c["name"] == "NULL") { null_const = &c; break; }
    ASSERT_NE(null_const, nullptr);
    ASSERT_EQ((*null_const)["value"], "0");
    ASSERT_EQ((*null_const)["value-type"]["type-kind"], "pntr");
    ASSERT_EQ((*null_const)["value-type"]["base-type"]["type-kind"], "prim");
    ASSERT_EQ((*null_const)["value-type"]["base-type"]["type-name"], "void");
}

TEST(c2ast, macro_const_alias_chain) {
    cleanTestEnv();
    string output = execTestCommand("palan-c2ast ../test/testdata/c2ast/024_macro_const_alias.h");
    json ast = json::parse(output);
    auto& constants = ast["ast"]["constants"];

    auto find_const = [&](const string& name) -> json* {
        for (auto& c : constants)
            if (c["name"] == name) return &c;
        return nullptr;
    };

    for (const string& name : {"A", "B", "C"}) {
        json* c = find_const(name);
        ASSERT_NE(c, nullptr) << "expected " << name << " to be exported";
        ASSERT_EQ((*c)["value"], "5");
        ASSERT_FALSE(c->contains("value-type"));
    }

    // Expands to an additive expression: folded now that binary operators are evaluated.
    json* d = find_const("D");
    ASSERT_NE(d, nullptr);
    ASSERT_EQ((*d)["value"], "6");
    ASSERT_FALSE(d->contains("value-type"));
}

TEST(c2ast, macro_const_fold_expr) {
    cleanTestEnv();
    string output = execTestCommand("palan-c2ast ../test/testdata/c2ast/029_macro_const_expr.h");
    json ast = json::parse(output);
    auto& constants = ast["ast"]["constants"];

    auto find_const = [&](const string& name) -> json* {
        for (auto& c : constants)
            if (c["name"] == name) return &c;
        return nullptr;
    };

    auto expect_value = [&](const string& name, const string& value) {
        json* c = find_const(name);
        ASSERT_NE(c, nullptr) << "expected " << name << " to be exported";
        ASSERT_EQ((*c)["value"], value) << "for " << name;
    };

    expect_value("BITS", "448");
    expect_value("SUB", "87");     // left-associative: (100 - 10) - 3, not 100 - (10 - 3)
    expect_value("SHIFTS", "8");   // left-associative: (64 >> 2) >> 1, not 64 >> (2 >> 1)
    expect_value("DIVS", "10");    // left-associative: (100 / 5) / 2, not 100 / (5 / 2)
    expect_value("MIXED", "13");   // precedence: 2 + (3 * 4) - 1
    expect_value("MASK", "63");
    expect_value("NEG", "-2");
    expect_value("MOD", "2");

    // Relational operators are recognized but intentionally not folded.
    ASSERT_EQ(find_const("CMP"), nullptr);
    // Would be undefined behavior to evaluate ourselves: folds to null, not a wrong value.
    ASSERT_EQ(find_const("DIVZERO"), nullptr);
    ASSERT_EQ(find_const("MODZERO"), nullptr);
    ASSERT_EQ(find_const("BIGSHIFT"), nullptr);
    ASSERT_EQ(find_const("OVERFLOWED"), nullptr);
    expect_value("SIZED", "16");
    // An unresolved identifier keeps the whole expression null.
    ASSERT_EQ(find_const("IDENT"), nullptr);
    json* suffixed = find_const("SUFFIXED");
    ASSERT_NE(suffixed, nullptr);
    ASSERT_EQ((*suffixed)["value"], "2");
    ASSERT_EQ((*suffixed)["value-type"]["type-name"], "int64");
}

TEST(c2ast, macro_const_suffix) {
    cleanTestEnv();
    string output = execTestCommand("palan-c2ast ../test/testdata/c2ast/038_macro_const_suffix.h");
    json ast = json::parse(output);
    auto& constants = ast["ast"]["constants"];

    auto find_const = [&](const string& name) -> json* {
        for (auto& c : constants)
            if (c["name"] == name) return &c;
        return nullptr;
    };

    auto expect_const = [&](const string& name, const string& value, const string& type_name) {
        json* c = find_const(name);
        ASSERT_NE(c, nullptr) << "expected " << name << " to be exported";
        ASSERT_EQ((*c)["value"], value) << "for " << name;
        ASSERT_EQ((*c)["value-type"]["type-kind"], "prim") << "for " << name;
        ASSERT_EQ((*c)["value-type"]["type-name"], type_name) << "for " << name;
    };

    expect_const("U1", "1", "uint32");
    expect_const("UL5", "5", "uint64");
    expect_const("ULL5", "5", "uint64");
    expect_const("L5", "5", "int64");
    expect_const("H80", "2147483648", "uint32");
    expect_const("ULMAX", "18446744073709551615", "uint64");
    expect_const("SH", "262144", "uint32");
    expect_const("AREV", "262144", "uint32");
    expect_const("MASK", "255", "uint32");
    expect_const("WRAP", "4294967295", "uint32");
    expect_const("NEGU", "4294967295", "uint32");
    expect_const("MIXNEG", "0", "uint32");
    expect_const("UCH", "44", "uint8");
    expect_const("LADD", "2", "int64");
    expect_const("UOR", "255", "uint32");
    expect_const("UAND", "48", "uint32");
    expect_const("UXOR", "240", "uint32");
    expect_const("UMUL", "15", "uint32");
    expect_const("UDIV", "3", "uint32");
    expect_const("UMOD", "1", "uint32");
    expect_const("LU5", "5", "uint64");
    expect_const("HEXL", "18446744073709551615", "uint64");
    expect_const("SNARROW", "-25536", "int16");
    expect_const("UNOT", "4294967295", "uint32");
    expect_const("ULNOT", "18446744073709551615", "uint64");
    expect_const("UCNOT", "-1", "int32");
    expect_const("AATTR", "4294967040", "uint32");
    for (auto [name, value] : {pair{"INOT", "-6"}, pair{"LNOT", "-4294967297"}}) {
        json* c = find_const(name);
        ASSERT_NE(c, nullptr) << name;
        ASSERT_EQ((*c)["value"], value) << name;
        ASSERT_FALSE(c->contains("value-type")) << name;
    }

    for (const char* name : {"BIGSH", "SOVF", "BIGDEC", "FLO", "UCMP", "UDIVZ", "UMODZ",
                             "TOOBIG", "UNTBIG", "SSHNEG", "LSHOVF"}) {
        ASSERT_EQ(find_const(name), nullptr) << name;
        ASSERT_NE(find(ast["ast"]["unexported-macros"].begin(), ast["ast"]["unexported-macros"].end(),
                       json{{"name", name}}), ast["ast"]["unexported-macros"].end()) << name;
    }
    ASSERT_NE(find(ast["ast"]["expandables"].begin(), ast["ast"]["expandables"].end(),
                   json{{"name", "FMAC"}, {"params", {"x"}}, {"binding", "substitute"},
                        {"body", {{"expr-type", "param"}, {"name", "x"}}}}), ast["ast"]["expandables"].end());
    for (auto& m : ast["ast"]["unexported-macros"]) ASSERT_NE(m["name"], "SELF");
}

TEST(c2ast, int_constant_width) {
    cleanTestEnv();
    string output = execTestCommand("palan-c2ast -s stdlib.h");
    json ast = json::parse(output);
    auto& constants = ast["ast"]["constants"];

    auto find_const = [&](const string& name) -> json* {
        for (auto& c : constants)
            if (c["name"] == name) return &c;
        return nullptr;
    };

    // Out of int32 range: the value is kept exactly rather than truncated.
    json* wclone = find_const("__WCLONE");
    ASSERT_NE(wclone, nullptr);
    ASSERT_EQ((*wclone)["value"], "2147483648");
    ASSERT_FALSE(wclone->contains("value-type"));

    json* exit_failure = find_const("EXIT_FAILURE");
    ASSERT_NE(exit_failure, nullptr);
    ASSERT_EQ((*exit_failure)["value"], "1");
    ASSERT_FALSE(exit_failure->contains("value-type"));
}

TEST(c2ast, sys_stat_h_perm_masks) {
    cleanTestEnv();
    string output = execTestCommand("palan-c2ast -s sys/stat.h");
    json ast = json::parse(output);
    auto& constants = ast["ast"]["constants"];

    auto find_const = [&](const string& name) -> json* {
        for (auto& c : constants)
            if (c["name"] == name) return &c;
        return nullptr;
    };

    // These are all expression-bodied macros (OR/shift of other macros), previously
    // silently skipped because binary operators weren't folded.
    auto expect_value = [&](const string& name, const string& value) {
        json* c = find_const(name);
        ASSERT_NE(c, nullptr) << "expected " << name << " to be exported";
        ASSERT_EQ((*c)["value"], value) << "for " << name;
    };

    expect_value("S_IRWXU", "448");
    expect_value("S_IRGRP", "32");
    expect_value("S_IRWXG", "56");
    expect_value("S_IROTH", "4");
    expect_value("S_IRWXO", "7");
    expect_value("ACCESSPERMS", "511");
    expect_value("ALLPERMS", "4095");
    expect_value("DEFFILEMODE", "438");
}

TEST(c2ast, string_h_null_constant) {
    cleanTestEnv();
    string output = execTestCommand("palan-c2ast -s string.h");
    json ast = json::parse(output);
    auto& constants = ast["ast"]["constants"];

    // NULL comes from stddef.h, pulled in transitively via string.h.
    json* null_const = nullptr;
    for (auto& c : constants)
        if (c["name"] == "NULL") { null_const = &c; break; }
    ASSERT_NE(null_const, nullptr);
    ASSERT_EQ((*null_const)["value"], "0");
    ASSERT_EQ((*null_const)["value-type"]["type-kind"], "pntr");
    ASSERT_EQ((*null_const)["value-type"]["base-type"]["type-kind"], "prim");
    ASSERT_EQ((*null_const)["value-type"]["base-type"]["type-name"], "void");
}

TEST(c2ast, empty_header) {
    cleanTestEnv();
    string output = execTestCommand("palan-c2ast -d ../test/testdata/c2ast/033_empty_header.h");
    ASSERT_EQ(output, "");
}

TEST(c2ast, blank_lines) {
    cleanTestEnv();
    string output = execTestCommand("palan-c2ast -d ../test/testdata/c2ast/034_blank_lines.h");
    ASSERT_EQ(output, "int f(int a);int g(int b);");
}

TEST(c2ast, macro_state) {
    cleanTestEnv();
    string output = execTestCommand("palan-c2ast ../test/testdata/c2ast/045_macro_state_a.h");
    json a = json::parse(output);
    string state = a["c-state"]["macros"];
    // Spelled back token by token, so each line re-lexes to the same macro.
    for (const char* line : {
            "#define A_FLAG\n", "#define A_CAT(x, y) x ## y\n", "#define A_STR(x) # x\n",
            "#define A_VARGS(...) f ( __VA_ARGS__ )\n", "#define A_NVARGS(fn, xs...) fn ( xs )\n",
            "#define A_SHIFT ( 1 << 2 )\n",
            "#define A_TEXT \"a b\"\n", "#define A_CH 'c'\n",
            "#pragma palan_once_included \"../test/testdata/c2ast/045_macro_state_a.h\"\n"})
        ASSERT_NE(state.find(line), string::npos) << line;
    ASSERT_EQ(state.find("__CHAR_BIT__"), string::npos);

    string state_file = "045_macro_state.tmp";
    ofstream(state_file) << a["c-state"].dump();
    output = execTestCommand("palan-c2ast -S " + state_file + " ../test/testdata/c2ast/045_macro_state_b.h");
    remove(state_file.c_str());
    json b = json::parse(output);
    // a.h is skipped by its #pragma once, so a_func is not declared again.
    set<string> funcs;
    for (auto& f : b["ast"]["functions"]) funcs.insert(f["name"]);
    ASSERT_EQ(funcs, (set<string>{"a_flag_on", "cated", "nvarg"}));
    // Inherited macros are not exported again; a redefinition is.
    ASSERT_EQ(b["ast"]["constants"], json::parse(R"([
        {"name":"B_SHIFT","value":"4"}, {"name":"A_VAL","value":"5"}])"));
    ASSERT_EQ(b["ast"]["undefined-macros"], json::parse(R"(["A_GONE"])"));
    ASSERT_FALSE(b["ast"].contains("unexported-macros"));

    string b_state = b["c-state"]["macros"];
    ASSERT_EQ(b_state.find("A_GONE"), string::npos);
    ASSERT_NE(b_state.find("#define A_VAL 5\n"), string::npos);
}

// A header its include guard skips still has its types and enumerators;
// only what this header adds or completes is exported.
TEST(c2ast, c_state_types) {
    cleanTestEnv();
    json a = json::parse(execTestCommand("palan-c2ast ../test/testdata/c2ast/054_c_state_a.h"));
    string state_file = "054_c_state.tmp";
    ofstream(state_file) << a["c-state"].dump();
    string output = execTestCommand("palan-c2ast --state " + state_file + " ../test/testdata/c2ast/054_c_state_b.h");
    remove(state_file.c_str());
    json b = json::parse(output);
    ASSERT_EQ(b["ast"]["structs"], json::parse(R"([
        {"name":"st_later","fields":[{"name":"y","var-type":
            {"type-kind":"prim","type-name":"uint16","typedef-name":"st_u16"}}]},
        {"name":"st_u","fields":[{"name":"z","var-type":{"type-kind":"arr","specifier":"raw",
            "embedded":true,"base-type":{"type-kind":"prim","type-name":"uint8"},
            "size-expr":{"expr-type":"lit-int","value":"11",
                "value-type":{"type-kind":"prim","type-name":"uint64"}}}}]}])"));
    ASSERT_FALSE(b["ast"].contains("enums"));
    ASSERT_FALSE(b["ast"].contains("typedefs"));
    ASSERT_FALSE(b["ast"].contains("constants"));
    ASSERT_FALSE(b["ast"].contains("functions"));
    ASSERT_EQ(b["c-state"]["typedefs"], a["c-state"]["typedefs"]);
    ASSERT_EQ(b["c-state"]["enumerators"]["ST_ANON"], json::parse("[3, 1]"));
}

TEST(c2ast, func_alias_macro) {
    cleanTestEnv();
    string output = execTestCommand("palan-c2ast ../test/testdata/c2ast/049_func_alias_macro.h");
    json ast = json::parse(output);
    auto funcs = [&](const string& name) {
        vector<json> found;
        for (auto& f : ast["ast"]["functions"])
            if (f["name"] == name) found.push_back(f);
        return found;
    };

    auto old_f = funcs("old_f");
    ASSERT_EQ(old_f.size(), 1u);
    ASSERT_EQ(old_f[0]["symbol"], "new_f");
    ASSERT_EQ(old_f[0]["parameters"].size(), 2u);
    ASSERT_EQ(funcs("new_f")[0].contains("symbol"), false);

    ASSERT_EQ(funcs("kept_f")[0].contains("symbol"), false);
    ASSERT_EQ(funcs("first_f")[0]["symbol"], "last_f");
    ASSERT_EQ(funcs("missing_f").size(), 0u);
    ASSERT_EQ(ast["ast"]["unexported-macros"], json::parse(R"([{"name":"missing_f"}])"));

    ast = json::parse(execTestCommand("palan-c2ast -s stdio.h"));
    ASSERT_EQ(funcs("sscanf")[0]["symbol"], "__isoc99_sscanf");
}

// `linux` in <linux/types.h> is not expanded, and "#ifdef __has_include" holds.
TEST(c2ast, has_include) {
    cleanTestEnv();
    json ast = json::parse(execTestCommand("palan-c2ast ../test/testdata/c2ast/057_has_include.h"));
    vector<string> names;
    for (auto& f : ast["ast"]["functions"]) names.push_back(f["name"]);
    ASSERT_EQ(names, vector<string>({"hi_found", "hi_defined"}));
}

TEST(c2ast, include_in_enum) {
    cleanTestEnv();
    json ast = json::parse(execTestCommand("palan-c2ast ../test/testdata/c2ast/058_enum_include.h"));
    ASSERT_EQ(ast["ast"]["enums"][0]["enumerators"], json::parse(
        R"([{"name":"ei_a","value":"1"},{"name":"ei_b","value":"4"},{"name":"ei_last","value":"5"}])"));
}

// The state file and predefined.h are the tool's input, not the source's headers.
TEST(c2ast, headers_read) {
    cleanTestEnv();
    const string dir = "../test/testdata/c2ast/";
    json a = json::parse(execTestCommand("palan-c2ast " + dir + "045_macro_state_a.h"));
    ASSERT_EQ(a["headers"], json::array({dir + "045_macro_state_a.h"}));

    string state_file = "out/state";
    ofstream(state_file) << a["c-state"].dump();
    json b = json::parse(execTestCommand("palan-c2ast -S " + state_file + " " + dir + "045_macro_state_b.h"));
    // a.h is skipped by its #pragma once: the run before this one read it.
    ASSERT_EQ(b["headers"], json::array({dir + "045_macro_state_b.h"}));
}

TEST(c2ast, function_macro) {
    cleanTestEnv();
    string output = execTestCommand("palan-c2ast ../test/testdata/c2ast/059_function_macro.h");
    json ast = json::parse(output)["ast"];

    auto body = [&](const string& name) -> json {
        for (auto& e : ast["expandables"])
            if (e["name"] == name) {
                EXPECT_EQ(e["binding"], "substitute");
                return e["body"];
            }
        ADD_FAILURE() << name << " is not expandable";
        return json();
    };
    auto param = [](const string& name) { return json{{"expr-type", "param"}, {"name", name}}; };
    auto lit = [](const string& value) { return json{{"expr-type", "lit-int"}, {"value", value}}; };

    // The macros in a body are expanded, the parameter kept.
    ASSERT_EQ(body("OUTER"), (json{{"expr-type", "cmp"}, {"op", "=="},
        {"left", {{"expr-type", "bitand"}, {"left", param("s")}, {"right", lit("127")}}},
        {"right", lit("0")}}));
    for (auto& e : ast["expandables"])
        if (e["name"] == "FIELDS") ASSERT_EQ(e["params"], (json{"p", "i"}));

    json typed = body("TYPED");
    ASSERT_EQ(typed["expr-type"], "shl");
    ASSERT_EQ(typed["left"]["expr-type"], "cast");
    ASSERT_EQ(typed["left"]["target-type"]["typedef-name"], "ushort_t");
    ASSERT_EQ(typed["left"]["src"], param("c"));

    // `->` and `.` are both a field, `*p` is element 0.
    json fields = body("FIELDS");
    json next = {{"expr-type", "field-access"}, {"object", param("p")}, {"field", "next"}};
    ASSERT_EQ(fields["left"], (json{{"expr-type", "arr-index"},
        {"array", {{"expr-type", "field-access"}, {"object", next}, {"field", "v"}}}, {"index", param("i")}}));
    ASSERT_EQ(fields["right"]["array"]["object"], (json{{"expr-type", "arr-index"},
        {"array", param("p")}, {"index", lit("0")}}));

    // A C name is marked as one; an enumerator is its value.
    json calls = body("CALLS");
    ASSERT_EQ(calls["expr-type"], "logical-and");
    ASSERT_EQ(calls["left"], (json{{"expr-type", "call"}, {"name", "get"}, {"args", {param("n")}}, {"c-name", true}}));
    ASSERT_EQ(calls["right"]["expr-type"], "logical-not");
    ASSERT_EQ(calls["right"]["operand"]["args"][0], lit("3"));

    json chars = body("CHARS");
    ASSERT_EQ(chars["left"]["left"]["right"], lit("97"));
    ASSERT_EQ(chars["left"]["right"]["right"], lit("10"));
    ASSERT_EQ(chars["right"]["right"], lit("65"));
    ASSERT_EQ(body("NEGATE")["left"], lit("-1"));
    ASSERT_EQ(body("NOPARAM")["args"][0], lit("0"));

    auto unexported = [&](const string& name) -> json {
        for (auto& u : ast["unexported-macros"])
            if (u["name"] == name) return u;
        ADD_FAILURE() << name << " is not unexported";
        return json();
    };
    for (auto [name, form] : {pair{"TERNARY", "?:"}, pair{"ASSIGN", "an assignment"},
            pair{"COMMA", "a comma operator"}, pair{"STRING", "a comma operator"},
            pair{"STRINGIZE", "#"}, pair{"PASTE", "##"}, pair{"VARIADIC", "__VA_ARGS__"},
            pair{"ADDRESS", "&"}, pair{"SIZEOF_EXPR", "sizeof of an expression"},
            pair{"INCREMENT", "++/--"}, pair{"PRE_DECREMENT", "++/--"},
            pair{"CALL_PTR", "a call through an expression"},
            pair{"FLOATING", "a floating constant"}})
        ASSERT_EQ(unexported(name), (json{{"name", name}, {"form", form}, {"function-like", true}})) << name;
    ASSERT_EQ(unexported("NOT_EXPR"), (json{{"name", "NOT_EXPR"}, {"function-like", true}}));

    auto find_const = [&](const string& name) -> json* {
        for (auto& c : ast["constants"])
            if (c["name"] == name) return &c;
        return nullptr;
    };
    ASSERT_NE(find_const("CHAR_CONST"), nullptr);
    ASSERT_EQ((*find_const("CHAR_CONST"))["value"], "122");
    // `x` is no typedef, so this is a subtraction, not a cast of -1.
    ASSERT_EQ(find_const("NOT_A_CAST"), nullptr);
}
