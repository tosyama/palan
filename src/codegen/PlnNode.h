/// Palan AST node definitions for code generation.
///
/// @file PlnNode.h
/// @copyright 2026 YAMAGUCHI Toshinobu

#pragma once
#include <string>
#include <vector>
#include <memory>
#include "PlnVProg.h"

using std::string;
using std::vector;
using std::unique_ptr;

// -------- Expressions --------

enum class ExprKind {
    StrLit,
    IntLit,
    UintLit,
    FloLit,
    Id,
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Neg,
    BitAnd,
    BitOr,
    BitXor,
    BitNot,
    Shift,
    Cmp,
    Convert,
    CCCall,
    PlnCall,
    SysCall,
    ArrIndex,
    FieldAccess,
    LogicalNot,
    LogicalAnd,
    LogicalOr,
    AddrOf,
    StaticAlloc,
    FuncRef,
    PtrCall,
    Temp,
    FrameAlloc,
};

struct Expr {
    const ExprKind kind;
    virtual ~Expr() = default;
protected:
    explicit Expr(ExprKind k) : kind(k) {}
};

struct StrLitExpr : Expr {
    StrLitExpr() : Expr(ExprKind::StrLit) {}
    string label;
};

// Reference to a cinclude'd C global variable (e.g. `stdout`). Lowered to
// LeaLabel+DerefLoad in PlnVCodeGen -- no dedicated VInstr, no x86 change.
// A block in static storage; its value is the block's address.
struct StaticAllocExpr : Expr {
    StaticAllocExpr() : Expr(ExprKind::StaticAlloc) {}
    string label;
    int    size  = 0;
    int    align = 1;
};

// A function's address, as a function pointer value. Functions are emitted
// as unmangled assembly labels (see PlnVCodeGen::generate), so this is just
// the address of that label -- lowered to a bare LeaLabel, same as
// StrLitExpr, with no dereference.
struct FuncRefExpr : Expr {
    FuncRefExpr() : Expr(ExprKind::FuncRef) {}
    string name;
};

// A call through a function pointer.
// One eightbyte of a struct passed or returned by value: the register type
// it travels in, and how many of its bytes belong to the struct (1-8; a width
// other than 1/2/4/8 is read or written piecewise). flo64: taken up by one
// flo64, so it is read whole rather than as two flo32 halves.
struct Eightbyte { VRegType type; int size; bool flo64 = false; };

// Where a call returning a struct writes it (a pointer), and the struct's
// eightbytes when it comes back in registers; none means MEMORY class, for
// which dest is passed as the hidden first argument.
struct StructRet {
    unique_ptr<Expr>  dest;
    vector<Eightbyte> eightbytes;
};

struct PtrCallExpr : Expr {
    PtrCallExpr() : Expr(ExprKind::PtrCall) {}
    unique_ptr<Expr> callee;
    bool     hasRet  = false;
    VRegType retType = VRegType::Int64;
    vector<unique_ptr<Expr>> args;
    std::optional<StructRet> structRet;
};

struct IntLitExpr : Expr {
    IntLitExpr() : Expr(ExprKind::IntLit) {}
    string   value;  // decimal string
    VRegType type = VRegType::Int64;
};

struct UintLitExpr : Expr {
    UintLitExpr() : Expr(ExprKind::UintLit) {}
    string   value;  // decimal string
    VRegType type = VRegType::Uint64;
};

struct FloLitExpr : Expr {
    FloLitExpr() : Expr(ExprKind::FloLit) {}
    string   value;  // decimal string e.g. "3.14"
    VRegType type = VRegType::Float64;
};

struct IdExpr : Expr {
    IdExpr() : Expr(ExprKind::Id) {}
    string name;
};

// A block in the function's stack frame; its value is the block's address.
struct FrameAllocExpr : Expr {
    FrameAllocExpr() : Expr(ExprKind::FrameAlloc) {}
    int size  = 0;
    int align = 1;
};

struct AddrOfExpr : Expr {
    AddrOfExpr() : Expr(ExprKind::AddrOf) {}
    string name;
};

struct AddExpr : Expr {
    AddExpr() : Expr(ExprKind::Add) {}
    unique_ptr<Expr> left;
    unique_ptr<Expr> right;
    VRegType type = VRegType::Int64;
};

struct SubExpr : Expr {
    SubExpr() : Expr(ExprKind::Sub) {}
    unique_ptr<Expr> left;
    unique_ptr<Expr> right;
    VRegType type = VRegType::Int64;
};

struct MulExpr : Expr {
    MulExpr() : Expr(ExprKind::Mul) {}
    unique_ptr<Expr> left;
    unique_ptr<Expr> right;
    VRegType type = VRegType::Int64;
};

struct DivExpr : Expr {
    DivExpr() : Expr(ExprKind::Div) {}
    unique_ptr<Expr> left;
    unique_ptr<Expr> right;
    VRegType type = VRegType::Int64;
};

struct ModExpr : Expr {
    ModExpr() : Expr(ExprKind::Mod) {}
    unique_ptr<Expr> left;
    unique_ptr<Expr> right;
    VRegType type = VRegType::Int64;
};

