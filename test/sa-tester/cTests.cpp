/// semantic analyzer tests: C interop
///
/// @file cTests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include <filesystem>
#include "../test-base/testBase.h"
#include "saTestUtil.h"

using namespace std;

TEST(sa, variadic_arg_int16_promoted_to_int32) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/004_variadic_promotion.pa");
	ASSERT_TRUE(jout.is_object());

	bool found = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "expr") continue;
		auto& call = stmt["body"];
		if (call["expr-type"] != "call" || call["name"] != "printf") continue;
		// args[1] should be a convert node wrapping the id
		auto& arg1 = call["args"][1];
		ASSERT_EQ(arg1["expr-type"], "convert");
		ASSERT_EQ(arg1["value-type"]["type-name"], "int32");
		ASSERT_EQ(arg1["src"]["expr-type"], "id");
		found = true;
		break;
	}
	ASSERT_TRUE(found);
}

TEST(sa, c_func_return_type_as_value_type) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/005_c_func_return_type.pa");
	ASSERT_TRUE(jout.is_object());

	bool found = false;
	for (auto& stmt : jout["statements"]) {
		if (stmt["stmt-type"] != "var-decl") continue;
		for (auto& var : stmt["vars"]) {
			if (var["name"] != "x") continue;
			// init should be a convert node (int32 -> int64) wrapping the call
			auto& init = var["init"];
			ASSERT_EQ(init["expr-type"], "convert");
			ASSERT_EQ(init["value-type"]["type-name"], "int64");
			ASSERT_EQ(init["src"]["expr-type"], "call");
			ASSERT_EQ(init["src"]["value-type"]["type-name"], "int32");
			found = true;
		}
	}
	ASSERT_TRUE(found);
}

TEST(sa, block_cinclude_scope) {
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/020_block_cinclude_scope.pa");
	ASSERT_TRUE(jout.is_object());
	// block is emitted with body
	ASSERT_EQ(jout["statements"].size(), 1u);
	ASSERT_EQ(jout["statements"][0]["stmt-type"], "block");
	// printf inside block resolved as C function
	ASSERT_EQ(jout["statements"][0]["body"][0]["stmt-type"], "expr");
	ASSERT_EQ(jout["statements"][0]["body"][0]["body"]["func-type"], "c");
}

TEST(sa, cinclude_alias)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/063_cinclude_alias.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmt = jout["statements"][0];
	ASSERT_EQ(stmt["stmt-type"], "expr");
	const auto& call = stmt["body"];
	ASSERT_EQ(call["expr-type"], "call");
	ASSERT_EQ(call["func-type"], "c");
	ASSERT_EQ(call["name"], "printf");
}

TEST(sa, cinclude_typedef_size_t)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/122_cinclude_typedef_size_t.pa");
	ASSERT_TRUE(jout.is_object());

	// size_t resolved via the cinclude typedef bridge to a clean uint64,
	// with no leftover "typedef-name" bookkeeping key.
	const auto& decl = jout["statements"][0];
	ASSERT_EQ(decl["stmt-type"], "var-decl");
	const auto& v = decl["vars"][0];
	ASSERT_EQ(v["name"], "n");
	ASSERT_EQ(v["var-type"]["type-kind"], "prim");
	ASSERT_EQ(v["var-type"]["type-name"], "uint64");
	ASSERT_FALSE(v["var-type"].contains("typedef-name"));

	// strlen() call's value-type is likewise clean.
	ASSERT_EQ(v["init"]["expr-type"], "call");
	ASSERT_EQ(v["init"]["name"], "strlen");
	ASSERT_EQ(v["init"]["func-type"], "c");
	ASSERT_EQ(v["init"]["value-type"]["type-name"], "uint64");
	ASSERT_FALSE(v["init"]["value-type"].contains("typedef-name"));
}

TEST(sa, cinclude_typedefs)
{
	// A header exporting a typedef but zero C functions
	// (unlike 122_cinclude_typedef_size_t.pa's size_t, which rides in on
	// strlen()'s return type) must still have that typedef registered --
	// the general rule the dedicated ast.typedefs section implements, not
	// the old signature-piggyback path.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/182_cinclude_typedefs.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& decl = jout["statements"][0];
	ASSERT_EQ(decl["stmt-type"], "var-decl");
	const auto& v = decl["vars"][0];
	ASSERT_EQ(v["name"], "n");
	ASSERT_EQ(v["var-type"]["type-kind"], "prim");
	ASSERT_EQ(v["var-type"]["type-name"], "int32");
	ASSERT_FALSE(v["var-type"].contains("typedef-name"));
}

TEST(sa, cinclude_typedef_struct_file)
{
	// `typedef struct _IO_FILE FILE;` (stdio.h) --
	// registerTypedefAliasInType resolves a struct-bottomed typedef the
	// same way it already resolved a scalar one (size_t), so `@!FILE` reaches
	// pntr(struct(_IO_FILE)) instead of the unresolved "user" type-kind that
	// used to make PlnTypeRegistry::fromJson abort.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/161_c_typedef_struct.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& decl = jout["statements"][0];
	ASSERT_EQ(decl["stmt-type"], "var-decl");
	const auto& v = decl["vars"][0];
	ASSERT_EQ(v["name"], "f");
	ASSERT_EQ(v["var-type"]["type-kind"], "pntr");
	ASSERT_EQ(v["var-type"]["mutable"], true);
	ASSERT_EQ(v["var-type"]["base-type"]["type-kind"], "struct");
	ASSERT_EQ(v["var-type"]["base-type"]["type-name"], "_IO_FILE");
	ASSERT_FALSE(v["var-type"].contains("typedef-name"));

	// fopen()'s return value-type is likewise clean.
	ASSERT_EQ(v["init"]["name"], "fopen");
	ASSERT_EQ(v["init"]["value-type"]["base-type"]["type-name"], "_IO_FILE");
}

TEST(sa, cinclude_struct_arg)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/128_cinclude_struct_arg.pa");
	ASSERT_TRUE(jout.is_object());

	// Point (cinclude'd via a local header) resolves through the same
	// structDefs_/malloc path as a native `type Point { ... }` -- c2ast's
	// "structs" capture reaches SA and registers "Point" before the
	// var-decl below is analyzed.
	const auto& decl = jout["statements"][0];
	ASSERT_EQ(decl["stmt-type"], "var-decl");
	const auto& v = decl["vars"][0];
	ASSERT_EQ(v["name"], "p");
	ASSERT_EQ(v["var-type"]["type-kind"], "pntr");
	ASSERT_EQ(v["var-type"]["base-type"]["type-kind"], "struct");
	ASSERT_EQ(v["var-type"]["base-type"]["type-name"], "Point");
	ASSERT_EQ(v["init"]["name"], "__pln_malloc");
	ASSERT_EQ(v["init"]["args"][0]["value"], "16");

	// 10 -> p.x / 20 -> p.y resolve to the same field-assign shape as a
	// native struct (offsets 0 and 8 for two int64 fields).
	ASSERT_EQ(jout["statements"][1]["stmt-type"], "field-assign");
	ASSERT_EQ(jout["statements"][1]["var"], "p");
	ASSERT_EQ(jout["statements"][1]["offset"], 0);
	ASSERT_EQ(jout["statements"][2]["stmt-type"], "field-assign");
	ASSERT_EQ(jout["statements"][2]["var"], "p");
	ASSERT_EQ(jout["statements"][2]["offset"], 8);

	// move_point(p, 5, 5): p is passed as a plain borrowed pointer -- no
	// wrapping alloc/copy call is inserted around the "id" arg.
	const auto& call = jout["statements"][3]["body"];
	ASSERT_EQ(call["expr-type"], "call");
	ASSERT_EQ(call["name"], "move_point");
	ASSERT_EQ(call["func-type"], "c");
	const auto& arg0 = call["args"][0];
	ASSERT_EQ(arg0["expr-type"], "id");
	ASSERT_EQ(arg0["name"], "p");
	ASSERT_EQ(arg0["var-type"]["base-type"]["type-name"], "Point");
}

