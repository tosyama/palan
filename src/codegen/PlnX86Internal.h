/// Shared x86-64 emission helpers used across PlnX86CodeGen implementation files.
///
/// @file PlnX86Internal.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once
#include <string>
#include <array>
#include <map>
#include <cctype>
#include <boost/assert.hpp>
#include "PlnVProg.h"
#include "PlnRegAlloc.h"

using namespace std;

inline const char* movInstrForType(VRegType type) {
    switch (type) {
        case VRegType::Int8:
        case VRegType::Uint8:   return "movb";
        case VRegType::Int16:
        case VRegType::Uint16:  return "movw";
        case VRegType::Int32:
        case VRegType::Uint32:  return "movl";
        case VRegType::Float32: return "movss";
        case VRegType::Float64: return "movsd";
        case VRegType::Float80: return "movdqu";
        default:                return "movq";
    }
}

// A value moved through an XMM register: an SSE float, or a flo80's 16 bytes.
inline bool movedViaXmm(VRegType t) { return isFloat(t) || isX87(t); }

inline const char* widthSuffix(int width) {
    switch (width) {
        case 1:  return "b";
        case 2:  return "w";
        case 4:  return "l";
        default: return "q";
    }
}

// mov(s|z)<fromSuffix><toSuffix> — e.g. extendMnemonic(true, 1, 8) => "movsbq"
inline string extendMnemonic(bool sourceSigned, int fromWidth, int toWidth) {
    return string("mov") + (sourceSigned ? "s" : "z") + widthSuffix(fromWidth) + widthSuffix(toWidth);
}

// Integer ALU mnemonic for `base` at the width of `type`, e.g. intMnemonic("add", Uint32) => "addl".
// Deriving the suffix from intWidth()/widthSuffix() instead of enumerating every VRegType keeps
// signed and unsigned integer widths in sync by construction.
inline string intMnemonic(const char* base, VRegType type) {
    return string(base) + widthSuffix(intWidth(type));
}

// The x87 forms pop %st (the right operand) into %st(1) (the left); AT&T's
// fsubrp/fdivrp are what Intel calls fsubp/fdivp, so they compute left op right.
inline string addInstrForType(VRegType type) {
    if (type == VRegType::Float80) return "faddp";
    if (type == VRegType::Float32) return "addss";
    if (type == VRegType::Float64) return "addsd";
    return intMnemonic("add", type);
}

inline string subInstrForType(VRegType type) {
    if (type == VRegType::Float80) return "fsubrp";
    if (type == VRegType::Float32) return "subss";
    if (type == VRegType::Float64) return "subsd";
    return intMnemonic("sub", type);
}

// imulb has no 2-operand form; emitInstrMul handles the 1-byte case itself.
inline string mulInstrForType(VRegType type) {
    if (type == VRegType::Float80) return "fmulp";
    if (type == VRegType::Float32) return "mulss";
    if (type == VRegType::Float64) return "mulsd";
    BOOST_ASSERT(intWidth(type) != 1);
    return intMnemonic("imul", type);
}

inline string negInstrForType(VRegType type) {
    return intMnemonic("neg", type);
}

// Bitwise operators are integer-only (rejected on float operands in SA); no float case needed.
inline string andInstrForType(VRegType type) { return intMnemonic("and", type); }
inline string orInstrForType(VRegType type)  { return intMnemonic("or",  type); }
inline string xorInstrForType(VRegType type) { return intMnemonic("xor", type); }
inline string notInstrForType(VRegType type) { return intMnemonic("not", type); }

inline string cmpInstrForType(VRegType type) {
    if (type == VRegType::Float32) return "ucomiss";
    if (type == VRegType::Float64) return "ucomisd";
    return intMnemonic("cmp", type);
}

inline const char* setCCForOp(const string& op, VRegType t) {
    // ucomis* reports its result in CF/ZF, so floats share the unsigned (below/above) codes.
    if (isSignedInt(t)) {
        if (op == "<")  return "setl";
        if (op == "<=") return "setle";
        if (op == ">")  return "setg";
        if (op == ">=") return "setge";
    } else {
        if (op == "<")  return "setb";
        if (op == "<=") return "setbe";
        if (op == ">")  return "seta";
        if (op == ">=") return "setae";
    }
    if (op == "==") return "sete";
    if (op == "!=") return "setne";
    BOOST_ASSERT(false); return "";
}

// Derive the sized register name from the 64-bit base name and VRegType.
// Classic registers (%rax/%rbx/...): drop 'r' prefix for 32-bit, use bare name for 16/8-bit.
// Extended registers (%r8-%r15): append 'd'/'w'/'b' suffix.
// XMM registers: name is identical regardless of float size (instruction determines the op).
inline string sizedRegName(const string& base, VRegType type)
{
    // XMM: name unchanged
    if (base.size() >= 4 && base.substr(0, 4) == "%xmm")
        return base;

    // Extended integer registers: %r8, %r9, %r10, ...  (base[2] is a digit)
    if (base.size() >= 3 && base[1] == 'r' && isdigit((unsigned char)base[2])) {
        string stem = base.substr(1);  // "r8", "r10", ...
        switch (type) {
            case VRegType::Ptr64:
            case VRegType::Int64:
            case VRegType::Uint64: return base;
            case VRegType::Int32:
            case VRegType::Uint32: return "%" + stem + "d";
            case VRegType::Int16:
            case VRegType::Uint16: return "%" + stem + "w";
            case VRegType::Int8:
            case VRegType::Uint8:  return "%" + stem + "b";
            default:               return base;
        }
    }

    // Classic registers: lookup table  [64, 32, 16, 8]
    static const map<string, array<string, 4>> classic = {
        {"%rax", {"%rax", "%eax", "%ax",  "%al"}},
        {"%rbx", {"%rbx", "%ebx", "%bx",  "%bl"}},
        {"%rcx", {"%rcx", "%ecx", "%cx",  "%cl"}},
        {"%rdx", {"%rdx", "%edx", "%dx",  "%dl"}},
        {"%rsi", {"%rsi", "%esi", "%si",  "%sil"}},
        {"%rdi", {"%rdi", "%edi", "%di",  "%dil"}},
        {"%rbp", {"%rbp", "%ebp", "%bp",  "%bpl"}},
        {"%rsp", {"%rsp", "%esp", "%sp",  "%spl"}},
    };
    auto it = classic.find(base);
    if (it != classic.end()) {
        switch (type) {
            case VRegType::Ptr64:
            case VRegType::Int64:
            case VRegType::Uint64: return it->second[0];
            case VRegType::Int32:
            case VRegType::Uint32: return it->second[1];
            case VRegType::Int16:
            case VRegType::Uint16: return it->second[2];
            case VRegType::Int8:
            case VRegType::Uint8:  return it->second[3];
            default:               return base;
        }
    }

    return base;  // fallback
} // LCOV_EXCL_BR_LINE

// Return the AT&T source operand string for a PhysLoc:
//   stack  → "-8(%rbp)"  (memory reference)
//   reg    → "%rsi"      (sized register name)
inline string srcOperand(const PhysLoc& loc)
{
    if (loc.isStack())
        return std::to_string(loc.stackOffset) + "(%rbp)";
    return sizedRegName(loc.base, loc.type);
}
