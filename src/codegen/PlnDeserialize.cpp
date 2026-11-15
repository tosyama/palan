/// Deserialize sa.json into the codegen AST (Module).
///
/// @file PlnDeserialize.cpp
/// @copyright 2026 YAMAGUCHI Toshinobu

#include "PlnDeserialize.h"
#include "PlnCodegenMessage.h"
#include <boost/assert.hpp>
#include <iostream>
using std::cerr;
using std::endl;

using namespace std;

// Maps one eightbyte descriptor ({"class":"int"|"sse","size":1..8}) to the
// register type it travels in. Sign doesn't matter here (movInstrForType only
// keys off width), so every "int" eightbyte maps to a plain signed VRegType
// regardless of the source field's actual signedness.
static Eightbyte toEightbyte(const json& eb) {
    int size = eb["size"].get<int>();
    if (eb["class"] == "sse")
        return {size == 4 ? VRegType::Float32 : VRegType::Float64, size, eb.value("flo64", false)};
    VRegType t = size == 1 ? VRegType::Int8 : size == 2 ? VRegType::Int16
               : size <= 4 ? VRegType::Int32 : VRegType::Int64;
    return {t, size};
}

static VRegType toVRegType(const json& vt) {
    if (vt["type-kind"] == "pntr") return VRegType::Ptr64;
    string name = vt["type-name"].get<string>();
    if (name == "int8")   return VRegType::Int8;
    if (name == "int16")  return VRegType::Int16;
    if (name == "int32")  return VRegType::Int32;
    if (name == "int64")  return VRegType::Int64;
    if (name == "uint8")  return VRegType::Uint8;
    if (name == "uint16") return VRegType::Uint16;
    if (name == "uint32") return VRegType::Uint32;
    if (name == "uint64") return VRegType::Uint64;
    if (name == "flo32")  return VRegType::Float32;
    if (name == "flo64")  return VRegType::Float64;
    // SA guarantees a bool holds 0/1, so it needs nothing beyond a uint8.
    if (name == "bool")   return VRegType::Uint8;
    cerr << PlnCodegenMessage::getMessage(E_UnknownType, name) << endl;
    exit(1);
}

static unique_ptr<Expr> deserializeExpr(const json& j);
static vector<unique_ptr<Stmt>> deserializeStatements(const json& arr);
static std::optional<StructRet> deserializeStructRet(const json& j);

static void deserializeArgs(const json& call, vector<unique_ptr<Expr>>& args)
{
    if (!call.contains("args"))
        return;
    for (auto& arg : call["args"])
        args.push_back(deserializeExpr(arg));
} // LCOV_EXCL_BR_LINE

