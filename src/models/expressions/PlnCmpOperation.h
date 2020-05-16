/// PlnCmpOperation model class declaration.
///
/// @file	PlnCmpOperation.h
/// @copyright	2018-2020 YAMAGUCHI Toshinobu 

#include "PlnBoolExpression.h"

class PlnCmpOperation : public PlnBoolExpression
{
public:
	PlnCmpType cmp_type;

	PlnDataPlace* result_dp;
	PlnExpression* l;
	PlnExpression* r;

	PlnCmpOperation(PlnExpression* l, PlnExpression* r, PlnCmpType cmp_type);
	~PlnCmpOperation();

	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void gen(PlnGenerator& g) override;

	static PlnExpression* create(PlnExpression* l, PlnExpression* r, PlnCmpType cmp_type);
};

