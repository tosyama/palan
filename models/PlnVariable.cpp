/// Variable model class definition.
///
/// PlnVariable model manage variable information
/// such as type and momory allocation.
///
/// @file	PlnVariable.cpp
/// @copyright	2017-2019 YAMAGUCHI Toshinobu 

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
#include "../PlnMessage.h"
#include "../PlnException.h"
#include "expressions/assignitem/PlnAssignItem.h"

// PlnVarInit
static inline PlnExpression *createVarExpression(PlnValue &val, PlnExpression* ex)
{
	if (val.asgn_type == ASGN_COPY) {
		PlnVariable* v = val.inf.var;
		if (v->ptr_type & PTR_OWNERSHIP && ex->type == ET_FUNCCALL) {
			val.asgn_type = ASGN_MOVE;
		}
	}

	return new PlnExpression(val);
}

PlnVarInit::PlnVarInit(vector<PlnValue>& vars, vector<PlnExpression*> *inits)
{
	int var_i=0;
	if (inits)
		for (auto &ex: *inits) {
			if (var_i >= vars.size()) {
				// No any more assign.
				PlnCompileError err(E_NumOfLRVariables);
				err.loc = ex->loc;
				throw err;
			}
			PlnAssignItem* ai = PlnAssignItem::createAssignItem(ex);
			for (int i=0; i<ex->values.size(); ++i) {
				if (var_i < vars.size()) {
					PlnType* src_type = ex->values[i].getType();	
					PlnType* dst_type = vars[var_i].getType();
					if (dst_type->canConvFrom(src_type) == TC_CANT_CONV) {
						delete ai;
						PlnCompileError err(E_IncompatibleTypeInitVar, vars[var_i].inf.var->name);
						err.loc = ex->loc;
						throw err;
					}

					// Note: vars'asgn_type is possible to update in this call. 
					auto var_ex = createVarExpression(vars[var_i], ex);
					ai->addDstEx(var_ex, false);

					++var_i;
				}
			}
			assgin_items.push_back(ai);
		}

	for (int i=0; i < vars.size(); ++i) {
		PlnVariable* v = vars[i].inf.var;
		PlnExpression* alloc_ex = NULL;
		if (v->ptr_type & PTR_OWNERSHIP) {
			if (i >= var_i || vars[i].asgn_type == ASGN_COPY) {
				alloc_ex = PlnAllocator::getAllocEx(v);
			}
		}
		varinits.push_back({v, alloc_ex});
	}
}

PlnVarInit::~PlnVarInit()
{
	for (auto vi: varinits)
		if (vi.alloc_ex)
			delete vi.alloc_ex;

	for (auto ai: assgin_items)
		delete ai;
}

void PlnVarInit::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	// alloc memory.
	int i=0;
	for (auto vi: varinits) {
		PlnVariable *v = vi.var;
		auto t = v->var_type.back();
		v->place = da.allocData(t->size, t->data_type);
		v->place->comment = &v->name;
		if (v->ptr_type & PTR_OWNERSHIP) {
			si.push_owner_var(v);
		}

		if (auto ex = varinits[i].alloc_ex) {
			PlnDataPlace* dp = da.getSeparatedDp(v->place);
			ex->data_places.push_back(dp);
			ex->finish(da, si);
			da.popSrc(dp);
			if (v->ptr_type & PTR_OWNERSHIP) {
				si.set_lifetime(v, VLT_ALLOCED);
			}
		}

		++i;
	}

	// initialze.
	for (auto ai: assgin_items) {
		ai->finishS(da, si);
		ai->finishD(da, si);
	}
}

void PlnVarInit::gen(PlnGenerator& g)
{
	for (auto vi: varinits) {
		PlnVariable *v = vi.var;
		if (auto ex = vi.alloc_ex) {
			ex->gen(g);
			g.genLoadDp(ex->data_places[0]);
		}
	}

	for (auto ai: assgin_items) {
		ai->genS(g);
		ai->genD(g);
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

