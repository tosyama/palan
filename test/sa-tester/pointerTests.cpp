/// semantic analyzer tests: pointers
///
/// @file pointerTests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include "../test-base/testBase.h"
#include "saTestUtil.h"

using namespace std;

TEST(sa, raw_ptr_field)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/076_raw_ptr_field.pa");
	ASSERT_TRUE(jout.is_object());

	// Node { int64 val; @Node next; } — int64(8) + ptr(8) = 16
	const auto& v = jout["statements"][0]["vars"][0];
	ASSERT_EQ(v["name"], "n");
	ASSERT_EQ(v["init"]["name"], "__pln_malloc");
	ASSERT_EQ(v["init"]["args"][0]["value"], "16");
}

TEST(sa, raw_ptr_prim_field)
{
	// Prerequisite fix: a raw-ptr struct field whose
	// pointee is primitive (e.g. `@!int64 p;`), not another struct, used to
	// have its pointee kind forgotten at registration (buildStructDef), so
	// fieldValueType/resolveObjectChain always rebuilt it as pntr(struct(...))
	// -- `s.p` failed with a misleading "unknown struct type 'int64'." This
	// covers read (`s.p`), write (`x -> s.p`), and index (`s.p[0]`), all of
	// which must resolve to pntr(prim int64), not pntr(struct("int64")).
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/176_raw_ptr_prim_field.pa");
	ASSERT_TRUE(jout.is_object());

	// x -> s.p;  (field-assign)
	const auto& assign = jout["statements"][2];
	ASSERT_EQ(assign["stmt-type"], "field-assign");
	ASSERT_EQ(assign["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(assign["value-type"]["base-type"]["type-kind"], "prim");
	ASSERT_EQ(assign["value-type"]["base-type"]["type-name"], "int64");

	// 9 -> s.p[0];  (arr-assign through the field)
	const auto& arr_target = jout["statements"][3]["target"];
	ASSERT_EQ(arr_target["array"]["value-type"]["base-type"]["type-name"], "int64");
	ASSERT_EQ(arr_target["value-type"]["type-name"], "int64");

	// @!int64 rp = s.p;  (plain field read)
	const auto& rp = jout["statements"][4]["vars"][0];
	ASSERT_EQ(rp["init"]["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(rp["init"]["value-type"]["base-type"]["type-kind"], "prim");
	ASSERT_EQ(rp["init"]["value-type"]["base-type"]["type-name"], "int64");
}

TEST(sa, ptr_field_chain)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/083_ptr_field_chain.pa");
	ASSERT_TRUE(jout.is_object());

	// int64 v = n.next.val: @Node next@8, int64 val@0
	// ptr-expr={var:"n",offset:8,mutable:false}, outer offset:0
	const auto& rd = jout["statements"][1]["vars"][0]["init"];
	ASSERT_EQ(rd["expr-type"],                "field-access");
	ASSERT_EQ(rd["offset"],                   0);
	ASSERT_EQ(rd["ptr-expr"]["var"],           "n");
	ASSERT_EQ(rd["ptr-expr"]["offset"],        8);
	ASSERT_EQ(rd["ptr-expr"]["value-type"]["mutable"], false);
}

TEST(sa, struct_ptr_field_assign)
{
	// type A { int64 v; }; type B { A inner; }; B b; A a; a -> b.inner;
	// An owned struct field is copied into, not rebound: a flat A is a memcpy
	// into the storage the field points to.
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/088_struct_ptr_field_assign.pa"));
	ASSERT_TRUE(jout.is_object());

	json* call = nullptr;
	for (auto& s : jout["statements"])
		if (s["stmt-type"] == "expr" && s["body"].value("name", "") == "memcpy") { call = &s["body"]; break; }
	ASSERT_NE(call, nullptr);

	const json& dst = (*call)["args"][0];
	ASSERT_EQ(dst["expr-type"],                           "field-access");
	ASSERT_EQ(dst["var"],                                 "b");
	ASSERT_EQ(dst["offset"],                              0);
	ASSERT_EQ(dst["value-type"]["base-type"]["type-name"], "A");
	ASSERT_EQ((*call)["args"][1]["name"],                 "a");
	ASSERT_EQ((*call)["args"][2]["value"],                "8");
}

TEST(sa, raw_ptr_mutable_field_assign)
{
	// type Node { int64 val; @!Node next; }; Node n1; Node n2; n2 -> n1.next;
	// field-assign for raw-ptr mutable field → fieldValueType sets mutable:true
	// Covers: fieldValueType raw-ptr branch (pntr["mutable"] = isMutable)
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/089_raw_ptr_mutable_field_assign.pa"));
	ASSERT_TRUE(jout.is_object());

	json* fa = nullptr;
	for (auto& s : jout["statements"])
		if (s["stmt-type"] == "field-assign") { fa = &s; break; }
	ASSERT_NE(fa, nullptr);

	// field "next" is raw-ptr mutable → value-type must be pntr with mutable:true
	ASSERT_EQ((*fa)["value-type"]["type-kind"],              "pntr");
	ASSERT_EQ((*fa)["value-type"]["mutable"],                true);
	ASSERT_EQ((*fa)["value-type"]["base-type"]["type-name"], "Node");
}

TEST(sa, deep_ptr_chain_access)
{
	// type A{int64 v}; type B{A a}; type C{B b}; C c;
	// int64 x = c.b.a.v;  — two levels of owned struct field traversal
	// Covers: resolveObjectChain when base.isPointerBased=true (nested ptr-expr)
	// Also: resolveObjectChain(forWrite=true) ptr-based for "10 -> c.b.a.v"
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/090_deep_ptr_chain.pa");
	ASSERT_TRUE(jout.is_object());

	// Find the var-decl for x (read chain)
	const json* x_decl = nullptr;
	for (auto& s : jout["statements"]) {
		if (s["stmt-type"] != "var-decl") continue;
		for (auto& v : s["vars"])
			if (v["name"] == "x") { x_decl = &v; break; }
		if (x_decl) break;
	}
	ASSERT_NE(x_decl, nullptr);

	// init = field-access with nested ptr-expr (c.b.a.v)
	const auto& init = (*x_decl)["init"];
	ASSERT_EQ(init["expr-type"], "field-access");
	ASSERT_EQ(init["offset"],    0);
	// ptr-expr points into B.a (second owned struct level)
	const auto& pe1 = init["ptr-expr"];
	ASSERT_EQ(pe1["expr-type"], "field-access");
	// pe1 itself has a ptr-expr pointing to C.b (first owned struct level)
	ASSERT_TRUE(pe1.contains("ptr-expr"));
	const auto& pe2 = pe1["ptr-expr"];
	ASSERT_EQ(pe2["expr-type"],  "field-access");
	ASSERT_EQ(pe2["var"],        "c");

	// Find the field-assign "10 -> c.b.a.v" (write chain)
	json* fa = nullptr;
	for (auto& s : jout["statements"])
		if (s["stmt-type"] == "field-assign") { fa = &s; break; }
	ASSERT_NE(fa, nullptr);
	// ptr-expr chain must also be nested
	ASSERT_TRUE((*fa).contains("ptr-expr"));
	ASSERT_TRUE((*fa)["ptr-expr"].contains("ptr-expr"));
}

TEST(sa, void_ptr_compat)
{
	// pntr(void) parameters (e.g. memcpy's void* dest/src) must resolve
	// and accept pntr(T) arguments without SA throwing on the unknown "void" prim.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/123_void_ptr_compat.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& call = jout["statements"][2]["body"];
	ASSERT_EQ(call["expr-type"], "call");
	ASSERT_EQ(call["name"], "memcpy");
	ASSERT_EQ(call["func-type"], "c");
}

TEST(sa, void_ptr_cmp)
{
	// Comparing two pntr(void) results (e.g. memchr() == memchr()) goes
	// through the "cmp" expr's unguarded fromJson() calls, which previously threw.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/124_void_ptr_cmp.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& decl = jout["statements"][2];
	ASSERT_EQ(decl["stmt-type"], "var-decl");
	const auto& v = decl["vars"][0];
	ASSERT_EQ(v["name"], "r");
	ASSERT_EQ(v["var-type"]["type-name"], "int32");
	ASSERT_EQ(v["init"]["expr-type"], "convert");
	ASSERT_EQ(v["init"]["src"]["expr-type"], "cmp");
	ASSERT_EQ(v["init"]["src"]["value-type"]["type-name"], "bool");
}

TEST(sa, at_bang_plain_var_decl)
{
	// `@!Point view = original;` as a plain (non-field) local var decl.
	// Two gaps had to be closed for this to work:
	//  1. gen-ast: var_declaration's "type_expr ID '=' expression" alt (PlnParser.yy)
	//     only whitelisted tk=="prim" for the initializer form; pntr-kind types
	//     (what `@T`/`@!T` produce) fell through to not-impl. Same for the bare
	//     no-init decl alt. Widened both whitelists to include tk=="pntr".
	//  2. SA: deepNormalizePrimToStruct must run on sa_var_decl's generic fallthrough
	//     path so `view`'s var-type carries base-type.type-kind=="struct" (not "prim"),
	//     matching what resolveObjectChain requires for field access.
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/130_at_bang_plain_var_decl.pa"));
	ASSERT_TRUE(jout.is_object());

	// statements: [0] type Point is consumed, so:
	// [0] var-decl original, [1] var-decl view (init=original), [2] field-assign view.x=20, [3] var-decl vx
	const auto& view_decl = jout["statements"][1];
	ASSERT_EQ(view_decl["stmt-type"], "var-decl");
	const auto& v = view_decl["vars"][0];
	ASSERT_EQ(v["name"], "view");
	ASSERT_EQ(v["var-type"]["type-kind"], "pntr");
	ASSERT_EQ(v["var-type"]["mutable"], true);
	ASSERT_EQ(v["var-type"]["base-type"]["type-kind"], "struct");
	ASSERT_EQ(v["var-type"]["base-type"]["type-name"], "Point");
	// No allocator call synthesized for a plain @!T decl -- init is just the
	// pointer value copied from `original` (id expr), not a malloc/__pln_alloc_ call.
	ASSERT_EQ(v["init"]["expr-type"], "id");
	ASSERT_EQ(v["init"]["name"], "original");

	const auto& fa = jout["statements"][2];
	ASSERT_EQ(fa["stmt-type"], "field-assign");
	ASSERT_EQ(fa["var"], "view");
	// field "x" is int64, not struct-typed -- writing through it exercises the
	// pntr(struct)-based field chain resolution, but the field itself stays prim.
	ASSERT_EQ(fa["value-type"]["type-name"], "int64");

	const auto& vx_decl = jout["statements"][3];
	ASSERT_EQ(vx_decl["vars"][0]["init"]["expr-type"], "field-access");
	ASSERT_EQ(vx_decl["vars"][0]["init"]["var"], "view");
}

TEST(sa, at_plain_var_decl_readonly)
{
	// `@Point view = original;` (read-only, non-mutable) plain local var decl.
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/131_at_plain_var_decl_readonly.pa"));
	ASSERT_TRUE(jout.is_object());

	const auto& view_decl = jout["statements"][1];
	const auto& v = view_decl["vars"][0];
	ASSERT_EQ(v["name"], "view");
	ASSERT_EQ(v["var-type"]["type-kind"], "pntr");
	ASSERT_EQ(v["var-type"].value("mutable", false), false);
	ASSERT_EQ(v["var-type"]["base-type"]["type-kind"], "struct");
	ASSERT_EQ(v["var-type"]["base-type"]["type-name"], "Point");
	ASSERT_EQ(v["init"]["expr-type"], "id");
	ASSERT_EQ(v["init"]["name"], "original");

	const auto& vx_decl = jout["statements"][2];
	ASSERT_EQ(vx_decl["vars"][0]["init"]["expr-type"], "field-access");
	ASSERT_EQ(vx_decl["vars"][0]["init"]["var"], "view");
}

TEST(sa, at_bang_bare_decl_then_assign)
{
	// `@!Point view;` with no initializer, followed by a plain
	// arrow-assign (`original -> view;`) -- the other newly-enabled gen-ast
	// path (bare var_declaration alt widened to accept tk=="pntr").
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/132_at_bang_bare_decl_then_assign.pa");
	ASSERT_TRUE(jout.is_object());

	// [0] var-decl original, [1] var-decl view (no init), [2] assign view=original,
	// [3] field-assign view.x=20, [4] var-decl vx
	const auto& view_decl = jout["statements"][1];
	const auto& v = view_decl["vars"][0];
	ASSERT_EQ(v["name"], "view");
	ASSERT_EQ(v["var-type"]["base-type"]["type-kind"], "struct");
	ASSERT_FALSE(v.contains("init"));

	const auto& assign = jout["statements"][2];
	ASSERT_EQ(assign["stmt-type"], "assign");
	ASSERT_EQ(assign["name"], "view");
	ASSERT_EQ(assign["value"]["expr-type"], "id");
	ASSERT_EQ(assign["value"]["name"], "original");

	const auto& fa = jout["statements"][3];
	ASSERT_EQ(fa["stmt-type"], "field-assign");
	ASSERT_EQ(fa["var"], "view");
}

TEST(sa, addr_of_readonly_local)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/133_addr_of_readonly_local.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& p = jout["statements"][1]["vars"][0];
	ASSERT_EQ(p["name"], "p");
	ASSERT_EQ(p["var-type"]["type-kind"], "pntr");
	ASSERT_EQ(p["var-type"]["mutable"], false);
	ASSERT_EQ(p["var-type"]["base-type"]["type-name"], "int64");

	const auto& init = p["init"];
	ASSERT_EQ(init["expr-type"], "addr-of");
	ASSERT_EQ(init["name"], "x");
	ASSERT_EQ(init["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(init["value-type"]["mutable"], false);
}

TEST(sa, addr_of_mutable_local)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/134_addr_of_mutable_local.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& q = jout["statements"][1]["vars"][0];
	ASSERT_EQ(q["name"], "q");
	ASSERT_EQ(q["var-type"]["type-kind"], "pntr");
	ASSERT_EQ(q["var-type"]["mutable"], true);

	const auto& init = q["init"];
	ASSERT_EQ(init["expr-type"], "addr-of");
	ASSERT_EQ(init["name"], "x");
	ASSERT_EQ(init["value-type"]["mutable"], true);
}

TEST(sa, addr_of_field_readonly)
{
	// `@int64 p = @s.x;` -- address-of on a struct field lowers to
	// field-access + addr-only:true (CalcAddr in codegen), not an "addr-of"
	// node -- the field leaf has no local-variable storage of its own.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/147_addr_of_field_readonly.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& p = jout["statements"][1]["vars"][0];
	ASSERT_EQ(p["var-type"]["mutable"], false);

	const auto& init = p["init"];
	ASSERT_EQ(init["expr-type"], "field-access");
	ASSERT_EQ(init["addr-only"], true);
	ASSERT_EQ(init["var"], "s");
	ASSERT_EQ(init["offset"], 0);
	ASSERT_EQ(init["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(init["value-type"]["mutable"], false);
	ASSERT_EQ(init["value-type"]["base-type"]["type-name"], "int64");
}

TEST(sa, addr_of_field_mutable)
{
	// `@!int64 p = @!s.y;` -- mutable counterpart; also exercises the
	// forWrite=true path through resolveObjectChain's "id" branch, which
	// must NOT reject it (s itself is a plain mutable struct variable).
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/148_addr_of_field_mutable.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& init = jout["statements"][1]["vars"][0]["init"];
	ASSERT_EQ(init["expr-type"], "field-access");
	ASSERT_EQ(init["addr-only"], true);
	ASSERT_EQ(init["var"], "s");
	ASSERT_EQ(init["offset"], 8);  // y follows x (int64 x 8 bytes)
	ASSERT_EQ(init["value-type"]["mutable"], true);
}

TEST(sa, addr_of_field_nested_embed)
{
	// `@!int64 p = @!s.in.v;` where `in` is `$Inner` (inline embed) -- the
	// embed hop accumulates offset inside resolveObjectChain and the result
	// is a single field-access node (var:"s"), not a nested ptr-expr chain.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/149_addr_of_field_nested_embed.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& init = jout["statements"][1]["vars"][0]["init"];
	ASSERT_EQ(init["expr-type"], "field-access");
	ASSERT_EQ(init["addr-only"], true);
	ASSERT_EQ(init["var"], "s");
	ASSERT_EQ(init["offset"], 16);  // x(8) + y(8)
	ASSERT_FALSE(init.contains("ptr-expr"));
	ASSERT_EQ(init["value-type"]["mutable"], true);
}

TEST(sa, addr_of_field_via_ptr_hop)
{
	// `@!int64 p = @!n.next.val;` where `next` is a `@!Node` raw-ptr field --
	// resolveObjectChain hops through the pointer field, producing a
	// ptr-expr-based field-access (chain.isPointerBased == true) rather than
	// a var-based one.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/150_addr_of_field_via_ptr_hop.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& init = jout["statements"][1]["vars"][0]["init"];
	ASSERT_EQ(init["expr-type"], "field-access");
	ASSERT_EQ(init["addr-only"], true);
	ASSERT_FALSE(init.contains("var"));
	ASSERT_TRUE(init.contains("ptr-expr"));
	ASSERT_EQ(init["ptr-expr"]["var"], "n");
	ASSERT_EQ(init["value-type"]["mutable"], true);
}

TEST(sa, addr_of_field_via_arr_index)
{
	// `@!int64 p = @!wpts[0].x;` where `wpts` is `[4]@!Point` (mutable
	// pointer-slot array) -- the field's object chain hops through an
	// arr-index node, exercising resolveObjectChain's "arr-index" branch
	// under forWrite=true (the mutable slot passes the check).
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/151_addr_of_field_via_arr_index.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& body = jout["functions"][0]["body"];
	const auto& init = body[1]["vars"][0]["init"];
	ASSERT_EQ(init["expr-type"], "field-access");
	ASSERT_EQ(init["addr-only"], true);
	ASSERT_TRUE(init.contains("ptr-expr"));
	ASSERT_EQ(init["ptr-expr"]["expr-type"], "arr-index");
	ASSERT_EQ(init["value-type"]["mutable"], true);
}

TEST(sa, addr_of_ptr_local)
{
	// `@!int8 end; @!@!int8 pp = @!end;` -- `@`/`@!`
	// also accepts a pointer-to-primitive local (not just a primitive one),
	// producing pntr-of-pntr. A struct local is still rejected (unchanged),
	// since it's already pntr(struct T) itself.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/169_addr_of_ptr_local.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& pp = jout["statements"][1]["vars"][0];
	ASSERT_EQ(pp["var-type"]["type-kind"], "pntr");
	ASSERT_EQ(pp["var-type"]["base-type"]["type-kind"], "pntr");
	ASSERT_EQ(pp["var-type"]["base-type"]["base-type"]["type-name"], "int8");

	const auto& init = pp["init"];
	ASSERT_EQ(init["expr-type"], "addr-of");
	ASSERT_EQ(init["name"], "end");
	ASSERT_EQ(init["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(init["value-type"]["mutable"], true);
	ASSERT_EQ(init["value-type"]["base-type"]["type-kind"], "pntr");
	ASSERT_EQ(init["value-type"]["base-type"]["base-type"]["type-name"], "int8");
}

TEST(sa, void_ptr_decl)
{
	// `@!void p = malloc(8);` -- pntr(void) has always
	// been valid internally (it's what a C `void*` deserializes to), but
	// sa_var_decl's pointee-name check rejected the *spelled* void pointee
	// until isKnownPointeeTypeName. `@!int8 q = p;` exercises the existing
	// bidirectional pntr(void)<->pntr(T) compat rule (typeCompat). The
	// trailing `@!void p2; @!@!void pp = @!p2;` is the void** out-param
	// idiom (e.g. posix_memalign) -- sa_expr_addr_of's isPtrToPrim already
	// accepted a void pointee, so this is a non-regression check.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/175_void_ptr_decl.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& p = jout["statements"][0]["vars"][0];
	ASSERT_EQ(p["var-type"]["type-kind"], "pntr");
	ASSERT_EQ(p["var-type"]["base-type"]["type-name"], "void");
	ASSERT_EQ(p["init"]["name"], "malloc");

	const auto& q = jout["statements"][1]["vars"][0];
	ASSERT_EQ(q["var-type"]["base-type"]["type-name"], "int8");
	ASSERT_EQ(q["init"]["value-type"]["base-type"]["type-name"], "void");

	const auto& pp = jout["statements"][4]["vars"][0];
	ASSERT_EQ(pp["var-type"]["type-kind"], "pntr");
	ASSERT_EQ(pp["var-type"]["base-type"]["type-kind"], "pntr");
	ASSERT_EQ(pp["var-type"]["base-type"]["base-type"]["type-name"], "void");
	ASSERT_EQ(pp["init"]["expr-type"], "addr-of");
	ASSERT_EQ(pp["init"]["name"], "p2");
}

TEST(sa, addr_of_embed_field)
{
	// `@!Inner q = @!s.in;` where `in` is `$Inner` (inline embed) --
	// An embed field's own value-type is already
	// pntr(struct Inner) (the field IS the struct's inline storage), so
	// taking its address must NOT double-wrap into pntr(pntr(struct Inner)).
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/170_addr_of_embed_field.pa"));
	ASSERT_TRUE(jout.is_object());

	const auto& init = jout["statements"][1]["vars"][0]["init"];
	ASSERT_EQ(init["expr-type"], "field-access");
	ASSERT_EQ(init["addr-only"], true);
	ASSERT_EQ(init["var"], "s");
	ASSERT_EQ(init["offset"], 8);  // x(8) then embedded Inner
	ASSERT_EQ(init["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(init["value-type"]["mutable"], true);
	ASSERT_EQ(init["value-type"]["base-type"]["type-kind"], "struct");
	ASSERT_EQ(init["value-type"]["base-type"]["type-name"], "Inner");
}

TEST(sa, deref_rw_mutable_ptr)
{
	// `@!int64 p = @!x; 99 -> p[0]; int64 y = p[0];` -- deref read/write
	// through a mutable `@!T` is allowed both ways.
	// Covers: sa_arr_assign_stmt isWritableThrough branch, mutable:true case
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/141_deref_rw_mutable_ptr.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& arr_assign = jout["statements"][2];
	ASSERT_EQ(arr_assign["stmt-type"], "arr-assign");
	ASSERT_EQ(arr_assign["target"]["array"]["value-type"]["mutable"], true);
}

TEST(sa, ptr_mutability_narrowing)
{
	// `@!int64 q = @!x; @int64 r = q;` -- binding a mutable pointer to a
	// read-only-typed destination narrows permission and is allowed.
	// Covers: sa_var_decl ptrPermissionOk branch, narrowing case (no error)
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/142_ptr_mutability_narrowing.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& r = jout["statements"][2]["vars"][0];
	ASSERT_EQ(r["name"], "r");
	ASSERT_EQ(r["var-type"]["mutable"], false);
}

TEST(sa, deref_scalar_widths)
{
	// `@!int32 p = @!x; 99 -> p[0]; int32 y = p[0];` -- `p[i]` is
	// specified as C-equivalent pointer subscript. A scalar element gives a
	// real load/store (addr-only:false) with elem-size == sizeof(elem).
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/143_deref_scalar_widths.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& arr_assign = jout["statements"][2];
	ASSERT_EQ(arr_assign["stmt-type"], "arr-assign");
	const auto& target = arr_assign["target"];
	ASSERT_EQ(target["addr-only"], false);
	ASSERT_EQ(target["elem-size"]["value"], "4");
	ASSERT_EQ(target["value-type"]["type-name"], "int32");

	const auto& y = jout["statements"][3]["vars"][0];
	ASSERT_EQ(y["init"]["addr-only"], false);
	ASSERT_EQ(y["init"]["elem-size"]["value"], "4");
}

TEST(sa, deref_var_index)
{
	// `p[i]` with a variable index takes the same shape as `p[0]` -- there is
	// no index==0 special case anywhere in SA.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/144_deref_var_index.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& arr_assign = jout["statements"][4];
	ASSERT_EQ(arr_assign["stmt-type"], "arr-assign");
	const auto& target = arr_assign["target"];
	ASSERT_EQ(target["index"]["expr-type"], "id");
	ASSERT_EQ(target["index"]["name"], "i");
	ASSERT_EQ(target["addr-only"], false);
}

TEST(sa, deref_struct_ptr_field_rw)
{
	// `Point pt; 10 -> pt[0].x; int64 v = pt[0].x;` -- a pointer to a
	// struct dereferences to an address computation (Palan has no
	// register-sized struct value), so `p[i]` yields pntr(struct) with
	// addr-only:true and elem-size == sizeof(struct), reachable for both
	// read (field-access) and write (field-assign) via `p[i].field`.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/145_deref_struct_ptr_field_rw.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& field_assign = jout["statements"][1];
	ASSERT_EQ(field_assign["stmt-type"], "field-assign");
	const auto& write_ptr = field_assign["ptr-expr"];
	ASSERT_EQ(write_ptr["expr-type"], "arr-index");
	ASSERT_EQ(write_ptr["addr-only"], true);
	ASSERT_EQ(write_ptr["elem-size"]["value"], "16");
	ASSERT_EQ(write_ptr["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(write_ptr["value-type"]["base-type"]["type-kind"], "struct");
	ASSERT_FALSE(write_ptr["value-type"].contains("mutable"));

	const auto& v = jout["statements"][2]["vars"][0];
	ASSERT_EQ(v["init"]["expr-type"], "field-access");
	ASSERT_EQ(v["init"]["ptr-expr"]["addr-only"], true);
}

TEST(sa, deref_readonly_struct_ptr_field_read)
{
	// `@Point ro = pt; int64 v = ro[0].x;` -- read-only enforcement
	// only blocks writes; reading a field through a read-only struct pointer
	// via `p[0].field` must still be allowed. Regression guard.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/146_deref_readonly_struct_ptr_field_read.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& v = jout["statements"][2]["vars"][0];
	const auto& ptr_expr = v["init"]["ptr-expr"];
	ASSERT_EQ(ptr_expr["addr-only"], true);
	ASSERT_FALSE(ptr_expr["value-type"].contains("mutable"));
	ASSERT_EQ(ptr_expr["array"]["value-type"]["mutable"], false);
}

TEST(sa, addr_of_arr_elem)
{
	// `@!arr[2]` / `@arr[1]` on a scalar `[4]int64` array -- the
	// scalar branch of sa_expr_arr_index (addr-only:false, prim elem) is
	// the addressable case; addr-of re-tags it addr-only:true and wraps
	// value-type in pntr(elem, mutable).
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/153_addr_of_arr_elem.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& body = jout["functions"][0]["body"];
	const auto& p_init = body[1]["vars"][0]["init"];
	ASSERT_EQ(p_init["expr-type"], "arr-index");
	ASSERT_EQ(p_init["addr-only"], true);
	ASSERT_EQ(p_init["array"]["name"], "arr");
	ASSERT_EQ(p_init["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(p_init["value-type"]["mutable"], true);
	ASSERT_EQ(p_init["value-type"]["base-type"]["type-name"], "int64");

	const auto& q_init = body[2]["vars"][0]["init"];
	ASSERT_EQ(q_init["addr-only"], true);
	ASSERT_EQ(q_init["value-type"]["mutable"], false);
}

TEST(sa, addr_of_embed_arr_field_elem)
{
	// `@!s.data[2]` where `data` is `[4]$int64` (embedded array field,
	// primitive leaf) -- exercises sa_expr_arr_index's embedded+prim branch
	// (addr-only:false before addr-of) nested under a field-access array base.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/154_addr_of_embed_arr_field_elem.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& body = jout["functions"][0]["body"];
	const auto& arg0 = body[2]["body"]["args"][0];
	ASSERT_EQ(arg0["expr-type"], "arr-index");
	ASSERT_EQ(arg0["addr-only"], true);
	ASSERT_EQ(arg0["array"]["expr-type"], "field-access");
	ASSERT_EQ(arg0["array"]["var"], "s");
	ASSERT_EQ(arg0["value-type"]["mutable"], true);
	ASSERT_EQ(arg0["value-type"]["base-type"]["type-name"], "int64");
}

TEST(sa, addr_of_2d_elem)
{
	// `@!mat[0][1]` where `mat` is `[2]$[3]int64` -- the outer arr-index's
	// array is itself an addr-only arr-index (the 2D row access), but the
	// outer element (after indexing into the row) is a plain prim scalar,
	// so addr-of accepts it.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/155_addr_of_2d_elem.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& body = jout["functions"][0]["body"];
	const auto& arg0 = body[2]["body"]["args"][0];
	ASSERT_EQ(arg0["expr-type"], "arr-index");
	ASSERT_EQ(arg0["addr-only"], true);
	ASSERT_EQ(arg0["array"]["expr-type"], "arr-index");
	ASSERT_EQ(arg0["array"]["addr-only"], true);
	ASSERT_EQ(arg0["array"]["array"]["name"], "mat");
	ASSERT_EQ(arg0["value-type"]["mutable"], true);
	ASSERT_EQ(arg0["value-type"]["base-type"]["type-name"], "int64");
}

TEST(sa, ptr_decl_alias_pointee)
{
	// `type MyInt = int64; @MyInt p = @a;` -- an alias used as a pointee
	// resolves to its underlying type instead of reaching
	// PlnTypeRegistry::fromJson unresolved (which used to abort with
	// "unknown prim type-name: MyInt").
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/157_ptr_decl_alias_pointee.pa");
	ASSERT_TRUE(jout.is_object());

	const auto& p = jout["statements"][1]["vars"][0];
	ASSERT_EQ(p["var-type"]["type-kind"], "pntr");
	ASSERT_EQ(p["var-type"]["base-type"]["type-kind"], "prim");
	ASSERT_EQ(p["var-type"]["base-type"]["type-name"], "int64");
}

TEST(sa, ptr_arr_field_permission)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/203_ptr_arr_field_permission.pa");
	ASSERT_TRUE(jout.is_object());

	const json& wr = jout["functions"][1];
	ASSERT_EQ(funcName(wr["name"]), "wr");
	int arrAssigns = 0;
	for (auto& st : wr["body"])
		if (st["stmt-type"] == "arr-assign") arrAssigns++;
	ASSERT_EQ(arrAssigns, 2);
}

TEST(sa, ptr_slot_arr_borrow)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/208_ptr_slot_arr_borrow.pa"));
	ASSERT_TRUE(jout.is_object());

	// The borrow's permission covers the slots only; each element pointer
	// keeps its own.
	json expected = {{"type-kind","pntr"},{"arr-size",2},{"mutable",false},
	                 {"base-type",{{"type-kind","pntr"},{"mutable",true},
	                               {"base-type",{{"type-kind","struct"},{"type-name","P"}}}}}};
	ASSERT_EQ(jout["functions"][0]["parameters"][0]["var-type"], expected);

	const json* call = nullptr;
	for (auto& st : jout["statements"])
		if (st["stmt-type"] == "expr") call = &st["body"];
	ASSERT_NE(call, nullptr);
	ASSERT_EQ((*call)["args"][0]["value-type"], expected);
}

TEST(sa, ptr_field_forward_ref)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/210_ptr_field_forward_ref.pa"));
	ASSERT_TRUE(jout.is_object());

	// a.bs[1].x: a [2]@B slot declared before B still reads as a struct pointer.
	const json& rd = jout["statements"].back()["vars"][0]["init"];
	ASSERT_EQ(rd["expr-type"], "field-access");
	ASSERT_EQ(rd["ptr-expr"]["expr-type"], "arr-index");
	ASSERT_EQ(rd["ptr-expr"]["value-type"]["base-type"]["type-name"], "B");
}

TEST(sa, ptr_slot_store) {
	json jout = run_sa("../test/testdata/sa/219_ptr_slot_store.pa");
	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts.size(), 4);
	// A '@![]T' slot holds a borrow, so '->' stores the pointer instead of copying.
	ASSERT_EQ(stmts[1]["stmt-type"], "arr-assign");
	ASSERT_EQ(stmts[1]["target"]["value-type"]["mutable"], true);
	ASSERT_EQ(stmts[1]["value"]["name"], "malloc");
}

TEST(sa, embed_ptr_slot_store) {
	json jout = run_sa("../test/testdata/sa/221_embed_ptr_slot_store.pa");
	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts.size(), 3);
	// A slot pointing at contiguous rows is a borrow too, not an array to copy into.
	ASSERT_EQ(stmts[1]["stmt-type"], "arr-assign");
	ASSERT_EQ(stmts[1]["target"]["value-type"]["embedded"], true);
	ASSERT_EQ(stmts[1]["target"]["value-type"]["mutable"], true);
	ASSERT_EQ(stmts[1]["value"]["name"], "malloc");
}

TEST(sa, ro_ptr_slot_store) {
	json jout = run_sa("../test/testdata/sa/324_ro_ptr_slot.pa");
	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts.size(), 14);
	ASSERT_EQ(stmts[11]["stmt-type"], "arr-assign");
	ASSERT_EQ(stmts[11]["target"]["value-type"]["mutable"], false);
	// Read-only reaches every row level, so '@m' of rows fits the slot.
	const auto& row = stmts[12]["target"]["value-type"];
	ASSERT_EQ(row["embedded"], true);
	ASSERT_EQ(row["mutable"], false);
	ASSERT_EQ(row["base-type"]["mutable"], false);
	ASSERT_EQ(stmts[13]["stmt-type"], "arr-assign");
}

// '@[]T' of no known count is the same type as '@T', in a parameter as in a local.
TEST(sa, ro_unsized_borrow) {
	json jout = run_sa("../test/testdata/sa/325_ro_unsized_borrow.pa");
	const auto& funcs = jout["functions"];
	json ro = {{"type-kind", "pntr"}, {"base-type", {{"type-kind", "prim"}, {"type-name", "uint8"}}}, {"mutable", false}};
	ASSERT_EQ(funcs[0]["parameters"][0]["var-type"], ro);
	ASSERT_EQ(funcs[1]["parameters"][0]["var-type"]["mutable"], true);
	ASSERT_FALSE(funcs[1]["parameters"][0]["var-type"].contains("arr-size"));
	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts[6]["vars"][0]["var-type"], ro);
}

// An array's element may be any pointer, in an array variable as in a borrow.
TEST(sa, ptr_ptr_slot_arr)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/314_ptr_ptr_slot_arr.pa");
	ASSERT_TRUE(jout.is_object());
	const json& v = jout["statements"][0]["vars"][0]["var-type"];
	ASSERT_EQ(v["arr-size"], 3);
	ASSERT_EQ(v["base-type"]["mutable"], true);
	ASSERT_EQ(v["base-type"]["base-type"]["mutable"], true);
	ASSERT_EQ(v["base-type"]["base-type"]["base-type"]["type-name"], "int32");
	const json& a = jout["functions"][0]["parameters"][0]["var-type"];
	ASSERT_EQ(a["base-type"]["mutable"], false);
	ASSERT_EQ(a["base-type"]["base-type"]["base-type"]["type-kind"], "struct");
}

// A `[]T` parameter, a C function and a pointer value take an array by name.
TEST(sa, arr_to_ptr_addr_of)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/316_arr_to_ptr_addr_of.pa");
	ASSERT_TRUE(jout.is_object());
}

// A `[+]@T` holds pointer slots as `[n]@T` does, in the generated functions of `[+]int64`.
TEST(sa, varr_ptr_slot)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/318_varr_ptr_slot.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	const json& v = stmts[0]["vars"][0]["var-type"]["base-type"];
	ASSERT_EQ(v["type-kind"], "varr");
	ASSERT_EQ(v["base-type"]["mutable"], true);
	ASSERT_EQ(v["base-type"]["base-type"]["type-kind"], "struct");
	const json& s = stmts[1]["vars"][0]["var-type"]["base-type"]["base-type"];
	ASSERT_EQ(s["mutable"], false);
	ASSERT_EQ(s["base-type"]["type-name"], "uint8");
	ASSERT_EQ(stmts[2]["vars"][0]["var-type"]["base-type"]["base-type"]["type-kind"], "varr");
	const json& f = jout["functions"][0];
	ASSERT_EQ(f["parameters"][0]["var-type"]["mutable"], false);
	ASSERT_FALSE(f["ret-type"].contains("mutable"));
	ASSERT_EQ(jout["alloc-shapes"][0]["shape-key"], "varr_int64");
}

TEST(sa, struct_ptr_elem_copy)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/237_struct_ptr_elem_copy.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& funcs = jout["functions"];
	const auto& copy = funcs[0]["body"][0]["body"];
	ASSERT_EQ(copy["expr-type"], "call");
	ASSERT_EQ(copy["name"], "memcpy");
	ASSERT_EQ(copy["args"][0]["expr-type"], "arr-index");
	const auto& borrow = funcs[1]["body"][0]["vars"][0]["init"];
	ASSERT_EQ(borrow["expr-type"], "arr-index");
	ASSERT_EQ(borrow["value-type"]["mutable"], true);
}

TEST(sa, ptr_ptr_field)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/255_ptr_ptr_field.pa"));
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	const auto& pp = stmts[1]["vars"][0]["init"]["value-type"];
	ASSERT_EQ(pp["mutable"], true);
	ASSERT_EQ(pp["base-type"]["type-kind"], "pntr");
	ASSERT_EQ(pp["base-type"]["mutable"], true);
	ASSERT_EQ(pp["base-type"]["base-type"]["type-name"], "int32");
	const auto& slot = stmts[2]["vars"][0]["init"]["value-type"];
	ASSERT_EQ(slot["mutable"], false);
	ASSERT_EQ(slot["base-type"]["mutable"], true);
	ASSERT_EQ(slot["base-type"]["base-type"]["type-name"], "int32");
	const auto& pl = stmts[3]["vars"][0]["init"]["value-type"];
	ASSERT_EQ(pl["mutable"], true);
	ASSERT_EQ(pl["base-type"]["mutable"], false);
	ASSERT_EQ(pl["base-type"]["base-type"]["type-kind"], "struct");
	ASSERT_EQ(pl["base-type"]["base-type"]["type-name"], "Later");
}

TEST(sa, ptr_inner_permission)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/257_ptr_inner_permission.pa");
	ASSERT_TRUE(jout.is_object());
}

