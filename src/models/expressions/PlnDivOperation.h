/// PlnDivOperation model class declaration.
///
/// @file	PlnDivOperation.h
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include "../PlnExpression.h"

enum PlnDivType {
	DV_DIV,
	DV_MOD
};

class PlnDivOperation : public PlnExpression
{
public:
	PlnExpression *l, *r;
	PlnDataPlace *quotient, *remainder;
	PlnDivType div_type;

	PlnDivOperation(PlnExpression* l, PlnExpression* r, PlnDivType dt);
	~PlnDivOperation();

	static PlnExpression* create(PlnExpression* l, PlnExpression* r);
	static PlnExpression* create_mod(PlnExpression* l, PlnExpression* r);
	
	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void gen(PlnGenerator& g) override;
};

