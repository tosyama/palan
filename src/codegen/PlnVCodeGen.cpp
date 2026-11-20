/// Virtual code generation: AST (Module) -> VProg. Functions, statements and control flow.
///
/// @file PlnVCodeGen.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include <algorithm>
#include "PlnVCodeGen.h"
#include <boost/assert.hpp>

using namespace std;

// -------- Variable scope management --------

void PlnVCodeGen::enterVarScope()
{
    varScopes.push_back({});
}

void PlnVCodeGen::leaveVarScope()
{
    varScopes.pop_back();
}

const PlnVCodeGen::VarBinding& PlnVCodeGen::findBinding(const string& name) const
{
    for (auto it = varScopes.rbegin(); it != varScopes.rend(); ++it) {
        auto f = it->find(name);
        if (f != it->end()) return f->second;
    }
    BOOST_ASSERT(false);  // variable not declared
    static const VarBinding none;
    return none;  // LCOV_EXCL_LINE
}

VReg PlnVCodeGen::findVar(const string& name, VFunc& func)
{
    const VarBinding& b = findBinding(name);
    if (b.label.empty()) return b.reg;
    VReg addr = allocVReg();
    func.instrs.push_back(LeaLabel{addr, VRegType::Ptr64, b.label});
    VReg dst = allocVReg();
    func.instrs.push_back(DerefLoad{dst, addr, 0, b.type});
    return dst;
}

void PlnVCodeGen::declareVar(const string& name, VReg r)
{
    varScopes.back()[name] = VarBinding{r, "", VRegType::Int64}; // LCOV_EXCL_BR_LINE
}

void PlnVCodeGen::bindStatic(const string& name, const string& label, VRegType type)
{
    varScopes.back()[name] = VarBinding{-1, label, type}; // LCOV_EXCL_BR_LINE
}

// -------- Utilities --------

VReg PlnVCodeGen::allocVReg()
{
    return nextVReg++;
}

VReg PlnVCodeGen::lowerTempExpr(const TempExpr& e, VFunc& func)
{
    // A body statement's temps are released with the temp itself: a statement
    // laying out a variadic argument may store a borrow of one for the call.
    // The releases already pending stay apart, since a condition inside a body
    // statement flushes what is pending.
    vector<const Stmt*> outer = move(pendingReleases_);
    pendingReleases_.clear();
    for (auto& s : e.body)
        lowerStmtKeepingTemps(*s, func);
    vector<const Stmt*> bodyTemps = move(pendingReleases_);
    pendingReleases_ = move(outer);
    for (auto& s : e.release)
        pendingReleases_.push_back(s.get()); // LCOV_EXCL_BR_LINE
    pendingReleases_.insert(pendingReleases_.end(), bodyTemps.begin(), bodyTemps.end());
    for (auto& s : e.reset)
        pendingResets_.push_back(s.get()); // LCOV_EXCL_BR_LINE
    return lowerExpr(*e.value, func);
}

void PlnVCodeGen::flushReleases(VFunc& func)
{
    vector<const Stmt*> releases = move(pendingReleases_);
    pendingReleases_.clear();
    for (auto s : releases)
        lowerStmt(*s, func);
}

void PlnVCodeGen::lowerAssignStmt(const AssignStmt& stmt, VFunc& func)
{
    VReg src = lowerExpr(*stmt.value, func);
    const VarBinding& b = findBinding(stmt.name);
    if (!b.label.empty()) {
        VReg addr = allocVReg();
        func.instrs.push_back(LeaLabel{addr, VRegType::Ptr64, b.label});
        func.instrs.push_back(DerefStore{addr, 0, src, stmt.type});
        return;
    }
    // Update the variable's canonical VReg in place with a Mov. This keeps the
    // VReg identity stable so that backward references — most importantly,
    // while-loop condition VInstrs that captured the VReg before the loop body
    // ran — always see the current value after each iteration.
    if (b.reg != src)
        func.instrs.push_back(Mov{b.reg, src, stmt.type});
}

