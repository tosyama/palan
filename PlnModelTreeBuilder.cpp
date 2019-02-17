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
#include "models/PlnObjectLiteral.h"
#include "models/PlnLoopStatement.h"
#include "models/PlnConditionalBranch.h"
#include "models/expressions/PlnAssignment.h"
#include "models/expressions/PlnFunctionCall.h"
#include "models/expressions/PlnAddOperation.h"
#include "models/expressions/PlnMulOperation.h"
#include "models/expressions/PlnDivOperation.h"
#include "models/expressions/PlnBoolOperation.h"
#include "models/expressions/PlnArrayItem.h"
#include "models/types/PlnFixedArrayType.h"

static void registerPrototype(json& proto, PlnScopeStack& scope);
static void buildFunction(json& func, PlnScopeStack &scope, json& ast);
static PlnStatement* buildStatement(json& stmt, PlnScopeStack &scope, json& ast);
static PlnBlock* buildBlock(json& stmts, PlnScopeStack &scope, json& ast);
static PlnExpression* buildExpression(json& exp, PlnScopeStack &scope);
static PlnVarInit* buildVarInit(json& var_init, PlnScopeStack &scope);
static void registerConst(json& cnst, PlnScopeStack &scope);
static PlnStatement* buildReturn(json& ret, PlnScopeStack& scope);
static PlnStatement* buildWhile(json& whl, PlnScopeStack& scope, json& ast);
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
		module->toplevel = buildBlock(stmts, scope, ast);
	}

	return module;
}

static vector<PlnType*> getVarType(json& var_type, PlnScopeStack& scope)
{
	PlnModule &module = *CUR_MODULE;
	vector<PlnType*> ret_vt;
	if (var_type.is_null()) return ret_vt;

	assertAST(var_type.is_array(), var_type);
	assertAST(var_type[0]["name"].is_string(), var_type);

	string type_name = var_type[0]["name"];
	ret_vt.push_back(module.getType(type_name));

	for (int i=var_type.size()-1; i>0; --i) {
		json &vt = var_type[i];
		type_name = vt["name"];

		assertAST(type_name == "[]", vt);
		assertAST(vt["sizes"].is_array(), vt);
		
		vector<int> sizes;
		for (json& i: vt["sizes"]) {
			PlnExpression* e = buildExpression(i, scope);

			int vtype = e->values[0].type;
			if (e->type == ET_VALUE && (vtype == VL_LIT_INT8 || vtype == VL_LIT_INT8)) {
				sizes.push_back(e->values[0].inf.uintValue);
			} else {
				BOOST_ASSERT(false);
			}
		}
		PlnType* arr_t = module.getFixedArrayType(ret_vt.back(), ret_vt, sizes);
		ret_vt.push_back(arr_t);
	}
	return ret_vt;
}

