#include <boost/assert.hpp>

#include "PlnFunction.h"
#include "PlnBlock.h"
#include "PlnStatement.h"
#include "PlnVariable.h"
#include "PlnType.h"
#include "expressions/PlnMultiExpression.h"
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
			if (inf.var_init->initializer)
				inf.var_init->initializer->dump(os, indent+" ");
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
PlnReturnStmt::PlnReturnStmt(PlnExpression *retexp, PlnBlock* parent)
{
	type = ST_RETURN;
	this->parent = parent;
	function = parent->parent_func;

	if (retexp) {
		inf.expression = retexp;
	} else if (!function->return_vals.size()) {
		inf.expression = NULL;
	} else if (function->return_vals.size() == 1) {
		if (function->name == "main"
			&& function->return_vals[0]->name=="")
			inf.expression = new PlnExpression(0);
		else
			inf.expression = new PlnExpression(function->return_vals[0]);
	} else {
		PlnMultiExpression *m = new PlnMultiExpression();
		for (auto v: function->return_vals)
			m->append(new PlnExpression(v));
		inf.expression = m;
	}
}

void PlnReturnStmt::finish(PlnDataAllocator& da)
{
	if (inf.expression) {
		int i=0;
		int diff=0;
		if (function->name == "main") {
			BOOST_ASSERT(function->return_vals.size()<=1);
			diff = 1;
		}

		vector<PlnDataPlace*> dps = da.allocReturnValues(function->return_vals);
		for (auto v: inf.expression->values) {
			PlnReturnPlace rp;
			rp.type = RP_ARGPLN;
			rp.inf.arg.index = i+diff;
			if (i < function->return_vals.size()) 
				rp.inf.arg.size = function->return_vals[i]->var_type->size;
			else
				rp.inf.arg.size = 8;	// TODO: get default.
				
			inf.expression->ret_places.push_back(rp);

			inf.expression->data_places.push_back(dps[i]);
			++i;
		}
		inf.expression->finish(da);
		da.returnedValues(dps);
	}
}

void PlnReturnStmt::dump(ostream& os, string indent)
{
	os << indent << "Return: " << endl;
	if (inf.expression) 
		inf.expression->dump(os, indent+" ");
}

void PlnReturnStmt::gen(PlnGenerator& g)
{
	if (inf.expression)
		inf.expression->gen(g);

	g.genFreeLocalVarArea(function->inf.pln.stack_size);
	if (function->name == "main") 
		g.genMainReturn();	
	else
		g.genReturn();
}

