/// PlnArrayItem model class declaration.
///
/// @file	PlnArrayItem.h
/// @copyright	2017-2018 YAMAGUCHI Toshinobu 

#include "../PlnExpression.h"

class PlnArrayItem : public PlnExpression
{
public:
	PlnExpression* array_ex;
	PlnExpression* index_ex;
	PlnArrayItem(PlnExpression *array_ex, vector<PlnExpression*> item_ind);
	PlnArrayItem(PlnExpression *array_ex, vector<PlnExpression*> item_ind,
			vector<PlnType*> arr_type);
	~PlnArrayItem();

	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void gen(PlnGenerator& g) override;
};
