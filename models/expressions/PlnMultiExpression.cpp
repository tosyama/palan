/// Multiple expression model class definition.
///
/// PlnMultiExpression integrate plural expressions
/// into one expresion
/// e.g.) = a+1, func(x)
///
/// @file	PlnMultirEexpression.cpp
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include "PlnMultiExpression.h"
#include "../../PlnGenerator.h"
#include <string>

PlnMultiExpression::PlnMultiExpression()
	: PlnExpression(ET_MULTI)
{
}

PlnMultiExpression::PlnMultiExpression(
	PlnExpression* first, PlnExpression* second)
	: PlnExpression(ET_MULTI)
{
	values = first->values;
	exps.push_back(first);
	append(second);
}

void PlnMultiExpression::append(PlnExpression* exp)
{
	values.insert(values.end(), exp->values.begin(), exp->values.end());
	exps.push_back(exp);
}

void PlnMultiExpression::finish(PlnDataAllocator& da)
{
	int i=0;
	for (auto exp: exps) {
		for (auto v: exp->values) {
			exp->data_places.push_back(data_places[i]);
			exp->finish(da);

			i++;
		}
	}
}

void PlnMultiExpression::dump(ostream& os, string indent)
{
	os << indent << "MultiExpression: " << exps.size() << endl;
	for (auto e: exps)
		e->dump(os, indent+" ");
}

void PlnMultiExpression::gen(PlnGenerator& g)
{
	for (auto e: exps)
		e->gen(g);
}

