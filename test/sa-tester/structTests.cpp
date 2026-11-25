/// semantic analyzer tests: structs
///
/// @file structTests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include <algorithm>
#include <map>
#include "../test-base/testBase.h"
#include "saTestUtil.h"

using namespace std;

TEST(sa, toplevel_call_named_return_struct)
{
	cleanTestEnv();
	// A top-level statement calling a Palan function with a struct-typed
	// @!T named return used to see the pre-registered signature before step 2.5's
	// struct renormalization ran, leaving value-type as unnormalized prim(Point)
	// and crashing palan-sa. Struct pre-registration now happens in step 0, so
	// the call resolved here (step 2) must already see the normalized
	// pntr(struct(Point)) form.
	json jout = written(run_sa("../test/testdata/sa/135_toplevel_call_named_return_struct.pa"));
	ASSERT_TRUE(jout.is_object());

	// statements[2] = var-decl for "view" = borrow(original)
	const auto& decl = jout["statements"][2];
	ASSERT_EQ(decl["stmt-type"], "var-decl");
	const auto& v = decl["vars"][0];
	ASSERT_EQ(v["name"], "view");
	const auto& vt = v["init"]["value-type"];
	ASSERT_EQ(vt["type-kind"], "pntr");
	ASSERT_EQ(vt["base-type"]["type-kind"], "struct");
	ASSERT_EQ(vt["base-type"]["type-name"], "Point");
}

TEST(sa, toplevel_call_struct_arg)
{
	cleanTestEnv();
	// Regression: same struct pre-registration path, but exercising a
	// struct-typed parameter (not just the return type) in a top-level call.
	json jout = written(run_sa("../test/testdata/sa/137_toplevel_call_struct_arg.pa"));
	ASSERT_TRUE(jout.is_object());

	// Pre-registered signature must have its parameter normalized to pntr(struct(Point)).
	const auto& params = jout["functions"][0]["parameters"];
	ASSERT_EQ(params[0]["var-type"]["type-kind"], "pntr");
	ASSERT_EQ(params[0]["var-type"]["base-type"]["type-kind"], "struct");
	ASSERT_EQ(params[0]["var-type"]["base-type"]["type-name"], "Point");

	// statements[2] = var-decl for "s" = sumX(original); the call's argument
	// must carry the normalized struct pointer type too.
	const auto& decl = jout["statements"][2];
	ASSERT_EQ(decl["stmt-type"], "var-decl");
	const auto& v = decl["vars"][0];
	ASSERT_EQ(v["name"], "s");
	const auto& arg_vt = v["init"]["args"][0]["value-type"];
	ASSERT_EQ(arg_vt["type-kind"], "pntr");
	ASSERT_EQ(arg_vt["base-type"]["type-kind"], "struct");
	ASSERT_EQ(arg_vt["base-type"]["type-name"], "Point");
}

TEST(sa, struct_def)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/069_struct_def.pa"));
	ASSERT_TRUE(jout.is_object());

	// struct-def is consumed; first stmt is var-decl for p
	const auto& decl = jout["statements"][0];
	ASSERT_EQ(decl["stmt-type"], "var-decl");
	const auto& v = decl["vars"][0];
	ASSERT_EQ(v["name"], "p");
	ASSERT_EQ(v["var-type"]["type-kind"], "pntr");
	ASSERT_EQ(v["var-type"]["base-type"]["type-kind"], "struct");
	ASSERT_EQ(v["var-type"]["base-type"]["type-name"], "Point");
	// init: malloc(16)
	ASSERT_EQ(v["init"]["expr-type"], "call");
	ASSERT_EQ(v["init"]["name"], "__pln_malloc");
	ASSERT_EQ(v["init"]["func-type"], "c");
	ASSERT_EQ(v["init"]["args"][0]["value"], "16");
}

TEST(sa, field_assign)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/070_field_assign.pa");
	ASSERT_TRUE(jout.is_object());

	// statements[1]: 10 -> p.x
	const auto& fa1 = jout["statements"][1];
	ASSERT_EQ(fa1["stmt-type"],                  "field-assign");
	ASSERT_EQ(fa1["var"],                         "p");
	ASSERT_EQ(fa1["offset"],                      0);
	ASSERT_EQ(fa1["value-type"]["type-name"],     "int64");
	ASSERT_EQ(fa1["value"]["value"],              "10");

	// statements[2]: 20 -> p.y
	const auto& fa2 = jout["statements"][2];
	ASSERT_EQ(fa2["stmt-type"],                  "field-assign");
	ASSERT_EQ(fa2["var"],                         "p");
	ASSERT_EQ(fa2["offset"],                      8);
	ASSERT_EQ(fa2["value-type"]["type-name"],     "int64");
	ASSERT_EQ(fa2["value"]["value"],              "20");
}

TEST(sa, field_access)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/071_field_access.pa");
	ASSERT_TRUE(jout.is_object());

	// statements[2]: int64 v = p.x
	const auto& fa = jout["statements"][2]["vars"][0]["init"];
	ASSERT_EQ(fa["expr-type"],              "field-access");
	ASSERT_EQ(fa["var"],                     "p");
	ASSERT_EQ(fa["offset"],                  0);
	ASSERT_EQ(fa["value-type"]["type-name"], "int64");
}

TEST(sa, struct_c_abi)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/072_struct_c_abi.pa");
	ASSERT_TRUE(jout.is_object());

	// var-decl for m: malloc(16) — int32 a@0(4B) + pad(4B) + int64 b@8(8B) = 16B
	ASSERT_EQ(jout["statements"][0]["vars"][0]["init"]["args"][0]["value"], "16");

	// 10 -> m.a: offset=0, int32
	const auto& fa1 = jout["statements"][1];
	ASSERT_EQ(fa1["offset"],                  0);
	ASSERT_EQ(fa1["value-type"]["type-name"], "int32");

	// 20 -> m.b: offset=8, int64
	const auto& fa2 = jout["statements"][2];
	ASSERT_EQ(fa2["offset"],                  8);
	ASSERT_EQ(fa2["value-type"]["type-name"], "int64");
}

TEST(sa, field_assign_widen)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/073_field_assign_widen.pa");
	ASSERT_TRUE(jout.is_object());

	// statements[2]: v -> b.val  (int32 widened to int64)
	// SA wraps the value in a convert expression
	const auto& fa = jout["statements"][2];
	ASSERT_EQ(fa["stmt-type"],              "field-assign");
	ASSERT_EQ(fa["value-type"]["type-name"], "int64");
	ASSERT_EQ(fa["value"]["expr-type"],      "convert");
	ASSERT_EQ(fa["value"]["value-type"]["type-name"], "int64");
}

TEST(sa, embed_struct_field)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/074_embed_struct_field.pa");
	ASSERT_TRUE(jout.is_object());

	// Line { $Point a; $Point b; } — totalSize = 16+16 = 32
	const auto& v = jout["statements"][0]["vars"][0];
	ASSERT_EQ(v["name"], "l");
	ASSERT_EQ(v["init"]["name"], "__pln_malloc");
	ASSERT_EQ(v["init"]["args"][0]["value"], "32");
}

TEST(sa, owned_struct_field)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/075_owned_struct_field.pa"));
	ASSERT_TRUE(jout.is_object());

	// Rect { Point tl; Point br; } — ownsFields → __pln_alloc_Rect
	const auto& v = jout["statements"][0]["vars"][0];
	ASSERT_EQ(v["name"], "r");
	ASSERT_EQ(funcName(v["init"]["name"]), "__pln_alloc_Rect");
	ASSERT_EQ(v["init"]["func-type"], "palan");
	ASSERT_TRUE(v["init"]["args"].empty());
}

TEST(sa, alloc_shape_owned)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/077_alloc_shape_owned.pa"));
	ASSERT_TRUE(jout.is_object());

	// Rect r; — init uses __pln_alloc_Rect (pln func, no args)
	const auto& v = jout["statements"][0]["vars"][0];
	ASSERT_EQ(v["name"], "r");
	ASSERT_EQ(funcName(v["init"]["name"]), "__pln_alloc_Rect");
	ASSERT_EQ(v["init"]["func-type"], "palan");
	ASSERT_TRUE(v["init"]["args"].empty());

	// alloc-shapes: Point's layout first (leaf), then Rect (root)
	const auto& shapes = jout["alloc-shapes"];
	ASSERT_EQ(shapes.size(), 2u);
	ASSERT_EQ(shapes[0]["shape-name"], "__pln_B16");
	ASSERT_EQ(shapes[0]["shape-kind"], "bytes");
	ASSERT_EQ(shapes[0]["total-size"], 16);
	ASSERT_FALSE(shapes[0].contains("owned-fields"));
	ASSERT_EQ(shapes[1]["shape-name"], "Rect");
	ASSERT_EQ(shapes[1]["total-size"], 16);
	ASSERT_EQ(shapes[1]["owned-fields"].size(), 2u);
	ASSERT_EQ(shapes[1]["owned-fields"][0]["elem"]["name"], "__pln_B16");
	ASSERT_EQ(shapes[1]["owned-fields"][1]["elem"]["name"], "__pln_B16");
}

TEST(sa, malloc_no_owned)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/078_malloc_no_owned.pa");
	ASSERT_TRUE(jout.is_object());

	// Point p; — no owned struct fields → malloc
	const auto& v = jout["statements"][0]["vars"][0];
	ASSERT_EQ(v["name"], "p");
	ASSERT_EQ(v["init"]["name"], "__pln_malloc");
	ASSERT_EQ(v["init"]["args"][0]["value"], "16");

	// No struct entries added to alloc-shapes
	ASSERT_TRUE(jout["alloc-shapes"].empty());
}

TEST(sa, struct_param)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/080_struct_param.pa"));
	ASSERT_TRUE(jout.is_object());

	// func __pln_free_Point(Point p) { free(p); }
	// parameters[0]: Point p → pntr(struct(Point))
	const auto& f = jout["functions"][0];
	ASSERT_EQ(funcName(f["name"]), "__pln_free_Point");

	const auto& param = f["parameters"][0];
	ASSERT_EQ(param["name"], "p");
	ASSERT_EQ(param["var-type"]["type-kind"], "pntr");
	ASSERT_EQ(param["var-type"]["base-type"]["type-name"], "Point");
}

TEST(sa, embed_field_chain)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/081_embed_field_chain.pa");
	ASSERT_TRUE(jout.is_object());

	// 10 -> l.a.x: $Point a@0, int64 x@0 → flatten: var="l", offset=0
	const auto& fa1 = jout["statements"][1];
	ASSERT_EQ(fa1["stmt-type"], "field-assign");
	ASSERT_EQ(fa1["var"],        "l");
	ASSERT_EQ(fa1["offset"],     0);

	// 20 -> l.b.y: $Point b@16, int64 y@8 → flatten: var="l", offset=24
	const auto& fa2 = jout["statements"][2];
	ASSERT_EQ(fa2["stmt-type"], "field-assign");
	ASSERT_EQ(fa2["var"],        "l");
	ASSERT_EQ(fa2["offset"],     24);

	// int64 v = l.a.x: field-access: var="l", offset=0
	const auto& rd = jout["statements"][3]["vars"][0]["init"];
	ASSERT_EQ(rd["expr-type"], "field-access");
	ASSERT_EQ(rd["var"],        "l");
	ASSERT_EQ(rd["offset"],     0);
}

TEST(sa, owned_field_chain)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/082_owned_field_chain.pa"));
	ASSERT_TRUE(jout.is_object());

	// 10 -> r.tl.x: Point tl@0, int64 x@0 → ptr-expr={var:"r",offset:0}, offset:0
	const auto& fa = jout["statements"][1];
	ASSERT_EQ(fa["stmt-type"],              "field-assign");
	ASSERT_EQ(fa["offset"],                 0);
	ASSERT_EQ(fa["ptr-expr"]["var"],         "r");
	ASSERT_EQ(fa["ptr-expr"]["offset"],      0);
	ASSERT_EQ(fa["ptr-expr"]["value-type"]["base-type"]["type-name"], "Point");

	// int64 v = r.tl.x: field-access with ptr-expr
	const auto& rd = jout["statements"][2]["vars"][0]["init"];
	ASSERT_EQ(rd["expr-type"],         "field-access");
	ASSERT_EQ(rd["offset"],             0);
	ASSERT_EQ(rd["ptr-expr"]["var"],    "r");
	ASSERT_EQ(rd["ptr-expr"]["offset"], 0);
}