void PlnVCodeGen::lowerVarDeclStmt(const VarDeclStmt& stmt, VFunc& func)
{
    for (auto& ve : stmt.vars) {
        if (!ve.label.empty()) {
            if (ve.init) {
                VReg v = lowerExpr(*ve.init, func);
                VReg addr = allocVReg();
                func.instrs.push_back(LeaLabel{addr, VRegType::Ptr64, ve.label});
                func.instrs.push_back(DerefStore{addr, 0, v, ve.type});
            }
            bindStatic(ve.varName, ve.label, ve.type);
            continue;
        }
        VReg r;
        if (ve.init) {
            if (ve.init->kind == ExprKind::FloLit) {
                auto& e = static_cast<const FloLitExpr&>(*ve.init);
                string label = addFloatLiteral(e.value, e.type);
                r = allocVReg();
                func.instrs.push_back(InitVarF{r, e.type, label});
                if (!blockVarStack_.empty())
                    blockVarStack_.back().push_back(r);
            } else if (ve.init->kind == ExprKind::IntLit) {
                auto& e = static_cast<const IntLitExpr&>(*ve.init);
                r = allocVReg();
                func.instrs.push_back(InitVar{r, e.type, intLitImm(e.value, e.type)});
                if (!blockVarStack_.empty())
                    blockVarStack_.back().push_back(r);
            } else if (ve.init->kind == ExprKind::UintLit) {
                auto& e = static_cast<const UintLitExpr&>(*ve.init);
                r = allocVReg();
                func.instrs.push_back(InitVar{r, e.type, intLitImm(e.value, e.type)});
                if (!blockVarStack_.empty())
                    blockVarStack_.back().push_back(r);
            } else if (ve.init->kind == ExprKind::Id) {
                // The source's VReg is its canonical storage, updated in place by
                // later assignments; sharing it would alias the two variables.
                r = allocVReg();
                func.instrs.push_back(Copy{r, lowerExpr(*ve.init, func), ve.type});
            } else {
                r = lowerExpr(*ve.init, func);
            }
        } else {
            r = allocVReg();
            func.instrs.push_back(DeclVar{r, ve.type});
            if (!blockVarStack_.empty())
                blockVarStack_.back().push_back(r);
        }
        declareVar(ve.varName, r);
    }
}

void PlnVCodeGen::lowerBlockStmt(const BlockStmt& stmt, VFunc& func)
{
    func.instrs.push_back(BlockEnter{});
    enterVarScope();
    blockVarStack_.push_back({});

    for (auto& s : stmt.body)
        lowerStmt(*s, func);

    vector<VReg> expired = move(blockVarStack_.back());
    blockVarStack_.pop_back();
    leaveVarScope();
    func.instrs.push_back(BlockLeave{move(expired)});
}

void PlnVCodeGen::lowerBranchCond(const Expr& expr, VFunc& func, const string& falseLabel)
{
    if (expr.kind == ExprKind::LogicalNot) {
        auto& e = static_cast<const LogicalNotExpr&>(expr);
        VReg cond = lowerExpr(*e.operand, func);
        flushReleases(func);
        func.instrs.push_back(CondJmp{falseLabel, cond, false});  // jne // LCOV_EXCL_BR_LINE
        return;
    }
    if (expr.kind == ExprKind::LogicalAnd) {
        auto& e = static_cast<const LogicalAndExpr&>(expr);
        lowerBranchCond(*e.left,  func, falseLabel);
        lowerBranchCond(*e.right, func, falseLabel);
        return;
    }
    if (expr.kind == ExprKind::LogicalOr) {
        auto& e = static_cast<const LogicalOrExpr&>(expr);
        int idx = labelCounter_++;
        string skipLabel = ".Llor" + to_string(idx) + "_skip";
        lowerBranchCondTrue(*e.left, func, skipLabel);   // left true → skip right check
        lowerBranchCond(*e.right, func, falseLabel);     // right false → jump to falseLabel
        func.instrs.push_back(Label{skipLabel});
        return;
    }
    VReg cond = lowerExpr(expr, func);
    flushReleases(func);
    func.instrs.push_back(CondJmp{falseLabel, cond, true});  // je // LCOV_EXCL_BR_LINE
}

