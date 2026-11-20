/// x86-64 call emission: C calls, Palan calls, raw Linux syscalls, and Palan
/// returns (ABI argument shuffling and return-value placement).
///
/// @file PlnX86Call.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <algorithm>
#include "PlnX86CodeGen.h"
#include "PlnX86Internal.h"

using namespace std;

// One leg of a register-argument shuffle: move `srcOperand` (a register or memory
// operand) into the physical register `dstBase`. `srcBase` is the source's register
// base name when the source is itself a register (empty for a memory operand), used
// to detect when one move's destination is needed as another move's source.
struct RegMove {
    string      movInstr;
    VRegType    type;
    string      srcOperand;
    string      srcBase;
    string      dstBase;
    string      dstSized;
};

// SysV leaves the bits above a narrow (<32-bit) argument unspecified, but gcc
// callers extend it to 32 bits and clang-compiled callees rely on that.
static bool isNarrowInt(VRegType t) { return intWidth(t) < 4; }

static RegMove makeIntArgMove(const PhysLoc& src_loc, const string& dstBase)
{
    VRegType t = src_loc.type;
    bool narrow = isNarrowInt(t);
    return RegMove{ // LCOV_EXCL_BR_LINE
        narrow ? extendMnemonic(isSignedInt(t), intWidth(t), 4) : string(movInstrForType(t)), t,
        srcOperand(src_loc), src_loc.isStack() ? "" : src_loc.base,
        dstBase, sizedRegName(dstBase, narrow ? VRegType::Int32 : t)
    };
}

static void emitIntStackArg(ostream& out, const PhysLoc& src_loc, int offset)
{
    VRegType t = src_loc.type;
    string src = srcOperand(src_loc);
    if (isNarrowInt(t)) {
        out << "\t" << extendMnemonic(isSignedInt(t), intWidth(t), 4) << " " << src << ", %r10d\n";
        out << "\tmovl %r10d, " << offset << "(%rsp)\n";
        return;
    }
    const char* mov = movInstrForType(t);
    if (src_loc.isStack()) {
        string scratch = sizedRegName("%r10", t);
        out << "\t" << mov << " " << src << ", " << scratch << "\n";
        src = scratch;
    }
    out << "\t" << mov << " " << src << ", " << offset << "(%rsp)\n";
}

// Sequence a set of moves into distinct physical registers so that no move clobbers
// a register another pending move still needs to read from. A naive argument-order
// pass breaks whenever a source register coincides with an earlier argument's
// destination register -- e.g. `g(b, a)` where parameters a/b are homed at %rdi/%rsi
// respectively: moving b into %rdi (position 0) would destroy a's value before it is
// read for position 1. Moves whose destination nothing else needs are emitted
// immediately; any remaining moves form a cycle, broken by stashing one destination's
// live value in `scratch` before it is overwritten.
static void emitSafeRegMoves(ostream& out, vector<RegMove> moves, const string& scratch)
{
    vector<bool> done(moves.size(), false);
    size_t remaining = moves.size();
    while (remaining > 0) {
        bool progress = false;
        for (size_t i = 0; i < moves.size(); i++) {
            if (done[i]) continue;
            bool needed = false;
            for (size_t j = 0; j < moves.size(); j++) {
                if (j == i || done[j]) continue;
                if (!moves[j].srcBase.empty() && moves[j].srcBase == moves[i].dstBase) { needed = true; break; }
            }
            if (!needed) {
                if (moves[i].srcOperand != moves[i].dstSized)
                    out << "\t" << moves[i].movInstr << " " << moves[i].srcOperand << ", " << moves[i].dstSized << "\n";
                done[i] = true;
                remaining--;
                progress = true;
            }
        }
        if (!progress) {
            size_t i = 0;
            while (done[i]) i++;
            out << "\tmovq " << moves[i].dstBase << ", " << scratch << "\n";
            for (size_t j = 0; j < moves.size(); j++) {
                if (done[j] || moves[j].srcBase != moves[i].dstBase) continue;
                moves[j].srcBase    = scratch;
                moves[j].srcOperand = sizedRegName(scratch, moves[j].type);
            }
        }
    }
}