TEST(sa, cinclude_nested_struct)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/129_cinclude_nested_struct.pa");
	ASSERT_TRUE(jout.is_object());

	// itimerspec { struct timespec it_interval; struct timespec it_value; }:
	// a C struct-by-value field ("strct", not "pntr") is registered the same
	// way native $T inline-embedding is -- 2 * (int64 tv_sec + int64 tv_nsec)
	// = 32 bytes, no owned-pointer indirection.
	const auto& decl = jout["statements"][0];
	const auto& v = decl["vars"][0];
	ASSERT_EQ(v["name"], "it");
	ASSERT_EQ(v["init"]["name"], "__pln_malloc");
	ASSERT_EQ(v["init"]["args"][0]["value"], "32");

	// it.it_interval.tv_sec: it_interval@0, tv_sec@0 -> flattened offset 0
	ASSERT_EQ(jout["statements"][1]["stmt-type"], "field-assign");
	ASSERT_EQ(jout["statements"][1]["var"], "it");
	ASSERT_EQ(jout["statements"][1]["offset"], 0);

	// it.it_value.tv_nsec: it_value@16, tv_nsec@8 -> flattened offset 24
	ASSERT_EQ(jout["statements"][2]["stmt-type"], "field-assign");
	ASSERT_EQ(jout["statements"][2]["var"], "it");
	ASSERT_EQ(jout["statements"][2]["offset"], 24);
}

TEST(sa, cinclude_union)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/195_c_union.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& st = jout["statements"];

	// u_t = union U { char b[12]; long l; }: max member 12 rounded up to
	// the 8-byte alignment of l.
	ASSERT_EQ(st[0]["vars"][0]["init"]["args"][0]["value"], "16");
	// struct S { int k; union U u; }: u aligned to 8 after k.
	ASSERT_EQ(st[1]["vars"][0]["init"]["args"][0]["value"], "24");
	// union W { struct Pt { int x; int y; } p; long l; }
	ASSERT_EQ(st[2]["vars"][0]["init"]["args"][0]["value"], "8");

	ASSERT_EQ(st[3]["stmt-type"], "field-assign");
	ASSERT_EQ(st[3]["offset"], 0);   // u.l
	ASSERT_EQ(st[4]["offset"], 8);   // s.u.l
	ASSERT_EQ(st[5]["offset"], 4);   // w.p.y
	ASSERT_EQ(st[6]["vars"][0]["init"]["array"]["offset"], 0);  // u.b[0]

	// @U / @!u_t borrow the union as a plain struct pointer, and the
	// mutable one passes to take(u_t *).
	ASSERT_EQ(st[7]["vars"][0]["var-type"]["base-type"]["type-name"], "U");
	ASSERT_EQ(st[8]["vars"][0]["var-type"]["base-type"]["type-name"], "U");
	const auto& call = st[9]["body"];
	ASSERT_EQ(call["name"], "take");
	ASSERT_EQ(call["args"][0]["name"], "m");
}

TEST(sa, cinclude_anon_member)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/273_c_anon_member.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& st = jout["statements"];

	// struct S { char k; union { int a; double d; struct { short lo; short hi; }; }; char t; }
	ASSERT_EQ(st[0]["vars"][0]["init"]["args"][0]["value"], "24");
	ASSERT_EQ(st[1]["offset"], 0);   // s.k
	ASSERT_EQ(st[2]["offset"], 8);   // s.a
	ASSERT_EQ(st[3]["offset"], 10);  // s.hi
	ASSERT_EQ(st[4]["offset"], 16);  // s.t
	ASSERT_EQ(st[5]["vars"][0]["init"]["offset"], 8);  // read s.a
}

TEST(sa, cinclude_null_constant)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/127_cinclude_null_compat.pa");
	ASSERT_TRUE(jout.is_object());

	// NULL from cinclude <string.h> (via stddef.h) is folded by gen-ast into a
	// typed lit-int 0 with pntr(void) value-type, which typeCompat's
	// pntr(void) rule accepts against strchr()'s pntr(int8) return.
	const auto& ifStmt = jout["statements"][1];
	ASSERT_EQ(ifStmt["stmt-type"], "if");
	const auto& cond = ifStmt["cond"];
	ASSERT_EQ(cond["expr-type"], "cmp");
	ASSERT_EQ(cond["op"], "==");

	const auto& left = cond["left"];
	ASSERT_EQ(left["expr-type"], "call");
	ASSERT_EQ(left["name"], "strchr");
	ASSERT_EQ(left["func-type"], "c");
	ASSERT_EQ(left["value-type"]["type-kind"], "pntr");

	const auto& right = cond["right"];
	ASSERT_EQ(right["expr-type"], "lit-int");
	ASSERT_EQ(right["value"], "0");
	ASSERT_EQ(right["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(right["value-type"]["base-type"]["type-kind"], "prim");
	ASSERT_EQ(right["value-type"]["base-type"]["type-name"], "void");
}

TEST(sa, macro_binop_typing)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/192_macro_binop_typing.pa");
	ASSERT_TRUE(jout.is_object());

	// A macro constant whose type C fixed (S_IFDIR cast to int) keeps it as
	// its own type in an "x + S_IFDIR" binary op -- not silently re-typed to
	// match the other operand -- and reconciled by the normal usual
	// arithmetic conversion (widened to x's int64 via an explicit convert
	// node), same as any other differently-typed operand pair would be.
	const auto& conv = jout["functions"][0]["body"][1]["values"][0];
	ASSERT_EQ(conv["expr-type"], "convert");
	const auto& add = conv["src"];
	ASSERT_EQ(add["expr-type"], "add");
	ASSERT_EQ(add["left"]["name"], "x");
	ASSERT_EQ(add["left"]["value-type"]["type-name"], "int64");

	const auto& right = add["right"];
	ASSERT_EQ(right["expr-type"], "convert");
	ASSERT_EQ(right["from-type"]["type-name"], "int32");
	ASSERT_EQ(right["src"]["expr-type"], "lit-int");
	ASSERT_EQ(right["src"]["value"], "16384");
	ASSERT_EQ(right["src"]["value-type"]["type-name"], "int32");
	ASSERT_EQ(right["value-type"]["type-name"], "int64");
}

