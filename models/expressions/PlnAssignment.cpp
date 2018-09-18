/// Assignment model class definition.
///
/// PlnAssignment store values to variables.
/// e.g.) 2 -> a;
///
/// @file	PlnAssignment.cpp
/// @copyright	2017 YAMAGUCHI Toshinobu 

#include <iostream>
#include <boost/assert.hpp>
#include "PlnAssignment.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnGenerator.h"
#include "../../PlnConstants.h"
#include "../../PlnScopeStack.h"
#include "../../PlnMessage.h"
#include "../../PlnException.h"
#include "../PlnVariable.h"
#include "../PlnType.h"
#include "PlnDivOperation.h"
#include "assignitem/PlnAssignItem.h"

#include "PlnArrayItem.h"

static bool didUpdate(PlnVariable* var, PlnExpression* e) {
	switch (e->type) {
		case ET_VALUE:
			return false;
		case ET_FUNCCALL:
		case ET_CHAINCALL:
		case ET_ADD:
		case ET_MUL:
		case ET_DIV:
		case ET_NEG:
		case ET_CMP:
		case ET_AND:
		case ET_OR:
		case ET_ARRAYITEM:
			return false; // TODO: deep analysys.
		case ET_ASSIGN:
		{
			auto as = static_cast<PlnAssignment*>(e);
			for (auto ex: as->expressions) {
				if (didUpdate(var, ex)) return true;
			}
			for (auto ex: as->lvals) {
				if (ex->type == ET_VALUE) {
					if (ex->values[0].inf.var == var) return true;
				} else {
					BOOST_ASSERT(ex->type == ET_ARRAYITEM);
					if (ex->values[0].inf.var->container == var) return true;

					auto ai = static_cast<PlnArrayItem*>(ex);
					if (didUpdate(var, ai->index_ex)) return true;
				}
			}
			break;
		}
	}
	return false;
}

static bool checkNeedToSave(vector<PlnExpression*> &exps, int exp_ind, vector<PlnExpression*> &lvals, int lval_ind)
{
	if (lvals.size()==1) {
		return false;
	}
	PlnVariable* var;
	PlnExpression* src_ex = exps[exp_ind];
	if (src_ex->type == ET_VALUE && src_ex->values[0].type == VL_VAR) {
		var = src_ex->values[0].inf.var;
	} else if (src_ex->type == ET_ARRAYITEM) {
		var = src_ex->values[0].inf.var->container;
	} else {
		return false;
	}

	while ((++exp_ind) < exps.size()) {
		if (didUpdate(var, exps[exp_ind])) return true;
	}

	while ((--lval_ind) >= 0) {
		PlnExpression* lval = lvals[lval_ind];
		if (lval->type == ET_VALUE) {
			if (lval->values[0].inf.var == var) return true;
		} else {
			BOOST_ASSERT(lval->type == ET_ARRAYITEM);
			if (lval->values[0].inf.var->container == var) return true;

			auto ai = static_cast<PlnArrayItem*>(lval);
			if (didUpdate(var, ai->index_ex)) return true;
		}
	}

	return false;
}

// PlnAssignment
PlnAssignment::PlnAssignment(vector<PlnExpression*>& lvals, vector<PlnExpression*>& exps)
	: lvals(move(lvals)), PlnExpression(ET_ASSIGN), expressions(move(exps))
{
	for (auto e: this->lvals) {
		BOOST_ASSERT(e->values.size() == 1);
		BOOST_ASSERT(e->type == ET_VALUE || e->type == ET_ARRAYITEM);
		BOOST_ASSERT(e->values[0].type == VL_VAR);
		values.push_back(e->values[0]);
	}

	int dst_i = 0;
	int exp_i = 0;
	for (auto ex: expressions) {
		if (!ex->values.size()) {
			PlnCompileError err(E_NumOfLRVariables);
			err.loc = ex->loc;
			throw err;
		}
		PlnAssignItem* ai = PlnAssignItem::createAssignItem(ex);
		for (int i=0; i<ex->values.size(); ++i) {
			if (dst_i < this->lvals.size()) {
				PlnType* src_type = ex->values[i].getType();	
				PlnType* dst_type = this->lvals[dst_i]->values[0].getType();
				if (dst_type->canConvFrom(src_type) == TC_CANT_CONV) {
					delete ai;
					PlnCompileError err(E_IncompatibleTypeAssign, src_type->name, dst_type->name);
					err.loc = ex->loc;
					throw err;
				}

				bool need_save = false;
				if (this->lvals[dst_i]->values[0].asgn_type == ASGN_COPY) {
					need_save = checkNeedToSave(expressions, exp_i, this->lvals, dst_i);
				}

				ai->addDstEx(this->lvals[dst_i], need_save);
				dst_i++;
			}
		}
		assgin_items.push_back(ai);
		exp_i++;
	}

	if (dst_i < this->lvals.size()) {
		throw PlnCompileError(E_NumOfLRVariables);
		BOOST_ASSERT(false);
	}
}

void PlnAssignment::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	int start_ind = 0;
	for (auto ai: assgin_items) {
		if (start_ind >= data_places.size())
			break;
		start_ind = ai->addDataPlace(data_places, start_ind);
	}

	for (auto ai: assgin_items)
		ai->finishS(da, si);

	for (auto ai: assgin_items)
		ai->finishD(da, si);
}

void PlnAssignment::gen(PlnGenerator& g)
{
	for (auto ai: assgin_items)
		ai->genS(g);

	for (auto ai: assgin_items)
		ai->genD(g);
}

