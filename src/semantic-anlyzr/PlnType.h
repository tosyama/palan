/// Palan Type System — PlnType class hierarchy, PlnTypeRegistry, and typeCompat()
///
/// @file PlnType.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once

#include <map>
#include <memory>
#include <tuple>
#include <vector>
#include "../../lib/json/single_include/nlohmann/json.hpp"

using json = nlohmann::json;

// Result of type compatibility check
enum class TypeCompat {
    Identical,      // same type
    ImplicitWiden,  // implicit widening (SA inserts convert node)
    ExplicitCast,   // explicit cast required (narrowing or signed<->unsigned)
    Incompatible,   // no conversion possible
};

// Type base class
struct PlnType {
    enum class Kind { Prim, Ptr, Struct, Enum, Func, Varr } kind;
    virtual ~PlnType() = default;
protected:
    PlnType(Kind k) : kind(k) {}
};

// Primitive type
struct PrimType : PlnType {
    enum class Name {
        Int8, Int16, Int32, Int64,
        Uint8, Uint16, Uint32, Uint64,
        Float32, Float64,
        Bool,       // 1 byte, value always 0 or 1
        Void        // pntr(void) base only — never used as a standalone value type
    } name;
    PrimType(Name n) : PlnType(Kind::Prim), name(n) {}
};

// Pointer type
struct PtrType : PlnType {
    const PlnType* base;
    PtrType(const PlnType* b) : PlnType(Kind::Ptr), base(b) {}
};

// Struct type (opaque; used as base for struct pointer types)
struct StructType : PlnType {
    std::string name;
    StructType(std::string n) : PlnType(Kind::Struct), name(std::move(n)) {}
};

// Enum type: stored as its base integer prim, but a distinct type for checking.
struct EnumType : PlnType {
    std::string name;
    const PrimType* base;
    EnumType(std::string n, const PrimType* b) : PlnType(Kind::Enum), name(std::move(n)), base(b) {}
};

// Function type: only ever a pointer's base (a `@func(...)` value). Pointer
// permissions of the parameters and return are not part of the identity; they
// are checked on the JSON (ptrPermission), as for any other pointer.
struct FuncType : PlnType {
    std::vector<const PlnType*> params;
    const PlnType* ret;  // Void prim when nothing is returned
    bool variadic;       // the last parameter is `[...]E`, passed as its elements' address and count
    FuncType(std::vector<const PlnType*> p, const PlnType* r, bool v)
        : PlnType(Kind::Func), params(std::move(p)), ret(r), variadic(v) {}
};

// A `[+]T` header {len, cap, data}: only ever a pointer's base, since a
// variable-length array variable is one word pointing at its header.
struct VarrType : PlnType {
    const PlnType* elem;
    bool embedded;
    VarrType(const PlnType* e, bool emb) : PlnType(Kind::Varr), elem(e), embedded(emb) {}
};

// Type registry — owns all type instances and interns them
class PlnTypeRegistry {
    std::map<PrimType::Name, std::unique_ptr<PrimType>>  primCache_;
    std::map<const PlnType*, std::unique_ptr<PtrType>>   ptrCache_;
    std::map<std::pair<const PlnType*, bool>, std::unique_ptr<VarrType>> varrCache_;
    std::map<std::string, std::unique_ptr<StructType>>   structCache_;
    std::map<std::string, std::unique_ptr<EnumType>>     enumCache_;
    std::map<std::tuple<std::vector<const PlnType*>, const PlnType*, bool>, std::unique_ptr<FuncType>> funcCache_;
public:
    const PrimType*  prim(PrimType::Name name);
    const PtrType*   ptr(const PlnType* base);
    const VarrType*  varr(const PlnType* elem, bool embedded);
    const StructType* structType(const std::string& name);
    const EnumType*  enumType(const std::string& name, const PrimType* base);
    const FuncType*  funcType(const std::vector<const PlnType*>& params, const PlnType* ret, bool variadic = false);
    const PlnType*   fromJson(const json& j);
    json             toJson(const PlnType* t);
};

// Mirrors PlnTypeRegistry::fromJson's accepted domain (prim with a known
// type-name, pntr, struct with a type-name, and a func whose parameters and
// return are scalars or pointers) without interning anything, so a
// caller can diagnose an unrepresentable type before treating a signature as
// callable instead of relying on fromJson's exception. Returns "" when
// fromJson(j) would succeed; otherwise a short display name for the first
// unrepresentable node found (recursing through a pntr chain's base-type).
std::string unrepresentableTypeName(const json& j);

// Type compatibility check
TypeCompat typeCompat(const PlnType* from, const PlnType* to,
                      const PlnTypeRegistry& registry);

// Usual arithmetic conversions (C-style) for a binary operator's operand
// pair. Returns the common type both operands convert to -- always `a` or
// `b` itself, so unlike variadicPromote() this needs no registry to intern a
// new type. Returns nullptr if either side is not Prim, or is Void (illegal
// as an operand type). Does not perform C's integer promotion to `int`: a
// narrow width (e.g. int8+int8) stays narrow, since Palan preserves
// declared-width wraparound rather than promoting to a machine word.
const PlnType* usualArithConv(const PlnType* a, const PlnType* b);

// Whether an argument of type `from` may be passed to a parameter of type
// `to` without an explicit cast. Distinct from typeCompat/usualArithConv:
// a call argument only needs to fit the callee's ABI width, not match a
// declared variable's exact type, so this also accepts a same-width
// signedness reinterpretation (e.g. int32 -> uint32) that a binding site
// (var-decl/assign/return) rejects.
bool argConvOk(const PlnType* from, const PlnType* to);

// Link-name elements of a Palan function, joined by ".." into its symbol.
// Each element uses only [A-Za-z0-9_.] and never contains "..", so the scope
// path can be split back; within a function element every type is
// self-delimiting (lowercase prim, digit-led user name, uppercase modifier).
std::string funcSymbol(const std::string& name, const json& params);
// Each operator a function can be named by, with the word its symbol uses.
const std::map<std::string, std::string>& operatorFuncNames();
// The symbol of struct typeName's '->': a C identifier, since the generated
// allocators declare it in C to call it. A type has one '->', so its name,
// the type's identity in the program, is enough, wherever an interface puts it.
std::string copyFuncSymbol(const std::string& typeName);
std::string releaseFuncSymbol(const std::string& typeName);
// A type as one identifier-safe word, distinct for distinct types.
std::string symbolType(const json& t);
