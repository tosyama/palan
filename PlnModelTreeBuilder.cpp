/// Build model trees from AST json definision.
///
/// @file	PlnModelTreeBuilder.cpp
/// @copyright	2018-2019 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include "PlnModel.h"
#include "PlnModelTreeBuilder.h"
#include "PlnScopeStack.h"
#include "PlnConstants.h"
#include "PlnMessage.h"
#include "PlnException.h"
#include "models/PlnType.h"
#include "models/PlnModule.h"
#include "models/PlnFunction.h"
#include "models/PlnBlock.h"
#include "models/PlnStatement.h"
#include "models/PlnExpression.h"
#include "models/PlnVariable.h"
#include "models/PlnLoopStatement.h"
#include "models/PlnConditionalBranch.h"
#include "models/expressions/PlnAssignment.h"
#include "models/expressions/PlnFunctionCall.h"
#include "models/expressions/PlnAddOperation.h"
#include "models/expressions/PlnMulOperation.h"
#include "models/expressions/PlnDivOperation.h"
#include "models/expressions/PlnBoolOperation.h"
#include "models/expressions/PlnArrayItem.h"
#include "models/expressions/PlnArrayValue.h"
#include "models/types/PlnFixedArrayType.h"
#include "models/types/PlnArrayValueType.h"
#include "models/types/PlnStructType.h"

static void registerPrototype(json& proto, PlnScopeStack& scope);
static void buildFunction(json& func, PlnScopeStack &scope, json& ast);
static PlnStatement* buildStatement(json& stmt, PlnScopeStack &scope, json& ast);
static PlnBlock* buildBlock(json& stmts, PlnScopeStack &scope, json& ast, PlnBlock* new_block = NULL);
static PlnExpression* buildExpression(json& exp, PlnScopeStack &scope);
static PlnVarInit* buildVarInit(json& var_init, PlnScopeStack &scope);
static void registerConst(json& cnst, PlnScopeStack &scope);
static void registerType(json& type, PlnScopeStack &scope);
static PlnStatement* buildReturn(json& ret, PlnScopeStack& scope);
static PlnStatement* buildWhile(json& whl, PlnScopeStack& scope, json& ast);
static PlnStatement* buildBreak(json& brk, PlnScopeStack& scope, json& ast);
static PlnStatement* buildContinue(json& cntn, PlnScopeStack& scope, json& ast);
static PlnStatement* buildIf(json& ifels, PlnScopeStack& scope, json& ast);
static PlnExpression* buildArrayValue(json& arrval, PlnScopeStack& scope);
static PlnExpression* buildVariarble(json var, PlnScopeStack &scope);
static PlnExpression* buildFuncCall(json& fcall, PlnScopeStack &scope);
static PlnExpression* buildAssignment(json& asgn, PlnScopeStack &scope);
static PlnExpression* buildChainCall(json& ccall, PlnScopeStack &scope);
static PlnExpression* buildAddOperation(json& add, PlnScopeStack &scope);
static PlnExpression* buildSubOperation(json& sub, PlnScopeStack &scope);
static PlnExpression* buildMulOperation(json& mul, PlnScopeStack &scope);
static PlnExpression* buildDivOperation(json& div, PlnScopeStack &scope);
static PlnExpression* buildModOperation(json& mod, PlnScopeStack &scope);
static PlnExpression* buildNegativeOperation(json& neg, PlnScopeStack &scope);
static PlnExpression* buildCmpOperation(json& cmp, PlnCmpType type, PlnScopeStack &scope);
static PlnExpression* buildBoolOperation(json& bl, PlnExprsnType type, PlnScopeStack &scope);
static PlnExpression* buildDstValue(json dval, PlnScopeStack &scope);

#define CUR_BLOCK	scope.back().inf.block
#define CUR_MODULE	scope.front().inf.module
#define CUR_FUNC	(CUR_BLOCK->parent_func)

#define throw_AST_err(j)	{ PlnCompileError err(E_InvalidAST, __FILE__, to_string(__LINE__)); setLoc(&err, j); throw err; }
#define assertAST(check,j)	{ if (!(check)) throw_AST_err(j); }

PlnModelTreeBuilder::PlnModelTreeBuilder()
{
}

template <typename T>
void setLoc(T obj, json& j)
{
	if (j["loc"].is_array()) {
		json& l = j["loc"];
		obj->loc.fid = l[0];
		obj->loc.begin_line = l[1];
		obj->loc.begin_col = l[2];
		obj->loc.end_line = l[3];
		obj->loc.end_col = l[4];
	}
}

PlnModule* PlnModelTreeBuilder::buildModule(json& ast)
{
	assertAST(!ast.is_null(), ast);
	json &stmts = ast["stmts"];
	assertAST(stmts.is_array() || stmts.is_null(), ast);

	PlnModule *module = new PlnModule();
	PlnScopeStack scope;
	scope.push_back(module);

	if (stmts.is_array()) {
		buildBlock(stmts, scope, ast, module->toplevel);
	}

	return module;
}

