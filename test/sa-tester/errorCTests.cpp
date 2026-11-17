/// semantic analyzer error case tests: C interop
///
/// @file errorCTests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include "../test-base/testBase.h"

using namespace std;

TEST(sa_error, tuple_c_func_single_ret) {
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_700_tuple_c_func_single_ret.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":2:1: error: Function 'abs' does not have multiple return values."), string::npos);
}

TEST(sa_error, cinclude_unqualified_alias)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand("palan-gen-ast ../test/testdata/sa/error_058_cinclude_unqualified_alias.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("requires a module alias qualifier"), string::npos);
}

TEST(sa_error, cinclude_typedef_conflict)
{
	// Two headers typedef the same name to different types.
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_089_cinclude_typedef_conflict.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":2:1: error: Typedef 'myint' conflicts"), string::npos);
}

TEST(sa_error, readonly_ptr_to_nonconst_c_param)
{
	// `clock_getcpuclockid(int32(0), p);` where `p` is `@int32` (read-only)
	// and the C parameter is `clockid_t *` (non-const) -- C function
	// arguments are checked by
	// ptrPermissionOk() too, previously exempted (see the removed comment at
	// sa_expr_call's arg loop).
	// Covers: sa_expr_call checkArgPtrPermission, C-func branch (E_ReadOnlyPtrToNonConstCParam)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_112_readonly_ptr_to_nonconst_c_param.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot pass read-only pointer '@T' to non-const parameter"), string::npos);
}

TEST(sa_error, readonly_ptr_to_nonconst_c_param_alias)
{
	// Same as readonly_ptr_to_nonconst_c_param but through an aliased
	// cinclude (`cinclude <time.h> as T; T.clock_getcpuclockid(...)`) --
	// sa_expr_member_call had no pointer-permission check at all before this
	// ticket, for either C or Palan callees.
	// Covers: sa_expr_member_call checkArgPtrPermission, C-func branch (E_ReadOnlyPtrToNonConstCParam)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_113_readonly_ptr_to_nonconst_c_param_alias.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot pass read-only pointer '@T' to non-const parameter"), string::npos);
}

TEST(sa_error, readonly_ptr_to_nonconst_c_param_unnamed)
{
	// `tmpnam(s)` -- glibc's stdio.h declares tmpnam's parameter with an
	// abstract declarator (no name), so c2ast's parameter entry has no
	// "name" key. checkArgPtrPermission falls back to a 1-based position
	// ("#1") for the diagnostic in that case.
	// Covers: checkArgPtrPermission, param.value("name","").empty() branch
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_114_readonly_ptr_to_nonconst_c_param_unnamed.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot pass read-only pointer '@T' to non-const parameter '#1'"), string::npos);
}

TEST(sa_error, incomplete_struct_var_decl)
{
	// `struct Tag { int x; int cells[]; };` (cinclude'd, "cells" unsupported)
	// then `Tag t;` -- an owned declaration needs Tag's totalSize to malloc it.
	// registerCStruct downgrades Tag to an incomplete struct
	// (opaque handle) instead of leaving the tag unregistered, so this is a
	// diagnosed E_IncompleteStructType, not "unknown type".
	// Covers: sa_struct_var_decl -> requireCompleteStruct
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_123_incomplete_struct_var_decl.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("struct 'Tag' has no known layout"), string::npos);
}

TEST(sa_error, incomplete_struct_forward_declared)
{
	// `struct Tag;` (forward-declared only, never defined in this header) then
	// `Tag t;` -- registerCStruct registers a
	// forward-declared-only tag as an incomplete struct too (previously it was
	// never registered at all, and a chain like this used to hit a raw
	// BOOST_ASSERT abort in requireCompleteStruct rather than a diagnostic).
	// The message distinguishes this reason ("forward-declared") from an
	// unsupported field shape.
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_132_forward_declared_struct.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("struct 'Tag' is only forward-declared in this header"), string::npos);
}

TEST(sa_error, c_union_unknown_field)
{
	// The unknown field sits mid-chain (u.c.a), so it is diagnosed while
	// resolving the object chain rather than at the final field store.
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_187_c_union_unknown_field.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("union 'U' has no field 'c'"), string::npos);
}

TEST(sa_error, c_union_forward_declared)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_188_c_union_forward_decl.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find("union 'F' is only forward-declared in this header"), string::npos);
}

