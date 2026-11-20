/// semantic analyzer unit tests
///
/// @file basicTests.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <gtest/gtest.h>
#include "PlnType.h"
#include "PlnSaInternal.h"

using N = PrimType::Name;

// -------- typeCompat: identical --------

TEST(typecompat, identical_prim) {
    PlnTypeRegistry reg;
    const PlnType* t = reg.prim(N::Int32);
    EXPECT_EQ(typeCompat(t, t, reg), TypeCompat::Identical);
}

TEST(typecompat, identical_ptr) {
    PlnTypeRegistry reg;
    const PlnType* base = reg.prim(N::Int32);
    const PlnType* p1 = reg.ptr(base);
    const PlnType* p2 = reg.ptr(base);  // interned — same pointer
    EXPECT_EQ(p1, p2);
    EXPECT_EQ(typeCompat(p1, p2, reg), TypeCompat::Identical);
}

// -------- typeCompat: ImplicitWiden --------

TEST(typecompat, implicit_widen_signed) {
    PlnTypeRegistry reg;
    EXPECT_EQ(typeCompat(reg.prim(N::Int32), reg.prim(N::Int64), reg),
              TypeCompat::ImplicitWiden);
}

TEST(typecompat, implicit_widen_unsigned) {
    PlnTypeRegistry reg;
    EXPECT_EQ(typeCompat(reg.prim(N::Uint16), reg.prim(N::Uint32), reg),
              TypeCompat::ImplicitWiden);
}

// -------- typeCompat: ExplicitCast --------

TEST(typecompat, explicit_cast_narrowing) {
    PlnTypeRegistry reg;
    EXPECT_EQ(typeCompat(reg.prim(N::Int64), reg.prim(N::Int32), reg),
              TypeCompat::ExplicitCast);
}

TEST(typecompat, explicit_cast_sign_change_same_size) {
    PlnTypeRegistry reg;
    EXPECT_EQ(typeCompat(reg.prim(N::Int32), reg.prim(N::Uint32), reg),
              TypeCompat::ExplicitCast);
}

TEST(typecompat, explicit_cast_sign_change_widen) {
    PlnTypeRegistry reg;
    EXPECT_EQ(typeCompat(reg.prim(N::Int32), reg.prim(N::Uint64), reg),
              TypeCompat::ExplicitCast);
}

// -------- typeCompat: Incompatible --------

TEST(typecompat, incompatible_prim_ptr) {
    PlnTypeRegistry reg;
    const PlnType* i32 = reg.prim(N::Int32);
    const PlnType* pi32 = reg.ptr(i32);
    EXPECT_EQ(typeCompat(i32, pi32, reg), TypeCompat::Incompatible);
}

TEST(typecompat, incompatible_ptr_diff_base) {
    PlnTypeRegistry reg;
    const PlnType* pi32 = reg.ptr(reg.prim(N::Int32));
    const PlnType* pi64 = reg.ptr(reg.prim(N::Int64));
    EXPECT_EQ(typeCompat(pi32, pi64, reg), TypeCompat::Incompatible);
}

// -------- typeCompat: pntr(void) --------

TEST(typecompat, void_ptr_compat_with_typed_ptr) {
    PlnTypeRegistry reg;
    const PlnType* pvoid = reg.ptr(reg.prim(N::Void));
    const PlnType* pi32  = reg.ptr(reg.prim(N::Int32));
    EXPECT_EQ(typeCompat(pvoid, pi32, reg), TypeCompat::Identical);
    EXPECT_EQ(typeCompat(pi32, pvoid, reg), TypeCompat::Identical);
}

TEST(typecompat, void_ptr_compat_with_void_ptr) {
    PlnTypeRegistry reg;
    const PlnType* pvoid = reg.ptr(reg.prim(N::Void));
    EXPECT_EQ(typeCompat(pvoid, pvoid, reg), TypeCompat::Identical);
}

// -------- PlnTypeRegistry: interning --------

TEST(typecompat, registry_intern_prim) {
    PlnTypeRegistry reg;
    const PlnType* a = reg.prim(N::Int32);
    const PlnType* b = reg.prim(N::Int32);
    EXPECT_EQ(a, b);
}

TEST(typecompat, registry_intern_ptr) {
    PlnTypeRegistry reg;
    const PlnType* base = reg.prim(N::Int32);
    const PlnType* p1 = reg.ptr(base);
    const PlnType* p2 = reg.ptr(base);
    EXPECT_EQ(p1, p2);
}

// -------- PlnTypeRegistry: fromJson --------

