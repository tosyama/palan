/// Virtual instruction set (IR) for palan-codegen.
///
/// @file PlnVProg.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once
#include <string>
#include <vector>
#include <variant>
#include <utility>
#include <optional>

using std::string;
using std::vector;

using VReg = int;

// Data size / type of a virtual register value
enum class VRegType {
    Ptr64,    // pointer (64-bit)
    Int8,
    Int16,
    Int32,
    Int64,
    Uint8,
    Uint16,
    Uint32,
    Uint64,
    Float32,
    Float64,
    Float80,  // x87 extended precision in a 16-byte stack slot, never in a register
};

// An SSE float, which travels in an XMM register.
inline bool isFloat(VRegType t) {
    return t == VRegType::Float32 || t == VRegType::Float64;
}

inline bool isX87(VRegType t) { return t == VRegType::Float80; }

inline int intWidth(VRegType t) {
    switch (t) {
        case VRegType::Int8:  case VRegType::Uint8:  return 1;
        case VRegType::Int16: case VRegType::Uint16: return 2;
        case VRegType::Int32: case VRegType::Uint32: return 4;
        default:                                     return 8;  // Int64, Uint64, Ptr64
    }
}

inline bool isSignedInt(VRegType t) {
    switch (t) {
        case VRegType::Int8: case VRegType::Int16: case VRegType::Int32: case VRegType::Int64:
            return true;
        default:
            return false;
    }
}

// -------- Virtual instructions --------

struct LeaLabel  { VReg dst; VRegType type; string label; };       // dst = address of label
struct MovImm    { VReg dst; VRegType type; long long value; };    // dst = immediate integer
struct InitVar   { VReg dst; VRegType type; long long imm; };      // variable init: dst_vreg = imm
struct InitVarF  { VReg dst; VRegType type; string label; };       // variable init: dst_vreg = float at label
struct DeclVar   { VReg dst; VRegType type; };                     // variable decl without initializer (no code)
struct Add      { VReg dst; VReg lhs; VReg rhs; VRegType type; }; // dst = lhs + rhs
struct Sub      { VReg dst; VReg lhs; VReg rhs; VRegType type; }; // dst = lhs - rhs
struct Mul      { VReg dst; VReg lhs; VReg rhs; VRegType type; }; // dst = lhs * rhs
struct Div      { VReg dst; VReg lhs; VReg rhs; VRegType type; }; // dst = lhs / rhs
struct Mod      { VReg dst; VReg lhs; VReg rhs; VRegType type; }; // dst = lhs % rhs
struct Neg      { VReg dst; VReg src; VRegType type; };            // dst = -src
struct BitAnd   { VReg dst; VReg lhs; VReg rhs; VRegType type; }; // dst = lhs & rhs
struct BitOr    { VReg dst; VReg lhs; VReg rhs; VRegType type; }; // dst = lhs | rhs
struct BitXor   { VReg dst; VReg lhs; VReg rhs; VRegType type; }; // dst = lhs ^ rhs
struct BitNot   { VReg dst; VReg src; VRegType type; };            // dst = ~src
enum class ShiftOp { Shl, Shr, Sar };  // Sar only lowers division and signed bit-fields; Palan has no arithmetic shift
struct Shift    { VReg dst; VReg lhs; VReg rhs; ShiftOp op; VRegType type; }; // count taken modulo the width
struct ShiftImm { VReg dst; VReg src; int count; ShiftOp op; VRegType type; }; // 0 < count < width
struct PackF32x2 { VReg dst; VReg lo; VReg hi; };                               // Float64 dst = lo, hi as its two halves
struct Cmp      { VReg dst; string op; VReg lhs; VReg rhs; VRegType type; }; // dst (uint8) = (lhs op rhs) ? 1 : 0
struct Convert  { VReg dst; VReg src; VRegType from; VRegType to; }; // dst = (to)src
// dsts/retTypes hold zero (statement-level, no return used), one (an ordinary
// scalar/pointer return), or more (a struct-by-value return classified into
// eightbytes by palan-sa; each dst receives one eightbyte from %rax/%rdx or
// %xmm0/%xmm1, then a DerefStore writes it into the destination struct) VRegs
// -- the same shape as CallPln, so both share PlnRegAlloc's CallPln visitor
// logic.
// With callee set, the call goes through that function pointer instead of
// name: a call through a pointer always uses the C convention.
// structArgs marks the runs of args that are the eightbytes of one struct
// passed by value (in argument order).
struct StructArgSpan { size_t first; size_t count; bool inMemory; int align = 8; };
struct CallC    { string name; vector<VReg> args; vector<VReg> dsts; vector<VRegType> retTypes;
                  std::optional<VReg> callee = std::nullopt; vector<StructArgSpan> structArgs = {}; };