TEST(sa_error, incomplete_struct_owned_arr)
{
	// Same incomplete Tag as above; `[3]Tag a;` (owned pointer array) needs
	// Tag's layout to record its alloc-shape (recordAllocShape reads totalSize).
	// Covers: sa_owned_struct_arr_var_decl -> requireCompleteStruct
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_124_incomplete_struct_owned_arr.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("struct 'Tag' has no known layout"), string::npos);
}

TEST(sa_error, incomplete_struct_embed_arr)
{
	// Same incomplete Tag; `[3]$Tag a;` (contiguous embedded array) needs Tag's
	// totalSize as the element stride for the single malloc(n * totalSize).
	// Covers: sa_embed_arr_var_decl -> requireCompleteStruct
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_125_incomplete_struct_embed_arr.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("struct 'Tag' has no known layout"), string::npos);
}

TEST(sa_error, incomplete_struct_native_embed)
{
	// Same incomplete Tag, embedded in a native struct: `type Wrap { $Tag a; };`
	// declared inside a function body so it is only processed by sa_statements
	// (pass 3), after cinclude registers Tag (pass 2) -- at top level a struct-def
	// is also pre-scanned in pass 0, before cinclude, which would hit the
	// separate "name not yet registered" E_UnknownType path instead of
	// this one.
	// Covers: buildStructDef "embed" branch, sub.isComplete check
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_126_incomplete_struct_native_embed.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":4:2: error: struct 'Tag' has no known layout"), string::npos);
}

TEST(sa_error, incomplete_struct_field_access)
{
	// `func f(@!Tag p) { int64 v = p.x; }` -- `@!Tag p` itself declares fine (no
	// layout needed for a pointer parameter), but reading a field requires
	// looking up Tag's field list, which an incomplete struct doesn't have.
	// Without this check this would previously degrade to a misleading
	// "struct 'Tag' has no field 'x'" (E_UnknownField) instead of naming the
	// real problem.
	// Covers: findFieldOrExit -> requireCompleteStruct
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_127_incomplete_struct_field_access.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("struct 'Tag' has no known layout"), string::npos);
	ASSERT_EQ(sa.find("has no field"), string::npos);
}

TEST(sa_error, incomplete_struct_index)
{
	// `func f(@!Tag p) { int64 v = p[0].x; }` -- `p[0]` computes an address as
	// base + i*sizeof(Tag), which needs Tag's totalSize as the stride; an
	// incomplete Tag has none.
	// Covers: sa_expr_arr_index struct branch -> requireCompleteStruct
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_128_incomplete_struct_index.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("struct 'Tag' has no known layout"), string::npos);
}

TEST(sa_error, incomplete_struct_native_owned_field)
{
	// Same incomplete Tag; `type Wrap { Tag a; };` (a bare-name field, i.e. an
	// owned struct field per native struct syntax) is declared inside a
	// function body, same reasoning as incomplete_struct_native_embed above,
	// so it processes after cinclude registers Tag as incomplete. An owned
	// struct field's declaring struct records an alloc-shape for it, which
	// needs the pointee's totalSize.
	// Covers: buildStructDef "prim name is a registered struct" branch (owned)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_129_incomplete_struct_native_owned_field.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":4:2: error: struct 'Tag' has no known layout"), string::npos);
}

TEST(sa_error, incomplete_struct_native_owned_arr)
{
	// Same incomplete Tag; `type Wrap { [3]Tag a; };` (owned pointer array
	// field, cascades to __pln_alloc_arr_T/__pln_free_arr_T) needs the leaf's
	// totalSize when that cascade's alloc-shape is recorded.
	// Covers: buildStructDef "[n]T owned pointer array, struct leaf" branch
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_130_incomplete_struct_native_owned_arr.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":4:2: error: struct 'Tag' has no known layout"), string::npos);
}

TEST(sa_error, incomplete_struct_native_embed_arr)
{
	// Same incomplete Tag; `type Wrap { [3]$Tag a; };` (contiguous embedded
	// array field within a *native* struct, distinct from the top-level
	// `[3]$Tag a;` var-decl covered by incomplete_struct_embed_arr above --
	// that goes through sa_embed_arr_var_decl, this goes through
	// buildStructDef's own embedded-array leaf case) needs the leaf's
	// totalSize/maxAlign/ownsFields to lay out the array stride.
	// Covers: buildStructDef "[n]$T embedded array, struct leaf" branch
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_131_incomplete_struct_native_embed_arr.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":4:2: error: struct 'Tag' has no known layout"), string::npos);
}