TEST(sa, mixed_field_chain)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/084_mixed_chain.pa"));
	ASSERT_TRUE(jout.is_object());

	// int64 v = outer.sub.inner.x
	// Sub sub@0 (owned) → ptr-expr={var:"outer",offset:0,pntr(struct(Sub))}
	// $Inner inner@0 (embed, flatten) → outer offset stays 0
	// int64 x@0 → final offset: 0
	const auto& rd = jout["statements"][1]["vars"][0]["init"];
	ASSERT_EQ(rd["expr-type"],          "field-access");
	ASSERT_EQ(rd["offset"],             0);
	ASSERT_EQ(rd["ptr-expr"]["var"],    "outer");
	ASSERT_EQ(rd["ptr-expr"]["offset"], 0);
	ASSERT_EQ(rd["ptr-expr"]["value-type"]["base-type"]["type-name"], "Sub");
}

TEST(sa, embed_struct_arr_decl)
{
	// [4]$Point pts — contiguous 1D struct array: malloc(4 * 16), free at scope exit
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/098_embed_struct_arr.pa"));
	ASSERT_TRUE(jout.is_object());

	ASSERT_FALSE(jout["functions"].empty());
	const auto& body = jout["functions"][0]["body"];
	ASSERT_GE(body.size(), 2u);

	// body[0]: pts = malloc(4 * 16)
	ASSERT_EQ(body[0]["stmt-type"], "var-decl");
	ASSERT_EQ(body[0]["vars"][0]["name"], "pts");

	const auto& vt = body[0]["vars"][0]["var-type"];
	ASSERT_EQ(vt["type-kind"], "pntr");
	ASSERT_TRUE(vt.value("embedded", false));
	ASSERT_EQ(vt["stride"], 16);
	ASSERT_EQ(vt["base-type"]["type-kind"], "struct");
	ASSERT_EQ(vt["base-type"]["type-name"], "Point");

	const auto& init = body[0]["vars"][0]["init"];
	ASSERT_EQ(init["name"], "__pln_malloc");
	ASSERT_EQ(init["expr-type"], "call");
	ASSERT_EQ(init["args"][0]["expr-type"],          "mul");
	ASSERT_EQ(init["args"][0]["right"]["expr-type"], "lit-uint");
	ASSERT_EQ(init["args"][0]["right"]["value"],     "16");

	// body[1]: pts[0] of pts[0].x — arr-index, value-type pntr(struct(Point)), elem-size 16
	const auto& idx = body[1]["vars"][0]["init"]["ptr-expr"];
	ASSERT_EQ(idx["expr-type"], "arr-index");
	ASSERT_EQ(idx["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(idx["value-type"]["base-type"]["type-kind"], "struct");
	ASSERT_EQ(idx["value-type"]["base-type"]["type-name"], "Point");
	ASSERT_FALSE(idx["value-type"].contains("mutable"));
	ASSERT_EQ(idx["elem-size"]["expr-type"], "lit-uint");
	ASSERT_EQ(idx["elem-size"]["value"], "16");

	// free(pts), then the trailing return
	ASSERT_EQ(body.back()["stmt-type"], "return");
	const auto& last = body[body.size() - 2];
	ASSERT_EQ(last["stmt-type"], "expr");
	ASSERT_EQ(last["body"]["name"], "free");
	ASSERT_EQ(last["body"]["args"][0]["name"], "pts");
}

TEST(sa, owned_struct_arr_decl)
{
	// [2]Point pts — owned pointer array: alloc/free via palan functions
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/099_owned_struct_arr.pa"));
	ASSERT_TRUE(jout.is_object());

	// alloc-shapes: Point's layout, then the arr shape of its owned slots
	const auto& shapes = jout["alloc-shapes"];
	ASSERT_GE(shapes.size(), 2u);
	auto arr_it = find_if(shapes.begin(), shapes.end(), [](const json& s){
		return s.value("shape-kind","") == "arr";
	});
	ASSERT_NE(arr_it, shapes.end());
	ASSERT_EQ((*arr_it)["shape-key"], "arr_P9__pln_B16");
	ASSERT_EQ((*arr_it)["elem"], json({{"kind","struct"},{"key","P9__pln_B16"},{"name","__pln_B16"},{"size",8}}));

	auto struct_it = find_if(shapes.begin(), shapes.end(), [](const json& s){
		return s.value("shape-kind","") == "bytes";
	});
	ASSERT_NE(struct_it, shapes.end());
	ASSERT_EQ((*struct_it)["shape-name"], "__pln_B16");

	ASSERT_FALSE(jout["functions"].empty());
	const auto& body = jout["functions"][0]["body"];
	ASSERT_GE(body.size(), 2u);

	// body[0]: pts = __pln_alloc_arr_P9__pln_B16(2), the constant count passed as it is
	ASSERT_EQ(body[0]["stmt-type"], "var-decl");
	ASSERT_EQ(body[0]["vars"][0]["name"], "pts");
	const auto& vt = body[0]["vars"][0]["var-type"];
	ASSERT_EQ(vt["type-kind"], "pntr");
	ASSERT_EQ(vt["base-type"]["type-kind"], "pntr");
	ASSERT_EQ(vt["base-type"]["base-type"]["type-kind"], "struct");
	ASSERT_EQ(vt["base-type"]["base-type"]["type-name"], "Point");
	const auto& init = body[0]["vars"][0]["init"];
	ASSERT_EQ(init["expr-type"], "call");
	ASSERT_EQ(funcName(init["name"]),      "__pln_alloc_arr_P9__pln_B16");
	ASSERT_EQ(init["func-type"], "palan");
	ASSERT_EQ(init["args"][0]["value"], "2");

	// body[1]: pts[0] of pts[0].x — arr-index, value-type pntr(struct(Point)), elem-size 8
	const auto& idx = body[1]["vars"][0]["init"]["ptr-expr"];
	ASSERT_EQ(idx["expr-type"], "arr-index");
	ASSERT_EQ(idx["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(idx["value-type"]["base-type"]["type-kind"], "struct");
	ASSERT_EQ(idx["value-type"]["base-type"]["type-name"], "Point");
	ASSERT_FALSE(idx["value-type"].contains("mutable"));
	ASSERT_EQ(idx["elem-size"]["expr-type"], "lit-uint");
	ASSERT_EQ(idx["elem-size"]["value"], "8");

	// __pln_free_arr_P9__pln_B16(pts, 2), then the trailing return
	ASSERT_EQ(body.back()["stmt-type"], "return");
	const auto& last = body[body.size() - 2];
	ASSERT_EQ(last["stmt-type"], "expr");
	ASSERT_EQ(funcName(last["body"]["name"]),      "__pln_free_arr_P9__pln_B16");
	ASSERT_EQ(last["body"]["func-type"], "palan");
	ASSERT_EQ(last["body"]["args"][0]["name"], "pts");
	ASSERT_EQ(last["body"]["args"][1]["value"], "2");
}

TEST(sa, at_struct_arr_decl)
{
	// [4]@Point rpts — non-owning read-only pointer array: malloc(4 * 8), free at scope exit
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/100_at_struct_arr.pa"));
	ASSERT_TRUE(jout.is_object());

	ASSERT_FALSE(jout["functions"].empty());
	const auto& body = jout["functions"][0]["body"];
	ASSERT_GE(body.size(), 2u);

	// body[0]: rpts = malloc(4 * 8)
	ASSERT_EQ(body[0]["stmt-type"], "var-decl");
	ASSERT_EQ(body[0]["vars"][0]["name"], "rpts");

	const auto& vt = body[0]["vars"][0]["var-type"];
	ASSERT_EQ(vt["type-kind"], "pntr");
	const auto& elem_vt = vt["base-type"];
	ASSERT_EQ(elem_vt["type-kind"], "pntr");
	ASSERT_EQ(elem_vt["mutable"], false);
	ASSERT_EQ(elem_vt["base-type"]["type-kind"], "struct");
	ASSERT_EQ(elem_vt["base-type"]["type-name"], "Point");

	const auto& init = body[0]["vars"][0]["init"];
	ASSERT_EQ(init["name"], "__pln_malloc");
	ASSERT_EQ(init["expr-type"], "call");
	ASSERT_EQ(init["args"][0]["expr-type"],          "mul");
	ASSERT_EQ(init["args"][0]["right"]["expr-type"], "lit-uint");
	ASSERT_EQ(init["args"][0]["right"]["value"],     "8");

	// body[1]: rpts[0] of rpts[0].x — arr-index, value-type pntr(struct(Point), mutable:false), elem-size 8
	const auto& idx = body[1]["vars"][0]["init"]["ptr-expr"];
	ASSERT_EQ(idx["expr-type"], "arr-index");
	ASSERT_EQ(idx["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(idx["value-type"]["base-type"]["type-kind"], "struct");
	ASSERT_EQ(idx["value-type"]["base-type"]["type-name"], "Point");
	ASSERT_EQ(idx["value-type"]["mutable"], false);
	ASSERT_EQ(idx["elem-size"]["expr-type"], "lit-uint");
	ASSERT_EQ(idx["elem-size"]["value"], "8");

	// free(rpts), then the trailing return
	ASSERT_EQ(body.back()["stmt-type"], "return");
	const auto& last = body[body.size() - 2];
	ASSERT_EQ(last["stmt-type"], "expr");
	ASSERT_EQ(last["body"]["name"], "free");
	ASSERT_EQ(last["body"]["args"][0]["name"], "rpts");
}

TEST(sa, at_bang_struct_arr_decl)
{
	// [4]@!Point wpts — non-owning mutable write-through pointer array
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/102_at_bang_struct_arr.pa"));
	ASSERT_TRUE(jout.is_object());

	ASSERT_FALSE(jout["functions"].empty());
	const auto& body = jout["functions"][0]["body"];
	ASSERT_GE(body.size(), 2u);

	// body[0]: wpts = malloc(4 * 8)
	ASSERT_EQ(body[0]["stmt-type"], "var-decl");
	ASSERT_EQ(body[0]["vars"][0]["name"], "wpts");

	const auto& vt = body[0]["vars"][0]["var-type"];
	const auto& elem_vt = vt["base-type"];
	ASSERT_EQ(elem_vt["type-kind"], "pntr");
	ASSERT_EQ(elem_vt["mutable"], true);
	ASSERT_EQ(elem_vt["base-type"]["type-kind"], "struct");
	ASSERT_EQ(elem_vt["base-type"]["type-name"], "Point");

	// body[1]: wpts[0] of wpts[0].x — arr-index, value-type pntr(struct(Point), mutable:true), elem-size 8
	const auto& idx = body[1]["vars"][0]["init"]["ptr-expr"];
	ASSERT_EQ(idx["expr-type"], "arr-index");
	ASSERT_EQ(idx["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(idx["value-type"]["base-type"]["type-kind"], "struct");
	ASSERT_EQ(idx["value-type"]["base-type"]["type-name"], "Point");
	ASSERT_EQ(idx["value-type"]["mutable"], true);
	ASSERT_EQ(idx["elem-size"]["expr-type"], "lit-uint");
	ASSERT_EQ(idx["elem-size"]["value"], "8");

	// free(wpts), then the trailing return
	ASSERT_EQ(body.back()["stmt-type"], "return");
	const auto& last = body[body.size() - 2];
	ASSERT_EQ(last["stmt-type"], "expr");
	ASSERT_EQ(last["body"]["name"], "free");
	ASSERT_EQ(last["body"]["args"][0]["name"], "wpts");
}

TEST(sa, embed_struct_arr_field_access)
{
	// [4]$Point pts; 10 -> pts[0].x; 20 -> pts[0].y; printf("%ld %ld\n", pts[0].x, pts[0].y);
	// Covers: resolveObjectChain arr-index base case (embedded struct array)
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/103_embed_struct_arr_field.pa"));
	ASSERT_TRUE(jout.is_object());

	ASSERT_FALSE(jout["functions"].empty());
	const auto& body = jout["functions"][0]["body"];

	// body[1]: 10 -> pts[0].x — field-assign, ptr-expr = pts[0] (arr-index), offset 0
	const auto& fa_x = body[1];
	ASSERT_EQ(fa_x["stmt-type"], "field-assign");
	ASSERT_EQ(fa_x["offset"], 0);
	ASSERT_EQ(fa_x["value-type"]["type-name"], "int64");
	ASSERT_EQ(fa_x["value"]["value"], "10");
	const auto& pe_x = fa_x["ptr-expr"];
	ASSERT_EQ(pe_x["expr-type"], "arr-index");
	ASSERT_EQ(pe_x["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(pe_x["value-type"]["base-type"]["type-kind"], "struct");
	ASSERT_EQ(pe_x["value-type"]["base-type"]["type-name"], "Point");
	ASSERT_FALSE(pe_x["value-type"].contains("mutable"));
	ASSERT_EQ(pe_x["elem-size"]["value"], "16");

	// body[2]: 20 -> pts[0].y — field-assign, offset 8
	const auto& fa_y = body[2];
	ASSERT_EQ(fa_y["stmt-type"], "field-assign");
	ASSERT_EQ(fa_y["offset"], 8);
	ASSERT_EQ(fa_y["value"]["value"], "20");

	// body[3]: printf("%ld %ld\n", pts[0].x, pts[0].y) — field-access args, ptr-expr = arr-index
	const auto& args = body[3]["body"]["args"];
	ASSERT_EQ(args[1]["expr-type"], "field-access");
	ASSERT_EQ(args[1]["offset"], 0);
	ASSERT_EQ(args[1]["ptr-expr"]["expr-type"], "arr-index");
	ASSERT_EQ(args[2]["expr-type"], "field-access");
	ASSERT_EQ(args[2]["offset"], 8);
	ASSERT_EQ(args[2]["ptr-expr"]["expr-type"], "arr-index");
}

TEST(sa, owned_struct_arr_field_access)
{
	// [2]Point pts; 5 -> pts[0].x; printf("%ld\n", pts[0].x);
	// Covers: resolveObjectChain arr-index base case (owned pointer array)
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/104_owned_struct_arr_field.pa"));
	ASSERT_TRUE(jout.is_object());

	ASSERT_FALSE(jout["functions"].empty());
	const auto& body = jout["functions"][0]["body"];

	// body[1]: 5 -> pts[0].x — field-assign, ptr-expr = pts[0] (arr-index), elem-size 8
	const auto& fa = body[1];
	ASSERT_EQ(fa["stmt-type"], "field-assign");
	ASSERT_EQ(fa["offset"], 0);
	ASSERT_EQ(fa["value-type"]["type-name"], "int64");
	const auto& pe = fa["ptr-expr"];
	ASSERT_EQ(pe["expr-type"], "arr-index");
	ASSERT_EQ(pe["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(pe["value-type"]["base-type"]["type-name"], "Point");
	ASSERT_FALSE(pe["value-type"].contains("mutable"));
	ASSERT_EQ(pe["elem-size"]["value"], "8");

	// body[2]: printf("%ld\n", pts[0].x) — field-access, ptr-expr = arr-index
	const auto& fa_read = body[2]["body"]["args"][1];
	ASSERT_EQ(fa_read["expr-type"], "field-access");
	ASSERT_EQ(fa_read["offset"], 0);
	ASSERT_EQ(fa_read["ptr-expr"]["expr-type"], "arr-index");
}

TEST(sa, at_bang_struct_arr_field_write)
{
	// Point p; [4]@!Point wpts; p -> wpts[0]; 42 -> wpts[0].x; printf("%ld\n", p.x);
	// Covers: resolveObjectChain(forWrite=true) arr-index base case, mutable:true (write-through allowed)
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/105_at_bang_struct_arr_field.pa"));
	ASSERT_TRUE(jout.is_object());

	ASSERT_FALSE(jout["functions"].empty());
	const auto& body = jout["functions"][0]["body"];

	// body[3]: 42 -> wpts[0].x — field-assign, ptr-expr = wpts[0] (arr-index, mutable:true)
	const auto& fa = body[3];
	ASSERT_EQ(fa["stmt-type"], "field-assign");
	ASSERT_EQ(fa["offset"], 0);
	ASSERT_EQ(fa["value"]["value"], "42");
	const auto& pe = fa["ptr-expr"];
	ASSERT_EQ(pe["expr-type"], "arr-index");
	ASSERT_EQ(pe["value-type"]["base-type"]["type-name"], "Point");
	ASSERT_EQ(pe["value-type"]["mutable"], true);
}

TEST(sa, at_struct_arr_field_read)
{
	// Point p; [4]@Point rpts; p -> rpts[0]; printf("%ld\n", rpts[0].x);
	// Covers: resolveObjectChain arr-index base case, mutable:false (read-only, read allowed)
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/106_at_struct_arr_field_read.pa"));
	ASSERT_TRUE(jout.is_object());

	ASSERT_FALSE(jout["functions"].empty());
	const auto& body = jout["functions"][0]["body"];

	// body[3]: printf("%ld\n", rpts[0].x) — field-access, ptr-expr = rpts[0] (arr-index, mutable:false)
	const auto& fa = body[3]["body"]["args"][1];
	ASSERT_EQ(fa["expr-type"], "field-access");
	ASSERT_EQ(fa["offset"], 0);
	const auto& pe = fa["ptr-expr"];
	ASSERT_EQ(pe["expr-type"], "arr-index");
	ASSERT_EQ(pe["value-type"]["base-type"]["type-name"], "Point");
	ASSERT_EQ(pe["value-type"]["mutable"], false);
}

TEST(sa, embed_prim_arr_field)
{
	// type Buf { [4]$int64 data; }; Buf buf;
	// Covers: buildStructDef "arr" branch, embedded primitive-leaf case (embed-arr typeKind)
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/107_embed_prim_arr_field.pa");
	ASSERT_TRUE(jout.is_object());

	// Buf buf; -> malloc(32): confirms totalSize == 4*8 == 32 and the plain malloc path
	// (ownsFields stays false for embed-arr-only structs).
	const auto& v = jout["statements"][0]["vars"][0];
	ASSERT_EQ(v["name"], "buf");
	ASSERT_EQ(v["init"]["name"], "__pln_malloc");
	ASSERT_EQ(v["init"]["args"][0]["value"], "32");
}

TEST(sa, embed_struct_arr_field)
{
	// type Point { int64 x; int64 y; }; type Polygon { [4]$Point pts; }; Polygon poly;
	// Covers: buildStructDef "arr" branch, embedded struct-leaf case (embed-arr typeKind, elemKind=="struct")
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/108_embed_struct_arr_field.pa");
	ASSERT_TRUE(jout.is_object());

	// Polygon poly; -> malloc(64): confirms totalSize == 4*Point.totalSize(16) == 64
	// and the plain malloc path (ownsFields stays false for embed-arr-only structs).
	const auto& v = jout["statements"][0]["vars"][0];
	ASSERT_EQ(v["name"], "poly");
	ASSERT_EQ(v["init"]["name"], "__pln_malloc");
	ASSERT_EQ(v["init"]["args"][0]["value"], "64");
}

TEST(sa, embed_ptr_arr_field)
{
	// type Point { int64 x; int64 y; }; type Ring { [4]@!Point nodes; }; Ring r;
	// Covers: buildStructDef "arr" branch, non-embedded pntr-wrapped base-type case
	// (embed-ptr-arr typeKind, isMutable==true, elemKind=="struct")
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/109_embed_ptr_arr_field.pa");
	ASSERT_TRUE(jout.is_object());

	// Ring r; -> malloc(32): confirms totalSize == 4*8 == 32 (4 pointer slots)
	// and the plain malloc path (ownsFields stays false for embed-ptr-arr-only structs).
	const auto& v = jout["statements"][0]["vars"][0];
	ASSERT_EQ(v["name"], "r");
	ASSERT_EQ(v["init"]["name"], "__pln_malloc");
	ASSERT_EQ(v["init"]["args"][0]["value"], "32");
}

TEST(sa, embed_ptr_arr_field_readonly)
{
	// type Point { int64 x; int64 y; }; type Watch { [3]@Point observed; }; Watch w;
	// Covers: buildStructDef "arr" branch, embed-ptr-arr typeKind, isMutable==false
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/110_embed_ptr_arr_field_readonly.pa");
	ASSERT_TRUE(jout.is_object());

	// Watch w; -> malloc(24): confirms totalSize == 3*8 == 24 (3 pointer slots)
	const auto& v = jout["statements"][0]["vars"][0];
	ASSERT_EQ(v["name"], "w");
	ASSERT_EQ(v["init"]["name"], "__pln_malloc");
	ASSERT_EQ(v["init"]["args"][0]["value"], "24");
}

TEST(sa, embed_ptr_arr_field_prim_leaf)
{
	// type Counter { [2]@!int64 slots; }; Counter c;
	// Covers: buildStructDef "arr" branch, embed-ptr-arr typeKind, elemKind=="prim"
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/111_embed_ptr_arr_field_prim_leaf.pa");
	ASSERT_TRUE(jout.is_object());

	// Counter c; -> malloc(16): confirms totalSize == 2*8 == 16 (2 pointer slots)
	const auto& v = jout["statements"][0]["vars"][0];
	ASSERT_EQ(v["name"], "c");
	ASSERT_EQ(v["init"]["name"], "__pln_malloc");
	ASSERT_EQ(v["init"]["args"][0]["value"], "16");
}

TEST(sa, owned_prim_arr_field)
{
	// type Bucket { [3]int64 vals; }; Bucket b;
	// Covers: buildStructDef "arr" branch, non-embedded non-pntr-wrapped primitive-leaf
	// case (an owned field), non-malloc path, recordAllocShape's
	// row element for it.
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/112_owned_prim_arr_field.pa"));
	ASSERT_TRUE(jout.is_object());

	// Bucket b; -> __pln_alloc_Bucket() (not malloc): an owned array field
	// alone takes the struct off the plain malloc path.
	const auto& v = jout["statements"][0]["vars"][0];
	ASSERT_EQ(v["name"], "b");
	ASSERT_EQ(funcName(v["init"]["name"]), "__pln_alloc_Bucket");
	ASSERT_EQ(v["init"]["func-type"], "palan");
	ASSERT_TRUE(v["init"]["args"].empty());

	const auto& shapes = jout["alloc-shapes"];
	ASSERT_EQ(shapes.size(), 1u);
	ASSERT_EQ(shapes[0]["shape-name"], "Bucket");
	ASSERT_EQ(shapes[0]["shape-kind"], "struct");
	ASSERT_EQ(shapes[0]["total-size"], 8);
	const auto& owned = shapes[0]["owned-fields"];
	ASSERT_EQ(owned.size(), 1u);
	ASSERT_EQ(owned[0]["name"], "vals");
	ASSERT_EQ(owned[0]["offset"], 0);
	ASSERT_EQ(owned[0]["elem"]["kind"], "row");
	ASSERT_EQ(owned[0]["elem"]["elem"]["type"], "int64");
	ASSERT_EQ(owned[0]["lens"], json::array({3}));
}

TEST(sa, owned_struct_arr_field)
{
	// type Point { int64 x; int64 y; }; type Cluster { [4]Point pts; }; Cluster c;
	// Covers: buildStructDef "arr" branch, non-embedded case, struct-leaf owned
	// pointer array field: a row of struct elements, whose helpers come with
	// the struct's, so no "arr" shape is registered.
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/113_owned_struct_arr_field.pa"));
	ASSERT_TRUE(jout.is_object());

	// Cluster c; -> __pln_alloc_Cluster() (not malloc)
	const auto& v = jout["statements"][0]["vars"][0];
	ASSERT_EQ(v["name"], "c");
	ASSERT_EQ(funcName(v["init"]["name"]), "__pln_alloc_Cluster");
	ASSERT_EQ(v["init"]["func-type"], "palan");
	ASSERT_TRUE(v["init"]["args"].empty());

	const auto& shapes = jout["alloc-shapes"];
	ASSERT_EQ(shapes.size(), 2u);

	auto point_it = find_if(shapes.begin(), shapes.end(), [](const json& s){
		return s.value("shape-kind","") == "bytes" && s.value("shape-name","") == "__pln_B16";
	});
	ASSERT_NE(point_it, shapes.end());

	auto cluster_it = find_if(shapes.begin(), shapes.end(), [](const json& s){
		return s.value("shape-kind","") == "struct" && s.value("shape-name","") == "Cluster";
	});
	ASSERT_NE(cluster_it, shapes.end());
	ASSERT_EQ((*cluster_it)["total-size"], 8);
	const auto& owned = (*cluster_it)["owned-fields"];
	ASSERT_EQ(owned.size(), 1u);
	ASSERT_EQ(owned[0]["name"], "pts");
	ASSERT_EQ(owned[0]["offset"], 0);
	ASSERT_EQ(owned[0]["elem"]["kind"], "row");
	ASSERT_EQ(owned[0]["elem"]["elem"]["kind"], "struct");
	ASSERT_EQ(owned[0]["elem"]["elem"]["name"], "__pln_B16");
	ASSERT_EQ(owned[0]["lens"], json::array({4}));
}

TEST(sa, embed_prim_arr_struct_field_access)
{
	// type Buf { $[4]int64 data; }; Buf buf; 10->buf.data[0]; printf(buf.data[0],buf.data[1]);
	// Covers: sa_expr_arr_index's 1D primitive embedded array field branch.
	// The "array" (buf.data) must be addr-only (FieldAccessExpr computes ptr+offset,
	// not a load), and the arr-index result itself must be a plain scalar (not pntr)
	// so the element is actually loaded (DerefLoadIdx), not just addressed.
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/114_embed_prim_arr_struct_field_access.pa");
	ASSERT_TRUE(jout.is_object());

	ASSERT_FALSE(jout["functions"].empty());
	const auto& body = jout["functions"][0]["body"];

	const auto& target = body[1]["target"];
	ASSERT_EQ(target["expr-type"], "arr-index");
	ASSERT_EQ(target["value-type"]["type-kind"], "prim");
	ASSERT_EQ(target["value-type"]["type-name"], "int64");
	ASSERT_EQ(target["elem-size"]["value"], "8");

	const auto& arr = target["array"];
	ASSERT_EQ(arr["expr-type"], "field-access");
	ASSERT_EQ(arr["addr-only"], true);
	// Typed as a `[4]int64` variable is.
	ASSERT_EQ(arr["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(arr["value-type"]["arr-size"], 4);
	ASSERT_FALSE(arr["value-type"].contains("embedded"));
	ASSERT_EQ(arr["value-type"]["base-type"]["type-kind"], "prim");
	ASSERT_EQ(arr["value-type"]["base-type"]["type-name"], "int64");

	// printf args: buf.data[0], buf.data[1] — both scalar arr-index reads
	const auto& args = body[3]["body"]["args"];
	ASSERT_EQ(args[1]["value-type"]["type-kind"], "prim");
	ASSERT_EQ(args[2]["value-type"]["type-kind"], "prim");
}

TEST(sa, embed_struct_arr_struct_field_access)
{
	// type Point{...}; type Polygon { [4]$Point pts; }; Polygon poly;
	// 10->poly.pts[0].x; 20->poly.pts[0].y; printf(poly.pts[0].x, poly.pts[0].y);
	// Covers: FieldAccessExpr addr-only fix for embed-arr struct leaf (previously
	// segfaulted at runtime — DerefLoad was reading raw struct bytes as a pointer
	// instead of computing poly_ptr+offset). Field-assign/field-access chain through
	// the arr-index base (poly.pts[0]) must resolve the same way the existing
	// variable-level embedded struct array (embed_struct_arr_field_access) does.
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/115_embed_struct_arr_struct_field_access.pa"));
	ASSERT_TRUE(jout.is_object());

	ASSERT_FALSE(jout["functions"].empty());
	const auto& body = jout["functions"][0]["body"];

	const auto& fa_x = body[1];
	ASSERT_EQ(fa_x["stmt-type"], "field-assign");
	ASSERT_EQ(fa_x["offset"], 0);
	const auto& pe_x = fa_x["ptr-expr"];
	ASSERT_EQ(pe_x["expr-type"], "arr-index");
	ASSERT_EQ(pe_x["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(pe_x["value-type"]["base-type"]["type-name"], "Point");
	ASSERT_EQ(pe_x["elem-size"]["value"], "16");

	const auto& arr = pe_x["array"];
	ASSERT_EQ(arr["expr-type"], "field-access");
	ASSERT_EQ(arr["addr-only"], true);
	ASSERT_EQ(arr["value-type"]["embedded"], true);
	ASSERT_EQ(arr["value-type"]["stride"], 16);
	ASSERT_EQ(arr["value-type"]["base-type"]["type-kind"], "struct");

	const auto& fa_y = body[2];
	ASSERT_EQ(fa_y["offset"], 8);
}

TEST(sa, owned_prim_arr_struct_field_access)
{
	// type Bucket { [3]int64 vals; }; Bucket b; 1->b.vals[0]; printf(b.vals[0]);
	// Covers: fieldValueType owned array field, primitive leaf — base-type must be
	// {"type-kind":"prim","type-name":"int64"}, not "struct" (the previous default
	// branch tagged every owned array leaf as a struct, which happened to be harmless
	// for codegen by luck but was schema-incorrect).
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/116_owned_prim_arr_struct_field_access.pa");
	ASSERT_TRUE(jout.is_object());

	ASSERT_FALSE(jout["functions"].empty());
	const auto& body = jout["functions"][0]["body"];

	const auto& target = body[1]["target"];
	ASSERT_EQ(target["value-type"]["type-kind"], "prim");
	ASSERT_EQ(target["value-type"]["type-name"], "int64");

	const auto& arr = target["array"];
	ASSERT_EQ(arr["addr-only"], false);
	ASSERT_FALSE(arr["value-type"].contains("embedded"));
	ASSERT_EQ(arr["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(arr["value-type"]["base-type"]["type-kind"], "prim");
	ASSERT_EQ(arr["value-type"]["base-type"]["type-name"], "int64");
}

TEST(sa, owned_struct_arr_struct_field_access)
{
	// type Point{...}; type Cluster { [4]Point pts; }; Cluster c;
	// 5->c.pts[0].x; printf(c.pts[0].x);
	// Covers: fieldValueType owned array field, struct leaf — field must be double-wrapped
	// pntr(pntr(struct)) matching the variable-level owned struct array shape
	// (sa_owned_struct_arr_var_decl), so pts[i] yields pntr(struct) (a real stored
	// pointer, loaded via DerefLoadIdx) rather than the bare struct type the old
	// default branch produced (which broke resolveObjectChain's arr-index case).
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/117_owned_struct_arr_struct_field_access.pa"));
	ASSERT_TRUE(jout.is_object());

	ASSERT_FALSE(jout["functions"].empty());
	const auto& body = jout["functions"][0]["body"];

	const auto& fa = body[1];
	ASSERT_EQ(fa["stmt-type"], "field-assign");
	ASSERT_EQ(fa["offset"], 0);
	const auto& pe = fa["ptr-expr"];
	ASSERT_EQ(pe["expr-type"], "arr-index");
	ASSERT_EQ(pe["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(pe["value-type"]["base-type"]["type-name"], "Point");
	ASSERT_EQ(pe["elem-size"]["value"], "8");

	const auto& arr = pe["array"];
	ASSERT_EQ(arr["addr-only"], false);
	ASSERT_FALSE(arr["value-type"].contains("embedded"));
	ASSERT_EQ(arr["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(arr["value-type"]["base-type"]["type-kind"], "pntr");
	ASSERT_EQ(arr["value-type"]["base-type"]["base-type"]["type-name"], "Point");
}

TEST(sa, embed_ptr_arr_struct_field_access)
{
	// type Point{...}; type Ring { [4]@!Point nodes; }; Point p; 99->p.x; Ring r;
	// p->r.nodes[0]; printf(r.nodes[0].x);
	// Covers: FieldAccessExpr addr-only fix for embed-ptr-arr (previously segfaulted
	// on the write `p -> r.nodes[0];` — DerefLoad on the freshly-allocated "nodes"
	// field read back 0 instead of computing r_ptr+offset, so the store target
	// address collapsed to NULL). Also covers write-through then read-back of the
	// stored pointer's field.
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/118_embed_ptr_arr_struct_field_access.pa"));
	ASSERT_TRUE(jout.is_object());

	ASSERT_FALSE(jout["functions"].empty());
	const auto& body = jout["functions"][0]["body"];

	// body[3]: p -> r.nodes[0]; — arr-assign, target.array is addr-only (embed-ptr-arr)
	const auto& target = body[3]["target"];
	ASSERT_EQ(target["expr-type"], "arr-index");
	ASSERT_EQ(target["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(target["value-type"]["base-type"]["type-name"], "Point");
	ASSERT_EQ(target["value-type"]["mutable"], true);

	const auto& arr = target["array"];
	ASSERT_EQ(arr["expr-type"], "field-access");
	ASSERT_EQ(arr["addr-only"], true);
	ASSERT_FALSE(arr["value-type"].contains("embedded"));
	ASSERT_EQ(arr["value-type"]["base-type"]["type-kind"], "pntr");
	ASSERT_EQ(arr["value-type"]["base-type"]["mutable"], true);
	ASSERT_EQ(arr["value-type"]["base-type"]["base-type"]["type-name"], "Point");

	// body[4]: printf("%ld\n", r.nodes[0].x) — field-access on the arr-index result
	const auto& fa_read = body[4]["body"]["args"][1];
	ASSERT_EQ(fa_read["expr-type"], "field-access");
	ASSERT_EQ(fa_read["offset"], 0);
	ASSERT_EQ(fa_read["ptr-expr"]["expr-type"], "arr-index");
}

TEST(sa, field_arr_readonly_ptr_slot)
{
	// type Point{...}; type Watch { [3]@Point observed; }; Point p; 99->p.x; Watch w;
	// p->w.observed[0]; printf(w.observed[0].x);
	// Covers: same embed-ptr-arr shape as embed_ptr_arr_struct_field_access
	// (118) but with the non-mutable `@T` slot instead of `@!T`, confirming that
	// assignment into the pointer slot itself is unaffected by the mutable flag
	// (only write-through to the pointee's fields is restricted; see
	// write_readonly_arr_field_elem for the rejected case).
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/119_field_arr_readonly_ptr_slot.pa"));
	ASSERT_TRUE(jout.is_object());

	ASSERT_FALSE(jout["functions"].empty());
	const auto& body = jout["functions"][0]["body"];

	// body[3]: p -> w.observed[0]; — arr-assign is allowed regardless of mutable flag
	const auto& target = body[3]["target"];
	ASSERT_EQ(target["expr-type"], "arr-index");
	ASSERT_EQ(target["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(target["value-type"]["base-type"]["type-name"], "Point");
	ASSERT_EQ(target["value-type"]["mutable"], false);

	// body[4]: printf("%ld\n", w.observed[0].x) — read-through is allowed too
	const auto& fa_read = body[4]["body"]["args"][1];
	ASSERT_EQ(fa_read["expr-type"], "field-access");
	ASSERT_EQ(fa_read["offset"], 0);
	ASSERT_EQ(fa_read["ptr-expr"]["expr-type"], "arr-index");
	ASSERT_EQ(fa_read["ptr-expr"]["value-type"]["mutable"], false);
}

TEST(sa, struct_type_alias)
{
	// `type Point {...}; type PT = Point; PT p;` -- prerequisite
	// bug: sa_var_decl's dispatch used to key off the raw var-type's
	// type-kind/type-name, so a struct reached only through an alias name
	// ("PT") never matched structDefs_.count() and silently fell through to
	// the plain scalar var-decl path, which declares the variable but never
	// allocates its storage. The alias is resolved before dispatch, so this
	// reaches sa_struct_var_decl the same way a direct `Point p;` would.
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/160_struct_type_alias.pa"));
	ASSERT_TRUE(jout.is_object());

	const auto& decl = jout["statements"][0];
	ASSERT_EQ(decl["stmt-type"], "var-decl");
	const auto& v = decl["vars"][0];
	ASSERT_EQ(v["name"], "p");
	ASSERT_EQ(v["var-type"]["type-kind"], "pntr");
	ASSERT_EQ(v["var-type"]["base-type"]["type-kind"], "struct");
	ASSERT_EQ(v["var-type"]["base-type"]["type-name"], "Point");
	// init: malloc(16) -- proof storage is actually allocated, not just a
	// bare pointer-typed variable with no backing memory.
	ASSERT_EQ(v["init"]["expr-type"], "call");
	ASSERT_EQ(v["init"]["name"], "__pln_malloc");
	ASSERT_EQ(v["init"]["args"][0]["value"], "16");
}

TEST(sa, struct_field_type_alias)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/198_struct_field_type_alias.pa"));
	ASSERT_TRUE(jout.is_object());
	const auto& st = jout["statements"];

	// S { I a; $PT e; @PT p; [2]$PT ea; }: 4 + pad 4 + 16 + 8 + 32
	ASSERT_EQ(st[0]["vars"][0]["init"]["args"][0]["value"], "64");
	ASSERT_EQ(st[2]["offset"], 0);   // s.a
	ASSERT_EQ(st[2]["value-type"]["type-name"], "int32");
	ASSERT_EQ(st[3]["offset"], 16);  // s.e.y
	ASSERT_EQ(st[4]["offset"], 24);  // s.p
	ASSERT_EQ(st[4]["value-type"]["base-type"]["type-name"], "Point");
	ASSERT_EQ(st[5]["ptr-expr"]["array"]["offset"], 32);  // s.ea
	ASSERT_EQ(st[6]["vars"][0]["init"]["value-type"]["type-name"], "int32");
}

TEST(sa, struct_borrow)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/202_struct_borrow.pa");
	ASSERT_TRUE(jout.is_object());

	vector<json> args;
	for (auto& st : jout["statements"])
		if (st["stmt-type"] == "expr" && funcName(st["body"]["name"]) == "getv")
			args.push_back(st["body"]["args"][0]);
	ASSERT_EQ(args.size(), 4u);

	// '@s' borrows the struct pointer itself: an id, not an address of its slot.
	ASSERT_EQ(args[0]["expr-type"], "id");
	ASSERT_EQ(args[0]["value-type"]["type-kind"], "pntr");
	ASSERT_EQ(args[0]["value-type"]["mutable"], false);
	ASSERT_EQ(args[0]["value-type"]["base-type"]["type-kind"], "struct");
	ASSERT_FALSE(args[0]["var-type"].contains("mutable"));

	// An owned struct field loads the pointer it holds.
	ASSERT_EQ(args[1]["expr-type"], "field-access");
	ASSERT_EQ(args[1]["addr-only"], false);
	ASSERT_EQ(args[1]["value-type"]["mutable"], false);

	ASSERT_EQ(args[2]["expr-type"], "arr-index");
	ASSERT_EQ(args[2]["value-type"]["mutable"], false);

	// '@mk()' borrows the returned struct, written into a frame temp for the
	// call; it owns nothing to release.
	ASSERT_EQ(args[3]["value"]["value-type"]["mutable"], false);
	ASSERT_EQ(tempInit(args[3])["expr-type"], "frame-alloc");
	ASSERT_EQ(args[3]["body"][1]["body"]["ret-dest"]["name"], args[3]["value"]["name"]);
	ASSERT_EQ(args[3]["release"].size(), 0u);

	vector<json> setArgs;
	for (auto& st : jout["statements"])
		if (st["stmt-type"] == "expr" && funcName(st["body"]["name"]) == "setv")
			setArgs.push_back(st["body"]["args"][0]);
	// 'setv(@!mk())' and the method form 'mk().setv()'.
	ASSERT_EQ(setArgs.size(), 2u);
	for (auto& a : setArgs) {
		ASSERT_EQ(a["value"]["value-type"]["mutable"], true);
		ASSERT_EQ(tempInit(a)["expr-type"], "frame-alloc");
	}
}

TEST(sa, struct_arr_2d)
{
	json jout = written(run_sa("../test/testdata/sa/204_struct_arr_2d.pa"));
	ASSERT_TRUE(jout.is_object());

	// An array of rows of owned structs, the row's length left to the helpers.
	json kinds = json::array();
	for (auto& s : jout["alloc-shapes"])
		kinds.push_back(s["shape-kind"]);
	ASSERT_EQ(kinds, json::array({"bytes", "arr"}));
	const json& shape = jout["alloc-shapes"][1];
	ASSERT_EQ(shape["shape-key"], "arr_PP8__pln_B8");
	ASSERT_EQ(shape["elem"]["kind"], "row");
	ASSERT_EQ(shape["elem"]["elem"]["kind"], "struct");

	const json& stmts = jout["statements"];
	const json& m = stmts[0]["vars"][0];
	ASSERT_EQ(m["name"], "m");
	ASSERT_EQ(funcName(m["init"]["name"]), "__pln_alloc_arr_PP8__pln_B8");
	ASSERT_EQ(m["init"]["args"].size(), 2);
	json point = {{"type-kind","struct"},{"type-name","Point"}};
	ASSERT_EQ(m["var-type"]["base-type"]["base-type"]["base-type"], point);
	ASSERT_EQ(stmts[1]["stmt-type"], "field-assign");

	const json& rowsElem = stmts[2]["vars"][0]["var-type"]["base-type"];
	ASSERT_EQ(rowsElem["base-type"]["base-type"], point);
}

TEST(sa, embed_struct_arr_2d)
{
	json jout = written(run_sa("../test/testdata/sa/209_embed_struct_arr_2d.pa"));
	ASSERT_TRUE(jout.is_object());

	// Each row is a block of n structs laid out in place.
	json kinds = json::array();
	for (auto& s : jout["alloc-shapes"])
		kinds.push_back(s["shape-kind"]);
	ASSERT_EQ(kinds, json::array({"bytes", "arr"}));
	ASSERT_EQ(jout["alloc-shapes"][1]["shape-key"], "arr_ES8_P8__pln_B8");
	ASSERT_EQ(jout["alloc-shapes"][1]["elem"]["elem"]["kind"], "embed-struct");

	const json& m = jout["statements"][0]["vars"][0];
	ASSERT_EQ(m["name"], "m");
	ASSERT_EQ(funcName(m["init"]["name"]), "__pln_alloc_arr_ES8_P8__pln_B8");
	ASSERT_EQ(m["init"]["args"][1]["value"], "3");

	json row = {{"type-kind","pntr"},{"embedded",true},{"stride",8},{"arr-size",3},
	            {"base-type",{{"type-kind","struct"},{"type-name","Point"}}}};
	json expected = {{"type-kind","pntr"},{"arr-size",2},{"base-type",row}};
	ASSERT_EQ(m["var-type"], expected);
	ASSERT_EQ(jout["statements"][1]["stmt-type"], "field-assign");
}

TEST(sa, struct_arr_lit)
{
	json jout = run_sa("../test/testdata/sa/205_struct_arr_lit.pa");
	ASSERT_TRUE(jout.is_object());

	// (offset, literal) of the field assignments that follow each array's declaration.
	map<string, vector<pair<int, string>>> assigns;
	string cur;
	for (auto& st : jout["statements"]) {
		if (st["stmt-type"] == "var-decl") {
			cur = st["vars"][0]["name"];
			continue;
		}
		ASSERT_EQ(st["stmt-type"], "field-assign");
		assigns[cur].push_back({st["offset"], st["value"]["value"]});
	}

	// A struct value written in field order or by name lowers identically.
	ASSERT_EQ(assigns["p1"], (vector<pair<int, string>>{{0, "1"}, {4, "2"}}));
	ASSERT_EQ(assigns["p1"], assigns["p2"]);
	ASSERT_EQ(assigns["l1"].size(), 4);
	ASSERT_EQ(assigns["l1"], assigns["l2"]);
	ASSERT_EQ(assigns["m"].size(), 8);
	ASSERT_EQ(assigns["m"][7], (pair<int, string>{4, "7"}));
}

TEST(sa, struct_arr_borrow)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/206_struct_arr_borrow.pa"));
	ASSERT_TRUE(jout.is_object());

	// The element stays the struct's storage (no "mutable"): only the array
	// level carries the borrow's permission.
	json elem = {{"type-kind","pntr"},{"base-type",{{"type-kind","struct"},{"type-name","P"}}}};
	const auto& pt = jout["functions"][0]["parameters"][0]["var-type"];
	ASSERT_EQ(pt["arr-size"], 2);
	ASSERT_EQ(pt["mutable"], true);
	ASSERT_EQ(pt["base-type"], elem);

	const json* call = nullptr;
	for (auto& st : jout["statements"])
		if (st["stmt-type"] == "expr") call = &st["body"];
	ASSERT_NE(call, nullptr);
	const auto& at = (*call)["args"][0]["value-type"];
	ASSERT_EQ(at["arr-size"], 2);
	ASSERT_EQ(at["mutable"], true);
	ASSERT_EQ(at["base-type"], elem);
}

TEST(sa, embed_struct_arr_borrow)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/207_embed_struct_arr_borrow.pa"));
	ASSERT_TRUE(jout.is_object());

	// The borrow keeps the variable's contiguous layout: the structs sit in
	// the array itself, so its base is the struct, not a pointer to one.
	json expected = {{"type-kind","pntr"},{"embedded",true},{"stride",8},{"arr-size",2},
	                 {"mutable",true},{"base-type",{{"type-kind","struct"},{"type-name","P"}}}};
	ASSERT_EQ(jout["functions"][0]["parameters"][0]["var-type"], expected);

	const json* call = nullptr;
	for (auto& st : jout["statements"])
		if (st["stmt-type"] == "expr") call = &st["body"];
	ASSERT_NE(call, nullptr);
	ASSERT_EQ((*call)["args"][0]["value-type"], expected);
}

TEST(sa, tapple_struct)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/214_tapple_struct.pa");
	ASSERT_TRUE(jout.is_object());

	// A returned struct is written into the variable's storage, allocated ahead
	// of the call, and freed at scope exit.
	size_t b = 0;
	while (jout["statements"][b]["stmt-type"] != "block") b++;
	const json& stmts = jout["statements"][b]["body"];
	ASSERT_EQ(stmts[0]["vars"][0]["name"], "s");
	ASSERT_EQ(stmts[0]["vars"][0]["init"]["name"], "__pln_malloc");
	ASSERT_EQ(stmts[1]["stmt-type"], "tapple-decl");
	ASSERT_EQ(stmts[1]["vars"][0]["ret-dest"]["name"], "s");
	ASSERT_EQ(stmts[1]["vars"][1]["var-name"], "c");
	const json& last = stmts.back();
	ASSERT_EQ(last["body"]["name"], "free");
	ASSERT_EQ(last["body"]["args"][0]["name"], "s");

	// The temps of `f() -> (s, n)` live only in its block: s is copied, then the temp is freed.
	const json& body = stmts[stmts.size() - 2]["body"];
	string t0 = body[0]["vars"][0]["name"];
	ASSERT_EQ(body[1]["vars"][0]["ret-dest"]["name"], t0);
	ASSERT_EQ(body[2]["body"]["name"], "memcpy");
	ASSERT_EQ(body.back()["body"]["name"], "free");
	ASSERT_EQ(body.back()["body"]["args"][0]["name"], t0);
}

TEST(sa, field_transfer) {
	json jout = written(run_sa("../test/testdata/sa/215_field_transfer.pa"));
	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts.size(), 5);
	// The field's previous array is freed before it takes over pa.
	ASSERT_EQ(funcName(stmts[2]["body"]["name"]), "__pln_free_arr_P8__pln_B4");
	ASSERT_EQ(stmts[2]["body"]["args"][0]["expr-type"], "field-access");
	ASSERT_EQ(stmts[2]["body"]["args"][1]["value"], "2");
	ASSERT_EQ(stmts[3]["stmt-type"], "field-assign");
	ASSERT_EQ(stmts[3]["value"]["name"], "pa");
	ASSERT_EQ(stmts[4]["stmt-type"], "assign");
	ASSERT_EQ(stmts[4]["name"], "pa");
	ASSERT_EQ(stmts[4]["value"]["value"], "0");
}

TEST(sa, transfer_from_slot) {
	json jout = run_sa("../test/testdata/sa/218_transfer_from_slot.pa");
	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts.size(), 9);
	ASSERT_EQ(stmts[4]["value"]["var"], "v");
	ASSERT_EQ(stmts[5]["stmt-type"], "field-assign");
	ASSERT_EQ(stmts[5]["var"], "v");
	ASSERT_EQ(stmts[5]["value"]["value"], "0");
	ASSERT_EQ(stmts[7]["value"]["expr-type"], "arr-index");
	ASSERT_EQ(stmts[8]["stmt-type"], "arr-assign");
	ASSERT_EQ(stmts[8]["target"]["array"]["name"], "s");
	ASSERT_EQ(stmts[8]["target"]["index"]["value"], "1");
	ASSERT_EQ(stmts[8]["value"]["value"], "0");
}

TEST(sa, struct_ret_copy) {
	json jout = run_sa("../test/testdata/sa/216_struct_ret_copy.pa");
	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts.size(), 2);
	// The returned struct is written into a frame temp and copied into s; it
	// owns nothing to release.
	const auto& blk = stmts[1];
	ASSERT_EQ(blk["stmt-type"], "block");
	const auto& body = blk["body"];
	ASSERT_EQ(body.size(), 3);
	ASSERT_EQ(body[0]["stmt-type"], "var-decl");
	string temp = body[0]["vars"][0]["name"];
	ASSERT_EQ(body[0]["vars"][0]["init"]["expr-type"], "frame-alloc");
	ASSERT_EQ(funcName(body[1]["body"]["name"]), "g");
	ASSERT_EQ(body[1]["body"]["ret-dest"]["name"], temp);
	ASSERT_EQ(body[2]["body"]["name"], "memcpy");
	ASSERT_EQ(body[2]["body"]["args"][0]["name"], "s");
	ASSERT_EQ(body[2]["body"]["args"][1]["name"], temp);
}

TEST(sa, struct_ret_temp) {
	json jout = written(run_sa("../test/testdata/sa/217_struct_ret_temp.pa"));
	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts.size(), 3);
	// A returned struct passed as an argument is written into a frame temp;
	// only what it owns is released.
	const auto& argP = stmts[0]["vars"][0]["init"]["args"][0];
	ASSERT_EQ(tempInit(argP)["expr-type"], "frame-alloc");
	ASSERT_EQ(argP["release"].size(), 0u);
	const auto& argQ = stmts[1]["vars"][0]["init"]["args"][0];
	ASSERT_EQ(tempRelease(argQ), "__pln_fini_Q");
	ASSERT_EQ(argQ["release"][0]["body"]["func-type"], "palan");
	// A discarded one is written into a frame temp too.
	const auto& blk = stmts[2];
	ASSERT_EQ(blk["stmt-type"], "block");
	const auto& body = blk["body"];
	ASSERT_EQ(body.size(), 2);
	string temp = body[0]["vars"][0]["name"];
	ASSERT_EQ(body[0]["vars"][0]["init"]["expr-type"], "frame-alloc");
	ASSERT_EQ(funcName(body[1]["body"]["name"]), "g");
	ASSERT_EQ(body[1]["body"]["ret-dest"]["name"], temp);
}

TEST(sa, unsized_struct_arr_sig) {
	json jout = written(run_sa("../test/testdata/sa/220_unsized_struct_arr_sig.pa"));
	// The same representations as [n]P / [n]$P, minus the size.
	json owned = {{"type-kind","pntr"},{"base-type",{{"type-kind","pntr"},
	              {"base-type",{{"type-kind","struct"},{"type-name","P"}}}}}};
	json contig = {{"type-kind","pntr"},{"embedded",true},{"stride",4},
	               {"base-type",{{"type-kind","struct"},{"type-name","P"}}}};
	const auto& f = jout["functions"][0];
	ASSERT_EQ(f["parameters"][0]["var-type"], owned);
	ASSERT_EQ(f["parameters"][1]["var-type"], contig);
	ASSERT_EQ(f["parameters"][2]["var-type"], (json{{"type-kind","pntr"},{"base-type",owned}}));
	ASSERT_EQ(f["ret-type"], owned);

	json ownedSlot = owned;
	ownedSlot["mutable"] = true;
	json contigSlot = contig;
	contigSlot["mutable"] = true;
	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts[0]["vars"][0]["var-type"]["base-type"], ownedSlot);
	ASSERT_EQ(stmts[1]["vars"][0]["var-type"]["base-type"], contigSlot);
}

// A field's array is typed and described as a variable's: rows of rows,
// rows laid out in place and rows of `[+]T`, each level's length in order.
TEST(sa, struct_multilevel_fields)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/310_struct_multilevel_fields.pa"));
	ASSERT_TRUE(jout.is_object());
	json s;
	for (auto& sh : jout["alloc-shapes"])
		if (sh.value("shape-kind", "") == "struct" && sh["shape-name"] == "S") s = sh;
	const auto& owned = s["owned-fields"];
	ASSERT_EQ(owned.size(), 3u);
	ASSERT_EQ(owned[0]["elem"]["kind"], "row");
	ASSERT_EQ(owned[0]["elem"]["elem"]["kind"], "row");
	ASSERT_EQ(owned[0]["elem"]["elem"]["elem"]["type"], "int32");
	ASSERT_EQ(owned[0]["lens"], json::array({3, 4}));
	ASSERT_EQ(owned[1]["elem"]["elem"]["elem"]["kind"], "embed-struct");
	ASSERT_EQ(owned[1]["lens"], json::array({2, 3}));
	ASSERT_EQ(owned[2]["elem"]["elem"]["kind"], "varr");
	ASSERT_EQ(owned[2]["lens"], json::array({2}));
	ASSERT_EQ(owned[2]["offset"], 16);

	const auto& m = jout["statements"][1]["target"]["array"]["array"]["value-type"];
	ASSERT_EQ(m["arr-size"], 3);
	ASSERT_EQ(m["base-type"]["arr-size"], 4);
	ASSERT_EQ(m["base-type"]["base-type"]["type-name"], "int32");
}

// A `[+]T` header laid out in a struct is built with it, and a struct laid out
// in another brings its owned fields along at their place in the outer one.
TEST(sa, field_embed_varr)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/334_field_embed_varr.pa"));
	ASSERT_TRUE(jout.is_object());
	json s;
	for (auto& sh : jout["alloc-shapes"])
		if (sh.value("shape-kind", "") == "struct") s = sh;
	ASSERT_EQ(s["shape-name"], "Named");
	ASSERT_EQ(s["total-size"], 48);
	const auto& owned = s["owned-fields"];
	ASSERT_EQ(owned.size(), 2u);
	ASSERT_EQ(owned[0]["name"], "name.bytes");
	ASSERT_EQ(owned[0]["offset"], 16);
	ASSERT_EQ(owned[0]["elem"]["kind"], "embed-varr");
	ASSERT_EQ(owned[0]["elem"]["shape-key"], "varr_int8");
	ASSERT_EQ(owned[0]["elem"]["size"], 24);
	ASSERT_EQ(owned[1]["name"], "xs");
	ASSERT_EQ(owned[1]["offset"], 40);
	ASSERT_EQ(owned[1]["elem"]["kind"], "varr");

	const auto& h = jout["statements"][1]["body"]["args"][0];
	ASSERT_EQ(h["expr-type"], "field-access");
	ASSERT_EQ(h["offset"], 16);
	ASSERT_TRUE(h["addr-only"].get<bool>());
	ASSERT_EQ(h["value-type"]["base-type"]["type-kind"], "varr");
}

// The module defining a struct stores its initializers by a function of its
// own, which the generated __pln_init_T calls; a struct laid out in it brings
// its own function along.
TEST(sa, field_init)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/335_field_init.pa"));
	ASSERT_TRUE(jout.is_object());
	map<string, json> shapes;
	for (auto& sh : jout["alloc-shapes"])
		if (sh.value("shape-kind", "") == "struct") shapes[sh["shape-name"]] = sh;
	string fiP = shapes.at("P")["init-func"], fiS = shapes.at("S")["init-func"];
	ASSERT_FALSE(shapes.at("S").contains("inits"));
	map<string, json> funcs;
	for (auto& f : jout["functions"]) funcs[f["name"]] = f;
	const auto& p = funcs.at(fiP);
	ASSERT_TRUE(p["export"].get<bool>());
	ASSERT_EQ(p["body"].size(), 2u);
	ASSERT_EQ(p["body"][0]["stmt-type"], "field-assign");
	ASSERT_EQ(p["body"][0]["offset"], 4);
	ASSERT_EQ(p["body"][0]["value"]["value"], "200");
	// $P p calls P's function; $[2]$P ps builds both as a run.
	const auto& sb = funcs.at(fiS)["body"];
	ASSERT_EQ(sb[0]["body"]["name"], fiP);
	ASSERT_EQ(sb[0]["body"]["args"][0]["offset"], 32);
	ASSERT_EQ(funcName(sb[1]["body"]["name"]), "__pln_initrun_P");
	ASSERT_EQ(sb[1]["body"]["args"][2]["value"], "2");

	// A named literal leaving a field out gives it the initializer.
	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts[3]["offset"], 4);
	ASSERT_EQ(stmts[3]["value"]["value"], "200");
	ASSERT_EQ(stmts[4]["value"]["value"], "4");
}

// Elements laid out alike share their functions: integers of one width and
// structs of one size owning nothing; a float has a layout of its own.
TEST(sa, layout_share)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/311_layout_share.pa"));
	ASSERT_TRUE(jout.is_object());
	vector<string> keys;
	for (auto& s : jout["alloc-shapes"])
		keys.push_back(s.value("shape-key", s.value("shape-name", "")));
	ASSERT_EQ(keys, (vector<string>{"varr_int32", "varr_flo32", "__pln_B8", "varr_P8__pln_B8"}));
	const auto& stmts = jout["statements"];
	ASSERT_EQ(funcName(stmts[0]["vars"][0]["init"]["name"]), "__pln_alloc_varr_int32");
	ASSERT_EQ(funcName(stmts[1]["vars"][0]["init"]["name"]), "__pln_alloc_varr_int32");
	ASSERT_EQ(funcName(stmts[3]["vars"][0]["init"]["name"]), "__pln_alloc_varr_P8__pln_B8");
	ASSERT_EQ(funcName(stmts[4]["vars"][0]["init"]["name"]), "__pln_alloc_varr_P8__pln_B8");
	// push takes each array's own element type but calls the shared function.
	ASSERT_EQ(stmts[5]["body"]["name"], stmts[6]["body"]["name"]);
	ASSERT_EQ(stmts[6]["body"]["args"][1]["value-type"]["type-name"], "uint32");
	ASSERT_EQ(funcName(stmts[7]["body"]["name"]), "__pln_push_varr_flo32");
}

// A field's `$[n]...` lays the array out in the struct, typed as a variable
// of `[n]...` is; a field without the `$` owns its array, as the variable does.
TEST(sa, field_inline_arr)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/312_field_inline_arr.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts[0]["vars"][0]["init"]["args"][0]["value"], "88");
	auto fieldOf = [](const json& target) {
		const json* t = &target;
		while (t->value("expr-type", "") != "field-access") t = &(*t)["array"];
		return *t;
	};
	const json a = fieldOf(stmts[4]["target"]);
	const json b = fieldOf(stmts[5]["ptr-expr"]);
	const json c = fieldOf(stmts[6]["target"]);
	ASSERT_EQ(a["offset"], 4);
	ASSERT_EQ(b["offset"], 32);
	ASSERT_EQ(c["offset"], 64);
	ASSERT_EQ(a["value-type"], stmts[1]["vars"][0]["var-type"]);
	ASSERT_EQ(b["value-type"], stmts[2]["vars"][0]["var-type"]);
	ASSERT_EQ(c["value-type"], stmts[3]["vars"][0]["var-type"]);
	// Without the `$`, `[2]$P` is an owned block of structs, as a variable's.
	const json& q = jout["alloc-shapes"][1];
	ASSERT_EQ(q["owned-fields"][0]["name"], "d");
	ASSERT_EQ(q["owned-fields"][0]["elem"]["kind"], "row");
	ASSERT_EQ(q["owned-fields"][0]["elem"]["elem"]["kind"], "embed-struct");
}

// An array laid out in the struct holding owned elements is one owned field,
// a row laid out in place, whose slots are built with the struct.
TEST(sa, field_inline_owned)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/315_field_inline_owned.pa");
	ASSERT_TRUE(jout.is_object());
	const json& owned = jout["alloc-shapes"].back()["owned-fields"];
	ASSERT_EQ(owned.size(), 3u);
	ASSERT_EQ(owned[0]["offset"], 8);
	ASSERT_EQ(owned[0]["elem"]["kind"], "embed-row");
	ASSERT_EQ(owned[0]["elem"]["depth"], 1);
	ASSERT_EQ(owned[0]["elem"]["elem"]["kind"], "struct");
	ASSERT_EQ(owned[0]["lens"], json::array({2}));
	ASSERT_EQ(owned[1]["offset"], 24);
	ASSERT_EQ(owned[1]["elem"]["depth"], 2);
	ASSERT_EQ(owned[1]["lens"], json::array({2, 3}));
	ASSERT_EQ(owned[2]["offset"], 72);
	ASSERT_EQ(owned[2]["elem"]["elem"]["kind"], "varr");
}

// A `[+]T` field is an owned slot built as a `[+]T` element is; a struct
// literal resizes it to the literal's length before setting the elements.
TEST(sa, struct_varr_field)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/313_struct_varr_field.pa");
	ASSERT_TRUE(jout.is_object());
	const json& owned = jout["alloc-shapes"][1]["owned-fields"];
	ASSERT_EQ(owned.size(), 1);
	ASSERT_EQ(owned[0]["name"], "a");
	ASSERT_EQ(owned[0]["offset"], 8);
	ASSERT_EQ(owned[0]["elem"]["kind"], "varr");
	ASSERT_EQ(owned[0]["elem"]["shape-key"], "varr_int32");
	const auto& stmts = jout["statements"];
	ASSERT_EQ(funcName(stmts[2]["body"]["name"]), "__pln_resize_varr_int32");
	ASSERT_EQ(stmts[2]["body"]["args"][1]["value"], "2");
	ASSERT_EQ(stmts[3]["stmt-type"], "arr-assign");
	ASSERT_EQ(stmts[4]["value"]["value"], "7");
}

TEST(sa, struct_field_const_size)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/233_struct_field_const_size.pa"));
	ASSERT_TRUE(jout.is_object());
	map<string, json> shapes;
	for (auto& s : jout["alloc-shapes"])
		if (s.value("shape-kind", "") == "struct") shapes[s["shape-name"]] = s;
	// a $[3]int32 12 + b $[3]$Point 24 (+4) + c $[3]@Point 24 + d 8 + e 8
	// + r $[3]int16 6 + z $[8]uint8 8 (+2)
	ASSERT_EQ(shapes["S"]["total-size"], 96);
	ASSERT_EQ(shapes["S"]["owned-fields"].size(), 2u);
	ASSERT_EQ(shapes["S"]["owned-fields"][0]["name"], "d");
	ASSERT_EQ(shapes["S"]["owned-fields"][0]["lens"], json::array({3}));
	ASSERT_EQ(shapes["S"]["owned-fields"][1]["offset"], 72);
	ASSERT_EQ(shapes["L"]["total-size"], 16);
	ASSERT_EQ(shapes["L"]["owned-fields"][0]["lens"], json::array({2}));
}

TEST(sa, struct_lit_init_assign)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/238_struct_lit_init_assign.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts[0]["stmt-type"], "var-decl");
	ASSERT_EQ(stmts[0]["vars"][0]["init"]["name"], "__pln_malloc");
	ASSERT_EQ(stmts[1]["stmt-type"], "field-assign");
	ASSERT_EQ(stmts[1]["var"], "a");
	ASSERT_EQ(stmts[3]["offset"], 8);
	// {z: 6, x: 4, y: 5} is stored in field order.
	ASSERT_EQ(stmts[5]["offset"], 0);
	ASSERT_EQ(stmts[5]["value"]["value"], "4");
	// s.a is an owned struct reached through its pointer; s.b is embedded at offset 8.
	ASSERT_TRUE(stmts[9].contains("ptr-expr"));
	ASSERT_EQ(stmts[12]["var"], "s");
	ASSERT_EQ(stmts[12]["offset"], 8);

	// Values first, then the target's address, then the stores through it.
	const auto& swap = stmts[15];
	ASSERT_EQ(swap["stmt-type"], "block");
	ASSERT_EQ(swap["body"].size(), 7);
	ASSERT_EQ(swap["body"][0]["vars"][0]["init"]["offset"], 4);
	ASSERT_EQ(swap["body"][3]["vars"][0]["var-type"]["mutable"], true);
	ASSERT_EQ(swap["body"][4]["stmt-type"], "field-assign");
	ASSERT_EQ(swap["body"][4]["value"]["name"], swap["body"][0]["vars"][0]["name"]);
	ASSERT_EQ(stmts[16]["stmt-type"], "block");
	ASSERT_EQ(stmts[18]["stmt-type"], "block");
	ASSERT_EQ(stmts[18]["body"][3]["vars"][0]["init"]["expr-type"], "arr-index");

	const auto& fbody = jout["functions"][0]["body"];
	ASSERT_EQ(fbody[0]["stmt-type"], "block");
	ASSERT_EQ(fbody[0]["body"][3]["vars"][0]["init"]["expr-type"], "arr-index");
}

TEST(sa, named_ret_struct_auto_decl)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/239_named_ret_struct_auto_decl.pa"));
	ASSERT_TRUE(jout.is_object());
	// A named struct return is the storage the caller provides: a frame slot
	// returned in registers, or a hidden argument whose owned fields are built
	// at entry. Nothing frees it.
	const auto& f = jout["functions"][0];
	ASSERT_EQ(f["ret-slot"]["name"], "p");
	const auto& fbody = f["body"];
	ASSERT_EQ(fbody.size(), 2);
	ASSERT_EQ(fbody[0]["vars"][0]["name"], "p");
	ASSERT_EQ(fbody[0]["vars"][0]["init"]["expr-type"], "frame-alloc");
	const auto& g = jout["functions"][1];
	ASSERT_EQ(g["rets"][0]["slot"], true);
	const auto& gbody = g["body"];
	ASSERT_EQ(gbody.size(), 3);
	ASSERT_EQ(funcName(gbody[0]["body"]["name"]), "__pln_init_O");
	ASSERT_EQ(gbody[0]["body"]["args"][0]["name"], "o");
}

TEST(sa, struct_init_from_call)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/240_struct_init_from_call.pa");
	ASSERT_TRUE(jout.is_object());
	// The struct is returned into the variable's storage: no copy.
	const auto& stmts = jout["statements"];
	ASSERT_EQ(stmts.size(), 6);
	const char* expected[][2] = {{"a", "named"}, {"b", "plain"}, {"c", "named"}};
	for (int i = 0; i < 3; i++) {
		const auto& decl = stmts[2 * i];
		ASSERT_EQ(decl["stmt-type"], "var-decl");
		ASSERT_EQ(decl["vars"].size(), 1);
		ASSERT_EQ(decl["vars"][0]["name"], expected[i][0]);
		ASSERT_EQ(decl["vars"][0]["init"]["name"], "__pln_malloc");
		const auto& call = stmts[2 * i + 1]["body"];
		ASSERT_EQ(funcName(call["name"]), expected[i][1]);
		ASSERT_EQ(call["ret-dest"]["name"], expected[i][0]);
	}
}

TEST(sa, return_struct_lit)
{
	cleanTestEnv();
	genLibSaTypes();
	json jout = run_sa("../test/testdata/sa/241_return_struct_lit.pa");
	ASSERT_TRUE(jout.is_object());
	// The literal is stored field by field into the return slot.
	for (int i = 0; i < 3; i++) {
		const auto& func = jout["functions"][i];
		ASSERT_EQ(func["ret-slot"]["name"], "__ret");
		const auto& body = func["body"];
		ASSERT_EQ(body[0]["vars"][0]["name"], "__ret");
		for (size_t k = 1; k + 1 < body.size(); k++) {
			ASSERT_EQ(body[k]["stmt-type"], "field-assign") << k;
			ASSERT_EQ(body[k]["var"], "__ret") << k;
		}
		const auto& ret = body[body.size() - 1];
		ASSERT_EQ(ret["stmt-type"], "return");
		ASSERT_FALSE(ret.contains("values"));
	}
}

TEST(sa, struct_lit_arr_field)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/243_struct_lit_arr_field.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	// Field order: c[0..2] and v[0..1] are element stores; ps and os elements are struct field stores.
	auto checkStores = [](const json& list, size_t from) {
		for (size_t k = 0; k < 13; k++)
			ASSERT_EQ(list[from + k]["stmt-type"], (k < 3 || k >= 11) ? "arr-assign" : "field-assign");
	};
	checkStores(stmts, 1);
	ASSERT_EQ(stmts[1]["target"]["array"]["expr-type"], "field-access");
	ASSERT_EQ(stmts[1]["target"]["array"]["var"], "m");

	// '->': all 13 values go to temps before the target's address and the stores.
	const auto& body = stmts[14]["body"];
	ASSERT_EQ(stmts[14]["stmt-type"], "block");
	ASSERT_EQ(body.size(), 27);
	checkStores(body, 14);
	ASSERT_EQ(body[26]["value"]["name"], body[12]["vars"][0]["name"]);

	checkStores(stmts, 16);
}

TEST(sa, struct_lit_struct_expr)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/244_struct_lit_struct_expr.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	// Q q = [v, mk(3)]: copy from v, and from the call result through a frame temp.
	ASSERT_EQ(stmts[4]["body"]["name"], "memcpy");
	ASSERT_EQ(stmts[4]["body"]["args"][1]["name"], "v");
	ASSERT_EQ(stmts[5]["body"][0]["vars"][0]["init"]["expr-type"], "frame-alloc");
	ASSERT_EQ(funcName(stmts[5]["body"][1]["body"]["name"]), "mk");

	// [q.b, @q.a] -> q: both values are copied into temp structs before the
	// stores, which copy from the temps; the temps are freed at the end.
	const auto& body = stmts[6]["body"];
	ASSERT_EQ(body.size(), 9);
	string t0 = body[0]["vars"][0]["name"], t1 = body[2]["vars"][0]["name"];
	ASSERT_EQ(body[1]["body"]["args"][1]["expr-type"], "field-access");
	ASSERT_EQ(body[3]["body"]["args"][1]["value-type"]["mutable"], false);
	ASSERT_EQ(body[5]["body"]["args"][1]["name"], t0);
	ASSERT_EQ(body[6]["body"]["args"][1]["name"], t1);
	ASSERT_EQ(body[7]["body"]["args"][0]["name"], t1);
	ASSERT_EQ(body[8]["body"]["args"][0]["name"], t0);

	// [os[1], os[0]] -> os: struct elements go through temps as well.
	ASSERT_EQ(stmts[13]["body"][7]["body"]["name"], "free");

	// return [p, mk(1)] from f: the slot's owned field is built, then copied
	// into from the '@P' parameter.
	const auto& fbody = jout["functions"][1]["body"];
	ASSERT_EQ(funcName(fbody[1]["body"]["name"]).rfind("__pln_init_", 0), 0);
	ASSERT_EQ(fbody[2]["body"]["args"][1]["name"], "p");
}

