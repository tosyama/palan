/// c2ast tests
///
/// @file basicTests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include <map>
#include <set>
#include <cstdint>
#include "../test-base/testBase.h"
#include "../../lib/json/single_include/nlohmann/json.hpp"

using json = nlohmann::json;
using namespace std;

TEST(c2ast, basic_tests) {
    cleanTestEnv();
    string output;
    output = execTestCommand("palan-c2ast -d ../test/testdata/000_temp_c_header.h");
    ASSERT_EQ(output, "int testproc(){H((A+H(1)));123;returnxSz;}");
}

TEST(c2ast, explicit_target) {
    cleanTestEnv();
    string output = execTestCommand("palan-c2ast --target x86_64-linux-gnu -s stdio.h");
    ASSERT_EQ(output[0], '{');
}

TEST(c2ast, std_headers) {
    cleanTestEnv();
    string output;
    output = execTestCommand("palan-c2ast -ds stdio.h");
	ASSERT_TRUE(output.find("int printf(const char") != string::npos);
}

TEST(c2ast, stdio_functions_in_ast) {
    cleanTestEnv();
    string output = execTestCommand("palan-c2ast -s stdio.h");
    json ast = json::parse(output);
    auto& functions = ast["ast"]["functions"];

    auto find_func = [&](const string& name) -> json* {
        for (auto& f : functions)
            if (f["name"] == name && f["func-type"] == "c") return &f;
        return nullptr;
    };

    // printf: int printf(const char *format, ...)
    {
        json* pf = find_func("printf");
        ASSERT_NE(pf, nullptr);
        // ret-type: int
        ASSERT_EQ((*pf)["ret-type"]["type-kind"], "prim");
        ASSERT_EQ((*pf)["ret-type"]["type-name"], "int32");
        // params
        auto& params = (*pf)["parameters"];
        ASSERT_GE(params.size(), 2u);
        ASSERT_EQ(params.back()["name"], "...");
        // first param: const char *
        auto& p0vt = params[0]["var-type"];
        ASSERT_EQ(p0vt["type-kind"], "pntr");
        ASSERT_EQ(p0vt["base-type"]["type-kind"], "prim");
        ASSERT_EQ(p0vt["base-type"]["type-name"], "int8");
        ASSERT_EQ(p0vt["base-type"]["const"], true);
    }

    // fgets: char *fgets(char *s, int n, FILE *stream)
    {
        json* pf = find_func("fgets");
        ASSERT_NE(pf, nullptr);
        // ret-type: char *
        auto& ret = (*pf)["ret-type"];
        ASSERT_EQ(ret["type-kind"], "pntr");
        ASSERT_EQ(ret["base-type"]["type-kind"], "prim");
        ASSERT_EQ(ret["base-type"]["type-name"], "int8");
        // params
        auto& params = (*pf)["parameters"];
        ASSERT_GE(params.size(), 3u);
        // first param: char *
        auto& p0vt = params[0]["var-type"];
        ASSERT_EQ(p0vt["type-kind"], "pntr");
        ASSERT_EQ(p0vt["base-type"]["type-kind"], "prim");
        ASSERT_EQ(p0vt["base-type"]["type-name"], "int8");
        // second param: int
        auto& p1vt = params[1]["var-type"];
        ASSERT_EQ(p1vt["type-kind"], "prim");
        ASSERT_EQ(p1vt["type-name"], "int32");
    }

    // stdin/stdout/stderr: extern FILE *NAME -- captured into ast.globals as
    // pntr(strct _IO_FILE), FILE's typedef resolved to the underlying struct.
    {
        auto& globals = ast["ast"]["globals"];
        auto find_global = [&](const string& name) -> json* {
            for (auto& g : globals)
                if (g["name"] == name) return &g;
            return nullptr;
        };
        for (const char* name : {"stdin", "stdout", "stderr"}) {
            json* g = find_global(name);
            ASSERT_NE(g, nullptr);
            auto& vt = (*g)["var-type"];
            ASSERT_EQ(vt["type-kind"], "pntr");
            ASSERT_EQ(vt["base-type"]["type-kind"], "strct");
            ASSERT_EQ(vt["base-type"]["type-name"], "_IO_FILE");
        }
    }
}

