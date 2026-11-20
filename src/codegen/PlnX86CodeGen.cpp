/// x86-64 code generation: VProg + RegMap -> AT&T assembly.
///
/// @file PlnX86CodeGen.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include "PlnX86CodeGen.h"
#include "PlnX86Internal.h"
#include "../common/PlnStrLit.h"
#include <cstdint>

using namespace std;

// x86-64 System V ABI physical register lists
const PhysRegs PlnX86CodeGen::x86PhysRegs = {
    { "%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9" },
    { "%xmm0", "%xmm1", "%xmm2", "%xmm3", "%xmm4", "%xmm5", "%xmm6", "%xmm7" },
    { "%rbx", "%r12", "%r13", "%r14", "%r15" },
    { "%rdi", "%rsi", "%rdx", "%r10", "%r8", "%r9" },
    { "%r10", "%r11" },
    { "%xmm10", "%xmm11", "%xmm12", "%xmm13", "%xmm14", "%xmm15" }
};

// Whether some function negates a float of `type`, which takes a sign mask (emitInstrNeg).
static bool negatesFloat(const VProg& prog, VRegType type)
{
    for (auto& f : prog.funcs)
        for (auto& instr : f.instrs)
            if (auto* n = std::get_if<Neg>(&instr); n && n->type == type)
                return true;
    return false;
}

