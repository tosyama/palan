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
#include "PlnArray.h"
#include "../PlnDataAllocator.h"
#include "../PlnGenerator.h"
#include "../PlnScopeStack.h"

// PlnVarInit
PlnVarInit::PlnVarInit(vector<PlnVariable*>& vars) : vars(move(vars))
{
}

PlnVarInit::PlnVarInit(vector<PlnVariable*>& vars, vector<PlnExpression*> &initializer)
	: vars(move(vars)), initializer(move(initializer))
{
}

void PlnVarInit::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	for (auto v: vars) {
		auto tp = v->var_type.back();
		v->place = da.allocData(tp->size, tp->data_type);
		v->place->comment = &v->name;
		if (v->ptr_type & PTR_OWNERSHIP) {
			da.memAlloced();
			si.push_owner_var(v);
		}
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
	for (auto v: vars) {
		if (v->ptr_type & PTR_OWNERSHIP)
			if (v->var_type.back()->name == "[]") {
				auto t = v->var_type.back();
				auto e = g.getPopEntity(v->place);
				int item_size = t->inf.fixedarray.item_size;
				int asize = 0;
				for (int i: *t->inf.fixedarray.sizes)
					asize += i;
				asize *= item_size;
				g.genMemAlloc(e.get(), asize, v->name);
			} else {
				BOOST_ASSERT(false);	// TODO: need to implement.
			}
	}

	for (auto i: initializer)
		i->gen(g);
}
