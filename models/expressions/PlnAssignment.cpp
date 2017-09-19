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
PlnAssignment::PlnAssignment(vector<PlnValue>& lvals, vector<PlnExpression*>& exps)
	: PlnExpression(ET_ASSIGN), expressions(move(exps))
{
	values = move(lvals);
}

void PlnAssignment::finish(PlnDataAllocator& da)
{
	int i=0;
	for (auto e: expressions) {
		for (auto v: e->values) {
			if (i >= values.size()) break;
			auto dp = values[i].inf.var->place;
			e->data_places.push_back(dp);
			i++;
		}
		e->finish(da);
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
	for (auto e: expressions)
		e->gen(g);
	PlnExpression::gen(g);
}
