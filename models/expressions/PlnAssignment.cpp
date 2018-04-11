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
#include "../PlnVariable.h"
#include "../PlnType.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnGenerator.h"
#include "../../PlnConstants.h"
#include "../../PlnScopeStack.h"
#include "../PlnHeapAllocator.h"
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
				ai->addDstEx(this->lvals[dst_i]);
				dst_i++;
			}
		}
		assgin_items.push_back(ai);
	}
}

void PlnAssignment::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	for (auto ai: assgin_items) {
		ai->finishS(da, si);
		ai->finishD(da, si);
	}
}

void PlnAssignment::dump(ostream& os, string indent)
{
	os << indent << "Assign:";
	for (auto lv: values)
		os << " " << lv.inf.var->name;
	os << endl;
	for (auto e: expressions)
		e->dump(os, indent+" ");	
}

void PlnAssignment::gen(PlnGenerator& g)
{
	for (auto ai: assgin_items) {
		ai->genS(g);
		ai->genD(g);
	}
	PlnExpression::gen(g);
}