TEST(c2ast, extern_globals) {
    // struct Handle;                    -- forward-declared only, never defined
    // extern int g_count;                extern double g_ratio;
    // extern struct Handle *g_handle;   -- prim/prim/pntr(strct): all captured
    // extern int g_a, g_b;              -- comma-separated: both captured
    // static int s_hidden;              -- no external linkage: not captured
    // extern char *g_names[2];          -- array type: not captured (non-goal)
    // typedef int A, B; A take_a(B b);  -- comma-separated typedef: B also
    //                                      registered, so take_a's signature
    //                                      resolves to int32/int32, not "user"
    cleanTestEnv();
    string output = execTestCommand("palan-c2ast ../test/testdata/c2ast/028_extern_globals.h");
    json ast = json::parse(output);
    auto& globals = ast["ast"]["globals"];

    auto find_global = [&](const string& name) -> json* {
        for (auto& g : globals)
            if (g["name"] == name) return &g;
        return nullptr;
    };

    json* count = find_global("g_count");
    ASSERT_NE(count, nullptr);
    ASSERT_EQ((*count)["var-type"]["type-kind"], "prim");
    ASSERT_EQ((*count)["var-type"]["type-name"], "int32");

    json* ratio = find_global("g_ratio");
    ASSERT_NE(ratio, nullptr);
    ASSERT_EQ((*ratio)["var-type"]["type-kind"], "prim");
    ASSERT_EQ((*ratio)["var-type"]["type-name"], "flo64");

    json* handle = find_global("g_handle");
    ASSERT_NE(handle, nullptr);
    auto& hvt = (*handle)["var-type"];
    ASSERT_EQ(hvt["type-kind"], "pntr");
    ASSERT_EQ(hvt["base-type"]["type-kind"], "strct");
    ASSERT_EQ(hvt["base-type"]["type-name"], "Handle");

    ASSERT_NE(find_global("g_a"), nullptr);
    ASSERT_NE(find_global("g_b"), nullptr);

    ASSERT_EQ(find_global("s_hidden"), nullptr);
    ASSERT_EQ(find_global("g_names"), nullptr);

    ASSERT_EQ(globals.size(), 5u);

    // take_a's signature proves the comma-separated typedef "B" was
    // registered too (both A and B resolve to int32, not left as "user").
    auto& functions = ast["ast"]["functions"];
    json* take_a = nullptr;
    for (auto& f : functions)
        if (f["name"] == "take_a") take_a = &f;
    ASSERT_NE(take_a, nullptr);
    ASSERT_EQ((*take_a)["ret-type"]["type-kind"], "prim");
    ASSERT_EQ((*take_a)["ret-type"]["type-name"], "int32");
    ASSERT_EQ((*take_a)["parameters"][0]["var-type"]["type-kind"], "prim");
    ASSERT_EQ((*take_a)["parameters"][0]["var-type"]["type-name"], "int32");
}

TEST(c2ast, ctype_h_parses) {
    cleanTestEnv();
    string output = execTestCommand("palan-c2ast -s ctype.h");
    json ast = json::parse(output);
    auto& fns = ast["ast"]["functions"];
    std::set<string> names;
    for (auto& f : fns) names.insert(f["name"].get<string>());

    // isctype is excluded: guarded by #ifdef __USE_GNU in glibc's ctype.h,
    // which is not among this environment's predefined feature-test macros.
    static const char* targets[] = {
        "isalnum", "isalpha", "iscntrl", "isdigit", "islower", "isgraph",
        "isprint", "ispunct", "isspace", "isupper", "isxdigit", "isblank",
        "tolower", "toupper", "isascii", "toascii", "_toupper", "_tolower"
    };
    for (auto* name : targets)
        ASSERT_TRUE(names.count(name)) << "missing function: " << name;
}

TEST(c2ast, time_h_struct_pointer) {
    cleanTestEnv();
    string output = execTestCommand("palan-c2ast -s time.h");
    json ast = json::parse(output);
    auto& functions = ast["ast"]["functions"];

    // gmtime returns struct tm*, and struct tm is captured (defined earlier
    // in the header chain via bits/types/struct_tm.h) so the pointer's
    // base-type carries a type-name back to it.
    json* gmtime = nullptr;
    for (auto& f : functions)
        if (f["name"] == "gmtime") { gmtime = &f; break; }
    ASSERT_NE(gmtime, nullptr);
    auto& ret = (*gmtime)["ret-type"];
    ASSERT_EQ(ret["type-kind"], "pntr");
    ASSERT_EQ(ret["base-type"]["type-kind"], "strct");
    ASSERT_EQ(ret["base-type"]["type-name"], "tm");

    auto& structs = ast["ast"]["structs"];
    json* tm = nullptr;
    for (auto& s : structs)
        if (s["name"] == "tm") { tm = &s; break; }
    ASSERT_NE(tm, nullptr);
    ASSERT_GT((*tm)["fields"].size(), 0);

    // asctime(const struct tm *tp): pointee const on a struct-typed pointer
    // parameter must be captured.
    json* asctime = nullptr;
    for (auto& f : functions)
        if (f["name"] == "asctime") { asctime = &f; break; }
    ASSERT_NE(asctime, nullptr);
    auto& asctime_p0 = (*asctime)["parameters"][0]["var-type"];
    ASSERT_EQ(asctime_p0["type-kind"], "pntr");
    ASSERT_EQ(asctime_p0["base-type"]["type-kind"], "strct");
    ASSERT_EQ(asctime_p0["base-type"]["const"], true);
}