TEST(sa, return_unowned_struct)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/245_return_unowned_struct.pa"));
	ASSERT_TRUE(jout.is_object());
	auto func = [&](const string& name) -> const json& {
		for (auto& f : jout["functions"])
			if (funcName(f["name"]) == name) return f["body"];
		throw runtime_error("no function " + name);
	};
	// A struct the function doesn't own is copied into the return slot.
	for (string name : {"ra", "rb", "rc", "rf"}) {
		const auto& body = func(name);
		ASSERT_EQ(body.size(), 3) << name;
		ASSERT_EQ(body[1]["body"]["name"], "memcpy") << name;
		ASSERT_EQ(body[1]["body"]["args"][0]["name"], "__ret") << name;
		ASSERT_FALSE(body[2].contains("values")) << name;
	}
	ASSERT_EQ(func("ra")[1]["body"]["args"][1]["name"], "p");
	ASSERT_EQ(func("rc")[1]["body"]["args"][1]["expr-type"], "field-access");

	// From a local: the copy is made before the local is freed.
	const auto& rd = func("rd");
	ASSERT_EQ(rd[5]["body"]["name"], "memcpy");
	ASSERT_EQ(funcName(rd[6]["body"]["name"]), "__pln_free_Q");
	ASSERT_EQ(func("re")[4]["body"]["name"], "memcpy");

	// A struct owning fields is copied with what it owns, into a slot built first.
	const auto& rg = func("rg");
	ASSERT_EQ(funcName(rg[1]["body"]["name"]), "__pln_init_Q");
	ASSERT_EQ(funcName(rg[2]["body"]["name"]), "__pln_copy_Q");
	ASSERT_EQ(rg[2]["body"]["args"][1]["name"], "q");

	// A local struct variable and a struct parameter move into the slot: their
	// bytes are copied and only their block is freed. A call result is
	// returned straight into the slot.
	for (auto [name, var] : {pair{"rh", "l"}, pair{"rk", "p"}}) {
		const auto& body = func(name);
		size_t n = body.size();
		ASSERT_EQ(body[n - 3]["body"]["name"], "memcpy") << name;
		ASSERT_EQ(body[n - 3]["body"]["args"][1]["name"], var) << name;
		ASSERT_EQ(body[n - 2]["body"]["name"], "free") << name;
		ASSERT_EQ(body[n - 2]["body"]["args"][0]["name"], var) << name;
	}
	const auto& ri = func("ri");
	ASSERT_EQ(ri.size(), 3);
	ASSERT_EQ(funcName(ri[1]["body"]["name"]), "mk");
	ASSERT_EQ(ri[1]["body"]["ret-dest"]["name"], "__ret");

	// A returned borrow stays a borrow.
	const auto& rj = func("rj");
	ASSERT_EQ(rj.size(), 1);
	ASSERT_EQ(rj[0]["values"][0]["name"], "p");
}

