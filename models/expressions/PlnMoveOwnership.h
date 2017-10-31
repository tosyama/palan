/// PlnMoveOwnership model class declaration.
///
/// @file	PlnMoveOwnership.h
/// @copyright	2017 YAMAGUCHI Toshinobu 

#include "../PlnExpression.h"

class PlnMoveOwnership : public PlnExpression {
public:
	PlnExpression* owner_var;
	PlnMoveOwnership(PlnExpression* e);

	void finish(PlnDataAllocator& da);	// override
	void dump(ostream& os, string indent="");	// override
	void gen(PlnGenerator& g);	// override
};

