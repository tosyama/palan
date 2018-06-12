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
#include "models/expressions/PlnAssignment.h"
#include "models/expressions/PlnFunctionCall.h"
#include "models/expressions/PlnAddOperation.h"

#define CUR_BLOCK	scope.back().inf.block
#define CUR_MODULE	scope.front().inf.module

PlnModelTreeBuilder::PlnModelTreeBuilder()
{
}

static void registerPrototype(PlnModule& module, json& proto);
static void buildFunction(json& func, PlnScopeStack &scope);
static void buildBlock(PlnBlock& block, json& stmts, PlnScopeStack &scope);

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
		scope.push_back(module->toplevel);
		buildBlock(*module->toplevel, ast["stmts"], scope);
	}

	return module;
}

static vector<PlnType*> getVarType(PlnModule& module, json& var_type)
{
	vector<PlnType*> ret_vt;
	for (auto& vt: var_type) {
		string type_name = vt["name"];
		if (type_name == "[]") {
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
		}

		for (auto& ret: proto["rets"]) {
			vector<PlnType*> var_type = getVarType(module, ret["var-type"]);
			string name;
			if (ret["name"].is_string())
				name = ret["name"];
			f->addRetValue(name, var_type, true);
		}

	} else if (ftype_str == "ccall") {
		f = new PlnFunction(FT_C, proto["name"]);
		if (proto["ret-type"].is_string()) {
			if(PlnType *t = module.getType(proto["ret-type"])) {
				string rname = "";
				vector<PlnType*> tv = { t };
				f->addRetValue(rname, tv, false);
			}
		}

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
	auto b = new PlnBlock();
	f->implement = b;
	b->setParent(f);
	scope.push_back(f);
	buildBlock(*b, func["impl"]["stmts"], scope);
	scope.pop_back();
}

static PlnStatement* buildStatement(json& stmt, PlnScopeStack &scope);
void buildBlock(PlnBlock& block, json& stmts, PlnScopeStack &scope)
{
	scope.push_back(&block);
	for (json& stmt: stmts) {
		block.statements.push_back(buildStatement(stmt, scope));
	}
	scope.pop_back();
}

static PlnExpression* buildExpression(json& exp, PlnScopeStack &scope);
static PlnVarInit* buildVarInit(json& var_init, PlnScopeStack &scope);
PlnStatement* buildStatement(json& stmt, PlnScopeStack &scope)
{
	string type = stmt["stmt-type"];

	if (type == "exp") {
		return new PlnStatement(buildExpression(stmt["exp"], scope), CUR_BLOCK);

	} else if (type == "block") {
		PlnBlock *block = new PlnBlock();
		block->setParent(CUR_BLOCK);
		buildBlock(*block, stmt["block"]["stmts"], scope);
		return new PlnStatement(block, CUR_BLOCK);

	} else if (type == "var-init") {
		return new PlnStatement(buildVarInit(stmt, scope), CUR_BLOCK);

	} else {
		BOOST_ASSERT(false);
	}
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

static PlnExpression* buildVariarble(json var, PlnScopeStack &scope);
static PlnExpression* buildFuncCall(json& fcall, PlnScopeStack &scope);
static PlnExpression* buildAssignment(json& asgn, PlnScopeStack &scope);
static PlnExpression* buildAddOperation(json& add, PlnScopeStack &scope);

PlnExpression* buildExpression(json& exp, PlnScopeStack &scope)
{
	BOOST_ASSERT(exp["exp-type"].is_string());
	string type = exp["exp-type"];

	if (type == "lit-int") {
		return new PlnExpression(exp["val"].get<int64_t>());
	} else if (type == "lit-uint") {
		return new PlnExpression(exp["val"].get<uint64_t>());
	} else if (type == "lit-str") {
		PlnReadOnlyData *ro = CUR_MODULE->getReadOnlyData(exp["val"]);
		return new PlnExpression(ro);
	} else if (type == "var") {
		return buildVariarble(exp, scope);
	} else if (type == "asgn") {
		return buildAssignment(exp, scope);
	} else if (type == "func-call") {
		return buildFuncCall(exp, scope);
	} else if (type == "+") {
		return buildAddOperation(exp, scope);
	} else {
		// BOOST_ASSERT(false);
		return new PlnExpression(uint64_t(99));
	}
}

PlnExpression* buildFuncCall(json& fcall, PlnScopeStack &scope)
{
	vector<PlnExpression*> args;
	for (auto& arg: fcall["args"])
		args.push_back(buildExpression(arg["exp"], scope));
		// TODO: check move.

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

	if (var["opes"].is_array()) {
		BOOST_ASSERT(false);
	}
	return new  PlnExpression(pvar);
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
