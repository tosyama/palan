/// Variable model class definition.
///
/// PlnVariable model manage variable information
/// such as type and momory allocation.
///
/// @file	PlnVariable.cpp
/// @copyright	2017- YAMAGUCHI Toshinobu 
#include <boost/assert.hpp>

#include "PlnFunction.h"
#include "PlnBlock.h"
#include "PlnExpression.h"
#include "PlnType.h"
#include "PlnVariable.h"
#include "../PlnGenerator.h"

inline int getBasePos(PlnBlock *b)
{
	BOOST_ASSERT(b);
	int pos = b->cur_stack_size;
	while (b->parent_type == BP_BLOCK) {
		b = b->parent.block;
		pos += b->cur_stack_size;
	}
	BOOST_ASSERT(b->parent_type == BP_FUNC);
	return b->parent.function->inf.pln.stack_size + pos;
}

//PlnVariable
PlnGenEntity* PlnVariable::genEntity(PlnGenerator& g)
{
	if (alloc_type == VA_STACK)
		return g.getStackAddress(inf.stack.pos_from_base);
	else if (alloc_type == VA_RETVAL)
		return g.getArgument(inf.index);

	BOOST_ASSERT(false);
	return NULL;
}

// PlnVarInit
PlnVarInit::PlnVarInit(vector<PlnVariable*>& vars, PlnExpression* initializer)
	: vars(move(vars)), initializer(initializer)
{
}

void PlnVarInit::finish()
{
	for (auto v: vars)
		if (v->alloc_type == VA_UNKNOWN) {
			v->alloc_type = VA_STACK;
			parent->cur_stack_size += v->var_type->size;
			v->inf.stack.pos_from_base = getBasePos(parent);
		}

	if (initializer) {
		PlnReturnPlace rp;
		rp.type = RP_VAR;
		for (auto v: vars) {
			rp.inf.var = v;
			initializer->ret_places.push_back(rp);
		}
		initializer->finish();
	}
}

void PlnVarInit::gen(PlnGenerator& g)
{
	if (initializer) {
		initializer->gen(g);
		BOOST_ASSERT(initializer->values.size() >= vars.size());
	}
}

