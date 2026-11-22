/// Palan Type System — PlnTypeRegistry and typeCompat() implementation
///
/// @file PlnType.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include "PlnType.h"
#include <stdexcept>
#include <cctype>
#include <set>
#include <cstdint>

// Singleton that holds bidirectional maps between type-name strings and PrimType::Name.
// Using a struct with an explicit constructor body so gcov tracks each entry individually.
namespace {
struct PrimTypeNames {
    std::map<std::string, PrimType::Name> toEnum;
    std::map<PrimType::Name, std::string> fromEnum;

    PrimTypeNames() {
        toEnum["int8"]   = PrimType::Name::Int8;
        toEnum["int16"]  = PrimType::Name::Int16;
        toEnum["int32"]  = PrimType::Name::Int32;
        toEnum["int64"]  = PrimType::Name::Int64;
        toEnum["uint8"]  = PrimType::Name::Uint8;
        toEnum["uint16"] = PrimType::Name::Uint16;
        toEnum["uint32"] = PrimType::Name::Uint32;
        toEnum["uint64"] = PrimType::Name::Uint64;
        toEnum["flo32"]  = PrimType::Name::Float32;
        toEnum["flo64"]  = PrimType::Name::Float64;
        toEnum["flo80"]  = PrimType::Name::Float80;
        toEnum["bool"]   = PrimType::Name::Bool;
        toEnum["void"]   = PrimType::Name::Void;
        for (auto& [k, v] : toEnum) fromEnum[v] = k;
    }

    static const PrimTypeNames& instance() {
        static PrimTypeNames inst;
        return inst;
    }
};
} // namespace

// PlnTypeRegistry implementation

const PrimType* PlnTypeRegistry::prim(PrimType::Name name)
{
    auto it = primCache_.find(name);
    if (it != primCache_.end()) return it->second.get();
    auto [ins, ok] = primCache_.emplace(name, std::make_unique<PrimType>(name));
    return ins->second.get();
} // LCOV_EXCL_BR_LINE

const PtrType* PlnTypeRegistry::ptr(const PlnType* base)
{
    auto it = ptrCache_.find(base);
    if (it != ptrCache_.end()) return it->second.get();
    auto [ins, ok] = ptrCache_.emplace(base, std::make_unique<PtrType>(base));
    return ins->second.get();
} // LCOV_EXCL_BR_LINE

const VarrType* PlnTypeRegistry::varr(const PlnType* elem, bool embedded)
{
    auto it = varrCache_.find({elem, embedded});
    if (it != varrCache_.end()) return it->second.get();
    auto [ins, ok] = varrCache_.emplace(std::make_pair(elem, embedded), std::make_unique<VarrType>(elem, embedded));
    return ins->second.get();
} // LCOV_EXCL_BR_LINE

const DictType* PlnTypeRegistry::dict(const PlnType* elem)
{
    auto it = dictCache_.find(elem);
    if (it != dictCache_.end()) return it->second.get();
    auto [ins, ok] = dictCache_.emplace(elem, std::make_unique<DictType>(elem));
    return ins->second.get();
} // LCOV_EXCL_BR_LINE

const StructType* PlnTypeRegistry::structType(const std::string& name)
{
    auto it = structCache_.find(name);
    if (it != structCache_.end()) return it->second.get();
    auto [ins, ok] = structCache_.emplace(name, std::make_unique<StructType>(name));
    return ins->second.get();
} // LCOV_EXCL_BR_LINE

const EnumType* PlnTypeRegistry::enumType(const std::string& name, const PrimType* base)
{
    auto it = enumCache_.find(name);
    if (it != enumCache_.end()) return it->second.get();
    auto [ins, ok] = enumCache_.emplace(name, std::make_unique<EnumType>(name, base));
    return ins->second.get();
} // LCOV_EXCL_BR_LINE

const FuncType* PlnTypeRegistry::funcType(const std::vector<const PlnType*>& params, const PlnType* ret,
                                          bool variadic)
{
    auto key = std::make_tuple(params, ret, variadic);
    auto it = funcCache_.find(key);
    if (it != funcCache_.end()) return it->second.get();
    auto [ins, ok] = funcCache_.emplace(key, std::make_unique<FuncType>(params, ret, variadic));
    return ins->second.get();
} // LCOV_EXCL_BR_LINE

