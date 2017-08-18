/// Block model class definition.
///
/// Block model manage list of statement and
/// local variables.
/// Block: "{" statements "}"
///
/// @file	PlnBlock.cpp
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include <boost/format.hpp>
#include <boost/range/adaptor/reversed.hpp>

#include "PlnFunction.h"
#include "PlnBlock.h"
#include "PlnStatement.h"
#include "PlnType.h"
#include "PlnVariable.h"
#include "../PlnScopeStack.h"

using std::endl;
using boost::format;
using boost::adaptors::reverse;

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

// TODO: do not use PlnScopeItem.
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