TEST(sa, macro_array_size)
{
	// A macro constant used as an array size whose type C fixed (ARR_N cast
	// to int) keeps it through gen-ast's fold, unlike a plain source literal
	// array size, which sa_expression always forces to uint64. sa_arr_size_expr
	// must explicitly widen it to uint64 (an int32 "mul" operand, sized to
	// its own width by codegen, mismatches the surrounding uint64 byte-count
	// math at the assembler otherwise).
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/194_macro_array_size.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& mallocArg = jout["statements"][0]["vars"][0]["init"]["args"][0];
	ASSERT_EQ(mallocArg["expr-type"], "mul");
	ASSERT_EQ(mallocArg["value-type"]["type-name"], "uint64");

	const auto& left = mallocArg["left"];
	ASSERT_EQ(left["expr-type"], "convert");
	ASSERT_EQ(left["value-type"]["type-name"], "uint64");
	ASSERT_EQ(left["from-type"]["type-name"], "int32");
	ASSERT_EQ(left["src"]["expr-type"], "lit-int");
	ASSERT_EQ(left["src"]["value"], "3");
	ASSERT_EQ(left["src"]["value-type"]["type-name"], "int32");
}

TEST(sa, variadic_flo32_promote)
{
	// printf("%f\n", x) where x is flo32 → variadicPromote(flo32) = flo64
	// Covers: variadicPromote Float32 branch
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/094_variadic_flo32.pa");
	ASSERT_TRUE(jout.is_object());

	// Find printf call
	json* call = nullptr;
	for (auto& s : jout["statements"])
		if (s["stmt-type"] == "expr" && s["body"]["name"] == "printf")
			{ call = &s; break; }
	ASSERT_NE(call, nullptr);

	// arg[1] (the flo32 var) should be wrapped in convert to flo64
	const auto& arg = (*call)["body"]["args"][1];
	ASSERT_EQ(arg["expr-type"],                  "convert");
	ASSERT_EQ(arg["from-type"]["type-name"],     "flo32");
	ASSERT_EQ(arg["value-type"]["type-name"],    "flo64");
}

TEST(sa, variadic_uint8_uint16_promote)
{
	// printf with uint8 and uint16 args → variadicPromote to uint32
	// Covers: variadicPromote Uint8+Uint16 branch
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/095_variadic_uint8_uint16.pa");
	ASSERT_TRUE(jout.is_object());

	json* call = nullptr;
	for (auto& s : jout["statements"])
		if (s["stmt-type"] == "expr" && s["body"]["name"] == "printf")
			{ call = &s; break; }
	ASSERT_NE(call, nullptr);

	// arg[1] = uint8 a → convert to uint32
	const auto& arg1 = (*call)["body"]["args"][1];
	ASSERT_EQ(arg1["expr-type"],               "convert");
	ASSERT_EQ(arg1["from-type"]["type-name"],  "uint8");
	ASSERT_EQ(arg1["value-type"]["type-name"], "uint32");

	// arg[2] = uint16 b → convert to uint32
	const auto& arg2 = (*call)["body"]["args"][2];
	ASSERT_EQ(arg2["expr-type"],               "convert");
	ASSERT_EQ(arg2["from-type"]["type-name"],  "uint16");
	ASSERT_EQ(arg2["value-type"]["type-name"], "uint32");
}

TEST(sa, cinclude_arr_field_prim)
{
	// struct BufField { char name[16]; }; (cinclude'd) -- c2ast reflects
	// C array declarators as an "arr" var-type instead of discarding them, so this
	// field reaches buildStructDef's embed-arr/prim-leaf case exactly like a native
	// `[16]$int8 name;` field would.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/138_cinclude_arr_field_prim.pa");
	ASSERT_TRUE(jout.is_object());

	// BufField b; -> malloc(16): confirms totalSize == 16 (was 8 before the
	// fix above, since "char name[16]" collapsed to a single int8 field).
	const auto& v = jout["statements"][0]["vars"][0];
	ASSERT_EQ(v["name"], "b");
	ASSERT_EQ(v["init"]["name"], "__pln_malloc");
	ASSERT_EQ(v["init"]["args"][0]["value"], "16");
}

TEST(sa, cinclude_arr_field_struct)
{
	// struct Point { int x; int y; }; struct Poly { struct Point pts[3]; }; (cinclude'd)
	// -- struct-leaf array field. cFieldVarType normalizes the "strct" leaf
	// inside the "arr" base-type to "prim" so buildStructDef's structDefs_ lookup
	// (by type-name, not type-kind) finds "Point" and treats it as an embed-arr
	// struct leaf, same as a native `[3]$Point pts;` field.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/139_cinclude_arr_field_struct.pa");
	ASSERT_TRUE(jout.is_object());

	// Poly p; -> malloc(24): confirms totalSize == 3 * Point.totalSize(8) == 24.
	const auto& v = jout["statements"][0]["vars"][0];
	ASSERT_EQ(v["name"], "p");
	ASSERT_EQ(v["init"]["name"], "__pln_malloc");
	ASSERT_EQ(v["init"]["args"][0]["value"], "24");
}

TEST(sa, cinclude_ptr_slot_arr_field)
{
	// struct Point { int x; int y; }; struct Slots { struct Point *pts[4]; long *vals[3]; };
	// (cinclude'd) -- a C struct field
	// that is an inline array of pointer slots ("T *field[n];") is Palan's
	// [n]@T / [n]@!T shape. Depends on a c2ast declarator-precedence fix:
	// before that fix, c2ast parsed "T *field[n]" inverted as a single pointer
	// (pntr(arr(...))), so this field silently registered at the wrong (too small)
	// size instead of reaching this shape at all.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/156_cinclude_ptr_slot_arr_field.pa");
	ASSERT_TRUE(jout.is_object());

	// Slots s; -> malloc(56): confirms totalSize == 4*8 (pts) + 3*8 (vals) == 56,
	// not 16 (two 8-byte pointers, the pre-fix misparse).
	const auto& v = jout["statements"][0]["vars"][0];
	ASSERT_EQ(v["name"], "s");
	ASSERT_EQ(v["init"]["name"], "__pln_malloc");
	ASSERT_EQ(v["init"]["args"][0]["value"], "56");
}

TEST(sa, cinclude_struct_stat_size)
{
	// cinclude <sys/stat.h>; stat s; -- the memory-safety validation target for
	// the C-array-field fix above: struct stat's trailing
	// "__syscall_slong_t __glibc_reserved[3]" (and
	// other array fields) used to collapse to single scalars, so Palan's computed
	// size was 128 instead of the real 144, meaning a Palan-allocated struct stat
	// passed to stat(2) would overflow by 16 bytes.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/140_cinclude_struct_stat_size.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& v = jout["statements"][0]["vars"][0];
	ASSERT_EQ(v["name"], "s");
	ASSERT_EQ(v["init"]["name"], "__pln_malloc");
	ASSERT_EQ(v["init"]["args"][0]["value"], "144");
}