// A by-value struct or array can't cross an indirect call: it would need the
// callee's ownership convention, which a function pointer does not carry.
static bool isFuncSigValueType(const json& t)
{
    std::string kind = t.value("type-kind", "");
    return kind == "prim" || kind == "pntr";
}

// Kept in sync with fromJson's three accepted shapes below by construction:
// fromJson calls this first and refuses to proceed unless it returns "", so
// the two cannot silently drift apart the way a second, independently
// maintained predicate could.
std::string unrepresentableTypeName(const json& j)
{
    if (!j.is_object() || !j.contains("type-kind")) // LCOV_EXCL_BR_LINE -- no real producer emits this
        return "malformed type"; // LCOV_EXCL_LINE
    std::string kind = j["type-kind"].get<std::string>();
    if (kind == "prim") {
        std::string tname = j.value("type-name", "");
        if (tname.empty()) return "malformed type"; // LCOV_EXCL_BR_LINE -- no real producer emits this
        return PrimTypeNames::instance().toEnum.count(tname) ? "" : tname; // LCOV_EXCL_BR_LINE
    }
    if (kind == "pntr" || kind == "varr" || kind == "strdict")
        return j.contains("base-type") ? unrepresentableTypeName(j["base-type"]) : "malformed type"; // LCOV_EXCL_BR_LINE
    if (kind == "struct")
        return j.contains("type-name") ? "" : "anonymous struct"; // LCOV_EXCL_BR_LINE -- normalizeCType only folds a named tag to "struct"
    if (kind == "arr")   return "array"; // LCOV_EXCL_BR_LINE -- arrays always decay to pntr before reaching here
    if (kind == "func") {
        for (auto& p : j.value("parameters", json::array())) {
            if (!p.contains("var-type") || !isFuncSigValueType(p["var-type"])) return "function pointer";
            const json& pt = p["var-type"];
            if (pt.value("type-name", "") == "void" || !unrepresentableTypeName(pt).empty()) return "function pointer";
        }
        if (!j.contains("ret-type") || !isFuncSigValueType(j["ret-type"])
            || !unrepresentableTypeName(j["ret-type"]).empty())
            return "function pointer";
        return "";
    }
    if (kind == "union") return "union";
    if (kind == "enum")  return "enum";
    // "strct": c2ast's struct tag. normalizeCType folds a named one to
    // "struct", so only a nameless one (e.g. `void f(struct { int a; } *p);`)
    // reaches here from a cinclude'd signature.
    if (kind == "strct") return j.value("type-name", "anonymous struct"); // LCOV_EXCL_BR_LINE -- a named strct is folded away first
    // "user": a typedef name c2ast could not resolve to a known underlying
    // type -- report the name itself, it is more useful than "user".
    if (kind == "user")  return j.value("type-name", "user"); // LCOV_EXCL_BR_LINE
    return kind;
} // LCOV_EXCL_BR_LINE

const PlnType* PlnTypeRegistry::fromJson(const json& j)
{
    std::string bad = unrepresentableTypeName(j);
    if (!bad.empty()) // LCOV_EXCL_BR_LINE -- callers only reach here with an already-validated type
        throw std::runtime_error("unrepresentable type: " + bad); // LCOV_EXCL_LINE
    std::string kind = j["type-kind"].get<std::string>();
    if (kind == "prim") {
        auto& toEnum = PrimTypeNames::instance().toEnum;
        const PrimType* p = prim(toEnum.at(j["type-name"].get<std::string>()));
        if (j.contains("enum")) return enumType(j["enum"].get<std::string>(), p);
        return p;
    }
    if (kind == "pntr")
        return ptr(fromJson(j["base-type"]));
    if (kind == "varr")
        return varr(fromJson(j["base-type"]), j.value("embedded", false));
    if (kind == "strdict")
        return dict(fromJson(j["base-type"]));
    if (kind == "func") {
        std::vector<const PlnType*> params;
        for (auto& p : j["parameters"]) params.push_back(fromJson(p["var-type"]));
        const json& ps = j["parameters"];
        bool variadic = !ps.empty() && ps.back().contains("var-type") && ps.back()["var-type"].value("variadic", false); // LCOV_EXCL_BR_LINE
        return funcType(params, fromJson(j["ret-type"]), variadic);
    }
    // kind == "struct" (the only remaining possibility once unrepresentableTypeName
    // returns "")
    return structType(j["type-name"].get<std::string>());
} // LCOV_EXCL_BR_LINE

