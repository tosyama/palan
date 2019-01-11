/// PlnArrayValue model class declaration.
///
/// @file	PlnArrayValue.h
/// @copyright	2018-2019 YAMAGUCHI Toshinobu 

#include "../PlnExpression.h"

enum PlnArrayValueType {
	AVT_UNKNOWN,
	AVT_OBJ_ARRAY,
	AVT_INT_LIT_ARRAY,
	AVT_FLO_LIT_ARRAY
};

class PlnArrayValue : public PlnExpression {
public:
	vector<PlnExpression*> elements;
	vector<PlnType*> element_type;
	PlnArrayValueType arrval_type;

	PlnArrayValue(vector<PlnExpression*> &elements);
	PlnArrayValue(const PlnArrayValue& src);

	// 0: not literal, 1: int64 literal, 2: uint64 literal, 3: float literal
	bool isLiteral();
	void setDefaultType(PlnModule* module);

	void adjustType(const vector<PlnType*> &var_type) override;
	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void gen(PlnGenerator& g) override;
};