void registerPrototype(json& proto, PlnScopeStack& scope)
{
	int f_type;
	string ftype_str = proto["func-type"];
	PlnFunction *f;
	PlnModule &module = *CUR_MODULE;

	if (ftype_str == "palan") {
		f = new PlnFunction(FT_PLN, proto["name"]);
		for (auto& param: proto["params"]) {
			vector<PlnType*> var_type2 = getVarType(param["var-type"], scope);
			PlnPassingMethod pm = FPM_COPY;
			if (param["move"].is_boolean()) {
				if (param["move"] == true)
					pm = FPM_MOVEOWNER;
			}
			PlnExpression* default_val = NULL;
			if (param["default-val"].is_object()) {
				default_val = buildExpression(param["default-val"], scope);
			}
			f->addParam(param["name"], var_type2.size() ? var_type2.back() : NULL, pm, default_val);
			setLoc(f->parameters.back(), param);
		}

		for (auto& ret: proto["rets"]) {
			vector<PlnType*> var_type = getVarType(ret["var-type"], scope);
			string name;
			if (ret["name"].is_string())
				name = ret["name"];

			try {
				f->addRetValue(name, var_type.size() ? var_type.back() : NULL, true);

			} catch (PlnCompileError& err) {
				if (err.loc.fid == -1)
					setLoc(&err, ret);
				throw err;
			}
			setLoc(f->return_vals.back(), ret);
		}
		setLoc(f, proto);

	} else if (ftype_str == "ccall") {
		f = new PlnFunction(FT_C, proto["name"]);
		if (proto["ret-type"].is_string()) {
			if (PlnType *t = module.getType(proto["ret-type"])) {
				string rname = "";
				f->addRetValue(rname, t, false);
			}
		}
		setLoc(f, proto);

	} else if (ftype_str == "syscall") {
		f = new PlnFunction(FT_SYS, proto["name"]);
		f->inf.syscall.id = proto["call-id"];
		if (proto["ret-type"].is_string()) {
			if(PlnType *t = module.getType(proto["ret-type"])) {
				string rname = "";
				f->addRetValue(rname, t, false);
			}
		}
		setLoc(f, proto);

	} else
		assertAST(false, proto);
	
	vector<string> param_types;
	for (auto p: f->parameters) {
		if (p->ptr_type == PTR_PARAM_MOVE) {
			param_types.push_back(p->var_type->name + ">>");
		} else {
			param_types.push_back(p->var_type->name);
		}
	}

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
		vector<PlnType*> var_type = getVarType(param["var-type"], scope);
		if (var_type.size()) {
			param_types.push_back(var_type.back()->name);
			pre_name = param_types.back();
			if (param["move"].is_boolean() && param["move"] == true) {
				param_types.back() = pre_name + ">>";
			}
		} else {
			param_types.push_back(pre_name);
		}
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


PlnBlock* buildBlock(json& stmts, PlnScopeStack &scope, json& ast)
{
	PlnBlock* block = new PlnBlock();
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

	} else if (type == "if") {
		statement = buildIf(stmt, scope, ast);

	} else if (type == "func-def") {
		json& f = getFuncDef(ast, stmt["id"]);
		if (f["func-type"] == "palan")
			buildFunction(f, scope, ast);

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

static PlnType* getDefaultType(PlnValue &val, PlnModule *module)
{
	if (val.type == VL_VAR)
		return val.inf.var->var_type;
	else if (val.type == VL_LIT_INT8)
		return PlnType::getSint();
	else if (val.type == VL_LIT_UINT8)
		return PlnType::getUint();
	else if (val.type == VL_LIT_FLO8)
		return PlnType::getFlo();
	else if (val.type == VL_WORK)
		return val.inf.wk_type;
	else if (val.type == VL_LIT_STR)
		return PlnType::getReadOnlyCStr();
	else if (val.type == VL_LIT_ARRAY)
		return val.inf.arrValue->getDefaultType(module).back();
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

		vector<PlnType*> t2;
		PlnType* t = NULL;

		if (infer == TYPE_INFER) {
			t = getDefaultType(inits[init_ex_ind]->values[init_val_ind], CUR_MODULE);
		} else if (infer == ARR_INDEX_INFER) {
			vector<int> sizes;
			PlnValue &val = inits[init_ex_ind]->values[init_val_ind];
			if (val.type == VL_LIT_ARRAY) {
				sizes = val.inf.arrValue->getArraySizes();
			} else {
				PlnType* t = getDefaultType(val, CUR_MODULE);
				while (t->type == TP_FIXED_ARRAY) {
					PlnFixedArrayType* atype = static_cast<PlnFixedArrayType*>(t);
					for (int sz: *atype->inf.fixedarray.sizes) {
						sizes.push_back(sz);	
					}
					t = atype->item_type;
				}
			}
			inferArrayIndex(var, sizes);
			t2 = getVarType(var["var-type"], scope);
			if (t2.size())
				t = t2.back();

		} else {
			t2 = getVarType(var["var-type"], scope);
			if (t2.size())
				t = t2.back();
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
	PlnBlock* block = buildBlock(whl["block"]["stmts"], scope, ast);
	setLoc(block, whl["block"]);
	return new PlnWhileStatement(cond, block, CUR_BLOCK);
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

		// *** Temporaly for getFunc
		if (e->type == ET_VALUE) {
			vector<PlnType*> types = { getDefaultType(e->values[0], CUR_MODULE) };
			e = e->adjustTypes(types);
		}
		for (PlnValue& val: e->values)
			arg_vals.push_back(&val);
		args.push_back(e);
	}

	try {
		PlnFunction* f = CUR_BLOCK->getFunc(fcall["func-name"], arg_vals);

		// Set default value and adjusting type.
		vector<PlnType*> types;
		int arg_ex_ind = 0;
		int arg_val_ind = 0;
		for (int i=0; i<f->parameters.size(); i++) {
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

		return new PlnFunctionCall(f, args);

	} catch (PlnCompileError& err) {
		setLoc(&err, fcall);
		throw err;
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
		throw err;
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

	try {
		PlnFunction* f = CUR_BLOCK->getFunc(ccall["func-name"], arg_vals);
		return new PlnFunctionCall(f, args);

	} catch (PlnCompileError& err) {
		setLoc(&err, ccall);
		throw err;
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
		vector<PlnObjectLiteralItem> items;
		for (PlnExpression *exp: exps) {
			BOOST_ASSERT(exp->type == ET_VALUE);
			PlnValue v = exp->values[0];
			switch (v.type) {
			case VL_LIT_INT8:
				items.push_back(v.inf.intValue);
				delete exp;
				break;
			case VL_LIT_UINT8:
				items.push_back(v.inf.uintValue);
				delete exp;
				break;
			case VL_LIT_FLO8:
				items.push_back(v.inf.floValue);
				delete exp;
				break;
			case VL_LIT_ARRAY:
				items.push_back(v.inf.arrValue);
				v.inf.arrValue = NULL;
				delete exp;
				break;
			default:
				BOOST_ASSERT(false);
			}
		}

		auto arr_lit = new PlnArrayLiteral(items); 
		return new PlnExpression(arr_lit);
	} else { // not literal
		PlnCompileError err(E_CantUseDynamicValue, PlnMessage::arrayValue());
		setLoc(&err, arrval);
		throw err;
	}

	BOOST_ASSERT(false);

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
					throw err;
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
		throw err;
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

