/// x86-64 arithmetic, comparison, and type-conversion emission.
///
/// @file PlnX86Arith.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include "PlnX86CodeGen.h"
#include "PlnX86Internal.h"
#include <iostream>
#include <cstdlib>

using namespace std;

void PlnX86CodeGen::emitBinArith(const string& op, VReg dst, VReg lhs, VReg rhs, VRegType type, const RegMap& rm)
{
    if (!rm.count(dst)) return;  // dead: result never used
    const PhysLoc& dst_loc = rm.at(dst);
    const PhysLoc& lhs_loc = rm.at(lhs);
    if (isX87(type)) {
        out << "\tfldt " << srcOperand(lhs_loc) << "\n";
        out << "\tfldt " << srcOperand(rm.at(rhs)) << "\n";
        out << "\t" << op << " %st, %st(1)\n";
        out << "\tfstpt " << srcOperand(dst_loc) << "\n";
        return;
    }
    const char* mov = movInstrForType(type);
    if (!dst_loc.isStack()) {
        string dst_reg = sizedRegName(dst_loc.base, type);
        out << "\t" << mov << " " << srcOperand(lhs_loc) << ", " << dst_reg << "\n";
        out << "\t" << op  << " " << srcOperand(rm.at(rhs)) << ", " << dst_reg << "\n";
    } else {
        // Spilled dst: route through scratch to avoid mem-mem.
        string scratch = isFloat(type) ? "%xmm8" : sizedRegName("%rax", type);
        out << "\t" << mov << " " << srcOperand(lhs_loc) << ", " << scratch << "\n";
        out << "\t" << op  << " " << srcOperand(rm.at(rhs)) << ", " << scratch << "\n";
        out << "\t" << mov << " " << scratch << ", " << srcOperand(dst_loc) << "\n";
    }
}

void PlnX86CodeGen::emitInstrMul(const Mul& ml, const RegMap& rm)
{
    if (isFloat(ml.type) || isX87(ml.type) || intWidth(ml.type) != 1) {
        emitBinArith(mulInstrForType(ml.type), ml.dst, ml.lhs, ml.rhs, ml.type, rm);
        return;
    }
    if (!rm.count(ml.dst)) return;  // dead: result never used
    // imulb has no 2-operand form. The product's low byte depends only on the
    // operands' low bytes, so a 32-bit multiply of zero-extended operands is exact.
    // rhs is loaded first so a dst sharing rhs's register cannot clobber it.
    const PhysLoc& dst_loc = rm.at(ml.dst);
    out << "\tmovzbl " << srcOperand(rm.at(ml.rhs)) << ", %r10d\n";
    string acc = dst_loc.isStack() ? "%eax" : sizedRegName(dst_loc.base, VRegType::Int32);
    out << "\tmovzbl " << srcOperand(rm.at(ml.lhs)) << ", " << acc << "\n";
    out << "\timull %r10d, " << acc << "\n";
    if (dst_loc.isStack())
        out << "\tmovb %al, " << srcOperand(dst_loc) << "\n";
}