void PlnX86CodeGen::emit(const VProg& prog, const vector<RegAllocResult>& allocs)
{
    bool needsF32Neg = negatesFloat(prog, VRegType::Float32);
    bool needsF64Neg = negatesFloat(prog, VRegType::Float64);
    if (!prog.data.empty() || !prog.floatData.empty() || needsF32Neg || needsF64Neg) {
        emitSection(".rodata");
        // xorps/xorpd read a 16-byte operand, which must be 16-byte aligned.
        if (needsF32Neg)
            out << "\t.p2align 4\n.neg_mask_f32:\n\t.long 0x80000000, 0, 0, 0\n";
        if (needsF64Neg)
            out << "\t.p2align 4\n.neg_mask_f64:\n\t.quad 0x8000000000000000, 0\n";
        for (auto& d : prog.data)
            emitStringLiteral(d.label, d.value);
        for (auto& f : prog.floatData) {
            // A flo80 is read as 16 bytes like any other copy of it.
            if (f.type == VRegType::Float80)
                out << "\t.p2align 4\n";
            emitLabel(f.label);
            if (f.type == VRegType::Float32)
                out << "\t.float " << f.value << "\n";
            else if (f.type == VRegType::Float64)
                out << "\t.double " << f.value << "\n";
            else
                out << "\t.tfloat " << f.value << "\n\t.zero 6\n";
        }
    }

    if (!prog.bss.empty()) {
        emitSection(".bss");
        for (auto& b : prog.bss) {
            out << "\t.align " << b.align << "\n";
            if (b.isExport) emitGlobal(b.label);
            emitLabel(b.label);
            out << "\t.zero " << b.size << "\n";
        }
    }

    emitSection(".text");
    for (size_t idx = 0; idx < prog.funcs.size(); ++idx) {
        const VFunc& func = prog.funcs[idx];
        const RegAllocResult& ra = allocs[idx];
        const RegMap& rm  = ra.regMap;

        if (func.isExport)
            emitGlobal(func.name);
        emitLabel(func.name);

        emitFuncPrologue(func, ra, rm);

        for (size_t ii = 0; ii < func.instrs.size(); ++ii) {
            auto& instr = func.instrs[ii];
            if      (auto* i  = std::get_if<LeaLabel> (&instr)) emitInstrLeaLabel(*i, rm);
            else if (auto* i  = std::get_if<MovImm>   (&instr)) emitInstrMovImm(*i, rm);
            else if (auto* i  = std::get_if<InitVar>  (&instr)) emitInstrInitVar(*i, rm);
            else if (auto* i  = std::get_if<InitVarF> (&instr)) emitInstrInitVarF(*i, rm);
            else if (auto* a  = std::get_if<Add>      (&instr)) emitBinArith(addInstrForType(a->type), a->dst, a->lhs, a->rhs, a->type, rm);
            else if (auto* s  = std::get_if<Sub>      (&instr)) emitBinArith(subInstrForType(s->type), s->dst, s->lhs, s->rhs, s->type, rm);
            else if (auto* m  = std::get_if<Mul>      (&instr)) emitInstrMul(*m, rm);
            else if (auto* i  = std::get_if<Div>      (&instr)) emitInstrDiv(*i, rm);
            else if (auto* i  = std::get_if<Mod>      (&instr)) emitInstrMod(*i, rm);
            else if (auto* i  = std::get_if<Neg>      (&instr)) emitInstrNeg(*i, rm);
            else if (auto* a  = std::get_if<BitAnd>   (&instr)) emitBinArith(andInstrForType(a->type), a->dst, a->lhs, a->rhs, a->type, rm);
            else if (auto* o  = std::get_if<BitOr>    (&instr)) emitBinArith(orInstrForType(o->type),  o->dst, o->lhs, o->rhs, o->type, rm);
            else if (auto* x  = std::get_if<BitXor>   (&instr)) emitBinArith(xorInstrForType(x->type), x->dst, x->lhs, x->rhs, x->type, rm);
            else if (auto* i  = std::get_if<BitNot>   (&instr)) emitInstrBitNot(*i, rm);
            else if (auto* i  = std::get_if<Shift>    (&instr)) emitInstrShift(*i, rm);
            else if (auto* i  = std::get_if<ShiftImm> (&instr)) emitInstrShiftImm(*i, rm);
            else if (auto* i  = std::get_if<PackF32x2>(&instr)) emitInstrPackF32x2(*i, rm);
            else if (auto* i  = std::get_if<Cmp>      (&instr)) emitInstrCmp(*i, rm);
            else if (auto* i  = std::get_if<Convert>  (&instr)) emitInstrConvert(*i, rm);
            else if (auto* i  = std::get_if<CallC>    (&instr)) emitInstrCallC(*i, rm);
            else if (auto* i  = std::get_if<CallPln>  (&instr)) emitInstrCallPln(*i, rm);
            else if (auto* i  = std::get_if<CallSys>  (&instr)) emitInstrCallSys(*i, rm);
            else if (auto* i  = std::get_if<RetPln>   (&instr)) emitInstrRetPln(*i, rm, ra.usedCalleeSaved);
            else if (auto* l  = std::get_if<Label>    (&instr)) out << l->name << ":\n";
            else if (auto* j  = std::get_if<Jmp>      (&instr)) out << "\tjmp " << j->label << "\n";
            else if (auto* i  = std::get_if<CondJmp>  (&instr)) emitInstrCondJmp(*i, rm);
            else if (auto* i  = std::get_if<Mov>      (&instr)) emitInstrMov(*i, rm);
            else if (auto* i  = std::get_if<Copy>     (&instr)) emitInstrMov(Mov{i->dst, i->src, i->type}, rm);
            else if (auto* i  = std::get_if<DerefLoadIdx> (&instr)) emitInstrDerefLoadIdx(*i, rm);
            else if (auto* i  = std::get_if<DerefStoreIdx>(&instr)) emitInstrDerefStoreIdx(*i, rm);
            else if (auto* c  = std::get_if<CalcAddrIdx>  (&instr)) emitInstrCalcAddrIdx(*c, rm);
            else if (auto* i  = std::get_if<DerefLoad>    (&instr)) emitInstrDerefLoad(*i,  rm);
            else if (auto* i  = std::get_if<DerefStore>   (&instr)) emitInstrDerefStore(*i, rm);
            else if (auto* i  = std::get_if<CalcAddr>     (&instr)) emitInstrCalcAddr(*i, rm);
            else if (auto* i  = std::get_if<LeaLocal>     (&instr)) emitInstrLeaLocal(*i, rm);
            else if (auto* i  = std::get_if<FrameAddr>    (&instr)) emitInstrFrameAddr(*i, rm, ra.frameBlockOffsets);
            // DeclVar, BlockEnter and BlockLeave are no-ops
            emitHomeMoves(ra.storesAfter, ii, rm, true);
            emitHomeMoves(ra.reloadsAfter, ii, rm, false);
        }
    }

    emitNoteGnuStack();
}

void PlnX86CodeGen::emitHomeMoves(const map<int, vector<VReg>>& moves, int idx, const RegMap& rm, bool store)
{
    auto it = moves.find(idx);
    if (it == moves.end()) return;
    for (VReg vr : it->second) {
        const PhysLoc& loc = rm.at(vr);
        string home = to_string(loc.homeOffset) + "(%rbp)";
        out << "\t" << movInstrForType(loc.type) << " "
            << (store ? loc.base : home) << ", " << (store ? home : loc.base) << "\n";
    }
}