TEST(typecompat, registry_from_json_prim) {
    PlnTypeRegistry reg;
    json j = {{"type-kind", "prim"}, {"type-name", "int32"}};
    const PlnType* t = reg.fromJson(j);
    EXPECT_EQ(t, reg.prim(N::Int32));
}

TEST(typecompat, registry_from_json_ptr) {
    PlnTypeRegistry reg;
    json j = {
        {"type-kind", "pntr"},
        {"base-type", {{"type-kind", "prim"}, {"type-name", "int32"}}}
    };
    const PlnType* t = reg.fromJson(j);
    ASSERT_EQ(t->kind, PlnType::Kind::Ptr);
    const auto* pt = static_cast<const PtrType*>(t);
    EXPECT_EQ(pt->base, reg.prim(N::Int32));
}

TEST(typecompat, registry_from_json_void_ptr) {
    PlnTypeRegistry reg;
    json j = {
        {"type-kind", "pntr"},
        {"base-type", {{"type-kind", "prim"}, {"type-name", "void"}}}
    };
    const PlnType* t = reg.fromJson(j);
    ASSERT_EQ(t->kind, PlnType::Kind::Ptr);
    const auto* pt = static_cast<const PtrType*>(t);
    EXPECT_EQ(pt->base, reg.prim(N::Void));
}

TEST(typecompat, void_ptr_interns_regardless_of_mutability) {
    // @void and @!void both parse to pntr(void), but PlnTypeRegistry::ptr()
    // interns solely on the base type (PlnType.cpp); "mutable" lives only in
    // the JSON value-type, not in the interned PlnType identity. The
    // @void/@!void grammar productions rely on this: fromJson("mutable":false)
    // and fromJson("mutable":true) must yield the identical PtrType*.
    PlnTypeRegistry reg;
    json j_ro = {{"type-kind", "pntr"}, {"mutable", false},
                 {"base-type", {{"type-kind", "prim"}, {"type-name", "void"}}}};
    json j_mut = {{"type-kind", "pntr"}, {"mutable", true},
                  {"base-type", {{"type-kind", "prim"}, {"type-name", "void"}}}};
    EXPECT_EQ(reg.fromJson(j_ro), reg.fromJson(j_mut));
}

TEST(typecompat, registry_from_json_unrepresentable_throws) {
    // fromJson delegates its domain check to
    // unrepresentableTypeName; this backstop throw only fires for a
    // value-type that reached it despite the upstream gates
    // (validateNativeSig / requireSupportedCFuncSig) -- exercised directly
    // here since no SA-level test path reaches it anymore.
    PlnTypeRegistry reg;
    json j = {{"type-kind", "union"}};
    EXPECT_THROW(reg.fromJson(j), std::runtime_error);
}

// -------- unrepresentableTypeName --------

TEST(typecompat, unrepresentable_prim_known) {
    EXPECT_EQ(unrepresentableTypeName({{"type-kind","prim"},{"type-name","int32"}}), "");
}

TEST(typecompat, unrepresentable_prim_unknown) {
    // e.g. c2ast's C spelling "__int128", not in PrimTypeNames
    EXPECT_EQ(unrepresentableTypeName({{"type-kind","prim"},{"type-name","__int128"}}), "__int128");
}

TEST(typecompat, unrepresentable_prim_missing_name) {
    EXPECT_EQ(unrepresentableTypeName({{"type-kind","prim"}}), "malformed type");
}

TEST(typecompat, unrepresentable_missing_type_kind) {
    EXPECT_EQ(unrepresentableTypeName(json::object()), "malformed type");
}

TEST(typecompat, unrepresentable_non_object) {
    EXPECT_EQ(unrepresentableTypeName(json(nullptr)), "malformed type");
}

TEST(typecompat, unrepresentable_pntr_known_base) {
    json j = {{"type-kind","pntr"},{"base-type",{{"type-kind","prim"},{"type-name","int64"}}}};
    EXPECT_EQ(unrepresentableTypeName(j), "");
}

TEST(typecompat, unrepresentable_pntr_missing_base) {
    EXPECT_EQ(unrepresentableTypeName({{"type-kind","pntr"}}), "malformed type");
}

TEST(typecompat, unrepresentable_pntr_recurses_to_func) {
    // int (*cb)(int) -- pntr(func(...))
    json j = {{"type-kind","pntr"},{"base-type",{{"type-kind","func"}}}};
    EXPECT_EQ(unrepresentableTypeName(j), "function pointer");
}

