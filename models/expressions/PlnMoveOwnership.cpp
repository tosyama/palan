/// PlnMoveOwnership model class definition.
/// Clear variable after pass it's ownership.
/// 
/// @file	PlnMoveOwnership.cpp
/// @copyright	2017 YAMAGUCHI Toshinobu 

#include "PlnMoveOwnership.h"
#include "../PlnExpression.h"
#include "../PlnVariable.h"
#include "../../PlnGenerator.h"
#include <boost/assert.hpp>

PlnMoveOwnership::PlnMoveOwnership(PlnExpression* e)
	: PlnExpression(ET_MVOWN), owner_var(e)
{
	values = e->values;

	BOOST_ASSERT(values.size() >= 1);
	// Should be compile error if not variable.
	BOOST_ASSERT(owner_var->values[0].type == VL_VAR);
}

void PlnMoveOwnership::finish(PlnDataAllocator& da)
{
	BOOST_ASSERT(data_places.size() <= 1);
	for (auto dp: data_places) 
		owner_var->data_places.push_back(dp);
	
	owner_var->finish(da);
	// TODO: check clear the var is safe?
}

void PlnMoveOwnership::dump(ostream& os, string indent)
{
	os << indent << "Move Ownership:" << endl;
	owner_var->dump(os, indent+" ");
}


void PlnMoveOwnership::gen(PlnGenerator& g)
{
	owner_var->gen(g);
	vector<unique_ptr<PlnGenEntity>> ve;
	ve.push_back(g.getPopEntity(owner_var->values[0].inf.var->place));
	g.genNullClear(ve);
}

