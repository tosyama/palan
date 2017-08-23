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
#include "PlnStack.h"
#include "../PlnGenerator.h"

//PlnVariable
unique_ptr<PlnGenEntity> PlnVariable::genEntity(PlnGenerator& g)
{
	if (alloc_type == VA_STACK) {
		return g.getStackAddress(inf.stack_item->pos_from_base);
	} else if (alloc_type == VA_RETVAL)
		return g.getArgument(inf.index);
	else 
		BOOST_ASSERT(false);
}

// PlnVarInit
PlnVarInit::PlnVarInit(vector<PlnVariable*>& vars, PlnExpression* initializer)
	: vars(move(vars)), initializer(initializer)
{
}

void PlnVarInit::finish()
{
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