void PlnX86CodeGen::emitIntDivMod(VReg dst, VReg lhs, VReg rhs, VRegType type, const char* result_reg, const RegMap& rm)
{
    if (!rm.count(dst)) return;  // dead: result never used
    const PhysLoc& lhs_loc = rm.at(lhs);
    const PhysLoc& rhs_loc = rm.at(rhs);
    const PhysLoc& dst_loc = rm.at(dst);
    int w = intWidth(type);
    bool sign = isSignedInt(type);
    const string mov = intMnemonic("mov", type);
    // Division clobbers %rax and %rdx, so rhs is moved out of them first.
    string rhs_src = srcOperand(rhs_loc);
    VRegType div_type = type;
    if (w < 4) {
        // 8/16-bit operands are divided at 32 bits: idivb leaves the remainder in %ah,
        // and int8 -128/-1 would raise #DE instead of wrapping.
        div_type = sign ? VRegType::Int32 : VRegType::Uint32;
        out << "\t" << extendMnemonic(sign, w, 4) << " " << rhs_src << ", %r10d\n";
        out << "\t" << extendMnemonic(sign, w, 4) << " " << srcOperand(lhs_loc) << ", %eax\n";
        rhs_src = "%r10d";
    } else {
        if (!rhs_loc.isStack() && (rhs_loc.base == "%rax" || rhs_loc.base == "%rdx")) {
            string r10 = sizedRegName("%r10", type);
            out << "\t" << mov << " " << rhs_src << ", " << r10 << "\n";
            rhs_src = r10;
        }
        out << "\t" << mov << " " << srcOperand(lhs_loc) << ", " << sizedRegName("%rax", type) << "\n";
    }
    if (sign)
        out << (w == 8 ? "\tcqto\n" : "\tcltd\n");
    else
        out << "\txorl %edx, %edx\n";
    out << "\t" << intMnemonic(sign ? "idiv" : "div", div_type) << " " << rhs_src << "\n";
    if (dst_loc.isStack() || dst_loc.base != result_reg)
        out << "\t" << mov << " " << sizedRegName(result_reg, type) << ", " << srcOperand(dst_loc) << "\n";
}

void PlnX86CodeGen::emitInstrDiv(const Div& dv, const RegMap& rm)
{
    if (isX87(dv.type)) {
        emitBinArith("fdivrp", dv.dst, dv.lhs, dv.rhs, dv.type, rm);
        return;
    }
    if (!isFloat(dv.type)) {
        emitIntDivMod(dv.dst, dv.lhs, dv.rhs, dv.type, "%rax", rm);
        return;
    }
    emitBinArith(dv.type == VRegType::Float32 ? "divss" : "divsd", dv.dst, dv.lhs, dv.rhs, dv.type, rm);
}

void PlnX86CodeGen::emitInstrMod(const Mod& md, const RegMap& rm)
{
    emitIntDivMod(md.dst, md.lhs, md.rhs, md.type, "%rdx", rm);
}

void PlnX86CodeGen::emitUnArith(const string& op, VReg dst, VReg src, VRegType type, const RegMap& rm)
{
    if (!rm.count(dst)) return;  // dead: result never used
    const PhysLoc& src_loc = rm.at(src);
    const PhysLoc& dst_loc = rm.at(dst);
    string mov = movInstrForType(type);
    if (!dst_loc.isStack()) {
        string dst_reg = sizedRegName(dst_loc.base, type);
        out << "\t" << mov << " " << srcOperand(src_loc) << ", " << dst_reg << "\n";
        out << "\t" << op  << " " << dst_reg << "\n";
    } else {
        // Spilled dst: route through scratch to avoid mem-mem.
        string scratch = sizedRegName("%rax", type);
        out << "\t" << mov << " " << srcOperand(src_loc) << ", " << scratch << "\n";
        out << "\t" << op  << " " << scratch << "\n";
        out << "\t" << mov << " " << scratch << ", " << srcOperand(dst_loc) << "\n";
    }
}

void PlnX86CodeGen::emitInstrNeg(const Neg& n, const RegMap& rm)
{
    if (!rm.count(n.dst)) return;  // dead: result never used
    if (isX87(n.type)) {
        out << "\tfldt " << srcOperand(rm.at(n.src)) << "\n";
        out << "\tfchs\n";
        out << "\tfstpt " << srcOperand(rm.at(n.dst)) << "\n";
        return;
    }
    if (isFloat(n.type)) {
        // float neg: flip sign bit via xorps/xorpd with mask in .rodata
        const PhysLoc& src_loc = rm.at(n.src);
        const PhysLoc& dst_loc = rm.at(n.dst);
        const char* mov  = movInstrForType(n.type);
        const char* xorI = (n.type == VRegType::Float32) ? "xorps" : "xorpd";
        const char* mask = (n.type == VRegType::Float32) ? ".neg_mask_f32" : ".neg_mask_f64";
        string reg = dst_loc.isStack() ? "%xmm8" : dst_loc.base;
        out << "\t" << mov  << " " << srcOperand(src_loc) << ", " << reg << "\n";
        out << "\t" << xorI << " " << mask << "(%rip), " << reg << "\n";
        if (dst_loc.isStack())
            out << "\t" << mov  << " %xmm8, " << srcOperand(dst_loc) << "\n";
        return;
    }
    emitUnArith(negInstrForType(n.type), n.dst, n.src, n.type, rm);
}