// SysV argument marshalling shared by C and Palan calls: each class takes its
// own register sequence, and overflow of either class goes to the stack in
// argument order. Returns the stack space reserved (to release after the call)
// and sets nFloatArgs to the float-class count (for C variadic %al).
// A callee pointer joins the register shuffle into %rax, which is no argument
// register and no stack argument's scratch, so no argument move overwrites it
// before it is read.
int PlnX86CodeGen::emitCallArgs(const vector<VReg>& args, const RegMap& rm, int& nFloatArgs,
                                const PhysLoc* callee, const vector<StructArgSpan>& structArgs)
{
    vector<VRegType> types;
    for (auto vr : args) types.push_back(rm.at(vr).type);
    vector<int> slots = placeCallArgs(types, structArgs, (int)x86PhysRegs.intArgs.size(),
                                      (int)x86PhysRegs.floatArgs.size());
    int stack_space;
    vector<int> offsets = placeStackArgs(types, structArgs, slots, stack_space);
    if (stack_space > 0)
        out << "\tsubq $" << stack_space << ", %rsp\n";
    // Int register-destined args are queued (see emitSafeRegMoves below) rather
    // than emitted here in argument order, and stack-destined args are still
    // emitted immediately: since no register move has been emitted yet at that
    // point, a stack arg whose source happens to be a register is read safely
    // regardless of which position that register is also a destination for.
    vector<RegMove> intMoves;
    int n_flt_regs_used = 0;
    for (size_t k = 0; k < args.size(); k++) {
        const PhysLoc& src_loc = rm.at(args[k]);
        bool is_flt = isFloat(src_loc.type);
        if (is_flt && slots[k] >= 0) {
            string xmm = x86PhysRegs.floatArgs[slots[k]];
            n_flt_regs_used++;
            string s = srcOperand(src_loc);
            if (s != xmm)
                out << "\t" << movInstrForType(src_loc.type) << " " << s << ", " << xmm << "\n";
        } else if (!is_flt && slots[k] >= 0) {
            intMoves.push_back(makeIntArgMove(src_loc, x86PhysRegs.intArgs[slots[k]]));
        } else {
            int offset = offsets[k];
            if (isX87(src_loc.type)) {
                out << "\tmovdqu " << srcOperand(src_loc) << ", %xmm8\n";
                out << "\tmovdqu %xmm8, " << offset << "(%rsp)\n";
            } else if (is_flt) {
                const char* mov = movInstrForType(src_loc.type);
                if (src_loc.isStack()) {
                    out << "\t" << mov << " " << srcOperand(src_loc) << ", %xmm8\n";
                    out << "\t" << mov << " %xmm8, " << offset << "(%rsp)\n";
                } else {
                    out << "\t" << mov << " " << srcOperand(src_loc) << ", " << offset << "(%rsp)\n";
                }
            } else {
                emitIntStackArg(out, src_loc, offset);
            }
        }
    }
    if (callee) intMoves.push_back(makeIntArgMove(*callee, "%rax"));
    emitSafeRegMoves(out, intMoves, "%r11");
    nFloatArgs = n_flt_regs_used;
    return stack_space;
}

static const array<const char*, 2> kIntRetRegs = {"%rax", "%rdx"};
static const array<const char*, 2> kSseRetRegs = {"%xmm0", "%xmm1"};

