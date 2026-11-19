/// VCodeGen: lowers C++ AST nodes to virtual instruction set (VProg).
///
/// @file PlnVCodeGen.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once
#include <map>
#include <string>
#include <vector>
#include "PlnNode.h"
#include "PlnVProg.h"

using std::map;
using std::string;
using std::vector;

class PlnVCodeGen {
    int nextVReg = 0;
    int labelCounter_ = 0;       // unique label index for control-flow labels
    int floatLabelCounter_ = 0;  // unique label index for float literals
    VProg* prog_ = nullptr;      // set during generate(); used to push float literals

    // Variable symbol table: scoped stack of name -> storage mappings.
    // enterVarScope/leaveVarScope push/pop; lookups search from innermost.
    // A global lives in a static slot instead of a VReg; every access goes to
    // memory, since a call may change it.
    struct VarBinding { VReg reg = -1; string label; VRegType type = VRegType::Int64; };
    vector<map<string, VarBinding>> varScopes;
    void enterVarScope();
    void leaveVarScope();
    const VarBinding& findBinding(const string& name) const;
    VReg findVar(const string& name, VFunc& func);
    void declareVar(const string& name, VReg r);
    void bindStatic(const string& name, const string& label, VRegType type);

    VReg allocVReg();

    const PlnFunc* currentPlnFunc_ = nullptr;  // set while lowering a Palan function body
    vector<vector<VReg>> blockVarStack_;

    // A temp's release statements, held until its full expression has been
    // evaluated (TempExpr).
    vector<const Stmt*> pendingReleases_;
    // A temp's reset statements, held until the arguments of the call it is
    // given to have been evaluated (lowerArgs).
    vector<const Stmt*> pendingResets_;

    struct LoopLabels { string start; string end; };
    vector<LoopLabels> loopStack_;  // push/pop in lowerWhileStmt

    string addFloatLiteral(const string& value, VRegType type);
    VReg lowerExpr(const Expr& expr, VFunc& func);
    void lowerStmt(const Stmt& stmt, VFunc& func);
    void lowerStmtKeepingTemps(const Stmt& stmt, VFunc& func);
    void lowerExprStmt(const ExprStmt& stmt, VFunc& func);
    void lowerVarDeclStmt(const VarDeclStmt& stmt, VFunc& func);
    void lowerAssignStmt(const AssignStmt& stmt, VFunc& func);
    VReg lowerU64FloatConvert(VReg src, VRegType from, VRegType to, VFunc& func);
    VReg shiftImm(VReg src, int count, ShiftOp op, VRegType type, VFunc& func);
    VReg lowerPow2DivMod(VReg x, int k, bool isMod, VRegType type, VFunc& func);
    VReg lowerArrIndexExpr(const ArrIndexExpr& expr, VFunc& func);
    void lowerArrAssignStmt(const ArrAssignStmt& stmt, VFunc& func);
    VReg convertWidth(VReg src, VRegType from, VRegType to, VFunc& func);
    VReg loadBitField(VReg ptr, const FieldAccessExpr& e, VFunc& func);
    void storeBitField(VReg ptr, const FieldAssignStmt& s, VReg src, VFunc& func);
    void lowerReturnStmt(const ReturnStmt& stmt, VFunc& func);
    void lowerCCCallExpr(const CCCallExpr& expr, VFunc& func);
    vector<VReg> lowerCCallArgs(const CCCallExpr& expr, vector<StructArgSpan>& spans, VFunc& func,
                                vector<VReg> args = {});
    VReg lowerStructRetDest(const std::optional<StructRet>& sr, vector<VReg>& args, VFunc& func);
    std::pair<vector<VReg>, vector<VRegType>> structRetDsts(const std::optional<StructRet>& sr);
    void storeStructRet(VReg dest, const std::optional<StructRet>& sr, const vector<VReg>& dsts, VFunc& func);
    void lowerSlotReturn(VFunc& func);
    void lowerPtrCallStmt(const PtrCallExpr& expr, VFunc& func);
    VReg loadEightbyte(VReg ptr, int offset, const Eightbyte& eb, VFunc& func);
    void storeEightbyte(VReg ptr, int offset, VReg src, const Eightbyte& eb, VFunc& func);
    void lowerPlnCallExpr(const PlnCallExpr& expr, VFunc& func);
    void lowerSysCallExpr(const SysCallExpr& expr, VFunc& func);
    vector<VReg> lowerArgs(const vector<unique_ptr<Expr>>& args, VFunc& func);
    VReg lowerTempExpr(const TempExpr& expr, VFunc& func);
    void flushReleases(VFunc& func);
    void lowerTappleDeclStmt(const TappleDeclStmt& stmt, VFunc& func);
    void lowerBlockStmt(const BlockStmt& stmt, VFunc& func);
    void lowerBranchCond(const Expr& expr, VFunc& func, const std::string& falseLabel);
    void lowerBranchCondTrue(const Expr& expr, VFunc& func, const std::string& trueLabel);
    void lowerIfStmt(const IfStmt& stmt, VFunc& func);
    void lowerWhileStmt(const WhileStmt& stmt, VFunc& func);
    void lowerBreakStmt(VFunc& func);
    void lowerContinueStmt(VFunc& func);

public:
    VProg generate(const Module& module);
};
