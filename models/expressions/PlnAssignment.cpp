/// Assignment model class definition.
///
/// PlnAssignment store values to variables.
/// e.g.) a = 2;
///
/// @file	PlnAssignment.cpp
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include <iostream>
#include "PlnAssignment.h"
#include "../PlnVariable.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnGenerator.h"

// PlnAssignment
PlnAssignment::PlnAssignment(vector<PlnValue>& lvals, PlnExpression* exp)
	: PlnExpression(ET_ASSIGN), expression(exp)
{
	values = move(lvals);
}

void PlnAssignment::finish(PlnDataAllocator& da)
{
	for (auto lv: values) {
		PlnDataPlace* dp = lv.inf.var->place;
		expression->data_places.push_back(dp);
	}
	expression->finish(da);
}

void PlnAssignment::dump(ostream& os, string indent)
{
	os << indent << "Assign:";
	for (auto lv: values)
		os << " " << lv.inf.var->name;
	os << endl;
	expression->dump(os, indent+" ");	
}

void PlnAssignment::gen(PlnGenerator& g)
{
	expression->gen(g);
	PlnExpression::gen(g);
}