TEST(sa, cinclude_pthread_union_size)
{
	// Expected sizes are gcc's sizeof on x86-64 glibc. pthread_cond_t
	// reaches a union nested in a struct whose member is an anonymous struct.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/196_cinclude_pthread_union_size.pa");
	ASSERT_TRUE(jout.is_object());

	const char* sizes[] = {"40", "48", "56", "4", "4", "56", "8", "32", "4"};
	const auto& st = jout["statements"];
	ASSERT_EQ(st.size(), size(sizes));
	for (size_t i = 0; i < size(sizes); i++) {
		const auto& v = st[i]["vars"][0];
		ASSERT_EQ(v["init"]["name"], "__pln_malloc") << v["name"];
		ASSERT_EQ(v["init"]["args"][0]["value"], sizes[i]) << v["name"];
	}
}

TEST(sa, cinclude_types_prescan)
{
	// Native struct defs and function signatures are pre-scanned before
	// top-level statements run, so cinclude types must be registered then too.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/197_cinclude_types_prescan.pa");
	ASSERT_TRUE(jout.is_object());

	ASSERT_EQ(jout["statements"][0]["vars"][0]["init"]["args"][0]["value"], "24");
	const auto& f = jout["functions"][0];
	ASSERT_EQ(f["parameters"][0]["var-type"]["base-type"]["type-name"], "timespec");
	ASSERT_EQ(f["parameters"][1]["var-type"]["type-name"], "int64");
	ASSERT_EQ(f["ret-type"]["type-name"], "int64");
}

TEST(sa, c_const_param_readonly_arg)
{
	// `ctime(@t)` -- C function arguments are
	// checked by ptrPermissionOk() too, so this must keep compiling: ctime's
	// `const time_t *` parameter normalizes (at the cinclude ingestion boundary,
	// PlnSaInternal.h normalizeCType) to a mutable:false pntr, matching `@t`'s
	// own mutable:false. Regression guard for every existing `@x`-into-const-C-arg
	// call site (ctime/gmtime/localtime/... in the time.h build-mgr tests).
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/152_c_const_param_readonly_arg.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& init = jout["statements"][1]["vars"][0]["init"];
	ASSERT_EQ(init["name"], "ctime");
	ASSERT_EQ(init["args"][0]["value-type"]["mutable"], false);
}

TEST(sa, incomplete_struct_ptr)
{
	// struct Tag { int x; int cells[]; }; (cinclude'd) -- "cells" is an
	// array field without a length, which buildStructDef can't lay out.
	// isSupportedCFieldType
	// downgrades the whole tag to an incomplete struct (opaque handle, C
	// incomplete-type equivalent) instead of leaving it unregistered, so `Tag`
	// is still a known type name -- unusable for a sized declaration, but usable
	// through a non-owning pointer with no layout needed, so `@!Tag p;` declares
	// successfully and no alloc-shape is emitted for it.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/159_incomplete_struct_ptr.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& p = jout["statements"][0]["vars"][0];
	ASSERT_EQ(p["name"], "p");
	ASSERT_EQ(p["var-type"]["type-kind"], "pntr");
	ASSERT_EQ(p["var-type"]["base-type"]["type-kind"], "struct");
	ASSERT_EQ(p["var-type"]["base-type"]["type-name"], "Tag");
	ASSERT_TRUE(jout["alloc-shapes"].empty());
}

TEST(sa, c_unsupported_sig_unused)
{
	// The header declares `union { int i; } make_val(void);` (unsupported --
	// a nameless union return type) alongside `int add(int a, int b);` (supported).
	// normalizeCFuncSig tags the unsupported entry with "_unsupported-sig" at
	// cinclude time but does not reject registration; only calling
	// requireSupportedCFuncSig's guarded function fails. Calling only `add`
	// must compile cleanly -- an unused unsupported C signature is inert, the
	// same policy union/enum types already got.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/162_c_unsupported_sig_unused.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& call = jout["statements"][0]["vars"][0]["init"];
	ASSERT_EQ(call["expr-type"], "call");
	ASSERT_EQ(call["name"], "add");
}

TEST(sa, c_widen_arg)
{
	// `take64(a)` where `a` is int32 and the C parameter is `long` (int64).
	// sa_expr_call's parameter-side fromJson call used to be guarded
	// by `catch (const std::runtime_error&) {}`; removing that guard must not
	// regress the implicit-widening path it happened to share code with --
	// the argument must still be wrapped in a "convert" node.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/163_c_widen_arg.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& call = jout["statements"][1]["vars"][0]["init"];
	ASSERT_EQ(call["expr-type"], "call");
	const auto& arg = call["args"][0];
	ASSERT_EQ(arg["expr-type"], "convert");
	ASSERT_EQ(arg["value-type"]["type-name"], "int64");
	ASSERT_EQ(arg["src"]["value-type"]["type-name"], "int32");
}

