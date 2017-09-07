/// Assignment model class definition.
///
/// PlnAssignment store values to variables.
/// e.g.) a = 2;
///
/// @file	PlnAssignment.cpp
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include "PlnAssignment.h"
#include "../PlnVariable.h"
#include "../../PlnGenerator.h"

// PlnAssignment
PlnAssignment::PlnAssignment(vector<PlnValue>& lvals, PlnExpression* exp)
	: PlnExpression(ET_ASSIGN), expression(exp)
{
	values = move(lvals);
}

void PlnAssignment::finish(PlnDataAllocator& da)
{
	PlnReturnPlace rp;
	rp.type = RP_VAR;
	for (auto lv: values) {
		rp.inf.var = lv.inf.var;
		expression->ret_places.push_back(rp);
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