TEST(sa, embed_field_value)
{
	cleanTestEnv();
	json jout = run_sa("../test/testdata/sa/246_embed_field_value.pa");
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	auto isEmbedA = [](const json& e) {
		return e["expr-type"] == "field-access" && e["addr-only"] == true && e["offset"] == 0;
	};
	// q.a -> v and P w = q.a: copied from the field's address.
	ASSERT_EQ(stmts[6]["body"]["name"], "memcpy");
	ASSERT_TRUE(isEmbedA(stmts[6]["body"]["args"][1]));
	ASSERT_EQ(stmts[8]["body"]["name"], "memcpy");
	ASSERT_TRUE(isEmbedA(stmts[8]["body"]["args"][1]));
	// g(q.a): copied into the struct parameter from the field's address.
	ASSERT_TRUE(isEmbedA(stmts[9]["vars"][0]["init"]["args"][0]["body"][1]["body"]["args"][1]));
	// [q.b, q.a] -> q: q.a is copied into a temp struct first.
	ASSERT_TRUE(isEmbedA(stmts[10]["body"][3]["body"]["args"][1]));
	// return q.a: copied into the returned struct.
	const auto& ra = jout["functions"][1]["body"];
	ASSERT_EQ(ra[1]["body"]["name"], "memcpy");
	ASSERT_EQ(ra[1]["body"]["args"][1]["addr-only"], true);
	// An embedded field of a cinclude'd struct.
	ASSERT_EQ(stmts[15]["body"]["name"], "memcpy");
	ASSERT_EQ(stmts[15]["body"]["args"][1]["addr-only"], true);
}