static PlnType* getVarType(json& var_type, PlnScopeStack& scope)
{
	if (var_type.is_null()) return NULL;

	PlnModule &module = *CUR_MODULE;
	PlnType* ret_vt;

	assertAST(var_type.is_array(), var_type);
	assertAST(var_type[0]["name"].is_string(), var_type);

	string type_name = var_type[0]["name"];
	ret_vt = CUR_BLOCK->getType(type_name);
	if (!ret_vt) {
		PlnCompileError err(E_UndefinedType, type_name);
		setLoc(&err, var_type[0]);
		throw err;
	}

	for (int i=var_type.size()-1; i>0; --i) {
		json &vt = var_type[i];
		type_name = vt["name"];

		assertAST(type_name == "[]", vt);
		assertAST(vt["sizes"].is_array(), vt);

		vector<int> sizes;
		for (json& i: vt["sizes"]) {
			PlnExpression* e = buildExpression(i, scope);

			int vtype = e->values[0].type;
			if (e->type == ET_VALUE && (vtype == VL_LIT_INT8 || vtype == VL_LIT_UINT8)) {
				sizes.push_back(e->values[0].inf.uintValue);
			} else {
				BOOST_ASSERT(false);
			}
		}
		PlnType* arr_t = CUR_BLOCK->getFixedArrayType(ret_vt, sizes);
		ret_vt = arr_t;
	}

	return ret_vt;
}

void registerPrototype(json& proto, PlnScopeStack& scope)
{
	int f_type;
	string ftype_str = proto["func-type"];
	PlnFunction *f;
	PlnModule &module = *CUR_MODULE;

	assertAST(ftype_str == "palan"
			|| ftype_str == "ccall"
			|| ftype_str == "syscall", proto);

	if (ftype_str == "palan") {
		f = new PlnFunction(FT_PLN, proto["name"]);
		f->parent = CUR_BLOCK;
		for (auto& param: proto["params"]) {
			if (param["name"] == "@" || param["name"] == "...") {
				PlnCompileError err(E_UnsuppotedGrammer,"Not supported placeholder or variable argument at palan function.");
				setLoc(&err, param);
				throw err;
			}

			PlnType *var_type = getVarType(param["var-type"], scope);
			assertAST(param["pass-by"] == "move" || param["pass-by"] == "copy" || param["pass-by"] == "ro-ref", param);

			PlnPassingMethod pm;
			if (param["pass-by"] == "move") {
				pm = FPM_MOVEOWNER;
			} else if (param["pass-by"] == "copy") {
				pm = FPM_COPY;
			} else if (param["pass-by"] == "ro-ref") {
				pm = FPM_REF;
			} else
				BOOST_ASSERT(false);

			PlnExpression* default_val = NULL;
			if (param["default-val"].is_object()) {
				default_val = buildExpression(param["default-val"], scope);
			}
			f->addParam(param["name"], var_type, PIO_INPUT, pm, default_val);
			setLoc(f->parameters.back(), param);
		}

		for (auto& ret: proto["rets"]) {
			PlnType *var_type = getVarType(ret["var-type"], scope);
			string name;
			if (ret["name"].is_string())
				name = ret["name"];

			try {
				f->addRetValue(name, var_type, true);

			} catch (PlnCompileError& err) {
				if (err.loc.fid == -1)
					setLoc(&err, ret);
				throw err;
			}
			setLoc(f->return_vals.back(), ret);
		}
		setLoc(f, proto);

	} else {
		if (ftype_str == "ccall") {
			f = new PlnFunction(FT_C, proto["name"]);
		} else { // if (ftype_str == "syscall")
			f = new PlnFunction(FT_SYS, proto["name"]);
			f->inf.syscall.id = proto["call-id"];
		} 
		f->parent = CUR_BLOCK;

		int i=0;
		for (auto& param: proto["params"]) {
			if (param["name"] == "@") {
				PlnCompileError err(E_NoMatchingParameter);
				setLoc(&err, param);
				throw err;

			} else if (param["name"] == "...") {
				BOOST_ASSERT((i+1)==proto["params"].size());
				int iomode = PIO_UNKNOWN;
				if (param["pass-by"] == "read") {
					iomode = PIO_INPUT;
				} else {
					assertAST(param["pass-by"] == "write", param);
					iomode = PIO_OUTPUT;
				}

				f->addParam(param["name"], PlnType::getAny(), iomode, FPM_COPY, NULL);

			} else {
				PlnType *var_type = getVarType(param["var-type"], scope);
				PlnPassingMethod pm = FPM_COPY;
				int iomode = PIO_UNKNOWN;
				if (param["pass-by"] == "move") {
					pm = FPM_MOVEOWNER;
					iomode = PIO_INPUT;

				} else if (param["pass-by"] == "ro-ref") {
					pm = FPM_REF;
					iomode = PIO_INPUT;

				} else if (param["pass-by"] == "write") {
					pm = FPM_REF;
					iomode = PIO_OUTPUT;

				} else {
					assertAST(param["pass-by"]=="copy", param);
					iomode = PIO_INPUT;
				}
				f->addParam(param["name"], var_type, iomode, pm, NULL);
			}
			i++;
		}
		if (proto["ret"].is_array()) {
			PlnType *t = getVarType(proto["ret"], scope);
			string rname = "";
			f->addRetValue(rname, t, false);
		}
		setLoc(f, proto);
	}
	
	vector<string> param_types = f->getParamStrs();

	if (CUR_BLOCK->getFuncProto(f->name, param_types)) {
		PlnCompileError err(E_DuplicateFunction, f->name);
		setLoc(&err, proto);
		throw err;
	}

	CUR_BLOCK->funcs.push_back(f);
	module.functions.push_back(f);
}

