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
#include "../PlnDataAllocator.h"
#include "../PlnGenerator.h"
#include "../PlnScopeStack.h"
#include "../PlnConstants.h"
#include "expressions/PlnClone.h"

// PlnVarInit
PlnVarInit::PlnVarInit()
{
}

PlnVarInit::PlnVarInit(vector<PlnValue>& vars) : vars(move(vars))
{
	for (auto v: this->vars) {
		BOOST_ASSERT(v.type == VL_VAR);

		varinits.push_back({v.inf.var, NULL});
		PlnType *t = v.inf.var->var_type.back();
		if (t->allocator) {
			varinits.back().alloc_ex = t->allocator->getAllocEx();
		}
	}
}

PlnVarInit::PlnVarInit(vector<PlnValue>& vars, vector<PlnExpression*> &inits)
	: vars(move(vars)), initializer(move(inits))
{
	BOOST_ASSERT(initializer.size()); // Should use another constractor.

	for (auto v: this->vars) {
		BOOST_ASSERT(v.type == VL_VAR);
		varinits.push_back({v.inf.var, NULL});
	}

	int val_num = 0;
	int ii = 0;

	for (auto e: initializer) {
		// insert clone/move owner exp when init by variable.
		if (e->type == ET_VALUE && e->values[0].type == VL_VAR) {
			auto src_var = e->values[0].inf.var;
			if (src_var->ptr_type & PTR_REFERENCE) {
				switch(this->vars[val_num].asgn_type) {
					case ASGN_COPY:
						initializer[ii] = new PlnClone(e);
						break;
					case ASGN_MOVE:
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

// for Use only init return value at PlnFunciton.
void PlnVarInit::addVar(PlnValue var) {
	vars.push_back(var);

	if (var.inf.var->ptr_type & PTR_OWNERSHIP) {
		varinits.push_back({var.inf.var, NULL});
		PlnType *t = var.inf.var->var_type.back();
		if (t->allocator) {
			varinits.back().alloc_ex = t->allocator->getAllocEx();
		}
	} else {
		varinits.push_back({var.inf.var, NULL});
	}
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

		if (auto ex = varinits[i].alloc_ex) {
			PlnDataPlace* dp = val.getDataPlace(da);
			ex->data_places.push_back(dp);
			ex->finish(da, si);
			da.popSrc(dp);
		}

		if (v->ptr_type & PTR_OWNERSHIP) {
			si.set_lifetime(v, VLT_ALLOCED);
		}

		++i;
	}

	// initialze.
	i=0;
	for (auto ie: initializer) {
		int j=i;
		for (auto ev: ie->values) {
			if (i >= vars.size()) break;
			ie->data_places.push_back(vars[i].inf.var->place);
			i++;
		}
		ie->finish(da, si);
		for (auto sdp: ie->data_places) {
			da.popSrc(sdp);
			auto v = vars[j].inf.var;
			if (v->ptr_type & PTR_OWNERSHIP) {
				si.set_lifetime(v, VLT_INITED);
			}
			j++;
		}
	}
}

void PlnVarInit::gen(PlnGenerator& g)
{
	bool do_init = initializer.size()>0;
	int i=0;
	for (auto val: vars) {
		PlnVariable *v = val.inf.var;
		if (auto ex = varinits[i].alloc_ex) {
			ex->gen(g);
			g.genLoadDp(ex->data_places[0]);
		}
		
		i++;
	}

	int vi = 0;
	for (auto i: initializer) {
		vector<unique_ptr<PlnGenEntity>> clr_es;
		i->gen(g);
		for (auto dp: i->data_places) {
			g.genLoadDp(dp);
			if (vars[vi].asgn_type == ASGN_MOVE)
				clr_es.push_back(g.getEntity(dp->src_place));
			vi++;
		}
		if (clr_es.size())
			g.genNullClear(clr_es);
	}
}

PlnVariable* PlnVariable::createTempVar(PlnDataAllocator& da, const vector<PlnType*> &var_type, string name)
{
	auto var = new PlnVariable();
	var->var_type = var_type;
	var->name = name;
	PlnType *t = var_type.back();
	var->place = da.prepareLocalVar(t->size, t->data_type);
	var->place->comment = &var->name;
	var->container = NULL;
	var->ptr_type = (t->data_type == DT_OBJECT_REF) ?
			PTR_REFERENCE : NO_PTR;

	return var;
}