// normalizeCType must recurse into a `func` type-kind's
// ret-type/parameters, not just stop at the outer pntr wrapping it -- a
// callback parameter's own inner pointers (e.g. qsort's comparator taking
// `const void*`) need "mutable" set too, or isWritableThrough's absent-key
// default (writable) would silently invert their permission.
TEST(normalize_ctype, func_recurses_into_ret_type_and_parameters) {
    // int (*cb)(const void*, const void*) -- pntr(func(params: [pntr(const void) x2], ret: pntr(const int)))
    json j = {
        {"type-kind","pntr"},
        {"base-type", {
            {"type-kind","func"},
            {"ret-type", {{"type-kind","pntr"},{"base-type",{{"type-kind","prim"},{"type-name","int32"},{"const",true}}}}},
            {"parameters", json::array({
                {{"var-type", {{"type-kind","pntr"},{"base-type",{{"type-kind","prim"},{"type-name","void"},{"const",true}}}}}},
                {{"var-type", {{"type-kind","pntr"},{"base-type",{{"type-kind","prim"},{"type-name","void"}}}}}},
                {{"name","..."}}
            })}
        }}
    };
    json n = normalizeCType(j);
    auto& func = n["base-type"];
    EXPECT_EQ(func["ret-type"]["mutable"], false);
    EXPECT_EQ(func["parameters"][0]["var-type"]["mutable"], false);
    EXPECT_EQ(func["parameters"][1]["var-type"]["mutable"], true);
    // variadic sentinel has no var-type -- must survive untouched, not crash
    ASSERT_FALSE(func["parameters"][2].contains("var-type"));
}

TEST(normalize_ctype, func_recurses_into_nested_strct) {
    // A callback returning a bare `struct Tag` by value -- proof the recursion
    // dispatches through normalizeCType's full switch (strct->struct fold),
    // not a partial copy of only the pntr/mutable logic.
    json j = {{"type-kind","func"}, {"ret-type", {{"type-kind","strct"},{"type-name","Tag"}}}};
    json n = normalizeCType(j);
    EXPECT_EQ(n["ret-type"]["type-kind"], "struct");
    EXPECT_EQ(n["ret-type"]["type-name"], "Tag");
}

TEST(typecompat, unrepresentable_struct_named) {
    EXPECT_EQ(unrepresentableTypeName({{"type-kind","struct"},{"type-name","Foo"}}), "");
}

TEST(typecompat, unrepresentable_struct_anonymous) {
    EXPECT_EQ(unrepresentableTypeName({{"type-kind","struct"}}), "anonymous struct");
}

TEST(typecompat, unrepresentable_arr) {
    EXPECT_EQ(unrepresentableTypeName({{"type-kind","arr"}}), "array");
}

TEST(typecompat, unrepresentable_union) {
    EXPECT_EQ(unrepresentableTypeName({{"type-kind","union"}}), "union");
}

TEST(typecompat, unrepresentable_enum) {
    EXPECT_EQ(unrepresentableTypeName({{"type-kind","enum"}}), "enum");
}

TEST(typecompat, unrepresentable_strct_named) {
    // c2ast's pre-normalizeCType shape (checked ahead of the strct->struct fold)
    EXPECT_EQ(unrepresentableTypeName({{"type-kind","strct"},{"type-name","Tag"}}), "Tag");
}

TEST(typecompat, unrepresentable_strct_anonymous) {
    EXPECT_EQ(unrepresentableTypeName({{"type-kind","strct"}}), "anonymous struct");
}

TEST(typecompat, unrepresentable_user_named) {
    EXPECT_EQ(unrepresentableTypeName({{"type-kind","user"},{"type-name","mystery_t"}}), "mystery_t");
}

TEST(typecompat, unrepresentable_user_unnamed) {
    EXPECT_EQ(unrepresentableTypeName({{"type-kind","user"}}), "user");
}

TEST(typecompat, unrepresentable_unknown_kind_fallback) {
    EXPECT_EQ(unrepresentableTypeName({{"type-kind","embed"}}), "embed");
}

// -------- usualArithConv --------

TEST(usual_arith_conv, same_signed_higher_rank_wins) {
    PlnTypeRegistry reg;
    const PlnType* i32 = reg.prim(N::Int32);
    const PlnType* i64 = reg.prim(N::Int64);
    EXPECT_EQ(usualArithConv(i32, i64), i64);
    EXPECT_EQ(usualArithConv(i64, i32), i64);  // commutative
}

TEST(usual_arith_conv, same_unsigned_higher_rank_wins) {
    PlnTypeRegistry reg;
    const PlnType* u16 = reg.prim(N::Uint16);
    const PlnType* u32 = reg.prim(N::Uint32);
    EXPECT_EQ(usualArithConv(u16, u32), u32);
    EXPECT_EQ(usualArithConv(u32, u16), u32);
}