static unique_ptr<Expr> deserializeExpr(const json& j)
{
    string expr_type = j["expr-type"];

    if (expr_type == "lit-str") {
        auto e = make_unique<StrLitExpr>();
        e->label = j["label"];
        return e;
    }
    if (expr_type == "lit-int") {
        auto e = make_unique<IntLitExpr>();
        e->value = j["value"];
        e->type  = j.contains("value-type") ? toVRegType(j["value-type"]) : VRegType::Int64;
        return e;
    }
    if (expr_type == "lit-uint") {
        auto e = make_unique<UintLitExpr>();
        e->value = j["value"];
        e->type  = j.contains("value-type") ? toVRegType(j["value-type"]) : VRegType::Uint64;
        return e;
    }
    if (expr_type == "lit-flo") {
        auto e = make_unique<FloLitExpr>();
        e->value = j["value"];
        e->type  = j.contains("value-type") ? toVRegType(j["value-type"]) : VRegType::Float64;
        return e;
    }
    if (expr_type == "id") {
        auto e = make_unique<IdExpr>();
        e->name = j["name"];
        return e;
    }
    if (expr_type == "temp") {
        auto e = make_unique<TempExpr>();
        e->body    = deserializeStatements(j["body"]);
        e->value   = deserializeExpr(j["value"]);
        e->release = deserializeStatements(j["release"]);
        if (j.contains("reset"))
            e->reset = deserializeStatements(j["reset"]);
        return e;
    }
    if (expr_type == "frame-alloc") {
        auto e = make_unique<FrameAllocExpr>();
        e->size  = j["size"];
        e->align = j["align"];
        return e;
    }
    if (expr_type == "static-alloc") {
        auto e = make_unique<StaticAllocExpr>();
        e->label = j["label"];
        e->size  = j["size"];
        e->align = j["align"];
        return e;
    }
    if (expr_type == "addr-of") {
        auto e = make_unique<AddrOfExpr>();
        e->name = j["name"];
        return e;
    }
    if (expr_type == "func-ref") {
        auto e = make_unique<FuncRefExpr>();
        e->name = j["name"];
        return e;
    }
    if (expr_type == "call-ptr") {
        auto e = make_unique<PtrCallExpr>();
        e->callee = deserializeExpr(j["callee"]);
        if (j.contains("value-type")) {
            e->hasRet  = true;
            e->retType = toVRegType(j["value-type"]);
        }
        deserializeArgs(j, e->args);
        e->structRet = deserializeStructRet(j);
        return e;
    }
    if (expr_type == "convert") {
        auto e = make_unique<ConvertExpr>();
        e->from = toVRegType(j["from-type"]);
        e->to   = toVRegType(j["value-type"]);
        e->src  = deserializeExpr(j["src"]);
        return e;
    }
    if (expr_type == "add") {
        auto e = make_unique<AddExpr>();
        e->left  = deserializeExpr(j["left"]);
        e->right = deserializeExpr(j["right"]);
        e->type  = j.contains("value-type") ? toVRegType(j["value-type"]) : VRegType::Int64;
        return e;
    }
    if (expr_type == "sub") {
        auto e = make_unique<SubExpr>();
        e->left  = deserializeExpr(j["left"]);
        e->right = deserializeExpr(j["right"]);
        e->type  = j.contains("value-type") ? toVRegType(j["value-type"]) : VRegType::Int64;
        return e;
    }
    if (expr_type == "mul") {
        auto e = make_unique<MulExpr>();
        e->left  = deserializeExpr(j["left"]);
        e->right = deserializeExpr(j["right"]);
        e->type  = j.contains("value-type") ? toVRegType(j["value-type"]) : VRegType::Int64;
        return e;
    }
    if (expr_type == "div") {
        auto e = make_unique<DivExpr>();
        e->left  = deserializeExpr(j["left"]);
        e->right = deserializeExpr(j["right"]);
        e->type  = j.contains("value-type") ? toVRegType(j["value-type"]) : VRegType::Int64;
        return e;
    }
    if (expr_type == "mod") {
        auto e = make_unique<ModExpr>();
        e->left  = deserializeExpr(j["left"]);
        e->right = deserializeExpr(j["right"]);
        e->type  = j.contains("value-type") ? toVRegType(j["value-type"]) : VRegType::Int64;
        return e;
    }
    if (expr_type == "neg") {
        auto e     = make_unique<NegExpr>();
        e->operand = deserializeExpr(j["operand"]);
        e->type    = j.contains("value-type") ? toVRegType(j["value-type"]) : VRegType::Int64;
        return e;
    }
    if (expr_type == "bitand") {
        auto e = make_unique<BitAndExpr>();
        e->left  = deserializeExpr(j["left"]);
        e->right = deserializeExpr(j["right"]);
        e->type  = j.contains("value-type") ? toVRegType(j["value-type"]) : VRegType::Int64;
        return e;
    }
    if (expr_type == "bitor") {
        auto e = make_unique<BitOrExpr>();
        e->left  = deserializeExpr(j["left"]);
        e->right = deserializeExpr(j["right"]);
        e->type  = j.contains("value-type") ? toVRegType(j["value-type"]) : VRegType::Int64;
        return e;
    }
    if (expr_type == "bitxor") {
        auto e = make_unique<BitXorExpr>();
        e->left  = deserializeExpr(j["left"]);
        e->right = deserializeExpr(j["right"]);
        e->type  = j.contains("value-type") ? toVRegType(j["value-type"]) : VRegType::Int64;
        return e;
    }
    if (expr_type == "shl" || expr_type == "shr") {
        auto e = make_unique<ShiftExpr>();
        e->isRight = expr_type == "shr";
        e->left  = deserializeExpr(j["left"]);
        e->right = deserializeExpr(j["right"]);
        e->type  = j.contains("value-type") ? toVRegType(j["value-type"]) : VRegType::Int64;
        return e;
    }
    if (expr_type == "bitnot") {
        auto e     = make_unique<BitNotExpr>();
        e->operand = deserializeExpr(j["operand"]);
        e->type    = j.contains("value-type") ? toVRegType(j["value-type"]) : VRegType::Int64;
        return e;
    }
    if (expr_type == "cmp") {
        auto e = make_unique<CmpExpr>();
        e->op    = j["op"];
        e->left  = deserializeExpr(j["left"]);
        e->right = deserializeExpr(j["right"]);
        e->operandType = j["left"].contains("value-type")
                         ? toVRegType(j["left"]["value-type"]) : VRegType::Int64;
        return e;
    }
    if (expr_type == "call") {
        string func_type = j.value("func-type", "");
        if (func_type == "c") {
            auto e = make_unique<CCCallExpr>();
            e->name = j["name"];
            if (j.contains("value-type")) {
                e->hasRet  = true;
                e->retType = toVRegType(j["value-type"]);
            }
            e->structRet = deserializeStructRet(j);
            deserializeArgs(j, e->args);
            for (auto& arg : j.value("args", json::array())) {
                if (!arg.contains("struct-arg")) {
                    e->structArgs.emplace_back();
                    continue;
                }
                const json& sa = arg["struct-arg"];
                CCCallExpr::StructArg info{sa["size"].get<int>(), {}};
                for (auto& eb : sa["eightbytes"])
                    info.eightbytes.push_back(toEightbyte(eb));
                e->structArgs.push_back(move(info));
            }
            return e;
        } else if (func_type == "syscall") {
            auto e = make_unique<SysCallExpr>();
            e->sysNum = j["syscall-number"].get<long long>();
            if (j.contains("value-type")) {
                e->hasRet  = true;
                e->retType = toVRegType(j["value-type"]);
            }
            deserializeArgs(j, e->args);
            return e;
        } else {
            auto e = make_unique<PlnCallExpr>();
            e->name = j["name"];
            if (j.contains("value-type")) {
                e->hasRet  = true;
                e->retType = toVRegType(j["value-type"]);
            }
            e->structRet = deserializeStructRet(j);
            deserializeArgs(j, e->args);
            return e;
        }
    }

    if (expr_type == "logical-not") {
        auto e = make_unique<LogicalNotExpr>();
        e->operand = deserializeExpr(j["operand"]);
        return e;
    }
    if (expr_type == "logical-and") {
        auto e = make_unique<LogicalAndExpr>();
        e->left  = deserializeExpr(j["left"]);
        e->right = deserializeExpr(j["right"]);
        return e;
    }
    if (expr_type == "logical-or") {
        auto e = make_unique<LogicalOrExpr>();
        e->left  = deserializeExpr(j["left"]);
        e->right = deserializeExpr(j["right"]);
        return e;
    }

    if (expr_type == "arr-index") {
        auto e = make_unique<ArrIndexExpr>();
        e->array    = deserializeExpr(j["array"]);
        e->index    = deserializeExpr(j["index"]);
        const json& es = j["elem-size"];
        if (es.value("expr-type", "") == "lit-uint") {
            e->scale = stoi(es["value"].get<string>());
        } else {
            e->scale_expr = deserializeExpr(es);  // variable inner dim: mul expression
        }
        // Address-vs-load is decided by SA and carried explicitly as "addr-only"
        // (same contract as field-access below): true for embedded struct-array
        // row access, embedded 2D row access, and pointer-to-struct dereference
        // (`p[i]` where the pointee is a struct); false for a scalar load.
        e->addrOnly = j.value("addr-only", false);
        e->type     = toVRegType(j["value-type"]);
        e->idx_type = toVRegType(j["index"]["value-type"]);
        return e;
    }

    if (expr_type == "field-access") {
        auto e = make_unique<FieldAccessExpr>();
        if (j.contains("ptr-expr"))
            e->ptrExpr = deserializeExpr(j["ptr-expr"]);
        else
            e->varName = j["var"];
        e->offset   = j["offset"];
        e->type     = toVRegType(j["value-type"]);
        e->addrOnly = j.value("addr-only", false);
        return e;
    }

    cerr << PlnCodegenMessage::getMessage(E_UnknownExprType, j["expr-type"].get<string>()) << endl;
    exit(1);
}