TEST(c2ast, sys_stat_h_public_names) {
    cleanTestEnv();
    string output = execTestCommand("palan-c2ast -s sys/stat.h");
    json ast = json::parse(output);
    auto& constants = ast["ast"]["constants"];

    // S_IFDIR is defined as an alias of __S_IFDIR; it must be exported too, not just
    // the internal name.
    json* s_ifdir = nullptr;
    for (auto& c : constants)
        if (c["name"] == "S_IFDIR") { s_ifdir = &c; break; }
    ASSERT_NE(s_ifdir, nullptr);
    ASSERT_EQ((*s_ifdir)["value"], "16384");
    ASSERT_FALSE(s_ifdir->contains("value-type"));
}

TEST(c2ast, array_decl) {
    cleanTestEnv();
    string output = execTestCommand("palan-c2ast ../test/testdata/c2ast/023_array_decl.h");
    json ast = json::parse(output);
    auto& functions = ast["ast"]["functions"];
    auto& structs = ast["ast"]["structs"];

    auto find_func = [&](const string& name) -> json* {
        for (auto& f : functions)
            if (f["name"] == name) return &f;
        return nullptr;
    };
    auto find_struct = [&](const string& name) -> json* {
        for (auto& s : structs)
            if (s["name"] == name) return &s;
        return nullptr;
    };
    auto find_field = [](json& fields, const string& name) -> json* {
        for (auto& f : fields)
            if (f["name"] == name) return &f;
        return nullptr;
    };

    // char name[16]; -- 1D array field, prim leaf
    json* rec = find_struct("Rec");
    ASSERT_NE(rec, nullptr);
    json* name_field = find_field((*rec)["fields"], "name");
    ASSERT_NE(name_field, nullptr);
    auto& name_vt = (*name_field)["var-type"];
    ASSERT_EQ(name_vt["type-kind"], "arr");
    ASSERT_EQ(name_vt["embedded"], true);
    ASSERT_EQ(name_vt["specifier"], "raw");
    ASSERT_EQ(name_vt["size-expr"]["expr-type"], "lit-int");
    ASSERT_EQ(name_vt["size-expr"]["value"], "16");
    ASSERT_EQ(name_vt["base-type"]["type-kind"], "prim");
    ASSERT_EQ(name_vt["base-type"]["type-name"], "int8");

    // struct Point pts[3]; -- 1D array field, struct leaf (kept as "strct" -- SA
    // normalizes this to "prim" at the registration boundary, not c2ast)
    json* pts_field = find_field((*rec)["fields"], "pts");
    ASSERT_NE(pts_field, nullptr);
    auto& pts_vt = (*pts_field)["var-type"];
    ASSERT_EQ(pts_vt["type-kind"], "arr");
    ASSERT_EQ(pts_vt["size-expr"]["value"], "3");
    ASSERT_EQ(pts_vt["base-type"]["type-kind"], "strct");
    ASSERT_EQ(pts_vt["base-type"]["type-name"], "Point");

    // int cells[2][3]; -- 2D array field, nested arr (outer dim first)
    json* grid = find_struct("Grid2D");
    ASSERT_NE(grid, nullptr);
    json* cells_field = find_field((*grid)["fields"], "cells");
    ASSERT_NE(cells_field, nullptr);
    auto& cells_vt = (*cells_field)["var-type"];
    ASSERT_EQ(cells_vt["type-kind"], "arr");
    ASSERT_EQ(cells_vt["size-expr"]["value"], "2");
    auto& cells_inner = cells_vt["base-type"];
    ASSERT_EQ(cells_inner["type-kind"], "arr");
    ASSERT_EQ(cells_inner["size-expr"]["value"], "3");
    ASSERT_EQ(cells_inner["base-type"]["type-name"], "int32");

    // #define N 4; int arr[N]; -- macro-sized array resolves to a literal
    json* macro_sized = find_struct("MacroSized");
    ASSERT_NE(macro_sized, nullptr);
    json* arr_field = find_field((*macro_sized)["fields"], "arr");
    ASSERT_NE(arr_field, nullptr);
    ASSERT_EQ((*arr_field)["var-type"]["size-expr"]["value"], "4");

    // int f(char buf[32], struct Rec recs[2]); -- parameter array decay
    json* f = find_func("f");
    ASSERT_NE(f, nullptr);
    auto& buf_vt = (*f)["parameters"][0]["var-type"];
    ASSERT_EQ(buf_vt["type-kind"], "pntr");
    ASSERT_EQ(buf_vt["base-type"]["type-kind"], "prim");
    ASSERT_EQ(buf_vt["base-type"]["type-name"], "int8");
    auto& recs_vt = (*f)["parameters"][1]["var-type"];
    ASSERT_EQ(recs_vt["type-kind"], "pntr");
    ASSERT_EQ(recs_vt["base-type"]["type-kind"], "strct");
    ASSERT_EQ(recs_vt["base-type"]["type-name"], "Rec");

    // int g(char buf[2][3]); -- only the outer dimension decays
    json* g = find_func("g");
    ASSERT_NE(g, nullptr);
    auto& g_buf_vt = (*g)["parameters"][0]["var-type"];
    ASSERT_EQ(g_buf_vt["type-kind"], "pntr");
    ASSERT_EQ(g_buf_vt["base-type"]["type-kind"], "arr");
    ASSERT_EQ(g_buf_vt["base-type"]["size-expr"]["value"], "3");
    ASSERT_EQ(g_buf_vt["base-type"]["base-type"]["type-name"], "int8");
}

