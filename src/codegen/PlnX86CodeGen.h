/// x86-64 AT&T assembly code generator.
///
/// @file PlnX86CodeGen.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once
#include <ostream>
#include "PlnCodeGen.h"
#include "PlnRegAlloc.h"

class PlnX86CodeGen : public PlnCodeGen {
    std::ostream& out;

    // Low-level emit helpers
    void emitSection(const string& name);
    void emitGlobal(const string& name);
    void emitLabel(const string& name);
    void emitStringLiteral(const string& label, const string& value);
    void emitNoteGnuStack();
    void emitLeaLabel(const string& reg, const string& label);
    void emitMovImm(const string& dst, bool dst_is_mem, VRegType type, long long value);
    void emitConvert(const string& dstBase, const PhysLoc& src, VRegType from, VRegType to);
    void emitX87Convert(const Convert& c, const RegMap& rm);
    void emitX87RetStore(VReg dst, const RegMap& rm);
    void emitCallC(const string& name, int nFloatArgs = 0, bool indirect = false);
    int  emitCallArgs(const vector<VReg>& args, const RegMap& rm, int& nFloatArgs,
                      const PhysLoc* callee = nullptr, const vector<StructArgSpan>& structArgs = {});

    // Per-function prologue
    void emitFuncPrologue(const VFunc& func, const RegAllocResult& ra, const RegMap& rm);
    void emitHomeMoves(const map<int, vector<VReg>>& moves, int idx, const RegMap& rm, bool store);

    // Shared helper: emit a binary reg-reg arithmetic instruction (Add/Sub/Mul/BitAnd/BitOr/BitXor)
    void emitBinArith(const string& op, VReg dst, VReg lhs, VReg rhs, VRegType type, const RegMap& rm);
    void emitIntDivMod(VReg dst, VReg lhs, VReg rhs, VRegType type, const char* result_reg, const RegMap& rm);
    // Shared helper: emit a unary reg arithmetic instruction (Neg's integer path/BitNot)
    void emitUnArith(const string& op, VReg dst, VReg src, VRegType type, const RegMap& rm);

    // Per-instruction emit helpers
    void emitInstrLeaLabel(const LeaLabel& i, const RegMap& rm);
    void emitInstrMovImm(const MovImm& i, const RegMap& rm);
    void emitInstrInitVar(const InitVar& i, const RegMap& rm);
    void emitInstrInitVarF(const InitVarF& i, const RegMap& rm);
    void emitInstrMul(const Mul& i, const RegMap& rm);
    void emitInstrDiv(const Div& i, const RegMap& rm);
    void emitInstrMod(const Mod& i, const RegMap& rm);
    void emitInstrNeg(const Neg& i, const RegMap& rm);
    void emitInstrBitNot(const BitNot& i, const RegMap& rm);
    void emitInstrShift(const Shift& i, const RegMap& rm);
    void emitInstrShiftImm(const ShiftImm& i, const RegMap& rm);
    void emitInstrPackF32x2(const PackF32x2& i, const RegMap& rm);
    void emitInstrCmp(const Cmp& i, const RegMap& rm);
    void emitInstrConvert(const Convert& i, const RegMap& rm);
    void emitInstrCallC(const CallC& i, const RegMap& rm);
    void emitInstrCallPln(const CallPln& i, const RegMap& rm);
    void emitInstrCallSys(const CallSys& i, const RegMap& rm);
    void emitInstrRetPln(const RetPln& i, const RegMap& rm, const vector<string>& usedCalleeSaved);
    void emitInstrCondJmp(const CondJmp& i, const RegMap& rm);
    void emitInstrMov(const Mov& i, const RegMap& rm);
    void emitInstrDerefLoadIdx(const DerefLoadIdx& dl, const RegMap& rm);
    void emitInstrDerefStoreIdx(const DerefStoreIdx& ds, const RegMap& rm);
    void emitInstrCalcAddrIdx(const CalcAddrIdx& ca, const RegMap& rm);
    void emitInstrDerefLoad (const DerefLoad&  dl, const RegMap& rm);
    void emitInstrDerefStore(const DerefStore& ds, const RegMap& rm);
    void emitInstrCalcAddr  (const CalcAddr&   ca, const RegMap& rm);
    void emitInstrLeaLocal  (const LeaLocal&   ll, const RegMap& rm);
    void emitInstrFrameAddr (const FrameAddr&  fa, const RegMap& rm, const vector<int>& blockOffsets);

public:
    static const PhysRegs x86PhysRegs;

    explicit PlnX86CodeGen(std::ostream& out) : out(out) {}

    void emit(const VProg& prog, const vector<RegAllocResult>& allocs) override;
    // The program entry: a _start that calls mainSymbol(argc, argv), then exit(0).
    void emitEntry(const vector<string>& symbols);
};