void PlnX86CodeGen::emitInstrCallC(const CallC& i, const RegMap& rm)
{
    int flt_idx;
    int stack_space = emitCallArgs(i.args, rm, flt_idx, i.callee ? &rm.at(*i.callee) : nullptr, i.structArgs);
    emitCallC(i.name, flt_idx, i.callee.has_value());
    if (stack_space > 0)
        out << "\taddq $" << stack_space << ", %rsp\n";
    // Move return value(s) to destination(s). An ordinary scalar/pointer
    // return is one INTEGER (%rax) or SSE (%xmm0) dst; a struct-by-value
    // return classified into eightbytes can add a
    // second dst of either class, consuming %rdx or %xmm1 next in that
    // class's own sequence -- SysV's <=16-byte register-return limit means
    // there is never a third of either. Each dst here is a freshly allocated
    // temp used only by the stores (and shifts) that write it into the
    // destination struct right after the call (see
    // PlnVCodeGen::storeEightbyte), so PlnRegAlloc always places it in a
    // callee-saved register or on the stack, never in %rax/%rdx/%xmm0/%xmm1
    // themselves -- copies below need no clobber-avoiding ordering.
    int intRetIdx = 0, sseRetIdx = 0;
    for (size_t k = 0; k < i.dsts.size(); k++) {
        if (isX87(i.retTypes[k])) {
            emitX87RetStore(i.dsts[k], rm);
            continue;
        }
        bool ret_is_float = isFloat(i.retTypes[k]);
        string ret_reg = ret_is_float ? kSseRetRegs[sseRetIdx++] : sizedRegName(kIntRetRegs[intRetIdx++], i.retTypes[k]);
        if (!rm.count(i.dsts[k])) continue;  // dead: result never used
        const PhysLoc& dst_loc = rm.at(i.dsts[k]);
        string dst_reg = ret_is_float ? (dst_loc.isStack() ? srcOperand(dst_loc) : dst_loc.base)
                                      : srcOperand(dst_loc);
        if (dst_reg != ret_reg)
            out << "\t" << movInstrForType(i.retTypes[k]) << " " << ret_reg << ", " << dst_reg << "\n";
    }
}

// Palan's own return convention: a single value uses the SysV return register
// of its class (%rax/%xmm0/%st); multiple values take each class's argument
// register sequence in order (intArgs / floatArgs), a flo80 whole in an XMM
// register, so a call whose values are dropped leaves nothing on the x87 stack.
// A struct's eightbytes (sysv) take SysV's return registers of their classes,
// as C's do.
static vector<string> plnRetRegs(const vector<VRegType>& types, bool sysv)
{
    if (types.size() == 1)
        return { isX87(types[0]) ? "%st" : isFloat(types[0]) ? "%xmm0" : "%rax" };
    vector<string> regs;
    int int_idx = 0, flt_idx = 0;
    for (VRegType t : types) {
        if (sysv)
            regs.push_back(isFloat(t) ? kSseRetRegs[flt_idx++] : kIntRetRegs[int_idx++]);
        else
            regs.push_back(movedViaXmm(t) ? PlnX86CodeGen::x86PhysRegs.floatArgs[flt_idx++] : PlnX86CodeGen::x86PhysRegs.intArgs[int_idx++]);
    }
    return regs;
} // LCOV_EXCL_BR_LINE

// A flo80 returned in %st is popped off the x87 stack even when nothing uses it.
void PlnX86CodeGen::emitX87RetStore(VReg dst, const RegMap& rm)
{
    if (rm.count(dst))
        out << "\tfstpt " << srcOperand(rm.at(dst)) << "\n";
    else
        out << "\tfstp %st(0)\n";
}

void PlnX86CodeGen::emitInstrCallPln(const CallPln& c, const RegMap& rm)
{
    int n_flt;
    int stack_space = emitCallArgs(c.args, rm, n_flt);
    out << "\tcall " << c.name << "\n";
    if (stack_space > 0)
        out << "\taddq $" << stack_space << ", %rsp\n";
    // Stack dsts first (they only read return registers), then register dsts
    // as a hazard-safe shuffle: a dst may be bound to the next call's argument
    // register that still holds another, not-yet-copied result.
    vector<string> regs = plnRetRegs(c.retTypes, c.sysvRet);
    vector<RegMove> retMoves;
    for (int j = 0; j < (int)c.dsts.size(); j++) {
        if (regs[j] == "%st") {
            emitX87RetStore(c.dsts[j], rm);
            continue;
        }
        if (!rm.count(c.dsts[j])) continue;
        const PhysLoc& dst = rm.at(c.dsts[j]);
        VRegType t = c.retTypes[j];
        string src_reg = sizedRegName(regs[j], t);
        if (dst.isStack())
            out << "\t" << movInstrForType(t) << " " << src_reg << ", " << srcOperand(dst) << "\n";
        else
            retMoves.push_back(RegMove{movInstrForType(t), t, src_reg, regs[j], dst.base, sizedRegName(dst.base, t)}); // LCOV_EXCL_BR_LINE
    }
    emitSafeRegMoves(out, retMoves, "%r11");
}