TEST(sa_error, c_unsupported_user_param)
{
	// `void take_handle(mystery_t h);` -- `mystery_t` is an identifier c2ast
	// never saw a typedef for, so it stays type-kind "user" through
	// normalizeCType. normalizeCFuncSig tags the registered
	// entry with "_unsupported-sig" and requireSupportedCFuncSig diagnoses it
	// at the call, instead of the unguarded fromJson at the argument site
	// aborting (or, before this ticket, the parameter-side try/catch quietly
	// degrading typeCompat/wrapConvert for the argument).
	// Covers: sa_expr_call -> requireSupportedCFuncSig (unaliased path)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_134_c_unsupported_user_param.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot call C function 'take_handle'"), string::npos);
	ASSERT_NE(sa.find("'mystery_t'"), string::npos);
}

TEST(sa_error, c_unsupported_union_param)
{
	// `int use_val(union { int i; } v);` -- a nameless union in a signature
	// has no tag to register a layout under, so c2ast emits a bare
	// {"type-kind":"union"}. glibc always names or typedefs its unions, so
	// this is a hand-written header.
	// Covers: sa_expr_call -> requireSupportedCFuncSig
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_136_c_unsupported_union_param.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot call C function 'use_val'"), string::npos);
	ASSERT_NE(sa.find("'union'"), string::npos);
}

TEST(sa_error, c_unsupported_enum_param)
{
	// `void pick(enum Color c);` where c2ast can't compute Color's values
	// (`= sizeof(struct Opaque)`, an incomplete type), so the enum is never registered.
	// Covers: sa_expr_call -> requireSupportedCFuncSig
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_137_c_unsupported_enum_param.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot call C function 'pick'"), string::npos);
	ASSERT_NE(sa.find("'enum'"), string::npos);
}

TEST(sa_error, c_unsupported_union_ret)
{
	// `union { int i; } make_val(void);` -- the ret-type-only path: the callee has
	// no parameters, so the only way to reach a diagnosis is if
	// requireSupportedCFuncSig fires before sa_expr["value-type"] is set from
	// ret-type (sa_expr_call:457-459) -- proves the gate precedes that copy
	// rather than only catching it downstream once value-type is consumed.
	// Covers: sa_expr_call -> requireSupportedCFuncSig, before ret-type copy
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_138_c_unsupported_union_ret.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot call C function 'make_val'"), string::npos);
	ASSERT_NE(sa.find("'union'"), string::npos);
}

TEST(sa_error, c_unsupported_func_param)
{
	// `void set_cb(int (*cb)(long double));` -- a function pointer whose own
	// parameter is unrepresentable makes the whole signature unsupported.
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_139_c_unsupported_func_param.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot call C function 'set_cb'"), string::npos);
	ASSERT_NE(sa.find("'function pointer'"), string::npos);
}

TEST(sa_error, c_unsupported_anon_strct_param)
{
	// `void f(struct { int a; } *p);` -- an inline anonymous struct behind a
	// pointer. c2ast's "strct" branch still omits type-name for a tagless
	// struct even after removing the definedStructs_ guard (that guard
	// only covered forward-declared *tagged* references); normalizeCType
	// folds it to a nameless {"type-kind":"struct"}.
	// Covers: sa_expr_call -> requireSupportedCFuncSig, nameless struct
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_140_c_unsupported_anon_strct_param.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot call C function 'f'"), string::npos);
	ASSERT_NE(sa.find("'anonymous struct'"), string::npos);
}

TEST(sa_error, c_unsupported_long_double)
{
	// `acosl(1.0)` from <math.h> -- `long double` maps to c2ast's "flt128"
	// prim type-name (CParser.cpp), which is not in PrimTypeNames -- the
	// dominant real-world case: math.h alone has 309 such nodes across 150
	// functions in the empirical audit for this ticket, far more than every
	// other unsupported kind combined. Also proves unrepresentableTypeName's
	// "prim" branch (a resolvable type-kind, unresolvable type-name), not
	// just its type-kind branches, and that the diagnostic fires instead of
	// the pre-2906 abort ("unknown prim type-name: flt128", rc=134).
	// Covers: sa_expr_call -> requireSupportedCFuncSig, unresolved prim name
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_141_c_unsupported_long_double.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot call C function 'acosl'"), string::npos);
	ASSERT_NE(sa.find("'flt128'"), string::npos);
	ASSERT_EQ(sa.find("unrepresentable type"), string::npos);  // diagnosed, not the raw fromJson throw text
}