json PlnTypeRegistry::toJson(const PlnType* t)
{
    if (t->kind == PlnType::Kind::Prim) {
        const auto* p = static_cast<const PrimType*>(t);
        auto& fromEnum = PrimTypeNames::instance().fromEnum;
        return {{"type-kind", "prim"}, {"type-name", fromEnum.at(p->name)}}; // LCOV_EXCL_BR_LINE
    }
    if (t->kind == PlnType::Kind::Enum) {
        const auto* e = static_cast<const EnumType*>(t);
        json j = toJson(e->base);
        j["enum"] = e->name;
        return j;
    }
    if (t->kind == PlnType::Kind::Ptr) {
        const auto* p = static_cast<const PtrType*>(t);
        return {{"type-kind", "pntr"}, {"base-type", toJson(p->base)}}; // LCOV_EXCL_BR_LINE
    }
    if (t->kind == PlnType::Kind::Varr) {
        const auto* v = static_cast<const VarrType*>(t);
        json j = {{"type-kind", "varr"}, {"base-type", toJson(v->elem)}}; // LCOV_EXCL_BR_LINE
        if (v->embedded) j["embedded"] = true;
        return j;
    }
    if (t->kind == PlnType::Kind::Dict)
        return {{"type-kind", "strdict"}, {"base-type", toJson(static_cast<const DictType*>(t)->elem)}}; // LCOV_EXCL_BR_LINE
    if (t->kind == PlnType::Kind::Struct) {
        const auto* s = static_cast<const StructType*>(t);
        return {{"type-kind", "struct"}, {"type-name", s->name}}; // LCOV_EXCL_BR_LINE
    }
    if (t->kind == PlnType::Kind::Func) {
        const auto* f = static_cast<const FuncType*>(t);
        json params = json::array();
        for (auto* p : f->params) params.push_back({{"var-type", toJson(p)}}); // LCOV_EXCL_BR_LINE
        if (f->variadic) params.back()["var-type"]["variadic"] = true;
        return {{"type-kind", "func"}, {"parameters", params}, {"ret-type", toJson(f->ret)}}; // LCOV_EXCL_BR_LINE
    }
    throw std::runtime_error("unknown PlnType::Kind"); // LCOV_EXCL_LINE
} // LCOV_EXCL_BR_LINE

// typeCompat implementation

static int primGroup(PrimType::Name n)
{
    // 0 = signed, 1 = unsigned, 2 = float, 3 = bool
    using N = PrimType::Name;
    switch (n) {
        case N::Int8: case N::Int16: case N::Int32: case N::Int64:   return 0;
        case N::Uint8: case N::Uint16: case N::Uint32: case N::Uint64: return 1;
        case N::Float32: case N::Float64: case N::Float80:             return 2;
        case N::Bool:                                                  return 3;
    }
    return -1; // LCOV_EXCL_LINE
}

static int primRank(PrimType::Name n)
{
    using N = PrimType::Name;
    switch (n) {
        case N::Int8:   case N::Uint8:   return 1;
        case N::Int16:  case N::Uint16:  return 2;
        case N::Int32:  case N::Uint32:  return 3;
        case N::Int64:  case N::Uint64:  return 4;
        case N::Float32:                 return 1;
        case N::Float64:                 return 2;
        case N::Float80:                 return 3;
    }
    return -1; // LCOV_EXCL_LINE
}

