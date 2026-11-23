/// Virtual code generation of expressions, array elements and bit-fields.
///
/// @file PlnVCodeGenExpr.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include "PlnVCodeGen.h"
#include <boost/assert.hpp>

using namespace std;

// An integer literal's text is parsed per the type SA gave it: a uint64 value
// above INT64_MAX would make stoll throw.
long long PlnVCodeGen::intLitImm(const string& value, VRegType type)
{
    if (type == VRegType::Uint8 || type == VRegType::Uint16
     || type == VRegType::Uint32 || type == VRegType::Uint64)
        return (long long)stoull(value);
    return stoll(value);
}

// k when `e` is an integer literal 2^k with 0 < k < width, else 0.
static int pow2Literal(const Expr& e, VRegType type)
{
    if (isFloat(type) || (e.kind != ExprKind::IntLit && e.kind != ExprKind::UintLit)) return 0;
    const string& text = e.kind == ExprKind::IntLit ? static_cast<const IntLitExpr&>(e).value
                                                    : static_cast<const UintLitExpr&>(e).value;
    if (text[0] == '-') return 0;
    unsigned long long v = stoull(text);
    if (v < 2 || (v & (v - 1))) return 0;
    int k = __builtin_ctzll(v);
    return k < intWidth(type) * 8 ? k : 0;
}

