/// PlnStatement model class definition.
///
/// @file	PlnStatement.cpp
/// @copyright	2017-2020 YAMAGUCHI Toshinobu 

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
#include "../PlnMessage.h"
#include "../PlnException.h"
#include "expressions/PlnClone.h"
#include "expressions/PlnAssignment.h"

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

PlnStatement::~PlnStatement()
{	
	switch (type) {
		case ST_EXPRSN:
			delete inf.expression;
			break;
		case ST_BLOCK:
			delete inf.block;
			break;
		case ST_VARINIT:
			delete inf.var_init;
			break;
	}
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
	}
}

// PlnReturnStmt
PlnReturnStmt::PlnReturnStmt(vector<PlnExpression *>& retexp, PlnBlock* parent)
	: expressions(move(retexp))
{
	type = ST_RETURN;
	this->parent = parent;
	function = parent->parent_func;
	BOOST_ASSERT(function);
	
	if (expressions.size()==0) {
		if (function->return_vals.size() > 0) {
			if (function->return_vals[0].local_var->name == "") {
				deleteInstatnces(expressions);
				PlnCompileError err(E_NeedRetValues);
				throw err;
			}

			for (auto& rv: function->return_vals) {
				expressions.push_back(new PlnExpression(rv.local_var));
			}
		}
	} else { // check only
		int i=0;
		int num_ret = function->return_vals.size();
		for (auto e: expressions) {
			BOOST_ASSERT(i<num_ret);
			i += e->values.size();
		}
 
		if (i<num_ret) {
			throw PlnCompileError(E_InvalidRetValues);
		}
	}
}

PlnReturnStmt::~PlnReturnStmt()
{
	for (auto e: expressions)
		delete e;
	for (auto free_var: free_vars)
		delete free_var;
}

void PlnReturnStmt::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	BOOST_ASSERT(function->type == FT_PLN);
	dps = function->createRetValDps();
	da.setRetValDps(function->type, dps, true);

	vector<PlnVariable*> ret_vars;
	
	int i = 0;
	int j = 0;
	for (int j=0; j<expressions.size(); j++) {
		PlnExpression* e = expressions[j];
		PlnClone* clone = NULL;
		for (auto &v: e->values) {
			if (i<dps.size()) {
				if (v.type == VL_LIT_ARRAY) { // e.g.) return [1, 2];
					BOOST_ASSERT(e->values.size() == 1);
					if (function->return_vals[i].local_var->name == "") { // e.g) func fname() -> ObjType {}
						clone = new PlnClone(da, e, e->values[0].getVarType(), false);
						clone->data_places.push_back(dps[i]);

					} else { // e.g) func fname() -> ObjType varname {}
						auto var_ex = new PlnExpression(function->return_vals[i].local_var);
						var_ex->values[0].asgn_type = ASGN_COPY;

						vector<PlnExpression*> dsts = { var_ex };
						vector<PlnExpression*> exps = { e };
						PlnAssignment* asgn_ex = new PlnAssignment(dsts, exps);

						expressions[j] = asgn_ex;
						asgn_ex->data_places.push_back(dps[i]);
						ret_vars.push_back(var_ex->values[0].inf.var);
					}

				} else {
					e->data_places.push_back(dps[i]);
				}
			}
			++i;
		}
		e = expressions[j];
		clones.push_back(clone);
		if (clone)
			clone->finishAlloc(da, si);
		e->finish(da, si);
		if (clone)
			clone->finish(da, si);

		// ret_vars is just used checking requirement of free varialbes.
		if (e->type == ET_VALUE && e->values[0].type == VL_VAR)
		{
			ret_vars.push_back(e->values[0].inf.var);
		}
	}

	// Create free varialbe expression.(variables in scope except returning)
	if (si.owner_vars.size() > 0) {
		for (auto &i: si.owner_vars) {
			if (i.var->var_type->mode[ALLOC_MD]=='h') {
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
	int i=0;
	for (auto e: expressions) {
		if (clones[i])
			clones[i]->genAlloc(g);
		e->gen(g);
		if (clones[i])
			clones[i]->gen(g);
		i++;
	}

	PlnDataPlace* adp = NULL;

	for (auto free_var: free_vars)
		free_var->gen(g);

	for(auto dp: dps)
		g.genLoadDp(dp, false);

	for(auto dp: dps)
		g.genSaveDp(dp);

	g.genReturn();
}