void PlnX86CodeGen::emitInstrBitNot(const BitNot& n, const RegMap& rm)
{
    emitUnArith(notInstrForType(n.type), n.dst, n.src, n.type, rm);
}

static const char* shiftMnemonic(ShiftOp op)
{
    return op == ShiftOp::Shl ? "shl" : op == ShiftOp::Shr ? "shr" : "sar";
}

void PlnX86CodeGen::emitInstrShift(const Shift& s, const RegMap& rm)
{
    if (!rm.count(s.dst)) return;  // dead: result never used
    // The count must be in %cl, and %rcx may hold a live value: it is kept in
    // %r11 while the value is shifted in %r10.
    const char* mov = movInstrForType(s.type);
    string val = sizedRegName("%r10", s.type);
    out << "\t" << mov << " " << srcOperand(rm.at(s.lhs)) << ", " << val << "\n";
    out << "\tmovq %rcx, %r11\n";
    out << "\t" << mov << " " << srcOperand(rm.at(s.rhs)) << ", " << sizedRegName("%rcx", s.type) << "\n";
    // x86 takes the count modulo 32 below 64 bits, so 8/16-bit widths mask it themselves.
    int w = intWidth(s.type);
    if (w < 4)
        out << "\tandb $" << (w * 8 - 1) << ", %cl\n";
    out << "\t" << intMnemonic(shiftMnemonic(s.op), s.type) << " %cl, " << val << "\n";
    out << "\tmovq %r11, %rcx\n";
    out << "\t" << mov << " " << val << ", " << srcOperand(rm.at(s.dst)) << "\n";
}

void PlnX86CodeGen::emitInstrShiftImm(const ShiftImm& s, const RegMap& rm)
{
    emitUnArith(intMnemonic(shiftMnemonic(s.op), s.type) + " $" + to_string(s.count) + ",",
                s.dst, s.src, s.type, rm);
}

// %xmm8/%xmm9 are the emitter's scratch, never assigned to a VReg.
void PlnX86CodeGen::emitInstrPackF32x2(const PackF32x2& p, const RegMap& rm)
{
    if (!rm.count(p.dst)) return;  // dead
    out << "\tmovss " << srcOperand(rm.at(p.lo)) << ", %xmm8\n";
    out << "\tmovss " << srcOperand(rm.at(p.hi)) << ", %xmm9\n";
    out << "\tunpcklps %xmm9, %xmm8\n";
    const PhysLoc& dst = rm.at(p.dst);
    out << "\tmovsd %xmm8, " << (dst.isStack() ? srcOperand(dst) : dst.base) << "\n";
}

