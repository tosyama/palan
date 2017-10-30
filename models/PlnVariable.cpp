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
#include "expressions/PlnClone.h"

// PlnVarInit
PlnVarInit::PlnVarInit(vector<PlnVariable*>& vars) : vars(move(vars))
{
}

PlnVarInit::PlnVarInit(vector<PlnVariable*>& vars, vector<PlnExpression*> &inits)
	: vars(move(vars)), initializer(move(inits))
{
	BOOST_ASSERT(initializer.size()); // Should use another constractor.
	int val_num = 0;
	int ii = 0;
	for (auto e: initializer) {
		// insert clone exp when init by variable.
		if (e->type == ET_VALUE && e->values[0].type == VL_VAR) {
			auto src_var = e->values[0].inf.var;
			if (src_var->ptr_type & PTR_REFERENCE) {
				auto cln = new PlnClone(e);
				initializer[ii] = cln;
			}
		}
		++ii;
		val_num += e->values.size();
	}

	// compiler must assure all variable have initializer.
	BOOST_ASSERT(val_num >= vars.size());
}

void PlnVarInit::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	bool do_init = initializer.size()>0;

	for (auto v: vars) {
		auto tp = v->var_type.back();
		v->place = da.allocData(tp->size, tp->data_type);
		v->place->comment = &v->name;
		if (v->ptr_type & PTR_OWNERSHIP) {
			if (!do_init)
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
	bool do_init = initializer.size()>0;
	for (auto v: vars) {
		if (v->ptr_type & PTR_OWNERSHIP)
			if (!do_init) {
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
	}

	for (auto i: initializer)
		i->gen(g);
}
