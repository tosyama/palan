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

void PlnMultiExpression::finish()
{
	int i=0;
	for (auto exp: exps) {
		for (auto v: exp->values) {
			exp->ret_places.push_back(ret_places[i]);
			exp->finish();
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
	for (auto e: exps) {
		e->gen(g);
	}

	int i=0;
	for (auto exp: exps)
		for (auto rp: exp->ret_places) {
			PlnGenEntity* re = rp.genEntity(g);
			PlnGenEntity* le = ret_places[i].genEntity(g);
			g.genMove(le, re, ret_places[i].commentStr());
			PlnGenEntity::freeEntity(re);
			PlnGenEntity::freeEntity(le);
			i++;
		}
}