void PlnVCodeGen::lowerBranchCondTrue(const Expr& expr, VFunc& func, const string& trueLabel)
{
    if (expr.kind == ExprKind::LogicalNot) {
        auto& e = static_cast<const LogicalNotExpr&>(expr);
        VReg cond = lowerExpr(*e.operand, func);
        flushReleases(func);
        func.instrs.push_back(CondJmp{trueLabel, cond, true});   // je: !x true when x==0 // LCOV_EXCL_BR_LINE
        return;
    }
    if (expr.kind == ExprKind::LogicalOr) {
        auto& e = static_cast<const LogicalOrExpr&>(expr);
        lowerBranchCondTrue(*e.left,  func, trueLabel);
        lowerBranchCondTrue(*e.right, func, trueLabel);
        return;
    }
    if (expr.kind == ExprKind::LogicalAnd) {
        auto& e = static_cast<const LogicalAndExpr&>(expr);
        int idx = labelCounter_++;
        string skipLabel = ".Land" + to_string(idx) + "_skip";
        lowerBranchCond(*e.left, func, skipLabel);       // left false → skip right check
        lowerBranchCondTrue(*e.right, func, trueLabel);  // right true → jump to trueLabel
        func.instrs.push_back(Label{skipLabel});
        return;
    }
    VReg cond = lowerExpr(expr, func);
    flushReleases(func);
    func.instrs.push_back(CondJmp{trueLabel, cond, false});  // jne: jump when non-zero // LCOV_EXCL_BR_LINE
}

void PlnVCodeGen::lowerIfStmt(const IfStmt& stmt, VFunc& func)
{
    int    idx      = labelCounter_++;
    string endLabel = ".Lif" + to_string(idx) + "_end";

    if (stmt.elseStmt) {
        string elseLabel = ".Lif" + to_string(idx) + "_else";
        lowerBranchCond(*stmt.cond, func, elseLabel);
        lowerStmt(*stmt.thenStmt, func);
        func.instrs.push_back(Jmp{endLabel});
        func.instrs.push_back(Label{elseLabel});
        lowerStmt(*stmt.elseStmt, func);
    } else {
        lowerBranchCond(*stmt.cond, func, endLabel);
        lowerStmt(*stmt.thenStmt, func);
    }

    func.instrs.push_back(Label{endLabel});
}

void PlnVCodeGen::lowerWhileStmt(const WhileStmt& stmt, VFunc& func)
{
    int    idx        = labelCounter_++;
    string startLabel = ".Lwhile" + to_string(idx) + "_start";
    string endLabel   = ".Lwhile" + to_string(idx) + "_end";

    loopStack_.push_back({startLabel, endLabel});
    func.instrs.push_back(Label{startLabel});
    lowerBranchCond(*stmt.cond, func, endLabel);
    lowerStmt(*stmt.body, func);
    func.instrs.push_back(Jmp{startLabel});
    func.instrs.push_back(Label{endLabel});
    loopStack_.pop_back();
} // LCOV_EXCL_BR_LINE

void PlnVCodeGen::lowerBreakStmt(VFunc& func)
{
    BOOST_ASSERT(!loopStack_.empty());
    func.instrs.push_back(Jmp{loopStack_.back().end});
}

void PlnVCodeGen::lowerContinueStmt(VFunc& func)
{
    BOOST_ASSERT(!loopStack_.empty());
    func.instrs.push_back(Jmp{loopStack_.back().start});
}

void PlnVCodeGen::lowerStmt(const Stmt& stmt, VFunc& func)
{
    lowerStmtKeepingTemps(stmt, func);
    flushReleases(func);
}

