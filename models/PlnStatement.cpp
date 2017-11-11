// PlnStatement model class definition.
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
	var_init->parent = parent;
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
			inf.expression->finish(da);
			break;
		case ST_BLOCK:
			inf.block->finish(da, si);
			break;
		case ST_VARINIT:
			inf.var_init->finish(da, si);
			break;
	}

	for (auto dp: da.release_stmt_end) 
		da.releaseData(dp);
	da.release_stmt_end.resize(0);
}

void PlnStatement::dump(ostream& os, string indent)
{
	switch (type) {
		case ST_EXPRSN:
			inf.expression->dump(os, indent);
			break;

		case ST_VARINIT:
			os << indent << "Initialize: ";
			for (auto v: inf.var_init->vars)
				os << v.inf.var->name << " ";
			os << endl;
			for (auto i: inf.var_init->initializer)
				i->dump(os, indent+" ");
			break;

		case ST_BLOCK:
			inf.block->dump(os, indent);
			break;

		default:
			os << indent << "Unknown type" << endl;
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
	late_pop_dp = NULL;
	
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
	vector<PlnDataPlace*> dps = da.prepareRetValDps(function->return_vals.size(), FT_PLN, true);
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
			if (da.isAccumulator(dps[i])) {
				late_pop_dp = dps[i];
			}
			++i;
		}
		e->finish(da);
		for(;j<i;++j) {
			da.allocDp(dps[j]);
		}

		// ret_vars is just used checking requirement of free varialbes.
		if (e->type == ET_VALUE
				&& e->values[0].type == VL_VAR
				&& (e->values[0].inf.var->ptr_type & PTR_OWNERSHIP))
		{
			ret_vars.push_back(e->values[0].inf.var);
		}
	}

	// create free varialbe list.(variables in scope except returning)
	if (si.owner_vars.size() > 0) {
		bool do_free = false;
		for (auto &i: si.owner_vars) {
			bool do_ret = false;
			for (auto rv: ret_vars)
				if (rv == i.var) {
					do_ret = true;
					break;
				}
			if (!do_ret) {
				to_free_vars.push_back(i.var);
				do_free = true;
			}
		}
		if (do_free)
			da.memFreed();
	}

	da.returnedValues(dps, FT_PLN);
}

void PlnReturnStmt::dump(ostream& os, string indent)
{
	os << indent << "Return: " << endl;
	for (auto e: expressions)
		e->dump(os, indent+" ");
}

void PlnReturnStmt::gen(PlnGenerator& g)
{
	for (auto e: expressions)
		e->gen(g);

	PlnDataPlace* adp = NULL;

	for (auto v: to_free_vars) {
		auto ve = g.getPopEntity(v->place);
		g.genMemFree(ve.get(), v->name, false);	
	}

	for (auto e: expressions)
		for (auto dp: e->data_places)
			if (dp != late_pop_dp)
				g.getPopEntity(dp);
				
	if (late_pop_dp)
		g.getPopEntity(late_pop_dp);

	for (int i; i<function->save_regs.size(); i++) {
		auto e = g.getPopEntity(function->save_reg_dps[i]);
		g.genLoadReg(function->save_regs[i], e.get());
	}
	
	g.genFreeLocalVarArea(function->inf.pln.stack_size);
	g.genReturn();
}

