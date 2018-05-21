/// PlnStatement model class definition.
///
/// @file	PlnStatement.cpp
/// @copyright	2017 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include "../PlnConstants.h"
#include "PlnFunction.h"
#include "PlnBlock.h"
#include "PlnStatement.h"
#include "PlnVariable.h"
#include "PlnType.h"
#include "PlnExpression.h"
#include "../PlnDataAllocator.h"
#include "../PlnGenerator.h"
#include "../PlnScopeStack.h"

using std::endl;

// PlnStatement
PlnStatement::PlnStatement(PlnExpression *exp, PlnBlock* parent)
	: type(ST_EXPRSN), parent(parent)
{
	inf.expression = exp;
}

PlnStatement::PlnStatement(PlnVarInit* var_init, PlnBlock* parent)
	: type(ST_VARINIT), parent(parent)
{
	inf.var_init = var_init;
}

PlnStatement::PlnStatement(PlnBlock* block, PlnBlock* parent)
	: type(ST_BLOCK), parent(parent)
{
	inf.block = block;
}

void PlnStatement::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	switch (type) {
		case ST_EXPRSN:
			inf.expression->finish(da, si);
			break;
		case ST_BLOCK:
			inf.block->finish(da, si);
			break;
		case ST_VARINIT:
			inf.var_init->finish(da, si);
			break;
	}
}

void PlnStatement::gen(PlnGenerator& g)
{
	switch (type) {
		case ST_EXPRSN:
			inf.expression->gen(g);
			break;
		case ST_VARINIT:
			inf.var_init->gen(g);
			break;
		case ST_BLOCK:
			inf.block->gen(g);
			break;
		default:
			BOOST_ASSERT(false);	
			break;
	}
}

// PlnReturnStmt
PlnReturnStmt::PlnReturnStmt(vector<PlnExpression *>& retexp, PlnBlock* parent)
{
	type = ST_RETURN;
	this->parent = parent;
	function = parent->parent_func;
	
	expressions = move(retexp);

	if (expressions.size()==0 && 
		function->return_vals.size() > 0)
	{
		for (auto v: function->return_vals)
			expressions.push_back(new PlnExpression(v));
	}
}

void PlnReturnStmt::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{

	BOOST_ASSERT(function->type == FT_PLN);
	vector<PlnDataPlace*> dps = da.prepareRetValDps(FT_PLN, function->ret_dtypes, function->arg_dtypes, true);
	vector<PlnVariable*> ret_vars;

	int i=0, j=0;
	for (auto rt: function->return_vals) {
		dps[i]->data_type = rt->var_type.back()->data_type;
		i++;
	}
	
	i = 0;
	for (auto e: expressions) {
		for (auto &v: e->values) {
			e->data_places.push_back(dps[i]);
			++i;
		}
		e->finish(da, si);

		// ret_vars is just used checking requirement of free varialbes.
		if (e->type == ET_VALUE
				&& e->values[0].type == VL_VAR
				&& (e->values[0].inf.var->ptr_type & PTR_OWNERSHIP))
		{
			ret_vars.push_back(e->values[0].inf.var);
		}
	}

	// Create free varialbe expression.(variables in scope except returning)
	if (si.owner_vars.size() > 0) {
		for (auto &i: si.owner_vars) {
			bool do_ret = false;
			for (auto rv: ret_vars)
				if (rv == i.var) {
					do_ret = true;
					break;
				}
			if (!do_ret) {
				PlnExpression* free_ex = PlnFreer::getFreeEx(i.var);
				free_vars.push_back(free_ex);
			}
		}
	}
	for (auto free_var: free_vars)
		free_var->finish(da, si);

	for(auto dp: dps) {
		da.popSrc(dp);
	}
	for(auto dp: dps) {
		da.releaseDp(dp);
	}
}

void PlnReturnStmt::gen(PlnGenerator& g)
{
	for (auto e: expressions)
		e->gen(g);

	PlnDataPlace* adp = NULL;

	for (auto free_var: free_vars)
		free_var->gen(g);

	for (auto e: expressions)
		for (auto dp: e->data_places)
				g.genLoadDp(dp, false);
				
	for (auto e: expressions)
		for (auto dp: e->data_places)
			g.genSaveDp(dp);

	for (int i; i<function->save_regs.size(); i++) {
		auto e = g.getEntity(function->save_reg_dps[i]);
		g.genLoadReg(function->save_regs[i], e.get());
	}
	
	g.genReturn();
}