void buildFunction(json& func, PlnScopeStack &scope, json& ast)
{
	assertAST(func["name"].is_string(), func);
	assertAST(func["params"].is_array(), func);
	assertAST(func["rets"].is_array(), func);
	assertAST(func["impl"].is_object(), func);
	assertAST(func["impl"]["stmts"].is_array(), func);

	string pre_name;
	vector<string> param_types;
	for (auto& param: func["params"]) {
		BOOST_ASSERT (param["name"]!="...");
		PlnType* var_type = getVarType(param["var-type"], scope);
		string p_name;
		if (var_type) {
			pre_name = var_type->name;
		}
		p_name = pre_name;

		if (param["pass-by"] == "move") {
			p_name += ">>";
		}

		param_types.push_back(p_name);
	}

	PlnFunction* f = CUR_BLOCK->getFuncProto(func["name"], param_types);
	assertAST(f, func);
	assertAST(f->return_vals.size() == func["rets"].size(), func);
	setLoc(f, func);

	f->parent = CUR_BLOCK;
	scope.push_back(f);
	f->implement = buildBlock(func["impl"]["stmts"], scope, ast);
	setLoc(f->implement, func["impl"]);

	// free memory
	func["impl"].clear();

	scope.pop_back();
}

static json& getFuncDef(json& ast, int id)
{
	assertAST(ast["funcs"].is_array(), ast);
	json& funcs = ast["funcs"];
	for (auto& f: funcs) {
		assertAST(f.is_object(), f);
		assertAST(f["id"].is_number_integer(), f);
		if (f["id"] == id) {
			return f;
		}
	}
	BOOST_ASSERT(false);
}

static void prebuildBlock(json& stmts, PlnScopeStack& scope, json& ast)
{
	// Register const
	for (json& stmt: stmts) {
		assertAST(stmt["stmt-type"].is_string(),stmt);
		string type = stmt["stmt-type"];
		if (type == "const") {
			registerConst(stmt, scope);
		} else if (type == "type-def") {
			registerType(stmt, scope);
		}
	}

	// Register function prototype
	for (json& stmt: stmts) {
		string type = stmt["stmt-type"];
		if (type == "func-def") {
			json& f = getFuncDef(ast, stmt["id"]);
			registerPrototype(f, scope);
		}
	}
}


PlnBlock* buildBlock(json& stmts, PlnScopeStack &scope, json& ast, PlnBlock* new_block)
{
	PlnBlock* block = new_block ? new_block : new PlnBlock();
	switch (scope.back().type) {
		case SC_MODULE: break;
		case SC_FUNCTION:
			block->setParent(scope.back().inf.function);
			break;
		case SC_BLOCK:
			block->setParent(CUR_BLOCK);
			break;
	}

	scope.push_back(block);
	prebuildBlock(stmts, scope, ast);
	for (json& stmt: stmts) {
		if (PlnStatement* s = buildStatement(stmt, scope, ast))
			block->statements.push_back(s);
	}
	scope.pop_back();
	return block;
}

PlnStatement* buildStatement(json& stmt, PlnScopeStack &scope, json& ast)
{
	string type = stmt["stmt-type"];
	PlnStatement *statement;

	if (type == "exp") {
		statement = new PlnStatement(buildExpression(stmt["exp"], scope), CUR_BLOCK);

	} else if (type == "block") {
		assertAST(stmt["block"].is_object(), stmt);
		json &stmts = stmt["block"]["stmts"];
		assertAST(stmts.is_array(), stmt);
		PlnBlock *block = buildBlock(stmts, scope, ast);
		setLoc(block, stmt["block"]);
		statement = new PlnStatement(block, CUR_BLOCK);

	} else if (type == "var-init") {
		statement = new PlnStatement(buildVarInit(stmt, scope), CUR_BLOCK);
	
	} else if (type == "const") {
		return NULL;

	} else if (type == "return") {
		statement = buildReturn(stmt, scope);

	} else if (type == "while") {
		statement = buildWhile(stmt, scope, ast);
	
	} else if (type == "break") {
		statement = buildBreak(stmt, scope, ast);

	} else if (type == "continue") {
		statement = buildContinue(stmt, scope, ast);

	} else if (type == "if") {
		statement = buildIf(stmt, scope, ast);

	} else if (type == "func-def") {
		json& f = getFuncDef(ast, stmt["id"]);
		if (f["func-type"] == "palan")
			buildFunction(f, scope, ast);
		return NULL;
	
	} else if (type == "type-def") {
		return NULL;

	} else {
		assertAST(false, stmt);
	}
	setLoc(statement, stmt);

	return statement;
}

enum InferenceType {
	NO_INFER,
	TYPE_INFER,
	ARR_INDEX_INFER
};

