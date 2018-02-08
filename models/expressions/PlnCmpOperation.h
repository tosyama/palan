/// PlnCmpOperation model class declaration.
///
/// @file	PlnCmpOperation.h
/// @copyright	2018 YAMAGUCHI Toshinobu 

#include "../PlnExpression.h"
#include "../../PlnConstants.h"

class PlnCmpOperation : public PlnExpression
{
	PlnCmpType cmp_type;
	int gen_cmp_type;

public:
	PlnExpression* l;
	PlnExpression* r;

	PlnCmpOperation(PlnExpression* l, PlnExpression* r, PlnCmpType cmp_type);
	void finish(PlnDataAllocator& da);	// override
	void dump(ostream& os, string indent="");	// override
	void gen(PlnGenerator& g);	// override
	bool isConst();
	int getCmpType();
};