void PlnVCodeGen::lowerStmtKeepingTemps(const Stmt& stmt, VFunc& func)
{
    switch (stmt.kind) {
        case StmtKind::Expr:
            lowerExprStmt(static_cast<const ExprStmt&>(stmt), func);
            break;
        case StmtKind::VarDecl:
            lowerVarDeclStmt(static_cast<const VarDeclStmt&>(stmt), func);
            break;
        case StmtKind::Assign:
            lowerAssignStmt(static_cast<const AssignStmt&>(stmt), func);
            break;
        case StmtKind::Return:
            lowerReturnStmt(static_cast<const ReturnStmt&>(stmt), func);
            break;
        case StmtKind::TappleDecl:
            lowerTappleDeclStmt(static_cast<const TappleDeclStmt&>(stmt), func);
            break;
        case StmtKind::Block:
            lowerBlockStmt(static_cast<const BlockStmt&>(stmt), func);
            break;
        case StmtKind::If:
            lowerIfStmt(static_cast<const IfStmt&>(stmt), func);
            break;
        case StmtKind::While:
            lowerWhileStmt(static_cast<const WhileStmt&>(stmt), func);
            break;
        case StmtKind::Break:
            lowerBreakStmt(func);
            break;
        case StmtKind::Continue:
            lowerContinueStmt(func);
            break;
        case StmtKind::ArrAssign:
            lowerArrAssignStmt(static_cast<const ArrAssignStmt&>(stmt), func);
            break;
        case StmtKind::FieldAssign: {
            auto& s   = static_cast<const FieldAssignStmt&>(stmt);
            VReg  src = lowerExpr(*s.value, func);
            VReg  ptr = s.ptrExpr ? lowerExpr(*s.ptrExpr, func) : findVar(s.varName, func);
            if (s.bitWidth) storeBitField(ptr, s, src, func);
            else func.instrs.push_back(DerefStore{ptr, s.offset, src, s.type});
            break;
        }
        default:
            BOOST_ASSERT(false);
    }
}

// -------- Entry point --------

VProg PlnVCodeGen::generate(const Module& module)
{
    VProg prog;
    prog_ = &prog;

    // Populate .rodata entries
    for (auto& d : module.strLiterals)
        prog.data.push_back(VStringLiteral{d.label, d.value}); // LCOV_EXCL_BR_LINE
    for (auto& g : module.globals)
        prog.bss.push_back({g.label, 8, 8, g.isExport}); // LCOV_EXCL_BR_LINE

    for (auto& pf : module.functions) {
        VFunc vf;
        vf.name = pf.name;
        vf.isExport = pf.isExport;
        enterVarScope();
        for (auto& p : pf.params) {
            VReg r = allocVReg();
            vf.params.push_back({r, p.type});
            declareVar(p.name, r);
        }
        for (auto& g : pf.globals)
            bindStatic(g.name, g.label, g.type);
        // Pre-declare single named return variable
        auto zeroInit = [&](VReg r, VRegType t) {
            if (isFloat(t) || isX87(t))
                vf.instrs.push_back(InitVarF{r, t, addFloatLiteral("0.0", t)});
            else
                vf.instrs.push_back(InitVar{r, t, 0});
        };
        // A MEMORY-class struct's slot is the hidden parameter, already declared.
        bool retIsParam = std::any_of(pf.params.begin(), pf.params.end(),
                                      [&](const VarDef& p) { return p.name == pf.retVarName; });
        if (!pf.retVarName.empty() && !retIsParam) {
            VReg r = allocVReg();
            zeroInit(r, pf.retType);
            declareVar(pf.retVarName, r);
        }
        // Pre-declare multiple named return variables
        for (auto& rv : pf.retVars) {
            VReg r = allocVReg();
            zeroInit(r, rv.type);
            declareVar(rv.name, r);
        }
        currentPlnFunc_ = &pf;
        for (auto& s : pf.body)
            lowerStmt(*s, vf);
        if (vf.instrs.empty() || !std::get_if<RetPln>(&vf.instrs.back()))
            lowerBareReturn(vf);
        currentPlnFunc_ = nullptr;
        leaveVarScope();
        prog.funcs.push_back(move(vf));
    }

    return prog;
} // LCOV_EXCL_BR_LINE