TEST(sa, member_call_borrow)
{
	cleanTestEnv();
	genModuleAst("../test/testdata/sa/lib_sa_mk.pa");
	json jout = run_sa("../test/testdata/sa/247_member_call_borrow.pa");
	ASSERT_TRUE(jout.is_object());

	vector<json> args;
	for (auto& st : jout["statements"])
		if (st["stmt-type"] == "expr")
			args.push_back(st["body"]["args"][0]);
	// @s.scaled(), @!s.scaled(), @M.mk(), @!M.mk(), @M.mk().scaled()
	ASSERT_EQ(args.size(), 5u);
	const char* names[] = {"scaled", "scaled", "mk", "mk", "scaled"};
	const bool muts[] = {false, true, false, true, false};
	// Each returned struct is written into a frame temp for the call.
	for (size_t i = 0; i < args.size(); i++) {
		ASSERT_EQ(tempInit(args[i])["expr-type"], "frame-alloc") << i;
		const json& call = args[i]["body"][1]["body"];
		ASSERT_EQ(funcName(call["name"]), names[i]) << i;
		ASSERT_EQ(call["ret-dest"]["name"], args[i]["value"]["name"]) << i;
		ASSERT_EQ(args[i]["value"]["value-type"]["mutable"], muts[i]) << i;
	}
	const auto& inner = args[4]["body"][1]["body"]["args"][0];
	ASSERT_EQ(funcName(inner["body"][1]["body"]["name"]), "mk");
}