VReg PlnVCodeGen::lowerExpr(const Expr& expr, VFunc& func)
{
    switch (expr.kind) {
        case ExprKind::StrLit: {
            auto& e = static_cast<const StrLitExpr&>(expr);
            VReg r = allocVReg();
            func.instrs.push_back(LeaLabel{r, VRegType::Ptr64, e.label});
            return r;
        }
        case ExprKind::StaticAlloc: {
            auto& e = static_cast<const StaticAllocExpr&>(expr);
            prog_->bss.push_back({e.label, e.size, e.align, false, e.dataType, e.data}); // LCOV_EXCL_BR_LINE
            VReg dst = allocVReg();
            func.instrs.push_back(LeaLabel{dst, VRegType::Ptr64, e.label});
            return dst;
        }
        case ExprKind::FuncRef: {
            auto& e = static_cast<const FuncRefExpr&>(expr);
            VReg r = allocVReg();
            func.instrs.push_back(LeaLabel{r, VRegType::Ptr64, e.name});
            return r;
        }
        case ExprKind::IntLit: {
            auto& e = static_cast<const IntLitExpr&>(expr);
            VReg r = allocVReg();
            func.instrs.push_back(MovImm{r, e.type, intLitImm(e.value, e.type)});
            return r;
        }
        case ExprKind::UintLit: {
            auto& e = static_cast<const UintLitExpr&>(expr);
            VReg r = allocVReg();
            func.instrs.push_back(MovImm{r, e.type, intLitImm(e.value, e.type)});
            return r;
        }
        case ExprKind::Convert: {
            auto& e = static_cast<const ConvertExpr&>(expr);
            VReg src = lowerExpr(*e.src, func);
            // The x87 unit reads a uint64 exactly itself (PlnX86CodeGen).
            bool fromF = isFloat(e.from) || isX87(e.from);
            if ((e.from == VRegType::Uint64 && isFloat(e.to)) || (fromF && e.to == VRegType::Uint64))
                return lowerU64FloatConvert(src, e.from, e.to, func);
            VReg dst = allocVReg();
            func.instrs.push_back(Convert{dst, src, e.from, e.to});
            return dst;
        }
        case ExprKind::Id: {
            auto& e = static_cast<const IdExpr&>(expr);
            return findVar(e.name, func);
        }
        case ExprKind::FrameAlloc: {
            auto& e = static_cast<const FrameAllocExpr&>(expr);
            func.frameBlocks.push_back({e.size, e.align});
            VReg dst = allocVReg();
            func.instrs.push_back(FrameAddr{dst, (int)func.frameBlocks.size() - 1});
            return dst;
        }
        case ExprKind::AddrOf: {
            auto& e = static_cast<const AddrOfExpr&>(expr);
            const VarBinding& b = findBinding(e.name);
            VReg dst = allocVReg();
            if (b.label.empty())
                func.instrs.push_back(LeaLocal{dst, b.reg});
            else
                func.instrs.push_back(LeaLabel{dst, VRegType::Ptr64, b.label});
            return dst;
        }
        case ExprKind::Add: {
            auto& e  = static_cast<const AddExpr&>(expr);
            VReg l   = lowerExpr(*e.left, func);
            VReg r   = lowerExpr(*e.right, func);
            VReg dst = allocVReg();
            func.instrs.push_back(Add{dst, l, r, e.type});
            return dst;
        }
        case ExprKind::Sub: {
            auto& e  = static_cast<const SubExpr&>(expr);
            VReg l   = lowerExpr(*e.left, func);
            VReg r   = lowerExpr(*e.right, func);
            VReg dst = allocVReg();
            func.instrs.push_back(Sub{dst, l, r, e.type});
            return dst;
        }
        case ExprKind::Mul: {
            auto& e  = static_cast<const MulExpr&>(expr);
            if (int k = pow2Literal(*e.right, e.type))
                return shiftImm(lowerExpr(*e.left, func), k, ShiftOp::Shl, e.type, func);
            if (int k = pow2Literal(*e.left, e.type))
                return shiftImm(lowerExpr(*e.right, func), k, ShiftOp::Shl, e.type, func);
            VReg l   = lowerExpr(*e.left, func);
            VReg r   = lowerExpr(*e.right, func);
            VReg dst = allocVReg();
            func.instrs.push_back(Mul{dst, l, r, e.type});
            return dst;
        }
        case ExprKind::Div: {
            auto& e  = static_cast<const DivExpr&>(expr);
            if (int k = pow2Literal(*e.right, e.type))
                return lowerPow2DivMod(lowerExpr(*e.left, func), k, false, e.type, func);
            VReg l   = lowerExpr(*e.left, func);
            VReg r   = lowerExpr(*e.right, func);
            VReg dst = allocVReg();
            func.instrs.push_back(Div{dst, l, r, e.type});
            return dst;
        }
        case ExprKind::Mod: {
            auto& e  = static_cast<const ModExpr&>(expr);
            if (int k = pow2Literal(*e.right, e.type))
                return lowerPow2DivMod(lowerExpr(*e.left, func), k, true, e.type, func);
            VReg l   = lowerExpr(*e.left, func);
            VReg r   = lowerExpr(*e.right, func);
            VReg dst = allocVReg();
            func.instrs.push_back(Mod{dst, l, r, e.type});
            return dst;
        }
        case ExprKind::Neg: {
            auto& e  = static_cast<const NegExpr&>(expr);
            VReg src = lowerExpr(*e.operand, func);
            VReg dst = allocVReg();
            func.instrs.push_back(Neg{dst, src, e.type});
            return dst;
        }
        case ExprKind::BitAnd: {
            auto& e  = static_cast<const BitAndExpr&>(expr);
            VReg l   = lowerExpr(*e.left, func);
            VReg r   = lowerExpr(*e.right, func);
            VReg dst = allocVReg();
            func.instrs.push_back(BitAnd{dst, l, r, e.type});
            return dst;
        }
        case ExprKind::BitOr: {
            auto& e  = static_cast<const BitOrExpr&>(expr);
            VReg l   = lowerExpr(*e.left, func);
            VReg r   = lowerExpr(*e.right, func);
            VReg dst = allocVReg();
            func.instrs.push_back(BitOr{dst, l, r, e.type});
            return dst;
        }
        case ExprKind::BitXor: {
            auto& e  = static_cast<const BitXorExpr&>(expr);
            VReg l   = lowerExpr(*e.left, func);
            VReg r   = lowerExpr(*e.right, func);
            VReg dst = allocVReg();
            func.instrs.push_back(BitXor{dst, l, r, e.type});
            return dst;
        }
        case ExprKind::Shift: {
            auto& e  = static_cast<const ShiftExpr&>(expr);
            VReg l   = lowerExpr(*e.left, func);
            VReg r   = lowerExpr(*e.right, func);
            VReg dst = allocVReg();
            func.instrs.push_back(Shift{dst, l, r, e.isRight ? ShiftOp::Shr : ShiftOp::Shl, e.type});
            return dst;
        }
        case ExprKind::BitNot: {
            auto& e  = static_cast<const BitNotExpr&>(expr);
            VReg src = lowerExpr(*e.operand, func);
            VReg dst = allocVReg();
            func.instrs.push_back(BitNot{dst, src, e.type});
            return dst;
        }
        case ExprKind::Cmp: {
            auto& e  = static_cast<const CmpExpr&>(expr);
            VReg l   = lowerExpr(*e.left, func);
            VReg r   = lowerExpr(*e.right, func);
            VReg dst = allocVReg();
            func.instrs.push_back(Cmp{dst, e.op, l, r, e.operandType}); // LCOV_EXCL_BR_LINE
            return dst;
        }
        case ExprKind::FloLit: {
            auto& e = static_cast<const FloLitExpr&>(expr);
            string label = addFloatLiteral(e.value, e.type);
            VReg r = allocVReg();
            func.instrs.push_back(InitVarF{r, e.type, label});
            return r;
        }
        case ExprKind::CCCall:
            return lowerCCCall(static_cast<const CCCallExpr&>(expr), true, func);
        case ExprKind::PlnCall:
            return lowerPlnCall(static_cast<const PlnCallExpr&>(expr), true, func);
        case ExprKind::PtrCall:
            return lowerPtrCall(static_cast<const PtrCallExpr&>(expr), true, func);
        case ExprKind::SysCall:
            return lowerSysCall(static_cast<const SysCallExpr&>(expr), true, func);
        case ExprKind::LogicalNot:
        case ExprKind::LogicalAnd:
        case ExprKind::LogicalOr: {
            // Short-circuited by lowerBranchCond, as a condition is.
            string endLabel = ".Lbool" + to_string(labelCounter_++) + "_end";
            VReg dst = allocVReg();
            func.instrs.push_back(InitVar{dst, VRegType::Uint8, 0});
            lowerBranchCond(expr, func, endLabel);
            VReg one = allocVReg();
            func.instrs.push_back(MovImm{one, VRegType::Uint8, 1});
            func.instrs.push_back(Mov{dst, one, VRegType::Uint8});
            func.instrs.push_back(Label{endLabel});
            return dst;
        }
        case ExprKind::Temp:
            return lowerTempExpr(static_cast<const TempExpr&>(expr), func);
        case ExprKind::ArrIndex:
            return lowerArrIndexExpr(static_cast<const ArrIndexExpr&>(expr), func);
        case ExprKind::FieldAccess: {
            auto& e   = static_cast<const FieldAccessExpr&>(expr);
            VReg  ptr = e.ptrExpr ? lowerExpr(*e.ptrExpr, func) : findVar(e.varName, func);
            if (e.bitWidth) return loadBitField(ptr, e, func);
            VReg  dst = allocVReg();
            if (e.addrOnly)
                func.instrs.push_back(CalcAddr{dst, ptr, e.offset});
            else
                func.instrs.push_back(DerefLoad{dst, ptr, e.offset, e.type});
            return dst;
        }
        default:
            BOOST_ASSERT(false);
            return -1;
    }
} // LCOV_EXCL_BR_LINE

