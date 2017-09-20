/// PlnAssignment model class declaration.
///
/// @file	PlnAssignment.h
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include "../PlnExpression.h"

// Assignment: lvals Expression
class PlnAssignment : public PlnExpression
{
public:
	PlnAssignment(vector<PlnValue>& lvals, vector<PlnExpression*>& exps);
	vector<PlnExpression*> expressions;

	void finish(PlnDataAllocator& da);	// override
	void dump(ostream& os, string indent="");	// override
	void gen(PlnGenerator& g);	// override
};