static InferenceType checkNeedsTypeInference(json& var_type)
{
	if (var_type.is_null())
		return NO_INFER;

	if (var_type.is_array() && var_type.size() == 0)
		return TYPE_INFER;

	for (json &vt: var_type) {
		if (vt["name"] == "[]") {
			assertAST(vt["sizes"].is_array(),vt);
			for (json& sz: vt["sizes"]) {
				if (sz["exp-type"] == "lit-int" && sz["val"] == -1) {
					return ARR_INDEX_INFER;
				}
			}
		}
	}
	return NO_INFER;
}

static PlnType* getDefaultType(PlnValue &val, PlnBlock *block)
{
	if (val.type == VL_VAR)
		return val.inf.var->var_type;
	else if (val.type == VL_LIT_INT8)
		return PlnType::getSint();
	else if (val.type == VL_LIT_UINT8)
		return PlnType::getUint();
	else if (val.type == VL_LIT_FLO8)
		return PlnType::getFlo();
	else if (val.type == VL_WORK) {
		if (val.inf.wk_type->type == TP_ARRAY_VALUE) {
			return static_cast<PlnArrayValueType*>(val.inf.wk_type)->getDefaultType(block);
		} else {
			return val.inf.wk_type;
		}
	} else if (val.type == VL_LIT_STR)
		return PlnType::getReadOnlyCStr();
	else if (val.type == VL_LIT_ARRAY)
		return static_cast<PlnArrayValueType*>(val.inf.arrValue->values[0].inf.wk_type)->getDefaultType(block);
	else
		BOOST_ASSERT(false);
}

static void inferArrayIndex(json& var, vector<int> sizes)
{
	json& var_type = var["var-type"];
	
	int sz_i = 0;
	for (json &vt: var_type) {
		if (vt["name"] == "[]") {
			assertAST(vt["sizes"].is_array(),vt);
			for (json& sz: vt["sizes"]) {
				if (sz_i >= sizes.size()) {
					goto sz_err;
				}
				if (sz["exp-type"] == "lit-int" && sz["val"] == -1) {
					sz["val"] = sizes[sz_i];
				}
				sz_i++;
			}
		}
	}

	if (sz_i == sizes.size())
		return;

sz_err:
	PlnCompileError err(E_IncompatibleTypeInitVar, var["name"]);
	setLoc(&err, var);
	throw err;
}

PlnVarInit* buildVarInit(json& var_init, PlnScopeStack &scope)
{
	assertAST(var_init["vars"].is_array(), var_init);
	vector<PlnExpression*> inits;
	if (var_init["inits"].is_array())
		for (json &exp: var_init["inits"]) {
			inits.push_back(buildExpression(exp, scope));
		}

	vector<PlnValue> vars;
	vector<PlnType*> types;
	int init_ex_ind = 0;
	int init_val_ind = 0;
	for (json &var: var_init["vars"]) {
		InferenceType infer = checkNeedsTypeInference(var["var-type"]);
		if (infer != NO_INFER) {
			if (init_ex_ind >= inits.size()) {
				PlnCompileError err(E_AmbiguousVarType, var["name"]);
				setLoc(&err, var);
				throw err;
			}
		}

		PlnType* t;
		if (infer == TYPE_INFER) {
			try {
				t = getDefaultType(inits[init_ex_ind]->values[init_val_ind], CUR_BLOCK);

				// check type not support for var
				if (t == PlnType::getReadOnlyCStr()) {
					throw PlnCompileError(E_UnsuppotedGrammer, "Not supported type for variable.");
				}

			} catch (PlnCompileError &err) {
				err.loc = inits[init_ex_ind]->loc;
				throw err;
			}

		} else if (infer == ARR_INDEX_INFER) {
			vector<int> sizes;
			PlnValue &val = inits[init_ex_ind]->values[init_val_ind];

			PlnType *tt = val.getType();
			if (tt->type == TP_FIXED_ARRAY) {
				while (tt->type == TP_FIXED_ARRAY) {
					PlnFixedArrayType* atype = static_cast<PlnFixedArrayType*>(tt);
					for (int sz: atype->sizes) {
						sizes.push_back(sz);	
					}
					tt = atype->item_type;
				}
			} else if (tt->type == TP_ARRAY_VALUE) {
				sizes = static_cast<PlnArrayValueType*>(tt)->getArraySizes();
			}

			inferArrayIndex(var, sizes);
			t = getVarType(var["var-type"], scope);

		} else {
			t = getVarType(var["var-type"], scope);
		}

		PlnVariable *v = CUR_BLOCK->declareVariable(var["name"], t, true);
		if (!v) {
			PlnCompileError err(E_DuplicateVarName, var["name"]);
			setLoc(&err, var);
			throw err;
		}
		vars.push_back(v);
		types.push_back(v->var_type);
		if (var["move"].is_boolean() && var["move"] == true) {
			vars.back().asgn_type = ASGN_MOVE;
		} else {
			vars.back().asgn_type = ASGN_COPY;
		}

		setLoc(v, var);

		if (init_ex_ind < inits.size()) {
			if (init_val_ind+1 < inits[init_ex_ind]->values.size()) {
				init_val_ind++;
			} else {
				inits[init_ex_ind] = inits[init_ex_ind]->adjustTypes(types);
				init_ex_ind++;
				init_val_ind = 0;
				types.clear();
			}
		}
	}


	if (inits.size()) {
		return new PlnVarInit(vars, &inits);
	} else {
		return new PlnVarInit(vars);
	}
}