VReg PlnVCodeGen::lowerArrIndexExpr(const ArrIndexExpr& e, VFunc& func)
{
    VReg base = lowerExpr(*e.array, func);
    VReg idx  = lowerExpr(*e.index, func);
    VReg dst  = allocVReg();

    if (e.scale_expr) {
        // Variable inner dim: runtime stride = scale_expr (e.g. __mat_d1 * sizeof(T))
        VReg stride = lowerExpr(*e.scale_expr, func);
        VReg offset = allocVReg();
        // Widen idx to Int64 before the 64-bit multiply; a narrower spilled vreg
        // would be read as 8 bytes by emitBinArith, corrupting the result.
        VReg idx64 = idx;
        if (e.idx_type != VRegType::Int64 && e.idx_type != VRegType::Uint64) {
            idx64 = allocVReg();
            func.instrs.push_back(Convert{idx64, idx, e.idx_type, VRegType::Int64});
        }
        func.instrs.push_back(Mul{offset, idx64, stride, VRegType::Uint64});
        if (e.addrOnly)
            func.instrs.push_back(Add{dst, base, offset, VRegType::Ptr64});
        else
            func.instrs.push_back(DerefLoadIdx{dst, base, offset, 1, e.type, VRegType::Uint64});
    } else if (e.addrOnly) {
        // Constant-stride row access on embedded 2D array: compute address without dereference.
        func.instrs.push_back(CalcAddrIdx{dst, base, idx, e.scale, e.idx_type});
    } else {
        // Ordinary element access (1D or 2D element via row pointer).
        func.instrs.push_back(DerefLoadIdx{dst, base, idx, e.scale, e.type, e.idx_type});
    }
    return dst;
}