TEST(c2ast, ptr_array_decl) {
    // Regression guard: postfix suffixes ('[n]', '(params)') must bind
    // tighter than the prefix '*', so
    // "int *a[3]" (array of pointers) and "int (*a)[3]" (pointer to array) must
    // NOT come out swapped.
    cleanTestEnv();
    string output = execTestCommand("palan-c2ast ../test/testdata/c2ast/026_ptr_array_decl.h");
    json ast = json::parse(output);
    auto& functions = ast["ast"]["functions"];
    auto& structs = ast["ast"]["structs"];

    auto find_func = [&](const string& name) -> json* {
        for (auto& f : functions)
            if (f["name"] == name) return &f;
        return nullptr;
    };
    auto find_struct = [&](const string& name) -> json* {
        for (auto& s : structs)
            if (s["name"] == name) return &s;
        return nullptr;
    };
    auto find_field = [](json& fields, const string& name) -> json* {
        for (auto& f : fields)
            if (f["name"] == name) return &f;
        return nullptr;
    };

    json* ptr_arr = find_struct("PtrArr");
    ASSERT_NE(ptr_arr, nullptr);

    // int *ptrs[3]; -- array of 3 pointers to int
    json* ptrs = find_field((*ptr_arr)["fields"], "ptrs");
    ASSERT_NE(ptrs, nullptr);
    auto& ptrs_vt = (*ptrs)["var-type"];
    ASSERT_EQ(ptrs_vt["type-kind"], "arr");
    ASSERT_EQ(ptrs_vt["size-expr"]["value"], "3");
    ASSERT_EQ(ptrs_vt["base-type"]["type-kind"], "pntr");
    ASSERT_EQ(ptrs_vt["base-type"]["base-type"]["type-name"], "int32");

    // int (*grouped)[3]; -- pointer to an array of 3 ints
    json* grouped = find_field((*ptr_arr)["fields"], "grouped");
    ASSERT_NE(grouped, nullptr);
    auto& grouped_vt = (*grouped)["var-type"];
    ASSERT_EQ(grouped_vt["type-kind"], "pntr");
    ASSERT_EQ(grouped_vt["base-type"]["type-kind"], "arr");
    ASSERT_EQ(grouped_vt["base-type"]["size-expr"]["value"], "3");
    ASSERT_EQ(grouped_vt["base-type"]["base-type"]["type-name"], "int32");

    // void (*fp)(int); -- pointer to function, no longer needing the removed
    // is_grouped special case to avoid mis-parsing as a plain function.
    json* fp = find_field((*ptr_arr)["fields"], "fp");
    ASSERT_NE(fp, nullptr);
    auto& fp_vt = (*fp)["var-type"];
    ASSERT_EQ(fp_vt["type-kind"], "pntr");
    ASSERT_EQ(fp_vt["base-type"]["type-kind"], "func");
    ASSERT_EQ(fp_vt["base-type"]["ret-type"]["type-name"], "void");

    // int f(char *argv[]); -- array-of-pointer parameter decay: only the outer
    // array dimension decays, leaving "pointer to pointer to char".
    json* f = find_func("f");
    ASSERT_NE(f, nullptr);
    auto& argv_vt = (*f)["parameters"][0]["var-type"];
    ASSERT_EQ(argv_vt["type-kind"], "pntr");
    ASSERT_EQ(argv_vt["base-type"]["type-kind"], "pntr");
    ASSERT_EQ(argv_vt["base-type"]["base-type"]["type-name"], "int8");

    // int * const * p; -- const binds to the inner pointer (the one closer to
    // the base type), matching "p is a non-const pointer to a const pointer
    // to int".
    json* qual_ptr = find_struct("QualPtr");
    ASSERT_NE(qual_ptr, nullptr);
    json* p = find_field((*qual_ptr)["fields"], "p");
    ASSERT_NE(p, nullptr);
    auto& p_vt = (*p)["var-type"];
    ASSERT_EQ(p_vt["type-kind"], "pntr");
    ASSERT_FALSE(p_vt.contains("const"));
    ASSERT_EQ(p_vt["base-type"]["type-kind"], "pntr");
    ASSERT_EQ(p_vt["base-type"]["const"], true);
}