struct NegExpr : Expr {
    NegExpr() : Expr(ExprKind::Neg) {}
    unique_ptr<Expr> operand;
    VRegType type = VRegType::Int64;
};

struct BitAndExpr : Expr {
    BitAndExpr() : Expr(ExprKind::BitAnd) {}
    unique_ptr<Expr> left;
    unique_ptr<Expr> right;
    VRegType type = VRegType::Int64;
};

struct BitOrExpr : Expr {
    BitOrExpr() : Expr(ExprKind::BitOr) {}
    unique_ptr<Expr> left;
    unique_ptr<Expr> right;
    VRegType type = VRegType::Int64;
};

struct BitXorExpr : Expr {
    BitXorExpr() : Expr(ExprKind::BitXor) {}
    unique_ptr<Expr> left;
    unique_ptr<Expr> right;
    VRegType type = VRegType::Int64;
};

struct ShiftExpr : Expr {
    ShiftExpr() : Expr(ExprKind::Shift) {}
    bool isRight = false;  // '>>>' (logical) rather than '<<<'
    unique_ptr<Expr> left;
    unique_ptr<Expr> right;
    VRegType type = VRegType::Int64;
};

struct BitNotExpr : Expr {
    BitNotExpr() : Expr(ExprKind::BitNot) {}
    unique_ptr<Expr> operand;
    VRegType type = VRegType::Int64;
};

struct CmpExpr : Expr {
    CmpExpr() : Expr(ExprKind::Cmp) {}
    string           op;           // "<", "<=", ">", ">=", "==", "!="
    unique_ptr<Expr> left;
    unique_ptr<Expr> right;
    VRegType operandType = VRegType::Int64;  // type of lhs/rhs operands
};

struct ConvertExpr : Expr {
    ConvertExpr() : Expr(ExprKind::Convert) {}
    VRegType         from;
    VRegType         to;
    unique_ptr<Expr> src;
};

struct CCCallExpr : Expr {
    CCCallExpr() : Expr(ExprKind::CCCall) {}
    string name;
    bool     hasRet = false;
    VRegType retType = VRegType::Int64;
    vector<unique_ptr<Expr>> args;
    std::optional<StructRet> structRet;
    // Parallel to args: set for an argument passed to a by-value struct
    // parameter, whose expression yields a pointer to the struct to copy.
    // Empty eightbytes means MEMORY class (copied to the stack).
    struct StructArg { int size; vector<Eightbyte> eightbytes; };
    vector<std::optional<StructArg>> structArgs;
};

struct PlnCallExpr : Expr {
    PlnCallExpr() : Expr(ExprKind::PlnCall) {}
    string name;
    bool     hasRet  = false;
    VRegType retType = VRegType::Int64;
    vector<unique_ptr<Expr>> args;
    std::optional<StructRet> structRet;
};

// A raw Linux syscall call (Linux syscall ABI, not System V) -- resolved by
// palan-sa to a `syscall` declaration and folded to a literal number, so
// this carries a number instead of a name (no symbol to link against).
struct SysCallExpr : Expr {
    SysCallExpr() : Expr(ExprKind::SysCall) {}
    long long sysNum = 0;
    bool     hasRet  = false;
    VRegType retType = VRegType::Int64;
    vector<unique_ptr<Expr>> args;
};

struct Stmt;

// A temp declared and initialized by body, read as value. Its release, and
// that of the temps body's statements hold, runs only once the enclosing full
// expression has been evaluated, since a call it is given to may return a
// borrow of it. Its reset runs once all of the
// arguments of the call it is given to have been evaluated, since a later
// one may borrow from the owner it was taken from.
struct TempExpr : Expr {
    TempExpr() : Expr(ExprKind::Temp) {}
    vector<unique_ptr<Stmt>> body;
    unique_ptr<Expr>         value;
    vector<unique_ptr<Stmt>> release;
    vector<unique_ptr<Stmt>> reset;
};

struct VarDef { string name; VRegType type; };

// -------- Statements --------

enum class StmtKind {
    Expr,
    VarDecl,
    Assign,
    Return,
    TappleDecl,
    Block,
    If,
    While,
    Break,
    Continue,
    ArrAssign,
    FieldAssign,
};

struct Stmt {
    const StmtKind kind;
    virtual ~Stmt() = default;
protected:
    explicit Stmt(StmtKind k) : kind(k) {}
};

struct ExprStmt : Stmt {
    ExprStmt() : Stmt(StmtKind::Expr) {}
    unique_ptr<Expr> body;
};

struct VarEntry {
    string           varName;
    string           label;      // a global's static slot; empty for a frame variable
    VRegType         type = VRegType::Int64;
    unique_ptr<Expr> init;       // nullable
};

struct VarDeclStmt : Stmt {
    VarDeclStmt() : Stmt(StmtKind::VarDecl) {}
    vector<VarEntry> vars;
};

struct AssignStmt : Stmt {
    AssignStmt() : Stmt(StmtKind::Assign) {}
    string           name;                       // destination variable name
    VRegType         type = VRegType::Int64;     // type of the assigned value
    unique_ptr<Expr> value;
};