void PlnVCodeGen::lowerArrAssignStmt(const ArrAssignStmt& s, VFunc& func)
{
    VReg base = lowerExpr(*s.array, func);
    VReg idx  = lowerExpr(*s.index, func);
    VReg src  = lowerExpr(*s.value, func);
    func.instrs.push_back(DerefStoreIdx{base, idx, s.scale, src, s.type, s.idx_type});
}

VReg PlnVCodeGen::shiftImm(VReg src, int count, ShiftOp op, VRegType type, VFunc& func)
{
    VReg dst = allocVReg();
    func.instrs.push_back(ShiftImm{dst, src, count, op, type});
    return dst;
}

static VRegType unsignedOfWidth(int bytes)
{
    return bytes == 1 ? VRegType::Uint8 : bytes == 2 ? VRegType::Uint16
         : bytes == 4 ? VRegType::Uint32 : VRegType::Uint64;
}

// Widens (or narrows) between `from` and `to`; nothing when both are 8 bytes.
VReg PlnVCodeGen::convertWidth(VReg src, VRegType from, VRegType to, VFunc& func)
{
    if (intWidth(from) == 8 && intWidth(to) == 8) return src;
    VReg dst = allocVReg();
    func.instrs.push_back(Convert{dst, src, from, to});
    return dst;
}

// The unit is moved to the top of 64 bits so that one more shift both
// extends the sign (or zero) and drops the other fields' bits.
VReg PlnVCodeGen::loadBitField(VReg ptr, const FieldAccessExpr& e, VFunc& func)
{
    VRegType unitType = unsignedOfWidth(intWidth(e.type));
    VReg unit = allocVReg();
    func.instrs.push_back(DerefLoad{unit, ptr, e.offset, unitType});
    VReg v = convertWidth(unit, unitType, VRegType::Uint64, func);
    bool isSigned = isSignedInt(e.type);
    VRegType wide = isSigned ? VRegType::Int64 : VRegType::Uint64;
    int up = 64 - e.bitShift - e.bitWidth, down = 64 - e.bitWidth;
    if (up) v = shiftImm(v, up, ShiftOp::Shl, wide, func);
    if (down) v = shiftImm(v, down, isSigned ? ShiftOp::Sar : ShiftOp::Shr, wide, func);
    return convertWidth(v, wide, e.type, func);
}

void PlnVCodeGen::storeBitField(VReg ptr, const FieldAssignStmt& s, VReg src, VFunc& func)
{
    VRegType unitType = unsignedOfWidth(intWidth(s.type));
    VReg bits = convertWidth(src, s.type, VRegType::Uint64, func);
    int up = 64 - s.bitWidth, down = 64 - s.bitWidth - s.bitShift;
    if (up) bits = shiftImm(bits, up, ShiftOp::Shl, VRegType::Uint64, func);
    if (down) bits = shiftImm(bits, down, ShiftOp::Shr, VRegType::Uint64, func);
    VReg unit = allocVReg();
    func.instrs.push_back(DerefLoad{unit, ptr, s.offset, unitType});
    VReg u = convertWidth(unit, unitType, VRegType::Uint64, func);
    uint64_t mask = (s.bitWidth == 64 ? ~uint64_t(0) : (uint64_t(1) << s.bitWidth) - 1) << s.bitShift;
    VReg keep = allocVReg(), kept = allocVReg(), merged = allocVReg();
    func.instrs.push_back(MovImm{keep, VRegType::Uint64, (long long)~mask});
    func.instrs.push_back(BitAnd{kept, u, keep, VRegType::Uint64});
    func.instrs.push_back(BitOr{merged, kept, bits, VRegType::Uint64});
    VReg out = convertWidth(merged, VRegType::Uint64, unitType, func);
    func.instrs.push_back(DerefStore{ptr, s.offset, out, unitType});
}

