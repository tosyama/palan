/// PlnMulOperation model class declaration.
///
/// @file	PlnMulOperation.h
/// @copyright	2017-2019 YAMAGUCHI Toshinobu 

#include "../PlnExpression.h"

class PlnMulOperation : public PlnExpression
{
public:
	PlnExpression *l, *r;
	PlnDataPlace *ldp, *rdp;
	bool do_cross;

	PlnMulOperation(PlnExpression* l, PlnExpression* r);
	~PlnMulOperation();

	static PlnExpression* create(PlnExpression* l, PlnExpression* r);
	
	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void gen(PlnGenerator& g) override;
};