TEST(sa_error, c_unsupported_aliased_call)
{
	// Same shape as c_unsupported_user_param, but through an aliased
	// `cinclude ... as M;` / `M.take_handle(...)` call -- exercises
	// sa_expr_member_call's separate gate rather than sa_expr_call's.
	// Covers: sa_expr_member_call -> requireSupportedCFuncSig (aliased path)
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_142_c_unsupported_aliased_call.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot call C function 'take_handle'"), string::npos);
	ASSERT_NE(sa.find("'mystery_t'"), string::npos);
}

TEST(sa_error, c_global_not_assignable)
{
	// `fopen(...) -> stderr;` -- writing a C global needs a `global stderr;` first.
	// Covers: requireAssignable C global branch, E_GlobalNotWritable
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_143_c_global_not_assignable.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot write to 'stderr'"), string::npos);
	ASSERT_NE(sa.find("'global stderr;'"), string::npos);
}

TEST(sa_error, c_global_addr_of)
{
	// `@stderr;` -- stderr is a FILE pointer, a borrow as any '@!FILE' variable is.
	// Covers: sa_expr_addr_of lookupVar of a C global, E_AddrOfBorrowed
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_144_c_global_addr_of.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("'stderr' is already a borrow"), string::npos);
}

TEST(sa_error, c_global_out_of_block_scope)
{
	// `stderr` referenced after the block whose cinclude registered it --
	// cGlobalScopes is popped by leaveScope like cFuncScopes, so this is an
	// ordinary undefined-variable error, not a C-global-specific one.
	// Covers: enterScope/leaveScope cGlobalScopes pop, sa_expression "id" fallthrough
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_146_c_global_out_of_block_scope.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("Undefined variable"), string::npos);
	ASSERT_NE(sa.find("'stderr'"), string::npos);
}

TEST(sa_error, c_global_unsupported_type)
{
	// `extern long double ld;` -- cinclude succeeds (deferred, same policy as
	// _unsupported-sig), but referencing `ld` diagnoses its unrepresentable
	// "flt128" type instead of the pre-2908 abort a raw "user"/unresolved
	// prim type-name would otherwise cause downstream.
	// Covers: sa_expression "id" branch -> requireSupportedCGlobal, E_UnsupportedCGlobalType
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_147_c_global_unsupported_type.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot reference C global variable 'ld'"), string::npos);
	ASSERT_NE(sa.find("'flt128'"), string::npos);
}

TEST(sa_error, call_arg_ptr_mismatch_c)
{
	// Same gap as call_arg_ptr_mismatch_native, but through the C-callee
	// path (a cinclude'd `long *` parameter fed a `@!int32` argument).
	// Covers: convertCallArg via sa_expr_call (C callee), E_IncompatibleTypes
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_154_call_arg_ptr_mismatch_c.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot convert '@!int32' to '@!int64'"), string::npos);
}

TEST(sa_error, callback_arg_not_func)
{
	// A variable of another type in a C function's function pointer parameter.
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_159_callback_arg_not_func.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":5:1: error: cannot convert 'int64' to '@func(@void, @void) -> int32'."), string::npos) << sa;
}

TEST(sa_error, callback_sig_arity_mismatch)
{
	// A function with fewer parameters than qsort's comparator.
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_160_callback_sig_arity_mismatch.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":4:1: error: cannot convert '@func(@int32) -> int32' to '@func(@void, @void) -> int32'."), string::npos) << sa;
}

TEST(sa_error, callback_sig_ptr_permission)
{
	// qsort passes `const void*`: a function that may write through its
	// parameters can't take them.
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_161_callback_ptr_permission.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":4:1: error: cannot convert '@func(@!int32, @!int32) -> int32' to '@func(@void, @void) -> int32'."), string::npos) << sa;
}