TEST(sa, frame_struct)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/274_frame_struct.pa"));
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	auto call = [&](size_t i) -> const json& { return stmts[i]["body"]; };

	// The var keeps the heap version's type; only its init differs.
	const auto& p = stmts[0]["vars"][0];
	ASSERT_EQ(p["var-type"]["base-type"]["type-name"], "P");
	ASSERT_EQ(p["init"]["expr-type"], "frame-alloc");
	ASSERT_EQ(p["init"]["size"], 16);
	ASSERT_EQ(p["init"]["align"], 8);

	ASSERT_EQ(stmts[1]["vars"][0]["init"]["size"], 24);
	ASSERT_EQ(funcName(call(2)["name"]), "__pln_init_Q");

	// A struct returned by a call is written into the frame, which the callee
	// builds, so no init follows.
	ASSERT_EQ(stmts[3]["vars"][0]["init"]["expr-type"], "frame-alloc");
	ASSERT_EQ(funcName(call(4)["name"]), "makeQ");
	ASSERT_EQ(call(4)["ret-dest"]["name"], "r");

	ASSERT_EQ(call(6)["name"], "memcpy");
	ASSERT_EQ(stmts[7]["vars"].size(), 2);
	ASSERT_EQ(stmts[7]["vars"][1]["init"]["expr-type"], "frame-alloc");

	// A returned stack struct moves into the return slot: its bytes are copied
	// and nothing it owns is released.
	const auto& retQ = jout["functions"][1]["body"];
	ASSERT_EQ(retQ.size(), 4);
	ASSERT_EQ(retQ[2]["body"]["name"], "memcpy");
	ASSERT_EQ(retQ[2]["body"]["args"][1]["name"], "t");
	ASSERT_EQ(retQ[3]["stmt-type"], "return");
}