TEST(sa, c_global)
{
	// `stderr` referenced as `fprintf`'s first argument is a plain "id"; the
	// top level binds the name to the C symbol's label in `main.globals`.
	// Covers: lookupVar findCGlobal fallback, bindStatic
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/164_c_global.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& call = jout["statements"][0]["body"];
	ASSERT_EQ(call["expr-type"], "call");
	ASSERT_EQ(call["func-type"], "c");
	ASSERT_EQ(call["name"], "fprintf");
	const auto& arg = call["args"][0];
	ASSERT_EQ(arg["expr-type"], "id");
	ASSERT_EQ(arg["name"], "stderr");
	ASSERT_EQ(jout["main"]["globals"].size(), 1);
	ASSERT_EQ(jout["main"]["globals"][0]["name"], "stderr");
	ASSERT_EQ(jout["main"]["globals"][0]["label"], "stderr");
	ASSERT_EQ(arg["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(arg["value-type"]["mutable"], true);
	ASSERT_EQ(arg["value-type"]["base-type"]["type-kind"], "struct");
	ASSERT_EQ(arg["value-type"]["base-type"]["type-name"], "_IO_FILE");
	ASSERT_FALSE(arg["value-type"].contains("typedef-name"));
}

TEST(sa, c_global_alias)
{
	// `cinclude <stdio.h> as S;` still registers globals unqualified, same as
	// constants/typedefs -- only functions require the S. qualifier.
	// Covers: sa_cinclude globals loop, alias-independent registration
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/165_c_global_alias.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& call = jout["statements"][0]["body"];
	ASSERT_EQ(call["name"], "fprintf");
	ASSERT_EQ(call["args"][0]["name"], "stderr");
	ASSERT_EQ(jout["main"]["globals"][0]["label"], "stderr");
}

TEST(sa, c_global_block_scope)
{
	// A C global from a block-scoped cinclude is visible within that block,
	// mirroring the existing block_cinclude_scope test for C functions.
	// Covers: enterScope/leaveScope cGlobalScopes push/pop
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/166_c_global_block_scope.pa");
	ASSERT_TRUE(jout.is_object());

	ASSERT_EQ(jout["statements"][0]["stmt-type"], "block");
	const auto& call = jout["statements"][0]["body"][0]["body"];
	ASSERT_EQ(call["func-type"], "c");
	ASSERT_EQ(call["args"][0]["name"], "stderr");
	ASSERT_EQ(jout["main"]["globals"][0]["label"], "stderr");
}

TEST(sa, neg_lit_expected_type)
{
	// `neg` did not propagate expectedType
	// to its operand (unlike the adjacent `bitnot`), so a negated literal in a
	// narrower/float initializer always adopted the default int64/flo64 and then
	// tripped the narrowing-initializer diagnostic -- e.g. `int32 a = -1;` was
	// rejected even though `int32 a = 1;` and `int32 a = -a;` (a variable) both work.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/167_neg_lit_expected_type.pa");
	ASSERT_TRUE(jout.is_object());

	auto& a = jout["statements"][0]["vars"][0];
	ASSERT_EQ(a["init"]["expr-type"], "lit-int");
	ASSERT_EQ(a["init"]["value"], "-1");
	ASSERT_EQ(a["init"]["value-type"]["type-name"], "int32");

	auto& f = jout["statements"][1]["vars"][0];
	ASSERT_EQ(f["init"]["expr-type"], "lit-flo");
	ASSERT_EQ(f["init"]["value"], "-1.5");
	ASSERT_EQ(f["init"]["value-type"]["type-name"], "flo32");

	auto& s = jout["statements"][3]["vars"][0];
	ASSERT_EQ(s["init"]["value-type"]["type-name"], "int16");
}

TEST(sa, usual_arith_conv)
{
	// typeCompat's ExplicitCast for a mixed
	// signed/unsigned operand pair used to be silently ignored by sa_expr_arith,
	// cmp, and call arguments -- the codegen result then had two different
	// register widths in one instruction. This pins the usual-arithmetic-
	// conversion rule that replaced the silent pass-through.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/168_usual_arith_conv.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	// [0..3] var-decls (uint32 m, int32 n, int64 big, uint32 mode)
	// [4] m & n, [5] n & m, [6] big & mode, [7] mode & big, [8] n < m,
	// [9] take_u32(n), [10] take_i64(mode)
	ASSERT_GE(stmts.size(), 11u);

	// uint32 & int32 -> uint32 (equal rank, mixed sign: unsigned wins), and the
	// result type is order-independent regardless of which side is converted.
	const auto& m_and_n = stmts[4]["body"];
	ASSERT_EQ(m_and_n["value-type"]["type-name"], "uint32");
	ASSERT_EQ(m_and_n["left"]["expr-type"],       "id");
	ASSERT_EQ(m_and_n["right"]["expr-type"],      "convert");
	ASSERT_EQ(m_and_n["right"]["value-type"]["type-name"], "uint32");

	const auto& n_and_m = stmts[5]["body"];
	ASSERT_EQ(n_and_m["value-type"]["type-name"], "uint32");
	ASSERT_EQ(n_and_m["left"]["expr-type"],       "convert");
	ASSERT_EQ(n_and_m["right"]["expr-type"],      "id");

	// int64 & uint32 -> int64 (mixed sign, signed rank(4) > unsigned rank(3):
	// signed wins), order-independent.
	const auto& big_and_mode = stmts[6]["body"];
	ASSERT_EQ(big_and_mode["value-type"]["type-name"], "int64");
	ASSERT_EQ(big_and_mode["right"]["expr-type"],      "convert");

	const auto& mode_and_big = stmts[7]["body"];
	ASSERT_EQ(mode_and_big["value-type"]["type-name"], "int64");
	ASSERT_EQ(mode_and_big["left"]["expr-type"],       "convert");

	// n < m (int32 < uint32) -> both promote to uint32; cmp's own value-type
	// (the boolean result) stays bool regardless of the operand promotion.
	const auto& cmp = stmts[8]["body"];
	ASSERT_EQ(cmp["value-type"]["type-name"], "bool");
	ASSERT_EQ(cmp["left"]["expr-type"],       "convert");
	ASSERT_EQ(cmp["left"]["value-type"]["type-name"], "uint32");

	// take_u32(n): int32 arg -> uint32 param, same-width sign reinterpretation
	// (argConvOk-only case, not a usualArithConv widen) -- allowed without an
	// explicit cast at a call site, unlike at a binding site.
	const auto& call_u32 = stmts[9]["body"];
	ASSERT_EQ(funcName(call_u32["name"]),                        "take_u32");
	ASSERT_EQ(call_u32["args"][0]["expr-type"],        "convert");
	ASSERT_EQ(call_u32["args"][0]["value-type"]["type-name"], "uint32");

	// take_i64(mode): uint32 arg -> int64 param, a genuine usualArithConv widen.
	const auto& call_i64 = stmts[10]["body"];
	ASSERT_EQ(funcName(call_i64["name"]),                        "take_i64");
	ASSERT_EQ(call_i64["args"][0]["expr-type"],        "convert");
	ASSERT_EQ(call_i64["args"][0]["value-type"]["type-name"], "int64");
}

TEST(sa, struct_ret_c_call) {
	// A C function's by-value struct return is normalized to a Palan one:
	// `div_t d = div(7, 2);` allocates d, then the call writes into it
	// (`ret-dest`) and has no value of its own.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/171_struct_ret_c_call.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts.size(), 2);

	const auto& decl = stmts[0];
	ASSERT_EQ(decl["stmt-type"], "var-decl");
	const auto& v = decl["vars"][0];
	ASSERT_EQ(v["name"], "d");
	ASSERT_EQ(v["init"]["name"], "__pln_malloc");

	const auto& call = stmts[1]["body"];
	ASSERT_EQ(stmts[1]["stmt-type"], "expr");
	ASSERT_EQ(call["expr-type"], "call");
	ASSERT_EQ(call["func-type"], "c");
	ASSERT_EQ(call["name"], "div");
	ASSERT_FALSE(call.contains("value-type"));
	ASSERT_EQ(call["ret-dest"]["name"], "d");

	const auto& sr = call["struct-ret"];
	ASSERT_EQ(sr["size"], 8);
	ASSERT_EQ(sr["eightbytes"].size(), 1);
	ASSERT_EQ(sr["eightbytes"][0]["class"], "int");
	ASSERT_EQ(sr["eightbytes"][0]["size"], 8);
}

TEST(sa, struct_ret_memory_class) {
	// MEMORY class (>16 bytes) has no eightbytes; codegen passes `ret-dest`
	// as the hidden first argument, so SA leaves `args` as written.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/172_struct_ret_memory_class.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts.size(), 2);

	const auto& call = stmts[1]["body"];
	ASSERT_EQ(call["name"], "get_big");
	ASSERT_FALSE(call.contains("value-type"));
	ASSERT_EQ(call["ret-dest"]["name"], "g");

	const auto& sr = call["struct-ret"];
	ASSERT_EQ(sr["size"], 24);
	ASSERT_TRUE(sr["eightbytes"].empty());
	ASSERT_EQ(call["args"].size(), 0);
}

TEST(sa, struct_ret_ptr_field) {
	// classifySysVWalk's raw-ptr/owned branch (PlnSaInternal.h)
	// was untested: every other struct-ret fixture uses all-prim fields.
	// `{int tag; char *name;}` is 16 bytes (tag padded to the pointer's
	// 8-byte alignment), so both eightbytes stay INTEGER via that branch.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/177_struct_ret_ptr_field.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& call = jout["statements"][1]["body"];
	ASSERT_EQ(call["name"], "make_with_ptr");
	const auto& sr = call["struct-ret"];
	ASSERT_EQ(sr["size"], 16);
	ASSERT_EQ(sr["eightbytes"].size(), 2);
	ASSERT_EQ(sr["eightbytes"][0]["class"], "int");
	ASSERT_EQ(sr["eightbytes"][1]["class"], "int");
}

TEST(sa, struct_ret_embed_and_slots_field) {
	// classifySysVWalk's embed and embed-ptr-arr branches were untested.
	// `{struct Point origin; void *slots[1];}` embeds a by-value struct
	// field (recurses into Point's own fields) and a one-slot pointer
	// array field, filling the two eightbytes with INTEGER via each
	// respective branch.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/178_struct_ret_embed_and_slots_field.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& call = jout["statements"][1]["body"];
	ASSERT_EQ(call["name"], "make_combo");
	const auto& sr = call["struct-ret"];
	ASSERT_EQ(sr["size"], 16);
	ASSERT_EQ(sr["eightbytes"].size(), 2);
	ASSERT_EQ(sr["eightbytes"][0]["class"], "int");
	ASSERT_EQ(sr["eightbytes"][1]["class"], "int");
}

TEST(sa, struct_ret_embed_arr_field) {
	// classifySysVWalk's embed-arr branch was untested for both of its leaf
	// kinds. `{struct Pair1 items[2]; short more[4];}` has an array-of-struct
	// field (recurses into Pair1 per element) and an array-of-prim field,
	// filling the two eightbytes with INTEGER via each leaf kind.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/179_struct_ret_embed_arr_field.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& call = jout["statements"][1]["body"];
	ASSERT_EQ(call["name"], "make_mixed");
	const auto& sr = call["struct-ret"];
	ASSERT_EQ(sr["size"], 16);
	ASSERT_EQ(sr["eightbytes"].size(), 2);
	ASSERT_EQ(sr["eightbytes"][0]["class"], "int");
	ASSERT_EQ(sr["eightbytes"][1]["class"], "int");
}

TEST(sa, struct_ret_sse_field) {
	// classifySysVWalk's SSE classification was untested: every other
	// struct-ret fixture (div_t, WithPtr, Combo, Mixed) uses only integer
	// fields, so `mark()`'s "fieldCls != Integer -> return" early-out and
	// the isFloat==true arm of the prim-field ternary never ran.
	// `{double v;}` is a single flo64 field -> one SSE eightbyte.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/180_struct_ret_sse_field.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& call = jout["statements"][1]["body"];
	ASSERT_EQ(call["name"], "get_flo");
	const auto& sr = call["struct-ret"];
	ASSERT_EQ(sr["size"], 8);
	ASSERT_EQ(sr["eightbytes"].size(), 1);
	ASSERT_EQ(sr["eightbytes"][0]["class"], "sse");
	ASSERT_EQ(sr["eightbytes"][0]["flo64"], true);
}

TEST(sa, struct_ret_embed_arr_sse_field) {
	// classifySysVWalk's embed-arr leafFloat==true arm was untested (the
	// struct_ret_embed_arr_field fixture's array field is int-only).
	// `{float vals[4];}` is a 16-byte array-of-flo32 field -> both
	// eightbytes SSE.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/181_struct_ret_embed_arr_sse_field.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& call = jout["statements"][1]["body"];
	ASSERT_EQ(call["name"], "get_flo_arr");
	const auto& sr = call["struct-ret"];
	ASSERT_EQ(sr["size"], 16);
	ASSERT_EQ(sr["eightbytes"].size(), 2);
	ASSERT_EQ(sr["eightbytes"][0]["class"], "sse");
	ASSERT_EQ(sr["eightbytes"][1]["class"], "sse");
	ASSERT_FALSE(sr["eightbytes"][0].contains("flo64"));
	ASSERT_FALSE(sr["eightbytes"][1].contains("flo64"));
}

TEST(sa, c_callback_arg) {
	// `cmp` takes `@int32`s where qsort's comparator takes `const void*`:
	// a void pointer matches any pointer in a function type too.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/173_c_callback_arg.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& call = jout["statements"][1]["body"];
	ASSERT_EQ(call["name"], "qsort");
	ASSERT_EQ(call["args"].size(), 4);

	const auto& cb = call["args"][3];
	ASSERT_EQ(cb["expr-type"], "func-ref");
	ASSERT_EQ(funcName(cb["name"]), "cmp");
	ASSERT_EQ(cb["value-type"]["mutable"], false);
	const auto& fn = cb["value-type"]["base-type"];
	ASSERT_EQ(fn["type-kind"], "func");
	ASSERT_EQ(fn["parameters"][0]["var-type"]["base-type"]["type-name"], "int32");
	ASSERT_EQ(fn["ret-type"]["type-name"], "int32");
}

TEST(sa, c_callback_void) {
	// `atexit(on_exit_cb);`: a function returning nothing matches a void
	// C function pointer.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/174_c_callback_void.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& call = jout["statements"][0]["body"];
	ASSERT_EQ(call["name"], "atexit");
	ASSERT_EQ(call["args"][0]["expr-type"], "func-ref");
	ASSERT_EQ(funcName(call["args"][0]["name"]), "on_exit_cb");
}

TEST(sa, link_libs) {
	// The `link` clause on the cinclude AST
	// node is collected by SA into the sa.json root "libs" section. The
	// cinclude statement itself is still consumed, as it always was.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/183_link_libs.pa");
	ASSERT_TRUE(jout.is_object());

	ASSERT_TRUE(jout.contains("libs"));
	ASSERT_EQ(jout["libs"].size(), 1u);
	ASSERT_EQ(jout["libs"][0], "m");

	for (auto& stmt : jout["statements"])
		ASSERT_NE(stmt["stmt-type"], "cinclude");
}

TEST(sa, link_libs_dedup) {
	// Two cinclude statements naming the same library yield one entry:
	// linkLibs_ is a set, so build-mgr never sees "-lm -lm".
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/184_link_libs_dedup.pa");
	ASSERT_TRUE(jout.is_object());
	ASSERT_EQ(jout["libs"].size(), 1u);
	ASSERT_EQ(jout["libs"][0], "m");
}

TEST(sa, link_libs_sorted) {
	// `link "rt", "m"` is written in that order; the output is sorted, so
	// sa.json is deterministic regardless of source order.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/185_link_libs_sort.pa");
	ASSERT_TRUE(jout.is_object());
	ASSERT_EQ(jout["libs"].size(), 2u);
	ASSERT_EQ(jout["libs"][0], "m");
	ASSERT_EQ(jout["libs"][1], "rt");
}

TEST(sa, link_objs) {
	// The same object file named twice, once inside a block, is linked once.
	cleanTestEnv();
	system("cp ../test/testdata/sa/270_link_obj.pa out/ && touch out/270_link_obj.o");
	json jout = run_sa("out/270_link_obj.pa");
	ASSERT_TRUE(jout.is_object());
	string obj = filesystem::absolute("out/270_link_obj.o").lexically_normal().string();
	ASSERT_EQ(jout["objs"], json::array({obj}));
	ASSERT_EQ(jout["libs"], json::array({"m"}));
}

TEST(sa, c_struct_arg) {
	// A by-value struct argument is marked with its SysV classification; the
	// argument itself stays a pointer to the struct to copy.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/271_c_struct_arg.pa");
	ASSERT_TRUE(jout.is_object());
	vector<json> calls;
	for (auto& s : jout["statements"])
		if (s["stmt-type"] == "expr") calls.push_back(s["body"]);
	ASSERT_EQ(calls.size(), 6u);

	json i8 = {{"size", 8}, {"align", 4}, {"eightbytes", json::array({{{"class", "int"}, {"size", 8}}})}};
	ASSERT_EQ(calls[0]["args"][0]["expr-type"], "id");
	ASSERT_EQ(calls[0]["args"][0]["struct-arg"], i8);
	ASSERT_EQ(calls[1]["args"][0]["struct-arg"], json({{"size", 24}, {"align", 8}, {"eightbytes", json::array()}}));
	ASSERT_EQ(calls[2]["args"][0]["struct-arg"],
		json({{"size", 3}, {"align", 1}, {"eightbytes", json::array({{{"class", "int"}, {"size", 3}}})}}));
	// A literal and a returned struct are held in a temp, which carries the mark.
	ASSERT_EQ(calls[3]["args"][0]["expr-type"], "temp");
	ASSERT_EQ(calls[3]["args"][0]["struct-arg"], i8);
	ASSERT_EQ(calls[4]["args"][0]["expr-type"], "temp");
	ASSERT_EQ(calls[4]["args"][0]["struct-arg"], i8);
	// A flo80 makes it MEMORY class, laid on the stack at its 16-byte alignment.
	ASSERT_EQ(calls[5]["args"][0]["struct-arg"], json({{"size", 32}, {"align", 16}, {"eightbytes", json::array()}}));
}

TEST(sa, c_struct_arg_read_only) {
	// The callee gets a copy, so a read-only pointer is accepted.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/272_c_struct_arg_borrow.pa");
	ASSERT_TRUE(jout.is_object());
	const json& call = jout["statements"].back()["body"];
	ASSERT_EQ(call["name"], "take_i8");
	ASSERT_TRUE(call["args"][0].contains("struct-arg"));
}

TEST(sa, link_libs_absent) {
	// "libs" is a required root section: present and empty when no cinclude
	// in the module carries a `link` clause, exactly like "str-literals",
	// "functions" and "alloc-shapes".
	cleanTestEnv();
	json jout = run_sa("../test/testdata/build-mgr/001_helloworld.pa");
	ASSERT_TRUE(jout.is_object());
	ASSERT_TRUE(jout.contains("libs"));
	ASSERT_TRUE(jout["libs"].is_array());
	ASSERT_EQ(jout["libs"].size(), 0u);
}

TEST(sa, link_libs_no_func_header) {
	// A header that declares zero functions produces a
	// cinclude node with no "functions" key at all -- collecting `libs`
	// must not be gated on that section. The link_marker_t declaration
	// proves the header really was loaded.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/186_link_no_func_header.pa");
	ASSERT_TRUE(jout.is_object());

	ASSERT_EQ(jout["libs"].size(), 1u);
	ASSERT_EQ(jout["libs"][0], "rt");

	const auto& v = jout["statements"][0]["vars"][0];
	ASSERT_EQ(v["name"], "n");
	ASSERT_EQ(v["var-type"]["type-name"], "int32");
}

TEST(sa, link_libs_in_block) {
	// A cinclude nested in a block still contributes to the ROOT "libs":
	// linking is a whole-program property, so unlike the C declarations the
	// header brings in (which stay scoped to the block), a library has no
	// scope to be local to. A cinclude inside a Palan function body takes
	// the identical path (sa_function -> sa_statements -> sa_cinclude); the
	// block fixture stands in for both.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/187_link_in_block.pa");
	ASSERT_TRUE(jout.is_object());

	ASSERT_EQ(jout["statements"][0]["stmt-type"], "block");
	ASSERT_EQ(jout["libs"].size(), 1u);
	ASSERT_EQ(jout["libs"][0], "m");
}

TEST(sa, syscall_decl) {
	// A syscall declaration registers in plnFuncScopes but is a prototype
	// with no body, so it does not appear in sa["functions"] -- only "main"
	// does. Covers both a lit-int and a lit-uint syscall-number, and the
	// 6-parameter ABI ceiling exactly at its limit (not over it).
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/188_syscall_decl.pa");
	ASSERT_TRUE(jout.is_object());

	ASSERT_EQ(jout["functions"].size(), 2u);
	ASSERT_EQ(funcName(jout["functions"][0]["name"]), "main");
}

TEST(sa, syscall_call) {
	// A call resolved to a syscall declaration gets func-type:"syscall" on
	// its call node (not the "palan" that a normal user function call
	// gets), routed through the same findPlnFunc/plnFuncScopes lookup.
	// Covers both a call with args (write) and a no-arg call (getpid), and
	// confirms the callee's ret-type still flows to value-type unchanged.
	// syscall-number rides along on the call node itself (folded to an
	// integer) since the declaration never reaches sa["functions"].
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/189_syscall_call.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& body = jout["functions"][0]["body"];

	const auto& writeCall = body[1]["vars"][0]["init"];
	ASSERT_EQ(writeCall["func-type"], "syscall");
	ASSERT_EQ(writeCall["name"], "write");
	ASSERT_EQ(writeCall["value-type"]["type-name"], "int64");
	ASSERT_EQ(writeCall["syscall-number"], 1);

	const auto& getpidCall = body[2]["vars"][0]["init"];
	ASSERT_EQ(getpidCall["func-type"], "syscall");
	ASSERT_EQ(getpidCall["name"], "getpid");
	ASSERT_EQ(getpidCall["value-type"]["type-name"], "int32");
	ASSERT_EQ(getpidCall["syscall-number"], 39);
}

TEST(sa, syscall_macro_number) {
	// A cinclude'd macro constant (SYS_write) is folded by gen-ast into a
	// typed lit-int before validateSyscallDecl runs, so it is accepted the
	// same as a plain literal syscall number.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/193_syscall_macro_number.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& writeCall = jout["functions"][0]["body"][1]["vars"][0]["init"];
	ASSERT_EQ(writeCall["func-type"], "syscall");
	ASSERT_EQ(writeCall["name"], "sys_write");
	ASSERT_EQ(writeCall["syscall-number"], 1);
}

TEST(sa, c_enum) {
	json jout = run_sa("../test/testdata/sa/223_c_enum.pa");
	ASSERT_TRUE(jout.is_object());
	auto enumType = [](const char* name) {
		return json{{"type-kind","prim"},{"type-name","int32"},{"enum",name}};
	};
	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts[1]["value-type"], enumType("Color"));
	ASSERT_EQ(stmts[1]["value"]["value"], "6");
	ASSERT_EQ(stmts[2]["value-type"], enumType("CSpace"));
	ASSERT_EQ(stmts[3]["target"]["array"]["value-type"]["base-type"], enumType("Mode"));
	const auto& args = stmts[5]["vars"][0]["init"]["args"];
	ASSERT_EQ(args[0]["value-type"], enumType("Color"));   // DEF_COLOR macro
	ASSERT_EQ(args[0]["value"], "5");
	ASSERT_EQ(args[1]["value-type"], enumType("Mode"));
	ASSERT_EQ(stmts[5]["vars"][0]["var-type"], enumType("CSpace"));
	ASSERT_EQ(stmts[6]["vars"][0]["init"]["value"], "10");    // anonymous enumerator
	ASSERT_EQ(stmts[7]["vars"][0]["init"]["value-type"], enumType("CSpace"));
	ASSERT_EQ(stmts[8]["vars"][0]["init"]["value-type"], enumType("Color"));
	ASSERT_EQ(stmts[9]["vars"][0]["init"]["value-type"], enumType("Color"));
}

TEST(sa, cross_cinclude_typedef) {
	json jout = run_sa("../test/testdata/sa/224_cross_cinclude_typedef.pa");
	ASSERT_TRUE(jout.is_object());
	json u64 = {{"type-kind","prim"},{"type-name","uint64"}};
	json kind = {{"type-kind","prim"},{"type-name","int32"},{"enum","Kind"}};
	const auto& stmts = jout["statements"];
	const auto& call = stmts[2]["vars"][0]["init"];   // box_len(@r, b.n, Kind.K_B)
	ASSERT_EQ(call["value-type"], u64);
	ASSERT_EQ(call["args"][0]["value-type"]["base-type"]["type-name"], "Rec");
	ASSERT_EQ(call["args"][1]["value-type"], u64);
	ASSERT_EQ(call["args"][2]["value-type"], kind);
	ASSERT_EQ(stmts[2]["vars"][0]["var-type"], u64);    // count_t: a typedef of my_size
	ASSERT_EQ(stmts[3]["vars"][0]["init"]["offset"], 16);
	ASSERT_EQ(stmts[3]["vars"][0]["init"]["value-type"], kind);
	json constU64 = u64;
	constU64["const"] = true;
	ASSERT_EQ(stmts[4]["vars"][0]["init"]["array"]["value-type"]["base-type"], constU64);
	// ((my_size)8) macro: typed by its cast as in my_size's own header.
	json mySize = u64;
	mySize["typedef-name"] = "my_size";
	ASSERT_EQ(stmts[5]["vars"][0]["init"]["value-type"], mySize);
}

TEST(sa, c_func_ptr_field)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/254_c_func_ptr_field.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	// The function pointer fields keep their 8-byte slots.
	ASSERT_EQ(stmts[0]["vars"][0]["init"]["args"][0]["value"], "32");
	ASSERT_EQ(stmts[1]["stmt-type"], "field-assign");
	ASSERT_EQ(stmts[1]["offset"], 24);
}