TypeCompat typeCompat(const PlnType* from, const PlnType* to,
                      const PlnTypeRegistry& registry)
{
    if (from == to) return TypeCompat::Identical;

    // Like bool, an enum widens out implicitly (as its base type, which shares
    // its representation), but entering one from anything else needs Name(x).
    if (to->kind == PlnType::Kind::Enum) {
        if (from->kind == PlnType::Kind::Enum) return TypeCompat::ExplicitCast;
        if (from->kind != PlnType::Kind::Prim) return TypeCompat::Incompatible;
        int g = primGroup(static_cast<const PrimType*>(from)->name);
        return (g == 0 || g == 1 || g == 3) ? TypeCompat::ExplicitCast : TypeCompat::Incompatible;
    }
    if (from->kind == PlnType::Kind::Enum) {
        TypeCompat c = typeCompat(static_cast<const EnumType*>(from)->base, to, registry);
        return c == TypeCompat::Identical ? TypeCompat::ImplicitWiden : c;
    }

    if (from->kind == PlnType::Kind::Prim && to->kind == PlnType::Kind::Prim) {
        const auto* pf = static_cast<const PrimType*>(from);
        const auto* pt = static_cast<const PrimType*>(to);

        int gf = primGroup(pf->name);
        int gt = primGroup(pt->name);
        // bool holds only 0/1: it widens into any numeric type, but entering
        // it from one needs bool(x), which tests != 0 instead of truncating.
        if (gf == 3) return TypeCompat::ImplicitWiden;
        if (gt == 3) return TypeCompat::ExplicitCast;
        // Integer (signed or unsigned) → float: implicit widening allowed.
        // Float → integer and cross-signedness require explicit cast.
        if ((gf == 0 || gf == 1) && gt == 2) return TypeCompat::ImplicitWiden;
        if (gf != gt) return TypeCompat::ExplicitCast;

        int rf = primRank(pf->name);
        int rt = primRank(pt->name);
        if (rt > rf) return TypeCompat::ImplicitWiden;
        return TypeCompat::ExplicitCast;  // narrowing
    }

    if (from->kind == PlnType::Kind::Ptr && to->kind == PlnType::Kind::Ptr) {
        // base pointers are interned so pointer equality is sufficient
        const auto* pf = static_cast<const PtrType*>(from);
        const auto* pt = static_cast<const PtrType*>(to);
        if (pf->base == pt->base) return TypeCompat::Identical;
        // A header is not its elements, so it must not pass as `@void`.
        auto isHeader = [](const PlnType* b) { return b->kind == PlnType::Kind::Varr || b->kind == PlnType::Kind::Dict; };
        if (isHeader(pf->base) || isHeader(pt->base))
            return TypeCompat::Incompatible;

        bool fromIsVoid = pf->base->kind == PlnType::Kind::Prim
            && static_cast<const PrimType*>(pf->base)->name == PrimType::Name::Void;
        bool toIsVoid = pt->base->kind == PlnType::Kind::Prim
            && static_cast<const PrimType*>(pt->base)->name == PrimType::Name::Void;
        if (fromIsVoid || toIsVoid) return TypeCompat::Identical;  // pntr(void) is bidirectionally compatible with any pntr(T)

        // Functions match when each parameter and the return would: a pointer
        // passes through unconverted, so pntr(void) and pntr(T) are as good as
        // the same type there too.
        if (pf->base->kind == PlnType::Kind::Func && pt->base->kind == PlnType::Kind::Func) {
            const auto* ff = static_cast<const FuncType*>(pf->base);
            const auto* ft = static_cast<const FuncType*>(pt->base);
            if (ff->params.size() != ft->params.size() || ff->variadic != ft->variadic) return TypeCompat::Incompatible;
            for (size_t i = 0; i < ff->params.size(); i++)
                if (typeCompat(ft->params[i], ff->params[i], registry) != TypeCompat::Identical)
                    return TypeCompat::Incompatible;
            return typeCompat(ff->ret, ft->ret, registry) == TypeCompat::Identical
                ? TypeCompat::Identical : TypeCompat::Incompatible;
        }

        // pntr(int8) <-> pntr(uint8) is Identical for string/byte-buffer C
        // interop (PalanReference.md "Passing to C Functions"); the scalar
        // types themselves remain distinct and still need an explicit cast.
        if (pf->base->kind == PlnType::Kind::Prim && pt->base->kind == PlnType::Kind::Prim) {
            auto bf = static_cast<const PrimType*>(pf->base)->name;
            auto bt = static_cast<const PrimType*>(pt->base)->name;
            bool fromIsByte = bf == PrimType::Name::Int8 || bf == PrimType::Name::Uint8;
            bool toIsByte   = bt == PrimType::Name::Int8 || bt == PrimType::Name::Uint8;
            if (fromIsByte && toIsByte) return TypeCompat::Identical;
        }

        return TypeCompat::Incompatible;
    }

    return TypeCompat::Incompatible;
}

