/// Virtual code generation of calls, their arguments and struct values passed
/// by value, and returns.
///
/// @file PlnVCodeGenCall.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include "PlnVCodeGen.h"
#include <boost/assert.hpp>

using namespace std;

// A width other than 1/2/4/8 is moved as 4-, 2- and 1-byte pieces from the
// low end, so no byte outside the struct is touched.
static vector<pair<int, VRegType>> eightbytePieces(int size)
{
    vector<pair<int, VRegType>> pieces;
    for (int pos = 0; pos < size; ) {
        int w = size - pos >= 4 ? 4 : size - pos >= 2 ? 2 : 1;
        pieces.push_back({pos, w == 4 ? VRegType::Uint32 : w == 2 ? VRegType::Uint16 : VRegType::Uint8});
        pos += w;
    }
    return pieces;
} // LCOV_EXCL_LINE

static bool isWholeEightbyte(const Eightbyte& eb)
{
    return isFloat(eb.type) || isX87(eb.type) || eb.size == intWidth(eb.type);
}

VReg PlnVCodeGen::loadEightbyte(VReg ptr, int offset, const Eightbyte& eb, VFunc& func)
{
    // An eightbyte of two floats is usually just written as two 4-byte stores,
    // which an 8-byte load spanning both would wait for (no store forwarding).
    if (eb.type == VRegType::Float64 && eb.size == 8 && !eb.flo64) {
        VReg lo = allocVReg(), hi = allocVReg(), packed = allocVReg();
        func.instrs.push_back(DerefLoad{lo, ptr, offset, VRegType::Float32});
        func.instrs.push_back(DerefLoad{hi, ptr, offset + 4, VRegType::Float32});
        func.instrs.push_back(PackF32x2{packed, lo, hi});
        return packed;
    }
    VReg acc = allocVReg();
    if (isWholeEightbyte(eb)) {
        func.instrs.push_back(DerefLoad{acc, ptr, offset, eb.type});
        return acc;
    }
    bool first = true;
    for (auto [pos, t] : eightbytePieces(eb.size)) {
        VReg piece = allocVReg(), wide = allocVReg();
        func.instrs.push_back(DerefLoad{piece, ptr, offset + pos, t});
        func.instrs.push_back(Convert{wide, piece, t, eb.type});
        if (first) {
            acc = wide;
            first = false;
            continue;
        }
        VReg shifted = allocVReg(), merged = allocVReg();
        func.instrs.push_back(ShiftImm{shifted, wide, 8 * pos, ShiftOp::Shl, eb.type});
        func.instrs.push_back(BitOr{merged, acc, shifted, eb.type});
        acc = merged;
    }
    return acc;
}

void PlnVCodeGen::storeEightbyte(VReg ptr, int offset, VReg src, const Eightbyte& eb, VFunc& func)
{
    if (isWholeEightbyte(eb)) {
        func.instrs.push_back(DerefStore{ptr, offset, src, eb.type});
        return;
    }
    for (auto [pos, t] : eightbytePieces(eb.size)) {
        VReg val = src;
        if (pos > 0) {
            val = allocVReg();
            func.instrs.push_back(ShiftImm{val, src, 8 * pos, ShiftOp::Shr, eb.type});
        }
        func.instrs.push_back(DerefStore{ptr, offset + pos, val, t});
    }
}

// A struct passed by value becomes its eightbytes, read from the struct the
// argument points at; MEMORY class is read the same way, in 8-byte INTEGER
// units, for the emitter to copy to the stack.
vector<VReg> PlnVCodeGen::lowerCCallArgs(const CCCallExpr& expr, vector<StructArgSpan>& spans, VFunc& func,
                                         vector<VReg> args)
{
    for (size_t i = 0; i < expr.args.size(); i++) {
        VReg v = lowerExpr(*expr.args[i], func);
        if (!expr.structArgs[i]) {
            args.push_back(v);
            continue;
        }
        const CCCallExpr::StructArg& sa = *expr.structArgs[i];
        vector<Eightbyte> ebs = sa.eightbytes;
        bool inMemory = ebs.empty();
        if (inMemory)
            for (int off = 0; off < sa.size; off += 8) {
                int w = std::min(8, sa.size - off);
                ebs.push_back({w == 1 ? VRegType::Int8 : w == 2 ? VRegType::Int16
                               : w <= 4 ? VRegType::Int32 : VRegType::Int64, w});
            }
        spans.push_back({args.size(), ebs.size(), inMemory, sa.align});
        for (size_t k = 0; k < ebs.size(); k++)
            args.push_back(loadEightbyte(v, 8 * (int)k, ebs[k], func));
    }
    return args;
} // LCOV_EXCL_BR_LINE