TEST(usual_arith_conv, mixed_sign_signed_rank_greater_wins_signed) {
    PlnTypeRegistry reg;
    const PlnType* i64 = reg.prim(N::Int64);
    const PlnType* u32 = reg.prim(N::Uint32);
    EXPECT_EQ(usualArithConv(i64, u32), i64);
    EXPECT_EQ(usualArithConv(u32, i64), i64);
}

TEST(usual_arith_conv, mixed_sign_equal_rank_wins_unsigned) {
    PlnTypeRegistry reg;
    const PlnType* i32 = reg.prim(N::Int32);
    const PlnType* u32 = reg.prim(N::Uint32);
    EXPECT_EQ(usualArithConv(i32, u32), u32);
    EXPECT_EQ(usualArithConv(u32, i32), u32);
}

TEST(usual_arith_conv, mixed_sign_unsigned_rank_greater_wins_unsigned) {
    PlnTypeRegistry reg;
    const PlnType* i8  = reg.prim(N::Int8);
    const PlnType* u32 = reg.prim(N::Uint32);
    EXPECT_EQ(usualArithConv(i8, u32), u32);
    EXPECT_EQ(usualArithConv(u32, i8), u32);
}

TEST(usual_arith_conv, no_integer_promotion_narrow_stays_narrow) {
    // Palan preserves declared-width wraparound; unlike C, int8+int8 does not
    // promote to a machine word.
    PlnTypeRegistry reg;
    const PlnType* i8 = reg.prim(N::Int8);
    EXPECT_EQ(usualArithConv(i8, i8), i8);
}

TEST(usual_arith_conv, float_beats_integer) {
    PlnTypeRegistry reg;
    const PlnType* i64 = reg.prim(N::Int64);
    const PlnType* f32 = reg.prim(N::Float32);
    EXPECT_EQ(usualArithConv(i64, f32), f32);
    EXPECT_EQ(usualArithConv(f32, i64), f32);
}

TEST(usual_arith_conv, wider_float_wins) {
    PlnTypeRegistry reg;
    const PlnType* f32 = reg.prim(N::Float32);
    const PlnType* f64 = reg.prim(N::Float64);
    EXPECT_EQ(usualArithConv(f32, f64), f64);
    EXPECT_EQ(usualArithConv(f64, f32), f64);
    const PlnType* f80 = reg.prim(N::Float80);
    EXPECT_EQ(usualArithConv(f64, f80), f80);
    EXPECT_EQ(typeCompat(f64, f80, reg), TypeCompat::ImplicitWiden);
    EXPECT_EQ(typeCompat(f80, f64, reg), TypeCompat::ExplicitCast);
}

TEST(usual_arith_conv, non_prim_returns_null) {
    PlnTypeRegistry reg;
    const PlnType* i32 = reg.prim(N::Int32);
    const PlnType* pi32 = reg.ptr(i32);
    EXPECT_EQ(usualArithConv(i32, pi32), nullptr);
    EXPECT_EQ(usualArithConv(pi32, pi32), nullptr);
}

TEST(usual_arith_conv, void_returns_null) {
    PlnTypeRegistry reg;
    const PlnType* v   = reg.prim(N::Void);
    const PlnType* i32 = reg.prim(N::Int32);
    EXPECT_EQ(usualArithConv(v, i32), nullptr);
    EXPECT_EQ(usualArithConv(i32, v), nullptr);  // void on either side, not just the first
}

// -------- argConvOk --------

TEST(arg_conv_ok, widening_ok) {
    PlnTypeRegistry reg;
    EXPECT_TRUE(argConvOk(reg.prim(N::Int32), reg.prim(N::Int64)));
}

TEST(arg_conv_ok, narrowing_rejected) {
    PlnTypeRegistry reg;
    EXPECT_FALSE(argConvOk(reg.prim(N::Int64), reg.prim(N::Int32)));
}

TEST(arg_conv_ok, same_width_sign_reinterpret_ok_both_directions) {
    // A call argument only needs to fit the callee's ABI width, so a same-width
    // signedness flip (unlike at a binding site) is allowed both ways.
    PlnTypeRegistry reg;
    EXPECT_TRUE(argConvOk(reg.prim(N::Int32), reg.prim(N::Uint32)));
    EXPECT_TRUE(argConvOk(reg.prim(N::Uint32), reg.prim(N::Int32)));
    EXPECT_TRUE(argConvOk(reg.prim(N::Uint64), reg.prim(N::Int64)));
}

