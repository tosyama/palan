/// PlnArrayValue model class declaration.
///
/// @file	PlnArrayValue.h
/// @copyright	2018 YAMAGUCHI Toshinobu 

#include "../PlnExpression.h"

enum PlnArrayValueType {
	AVT_OBJ_ARRAY,
	AVT_INT_LIT_ARRAY
};

class PlnArrayValue : public PlnExpression {
public:
	vector<PlnExpression*> elements;
	vector<PlnType*> element_type;
	PlnArrayValueType arrval_type;
	PlnDataPlace *test_dp;

	PlnArrayValue(vector<PlnExpression*> &elements);

	void setVarType(vector<PlnType*> var_type);

	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void gen(PlnGenerator& g) override;
};


