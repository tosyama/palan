/// Build model trees from AST json definision.
///
/// @file	PlnModelTreeBuilder.cpp
/// @copyright	2018 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
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
#include "models/expressions/PlnArrayValue.h"
#include "models/expressions/PlnAssignment.h"
#include "models/expressions/PlnFunctionCall.h"
#include "models/expressions/PlnAddOperation.h"
#include "models/expressions/PlnMulOperation.h"
#include "models/expressions/PlnDivOperation.h"
#include "models/expressions/PlnBoolOperation.h"
#include "models/expressions/PlnArrayItem.h"

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

	PlnModule *module = new PlnModule();
	PlnScopeStack scope;
	scope.push_back(module);

	if (ast["stmts"].is_array()) {
		module->toplevel = buildBlock(ast["stmts"], scope, ast);
	}

	return module;
}

static vector<PlnType*> getVarType(json& var_type, PlnScopeStack& scope)
{
	PlnModule &module = *CUR_MODULE;
	vector<PlnType*> ret_vt;
	if (var_type.is_null()) return ret_vt;

	assertAST(var_type.is_array(), var_type);
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
		PlnType* arr_t = module.getFixedArrayType(ret_vt, sizes);
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
			vector<PlnType*> var_type = getVarType(param["var-type"], scope);
			PlnPassingMethod pm = FPM_COPY;
			if (param["move"].is_boolean()) {
				if (param["move"] == true)
					pm = FPM_MOVEOWNER;
			}
			f->addParam(param["name"], var_type, pm, NULL);
			setLoc(f->parameters.back(), param);
		}

		for (auto& ret: proto["rets"]) {
			vector<PlnType*> var_type = getVarType(ret["var-type"], scope);
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

	} else if (ftype_str == "ccall") {
		f = new PlnFunction(FT_C, proto["name"]);
		if (proto["ret-type"].is_string()) {
			if (PlnType *t = module.getType(proto["ret-type"])) {
				string rname = "";
				vector<PlnType*> tv = { t };
				f->addRetValue(rname, tv, false);
			}
		}
		setLoc(f, proto);

	} else if (ftype_str == "syscall") {
		f = new PlnFunction(FT_SYS, proto["name"]);
		f->inf.syscall.id = proto["call-id"];
		if (proto["ret-type"].is_string()) {
			if(PlnType *t = module.getType(proto["ret-type"])) {
				string rname = "";
				vector<PlnType*> tv = { t };
				f->addRetValue(rname, tv, false);
			}
		}
		setLoc(f, proto);

	} else
		assertAST(false, proto);
	
	vector<string> param_types;
	for (auto p: f->parameters) {
		if (p->ptr_type == PTR_PARAM_MOVE) {
			param_types.push_back(p->var_type.back()->name + ">>");
		} else {
			param_types.push_back(p->var_type.back()->name);
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
	json& funcs = ast["funcs"];
	for (auto& f: funcs) {
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
		PlnBlock *block = buildBlock(stmt["block"]["stmts"], scope, ast);
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

PlnVarInit* buildVarInit(json& var_init, PlnScopeStack &scope)
{
	assertAST(var_init["vars"].is_array(), var_init);
	vector<PlnValue> vars;
	for (json &var: var_init["vars"]) {
		vector<PlnType*> t = getVarType(var["var-type"], scope);
		PlnVariable *v = CUR_BLOCK->declareVariable(var["name"], t, true);
		if (!v) {
			PlnCompileError err(E_DuplicateVarName, var["name"]);
			setLoc(&err, var);
			throw err;
		}
		vars.push_back(v);
		if (var["move"].is_boolean() && var["move"] == true) {
			vars.back().asgn_type = ASGN_MOVE;
		} else {
			vars.back().asgn_type = ASGN_COPY;
		}
		setLoc(v, var);
	}

	vector<PlnExpression*> inits;
	if (var_init["inits"].is_array())
		for (json &exp: var_init["inits"]) {
			inits.push_back(buildExpression(exp, scope));
		}

	if (inits.size())
		return new PlnVarInit(vars, &inits);
	else
		return new PlnVarInit(vars);
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
		PlnExpression *e = buildExpression(val, scope);
		PlnValue value = e->values[0];
		delete e;

		int vtype = value.type;
		const string& name = cnst["names"][i];
		if (e->type == ET_VALUE
				&& (vtype == VL_LIT_INT8 || vtype == VL_LIT_INT8 || vtype == VL_RO_DATA || vtype == VL_LIT_FLO8)) {
			if (!CUR_BLOCK->declareConst(name, value)) {
				PlnCompileError err(E_DuplicateConstName, name);
				setLoc(&err, cnst);
				throw err;
			}
		} else {
			PlnCompileError err(E_CantDefineConst, name);
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
		for (json& ret_val: ret["ret-vals"])
			ret_vals.push_back(buildExpression(ret_val, scope));
	
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
		expression = new PlnExpression(exp["val"].get<int64_t>());
	} else if (type == "lit-uint") {
		expression = new PlnExpression(exp["val"].get<uint64_t>());
	} else if (type == "lit-float") {
		expression = new PlnExpression(exp["val"].get<double>());
	} else if (type == "lit-str") {
		PlnReadOnlyData *ro = CUR_MODULE->getReadOnlyData(exp["val"]);
		expression = new PlnExpression(ro);
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
	for (auto& arg: fcall["args"]) {
		args.push_back(buildExpression(arg["exp"], scope));
		if (arg["move"].is_boolean() && arg["move"] == true) {
			args.back()->values[0].asgn_type = ASGN_MOVE;
		}
		arg_vals.push_back(&args.back()->values[0]);
	}

	try {
		PlnFunction* f = CUR_BLOCK->getFunc(fcall["func-name"], arg_vals);
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

	vector<PlnExpression *> dst_vals;
	for (json& dval: dst) {
		dst_vals.push_back(buildDstValue(dval, scope));
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
	const char *anames[] = { "in-args", "args" };

	vector<PlnExpression*> args;
	vector<PlnValue*> arg_vals;
	for (auto aname: anames) {
		for (auto& arg: ccall[aname]) {
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
	vector<PlnExpression*> exps;
	for (auto v: arrval["vals"]) {
		exps.push_back(buildExpression(v, scope));
	}
	return new PlnArrayValue(exps);
}

PlnExpression* buildVariarble(json var, PlnScopeStack &scope)
{
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

	if (var["opes"].is_array()) {
		for (json& ope: var["opes"]) {
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

	if (dval["move"].is_boolean() && dval["move"]==true) {
		var_exp->values[0].asgn_type = ASGN_MOVE;
	} else {
		var_exp->values[0].asgn_type = ASGN_COPY;
	}
	return var_exp;
}

PlnExpression* buildAddOperation(json& add, PlnScopeStack &scope)
{
	PlnExpression *l = buildExpression(add["lval"], scope);
	PlnExpression *r = buildExpression(add["rval"], scope);
	return PlnAddOperation::create(l, r);
}

PlnExpression* buildSubOperation(json& sub, PlnScopeStack &scope)
{
	PlnExpression *l = buildExpression(sub["lval"], scope);
	PlnExpression *r = buildExpression(sub["rval"], scope);
	return PlnAddOperation::create_sub(l, r);
}

PlnExpression* buildMulOperation(json& mul, PlnScopeStack &scope)
{
	PlnExpression *l = buildExpression(mul["lval"], scope);
	PlnExpression *r = buildExpression(mul["rval"], scope);
	return PlnMulOperation::create(l, r);
}

PlnExpression* buildDivOperation(json& div, PlnScopeStack &scope)
{
	PlnExpression *l = buildExpression(div["lval"], scope);
	PlnExpression *r = buildExpression(div["rval"], scope);
	return PlnDivOperation::create(l, r);
}

PlnExpression* buildModOperation(json& mod, PlnScopeStack &scope)
{
	PlnExpression *l = buildExpression(mod["lval"], scope);
	PlnExpression *r = buildExpression(mod["rval"], scope);
	try {
		return PlnDivOperation::create_mod(l, r);
	} catch (PlnCompileError& err) {
		setLoc(&err, mod);
		throw err;
	}
}

PlnExpression* buildNegativeOperation(json& neg, PlnScopeStack &scope)
{
	PlnExpression *e = buildExpression(neg["val"], scope);
	return PlnNegative::create(e);
}

PlnExpression* buildCmpOperation(json& cmp, PlnCmpType type, PlnScopeStack &scope)
{
	PlnExpression *l = buildExpression(cmp["lval"], scope);
	PlnExpression *r = buildExpression(cmp["rval"], scope);

	return new PlnCmpOperation(l, r, type);
}

PlnExpression* buildBoolOperation(json& bl, PlnExprsnType type, PlnScopeStack &scope)
{
	PlnExpression *l = buildExpression(bl["lval"], scope);
	PlnExpression *r = buildExpression(bl["rval"], scope);
	return new PlnBoolOperation(l, r, type);
}