static unique_ptr<Stmt> deserializeStmt(const json& j)
{
    string stmt_type = j["stmt-type"];

    if (stmt_type == "expr") {
        auto s = make_unique<ExprStmt>();
        s->body = deserializeExpr(j["body"]);
        return s;
    }
    if (stmt_type == "var-decl") {
        auto s = make_unique<VarDeclStmt>();
        for (auto& jv : j["vars"]) {
            VarEntry ve;
            ve.varName  = jv["name"];
            ve.type     = toVRegType(jv["var-type"]);
            ve.label    = jv.value("label", "");
            if (jv.contains("init")) {
                ve.init = deserializeExpr(jv["init"]);
            }
            s->vars.push_back(move(ve));
        }
        return s;
    }
    if (stmt_type == "assign") {
        auto s = make_unique<AssignStmt>();
        s->name  = j["name"];
        s->type  = j["value"].contains("value-type") ? toVRegType(j["value"]["value-type"])
                                                      : VRegType::Int64;
        s->value = deserializeExpr(j["value"]);
        return s;
    }
    if (stmt_type == "return") {
        auto s = make_unique<ReturnStmt>();
        if (j.contains("values"))
            for (auto& v : j["values"])
                s->values.push_back(deserializeExpr(v));
        return s;
    }
    if (stmt_type == "tapple-decl") {
        auto s = make_unique<TappleDeclStmt>();
        s->funcName = j["value"]["name"];
        for (auto& jv : j["vars"]) {
            if (jv.contains("ret-dest")) {
                s->retDests.push_back(deserializeExpr(jv["ret-dest"]));
                continue;
            }
            s->vars.push_back({jv["var-name"], toVRegType(jv["var-type"])});
            s->retTypes.push_back(toVRegType(jv["var-type"]));
        }
        deserializeArgs(j["value"], s->args);
        return s;
    }
    if (stmt_type == "block") {
        auto s = make_unique<BlockStmt>();
        s->body = deserializeStatements(j["body"]);
        return s;
    }
    if (stmt_type == "if") {
        auto s = make_unique<IfStmt>();
        s->cond = deserializeExpr(j["cond"]);
        auto tb = make_unique<BlockStmt>();
        tb->body = deserializeStatements(j["then"]["body"]);
        s->thenStmt = move(tb);
        if (j.contains("else")) {
            if (j["else"]["stmt-type"] == "if") {
                s->elseStmt = deserializeStmt(j["else"]);
            } else {
                auto eb = make_unique<BlockStmt>();
                eb->body = deserializeStatements(j["else"]["body"]);
                s->elseStmt = move(eb);
            }
        }
        return s;
    }
    if (stmt_type == "while") {
        auto s = make_unique<WhileStmt>();
        s->cond = deserializeExpr(j["cond"]);
        auto body = make_unique<BlockStmt>();
        body->body = deserializeStatements(j["body"]);
        s->body = move(body);
        return s;
    }
    if (stmt_type == "break") {
        return make_unique<BreakStmt>();
    }
    if (stmt_type == "continue") {
        return make_unique<ContinueStmt>();
    }
    if (stmt_type == "arr-assign") {
        auto s = make_unique<ArrAssignStmt>();
        const json& target = j["target"];
        s->array    = deserializeExpr(target["array"]);
        s->index    = deserializeExpr(target["index"]);
        s->scale    = stoi(target["elem-size"]["value"].get<string>());
        s->type     = toVRegType(target["value-type"]);
        s->idx_type = toVRegType(target["index"]["value-type"]);
        s->value    = deserializeExpr(j["value"]);
        return s;
    }
    if (stmt_type == "field-assign") {
        auto s = make_unique<FieldAssignStmt>();
        if (j.contains("ptr-expr"))
            s->ptrExpr = deserializeExpr(j["ptr-expr"]);
        else
            s->varName = j["var"];
        s->offset = j["offset"];
        s->type   = toVRegType(j["value-type"]);
        s->value  = deserializeExpr(j["value"]);
        return s;
    }
    cerr << PlnCodegenMessage::getMessage(E_UnknownStmtType, stmt_type) << endl;
    exit(1);
} // LCOV_EXCL_BR_LINE