TEST(arg_conv_ok, cross_sign_widen_to_wider_signed_ok) {
    // usualArithConv(int64, uint32) == int64 (rule 3), so passing a uint32
    // argument to an int64 parameter fits without loss.
    PlnTypeRegistry reg;
    EXPECT_TRUE(argConvOk(reg.prim(N::Uint32), reg.prim(N::Int64)));
}

TEST(arg_conv_ok, cross_sign_narrowing_rejected) {
    PlnTypeRegistry reg;
    EXPECT_FALSE(argConvOk(reg.prim(N::Int64), reg.prim(N::Uint32)));
}

TEST(arg_conv_ok, float_narrowing_rejected) {
    PlnTypeRegistry reg;
    EXPECT_FALSE(argConvOk(reg.prim(N::Float64), reg.prim(N::Float32)));
}

TEST(arg_conv_ok, non_prim_rejected) {
    PlnTypeRegistry reg;
    const PlnType* i32 = reg.prim(N::Int32);
    const PlnType* pi32 = reg.ptr(i32);
    EXPECT_FALSE(argConvOk(pi32, i32));
    EXPECT_FALSE(argConvOk(i32, pi32));
}

TEST(arg_conv_ok, void_rejected_either_side) {
    PlnTypeRegistry reg;
    const PlnType* v   = reg.prim(N::Void);
    const PlnType* i32 = reg.prim(N::Int32);
    EXPECT_FALSE(argConvOk(v, i32));
    EXPECT_FALSE(argConvOk(i32, v));
}

// -------- classifySysVStruct: SysV AMD64 by-value struct classification --------
//
// FieldLayout's first six members have no default initializers, so every
// field below is built with positional aggregate init:
//   {name, typeKind, typeName, isMutable, offset, size[, count, elemKind, stride]}

TEST(sysv_struct, one_eightbyte_integer) {
    // div_t shape: { int32 quot; int32 rem; }
    StructDef def;
    def.totalSize = 8;
    def.fields = {
        {"quot", "prim", "int32", false, 0, 4},
        {"rem",  "prim", "int32", false, 4, 4},
    };
    map<string, StructDef> defs;
    auto result = classifySysVStruct(def, defs, false);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].cls,  EightbyteClass::Integer);
    EXPECT_EQ(result[0].size, 8);
}

TEST(sysv_struct, two_eightbyte_integer) {
    // ldiv_t/lldiv_t shape: { int64 quot; int64 rem; }
    StructDef def;
    def.totalSize = 16;
    def.fields = {
        {"quot", "prim", "int64", false, 0, 8},
        {"rem",  "prim", "int64", false, 8, 8},
    };
    map<string, StructDef> defs;
    auto result = classifySysVStruct(def, defs, false);
    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].cls, EightbyteClass::Integer);
    EXPECT_EQ(result[1].cls, EightbyteClass::Integer);
}

TEST(sysv_struct, flo80_memory_or_x87) {
    // A flo80 alone comes back in %st(0); passed, or beside another field, it is MEMORY.
    StructDef one;
    one.totalSize = 16;
    one.fields = { {"v", "prim", "flo80", false, 0, 16} };
    StructDef mix;
    mix.totalSize = 32;
    mix.fields = { {"n", "prim", "int32", false, 0, 4}, {"v", "prim", "flo80", false, 16, 16} };
    map<string, StructDef> defs;
    auto ret = classifySysVStruct(one, defs, true);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0].cls, EightbyteClass::X87);
    EXPECT_EQ(ret[0].size, 16);
    EXPECT_TRUE(classifySysVStruct(one, defs, false).empty());
    EXPECT_TRUE(classifySysVStruct(mix, defs, true).empty());
}

TEST(sysv_struct, one_eightbyte_sse) {
    StructDef def;
    def.totalSize = 8;
    def.fields = { {"x", "prim", "flo64", false, 0, 8} };
    map<string, StructDef> defs;
    auto result = classifySysVStruct(def, defs, false);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].cls, EightbyteClass::Sse);
}

TEST(sysv_struct, two_flo32_one_eightbyte) {
    StructDef def;
    def.totalSize = 8;
    def.fields = {
        {"a", "prim", "flo32", false, 0, 4},
        {"b", "prim", "flo32", false, 4, 4},
    };
    map<string, StructDef> defs;
    auto result = classifySysVStruct(def, defs, false);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].cls, EightbyteClass::Sse);
}