TEST(sa, c_ptr_ptr_field)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/256_c_ptr_ptr_field.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	const auto& pp = stmts[1]["vars"][0]["init"]["value-type"];
	ASSERT_EQ(pp["base-type"]["type-kind"], "pntr");
	ASSERT_EQ(pp["base-type"]["base-type"]["type-name"], "int32");
	const auto& pt = stmts[2]["vars"][0]["init"]["value-type"];
	ASSERT_EQ(pt["base-type"]["base-type"]["type-kind"], "struct");
	ASSERT_EQ(pt["base-type"]["base-type"]["type-name"], "T");
	const auto& slot = stmts[3]["vars"][0]["init"]["value-type"];
	ASSERT_EQ(slot["base-type"]["type-kind"], "pntr");
	ASSERT_EQ(slot["base-type"]["base-type"]["type-name"], "int64");
}

TEST(sa, c_ptr_field_mutable)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/261_c_ptr_field_mutable.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	for (int i : {1, 2, 3, 4})
		EXPECT_EQ(stmts[i]["vars"][0]["init"]["value-type"]["mutable"], true) << "stmt " << i;
	EXPECT_EQ(stmts[4]["vars"][0]["init"]["value-type"]["base-type"]["mutable"], true);
	EXPECT_EQ(stmts[5]["vars"][0]["init"]["value-type"]["mutable"], false);
}