TEST(sa, func_ptr)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/269_func_ptr.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	const auto& ref = stmts[0]["vars"][0]["init"];
	ASSERT_EQ(ref["expr-type"], "func-ref");
	ASSERT_EQ(funcName(ref["name"]), "twice");
	ASSERT_EQ(ref["func-type"], "palan");
	// A single named return is the function's return value.
	ASSERT_EQ(funcName(stmts[1]["value"]["name"]), "one");
	ASSERT_EQ(stmts[1]["value"]["value-type"]["base-type"]["ret-type"]["type-name"], "int32");

	const auto& call = stmts[2]["vars"][0]["init"];
	ASSERT_EQ(call["expr-type"], "call-ptr");
	ASSERT_EQ(call["callee"]["name"], "f");
	ASSERT_EQ(call["value-type"]["type-name"], "int32");
	ASSERT_EQ(call["args"].size(), 1);

	ASSERT_EQ(stmts[4]["stmt-type"], "field-assign");
	const auto& fieldCall = stmts[5]["vars"][0]["init"];
	ASSERT_EQ(fieldCall["expr-type"], "call-ptr");
	ASSERT_EQ(fieldCall["callee"]["expr-type"], "field-access");

	ASSERT_EQ(stmts[6]["stmt-type"], "expr");
	ASSERT_EQ(stmts[6]["body"]["expr-type"], "call-ptr");
}