TEST(sysv_struct, integer_wins_within_eightbyte) {
    // Any-INTEGER-wins merge rule: a float field sharing an eightbyte with an
    // integer field classifies the whole eightbyte as INTEGER.
    StructDef def;
    def.totalSize = 8;
    def.fields = {
        {"a", "prim", "flo32", false, 0, 4},
        {"b", "prim", "int32", false, 4, 4},
    };
    map<string, StructDef> defs;
    auto result = classifySysVStruct(def, defs, false);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].cls, EightbyteClass::Integer);
}

TEST(sysv_struct, mixed_integer_sse) {
    StructDef def;
    def.totalSize = 16;
    def.fields = {
        {"a", "prim", "int64", false, 0, 8},
        {"b", "prim", "flo64", false, 8, 8},
    };
    map<string, StructDef> defs;
    auto result = classifySysVStruct(def, defs, false);
    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].cls, EightbyteClass::Integer);
    EXPECT_EQ(result[1].cls, EightbyteClass::Sse);
}

TEST(sysv_struct, partial_tail_eightbyte) {
    // 12 bytes: 2 eightbytes, the second only 4 bytes wide.
    StructDef def;
    def.totalSize = 12;
    def.fields = {
        {"a", "prim", "int32", false, 0, 4},
        {"b", "prim", "int32", false, 4, 4},
        {"c", "prim", "int32", false, 8, 4},
    };
    map<string, StructDef> defs;
    auto result = classifySysVStruct(def, defs, false);
    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].size, 8);
    EXPECT_EQ(result[1].size, 4);
}

TEST(sysv_struct, memory_class) {
    // Over 16 bytes -> MEMORY class (empty vector).
    StructDef def;
    def.totalSize = 24;
    def.fields = {
        {"a", "prim", "int64", false, 0,  8},
        {"b", "prim", "int64", false, 8,  8},
        {"c", "prim", "int64", false, 16, 8},
    };
    map<string, StructDef> defs;
    auto result = classifySysVStruct(def, defs, false);
    EXPECT_TRUE(result.empty());
}

TEST(sysv_struct, fractional_tail_width) {
    StructDef def;
    def.totalSize = 3;
    def.fields = {
        {.name="a", .typeKind="embed-arr", .isMutable=false, .offset=0, .size=3,
         .type={{"type-kind","pntr"},{"arr-size",3},{"base-type",{{"type-kind","prim"},{"type-name","int8"}}}}},
    };
    map<string, StructDef> defs;
    auto result = classifySysVStruct(def, defs, false);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].cls, EightbyteClass::Integer);
    EXPECT_EQ(result[0].size, 3);
}

TEST(sysv_struct, union_float_and_int) {
    // A union's members overlap at offset 0; the INTEGER member wins.
    StructDef def;
    def.totalSize = 4;
    def.fields = {
        {"f", "prim", "flo32", false, 0, 4},
        {"i", "prim", "int32", false, 0, 4},
    };
    map<string, StructDef> defs;
    auto result = classifySysVStruct(def, defs, false);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].cls, EightbyteClass::Integer);
    EXPECT_EQ(result[0].size, 4);
}

TEST(sysv_struct, raw_ptr_field) {
    StructDef def;
    def.totalSize = 8;
    def.fields = { {"p", "raw-ptr", "int32", false, 0, 8} };
    map<string, StructDef> defs;
    auto result = classifySysVStruct(def, defs, false);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].cls, EightbyteClass::Integer);
}

TEST(sysv_struct, owned_field) {
    StructDef def;
    def.totalSize = 8;
    def.fields = { {"p", "owned", "", false, 0, 8} };
    map<string, StructDef> defs;
    auto result = classifySysVStruct(def, defs, false);
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].cls, EightbyteClass::Integer);
}

TEST(sysv_struct, embed_struct_recursion) {
    // Outer { $Inner in; int64 n; } where Inner { flo64 x; } -- classifying
    // Outer must recurse into `defs.at("Inner")` at the embed field's offset.
    StructDef inner;
    inner.totalSize = 8;
    inner.fields = { {"x", "prim", "flo64", false, 0, 8} };

    StructDef outer;
    outer.totalSize = 16;
    outer.fields = {
        {"in", "embed", "Inner", false, 0, 8},
        {"n",  "prim",  "int64", false, 8, 8},
    };

    map<string, StructDef> defs = { {"Inner", inner} };
    auto result = classifySysVStruct(outer, defs, false);
    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].cls, EightbyteClass::Sse);
    EXPECT_EQ(result[1].cls, EightbyteClass::Integer);
}