TEST(sa_error, callback_sig_width_mismatch)
{
	// The callee is called with no conversion step, so int32 does not widen
	// to an int64 parameter as it would in a direct call.
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_162_callback_sig_width_mismatch.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":4:1: error: cannot convert '@func(int64, @int8)' to '@func(int32, @!void)'."), string::npos) << sa;
}

TEST(sa_error, c_unsupported_func_ret)
{
	// `void set_cb(long double (*cb)(int));` -- the function pointer's return
	// type, not a parameter, is unrepresentable.
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_163_c_unsupported_func_ret.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot call C function 'set_cb'"), string::npos);
	ASSERT_NE(sa.find("'function pointer'"), string::npos);
}

TEST(sa_error, callback_ret_void_mismatch)
{
	// A function returning a value where a void function is expected.
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_164_callback_ret_void_mismatch.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":3:1: error: cannot convert '@func() -> int32' to '@func()'."), string::npos) << sa;
}

TEST(sa_error, callback_ret_type_mismatch)
{
	// Matching parameters, different non-void return type.
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_165_callback_ret_type_mismatch.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":4:1: error: cannot convert '@func(@int32, @int32) -> int64' to '@func(@void, @void) -> int32'."), string::npos) << sa;
}

TEST(sa_error, callback_multi_ret_mismatch)
{
	// Multiple named returns have no single return register to bind to.
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_169_callback_multi_ret_mismatch.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":4:34: error: function 'cmp' cannot be used as a function pointer: it returns multiple values."), string::npos) << sa;
}

TEST(sa_error, callback_ret_missing_mismatch)
{
	// A void function where qsort's comparator returns int.
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_170_callback_ret_missing_mismatch.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":4:1: error: cannot convert '@func(@int32, @int32)' to '@func(@void, @void) -> int32'."), string::npos) << sa;
}

TEST(sa_error, invalid_link_lib_name)
{
	// A library name reaches build-mgr's `ld` command
	// string, so a name carrying shell metacharacters is rejected
	// at the one point a name enters "libs", not downstream.
	// Covers: isValidLinkLibName reject path, E_InvalidLinkLibName
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_171_invalid_link_lib.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(": error:"), string::npos);
	ASSERT_NE(sa.find("invalid library name"), string::npos);
}

TEST(sa_error, link_obj_not_found)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_611_link_obj_not_found.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":1:1: error: object file '"), string::npos);
	ASSERT_NE(sa.find("/test/testdata/sa/error_611_missing.o' in a cinclude link clause is not found."), string::npos);
}

TEST(sa_error, empty_link_lib_name)
{
	// `link "";` lexes fine (the link-clause strings are scanned in
	// INITIAL, where STRING allows zero characters) and would otherwise
	// emit a bare "-l" that swallows the next ld argument.
	// Covers: isValidLinkLibName empty() guard
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_172_empty_link_lib.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("invalid library name"), string::npos);
}

TEST(sa_error, syscall_number_not_constant)
{
	// A syscall number must be a literal integer.
	// Covers: validateSyscallDecl expr-type guard, E_SyscallNumberNotConstant
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_173_syscall_number_not_constant.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("number must be a literal integer"), string::npos);
}

TEST(sa_error, syscall_number_out_of_range)
{
	// Covers: validateSyscallDecl uint32 range guard, E_SyscallNumberOutOfRange
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_175_syscall_number_out_of_range.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("does not fit in a 32-bit unsigned integer"), string::npos);
}

TEST(sa_error, syscall_too_many_params)
{
	// Linux syscall ABI supports at most 6 register arguments.
	// Covers: validateSyscallDecl parameter-count guard, E_SyscallTooManyParams
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_176_syscall_too_many_params.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("Linux syscall ABI supports at most 6"), string::npos);
}

TEST(sa_error, syscall_float_param)
{
	// flo32/flo64 do not fit in a GP register, which is all the Linux
	// syscall ABI passes arguments through.
	// Covers: validateSyscallDecl float-parameter guard, E_SyscallUnsupportedParamType
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_179_syscall_float_param.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot pass through the Linux syscall ABI: 'flo64'"), string::npos);
}

TEST(sa_error, syscall_float_return)
{
	// Covers: validateSyscallDecl float-return guard, E_SyscallUnsupportedParamType
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_180_syscall_float_return.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot pass through the Linux syscall ABI: 'flo64'"), string::npos);
}

