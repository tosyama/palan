#include <boost/assert.hpp>
#include <boost/format.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include "PlnModel.h"
#include "PlnScopeStack.h"
#include "PlnX86_64Generator.h"

using std::endl;
using boost::format;
using boost::adaptors::reverse;

inline PlnFunction* getFunction(PlnBlock *b)
{
	BOOST_ASSERT(b);
	while (b->parent_type == BP_BLOCK) {
		b = b->parent.block;
	}
	BOOST_ASSERT(b->parent_type == BP_FUNC);
	return b->parent.function;
}


PlnBlock::PlnBlock() : cur_stack_size(0)
{
}

PlnVariable* PlnBlock::getVariable(string& var_name)
{
	PlnBlock* b = this;
	for(;;) {
		for (auto v: b->variables)
			if (v->name == var_name)
				return v;
		if (b->parent_type == BP_BLOCK)
			b = b->parent.block;
		else {
			PlnFunction* f=b->parent.function;
			for (auto rv: f->return_vals)
				if (rv->name == var_name) return rv;
			for (auto p: f->parameters)
				if (p->name == var_name) return p;
			// TODO: search grobal.
			return NULL;
		}
	}
}

void PlnBlock::setParent(PlnScopeItem& scope)
{
	switch(scope.type) {
		case SC_BLOCK:
			parent_type = BP_BLOCK;
			parent.block = scope.inf.block;
			break;
		case SC_FUNCTION:
			parent_type = BP_FUNC;
			parent.function = scope.inf.function;
			break;
		default:
			BOOST_ASSERT(false);
	}
}

int PlnBlock::totalStackSize()
{
	int sz;
	int maxsz = 0;
	for (auto v: reverse(variables)) {
		if (v->alloc_type == VA_STACK) {
			maxsz = v->inf.stack.pos_from_base;
			break;
		}
	}
	
	for (auto s: statements) {
		if (s->type == ST_BLOCK) {
			if ((sz = s->inf.block->totalStackSize()) > maxsz) {
				maxsz = sz;
			}
		}
	}
	return maxsz;
}

PlnVariable* PlnBlock::declareVariable(string& var_name, PlnType* var_type)
{
	for (auto v: variables)
		if (v->name == var_name) return NULL;
	
	PlnVariable* v = new PlnVariable();
	v->name = var_name;

	if (var_type) v->var_type = var_type;
	else v->var_type = variables.back()->var_type;

	v->alloc_type = VA_UNKNOWN;
	variables.push_back(v);

	return v;
}

void PlnBlock::finish()
{
	cur_stack_size = 0;
	for (auto s: statements)
		s->finish();
}

void PlnBlock::dump(ostream& os, string indent)
{
	os << indent << "Block: " << statements.size() << endl;
	for (auto v: variables)
		os << format("%1% Variable: %2% %3%(%4%)")
				% indent % v->var_type->name % v->name % v->inf.stack.pos_from_base << endl;

	for (auto s: statements)
		s->dump(os, indent+" ");
}

void PlnBlock::gen(PlnGenerator& g)
{
	for (auto s: statements)
		s->gen(g);
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
			if (i) ++i; else i = function->parameters.size()+1;
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

