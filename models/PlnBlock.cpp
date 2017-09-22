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
#include "../PlnDataAllocator.h"

using std::endl;
using boost::format;
using boost::adaptors::reverse;

PlnBlock::PlnBlock()
	: parent_func(NULL), parent_block(NULL)
{
}

void PlnBlock::setParent(PlnFunction* f)
{
	parent_func = f;
	parent_block = NULL;
}

void PlnBlock::setParent(PlnBlock* b)
{
	parent_func = b->parent_func;
	parent_block = b;
}

PlnVariable* PlnBlock::declareVariable(string& var_name, PlnType* var_type)
{
	for (auto v: variables)
		if (v->name == var_name) return NULL;
	
	PlnVariable* v = new PlnVariable();
	v->name = var_name;

	if (var_type) v->var_type = var_type;
	else v->var_type = variables.back()->var_type;

	variables.push_back(v);

	return v;
}

PlnVariable* PlnBlock::getVariable(string& var_name)
{
	PlnBlock* b = this;
	for(;;) {
		for (auto v: b->variables)
			if (v->name == var_name)
				return v;
		if (b->parent_block)
			b = b->parent_block;
		else if (b->parent_func) {
			for (auto rv: parent_func->return_vals)
				if (rv->name == var_name) return rv;
			for (auto p: parent_func->parameters)
				if (p->name == var_name) return p;
			return NULL;
		} else
			return NULL;
		// TODO: search grobal.
	}
}

void PlnBlock::finish(PlnDataAllocator& da)
{
	for (auto s: statements)
		s->finish(da);
	
	for (auto v: variables)
		da.releaseData(v->place);
}

void PlnBlock::dump(ostream& os, string indent)
{
	os << indent << "Block: " << statements.size() << endl;
	for (auto v: variables)
		os << format("%1% Variable: %2% %3%(%4%)")
				% indent % v->var_type->name % v->name 
				% v->place->data.stack.offset << endl;

	for (auto s: statements)
		s->dump(os, indent+" ");
}

void PlnBlock::gen(PlnGenerator& g)
{
	for (auto s: statements)
		s->gen(g);
}
