/// Variable model class definition.
///
/// PlnVariable model manage variable information
/// such as type and momory allocation.
///
/// @file	PlnVariable.cpp
/// @copyright	2017 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>

#include "PlnFunction.h"
#include "PlnBlock.h"
#include "PlnExpression.h"
#include "PlnType.h"
#include "PlnVariable.h"
#include "PlnArray.h"
#include "PlnHeapAllocator.h"
#include "../PlnDataAllocator.h"
#include "../PlnGenerator.h"
#include "../PlnScopeStack.h"
#include "expressions/PlnClone.h"

// PlnVarInit
PlnVarInit::PlnVarInit()
{
}

PlnVarInit::PlnVarInit(vector<PlnValue>& vars) : vars(move(vars))
{
	for (auto v: this->vars) {
		BOOST_ASSERT(v.type == VL_VAR);
		PlnHeapAllocator* a;
		a = PlnHeapAllocator::createHeapAllocation(v.inf.var->var_type);
		allocators.push_back(a);
	}
}

PlnVarInit::PlnVarInit(vector<PlnValue>& vars, vector<PlnExpression*> &inits)
	: vars(move(vars)), initializer(move(inits))
{
	BOOST_ASSERT(initializer.size()); // Should use another constractor.

	for (auto v: this->vars) {
		BOOST_ASSERT(v.type == VL_VAR);
		allocators.push_back(NULL);
	}

	int val_num = 0;
	int ii = 0;

	for (auto e: initializer) {
		// insert clone/move owner exp when init by variable.
		if (e->type == ET_VALUE && e->values[0].type == VL_VAR) {
			auto src_var = e->values[0].inf.var;
			if (src_var->ptr_type & PTR_REFERENCE) {
				switch(this->vars[val_num].lval_type) {
					case LVL_COPY:
						initializer[ii] = new PlnClone(e);
						break;
					case LVL_MOVE:
						break;
					defalut:
						BOOST_ASSERT(false);
				} 
			}
		}
		++ii;
		val_num += e->values.size();	// for assertion
	}
	// compiler must assure all variables have initializer.
	BOOST_ASSERT(val_num >= vars.size());

}

void PlnVarInit::addVar(PlnValue var) {
	vars.push_back(var);
	PlnHeapAllocator* a;
	a = PlnHeapAllocator::createHeapAllocation(var.inf.var->var_type);
	allocators.push_back(a);
}

void PlnVarInit::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	bool do_init = (initializer.size() > 0);

	// alloc memory.
	int i=0;
	for (auto val: vars) {
		PlnVariable *v = val.inf.var;
		auto tp = v->var_type.back();
		v->place = da.allocData(tp->size, tp->data_type);
		v->place->comment = &v->name;
		if (v->ptr_type & PTR_OWNERSHIP) {
			si.push_owner_var(v);
		}

		if (auto a = allocators[i]) {
			a->data_places.push_back(val.getDataPlace(da));
			a->finish(da, si);
			da.popSrc(a->data_places[0]);
		}

		++i;
	}

	// initialze.
	i=0;
	for (auto ie: initializer) {
		for (auto ev: ie->values) {
			if (i >= vars.size()) break;
			ie->data_places.push_back(vars[i].inf.var->place);
			i++;
		}
		ie->finish(da, si);
		for (auto sdp: ie->data_places)
			da.popSrc(sdp);
	}
}

void PlnVarInit::gen(PlnGenerator& g)
{
	bool do_init = initializer.size()>0;
	int i=0;
	for (auto val: vars) {
		PlnVariable *v = val.inf.var;
		if (auto a = allocators[i]) {
			a->gen(g);
			g.genLoadDp(a->data_places[0]);
		}
		i++;
	}

	int vi = 0;
	for (auto i: initializer) {
		vector<unique_ptr<PlnGenEntity>> clr_es;
		i->gen(g);
		for (auto dp: i->data_places) {
			g.genLoadDp(dp);
			if (vars[vi].lval_type == LVL_MOVE)
				clr_es.push_back(g.getEntity(dp->src_place));
			vi++;
		}
		if (clr_es.size())
			g.genNullClear(clr_es);
	}
}
