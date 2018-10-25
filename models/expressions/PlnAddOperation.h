/// PlnAddOperation model class declaration.
///
/// @file	PlnAddOperation.h
/// @copyright	2017-2018 YAMAGUCHI Toshinobu 

#include "../PlnExpression.h"

class PlnAddOperation : public PlnExpression
{
public:
	PlnExpression* l;
	PlnExpression* r;
	bool is_add;

	PlnAddOperation(PlnExpression* l, PlnExpression* r, bool is_add=true);
	static PlnExpression* create(PlnExpression* l, PlnExpression* r);
	static PlnExpression* create_sub(PlnExpression* l, PlnExpression* r);
	
	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void gen(PlnGenerator& g) override;
};

class PlnNegative : public PlnExpression
{
	PlnExpression *e;
public:
	static PlnExpression* create(PlnExpression* exp);
	PlnNegative(PlnExpression* e);

	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void gen(PlnGenerator& g) override;
};

