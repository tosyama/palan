#include <boost/assert.hpp>

#include "PlnFunction.h"
#include "PlnBlock.h"
#include "PlnStatement.h"
#include "PlnVariable.h"
#include "expressions/PlnMultiExpression.h"
#include "../PlnGenerator.h"

using std::endl;

inline PlnFunction* getFunction(PlnBlock *b)
{
	BOOST_ASSERT(b);
	while (b->parent_type == BP_BLOCK) {
		b = b->parent.block;
	}
	BOOST_ASSERT(b->parent_type == BP_FUNC);
	return b->parent.function;
}

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

void PlnStatement::finish()
{
	switch (type) {
		case ST_EXPRSN:
			inf.expression->finish();
			break;
		case ST_BLOCK:
			inf.block->finish();
			break;
		case ST_VARINIT:
			inf.var_init->finish();
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
	function = getFunction(parent);

	if (retexp) {
		inf.expression = retexp;
	} else if (!function->return_vals.size()) {
		if (function->name == "main")
			inf.expression = new PlnExpression(0);
		else
			inf.expression = NULL;
	} else if (function->return_vals.size() == 1) {
		inf.expression = new PlnExpression(function->return_vals[0]);
	} else {
		PlnMultiExpression *m = new PlnMultiExpression();
		for (auto v: function->return_vals)
			m->append(new PlnExpression(v));
		inf.expression = m;
	}
}

void PlnReturnStmt::finish()
{
	if (inf.expression) {
		int i=0;
		for (auto v: inf.expression->values) {
			PlnReturnPlace rp;
			rp.type = RP_ARGPLN;
			rp.inf.index = i;
			inf.expression->ret_places.push_back(rp);
			++i;
		}
		inf.expression->finish();
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

	if (function->name == "main") 
		g.genMainReturn();	
	else
		g.genReturn();
}

