/// Helper functions for building model tree directly.
///
/// @file	PlnBuildTreeHelper.cpp
/// @copyright	2018 YAMAGUCHI Toshinobu 

#include "PlnBuildTreeHelper.h"
#include "models/PlnType.h"
#include "models/PlnBlock.h"
#include "models/PlnExpression.h"
#include "models/PlnStatement.h"
#include "models/PlnVariable.h"
#include "models/expressions/PlnAddOperation.h"
#include "models/expressions/PlnAssignment.h"
#include "models/expressions/PlnFunctionCall.h"
#include <boost/assert.hpp>

namespace palan
{

PlnVariable* declareUInt(PlnBlock* block, string name, uint64_t init_i)
{
	vector<PlnType*> t = { PlnType::getSint() };
	PlnVariable *var = block->declareVariable(name, t);
	BOOST_ASSERT(var);
	vector<PlnValue> vars = { var };

	vector<PlnExpression*> inis = { new PlnExpression(init_i) };
	block->statements.push_back(new PlnStatement(new PlnVarInit(vars, inis), block));

	return var;
}

void incrementUInt(PlnBlock* block, PlnVariable *var, uint64_t i)
{
	auto var_ex = new PlnExpression(var);
	auto inc_ex = new PlnExpression(i);
	auto add = new PlnAddOperation(var_ex, inc_ex);

	vector<PlnExpression*> lvals = { new PlnExpression(var) };
	vector<PlnExpression*> exps = { add };

	auto inc_st = new PlnAssignment(lvals, exps);

	block->statements.push_back(new PlnStatement(inc_st, block));
}

void malloc(PlnBlock* block, PlnVariable* var, uint64_t alloc_size)
{
	PlnValue var_val(var);
	var_val.lval_type = LVL_REF;
	auto var_ex = new PlnExpression(var_val);
	vector<PlnExpression*> lvals = { var_ex };

	PlnFunction* func_malloc = PlnFunctionCall::getInternalFunc(IFUNC_MALLOC);
	vector<PlnExpression*> args = { new PlnExpression(alloc_size) };
	PlnFunctionCall *call_malloc= new PlnFunctionCall(func_malloc, args);

	vector<PlnExpression*> exps = { call_malloc };

	block->statements.push_back(new PlnStatement(new PlnAssignment(lvals, exps), block));
}

void free(PlnBlock* block, PlnVariable* var)
{
	PlnFunction* func_free = PlnFunctionCall::getInternalFunc(IFUNC_FREE);
	vector<PlnExpression*> args = { new PlnExpression(var) };
	PlnFunctionCall *call_free = new PlnFunctionCall(func_free, args);
	block->statements.push_back(new PlnStatement(call_free, block));
}

} // end namespace

