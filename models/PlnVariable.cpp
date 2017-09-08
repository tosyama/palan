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
#include "../PlnDataAllocator.h"
#include "../PlnGenerator.h"

//PlnVariable
unique_ptr<PlnGenEntity> PlnVariable::genEntity(PlnGenerator& g)
{
	return g.getEntity(place);
}

// PlnVarInit
PlnVarInit::PlnVarInit(vector<PlnVariable*>& vars, PlnExpression* initializer)
	: vars(move(vars)), initializer(initializer)
{
}

void PlnVarInit::finish(PlnDataAllocator& da)
{
	for (auto v: vars)
		v->place = da.allocData(8);

	if (initializer) {
		PlnReturnPlace rp;
		rp.type = RP_VAR;
		for (auto v: vars) {
			rp.inf.var = v;
			initializer->ret_places.push_back(rp);
		}
		initializer->finish(da);
	}
}

void PlnVarInit::gen(PlnGenerator& g)
{
	if (initializer) {
		initializer->gen(g);
		BOOST_ASSERT(initializer->values.size() >= vars.size());
	}
}

