/// PlnArrayItem model class declaration.
///
/// @file	PlnArrayItem.h
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include "../PlnExpression.h"

class PlnArrayItem : public PlnExpression
{
public:
	PlnExpression* array_ex;
	PlnExpression* index_ex;
	PlnArrayItem(PlnExpression *array_ex, vector<int> item_ind);

	void finish(PlnDataAllocator& da); // override;
	void gen(PlnGenerator& g);	// override
};