void registerConst(json& cnst, PlnScopeStack &scope)
{
	assertAST(cnst["names"].is_array(), cnst);
	assertAST(cnst["values"].is_array(), cnst);
	if (cnst["names"].size() != cnst["values"].size()) {
		PlnCompileError err(E_NumOfLRVariables);
		setLoc(&err, cnst);
		throw err;
	}

	int i = 0;
	for(json& val: cnst["values"]) {
		assertAST(val.is_object(), cnst);
		json &cname = cnst["names"][i];
		assertAST(cname.is_string(), cnst);

		PlnExpression *e;
		try {
			e = buildExpression(val, scope);

		} catch (PlnCompileError &err) {
			if (err.err_code == E_UndefinedVariable) {
				PlnCompileError cerr(E_UndefinedConst, err.arg1);
				cerr.loc = err.loc;
				throw cerr;
			}
			throw;
		}

		try {
			CUR_BLOCK->declareConst(cname, e);

		} catch (PlnCompileError &err) {
			setLoc(&err, cnst);
			throw err;
		}
		i++;
	}
}

void registerType(json& type, PlnScopeStack &scope)
{
	string type_name = type["name"];
	PlnType *cur_type = CUR_BLOCK->getType(type_name);
	if (cur_type) {
		BOOST_ASSERT(false);
	}

	// cur_type == NULL;

	if (type["type"] == "obj-ref") {
		CUR_BLOCK->declareType(type_name);

	} else if (type["type"] == "struct") {
		assertAST(type["members"].is_array(), type);

		vector<PlnStructMember*> members;
		for (auto m: type["members"]) {
			PlnType* t = getVarType(m["type"], scope);

			auto member = new PlnStructMember(t, m["name"]);
			members.push_back(member);
		}
		CUR_BLOCK->declareType(type_name, members);

	} else {
		BOOST_ASSERT(false);
	}
}

PlnStatement* buildReturn(json& ret, PlnScopeStack& scope)
{
	if (!CUR_FUNC) {
		PlnCompileError err(E_CantUseAtToplevel, "return");
		setLoc(&err, ret);
		throw err;
	}

	vector<PlnExpression *> ret_vals;
	if (ret["ret-vals"].is_array())
		for (json& ret_val: ret["ret-vals"]) {
			assertAST(ret_val.is_object(), ret);
			ret_vals.push_back(buildExpression(ret_val, scope));
		}
	
	try {
		return new PlnReturnStmt(ret_vals, CUR_BLOCK);

	} catch (PlnCompileError &err) {
		if (err.loc.fid == -1)
			setLoc(&err, ret);
		throw err;
	}
}

PlnStatement* buildWhile(json& whl, PlnScopeStack& scope, json& ast)
{
	PlnExpression* cond = buildExpression(whl["cond"], scope);
	PlnBlock* stmts_block = new PlnBlock();
	PlnWhileStatement* while_stmt = new PlnWhileStatement(cond, stmts_block, CUR_BLOCK);
	buildBlock(whl["block"]["stmts"], scope, ast, stmts_block);
	setLoc(stmts_block, whl["block"]);

	return while_stmt;
}

PlnStatement* buildBreak(json& brk, PlnScopeStack& scope, json& ast)
{
	PlnStatement* stmt = NULL;
	for (auto si=scope.rbegin(); si!=scope.rend(); ++si) {
		if (si->type == SC_BLOCK) {
			PlnStatement *s = si->inf.block->owner_stmt;
			if (s && s->type == ST_WHILE) {
				stmt = s;
				break;
			}
		} else
			break;
	}

	if (!stmt) {
		PlnCompileError err(E_NotWithInLoop);
		setLoc(&err, brk);
		throw err;
	}

	return new PlnBreakStatement(stmt);
}

PlnStatement* buildContinue(json& cntn, PlnScopeStack& scope, json& ast)
{
	PlnStatement* stmt = NULL;
	for (auto si=scope.rbegin(); si!=scope.rend(); ++si) {
		if (si->type == SC_BLOCK) {
			PlnStatement *s = si->inf.block->owner_stmt;
			if (s && s->type == ST_WHILE) {
				stmt = s;
				break;
			}
		} else
			break;
	}

	if (!stmt) {
		PlnCompileError err(E_NotWithInLoop);
		setLoc(&err, cntn);
		throw err;
	}

	return new PlnContinueStatement(stmt);
}

PlnStatement* buildIf(json& ifels, PlnScopeStack& scope, json& ast)
{
	PlnExpression* cond = buildExpression(ifels["cond"], scope);
	PlnBlock* ifblock = buildBlock(ifels["block"]["stmts"], scope, ast);
	setLoc(ifblock, ifels["block"]);
	PlnStatement* else_stmt = NULL;
	if (ifels["else"].is_object()) {
		else_stmt = buildStatement(ifels["else"], scope, ast);
	}

	return new PlnIfStatement(cond, ifblock, else_stmt, CUR_BLOCK);
}

