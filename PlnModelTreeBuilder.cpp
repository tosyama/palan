/// Build model trees from AST json definision.
///
/// @file	PlnModelTreeBuilder.cpp
/// @copyright	2018 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include "PlnModel.h"
#include "PlnModelTreeBuilder.h"
#include "PlnScopeStack.h"
#include "PlnConstants.h"
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

#define CUR_BLOCK	scope.back().inf.block
#define CUR_MODULE	scope.front().inf.module

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

static void registerPrototype(PlnModule& module, json& proto);
static void buildFunction(json& func, PlnScopeStack &scope);
static PlnBlock* buildBlock(json& stmts, PlnScopeStack &scope);

PlnModule* PlnModelTreeBuilder::buildModule(json& ast)
{
	BOOST_ASSERT(!ast.is_null());

	PlnModule *module = new PlnModule();
	PlnScopeStack scope;
	scope.push_back(module);

	if (ast["protos"].is_array()) {
		json& protos = ast["protos"];
		for (auto& proto: protos) {
			registerPrototype(*module, proto);
		}
	}

	if (ast["funcs"].is_array()) {
		json& funcs = ast["funcs"];
		for (auto& func: funcs) {
			buildFunction(func, scope);
		}
	}

	if (ast["stmts"].is_array()) {
		module->toplevel = buildBlock(ast["stmts"], scope);
	}

	return module;
}

static vector<PlnType*> getVarType(PlnModule& module, json& var_type)
{
	vector<PlnType*> ret_vt;
	for (auto& vt: var_type) {
		string type_name = vt["name"];
		if (type_name == "[]") {
			BOOST_ASSERT(vt["sizes"].is_array());
			vector<int> sizes;
			for (json& i: vt["sizes"])
				sizes.push_back(i);
			PlnType* arr_t = module.getFixedArrayType(ret_vt, sizes);
			ret_vt.push_back(arr_t);
		} else {
			ret_vt.push_back(module.getType(type_name));
		}
	}
	return ret_vt;
}

void registerPrototype(PlnModule& module, json& proto)
{
	int f_type;
	string ftype_str = proto["func-type"];
	PlnFunction *f;

	if (ftype_str == "palan") {
		f = new PlnFunction(FT_PLN, proto["name"]);
		for (auto& param: proto["params"]) {
			vector<PlnType*> var_type = getVarType(module, param["var-type"]);
			PlnPassingMethod pm = FPM_COPY;
			if (param["move"].is_boolean()) {
				if (param["move"] == true)
					pm = FPM_MOVEOWNER;
			}
			f->addParam(param["name"], var_type, pm, NULL);
			setLoc(f->parameters.back(), param);
		}

		for (auto& ret: proto["rets"]) {
			vector<PlnType*> var_type = getVarType(module, ret["var-type"]);
			string name;
			if (ret["name"].is_string())
				name = ret["name"];
			f->addRetValue(name, var_type, true);
			setLoc(f->return_vals.back(), ret);
		}
		setLoc(f, proto);

	} else if (ftype_str == "ccall") {
		f = new PlnFunction(FT_C, proto["name"]);
		if (proto["ret-type"].is_string()) {
			if(PlnType *t = module.getType(proto["ret-type"])) {
				string rname = "";
				vector<PlnType*> tv = { t };
				f->addRetValue(rname, tv, false);
			}
		}
		setLoc(f, proto);

	} else if (ftype_str == "syscall") {
		f = new PlnFunction(FT_SYS, proto["name"]);
		f->inf.syscall.id = proto["id"];
		if (proto["ret-type"].is_string()) {
			if(PlnType *t = module.getType(proto["ret-type"])) {
				string rname = "";
				vector<PlnType*> tv = { t };
				f->addRetValue(rname, tv, false);
			}
		}
		setLoc(f, proto);

	} else
		BOOST_ASSERT(false);
	
	f->setParent(&module);
	module.functions.push_back(f);
}