// A raw Linux syscall: number in %rax, arguments in the syscall ABI's own
// register table (%r10 in 4th position, not %rcx), and the kernel's raw %rax
// result (a negative value is -errno, deliberately not translated). palan-sa
// rejects more than 6 parameters, float parameters/returns and multi-value
// returns, so there is no stack-overflow, XMM or multi-destination path to
// mirror from emitInstrCallC.
void PlnX86CodeGen::emitInstrCallSys(const CallSys& c, const RegMap& rm)
{
    BOOST_ASSERT(c.args.size() <= x86PhysRegs.syscallArgs.size());
    BOOST_ASSERT(c.dsts.size() <= 1);

    // Slot 3 is %r10, left unbound by PlnRegAlloc as emitter scratch
    // (PhysRegs::scratch) -- which is also why queueing its move with the rest
    // is hazard-free: no allocated VReg lives in %r10 or %r11, so no pending
    // move can be sourced from either.
    vector<RegMove> intMoves;
    for (int j = 0; j < (int)c.args.size(); j++)
        intMoves.push_back(makeIntArgMove(rm.at(c.args[j]), x86PhysRegs.syscallArgs[j]));
    emitSafeRegMoves(out, intMoves, "%r11");

    // Loaded after the shuffle: %rax is allocatable (PlnRegAlloc binds a single
    // return value there), so it must not be written while an argument may
    // still be read from it. movl keeps the immediate in the 32-bit form
    // (palan-sa caps the number at UINT32_MAX) and zeroes the rest of %rax,
    // which the kernel's entry path range-checks in full.
    emitMovImm("%eax", false, VRegType::Int32, c.num);
    out << "\tsyscall\n";

    if (c.dsts.size() == 1 && rm.count(c.dsts[0])) {
        const PhysLoc& dst = rm.at(c.dsts[0]);
        string rax = sizedRegName("%rax", c.retTypes[0]);
        string d   = srcOperand(dst);
        if (rax != d)
            out << "\t" << movInstrForType(c.retTypes[0]) << " " << rax << ", " << d << "\n";
    }
}

void PlnX86CodeGen::emitInstrRetPln(const RetPln& r, const RegMap& rm, const vector<string>& usedCalleeSaved)
{
    vector<string> regs = plnRetRegs(r.types, r.sysvRet);
    for (int j = 0; j < (int)r.rets.size(); j++) {
        string s = srcOperand(rm.at(r.rets[j]));
        if (regs[j] == "%st") {
            out << "\tfldt " << s << "\n";
            continue;
        }
        string dst_reg = sizedRegName(regs[j], r.types[j]);
        if (s != dst_reg)
            out << "\t" << movInstrForType(r.types[j]) << " " << s << ", " << dst_reg << "\n";
    }
    // Restore callee-saved registers in reverse order before returning
    for (int i = (int)usedCalleeSaved.size() - 1; i >= 0; i--)
        out << "\tmovq " << -(i + 1) * 8 << "(%rbp), " << usedCalleeSaved[i] << "\n";
    out << "\tleave\n";
    out << "\tret\n";
}

// indirect: the callee pointer is in %rax (see emitCallArgs), and moves to
// %r11 to free %al.
void PlnX86CodeGen::emitCallC(const string& name, int nFloatArgs, bool indirect)
{
    if (indirect)
        out << "\tmovq %rax, %r11\n";
    // For variadic C functions, al = number of XMM registers used for float args.
    if (nFloatArgs == 0)
        out << "\txorl %eax, %eax\n";
    else
        out << "\tmovl $" << nFloatArgs << ", %eax\n";
    if (indirect)
        out << "\tcall *%r11\n";
    else
        out << "\tcall " << name << "\n";
}