TEST(sa, func_ptr_paths)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/282_func_ptr_paths.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts[0]["vars"][0]["init"]["value-type"]["base-type"]["ret-type"]["type-name"], "void");
	const auto& hook = stmts[1]["vars"][0]["init"];
	ASSERT_EQ(hook["expr-type"], "call-ptr");
	ASSERT_EQ(hook["callee"]["expr-type"], "id");
	ASSERT_EQ(jout["main"]["globals"][0]["label"], hook["callee"]["name"]);
	// A function whose receiver is read-only is not the method form of a
	// call through a '@!H', so the field is called.
	const auto& field = stmts[3]["vars"][0]["init"];
	ASSERT_EQ(field["expr-type"], "call-ptr");
	ASSERT_EQ(field["callee"]["expr-type"], "field-access");
}

TEST(sa, func_cast_keeps_ownership)
{
	// A Palan function's cast may change a pointer's permission or keep a type
	// with `_`; only a C function's may change ownership.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/332_func_cast_keeps_ownership.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	const auto& palanRef = stmts[2]["body"]["callee"]["src"];
	ASSERT_EQ(palanRef["func-type"], "palan");
	const auto& cRef = stmts[3]["body"]["callee"]["src"];
	ASSERT_EQ(cRef["func-type"], "c");
}
