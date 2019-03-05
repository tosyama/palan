/// Object Array value class difinition.
///
/// @file	PlnArrayValue.cpp
/// @copyright	2019 YAMAGUCHI Toshinobu 

#include "../PlnType.h"
#include "PlnArrayValue.h"
#include "../types/PlnArrayValueType.h"

PlnArrayValue::PlnArrayValue(vector<PlnExpression*> &exps)
	: PlnExpression(ET_ARRAYVALUE), item_exps(move(exps))
{
	PlnValue aval;
	aval.type = VL_WORK;
	aval.inf.wk_type = new PlnArrayValueType(this);
	values.push_back(aval);
}

void PlnArrayValue::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	for (auto exp: item_exps)
		exp->finish(da, si);
}

void PlnArrayValue::gen(PlnGenerator& g)
{
	for (auto exp: item_exps)
		exp->gen(g);
}