TEST(c2ast, array_size_expr_fold) {
    cleanTestEnv();
    string output = execTestCommand("palan-c2ast ../test/testdata/c2ast/030_array_size_expr.h");
    json ast = json::parse(output);
    auto& structs = ast["ast"]["structs"];

    json* sizes = nullptr;
    for (auto& s : structs)
        if (s["name"] == "Sizes") { sizes = &s; break; }
    ASSERT_NE(sizes, nullptr);

    auto find_field = [](json& fields, const string& name) -> json* {
        for (auto& f : fields)
            if (f["name"] == name) return &f;
        return nullptr;
    };

    // char a[100 - 10 - 3]; -- folded to a lit-int size-expr now that array
    // sizes go through the same expression chain as macro bodies.
    json* a = find_field((*sizes)["fields"], "a");
    ASSERT_NE(a, nullptr);
    ASSERT_EQ((*a)["var-type"]["size-expr"]["expr-type"], "lit-int");
    ASSERT_EQ((*a)["var-type"]["size-expr"]["value"], "87");

    // char b[4 * sizeof(int) - 2]; -- sizeof is size_t, so the result is too.
    json* b = find_field((*sizes)["fields"], "b");
    ASSERT_NE(b, nullptr);
    ASSERT_EQ((*b)["var-type"]["size-expr"]["value"], "14");
    ASSERT_EQ((*b)["var-type"]["size-expr"]["value-type"]["type-name"], "uint64");
}

TEST(c2ast, sizeof_fold) {
    cleanTestEnv();
    string output = execTestCommand("palan-c2ast ../test/testdata/c2ast/043_sizeof.h");
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
        ASSERT_EQ((*c)["value-type"]["type-name"], type_name) << "for " << name;
    };

    // Expected values are what gcc gives on x86-64.
    expect_const("SZ_CHAR", "1", "uint64");
    expect_const("SZ_SHORT", "2", "uint64");
    expect_const("SZ_INT", "4", "uint64");
    expect_const("SZ_LONG", "8", "uint64");
    expect_const("SZ_DOUBLE", "8", "uint64");
    expect_const("SZ_LDOUBLE", "16", "uint64");
    expect_const("SZ_BOOL", "1", "uint64");
    expect_const("SZ_PTR", "8", "uint64");
    expect_const("SZ_FUNCPTR", "8", "uint64");
    expect_const("SZ_TYPEDEF", "8", "uint64");
    expect_const("SZ_TDPTR", "8", "uint64");
    expect_const("SZ_ARR", "24", "uint64");
    expect_const("SZ_PAD", "24", "uint64");
    expect_const("SZ_UNION", "8", "uint64");
    expect_const("SZ_NEST", "48", "uint64");
    expect_const("SZ_FLEX", "8", "uint64");
    expect_const("SZ_SMALL", "4", "uint64");
    expect_const("SZ_BIG", "8", "uint64");
    expect_const("SZ_ARRS", "64", "uint64");
    expect_const("SZ_CAST", "64", "int32");
    expect_const("SZ_EXPR", "20", "uint64");
    ASSERT_EQ(find_const("SZ_VOID"), nullptr);
    ASSERT_EQ(find_const("SZ_OPAQUE"), nullptr);
    ASSERT_EQ(find_const("SZ_UNKNOWN"), nullptr);
}