TEST(sa, frame_arr)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/275_frame_arr.pa"));
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	auto init = [&](size_t i) -> const json& { return stmts[i]["vars"][0]["init"]; };
	auto sizeAlign = [&](size_t i) { return make_pair(init(i)["size"].get<int>(), init(i)["align"].get<int>()); };

	ASSERT_EQ(sizeAlign(0), make_pair(16, 4));
	ASSERT_EQ(stmts[0]["vars"][0]["var-type"]["arr-size"], 4);
	ASSERT_EQ(sizeAlign(1), make_pair(24, 8));
	ASSERT_EQ(sizeAlign(2), make_pair(32, 8));
	ASSERT_EQ(sizeAlign(3), make_pair(12, 2));
	// Only the pointer array is in the frame; its elements or rows are on the heap.
	ASSERT_EQ(sizeAlign(4), make_pair(24, 8));
	ASSERT_EQ(funcName(stmts[5]["body"]["name"]), "__pln_init_arr_P9__pln_B16");
	ASSERT_EQ(sizeAlign(6), make_pair(16, 8));
	ASSERT_EQ(funcName(stmts[7]["body"]["name"]), "__pln_init_arr_Pint32");
	ASSERT_EQ(funcName(stmts[9]["body"]["name"]), "__pln_init_arr_ES16_P9__pln_B16");
	ASSERT_EQ(funcName(stmts[11]["body"]["name"]), "__pln_init_arr_PP9__pln_B16");
	ASSERT_EQ(stmts[11]["body"]["args"].size(), 3);
	ASSERT_EQ(sizeAlign(12), make_pair(12, 4));
	ASSERT_EQ(stmts[13]["stmt-type"], "arr-assign");
}

