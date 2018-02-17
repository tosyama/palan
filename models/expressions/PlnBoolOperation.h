/// PlnBoolOperation model class declaration.
///
/// @file	PlnBoolOperation.h
/// @copyright	2018 YAMAGUCHI Toshinobu 

#include "../PlnExpression.h"

class PlnCmpOperation;

class PlnBoolOperation : public PlnExpression
{
protected:
	int jmp_end_id;
	int lcmp_type, rcmp_type;
	PlnDataPlace *result_dp, *rconst_dp;
	PlnCmpOperation *l, *r;
public:
	PlnBoolOperation(PlnExpression* l, PlnExpression* r, PlnExprsnType type);
	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void dump(ostream& os, string indent="") override;
	void gen(PlnGenerator& g) override;

	static PlnExpression* getNot(PlnExpression *e);
};