PlnExpression* buildExpression(json& exp, PlnScopeStack &scope)
{
	assertAST(exp["exp-type"].is_string(), exp);
	string type = exp["exp-type"];
	PlnExpression *expression;

	if (type == "lit-int") {
		assertAST(exp["val"].is_number_integer(), exp);
		expression = new PlnExpression(exp["val"].get<int64_t>());
	} else if (type == "lit-uint") {
		assertAST(exp["val"].is_number_integer(), exp);
		expression = new PlnExpression(exp["val"].get<uint64_t>());
	} else if (type == "lit-float") {
		assertAST(exp["val"].is_number(), exp);
		expression = new PlnExpression(exp["val"].get<double>());
	} else if (type == "lit-str") {
		assertAST(exp["val"].is_string(), exp);
		// PlnReadOnlyData *ro = CUR_MODULE->getReadOnlyData(exp["val"]);
		expression = new PlnExpression(exp["val"].get<string>());
	} else if (type == "array-val") {
		expression = buildArrayValue(exp, scope);
	} else if (type == "var") {
		expression = buildVariarble(exp, scope);
	} else if (type == "asgn") {
		expression = buildAssignment(exp, scope);
	} else if (type == "func-call") {
		expression = buildFuncCall(exp, scope);
	} else if (type == "chain-call") {
		expression = buildChainCall(exp, scope);
	} else if (type == "+") {
		expression = buildAddOperation(exp, scope);
	} else if (type == "-") {
		expression = buildSubOperation(exp, scope);
	} else if (type == "*") {
		expression = buildMulOperation(exp, scope);
	} else if (type == "/") {
		expression = buildDivOperation(exp, scope);
	} else if (type == "%") {
		expression = buildModOperation(exp, scope);
	} else if (type == "==") {
		expression = buildCmpOperation(exp, CMP_EQ, scope);
	} else if (type == "!=") {
		expression = buildCmpOperation(exp, CMP_NE, scope);
	} else if (type == "<") {
		expression = buildCmpOperation(exp, CMP_L, scope);
	} else if (type == ">") {
		expression = buildCmpOperation(exp, CMP_G, scope);
	} else if (type == "<=") {
		expression = buildCmpOperation(exp, CMP_LE, scope);
	} else if (type == ">=") {
		expression = buildCmpOperation(exp, CMP_GE, scope);
	} else if (type == "&&") {
		expression = buildBoolOperation(exp, ET_AND, scope);
	} else if (type == "||") {
		expression = buildBoolOperation(exp, ET_OR, scope);
	} else if (type == "uminus") {
		expression = buildNegativeOperation(exp, scope);
	} else if (type == "not") {
		expression = PlnBoolOperation::getNot(buildExpression(exp["val"], scope));
	} else {
		assertAST(false, exp);
	}
	setLoc(expression, exp);
	return expression;
}

PlnExpression* buildFuncCall(json& fcall, PlnScopeStack &scope)
{
	vector<PlnExpression*> args;
	vector<PlnValue*> arg_vals;
	assertAST(fcall["func-name"].is_string(), fcall);
	assertAST(fcall["args"].is_array(), fcall);
	assertAST(fcall["out-args"].is_array(), fcall);

	for (auto& arg: fcall["args"]) {
		if (arg.is_null()) {
			// use default
			arg_vals.push_back(NULL);
			args.push_back(NULL);
			continue;
		}
		assertAST(arg["exp"].is_object(), fcall);
		PlnExpression *e = buildExpression(arg["exp"], scope);

		if (arg["move"].is_boolean() && arg["move"] == true) {
			e->values[0].asgn_type = ASGN_MOVE;
			// ??
			if (e->type == ET_VALUE) {
				if (e->values[0].type == VL_LIT_ARRAY) {
					PlnCompileError err(E_CantUseMoveOwnership, PlnMessage::arrayValue());
					err.loc = e->loc;
					throw err;
				}
			}
		}

		for (PlnValue& val: e->values)
			arg_vals.push_back(&val);
		args.push_back(e);
	}

	vector<PlnExpression*> out_args;
	vector<PlnValue*> out_arg_vals;
	for (auto& arg: fcall["out-args"]) {
		assertAST(arg["exp"].is_object(), fcall);
		PlnExpression *e = buildExpression(arg["exp"], scope);

		for (PlnValue& val: e->values)
			out_arg_vals.push_back(&val);
		out_args.push_back(e);
	}

	try {
		PlnFunction* f = CUR_BLOCK->getFunc(fcall["func-name"], arg_vals, out_arg_vals);

		// Set default value and adjusting type.
		vector<PlnType*> types;
		int arg_ex_ind = 0;
		int arg_val_ind = 0;
		for (int i=0; i<f->parameters.size(); i++) {
			if (f->parameters[i]->iomode == PIO_OUTPUT) {
				// TODO: output's adjust Types.
				continue;
			}

			if (f->parameters[i]->name == "...") {
				break;
			}

			if (arg_ex_ind == args.size()) {
				args.push_back(NULL);
			}

			if (!args[arg_ex_ind]) {
				PlnExpression* dexp = f->parameters[i]->dflt_value;
				BOOST_ASSERT(dexp && dexp->type == ET_VALUE);
				args[arg_ex_ind] = new PlnExpression(dexp->values[0]);
			}

			types.push_back(f->parameters[i]->var_type);
			if (arg_val_ind+1 < args[arg_ex_ind]->values.size()) {
				arg_val_ind++;

			} else {
				args[arg_ex_ind] = args[arg_ex_ind]->adjustTypes(types);
				arg_ex_ind++;
				arg_val_ind = 0;
				types.clear();
			}
		}

		return new PlnFunctionCall(f, args, out_args);

	} catch (PlnCompileError& err) {
		setLoc(&err, fcall);
		throw;
	}
}