TEST(c2ast, sizeof_unknown) {
    cleanTestEnv();
    string output = execTestCommand("palan-c2ast ../test/testdata/c2ast/050_sizeof_unknown.h");
    json ast = json::parse(output);
    map<string, string> constants;
    for (auto& c : ast["ast"]["constants"]) constants[c["name"]] = c["value"];
    // An enum value below INT32_MIN widens the enum to 8 bytes.
    ASSERT_EQ(constants["SZ_E4"], "8");
    // A layout or operand c2ast cannot compute leaves the macro unfolded.
    for (const char* name : {"SZ_A2", "SZ_A3", "SZ_A4", "SZ_A5", "SZ_E2", "SZ_EU", "BN"})
        ASSERT_EQ(constants.count(name), 0) << name;
}

TEST(c2ast, unfolded_arr_size) {
    cleanTestEnv();
    string output = execTestCommand("palan-c2ast ../test/testdata/c2ast/051_unfolded_arr_size.h");
    json ast = json::parse(output);
    map<string, json> tails;
    for (auto& s : ast["ast"]["structs"]) tails[s["name"]] = s["fields"][1]["var-type"]["size-expr"];
    map<string, string> constants;
    for (auto& c : ast["ast"]["constants"]) constants[c["name"]] = c["value"];

    // Only `[]` is a flexible array member; a size that doesn't fold is not one.
    for (const char* name : {"T", "V"}) {
        ASSERT_EQ(tails.at(name)["expr-type"], "not-const") << name;
        ASSERT_EQ(constants.count(string("SZ_") + name), 0) << name;
    }
    ASSERT_TRUE(tails.at("U").is_null());
    ASSERT_EQ(constants["SZ_U"], "4");
}

TEST(c2ast, attributes) {
    cleanTestEnv();
    string output = execTestCommand("palan-c2ast ../test/testdata/c2ast/047_attributes.h");
    json ast = json::parse(output);
    auto find = [&](const string& section, const string& name) -> json* {
        for (auto& e : ast["ast"][section])
            if (e["name"] == name) return &e;
        return nullptr;
    };

    json* p = find("structs", "P");
    ASSERT_NE(p, nullptr);
    ASSERT_EQ((*p)["packed"], true);
    ASSERT_EQ((*find("structs", "A"))["aligned"], 32);
    json& f = (*find("structs", "F"))["fields"];
    ASSERT_EQ(f[1]["aligned"], 8);
    ASSERT_EQ(f[2]["packed"], true);
    ASSERT_EQ((*find("structs", "S"))["fields"][0]["aligned"], 8);
    ASSERT_EQ((*find("structs", "U"))["packed"], true);
    ASSERT_EQ((*find("structs", "TP"))["packed"], true);
    ASSERT_EQ((*find("structs", "TB"))["aligned"], 16);

    // A typedef's own type change or alignment can't be carried over.
    ASSERT_EQ(find("structs", "TA"), nullptr);
    for (const char* name : {"TA", "u16m", "v4f", "ai8", "aibad", "both"})
        ASSERT_EQ(find("typedefs", name), nullptr) << name;

    // Expected values are what gcc gives on x86-64.
    auto expect_const = [&](const string& name, const string& value) {
        json* c = find("constants", name);
        ASSERT_NE(c, nullptr) << "expected " << name << " to be exported";
        ASSERT_EQ((*c)["value"], value) << "for " << name;
    };
    expect_const("ATTR_MACRO_DEFINED", "1");
    expect_const("SZ_P", "13");
    expect_const("SZ_A", "32");
    expect_const("SZ_F", "16");
    expect_const("SZ_TP", "9");
    expect_const("SZ_TB", "16");
    expect_const("SZ_S", "16");
    expect_const("SZ_U", "4");
    expect_const("AL_A", "32");
    expect_const("AL_P", "1");
    ASSERT_EQ(find("constants", "SZ_TA"), nullptr);

    for (const char* name : {"die", "pure_f", "cleanup_f"})
        ASSERT_NE(find("functions", name), nullptr) << name;
    ASSERT_EQ((*find("functions", "pure_f"))["parameters"][0].contains("attributes"), false);
    ASSERT_NE(find("globals", "aligned_var"), nullptr);
}

