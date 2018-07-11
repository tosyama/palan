/// Assignment model class definition.
///
/// PlnAssignment store values to variables.
/// e.g.) a = 2;
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
	for (auto ex: expressions) {
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

				ai->addDstEx(this->lvals[dst_i]);
				dst_i++;
			}
		}
		assgin_items.push_back(ai);
	}

	if (dst_i < this->lvals.size()) {
		throw PlnCompileError(E_NumOfLRVariables);
		BOOST_ASSERT(false);
	}
}

void PlnAssignment::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	for (auto ai: assgin_items) {
		ai->finishS(da, si);
		ai->finishD(da, si);
	}
}

void PlnAssignment::gen(PlnGenerator& g)
{
	for (auto ai: assgin_items) {
		ai->genS(g);
		ai->genD(g);
	}
	PlnExpression::gen(g);
}