PlnExpression* buildAssignment(json& asgn, PlnScopeStack &scope)
{
	json& src = asgn["src-exps"];
	assertAST(src.is_array(), asgn);

	vector<PlnExpression *> src_exps;
	for (json& exp: src) {
		src_exps.push_back(buildExpression(exp, scope));
	}

	json& dst = asgn["dst-vals"];
	assertAST(dst.is_array(), asgn);

	vector<PlnExpression*> dst_vals;
	vector<PlnType*> types;

	int src_ex_ind = 0;
	int src_val_ind = 0;
	for (json& dval: dst) {
		dst_vals.push_back(buildDstValue(dval, scope));
		BOOST_ASSERT(dst_vals.back()->values[0].type == VL_VAR);

		types.push_back(dst_vals.back()->values[0].inf.var->var_type);
		if (src_ex_ind < src_exps.size()) {
			if (src_val_ind+1 < src_exps[src_ex_ind]->values.size()) {
				src_val_ind++;
			} else {
				src_exps[src_ex_ind] = src_exps[src_ex_ind]->adjustTypes(types);
				src_ex_ind++;
				src_val_ind = 0;
				types.clear();
			}
		}
	}

	try {
		return new PlnAssignment(dst_vals, src_exps);

	} catch (PlnCompileError &err) {
		if (err.loc.fid == -1)
			setLoc(&err, asgn);
		throw;
	}
}

PlnExpression* buildChainCall(json& ccall, PlnScopeStack &scope)
{
	assertAST(ccall["func-name"].is_string(), ccall);
	const char *anames[] = { "in-args", "args" };

	vector<PlnExpression*> args;
	vector<PlnValue*> arg_vals;
	for (auto aname: anames) {
		assertAST(ccall[aname].is_array(), ccall);
		for (auto& arg: ccall[aname]) {
			assertAST(arg["exp"].is_object(), ccall);
			args.push_back(buildExpression(arg["exp"], scope));
			if (arg["move"].is_boolean() && arg["move"] == true) {
				args.back()->values[0].asgn_type = ASGN_MOVE;
			}
			vector<PlnValue> &vals = args.back()->values;
			for (int i=0; i<vals.size(); i++)
				arg_vals.push_back(&vals[i]);
		}
	}

	vector<PlnExpression*> out_args;
	vector<PlnValue*> out_arg_vals;
	for (auto& arg: ccall["out-args"]) {
		assertAST(arg["exp"].is_object(), ccall);
		PlnExpression *e = buildExpression(arg["exp"], scope);

		for (PlnValue& val: e->values)
			out_arg_vals.push_back(&val);
		out_args.push_back(e);
	}

	try {
		PlnFunction* f = CUR_BLOCK->getFunc(ccall["func-name"], arg_vals, out_arg_vals);

		// Set default value and adjusting type.
		vector<PlnType*> types;
		int arg_ex_ind = 0;
		int arg_val_ind = 0;
		for (int i=0; i<f->parameters.size(); i++) {
			if (f->parameters[i]->iomode == PIO_OUTPUT) {
				// TODO: output's adjust Types.
				continue;
			}

			if (f->parameters[i]->name == "...") {
				break;
			}

			if (arg_ex_ind == args.size()) {
				args.push_back(NULL);
			}

			if (!args[arg_ex_ind]) {
				PlnExpression* dexp = f->parameters[i]->dflt_value;
				BOOST_ASSERT(dexp && dexp->type == ET_VALUE);
				args[arg_ex_ind] = new PlnExpression(dexp->values[0]);
			}

			types.push_back(f->parameters[i]->var_type);
			if (arg_val_ind+1 < args[arg_ex_ind]->values.size()) {
				arg_val_ind++;

			} else {
				args[arg_ex_ind] = args[arg_ex_ind]->adjustTypes(types);
				arg_ex_ind++;
				arg_val_ind = 0;
				types.clear();
			}
		}

		return new PlnFunctionCall(f, args, out_args);

	} catch (PlnCompileError& err) {
		setLoc(&err, ccall);
		throw;
	}
}

