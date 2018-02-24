/// PlnBoolOperation model class declaration.
///
/// @file	PlnBoolOperation.h
/// @copyright	2018 YAMAGUCHI Toshinobu 

#include "PlnCmpOperation.h"

class PlnBoolOperation : public PlnCmpExpression
{
protected:
	int jmp_end_id;
	PlnDataPlace *result_dp, *zero_dp;
	PlnCmpOperation *l, *r;
public:
	PlnBoolOperation(PlnExpression* l, PlnExpression* r, PlnExprsnType type);
	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void dump(ostream& os, string indent="") override;
	void gen(PlnGenerator& g) override;

	static PlnExpression* getNot(PlnExpression *e);
};