static vector<unique_ptr<Stmt>> deserializeStatements(const json& arr)
{
    vector<unique_ptr<Stmt>> result;
    for (auto& j : arr)
        result.push_back(deserializeStmt(j));
    return result;
}

static std::optional<StructRet> deserializeStructRet(const json& j)
{
    if (!j.contains("ret-dest"))
        return std::nullopt;
    StructRet sr;
    sr.dest = deserializeExpr(j["ret-dest"]);
    for (auto& eb : j["struct-ret"]["eightbytes"])
        sr.eightbytes.push_back(toEightbyte(eb));
    return sr;
}

static PlnFunc deserializeSig(const json& jf)
{
    PlnFunc pf;
    pf.name = jf["name"];
    for (auto& jp : jf["parameters"])
        pf.params.push_back({jp["name"], toVRegType(jp["var-type"])});
    for (auto& jg : jf.value("globals", json::array()))
        pf.globals.push_back({jg["name"], jg["label"], toVRegType(jg["var-type"])});
    return pf;
} // LCOV_EXCL_BR_LINE

Module deserialize(const json& sa)
{
    Module mod;
    mod.original = sa.value("original", "");

    if (sa.contains("str-literals")) {
        for (auto& j : sa["str-literals"]) {
            mod.strLiterals.push_back({j["label"], j["value"]});
        }
    }

    for (auto& g : sa.value("globals", json::array()))
        mod.globals.push_back({g["label"], g.value("export", false)});

    if (sa.contains("functions")) {
        for (auto& jf : sa["functions"]) {
            PlnFunc pf = deserializeSig(jf);
            if (jf.contains("ret-type")) {
                pf.hasRetType = true;
                pf.retType    = toVRegType(jf["ret-type"]);
            }
            // Struct returns written through hidden pointers come ahead of the parameters.
            vector<VarDef> hidden;
            if (jf.contains("ret-slot")) {
                const json& rs = jf["ret-slot"];
                for (auto& eb : rs["eightbytes"])
                    pf.retSlotEightbytes.push_back(toEightbyte(eb));
                if (pf.retSlotEightbytes.empty()) {
                    pf.retVarName = rs["name"];
                    hidden.push_back({rs["name"], VRegType::Ptr64});
                } else {
                    pf.retSlot = rs["name"];
                }
            } else if (jf.contains("rets") && jf["rets"].size() == 1) {
                pf.retVarName = jf["rets"][0]["name"];
            } else if (jf.contains("rets") && jf["rets"].size() >= 2) {
                for (auto& r : jf["rets"]) {
                    if (r.value("slot", false))
                        hidden.push_back({r["name"], VRegType::Ptr64});
                    else
                        pf.retVars.push_back({r["name"], toVRegType(r["var-type"])});
                }
            }
            pf.params.insert(pf.params.begin(), hidden.begin(), hidden.end());
            pf.isExport = jf.value("export", false);
            pf.body = deserializeStatements(jf["body"]);
            mod.functions.push_back(move(pf));
        }
    }

    // The top-level statements are the body of the module's exported
    // `<module>..main`; the program's _start is a separate link-time stub.
    PlnFunc top = deserializeSig(sa["main"]);
    top.isExport = true;
    if (sa.contains("statements"))
        top.body = deserializeStatements(sa["statements"]);
    mod.functions.push_back(move(top));

    return mod;
} // LCOV_EXCL_BR_LINE