const PlnType* usualArithConv(const PlnType* a, const PlnType* b)
{
    if (a->kind != PlnType::Kind::Prim || b->kind != PlnType::Kind::Prim) return nullptr;
    const auto* pa = static_cast<const PrimType*>(a);
    const auto* pb = static_cast<const PrimType*>(b);

    int ga = primGroup(pa->name), gb = primGroup(pb->name);
    if (ga < 0 || gb < 0) return nullptr; // Void is not a valid operand type; LCOV_EXCL_BR_LINE -- already rejected upstream (E_VoidCallUsedAsValue)

    // bool ranks below every other type, so the other side always wins.
    if (ga == 3) return b;
    if (gb == 3) return a;

    // 1. Either side float -> the wider float wins (both sides float: wider; one
    //    side integer: the float side, per the existing int-to-float ImplicitWiden rule).
    if (ga == 2 || gb == 2) {
        if (ga == 2 && gb == 2) return primRank(pa->name) >= primRank(pb->name) ? a : b;
        return ga == 2 ? a : b;
    }
    // 2. Same signedness -> higher rank wins.
    if (ga == gb) return primRank(pa->name) >= primRank(pb->name) ? a : b;
    // 3/4. Mixed signedness: signed wins only if its rank is strictly greater
    //      than the unsigned side's rank; otherwise the unsigned type wins
    //      (matches C's usual arithmetic conversions on same-size ranks).
    const PlnType* signedT   = (ga == 0) ? a : b;
    const PlnType* unsignedT = (ga == 0) ? b : a;
    auto signedName   = static_cast<const PrimType*>(signedT)->name;
    auto unsignedName = static_cast<const PrimType*>(unsignedT)->name;
    return primRank(signedName) > primRank(unsignedName) ? signedT : unsignedT;
}

bool argConvOk(const PlnType* from, const PlnType* to)
{
    if (to->kind == PlnType::Kind::Enum) return false;
    if (from->kind == PlnType::Kind::Enum) from = static_cast<const EnumType*>(from)->base;
    if (from == to) return true;
    if (usualArithConv(from, to) == to) return true;
    if (from->kind != PlnType::Kind::Prim || to->kind != PlnType::Kind::Prim) return false;
    const auto* pf = static_cast<const PrimType*>(from);
    const auto* pt = static_cast<const PrimType*>(to);
    int gf = primGroup(pf->name), gt = primGroup(pt->name);
    if (gf < 0 || gt < 0 || gf == 2 || gt == 2) return false;  // float pairs handled above
    if (gt == 3) return false;  // int8 -> bool is not a reinterpretation
    return primRank(pf->name) == primRank(pt->name);           // same-width sign reinterpretation
}

// Symbol elements

static std::string symbolUserName(const std::string& name)
{
    return std::to_string(name.size()) + name;
}


// A pointer's mutable key is three-way: absent for an owned or by-value
// parameter, false for a read borrow, true for a write borrow.
static std::string symbolPntr(const json& t)
{
    // LCOV_EXCL_BR_START
    // A variadic parameter is its elements' address and count, not a pointer.
    if (t.value("variadic", false)) return "X" + symbolType(t["base-type"]);
    std::string s;
    if (t.value("embedded", false)) s += "E";
    if (t.contains("stride")) s += "S" + std::to_string(t["stride"].get<int64_t>()) + "_";
    if (t.contains("arr-size")) s += "A" + std::to_string(t["arr-size"].get<int64_t>()) + "_";
    if (!t.contains("mutable")) s += "P";
    else s += t["mutable"].get<bool>() ? "W" : "R";
    return s + symbolType(t["base-type"]);
    // LCOV_EXCL_BR_STOP
} // LCOV_EXCL_BR_LINE

