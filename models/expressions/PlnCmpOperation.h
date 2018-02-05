/// PlnCmpOperation model class declaration.
///
/// @file	PlnCmpOperation.h
/// @copyright	2018 YAMAGUCHI Toshinobu 

#include "../PlnExpression.h"

enum PlnCmpType {
	CMP_NE
};

class PlnCmpOperation : public PlnExpression
{
public:
	PlnExpression* l;
	PlnExpression* r;
	PlnCmpType cmp_type;

	PlnCmpOperation(PlnExpression* l, PlnExpression* r, PlnCmpType cmp_type);
	void finish(PlnDataAllocator& da);	// override
	void dump(ostream& os, string indent="");	// override
	void gen(PlnGenerator& g);	// override
};

