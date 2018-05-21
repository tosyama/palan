/// PlnMulOperation model class declaration.
///
/// @file	PlnMulOperation.h
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include "../PlnExpression.h"

class PlnMulOperation : public PlnExpression
{
public:
	PlnExpression *l, *r;

	PlnMulOperation(PlnExpression* l, PlnExpression* r);
	static PlnExpression* create(PlnExpression* l, PlnExpression* r);
	
	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void gen(PlnGenerator& g) override;
};