TEST(sa, c_ptr_typedef)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/262_c_ptr_typedef.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	auto pntr = [](json base, bool mut) {
		return json{{"type-kind", "pntr"}, {"mutable", mut}, {"base-type", base}};
	};
	json i32 = {{"type-kind", "prim"}, {"type-name", "int32"}};
	EXPECT_EQ(stmts[1]["vars"][0]["var-type"], pntr(i32, true));    // P
	EXPECT_EQ(stmts[2]["vars"][0]["var-type"], pntr(i32, false));   // CP
	EXPECT_EQ(stmts[3]["vars"][0]["var-type"], pntr(i32, true));    // PC: the pointer's own const is not a pointee permission
	EXPECT_EQ(stmts[4]["vars"][0]["var-type"], pntr(i32, true));    // PA
	EXPECT_EQ(stmts[5]["vars"][0]["var-type"],
		pntr(pntr({{"type-kind", "prim"}, {"type-name", "int8"}}, true), true));    // PP
	EXPECT_EQ(stmts[6]["vars"][0]["var-type"],
		pntr({{"type-kind", "struct"}, {"type-name", "S"}}, true));    // SP
	EXPECT_EQ(stmts[7]["vars"][0]["var-type"],
		pntr({{"type-kind", "prim"}, {"type-name", "void"}}, true));    // timer_t
}

TEST(sa, c_fd_set)
{
	// fd_set's array field is sized with sizeof, so it is a complete struct.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/264_c_fd_set.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& init = jout["statements"][0]["vars"][0]["init"];
	ASSERT_EQ(init["name"], "__pln_malloc");
	ASSERT_EQ(init["args"][0]["value"], "128");
}

TEST(sa, c_func_ptr_typedef)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/268_c_func_ptr_typedef.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	const auto& vt = stmts[0]["vars"][0]["var-type"];
	ASSERT_EQ(vt["mutable"], false);
	ASSERT_EQ(vt["base-type"]["type-kind"], "func");
	ASSERT_EQ(vt["base-type"]["parameters"][0]["var-type"]["type-name"], "int32");
	ASSERT_EQ(stmts[1]["vars"][0]["init"]["expr-type"], "call-ptr");
}