TEST(sysv_struct, embed_arr_prim_float) {
    // `$[4]flo32 v;` -- an array laid out in the struct.
    StructDef def;
    def.totalSize = 16;
    def.fields = {
        {.name="v", .typeKind="embed-arr", .isMutable=false, .offset=0, .size=16,
         .type={{"type-kind","pntr"},{"arr-size",4},{"base-type",{{"type-kind","prim"},{"type-name","flo32"}}}}},
    };
    map<string, StructDef> defs;
    auto result = classifySysVStruct(def, defs, false);
    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].cls, EightbyteClass::Sse);
    EXPECT_EQ(result[1].cls, EightbyteClass::Sse);
}

TEST(sysv_struct, embed_arr_struct_leaf) {
    // `$[2]$Elem arr;` where Elem { int32 a; int32 b; } -- each element must
    // recurse via `defs.at("Elem")` at its own element offset.
    StructDef elem;
    elem.totalSize = 8;
    elem.fields = {
        {"a", "prim", "int32", false, 0, 4},
        {"b", "prim", "int32", false, 4, 4},
    };

    StructDef def;
    def.totalSize = 16;
    def.fields = {
        {.name="arr", .typeKind="embed-arr", .isMutable=false, .offset=0, .size=16,
         .type={{"type-kind","pntr"},{"arr-size",2},{"embedded",true},{"stride",8},
                {"base-type",{{"type-kind","struct"},{"type-name","Elem"}}}}},
    };

    map<string, StructDef> defs = { {"Elem", elem} };
    auto result = classifySysVStruct(def, defs, false);
    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].cls, EightbyteClass::Integer);
    EXPECT_EQ(result[1].cls, EightbyteClass::Integer);
}

TEST(sysv_struct, embed_arr_rows) {
    // `$[2]$[1]int32 m; flo64 f;` -- the rows laid out in place make the
    // first eightbyte INTEGER, leaving the flo64's SSE.
    json row = {{"type-kind","pntr"},{"arr-size",1},{"base-type",{{"type-kind","prim"},{"type-name","int32"}}}};
    StructDef def;
    def.totalSize = 16;
    def.fields = {
        {.name="m", .typeKind="embed-arr", .isMutable=false, .offset=0, .size=8,
         .type={{"type-kind","pntr"},{"arr-size",2},{"embedded",true},{"stride",4},{"base-type",row}}},
        {.name="f", .typeKind="prim", .typeName="flo64", .isMutable=false, .offset=8, .size=8},
    };
    map<string, StructDef> defs;
    auto result = classifySysVStruct(def, defs, false);
    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].cls, EightbyteClass::Integer);
    EXPECT_EQ(result[1].cls, EightbyteClass::Sse);
}

TEST(sysv_struct, embed_arr_ptr_slots) {
    // `$[2]@!int32 v;` -- pointer slots laid out in the struct, each 8 bytes.
    StructDef def;
    def.totalSize = 16;
    def.fields = {
        {.name="v", .typeKind="embed-arr", .isMutable=false, .offset=0, .size=16,
         .type={{"type-kind","pntr"},{"arr-size",2},{"base-type",{{"type-kind","pntr"},{"mutable",true},
                {"base-type",{{"type-kind","prim"},{"type-name","int32"}}}}}}},
    };
    map<string, StructDef> defs;
    auto result = classifySysVStruct(def, defs, false);
    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].cls, EightbyteClass::Integer);
    EXPECT_EQ(result[1].cls, EightbyteClass::Integer);
}

// -------- bool --------

TEST(bool_type, widens_into_numeric_but_needs_cast_from_it) {
    PlnTypeRegistry reg;
    const PlnType* b = reg.prim(N::Bool);
    EXPECT_EQ(typeCompat(b, reg.prim(N::Int8), reg),    TypeCompat::ImplicitWiden);
    EXPECT_EQ(typeCompat(b, reg.prim(N::Float64), reg), TypeCompat::ImplicitWiden);
    EXPECT_EQ(typeCompat(reg.prim(N::Uint8), b, reg),   TypeCompat::ExplicitCast);
    EXPECT_EQ(typeCompat(reg.prim(N::Float32), b, reg), TypeCompat::ExplicitCast);
}

TEST(bool_type, other_operand_wins_arith_conv) {
    PlnTypeRegistry reg;
    const PlnType* b  = reg.prim(N::Bool);
    const PlnType* i8 = reg.prim(N::Int8);
    EXPECT_EQ(usualArithConv(b, i8), i8);
    EXPECT_EQ(usualArithConv(i8, b), i8);
    EXPECT_EQ(usualArithConv(b, b), b);
}