void PlnX86CodeGen::emitInstrCmp(const Cmp& cm, const RegMap& rm)
{
    if (!rm.count(cm.dst)) return;  // dead
    const PhysLoc& dst_loc = rm.at(cm.dst);
    const PhysLoc& lhs_loc = rm.at(cm.lhs);
    string dst = dst_loc.isStack() ? srcOperand(dst_loc) : sizedRegName(dst_loc.base, VRegType::Uint8);
    if (isX87(cm.type)) {
        // fucomip sets CF/ZF as ucomis* does, comparing %st (lhs) with %st(1) (rhs).
        out << "\tfldt " << srcOperand(rm.at(cm.rhs)) << "\n";
        out << "\tfldt " << srcOperand(lhs_loc) << "\n";
        out << "\tfucomip %st(1), %st\n";
        out << "\tfstp %st(0)\n";
        out << "\t" << setCCForOp(cm.op, cm.type) << " " << dst << "\n";
        return;
    }
    // cmpX %rhs, lhs  →  flags = lhs - rhs
    // x86 disallows two memory operands: load lhs into scratch if spilled
    string lhs_operand;
    if (lhs_loc.isStack()) {
        string scratch = isFloat(cm.type) ? "%xmm8" : sizedRegName("%rax", cm.type);
        out << "\t" << movInstrForType(cm.type) << " " << srcOperand(lhs_loc) << ", " << scratch << "\n";
        lhs_operand = scratch;
    } else {
        lhs_operand = srcOperand(lhs_loc);
    }
    out << "\t" << cmpInstrForType(cm.type) << " " << srcOperand(rm.at(cm.rhs)) << ", " << lhs_operand << "\n";
    out << "\t" << setCCForOp(cm.op, cm.type) << " " << dst << "\n";
}

void PlnX86CodeGen::emitInstrConvert(const Convert& c, const RegMap& rm)
{
    if (!rm.count(c.dst)) return;  // dead
    if (isX87(c.from) || isX87(c.to)) {
        emitX87Convert(c, rm);
        return;
    }
    const PhysLoc& dst_loc = rm.at(c.dst);
    bool dst_is_float = isFloat(c.to);
    if (dst_loc.isStack()) {
        // Spilled dst: convert into scratch register, then store to stack slot.
        // Float results use %xmm8 as scratch; integer results use %rax.
        string scratchBase = dst_is_float ? "%xmm8" : "%rax";
        emitConvert(scratchBase, rm.at(c.src), c.from, c.to);
        string scratch = dst_is_float ? scratchBase : sizedRegName(scratchBase, c.to);
        out << "\t" << movInstrForType(c.to) << " " << scratch << ", " << srcOperand(dst_loc) << "\n";
    } else {
        emitConvert(dst_loc.base, rm.at(c.src), c.from, c.to);
    }
}

