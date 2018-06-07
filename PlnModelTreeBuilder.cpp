/// Build model trees from AST json definision.
///
/// @file	PlnModelTree.h
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
#include "models/expressions/PlnFunctionCall.h"

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

void registerPrototype(PlnModule& module, json& proto)
{
	int f_type;
	string ftype_str = proto["func-type"];
	PlnFunction *f;

	if (ftype_str == "palan") {
		f = new PlnFunction(FT_PLN, proto["name"]);
		for (auto& ret: proto["rets"])
			;

	} else if (ftype_str == "ccall") {
		f = new PlnFunction(FT_C, proto["name"]);
		if (proto["ret-type"].is_string()) {
			if(PlnType *t = module.getType(proto["ret-type"])) {
				string rname = "";
				vector<PlnType*> tv = { t };
				f->addRetValue(rname, &tv, false);
			}
		}

	} else if (ftype_str == "syscall") {
		f = new PlnFunction(FT_SYS, proto["name"]);
		f->inf.syscall.id = proto["id"];
		if (proto["ret-type"].is_string()) {
			if(PlnType *t = module.getType(proto["ret-type"])) {
				string rname = "";
				vector<PlnType*> tv = { t };
				f->addRetValue(rname, &tv, false);
			}
		}

	} else
		BOOST_ASSERT(false);
	
	f->setParent(&module);
	module.functions.push_back(f);
}

void buildFunction(json& func, PlnScopeStack &scope)
{
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
PlnStatement* buildStatement(json& stmt, PlnScopeStack &scope)
{
	string type = stmt["stmt-type"];

	if (type == "exp") {
		return new PlnStatement(buildExpression(stmt["exp"], scope), CUR_BLOCK);
	} else {
		BOOST_ASSERT(false);
	}
}

static PlnExpression* buildFuncCall(json& exp, PlnScopeStack &scope);
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
	} else if (type == "func-call") {
		return buildFuncCall(exp, scope);
	} else {
		// BOOST_ASSERT(false);
		return new PlnExpression(uint64_t(99));
	}
}

PlnExpression* buildFuncCall(json& exp, PlnScopeStack &scope)
{
	vector<PlnExpression*> args;
	for (auto& arg: exp["args"])
		args.push_back(buildExpression(arg["exp"], scope));
		// TODO: check move.

	PlnFunction* f = CUR_MODULE->getFunc(exp["func-name"], args);
	if (f) {
		return new PlnFunctionCall(f, args);
	} else {
		BOOST_ASSERT(false);
	}
}