std::string symbolType(const json& t)
{
    // LCOV_EXCL_BR_START
    static const std::map<std::string, std::set<std::string>> knownKeys = {
        {"prim",   {"type-kind", "type-name", "enum"}},
        {"struct", {"type-kind", "type-name"}},
        {"pntr",   {"type-kind", "base-type", "mutable", "embedded", "stride", "arr-size", "variadic"}},
        {"func",   {"type-kind", "parameters", "ret-type"}},
        {"varr",   {"type-kind", "base-type", "embedded"}},
        {"strdict", {"type-kind", "base-type"}},
    };
    const std::string kind = t.value("type-kind", "");
    auto known = knownKeys.find(kind);
    // Every key a type carries tells overloads apart, so one the encoding
    // skips could give two functions the same symbol.
    if (known == knownKeys.end()) throw std::logic_error("symbol: type-kind " + kind); // LCOV_EXCL_LINE
    for (auto& [k, v] : t.items())
        if (!known->second.count(k)) throw std::logic_error("symbol: " + kind + " key " + k); // LCOV_EXCL_LINE

    if (kind == "prim") {
        std::string prim = t["type-name"].get<std::string>();
        return t.contains("enum") ? "N" + symbolUserName(t["enum"].get<std::string>()) + prim : prim;
    }
    if (kind == "struct") return symbolUserName(t["type-name"].get<std::string>());
    if (kind == "pntr") return symbolPntr(t);
    if (kind == "varr") return std::string(t.value("embedded", false) ? "VC" : "V") + symbolType(t["base-type"]);
    if (kind == "strdict") return "D" + symbolType(t["base-type"]);
    const json& params = t["parameters"];
    std::string s = "F" + std::to_string(params.size()) + "_";
    for (auto& p : params) s += symbolType(p["var-type"]);
    return s + symbolType(t["ret-type"]);
    // LCOV_EXCL_BR_STOP
} // LCOV_EXCL_BR_LINE

const std::map<std::string, std::string>& operatorFuncNames()
{
    // LCOV_EXCL_BR_START
    // A digit-led word: a label cannot hold the operator, and no written
    // function name starts with a digit.
    static const std::map<std::string, std::string> names = {
        {"+", "0add"}, {"-", "0sub"}, {"*", "0mul"}, {"/", "0div"}, {"%", "0mod"},
        {"==", "0eq"}, {"<", "0lt"}, {"<<", "0app"},
    };
    // LCOV_EXCL_BR_STOP
    return names;
} // LCOV_EXCL_BR_LINE

std::string copyFuncSymbol(const std::string& typeName)
{
    return "__pln_cp_" + symbolType({{"type-kind", "struct"}, {"type-name", typeName}}); // LCOV_EXCL_BR_LINE
} // LCOV_EXCL_BR_LINE

std::string fieldInitSymbol(const std::string& typeName)
{
    return "__pln_fi_" + symbolType({{"type-kind", "struct"}, {"type-name", typeName}}); // LCOV_EXCL_BR_LINE
} // LCOV_EXCL_BR_LINE

std::string releaseFuncSymbol(const std::string& typeName)
{
    return "__pln_rl_" + symbolType({{"type-kind", "struct"}, {"type-name", typeName}}); // LCOV_EXCL_BR_LINE
} // LCOV_EXCL_BR_LINE

std::string funcSymbol(const std::string& name, const json& params)
{
    // LCOV_EXCL_BR_START
    auto op = operatorFuncNames().find(name);
    std::string s = op != operatorFuncNames().end() ? op->second : name;
    if (params.empty()) return s + ".v";
    for (auto& p : params) s += "." + symbolType(p["var-type"]);
    return s;
    // LCOV_EXCL_BR_STOP
} // LCOV_EXCL_BR_LINE
