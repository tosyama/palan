/// PlnCmpOperation model class declaration.
///
/// @file	PlnCmpOperation.h
/// @copyright	2018 YAMAGUCHI Toshinobu 

#include "../PlnExpression.h"
#include "../../PlnConstants.h"

class PlnCmpExpression : public PlnExpression
{
public:
	int gen_cmp_type;

	PlnCmpExpression(PlnExprsnType type) : PlnExpression(type), gen_cmp_type(-1) { };
	int getCmpType() { return gen_cmp_type; };
};

class PlnCmpOperation : public PlnCmpExpression
{
public:
	PlnCmpType cmp_type;

	PlnDataPlace* result_dp;
	PlnExpression* l;
	PlnExpression* r;

	PlnCmpOperation(PlnExpression* l, PlnExpression* r, PlnCmpType cmp_type);
	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void dump(ostream& os, string indent="") override;
	void gen(PlnGenerator& g) override;
	bool isConst();
};

