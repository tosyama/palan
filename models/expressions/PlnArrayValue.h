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

	void finishS(PlnDataAllocator& da, PlnScopeInfo& si);
	void finishD(PlnDataAllocator& da, PlnScopeInfo& si);
	void genS(PlnGenerator& g);
	void genD(PlnGenerator& g);

	static bool isFixedArray(const vector<PlnExpression*> &items, vector<int> &fixarr_sizes, int &item_type, int depth=0);
};
