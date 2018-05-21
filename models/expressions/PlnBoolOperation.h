/// PlnBoolOperation model class declaration.
///
/// @file	PlnBoolOperation.h
/// @copyright	2018 YAMAGUCHI Toshinobu 

#include "PlnCmpOperation.h"

class PlnBoolOperation : public PlnCmpExpression
{
protected:
	int jmp_l_id, jmp_r_id;
	PlnDataPlace *result_dp, *zero_dp;
	PlnCmpOperation *l, *r;
public:
	PlnBoolOperation(PlnExpression* l, PlnExpression* r, PlnExprsnType type);

	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void gen(PlnGenerator& g) override;

	static PlnExpression* getNot(PlnExpression *e);
};