TEST(sa_error, syscall_narrow_param)
{
	// int8/int16/uint8/uint16 have no dedicated slot in the Linux syscall
	// ABI either -- every GP register is read/written in full, so a
	// narrower declared type would leave stale high bits behind a
	// partial-register move instead of the extension the ABI assumes.
	// Covers: validateSyscallDecl narrow-int guard, E_SyscallUnsupportedParamType
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_184_syscall_narrow_param.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot pass through the Linux syscall ABI: 'int8'"), string::npos);
}

TEST(sa_error, syscall_duplicate)
{
	// Proves a syscall declaration actually lands in plnFuncScopes (shared
	// with "palan" func-defs, not a separate table) -- a second syscall of
	// the same name hits the same E_DuplicateFuncDef guard.
	// Covers: preregisterFunc -> registerPlnFunc shared table, E_DuplicateFuncDef
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_181_syscall_duplicate.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":2:1: error: Function 'foo' is already defined."), string::npos);
}

TEST(sa_error, syscall_call_narrowing)
{
	// A syscall call's argument checking reuses the same convertCallArg path
	// as a Palan/C call -- an implicit narrowing conversion is rejected
	// identically.
	// Covers: sa_expr_call syscall resolution -> saCallArgs -> convertCallArg, E_InvalidNarrowingConv
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_182_syscall_call_narrowing.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("Implicit conversion from 'int64' to 'int32' is not allowed"), string::npos);
}

TEST(sa_error, syscall_call_ptr_permission)
{
	// A syscall call's pointer argument checking reuses checkArgPtrPermission
	// -- passing a read-only '@T' where the syscall declares a mutable
	// '@!T' parameter is rejected the same way a Palan call would be.
	// Covers: sa_expr_call syscall resolution -> checkArgPtrPermission (isCFunc=false path), E_PtrMutabilityUpgrade
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_183_syscall_call_ptr_permission.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("cannot bind a read-only pointer"), string::npos);
}

TEST(sa_error, macro_backref_undefined)
{
	// A reference to a cinclude'd macro name that textually precedes its
	// cinclude statement is not folded by gen-ast (visibility is loc-order,
	// same as the macro table itself) -- it reaches SA as a plain "id" and
	// is diagnosed like any other undefined variable.
	// Covers: foldMacroConstants loc-order gate, E_UndefinedVariable
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_185_macro_backref_undefined.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find("Undefined variable"), string::npos);
}

TEST(sa_error, macro_backref_arr_field)
{
	// Same loc-order gate as macro_backref_undefined, but for a struct
	// field array size -- the un-folded "id" names no const.
	// Covers: foldMacroConstants loc-order gate
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_186_macro_backref_arr_field.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa, "");
	ASSERT_NE(sa.find(":1:1: error: struct field array size must be a compile-time constant integer; 'ARR_N' is not a const."), string::npos) << sa;
}

TEST(sa_error, c_call_too_many_args)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_382_c_call_too_many_args.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":2:11: error: function 'abs' takes 1 argument(s), but 2 given."), string::npos);
}

TEST(sa_error, c_call_too_few_args)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_383_c_call_too_few_args.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":2:11: error: function 'abs' takes 1 argument(s), but 0 given."), string::npos);
}

TEST(sa_error, c_void_call_with_arg)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_384_c_void_call_with_arg.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":2:11: error: function 'getchar' takes 0 argument(s), but 1 given."), string::npos);
}

TEST(sa_error, variadic_too_few_args)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_385_variadic_too_few_args.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":2:1: error: function 'printf' takes at least 1 argument(s), but 0 given."), string::npos);
}

TEST(sa_error, c_enum_field_from_int)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_403_c_enum_field_from_int.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":3:1: error: Implicit conversion from 'int32' to 'CSpace' is not allowed"), string::npos);
}

TEST(sa_error, c_enum_name_conflict)
{
	cleanTestEnv();
	string ast_out = "out/test.ast.json";
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_404_c_enum_name_conflict.pa -o " + ast_out), "");
	string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	// A top-level cinclude's types are registered before any Palan declaration.
	ASSERT_NE(sa.find(":1:1: error: type 'Color' is already defined."), string::npos);

	cleanTestEnv();
	ASSERT_EQ(execTestCommand(
		"palan-gen-ast ../test/testdata/sa/error_525_c_enum_name_conflict_nested.pa -o " + ast_out), "");
	sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
	ASSERT_NE(sa.find(":3:2: error: type 'Color' is already defined."), string::npos);
}

