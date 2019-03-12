/// Object Array value class declaration.
///
/// @file	PlnArrayValue.h
/// @copyright	2019 YAMAGUCHI Toshinobu 

#include "../PlnExpression.h"

class PlnArrayValue : public PlnExpression
{
public:
	vector<PlnExpression *> item_exps;
	vector<PlnDataPlace*> arr_item_dps;

	PlnArrayValue(vector<PlnExpression*> &exps);
	PlnExpression* adjustTypes(const vector<PlnType*> &types) override;
	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void gen(PlnGenerator& g) override;
};
