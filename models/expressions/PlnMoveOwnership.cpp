/// PlnMoveOwnership model class definition.
///
/// @file	PlnMoveOwnership.cpp
/// @copyright	2017 YAMAGUCHI Toshinobu 

#include "PlnMoveOwnership.h"

PlnMoveOwnership::PlnMoveOwnership(PlnExpression* e)
	: PlnExpression(ET_MVOWN), owner_var(e)
{
	values = e->values;
}

void PlnMoveOwnership::finish(PlnDataAllocator& da)
{
	for (auto dp: data_places) 
		owner_var->data_places.push_back(dp);
	
	owner_var->finish(da);

	// TODO? : freeif deata_pacles num = 0;
}

void PlnMoveOwnership::dump(ostream& os, string indent)
{
	os << indent << "Move Ownership:" << endl;
	owner_var->dump(os, indent+" ");
}

void PlnMoveOwnership::gen(PlnGenerator& g)
{
	owner_var->gen(g);
}