TEST(c2ast, sizeof_fd_set) {
    // fd_set's array size and __NFDBITS are both built from sizeof.
    cleanTestEnv();
    string output = execTestCommand("palan-c2ast -s sys/select.h");
    json ast = json::parse(output);
    json* fd_set = nullptr;
    for (auto& s : ast["ast"]["structs"])
        if (s["name"] == "fd_set") { fd_set = &s; break; }
    ASSERT_NE(fd_set, nullptr);
    ASSERT_EQ((*fd_set)["fields"][0]["var-type"]["size-expr"]["value"], "16");

    json* nfdbits = nullptr;
    for (auto& c : ast["ast"]["constants"])
        if (c["name"] == "__NFDBITS") { nfdbits = &c; break; }
    ASSERT_NE(nfdbits, nullptr);
    ASSERT_EQ((*nfdbits)["value"], "64");
    ASSERT_EQ((*nfdbits)["value-type"]["type-name"], "int32");
}

// A header whose typedefs are never referenced by any C function -- stdint.h
// declares zero functions, so without the dedicated ast.typedefs section
// none of its typedefs would reach the AST at all (the only other export
// path is piggybacking on a function/global's var-type node).
TEST(c2ast, stdint_typedefs) {
    cleanTestEnv();
    string output = execTestCommand("palan-c2ast -s stdint.h");
    json ast = json::parse(output);
    auto& typedefs = ast["ast"]["typedefs"];

    auto find_typedef = [&](const string& name) -> json* {
        for (auto& t : typedefs)
            if (t["name"] == name) return &t;
        return nullptr;
    };

    auto expect_prim = [&](const string& name, const string& prim) {
        json* t = find_typedef(name);
        ASSERT_NE(t, nullptr) << "expected " << name << " to be registered";
        ASSERT_EQ((*t)["var-type"]["type-kind"], "prim");
        ASSERT_EQ((*t)["var-type"]["type-name"], prim) << "for " << name;
    };

    expect_prim("int32_t", "int32");
    expect_prim("int64_t", "int64");
    expect_prim("uint32_t", "uint32");
    expect_prim("uint64_t", "uint64");
}

// `(void)` normalizes to an empty parameter list, both at
// top level and inside a function-pointer's own parameter list, while a real
// parameter and an already-empty `()` are left untouched.
TEST(c2ast, void_param_list) {
    cleanTestEnv();
    string output = execTestCommand("palan-c2ast ../test/testdata/c2ast/032_void_param_list.h");
    json ast = json::parse(output);
    auto& functions = ast["ast"]["functions"];

    auto find_func = [&](const string& name) -> json* {
        for (auto& f : functions)
            if (f["name"] == name) return &f;
        return nullptr;
    };

    json* f = find_func("f");
    ASSERT_NE(f, nullptr);
    ASSERT_TRUE((*f)["parameters"].empty());

    json* g = find_func("g");
    ASSERT_NE(g, nullptr);
    ASSERT_EQ((*g)["parameters"].size(), 1);
    ASSERT_EQ((*g)["parameters"][0]["name"], "a");

    json* h = find_func("h");
    ASSERT_NE(h, nullptr);
    ASSERT_TRUE((*h)["parameters"].empty());

    json* set_cb = find_func("set_cb");
    ASSERT_NE(set_cb, nullptr);
    auto& cb_vt = (*set_cb)["parameters"][0]["var-type"];
    ASSERT_EQ(cb_vt["type-kind"], "pntr");
    ASSERT_EQ(cb_vt["base-type"]["type-kind"], "func");
    ASSERT_TRUE(cb_vt["base-type"]["parameters"].empty());

    // No typedef in this header at all -- the "typedefs" key itself is
    // omitted, same convention as "structs".
    ASSERT_FALSE(ast["ast"].contains("typedefs"));
}

TEST(c2ast, bool_keyword) {
    // stdbool.h's `bool` is a macro for `_Bool`, so both spellings reach the same keyword.
    cleanTestEnv();
    string output = execTestCommand("palan-c2ast ../test/testdata/c2ast/039_bool.h");
    json ast = json::parse(output);
    auto& functions = ast["ast"]["functions"];

    auto find_func = [&](const string& name) -> json* {
        for (auto& f : functions)
            if (f["name"] == name) return &f;
        return nullptr;
    };
    json prim_bool = {{"type-kind", "prim"}, {"type-name", "bool"}};
    json const_bool = {{"type-kind", "prim"}, {"type-name", "bool"}, {"const", true}};

    json* f = find_func("f");
    ASSERT_NE(f, nullptr);
    ASSERT_EQ((*f)["parameters"][0]["var-type"], prim_bool);

    json* g = find_func("g");
    ASSERT_NE(g, nullptr);
    ASSERT_EQ((*g)["ret-type"], prim_bool);

    json* h = find_func("h");
    ASSERT_NE(h, nullptr);
    ASSERT_EQ((*h)["ret-type"], prim_bool);
    ASSERT_EQ((*h)["parameters"][0]["var-type"], const_bool);

    auto& structs = ast["ast"]["structs"];
    ASSERT_EQ(structs.size(), 1);
    ASSERT_EQ(structs[0]["fields"][0]["var-type"], prim_bool);
}