void buildFunction(json& func, PlnScopeStack &scope)
{
	string pre_name;
	vector<string> param_types;
	for (auto& param: func["params"]) {
		vector<PlnType*> var_type = getVarType(*CUR_MODULE, param["var-type"]);
		if (var_type.size()) {
			param_types.push_back(var_type.back()->name);
			pre_name = param_types.back();
		} else {
			param_types.push_back(pre_name);
		}
	}

	vector<string> ret_types;
	for (auto& ret: func["rets"]) {
		vector<PlnType*> var_type = getVarType(*CUR_MODULE, ret["var-type"]);
		if (var_type.size()) {
			ret_types.push_back(var_type.back()->name);
			pre_name = ret_types.back();
		} else {
			ret_types.push_back(pre_name);
		}
	}

	PlnFunction* f = CUR_MODULE->getFunc(func["name"], param_types, ret_types);
	BOOST_ASSERT(f);
	setLoc(f, func);

	scope.push_back(f);
	f->implement = buildBlock(func["impl"]["stmts"], scope);
	setLoc(f->implement, func["impl"]);
	 
	scope.pop_back();
}

static PlnStatement* buildStatement(json& stmt, PlnScopeStack &scope);
PlnBlock* buildBlock(json& stmts, PlnScopeStack &scope)
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
	for (json& stmt: stmts) {
		block->statements.push_back(buildStatement(stmt, scope));
	}
	scope.pop_back();
	return block;
}

static PlnExpression* buildExpression(json& exp, PlnScopeStack &scope);
static PlnVarInit* buildVarInit(json& var_init, PlnScopeStack &scope);
static PlnStatement* buildReturn(json& ret, PlnScopeStack& scope);
static PlnStatement* buildWhile(json& whl, PlnScopeStack& scope);
static PlnStatement* buildIf(json& ifels, PlnScopeStack& scope);

PlnStatement* buildStatement(json& stmt, PlnScopeStack &scope)
{
	string type = stmt["stmt-type"];
	PlnStatement *statement;

	if (type == "exp") {
		statement = new PlnStatement(buildExpression(stmt["exp"], scope), CUR_BLOCK);

	} else if (type == "block") {
		PlnBlock *block = buildBlock(stmt["block"]["stmts"], scope);
		setLoc(block, stmt["block"]);
		statement = new PlnStatement(block, CUR_BLOCK);

	} else if (type == "var-init") {
		statement = new PlnStatement(buildVarInit(stmt, scope), CUR_BLOCK);

	} else if (type == "return") {
		statement = buildReturn(stmt, scope);

	} else if (type == "while") {
		statement = buildWhile(stmt, scope);

	} else if (type == "if") {
		statement = buildIf(stmt, scope);

	} else {
		BOOST_ASSERT(false);
	}
	setLoc(statement, stmt);
	return statement;
}

