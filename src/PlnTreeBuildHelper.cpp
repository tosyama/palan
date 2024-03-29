/// Helper functions for building model tree directly.
///
/// @file	PlnTreeBuildHelper.cpp
/// @copyright	2018-2020 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>

#include "PlnConstants.h"
#include "PlnTreeBuildHelper.h"
#include "models/PlnType.h"
#include "models/PlnBlock.h"
#include "models/PlnExpression.h"
#include "models/PlnStatement.h"
#include "models/PlnVariable.h"
#include "models/PlnLoopStatement.h"
#include "models/expressions/PlnAddOperation.h"
#include "models/expressions/PlnArrayItem.h"
#include "models/expressions/PlnAssignment.h"
#include "models/expressions/PlnCmpOperation.h"
#include "models/expressions/PlnFunctionCall.h"
#include "models/types/PlnFixedArrayType.h"

namespace palan
{

PlnVariable* declareUInt(PlnBlock* block, string name, uint64_t init_i)
{
	PlnVariable *var = block->declareVariable(name, PlnVarType::getUint(), false);
	BOOST_ASSERT(var);
	vector<PlnValue> vars = { var };

	vector<PlnExpression*> inis = { new PlnExpression(init_i) };
	block->statements.push_back(new PlnStatement(new PlnVarInit(vars, &inis), block));

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

void malloc(PlnBlock* block, PlnVariable* var, PlnExpression* alloc_size_ex)
{
	PlnValue var_val(var);
	var_val.asgn_type = ASGN_COPY_REF;
	auto var_ex = new PlnExpression(var_val);
	vector<PlnExpression*> lvals = { var_ex };

	PlnFunction* func_malloc = PlnFunctionCall::getInternalFunc(IFUNC_MALLOC);
	vector<PlnExpression*> args = { alloc_size_ex };
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

void exit(PlnBlock* block, uint64_t result)
{
	PlnFunction* func_free = PlnFunctionCall::getInternalFunc(IFUNC_EXIT);
	vector<PlnExpression*> args = { new PlnExpression(result) };
	PlnFunctionCall *call_free = new PlnFunctionCall(func_free, args);
	block->statements.push_back(new PlnStatement(call_free, block));
}

PlnArrayItem* rawArrayItem(PlnVariable* var, PlnVariable* index)
{
	PlnValue var_val(var);
	var_val.asgn_type = ASGN_COPY_REF;
	auto arr_ex = new PlnExpression(var_val);
	auto index_ex = new PlnExpression(index);
	vector<PlnExpression*> inds = { index_ex };

	PlnFixedArrayVarType *farr_type = static_cast<PlnFixedArrayVarType*>(var->var_type);
	vector<int> raw_sizes = {0};

	PlnVarType* raw_arr_vtype = farr_type->getVarType("wmr");
	static_cast<PlnFixedArrayVarType*>(raw_arr_vtype)->sizes = raw_sizes;

	return new PlnArrayItem(arr_ex, inds, raw_arr_vtype);
}

PlnBlock* whileLess(PlnBlock* block, PlnVariable *var, PlnExpression* loop_num_ex)
{
	PlnCmpOperation *cmp_op;
	auto l_ex = new PlnExpression(var);
	cmp_op = new PlnCmpOperation(l_ex, loop_num_ex, CMP_L);
	auto wblock = new PlnBlock();
	auto whl = new PlnWhileStatement(cmp_op, wblock, block);
	block->statements.push_back(whl);

	wblock->setParent(block);

	return wblock;
}

} // end namespace