TEST(sa, frame_scope_exit)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/276_frame_scope_exit.pa"));
	ASSERT_TRUE(jout.is_object());
	const auto& body = jout["functions"][0]["body"];
	const auto& loop = body[2]["body"];
	ASSERT_EQ(funcName(loop[3]["then"]["body"][0]["body"]["name"]), "__pln_fini_Q");
	ASSERT_EQ(loop[3]["then"]["body"][1]["stmt-type"], "continue");
	ASSERT_EQ(funcName(loop[4]["then"]["body"][0]["body"]["name"]), "__pln_fini_Q");
	ASSERT_EQ(loop[4]["then"]["body"][1]["stmt-type"], "break");
	ASSERT_EQ(funcName(loop[5]["body"]["name"]), "__pln_fini_Q");
	ASSERT_EQ(funcName(body[4]["body"]["name"]), "__pln_fini_Q");
	ASSERT_EQ(body[5]["stmt-type"], "return");
}

TEST(sa, literal_conversion)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/342_literal_conversion.pa"));
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	auto isConversion = [](const json& call) {
		return call["expr-type"] == "call" && funcName(call["name"]) == "text"
		    && call["args"][0]["expr-type"] == "lit-str" && call["args"][1]["value"] == "0";
	};
	// Initialization: the conversion returns into the variable.
	ASSERT_TRUE(isConversion(stmts[1]["body"]));
	ASSERT_EQ(stmts[1]["body"]["ret-dest"]["name"], "s");
	// '->': returned into a temp copied from.
	ASSERT_TRUE(isConversion(stmts[2]["body"][1]["body"]));
	// '@T' argument: a temp released after the call; 'T' argument: the callee's.
	const auto& borrowed = stmts[3]["body"]["args"][0];
	ASSERT_EQ(borrowed["expr-type"], "temp");
	ASSERT_TRUE(isConversion(borrowed["body"][1]["body"]));
	ASSERT_EQ(borrowed["release"].size(), 1);
	const auto& owned = stmts[4]["body"]["args"][0];
	ASSERT_TRUE(isConversion(owned["body"][1]["body"]));
	ASSERT_EQ(owned["release"].size(), 0);
	// An overload taking the literal as it is wins over a conversion.
	ASSERT_EQ(funcName(stmts[5]["vars"][0]["init"]["name"]), "bytes");
	ASSERT_EQ(stmts[5]["vars"][0]["init"]["args"][0]["expr-type"], "lit-str");
	// Variadic elements and an operator's operand.
	ASSERT_TRUE(isConversion(stmts[6]["body"]["args"][0]["body"][2]["body"]));
	const auto& eq = stmts[7]["vars"][0]["init"];
	ASSERT_EQ(funcName(eq["name"]), "0eq");
	ASSERT_TRUE(isConversion(eq["args"][0]["body"][1]["body"]));
	// A struct literal's field.
	ASSERT_TRUE(isConversion(stmts[9]["body"][1]["body"]));
	// return.
	ASSERT_TRUE(isConversion(jout["functions"][6]["body"][1]["body"]));
	// A variadic parameter takes the literal alone too.
	const auto& words = stmts[12]["body"];
	ASSERT_EQ(funcName(words["name"]), "words");
	ASSERT_EQ(words["ret-dest"]["name"], "w");
}

TEST(sa, numeric_literal_conversion)
{
	cleanTestEnv();
	json jout = written(run_sa("../test/testdata/sa/353_numeric_literal_conversion.pa"));
	ASSERT_TRUE(jout.is_object());
	const auto& stmts = jout["statements"];
	// The overload taking the literal's default type converts it; a constant
	// expression or const is folded first.
	auto paramType = [](const json& call) {
		string name = call["name"];
		return name.substr(name.rfind('.') + 1);
	};
	auto isConversion = [&](const json& call, const string& from, const string& value) {
		return call["expr-type"] == "call" && funcName(call["name"]) == "big"
		    && paramType(call) == from && call["args"][0]["value"] == value;
	};
	ASSERT_TRUE(isConversion(stmts[1]["body"], "int64", "12"));
	ASSERT_TRUE(isConversion(stmts[2]["body"][1]["body"], "flo64", "1.5"));
	ASSERT_TRUE(isConversion(stmts[3]["body"]["args"][0]["body"][1]["body"], "int64", "-12"));
	// An overload taking the literal as it is wins over a conversion.
	ASSERT_EQ(paramType(stmts[4]["vars"][0]["init"]), "int64");
	// Operator operands.
	ASSERT_TRUE(isConversion(stmts[6]["body"]["args"][1]["body"][1]["body"], "int64", "2"));
	ASSERT_TRUE(isConversion(stmts[7]["vars"][0]["init"]["args"][1]["body"][1]["body"], "int64", "36"));
	// A struct literal's field, array literal elements, and return.
	ASSERT_TRUE(isConversion(stmts[9]["body"][1]["body"], "int64", "10"));
	ASSERT_TRUE(isConversion(stmts[12]["body"][1]["body"], "int64", "1"));
	ASSERT_TRUE(isConversion(stmts[13]["body"][1]["body"], "int64", "2"));
	ASSERT_TRUE(isConversion(jout["functions"][7]["body"][1]["body"], "int64", "7"));
	// A string const converts as the literal does.
	ASSERT_EQ(funcName(stmts[15]["body"]["name"]), "name");
	ASSERT_EQ(stmts[15]["body"]["ret-dest"]["name"], "n");
}