void PlnX86CodeGen::emitConvert(const string& dstBase, const PhysLoc& src, VRegType from, VRegType to)
{
    // Stack is a memory ref; register is sized by the requested type.
    auto srcAt = [&](VRegType t) -> string {
        if (src.isStack()) return std::to_string(src.stackOffset) + "(%rbp)";
        return sizedRegName(src.base, t);
    };
    // Destination is always a register; XMM register names ignore width.
    auto dstAt = [&](VRegType t) -> string {
        if (isFloat(t)) return dstBase;
        return sizedRegName(dstBase, t);
    };

    // Integer <-> integer, any width and signedness.
    if (!isFloat(from) && !isFloat(to)) {
        int fw = intWidth(from), tw = intWidth(to);
        if (tw <= fw) {
            // Narrowing, or same-width sign reinterpretation (e.g. int32<->uint32):
            // the bit pattern is unchanged, so just reference the low bits.
            out << "\t" << movInstrForType(to) << " " << srcAt(to) << ", " << dstAt(to) << "\n";
        } else if (fw == 4 && !isSignedInt(from)) {
            // No movzlq instruction exists: movl into the 32-bit destination
            // register implicitly zero-extends the upper 32 bits of the 64-bit register.
            out << "\tmovl " << srcAt(from) << ", " << dstAt(VRegType::Int32) << "\n";
        } else {
            // Widening: extend according to the SOURCE's signedness (C semantics).
            out << "\t" << extendMnemonic(isSignedInt(from), fw, tw)
                << " " << srcAt(from) << ", " << dstAt(to) << "\n";
        }
        return;
    }

    // Float to integer conversion (truncation toward zero)
    if (isFloat(from) && !isFloat(to)) {
        if (to == VRegType::Uint64) { // LCOV_EXCL_BR_LINE
            // Unreachable: PlnVCodeGen lowers float-to-uint64 into a branch sequence.
            std::cerr << "PlnX86CodeGen: float-to-uint64 conversion must be lowered in PlnVCodeGen\n"; // LCOV_EXCL_LINE
            std::abort(); // LCOV_EXCL_LINE
        }
        // cvtt*2si has only 32-bit and 64-bit integer destination forms.
        // Uint8/16/32 use the 64-bit form: a uint32 above INT32_MAX would not
        // fit the signed 32-bit result of the 32-bit form. Int8/16 reuse the
        // 32-bit form and take their answer from the destination's low bits.
        bool use64 = (to == VRegType::Int64) || !isSignedInt(to);
        const char* cvt = (from == VRegType::Float64)
            ? (use64 ? "cvttsd2siq" : "cvttsd2sil")
            : (use64 ? "cvttss2siq" : "cvttss2sil");
        out << "\t" << cvt << " " << srcAt(from) << ", "
            << dstAt(use64 ? VRegType::Int64 : VRegType::Int32) << "\n";
        return;
    }
    // Float precision conversion
    if (from == VRegType::Float32 && to == VRegType::Float64) {
        out << "\tcvtss2sd " << srcAt(from) << ", " << dstAt(to) << "\n"; return;
    }
    if (from == VRegType::Float64 && to == VRegType::Float32) {
        out << "\tcvtsd2ss " << srcAt(from) << ", " << dstAt(to) << "\n"; return;
    }
    // Integer to float conversion
    if (from == VRegType::Int32 && to == VRegType::Float64) {
        out << "\tcvtsi2sdl " << srcAt(from) << ", " << dstAt(to) << "\n"; return;
    }
    if (from == VRegType::Int64 && to == VRegType::Float64) {
        out << "\tcvtsi2sdq " << srcAt(from) << ", " << dstAt(to) << "\n"; return;
    }
    if (from == VRegType::Int32 && to == VRegType::Float32) {
        out << "\tcvtsi2ssl " << srcAt(from) << ", " << dstAt(to) << "\n"; return;
    }
    if (from == VRegType::Int64 && to == VRegType::Float32) {
        out << "\tcvtsi2ssq " << srcAt(from) << ", " << dstAt(to) << "\n"; return;
    }
    // Smaller signed integers: sign-extend to 64-bit via %rax, then convert.
    if ((from == VRegType::Int8 || from == VRegType::Int16) &&
        (to == VRegType::Float32 || to == VRegType::Float64)) {
        const char* sx  = (from == VRegType::Int8) ? "movsbq" : "movswq";
        const char* cvt = (to == VRegType::Float32) ? "cvtsi2ssq" : "cvtsi2sdq";
        out << "\t" << sx << " " << srcAt(from) << ", %rax\n";
        out << "\t" << cvt << " %rax, " << dstAt(to) << "\n";
        return;
    }
    // Unsigned integer to float conversion (uint8/16/32 fit in int64, so cvtsi2s*q is correct after zero-extension)
    if (from == VRegType::Uint8 && (to == VRegType::Float32 || to == VRegType::Float64)) {
        const char* cvt = (to == VRegType::Float32) ? "cvtsi2ssq" : "cvtsi2sdq";
        out << "\tmovzbq " << srcAt(from) << ", %rax\n";
        out << "\t" << cvt << " %rax, " << dstAt(to) << "\n";
        return;
    }
    if (from == VRegType::Uint16 && (to == VRegType::Float32 || to == VRegType::Float64)) {
        const char* cvt = (to == VRegType::Float32) ? "cvtsi2ssq" : "cvtsi2sdq";
        out << "\tmovzwq " << srcAt(from) << ", %rax\n";
        out << "\t" << cvt << " %rax, " << dstAt(to) << "\n";
        return;
    }
    if (from == VRegType::Uint32 && (to == VRegType::Float32 || to == VRegType::Float64)) {
        // movl zero-extends uint32 into %rax; result fits in int64, so cvtsi2s*q is correct.
        const char* cvt = (to == VRegType::Float32) ? "cvtsi2ssq" : "cvtsi2sdq";
        out << "\tmovl " << srcAt(from) << ", %eax\n";
        out << "\t" << cvt << " %rax, " << dstAt(to) << "\n";
        return;
    }
    // Abort unconditionally — do not rely on BOOST_ASSERT which is a no-op in release builds.
    // Unreachable: uint64-to-float is lowered in PlnVCodeGen, and every other
    // VRegType pair is handled by the branches above.
    std::cerr << "PlnX86CodeGen: unsupported conversion\n"; // LCOV_EXCL_LINE
    std::abort(); // LCOV_EXCL_LINE
}