void PlnX86CodeGen::emitFuncPrologue(const VFunc& func, const RegAllocResult& ra, const RegMap& rm)
{
    // Always save %rbp for leave/ret
    out << "\tpushq %rbp\n";
    out << "\tmovq %rsp, %rbp\n";
    if (ra.frameSize > 0)
        out << "\tsubq $" << ra.frameSize << ", %rsp\n";
    // Save callee-saved registers into reserved frame slots (top of frame)
    for (int i = 0; i < (int)ra.usedCalleeSaved.size(); i++)
        out << "\tmovq " << ra.usedCalleeSaved[i] << ", " << -(i + 1) * 8 << "(%rbp)\n";
    // Copy parameters remapped from argument registers to their allocated locations
    for (auto& pc : ra.paramCopies) {
        string src = sizedRegName(pc.srcArgReg, pc.type);
        string dst = srcOperand(pc.dst);
        if (src != dst)
            out << "\t" << movInstrForType(pc.type) << " " << src << ", " << dst << "\n";
    }
}

void PlnX86CodeGen::emitInstrLeaLabel(const LeaLabel& i, const RegMap& rm)
{
    if (!rm.count(i.dst)) return;  // dead: result never used
    const PhysLoc& loc = rm.at(i.dst);
    if (!loc.isStack()) {
        emitLeaLabel(sizedRegName(loc.base, loc.type), i.label);
    } else {
        // Spilled dst: load address into scratch %rax then store to stack.
        out << "\tleaq " << i.label << "(%rip), %rax\n";
        out << "\tmovq %rax, " << srcOperand(loc) << "\n";
    }
}

void PlnX86CodeGen::emitInstrMovImm(const MovImm& i, const RegMap& rm)
{
    const PhysLoc& loc = rm.at(i.dst);
    string dst = loc.isStack() ? srcOperand(loc) : sizedRegName(loc.base, i.type);
    emitMovImm(dst, loc.isStack(), i.type, i.value);
}

void PlnX86CodeGen::emitInstrInitVar(const InitVar& i, const RegMap& rm)
{
    const PhysLoc& loc = rm.at(i.dst);
    string dst = loc.isStack() ? srcOperand(loc) : sizedRegName(loc.base, i.type);
    emitMovImm(dst, loc.isStack(), i.type, i.imm);
}

void PlnX86CodeGen::emitInstrInitVarF(const InitVarF& i, const RegMap& rm)
{
    const PhysLoc& loc = rm.at(i.dst);
    const char* mov = movInstrForType(i.type);
    if (loc.isStack()) {
        // Load float constant into %xmm0 scratch, then store to stack slot.
        out << "\t" << mov << " " << i.label << "(%rip), %xmm0\n";
        out << "\t" << mov << " %xmm0, " << loc.stackOffset << "(%rbp)\n";
    } else {
        out << "\t" << mov << " " << i.label << "(%rip), " << loc.base << "\n";
    }
}

void PlnX86CodeGen::emitInstrCondJmp(const CondJmp& cj, const RegMap& rm)
{
    // Test exactly the value's width: bits above a sub-32-bit value are not
    // guaranteed clean (a caller passes a uint8 argument with movb), and an
    // int64 may be nonzero only above bit 31.
    const PhysLoc& loc = rm.at(cj.cond);
    const char* sfx = widthSuffix(intWidth(loc.type));
    string cond = srcOperand(loc);
    if (loc.isStack())
        out << "\tcmp" << sfx << " $0, " << cond << "\n";
    else
        out << "\ttest" << sfx << " " << cond << ", " << cond << "\n";
    out << (cj.jumpIfZero ? "\tje " : "\tjne ") << cj.label << "\n";
}

void PlnX86CodeGen::emitInstrMov(const Mov& mv, const RegMap& rm)
{
    // Copy src into dst (variable's canonical stack slot or register).
    if (!rm.count(mv.dst) || !rm.count(mv.src)) return;
    const PhysLoc& dst_loc = rm.at(mv.dst);
    const PhysLoc& src_loc = rm.at(mv.src);
    string s = srcOperand(src_loc);
    string d = srcOperand(dst_loc);
    if (s == d) return;  // same location: no-op
    if (!dst_loc.isStack()) {
        string dst_reg = sizedRegName(dst_loc.base, mv.type);
        out << "\t" << movInstrForType(mv.type) << " " << s << ", " << dst_reg << "\n";
    } else if (!src_loc.isStack()) {
        out << "\t" << movInstrForType(mv.type) << " " << s << ", " << d << "\n";
    } else {
        // Both on stack: route through scratch register.
        string scratch = movedViaXmm(mv.type) ? "%xmm8" : sizedRegName("%rax", mv.type);
        out << "\t" << movInstrForType(mv.type) << " " << s << ", " << scratch << "\n";
        out << "\t" << movInstrForType(mv.type) << " " << scratch << ", " << d << "\n";
    }
}