// sysvRet: the dsts are a returned struct's eightbytes, in the System V
// return registers (%rax/%rdx, %xmm0/%xmm1) instead of Palan's multiple-value ones.
struct CallPln  { string name; vector<VReg> args; vector<VReg> dsts; vector<VRegType> retTypes; bool sysvRet = false; };
// Same dsts/retTypes shape as CallC/CallPln, but always 0 or 1 (palan-sa's
// validateSyscallDecl rejects multi-return syscall declarations) and no
// name -- num is the Linux syscall number (raw syscall ABI, not System V).
struct CallSys  { long long num; vector<VReg> args; vector<VReg> dsts; vector<VRegType> retTypes; };
struct RetPln   { vector<VReg> rets; vector<VRegType> types; bool sysvRet = false; };
struct BlockEnter {};
struct BlockLeave { vector<VReg> expiredVars; };
struct Label      { string name; };                              // name:
struct Jmp        { string label; };                             // jmp label
struct CondJmp    { string label; VReg cond; bool jumpIfZero; }; // testl+je/jne
struct Mov        { VReg dst; VReg src; VRegType type; };        // dst = src (variable update)
struct Copy       { VReg dst; VReg src; VRegType type; };        // dst = src (defines dst)
struct DerefLoadIdx  { VReg dst; VReg base; VReg idx; int scale; VRegType type; VRegType idx_type; };  // dst = base[idx*scale]
struct DerefStoreIdx { VReg base; VReg idx; int scale; VReg src; VRegType type; VRegType idx_type; };  // base[idx*scale] = src
struct CalcAddrIdx   { VReg dst; VReg base; VReg idx; int scale; VRegType idx_type; };                 // dst = base + idx*scale (no dereference)
struct DerefLoad     { VReg dst; VReg ptr; int offset; VRegType type; };                               // dst = *(ptr+offset)
struct DerefStore    { VReg ptr; int offset; VReg src;  VRegType type; };                              // *(ptr+offset) = src
struct CalcAddr      { VReg dst; VReg ptr; int offset; };                                              // dst = ptr + offset (no dereference)
struct LeaLocal      { VReg dst; VReg local; };                                                        // dst = &local (local's own stack slot address)
struct FrameAddr     { VReg dst; int block; };                                                         // dst = address of VFunc::frameBlocks[block]

using VInstr = std::variant<LeaLabel, MovImm, InitVar, InitVarF, DeclVar, Add, Sub, Mul, Div, Mod, Neg,
                             BitAnd, BitOr, BitXor, BitNot, Shift, ShiftImm, PackF32x2, Cmp, Convert,
                             CallC, CallPln, CallSys, RetPln,
                             BlockEnter, BlockLeave,
                             Label, Jmp, CondJmp, Mov, Copy, DerefLoadIdx, DerefStoreIdx, CalcAddrIdx,
                             DerefLoad, DerefStore, CalcAddr, LeaLocal, FrameAddr>;

// SysV argument placement, shared by the register allocator's argument hints
// and the x86 emitter: each arg's index in its class's register sequence, or
// -1 for the stack. A struct goes to the stack whole when its eightbytes don't
// all fit in the remaining registers (or it is MEMORY class); later args still
// take the registers left free.
inline vector<int> placeCallArgs(const vector<VRegType>& types, const vector<StructArgSpan>& spans,
                                 int nIntRegs, int nFltRegs)
{
    vector<int> slots(types.size(), -1);
    int nextInt = 0, nextFlt = 0;
    size_t span = 0;
    for (size_t i = 0; i < types.size(); ) {
        if (span < spans.size() && spans[span].first == i) {
            const StructArgSpan& s = spans[span++];
            int nInt = 0, nFlt = 0;
            for (size_t k = i; k < i + s.count; k++)
                (isFloat(types[k]) ? nFlt : nInt)++;
            if (!s.inMemory && nextInt + nInt <= nIntRegs && nextFlt + nFlt <= nFltRegs)
                for (size_t k = i; k < i + s.count; k++)
                    slots[k] = isFloat(types[k]) ? nextFlt++ : nextInt++;
            i += s.count;
            continue;
        }
        if (isFloat(types[i]))     { if (nextFlt < nFltRegs) slots[i] = nextFlt++; }
        else if (!isX87(types[i])) { if (nextInt < nIntRegs) slots[i] = nextInt++; }
        i++;
    }
    return slots;
}

// Each stack argument's offset from the bottom of the argument area (-1 for
// one in a register), and the area's size. A flo80 takes 16 bytes and, like a
// struct of that alignment, starts at a 16-byte boundary.
inline vector<int> placeStackArgs(const vector<VRegType>& types, const vector<StructArgSpan>& spans,
                                  const vector<int>& slots, int& bytes)
{
    vector<int> offsets(types.size(), -1);
    int at = 0;
    size_t span = 0;
    for (size_t i = 0; i < types.size(); ) {
        if (span < spans.size() && spans[span].first == i) {
            const StructArgSpan& s = spans[span++];
            if (slots[i] < 0) {
                at = (at + s.align - 1) / s.align * s.align;
                for (size_t k = i; k < i + s.count; k++, at += 8) offsets[k] = at;
            }
            i += s.count;
            continue;
        }
        if (slots[i] < 0) {
            int size = isX87(types[i]) ? 16 : 8;
            at = (at + size - 1) / size * size;
            offsets[i] = at;
            at += size;
        }
        i++;
    }
    bytes = (at + 15) & ~15;
    return offsets;
}

// -------- Program structure --------

struct VStringLiteral {
    string label;
    string value;
};

struct VFloatLiteral {
    string   label;
    string   value;   // decimal string e.g. "3.14"
    VRegType type;
};

struct FrameBlock { int size; int align; };
struct StaticBlock { string label; int size; int align; bool isExport = false; };

struct VFunc {
    string         name;
    vector<VInstr> instrs;
    vector<FrameBlock> frameBlocks;   // stack-frame storage of `$` variables, indexed by FrameAddr::block
    bool           isExport = false;  // true for export func
    vector<std::pair<VReg, VRegType>> params;  // (vreg, type) in arg order
};

struct VProg {
    vector<VStringLiteral> data;       // .rodata string literals
    vector<VFloatLiteral>  floatData;  // .rodata float literals
    vector<StaticBlock>    bss;        // .bss global slots and storage
    vector<VFunc>          funcs;      // .text
    bool needsF32Neg = false;          // true if any Neg{Float32} instruction exists
    bool needsF64Neg = false;          // true if any Neg{Float64} instruction exists
};