// The pointer a call returning a struct writes it through, evaluated ahead of
// the arguments; a MEMORY-class one is also the hidden first argument.
VReg PlnVCodeGen::lowerStructRetDest(const std::optional<StructRet>& sr, vector<VReg>& args, VFunc& func)
{
    if (!sr) return VReg{};
    VReg dest = lowerExpr(*sr->dest, func);
    if (sr->eightbytes.empty()) args.push_back(dest);
    return dest;
}

// A call's destination vregs. A struct of up to 16 bytes comes back as its
// eightbytes, which storeStructRet writes through its destination; a call
// returning a struct has no value of its own. Otherwise the value takes one,
// as does a flo80 a statement drops: it must still be popped off the x87 stack.
std::pair<vector<VReg>, vector<VRegType>> PlnVCodeGen::callDsts(const std::optional<StructRet>& sr,
                                                                bool hasRet, VRegType retType, bool valueUsed)
{
    std::pair<vector<VReg>, vector<VRegType>> out;
    if (sr) {
        BOOST_ASSERT(!valueUsed);
        for (const Eightbyte& eb : sr->eightbytes) {
            out.first.push_back(allocVReg());
            out.second.push_back(eb.type);
        }
    } else if (hasRet && (valueUsed || isX87(retType))) {
        out = {{allocVReg()}, {retType}};
    }
    return out;
} // LCOV_EXCL_LINE

void PlnVCodeGen::storeStructRet(VReg dest, const std::optional<StructRet>& sr, const vector<VReg>& dsts, VFunc& func)
{
    if (!sr) return;
    for (size_t i = 0; i < dsts.size(); i++)
        storeEightbyte(dest, 8 * (int)i, dsts[i], sr->eightbytes[i], func);
}

// The call's value when valueUsed, else an invalid vreg.
static VReg callValue(const vector<VReg>& dsts, bool valueUsed)
{
    return valueUsed ? dsts.front() : VReg{-1};
}

VReg PlnVCodeGen::lowerCCCall(const CCCallExpr& expr, bool valueUsed, VFunc& func)
{
    vector<VReg> prefix;
    VReg dest = lowerStructRetDest(expr.structRet, prefix, func);
    vector<StructArgSpan> spans;
    vector<VReg> args = lowerCCallArgs(expr, spans, func, move(prefix));
    auto [dsts, types] = callDsts(expr.structRet, expr.hasRet, expr.retType, valueUsed);
    func.instrs.push_back(CallC{expr.name, args, dsts, types, std::nullopt, spans}); // LCOV_EXCL_BR_LINE
    storeStructRet(dest, expr.structRet, dsts, func);
    return callValue(dsts, valueUsed);
}

VReg PlnVCodeGen::lowerPlnCall(const PlnCallExpr& expr, bool valueUsed, VFunc& func)
{
    vector<VReg> args;
    VReg dest = lowerStructRetDest(expr.structRet, args, func);
    for (VReg a : lowerArgs(expr.args, func)) args.push_back(a);
    auto [dsts, types] = callDsts(expr.structRet, expr.hasRet, expr.retType, valueUsed);
    func.instrs.push_back(CallPln{expr.name, args, dsts, types, expr.structRet.has_value()}); // LCOV_EXCL_BR_LINE
    storeStructRet(dest, expr.structRet, dsts, func);
    return callValue(dsts, valueUsed);
}

VReg PlnVCodeGen::lowerPtrCall(const PtrCallExpr& expr, bool valueUsed, VFunc& func)
{
    vector<VReg> args;
    VReg dest = lowerStructRetDest(expr.structRet, args, func);
    VReg callee = lowerExpr(*expr.callee, func);
    for (VReg a : lowerArgs(expr.args, func)) args.push_back(a);
    auto [dsts, types] = callDsts(expr.structRet, expr.hasRet, expr.retType, valueUsed);
    func.instrs.push_back(CallC{"", args, dsts, types, callee}); // LCOV_EXCL_BR_LINE
    storeStructRet(dest, expr.structRet, dsts, func);
    return callValue(dsts, valueUsed);
}

VReg PlnVCodeGen::lowerSysCall(const SysCallExpr& expr, bool valueUsed, VFunc& func)
{
    vector<VReg> args = lowerArgs(expr.args, func);
    auto [dsts, types] = callDsts(std::nullopt, expr.hasRet, expr.retType, valueUsed);
    func.instrs.push_back(CallSys{expr.sysNum, args, dsts, types}); // LCOV_EXCL_BR_LINE
    return callValue(dsts, valueUsed);
}