PlnExpression* buildArrayValue(json& arrval, PlnScopeStack& scope)
{
	assertAST(arrval["vals"].is_array(), arrval);
	vector<PlnExpression*> exps;
	bool isLiteral = true;
	for (auto v: arrval["vals"]) {
		assertAST(v.is_object(), arrval);
		auto exp = buildExpression(v, scope);
		exps.push_back(exp);
		if (exp->type == ET_VALUE) {
			int type = exp->values[0].type;
			if (type != VL_LIT_INT8 && type != VL_LIT_UINT8 && type != VL_LIT_FLO8
				&& type != VL_LIT_STR && type != VL_LIT_ARRAY) {
				isLiteral = false;
			}
		} else
			isLiteral = false;
	}

	if (isLiteral) {
		auto arr_val = new PlnArrayValue(exps, true); 
		auto exp = new PlnExpression(arr_val);
		setLoc(exp->values[0].inf.arrValue, arrval);
		return exp;
	} else { // not literal
		return new PlnArrayValue(exps, false);
	}
}

PlnExpression* buildVariarble(json var, PlnScopeStack &scope)
{
	assertAST(var["base-var"].is_string(), var);
	PlnVariable* pvar = CUR_BLOCK->getVariable(var["base-var"]);
	if (!pvar) {
		// check constant value
		PlnExpression* cnst_val = CUR_BLOCK->getConst(var["base-var"]);
		if (cnst_val) {
			if (var["opes"].is_null()) {
				return cnst_val;
			} else {
				PlnCompileError err(E_CantUseOperatorHere, var["base-var"]);
				setLoc(&err, var);
				throw err;
			}
	
		} else {
			PlnCompileError err(E_UndefinedVariable, var["base-var"]);
			setLoc(&err, var);
			throw err;
		}
	}

	PlnExpression *var_exp = new PlnExpression(pvar);
	setLoc(var_exp, var);

	assertAST(var["opes"].is_array() || var["opes"].is_null(), var);
	if (var["opes"].is_array()) {
		for (json& ope: var["opes"]) {
			assertAST(ope["ope-type"] == "index", ope);
			if (ope["ope-type"]=="index") {
				assertAST(ope["indexes"].is_array(), var);
				vector<PlnExpression*> indexes;
				for (json& exp: ope["indexes"])
					indexes.push_back(buildExpression(exp, scope));
				try {
					var_exp = new PlnArrayItem(var_exp, indexes);
				} catch (PlnCompileError& err) {
					setLoc(&err, var);
					throw;
				}
			}
		}
	}
	return var_exp;
}

PlnExpression* buildDstValue(json dval, PlnScopeStack &scope)
{
	PlnExpression* var_exp = buildVariarble(dval, scope);

	if (var_exp->values[0].type != VL_VAR) {
		PlnCompileError err(E_CantUseConstHere);
		setLoc(&err, dval);
		throw err;
	}

	if (dval["move"].is_boolean() && dval["move"]==true) {
		var_exp->values[0].asgn_type = ASGN_MOVE;
	} else {
		var_exp->values[0].asgn_type = ASGN_COPY;
	}
	return var_exp;
}

struct BinaryEx {
	PlnExpression *l, *r;
};

static BinaryEx getBiEx(json& biope, PlnScopeStack &scope)
{
	BinaryEx bex;
	assertAST(biope["lval"].is_object(), biope);
	assertAST(biope["rval"].is_object(), biope);
	bex.l = buildExpression(biope["lval"], scope);
	bex.r = buildExpression(biope["rval"], scope);
	return bex;
}

PlnExpression* buildAddOperation(json& add, PlnScopeStack &scope)
{
	BinaryEx bex = getBiEx(add, scope);
	return PlnAddOperation::create(bex.l, bex.r);
}

PlnExpression* buildSubOperation(json& sub, PlnScopeStack &scope)
{
	BinaryEx bex = getBiEx(sub, scope);
	return PlnAddOperation::create_sub(bex.l, bex.r);
}

PlnExpression* buildMulOperation(json& mul, PlnScopeStack &scope)
{
	BinaryEx bex = getBiEx(mul, scope);
	return PlnMulOperation::create(bex.l, bex.r);
}

PlnExpression* buildDivOperation(json& div, PlnScopeStack &scope)
{
	BinaryEx bex = getBiEx(div, scope);
	return PlnDivOperation::create(bex.l, bex.r);
}

PlnExpression* buildModOperation(json& mod, PlnScopeStack &scope)
{
	BinaryEx bex = getBiEx(mod, scope);
	try {
		return PlnDivOperation::create_mod(bex.l, bex.r);
	} catch (PlnCompileError& err) {
		setLoc(&err, mod);
		throw;
	}
}

PlnExpression* buildNegativeOperation(json& neg, PlnScopeStack &scope)
{
	assertAST(neg["val"].is_object(), neg);
	PlnExpression *e = buildExpression(neg["val"], scope);
	return PlnNegative::create(e);
}

PlnExpression* buildCmpOperation(json& cmp, PlnCmpType type, PlnScopeStack &scope)
{
	BinaryEx bex = getBiEx(cmp, scope);
	return new PlnCmpOperation(bex.l, bex.r, type);
}

PlnExpression* buildBoolOperation(json& bl, PlnExprsnType type, PlnScopeStack &scope)
{
	BinaryEx bex = getBiEx(bl, scope);
	return new PlnBoolOperation(bex.l, bex.r, type);
}