TEST(c2ast, struct_typedef_name) {
    cleanTestEnv();
    json ast = json::parse(execTestCommand("palan-c2ast ../test/testdata/c2ast/055_struct_typedef_name.h"));
    map<string, json> structs;
    for (auto& s : ast["ast"]["structs"]) structs[s["name"]] = s;
    // The first typedef written over the tag itself; not one through another typedef.
    ASSERT_EQ(structs["ta_s"]["typedef-name"], "ta_t");
    ASSERT_EQ(structs["tu_u"]["typedef-name"], "tu_t");
    ASSERT_FALSE(structs["tb_s"].contains("typedef-name"));
    ASSERT_FALSE(structs["tc_t"].contains("typedef-name"));
    // A reserved name gives way to the next one that is not, and is kept when there is none.
    ASSERT_EQ(structs["tr_s"]["typedef-name"], "tr_t");
    ASSERT_EQ(structs["tq_s"]["typedef-name"], "_Tq_t");
}

// GCC's alternate keywords and qualifier placements, as glibc and Linux headers
// write them; a function's body is skipped and a static one is not exported.
TEST(c2ast, gnu_spellings) {
    cleanTestEnv();
    json ast = json::parse(execTestCommand("palan-c2ast ../test/testdata/c2ast/056_gnu_spellings.h"));
    map<string, json> funcs;
    for (auto& f : ast["ast"]["functions"]) funcs[f["name"]] = f;
    auto param = [&](const string& f, int i) { return funcs[f]["parameters"][i]["var-type"]; };
    auto prim = [](const string& name) { return json{{"type-kind", "prim"}, {"type-name", name}}; };
    auto ptr = [](json base) { return json{{"type-kind", "pntr"}, {"base-type", base}}; };
    json constChar = prim("int8");
    constChar["const"] = true;
    ASSERT_EQ(param("f_const_after", 0), ptr(constChar));
    ASSERT_EQ(param("f_const_after", 1)["base-type"]["const"], true);
    ASSERT_EQ(param("f_unnamed_restrict", 0), ptr(constChar));
    ASSERT_EQ(param("f_unnamed_restrict", 1), ptr(prim("void")));
    ASSERT_EQ(param("f_arr_qualified", 0), ptr(ptr(prim("int8"))));
    ASSERT_EQ(param("f_arr_qualified", 1), ptr(prim("int32")));
    ASSERT_EQ(param("f_arr_qualified", 2), ptr(prim("int32")));
    ASSERT_EQ(funcs["f_signed"]["ret-type"], prim("int8"));
    ASSERT_EQ(param("f_signed", 0), prim("int16"));
    ASSERT_EQ(funcs["f_const_alt"]["ret-type"], ptr(constChar));
    ASSERT_EQ(funcs["f_complex"]["ret-type"], prim("double _Complex"));
    ASSERT_EQ(param("f_complex", 0), prim("float _Complex"));
    ASSERT_EQ(param("f_complex", 1), prim("double _Complex"));
    ASSERT_EQ(param("f_complex", 2), prim("long double _Complex"));
    ASSERT_EQ(funcs["f_int128"]["ret-type"], prim("unsigned __int128"));
    ASSERT_EQ(param("f_int128", 1), prim("__int128"));
    ASSERT_EQ(funcs.count("f_static_inline"), 0u);
    ASSERT_EQ(funcs["f_inline"]["ret-type"], prim("int32"));
    map<string, string> constants;
    for (auto& c : ast["ast"]["constants"]) constants[c["name"]] = c["value"];
    ASSERT_EQ(constants, (map<string, string>{{"sz_cf", "8"}, {"al_cf", "4"}, {"sz_cd", "16"},
        {"sz_cld", "32"}, {"al_cld", "16"}, {"sz_i128", "16"}}));
}

// --- Input file edge cases ---