struct ReturnStmt : Stmt {
    ReturnStmt() : Stmt(StmtKind::Return) {}
    vector<unique_ptr<Expr>> values;  // empty for bare return
};

struct TappleDeclStmt : Stmt {
    TappleDeclStmt() : Stmt(StmtKind::TappleDecl) {}
    vector<VarDef>           vars;      // the values returned in registers
    string                   funcName;
    vector<unique_ptr<Expr>> args;
    vector<VRegType>         retTypes;
    vector<unique_ptr<Expr>> retDests;  // where each returned struct is written, in order
};

struct BlockStmt : Stmt {
    BlockStmt() : Stmt(StmtKind::Block) {}
    vector<unique_ptr<Stmt>> body;
};

struct IfStmt : Stmt {
    IfStmt() : Stmt(StmtKind::If) {}
    unique_ptr<Expr> cond;
    unique_ptr<Stmt> thenStmt;  // always BlockStmt
    unique_ptr<Stmt> elseStmt;  // nullptr | BlockStmt | IfStmt (else-if chain)
};

struct WhileStmt : Stmt {
    WhileStmt() : Stmt(StmtKind::While) {}
    unique_ptr<Expr> cond;
    unique_ptr<Stmt> body;  // always BlockStmt
};

struct BreakStmt : Stmt {
    BreakStmt() : Stmt(StmtKind::Break) {}
};

struct ContinueStmt : Stmt {
    ContinueStmt() : Stmt(StmtKind::Continue) {}
};

struct ArrIndexExpr : Expr {
    explicit ArrIndexExpr() : Expr(ExprKind::ArrIndex), scale(4), type(VRegType::Int64), idx_type(VRegType::Int64) {}
    unique_ptr<Expr> array;      // Ptr64 expression (heap pointer)
    unique_ptr<Expr> index;      // integer index expression
    int scale;                    // element byte size or constant stride
    unique_ptr<Expr> scale_expr; // non-null when inner dim is a runtime variable
    bool addrOnly = false;        // true for embedded 2D row access (compute address, no dereference)
    VRegType type;                // element type (Ptr64 for row access)
    VRegType idx_type;            // index expression type
};

struct LogicalNotExpr : Expr {
    LogicalNotExpr() : Expr(ExprKind::LogicalNot) {}
    unique_ptr<Expr> operand;
};

struct LogicalAndExpr : Expr {
    LogicalAndExpr() : Expr(ExprKind::LogicalAnd) {}
    unique_ptr<Expr> left;
    unique_ptr<Expr> right;
};

struct LogicalOrExpr : Expr {
    LogicalOrExpr() : Expr(ExprKind::LogicalOr) {}
    unique_ptr<Expr> left;
    unique_ptr<Expr> right;
};

struct ArrAssignStmt : Stmt {
    explicit ArrAssignStmt() : Stmt(StmtKind::ArrAssign) {}
    unique_ptr<Expr> array;
    unique_ptr<Expr> index;
    int scale;
    VRegType type;
    VRegType idx_type = VRegType::Int64;  // index expression type
    unique_ptr<Expr> value;
};

struct FieldAccessExpr : Expr {
    FieldAccessExpr() : Expr(ExprKind::FieldAccess) {}
    string           varName;
    unique_ptr<Expr> ptrExpr;
    int              offset;
    VRegType         type;
    bool             addrOnly = false;  // true: field data is embedded inline; compute ptr+offset, do not load
    int              bitShift = 0;      // a C bit-field within the unit of `type` at offset; width 0 if not
    int              bitWidth = 0;
};

struct FieldAssignStmt : Stmt {
    FieldAssignStmt() : Stmt(StmtKind::FieldAssign) {}
    string           varName;
    unique_ptr<Expr> ptrExpr;
    int              offset;
    VRegType         type;
    unique_ptr<Expr> value;
    int              bitShift = 0;  // as FieldAccessExpr
    int              bitWidth = 0;
};

// -------- Module --------

struct StrLiteralDef {
    string label;
    string value;
};

// A name a function binds to a static slot at its entry.
struct StaticBinding { string name; string label; VRegType type; };

struct PlnFunc {
    string             name;
    vector<StaticBinding> globals;
    vector<VarDef>     params;
    bool               hasRetType = false;
    VRegType           retType    = VRegType::Int64;
    string             retVarName;   // single named return variable; empty if none
    vector<VarDef>     retVars;      // multiple named return variables (size>=2)
    // A single returned struct is built in retSlot, a frame variable, and
    // returned in registers by these eightbytes; a MEMORY-class one's slot is
    // the hidden first parameter, returned as retVarName.
    string             retSlot;
    vector<Eightbyte>  retSlotEightbytes;
    vector<unique_ptr<Stmt>> body;
    bool               isExport = false;
};

struct Module {
    string original;
    vector<StrLiteralDef>    strLiterals;
    struct GlobalSlot { string label; bool isExport; };
    vector<GlobalSlot>       globals;  // this module's global slots
    vector<PlnFunc>          functions;
};