// Division by 2^k truncates toward zero, while an arithmetic shift rounds
// toward negative infinity: a negative dividend first gets 2^k - 1 added.
// The remainder then follows from the quotient, keeping the dividend's sign.
VReg PlnVCodeGen::lowerPow2DivMod(VReg x, int k, bool isMod, VRegType type, VFunc& func)
{
    if (!isSignedInt(type)) {
        if (!isMod) return shiftImm(x, k, ShiftOp::Shr, type, func);
        VReg mask = allocVReg();
        func.instrs.push_back(MovImm{mask, type, (long long)((1ULL << k) - 1)});
        VReg dst = allocVReg();
        func.instrs.push_back(BitAnd{dst, x, mask, type});
        return dst;
    }
    int bits = intWidth(type) * 8;
    VReg sign = shiftImm(x, bits - 1, ShiftOp::Sar, type, func);
    VReg bias = shiftImm(sign, bits - k, ShiftOp::Shr, type, func);
    VReg adj = allocVReg();
    func.instrs.push_back(Add{adj, x, bias, type});
    VReg q = shiftImm(adj, k, ShiftOp::Sar, type, func);
    if (!isMod) return q;
    VReg back = shiftImm(q, k, ShiftOp::Shl, type, func);
    VReg dst = allocVReg();
    func.instrs.push_back(Sub{dst, x, back, type});
    return dst;
}

// SSE converts only cover the signed int64 range, so values with the top bit
// set need a branch; emitConvert emits straight-line code only.
VReg PlnVCodeGen::lowerU64FloatConvert(VReg src, VRegType from, VRegType to, VFunc& func)
{
    auto imm = [&](VRegType t, long long v) {
        VReg r = allocVReg();
        func.instrs.push_back(MovImm{r, t, v});
        return r;
    };
    auto op = [&](auto instr) {
        func.instrs.push_back(instr);
        return instr.dst;
    };

    bool toFloat = from == VRegType::Uint64;
    string prefix   = (toFloat ? ".Lu2f" : ".Lf2u") + to_string(labelCounter_++);
    string hiLabel  = prefix + "_hi";
    string endLabel = prefix + "_end";
    VReg dst = allocVReg();

    if (toFloat) {
        func.instrs.push_back(InitVarF{dst, to, addFloatLiteral("0.0", to)});

        VReg neg = op(Cmp{allocVReg(), "<", src, imm(VRegType::Int64, 0), VRegType::Int64}); // LCOV_EXCL_BR_LINE
        func.instrs.push_back(CondJmp{hiLabel, neg, false}); // LCOV_EXCL_BR_LINE
        VReg lo = op(Convert{allocVReg(), src, VRegType::Int64, to});
        func.instrs.push_back(Mov{dst, lo, to});
        func.instrs.push_back(Jmp{endLabel});

        // Halve, keeping the dropped low bit as a sticky bit so the
        // int64->float rounding of the halved value matches that of x.
        func.instrs.push_back(Label{hiLabel});
        VReg half = op(Div{allocVReg(), src, imm(VRegType::Uint64, 2), VRegType::Uint64});
        VReg bit  = op(BitAnd{allocVReg(), src, imm(VRegType::Int64, 1), VRegType::Int64});
        VReg h    = op(BitOr{allocVReg(), half, bit, VRegType::Int64});
        VReg hf   = op(Convert{allocVReg(), h, VRegType::Int64, to});
        VReg hi   = op(Add{allocVReg(), hf, hf, to});
        func.instrs.push_back(Mov{dst, hi, to});
    } else {
        func.instrs.push_back(InitVar{dst, VRegType::Uint64, 0});

        VReg lim = allocVReg();
        func.instrs.push_back(InitVarF{lim, from, addFloatLiteral("9223372036854775808.0", from)});
        VReg ge = op(Cmp{allocVReg(), ">=", src, lim, from}); // LCOV_EXCL_BR_LINE
        func.instrs.push_back(CondJmp{hiLabel, ge, false}); // LCOV_EXCL_BR_LINE
        VReg lo = op(Convert{allocVReg(), src, from, VRegType::Int64});
        func.instrs.push_back(Mov{dst, lo, VRegType::Uint64});
        func.instrs.push_back(Jmp{endLabel});

        func.instrs.push_back(Label{hiLabel});
        VReg biased = op(Sub{allocVReg(), src, lim, from});
        VReg bi     = op(Convert{allocVReg(), biased, from, VRegType::Int64});
        VReg hi     = op(BitXor{allocVReg(), bi, imm(VRegType::Int64, INT64_MIN), VRegType::Int64});
        func.instrs.push_back(Mov{dst, hi, VRegType::Uint64});
    }
    func.instrs.push_back(Label{endLabel});
    return dst;
}

// Return a float literal label, adding the literal to prog_.floatData.
string PlnVCodeGen::addFloatLiteral(const string& value, VRegType type)
{
    string label = ".LF" + to_string(floatLabelCounter_++);
    prog_->floatData.push_back(VFloatLiteral{label, value, type}); // LCOV_EXCL_BR_LINE
    return label;
}