TEST(bool_type, arg_conv) {
    // Same width as int8/uint8, but that is not a reinterpretation: bool only holds 0/1.
    PlnTypeRegistry reg;
    const PlnType* b = reg.prim(N::Bool);
    EXPECT_TRUE(argConvOk(b, reg.prim(N::Int32)));
    EXPECT_FALSE(argConvOk(reg.prim(N::Int8), b));
    EXPECT_FALSE(argConvOk(reg.prim(N::Uint8), b));
}

// -------- symbols --------

static json params(std::initializer_list<json> types) {
    json ps = json::array();
    for (auto& t : types) ps.push_back({{"var-type", t}});
    return ps;
}
static json prim(const char* n) { return {{"type-kind","prim"},{"type-name",n}}; }
static json strct(const char* n) { return {{"type-kind","struct"},{"type-name",n}}; }
static json pntr(json base) { return {{"type-kind","pntr"},{"base-type",base}}; }
static json pntr(json base, bool mut) { json p = pntr(base); p["mutable"] = mut; return p; }

TEST(symbol, prim_and_user_types) {
    EXPECT_EQ(funcSymbol("f", json::array()), "f.v");
    EXPECT_EQ(funcSymbol("show", params({prim("int32")})), "show.int32");
    EXPECT_EQ(funcSymbol("show", params({prim("flo64"), prim("bool")})), "show.flo64.bool");
    EXPECT_EQ(funcSymbol("f", params({strct("Point")})), "f.5Point");
    json color = prim("int32");
    color["enum"] = "Color";
    EXPECT_EQ(funcSymbol("f", params({color})), "f.N5Colorint32");
}

TEST(symbol, pointer_permissions) {
    // Owned or by value, read borrow and write borrow are different parameters.
    EXPECT_EQ(funcSymbol("f", params({pntr(strct("P"))})), "f.P1P");
    EXPECT_EQ(funcSymbol("f", params({pntr(strct("P"), false)})), "f.R1P");
    EXPECT_EQ(funcSymbol("f", params({pntr(strct("P"), true)})), "f.W1P");
    EXPECT_EQ(funcSymbol("f", params({pntr(pntr(prim("int8"), true))})), "f.PWint8");
}

TEST(symbol, array_shapes) {
    json rows = pntr(prim("int32"), false);
    rows["arr-size"] = 4;
    EXPECT_EQ(funcSymbol("f", params({rows})), "f.A4_Rint32");
    json embed = pntr(strct("P"));
    embed["embedded"] = true;
    embed["stride"] = 16;
    EXPECT_EQ(funcSymbol("f", params({embed})), "f.ES16_P1P");
}

// A variadic parameter is passed as an address and a count, so it is told
// apart from a borrowed array and from that array followed by an int64.
TEST(symbol, variadic) {
    json xs = {{"type-kind","pntr"},{"variadic",true},{"base-type",prim("int64")}};
    EXPECT_EQ(funcSymbol("f", params({xs})), "f.Xint64");
    EXPECT_NE(funcSymbol("f", params({xs})), funcSymbol("f", params({pntr(prim("int64"), false)})));
    EXPECT_NE(funcSymbol("f", params({xs})),
              funcSymbol("f", params({pntr(prim("int64"), false), prim("int64")})));
}

TEST(typecompat, variadic_func_type) {
    PlnTypeRegistry reg;
    json xs = {{"type-kind","pntr"},{"variadic",true},{"base-type",prim("int64")}};
    json variadic = {{"type-kind","func"},{"parameters",params({xs})},{"ret-type",prim("void")}};
    json borrow = {{"type-kind","func"},{"parameters",params({pntr(prim("int64"), false)})},{"ret-type",prim("void")}};
    const PlnType* v = reg.fromJson(pntr(variadic));
    const PlnType* b = reg.fromJson(pntr(borrow));
    EXPECT_NE(v, b);
    EXPECT_EQ(typeCompat(v, b, reg), TypeCompat::Incompatible);
    EXPECT_EQ(reg.toJson(static_cast<const PtrType*>(v)->base)["parameters"][0]["var-type"]["variadic"], true);
}

TEST(symbol, func_pointer) {
    json fn = {{"type-kind","func"},{"parameters",params({prim("int32"), pntr(prim("void"), false)})},
               {"ret-type",prim("int32")}};
    EXPECT_EQ(funcSymbol("apply", params({pntr(fn, false)})), "apply.RF2_int32Rvoidint32");
    json proc = {{"type-kind","func"},{"parameters",json::array()},{"ret-type",prim("void")}};
    EXPECT_EQ(funcSymbol("on", params({pntr(proc, false)})), "on.RF0_void");
}
