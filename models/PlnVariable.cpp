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

// PlnVariable
unique_ptr<PlnGenEntity> PlnVariable::genEntity(PlnGenerator& g)
{
	return g.getEntity(place);
}

// PlnVarInit
PlnVarInit::PlnVarInit(vector<PlnVariable*>& vars) : vars(move(vars))
{
}

PlnVarInit::PlnVarInit(vector<PlnVariable*>& vars, vector<PlnExpression*> &initializer)
	: vars(move(vars)), initializer(move(initializer))
{
}

void PlnVarInit::finish(PlnDataAllocator& da)
{
	for (auto v: vars) {
		auto tp = v->var_type;
		v->place = da.allocData(tp->size, tp->data_type);
		v->place->comment = &v->name;
	}
	int i=0;
	for (auto ie: initializer) {
		for (auto ev: ie->values) {
			if (i >= vars.size()) break;
			ie->data_places.push_back(vars[i]->place);
			i++;
		}
		ie->finish(da);
	}
}

void PlnVarInit::gen(PlnGenerator& g)
{
	for (auto i: initializer)
		i->gen(g);
}
