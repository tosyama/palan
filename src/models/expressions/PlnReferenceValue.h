/// PlnReferenceValue model class declaration.
///
/// @file	PlnReferenceValue.h
/// @copyright	2020 YAMAGUCHI Toshinobu 

#include "../PlnExpression.h"

class PlnReferenceValue : public PlnExpression
{
public:
	PlnExpression* refvar_ex;
	PlnReferenceValue(PlnExpression *refvar_ex);
	~PlnReferenceValue();

	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void gen(PlnGenerator& g) override;
};