TEST(sa_error, c_macro_unexported)
{
	const pair<string, string> cases[] = {
		{"error_617_c_macro_func_like.pa", ":2:11: error: C macro 'FMAC' is function-like; Palan cannot use it."},
		{"error_618_c_macro_not_constant.pa", ":2:11: error: C macro 'NOFOLD' cannot be used: it does not expand to an integer constant."},
		{"error_619_c_macro_arr_field.pa", ":2:1: error: C macro 'NOFOLD' cannot be used: it does not expand to an integer constant."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, unsupported_c_field)
{
	// A variadic function pointer can't be typed as `@func`.
	const string msg = "of 'CB': its type uses 'function pointer', a C type this version cannot represent.";
	const pair<string, string> cases[] = {
		{"error_534_c_func_ptr_field_read.pa", ":3:11: error: cannot access field 'fn' " + msg},
		{"error_535_c_func_ptr_field_write.pa", ":3:1: error: cannot access field 'fn' " + msg},
		{"error_536_c_func_ptr_field_elem.pa", ":3:11: error: cannot access field 'fns' " + msg},
		{"error_537_c_func_ptr_field_addr.pa", ":3:15: error: cannot access field 'fn' " + msg},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, c_bit_field)
{
	const string msg = "cannot access field 'flag' of 'BF': its type uses 'bit-field', a C type this version cannot represent.";
	const pair<string, string> cases[] = {
		{"error_814_c_bitfield_read.pa", ":3:11: error: " + msg},
		{"error_815_c_bitfield_write.pa", ":3:1: error: " + msg},
		{"error_816_c_bitfield_addr.pa", ":3:14: error: " + msg},
		{"error_817_c_bitfield_lit.pa", ":2:21: error: " + msg},
		{"error_818_c_bitfield_width.pa", ":2:1: error: struct 'BN' has no known layout"},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, c_const_ptr_field)
{
	const pair<string, string> cases[] = {
		{"error_574_c_const_ptr_field_write.pa", ":3:1: error: cannot write through read-only pointer '@T'"},
		{"error_575_c_const_ptr_field_hop.pa", ":3:6: error: cannot write through read-only pointer field '@T'"},
		{"error_576_c_ptr_field_bind_read_only.pa", ":4:1: error: cannot bind a read-only pointer '@T' to a mutable pointer '@!T'."},
		{"error_577_c_const_ptr_typedef_write.pa", ":4:1: error: cannot write through read-only pointer '@T'"},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, c_struct_arg)
{
	const pair<string, string> cases[] = {
		{"error_155_c_by_value_struct_param.pa", ":3:11: error: cannot convert 'int64' to 'Point'."},
		{"error_612_c_struct_arg_func_ref.pa",
		 ":3:25: error: function 'take_i8' cannot be used as a function pointer: its parameter type 'I8' cannot be passed through one."},
		// Only a by-value struct parameter takes a literal; a pointer parameter still doesn't.
		{"error_613_c_ptr_param_lit.pa", ":4:13: error: a literal cannot be passed to C function 'take_i8_ptr': only a by-value struct parameter takes one."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}

TEST(sa_error, c_struct_typedef_name)
{
	const pair<string, string> cases[] = {
		{"error_1087_c_struct_typedef_field.pa", ":3:2: error: struct 'cs_a_t' has no field 'w'."},
		{"error_1088_c_struct_typedef_arg.pa", ":3:2: error: cannot convert '@!cs_a_t' to '@!cs_b_t'."},
		{"error_1089_c_struct_tag_field.pa", ":3:2: error: struct 'cs_c' has no field 'w'."},
	};
	for (auto& [file, expected] : cases) {
		cleanTestEnv();
		string ast_out = "out/test.ast.json";
		ASSERT_EQ(execTestCommand(
			"palan-gen-ast ../test/testdata/sa/" + file + " -o " + ast_out), "");
		string sa = execTestCommand("palan-sa " + ast_out + " -o out/test.sa.json");
		ASSERT_NE(sa.find(expected), string::npos) << file << ": " << sa;
	}
}