void PlnX86CodeGen::emitSection(const string& name)
{
    out << "\t.section " << name << "\n";
}

void PlnX86CodeGen::emitGlobal(const string& name)
{
    out << "\t.globl " << name << "\n";
}

void PlnX86CodeGen::emitLabel(const string& name)
{
    out << name << ":\n";
}

void PlnX86CodeGen::emitStringLiteral(const string& label, const string& value)
{
    out << label << ":\n";
    out << "\t.string \"";
    // as reads every hex digit after \x, so other bytes go out as three-digit octal escapes.
    for (char ch : strLitBytes(value)) {
        unsigned char c = ch;
        switch (c) {
            case '\n': out << "\\n"; break;
            case '\t': out << "\\t"; break;
            case '"':  out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            default:
                if (c >= 0x20 && c < 0x7F) out << ch;
                else out << '\\' << char('0' + (c >> 6)) << char('0' + ((c >> 3) & 7)) << char('0' + (c & 7));
        }
    }
    out << "\"\n";
}

// ELF/System V target glue that a C program's crt startup objects would supply.
// Palan owns its own program entry -- the entry object holds the _start
// emitted here and the build manager links `ld <objs> -lc` with no
// crt1.o/crti.o/crtbegin.o -- so it must supply these itself. A non-ELF backend
// must not inherit these methods; see the ticket for the deferred VProg-level
// VGlobal promotion if a second backend or a second compiler-emitted global
// ever appears.
void PlnX86CodeGen::emitEntry(const vector<string>& symbols)
{
    // The kernel enters _start with %rsp 16-byte aligned and argc, argv[0..]
    // on the stack, so a plain call keeps the callee's alignment standard.
    emitSection(".text");
    emitGlobal("_start");
    emitLabel("_start");
    for (auto& symbol : symbols) {
        out << "\tmovq (%rsp), %rdi\n";
        out << "\tleaq 8(%rsp), %rsi\n";
        out << "\tcall " << symbol << "\n";
    }
    out << "\tmovl $0, %edi\n";
    out << "\tcall exit\n";

    // Mirrors crtbegin.o's definition for an executable (gcc crtstuff.c's
    // `void *__dso_handle = 0;`): GLOBAL, HIDDEN, 8-byte aligned, in .data, and
    // with no .size directive -- crtbegin's own symbol has size 0. glibc's
    // atexit forwards to __cxa_atexit(func, arg, __dso_handle) from
    // libc_nonshared.a, whose hidden reference has nothing to bind to
    // otherwise. Emitted whether or not this program calls atexit: the compiler
    // models no "which libc symbols will this link pull in" fact, and
    // at_quick_exit/pthread_atfork reference it too, so a use-site gate would
    // grow into a whitelist of C function names.
    emitSection(".data");
    emitGlobal("__dso_handle");
    out << "\t.hidden __dso_handle\n";
    out << "\t.type __dso_handle, @object\n";
    out << "\t.align 8\n";
    emitLabel("__dso_handle");
    out << "\t.quad 0\n";

    emitNoteGnuStack();
}

// Every object, the entry one or not: ld takes the union of its inputs, so a
// single note-less object marks the whole output's stack executable -- and since
// binutils 2.39 also warns on stderr. Without it, a program linking
// libc_nonshared.a gets PT_GNU_STACK RWE, and one that does not gets no
// PT_GNU_STACK at all (kernel default: READ_IMPLIES_EXEC).
void PlnX86CodeGen::emitNoteGnuStack()
{
    out << "\t.section .note.GNU-stack,\"\",@progbits\n";
}

void PlnX86CodeGen::emitLeaLabel(const string& reg, const string& label)
{
    out << "\tleaq " << label << "(%rip), " << reg << "\n";
}

void PlnX86CodeGen::emitMovImm(const string& dst, bool dst_is_mem, VRegType type, long long value)
{
    // mov to memory only takes a sign-extended imm32; a wider 64-bit value
    // must be materialized in a register first.
    if (intWidth(type) == 8 && (value < INT32_MIN || value > INT32_MAX)) {
        if (!dst_is_mem) {
            out << "\tmovabsq $" << value << ", " << dst << "\n";
            return;
        }
        out << "\tmovabsq $" << value << ", %r11\n";
        out << "\tmovq %r11, " << dst << "\n";
        return;
    }
    out << "\t" << movInstrForType(type) << " $" << value << ", " << dst << "\n";
}

