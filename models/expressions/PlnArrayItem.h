/// PlnArrayItem model class declaration.
///
/// @file	PlnArrayItem.h
/// @copyright	2017-2019 YAMAGUCHI Toshinobu 

#include "../PlnExpression.h"

class PlnArrayItem : public PlnExpression
{
public:
	PlnExpression* array_ex;
	PlnExpression* index_ex;
	PlnArrayItem(PlnExpression *array_ex, vector<PlnExpression*> item_ind);
	PlnArrayItem(PlnExpression *array_ex, vector<PlnExpression*> item_ind, PlnVarType* arr_type);
	PlnArrayItem(PlnExpression *array_ex, PlnExpression* index_ex, PlnVarType* item_type);
	~PlnArrayItem();

	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void gen(PlnGenerator& g) override;

	static vector<PlnExpression*> getAllArrayItems(PlnVariable* var);
};
