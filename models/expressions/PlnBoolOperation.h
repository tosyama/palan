/// PlnBoolOperation model class declaration.
///
/// @file	PlnBoolOperation.h
/// @copyright	2018 YAMAGUCHI Toshinobu 

#include "../PlnExpression.h"

class PlnCmpOperation;

class PlnAndOperation : public PlnExpression
{
public:
	PlnDataPlace* result_dp;
	PlnCmpOperation* l;
	PlnCmpOperation* r;

	PlnAndOperation(PlnExpression* l, PlnExpression* r);
	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void dump(ostream& os, string indent="") override;
	void gen(PlnGenerator& g) override;
};

