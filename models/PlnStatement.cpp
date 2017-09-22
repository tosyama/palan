/// PlnStatement model class definition.
///
/// @file	PlnStatement.cpp
/// @copyright	2017- YAMAGUCHI Toshinobu 
#include <boost/assert.hpp>

#include "PlnFunction.h"
#include "PlnBlock.h"
#include "PlnStatement.h"
#include "PlnVariable.h"
#include "PlnType.h"
#include "PlnExpression.h"
#include "../PlnDataAllocator.h"
#include "../PlnGenerator.h"

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

void PlnStatement::finish(PlnDataAllocator& da)
{
	switch (type) {
		case ST_EXPRSN:
			inf.expression->finish(da);
			break;
		case ST_BLOCK:
			inf.block->finish(da);
			break;
		case ST_VARINIT:
			inf.var_init->finish(da);
			break;
	}
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
				os << v->name << " ";
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

void PlnReturnStmt::finish(PlnDataAllocator& da)
{
	int i=0, j=0;

	BOOST_ASSERT(function->type == FT_PLN);

	vector<PlnDataPlace*> dps = da.prepareRetValDps(function->return_vals.size(), DPF_PLN, true);
	for (auto e: expressions) {
		for (auto v: e->values) {
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
	}
	da.returnedValues(dps, DPF_PLN);
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

	for (auto e: expressions)
		for (auto dp: e->data_places)
			if (dp != late_pop_dp)
				g.getPopEntity(dp);
				
	if (late_pop_dp)
		g.getPopEntity(late_pop_dp);

	g.genFreeLocalVarArea(function->inf.pln.stack_size);
	g.genReturn();
}