PlnVarInit* buildVarInit(json& var_init, PlnScopeStack &scope)
{
	BOOST_ASSERT(var_init["vars"].is_array());
	vector<PlnValue> vars;
	for (json &var: var_init["vars"]) {
		vector<PlnType*> t = getVarType(*CUR_MODULE, var["var-type"]);
		PlnVariable *v = CUR_BLOCK->declareVariable(var["name"], t, true);
		BOOST_ASSERT(v);
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

PlnStatement* buildReturn(json& ret, PlnScopeStack& scope)
{
	vector<PlnExpression *> ret_vals;
	if (ret["ret-vals"].is_array())
		for (json& ret_val: ret["ret-vals"])
			ret_vals.push_back(buildExpression(ret_val, scope));
	
	return new PlnReturnStmt(ret_vals, CUR_BLOCK);
}

PlnStatement* buildWhile(json& whl, PlnScopeStack& scope)
{
	PlnExpression* cond = buildExpression(whl["cond"], scope);
	PlnBlock* block = buildBlock(whl["block"]["stmts"], scope);
	setLoc(block, whl["block"]);
	return new PlnWhileStatement(cond, block, CUR_BLOCK);
}

PlnStatement* buildIf(json& ifels, PlnScopeStack& scope)
{
	PlnExpression* cond = buildExpression(ifels["cond"], scope);
	PlnBlock* ifblock = buildBlock(ifels["block"]["stmts"], scope);
	setLoc(ifblock, ifels["block"]);
	PlnStatement* else_stmt = NULL;
	if (ifels["else"].is_object()) {
		else_stmt = buildStatement(ifels["else"], scope);
	}

	return new PlnIfStatement(cond, ifblock, else_stmt, CUR_BLOCK);
}

static PlnExpression* buildVariarble(json var, PlnScopeStack &scope);
static PlnExpression* buildFuncCall(json& fcall, PlnScopeStack &scope);
static PlnExpression* buildAssignment(json& asgn, PlnScopeStack &scope);
static PlnExpression* buildAddOperation(json& add, PlnScopeStack &scope);
static PlnExpression* buildSubOperation(json& sub, PlnScopeStack &scope);
static PlnExpression* buildMulOperation(json& mul, PlnScopeStack &scope);
static PlnExpression* buildDivOperation(json& div, PlnScopeStack &scope);
static PlnExpression* buildModOperation(json& mod, PlnScopeStack &scope);
static PlnExpression* buildNegativeOperation(json& neg, PlnScopeStack &scope);
static PlnExpression* buildCmpOperation(json& cmp, PlnCmpType type, PlnScopeStack &scope);
static PlnExpression* buildBoolOperation(json& bl, PlnExprsnType type, PlnScopeStack &scope);

PlnExpression* buildExpression(json& exp, PlnScopeStack &scope)
{
	BOOST_ASSERT(exp["exp-type"].is_string());
	string type = exp["exp-type"];
	PlnExpression *expression;

	if (type == "lit-int") {
		expression = new PlnExpression(exp["val"].get<int64_t>());
	} else if (type == "lit-uint") {
		expression = new PlnExpression(exp["val"].get<uint64_t>());
	} else if (type == "lit-str") {
		PlnReadOnlyData *ro = CUR_MODULE->getReadOnlyData(exp["val"]);
		expression = new PlnExpression(ro);
	} else if (type == "var") {
		expression = buildVariarble(exp, scope);
	} else if (type == "asgn") {
		expression = buildAssignment(exp, scope);
	} else if (type == "func-call") {
		expression = buildFuncCall(exp, scope);
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
		BOOST_ASSERT(false);
	}
	setLoc(expression, exp);
	return expression;
}

PlnExpression* buildFuncCall(json& fcall, PlnScopeStack &scope)
{
	vector<PlnExpression*> args;
	for (auto& arg: fcall["args"]) {
		args.push_back(buildExpression(arg["exp"], scope));
		if (arg["move"].is_boolean() && arg["move"] == true) {
			BOOST_ASSERT(args.back()->values[0].type == VL_VAR);
			args.back()->values[0].asgn_type = ASGN_MOVE;
		}
	}

	PlnFunction* f = CUR_MODULE->getFunc(fcall["func-name"], args);
	if (f) {
		return new PlnFunctionCall(f, args);
	} else {
		BOOST_ASSERT(false);
	}
}

static PlnExpression* buildDstValue(json dval, PlnScopeStack &scope);

PlnExpression* buildAssignment(json& asgn, PlnScopeStack &scope)
{
	json& src = asgn["src-exps"];
	BOOST_ASSERT(src.is_array());

	vector<PlnExpression *> src_exps;
	for (json& exp: src) {
		src_exps.push_back(buildExpression(exp, scope));
	}

	json& dst = asgn["dst-vals"];
	BOOST_ASSERT(dst.is_array());

	vector<PlnExpression *> dst_vals;
	for (json& dval: dst) {
		dst_vals.push_back(buildDstValue(dval, scope));
	}

	return new PlnAssignment(dst_vals, src_exps);
}

PlnExpression* buildVariarble(json var, PlnScopeStack &scope)
{
	PlnVariable* pvar = CUR_BLOCK->getVariable(var["base-var"]);
	BOOST_ASSERT(pvar);

	PlnExpression *var_exp = new PlnExpression(pvar);
	setLoc(var_exp, var);

	if (var["opes"].is_array()) {
		for (json& ope: var["opes"]) {
			if (ope["ope-type"]=="index") {
				BOOST_ASSERT(ope["indexes"].is_array());
				vector<PlnExpression*> indexes;
				for (json& exp: ope["indexes"])
					indexes.push_back(buildExpression(exp, scope));
				var_exp = new PlnArrayItem(var_exp, indexes);
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
	return PlnDivOperation::create_mod(l, r);
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