vector<VReg> PlnVCodeGen::lowerArgs(const vector<unique_ptr<Expr>>& args, VFunc& func)
{
    vector<const Stmt*> outer = move(pendingResets_);
    pendingResets_.clear();
    vector<VReg> regs;
    for (auto& a : args)
        regs.push_back(lowerExpr(*a, func));
    for (auto s : pendingResets_) {
        // An argument read straight from a local the reset writes is its
        // canonical VReg, which the reset updates in place.
        if (s->kind != StmtKind::Assign) continue;
        const VarBinding& b = findBinding(static_cast<const AssignStmt*>(s)->name);
        if (!b.label.empty()) continue;
        for (VReg& r : regs) {
            if (r != b.reg) continue;
            VReg c = allocVReg();
            func.instrs.push_back(Copy{c, r, VRegType::Ptr64}); // LCOV_EXCL_BR_LINE
            r = c;
        }
    }
    // A reset is a statement of its own, which would flush the releases
    // pending for after the call.
    vector<const Stmt*> releases = move(pendingReleases_);
    pendingReleases_.clear();
    for (auto s : pendingResets_)
        lowerStmt(*s, func);
    pendingReleases_ = move(releases);
    pendingResets_ = move(outer);
    return regs;
} // LCOV_EXCL_LINE -- only exception cleanup is attributed here

void PlnVCodeGen::lowerExprStmt(const ExprStmt& stmt, VFunc& func)
{
    switch (stmt.body->kind) {
        case ExprKind::CCCall:
            lowerCCCall(static_cast<const CCCallExpr&>(*stmt.body), false, func);
            return;
        case ExprKind::PlnCall:
            lowerPlnCall(static_cast<const PlnCallExpr&>(*stmt.body), false, func);
            return;
        case ExprKind::SysCall:
            lowerSysCall(static_cast<const SysCallExpr&>(*stmt.body), false, func);
            return;
        case ExprKind::PtrCall:
            lowerPtrCall(static_cast<const PtrCallExpr&>(*stmt.body), false, func);
            return;
        default:
            BOOST_ASSERT(false);  // only call expressions are valid as statements
    }
}

// A struct built in the frame returns as its eightbytes in the SysV return registers.
void PlnVCodeGen::lowerSlotReturn(VFunc& func)
{
    VReg slot = findVar(currentPlnFunc_->retSlot, func);
    vector<VReg>     rets;
    vector<VRegType> types;
    for (size_t i = 0; i < currentPlnFunc_->retSlotEightbytes.size(); i++) {
        const Eightbyte& eb = currentPlnFunc_->retSlotEightbytes[i];
        rets.push_back(loadEightbyte(slot, 8 * (int)i, eb, func));
        types.push_back(eb.type);
    }
    func.instrs.push_back(RetPln{rets, types, true}); // LCOV_EXCL_BR_LINE
}

void PlnVCodeGen::lowerReturnStmt(const ReturnStmt& stmt, VFunc& func)
{
    if (stmt.values.empty()) {
        lowerBareReturn(func);
        return;
    }
    BOOST_ASSERT(stmt.values.size() == 1 && currentPlnFunc_ && currentPlnFunc_->hasRetType
                 && currentPlnFunc_->retSlot.empty() && currentPlnFunc_->retVars.empty());
    VReg r = lowerExpr(*stmt.values[0], func);
    flushReleases(func);
    func.instrs.push_back(RetPln{{r}, {currentPlnFunc_->retType}}); // LCOV_EXCL_BR_LINE
}

// A return without a value, written or implicit at the end of a function,
// returns the named return values. A function with an unnamed one ends
// without returning only where no path reaches its end.
void PlnVCodeGen::lowerBareReturn(VFunc& func)
{
    const PlnFunc* pf = currentPlnFunc_;
    if (pf && !pf->retSlot.empty()) {
        lowerSlotReturn(func);
        return;
    }
    vector<VReg>     rets;
    vector<VRegType> types;
    if (pf && !pf->retVars.empty()) {
        for (auto& rv : pf->retVars) {
            rets.push_back(findVar(rv.name, func));
            types.push_back(rv.type);
        }
    } else if (pf && pf->hasRetType && !pf->retVarName.empty()) {
        rets.push_back(findVar(pf->retVarName, func));
        types.push_back(pf->retType);
    }
    func.instrs.push_back(RetPln{rets, types}); // LCOV_EXCL_BR_LINE
}

void PlnVCodeGen::lowerTappleDeclStmt(const TappleDeclStmt& stmt, VFunc& func)
{
    // Each returned struct is written through a hidden argument, in order, ahead of the arguments.
    vector<VReg> args = lowerArgs(stmt.retDests, func);
    for (VReg a : lowerArgs(stmt.args, func)) args.push_back(a);

    vector<VReg>     dsts;
    vector<VRegType> types;
    for (int j = 0; j < (int)stmt.vars.size(); j++) {
        VReg r = allocVReg();
        dsts.push_back(r);
        types.push_back(stmt.retTypes[j]);
        declareVar(stmt.vars[j].name, r);
    }
    func.instrs.push_back(CallPln{stmt.funcName, args, move(dsts), move(types)}); // LCOV_EXCL_BR_LINE
}