// The x87 unit reads and writes only memory to exchange values with the other
// registers, so a register operand passes through the red zone below %rsp.
void PlnX86CodeGen::emitX87Convert(const Convert& c, const RegMap& rm)
{
    const PhysLoc& src = rm.at(c.src);
    const PhysLoc& dst = rm.at(c.dst);
    const string tmp = "-16(%rsp)";
    if (isX87(c.to)) {
        string s = srcOperand(src);
        if (isFloat(c.from)) {
            if (!src.isStack()) {
                out << "\t" << movInstrForType(c.from) << " " << s << ", " << tmp << "\n";
                s = tmp;
            }
            out << "\t" << (c.from == VRegType::Float32 ? "flds " : "fldl ") << s << "\n";
        } else {
            // fild reads a signed integer: narrower integers are first extended
            // to 64 bits, and a uint64 with its top bit set gets 2^64 added
            // (0x5f800000 as a flo32), exactly, as a flo80 holds 64 bits.
            int w = intWidth(c.from);
            if (w < 8) {
                if (w == 4 && !isSignedInt(c.from))
                    out << "\tmovl " << s << ", %eax\n";
                else
                    out << "\t" << extendMnemonic(isSignedInt(c.from), w, 8) << " " << s << ", %rax\n";
                out << "\tmovq %rax, " << tmp << "\n";
                s = tmp;
            } else if (!src.isStack()) {
                out << "\tmovq " << s << ", " << tmp << "\n";
                s = tmp;
            }
            out << "\tfildq " << s << "\n";
            if (c.from == VRegType::Uint64) {
                out << "\tcmpq $0, " << s << "\n";
                out << "\tjns 1f\n";
                out << "\tmovl $0x5f800000, -20(%rsp)\n";
                out << "\tfadds -20(%rsp)\n";
                out << "1:\n";
            }
        }
        out << "\tfstpt " << srcOperand(dst) << "\n";
        return;
    }
    out << "\tfldt " << srcOperand(src) << "\n";
    if (isFloat(c.to)) {
        const char* mov = movInstrForType(c.to);
        out << "\t" << (c.to == VRegType::Float32 ? "fstps " : "fstpl ") << tmp << "\n";
        string reg = dst.isStack() ? "%xmm8" : dst.base;
        out << "\t" << mov << " " << tmp << ", " << reg << "\n";
        if (dst.isStack())
            out << "\t" << mov << " %xmm8, " << srcOperand(dst) << "\n";
        return;
    }
    // Truncation toward zero; a narrower integer takes the low bits, as cvtt*2si does.
    out << "\tfisttpq " << tmp << "\n";
    string reg = dst.isStack() ? "%rax" : dst.base;
    out << "\tmovq " << tmp << ", " << reg << "\n";
    if (dst.isStack())
        out << "\t" << movInstrForType(c.to) << " " << sizedRegName("%rax", c.to) << ", " << srcOperand(dst) << "\n";
}
