/// Object Array value class difinition.
///
/// @file	PlnArrayValue.cpp
/// @copyright	2019 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include "../PlnType.h"
#include "PlnArrayValue.h"
#include "../types/PlnArrayValueType.h"
#include "../PlnObjectLiteral.h"

static PlnArrayValue* convertArrLit2Value(PlnArrayLiteral* arr_lit)
{
	for (auto& exp: arr_lit->exps) {
		if (exp->values[0].type == VL_LIT_ARRAY) {
			auto newexp = convertArrLit2Value(exp->values[0].inf.arrValue);
			delete exp;
			exp = newexp;
		}
	}
	return new PlnArrayValue(arr_lit->exps);
}

PlnArrayValue::PlnArrayValue(vector<PlnExpression*> &exps)
	: PlnExpression(ET_ARRAYVALUE), item_exps(move(exps))
{
	PlnValue aval;
	aval.type = VL_WORK;
	aval.inf.wk_type = new PlnArrayValueType(this);
	values.push_back(aval);

	// Exchange array lit -> array value
	for (auto& exp: item_exps) {
		if (exp->values[0].type == VL_LIT_ARRAY) {
			auto newexp = convertArrLit2Value(exp->values[0].inf.arrValue);
			delete exp;
			exp = newexp;
		}
	}
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
