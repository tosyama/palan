/// PlnMultiExpression model class declaration.
///
/// @file	PlnMultiExpression.h
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include "../PlnExpression.h"

// MultiExpression
class PlnMultiExpression : public PlnExpression
{
public:
	vector<PlnExpression*> exps;

	PlnMultiExpression();
	PlnMultiExpression(PlnExpression* first, PlnExpression *second);
	void append(PlnExpression *exp);

	void finish(PlnDataAllocator& da);	// override
	void dump(ostream& os, string indent="");	// override
	void gen(PlnGenerator& g);	// override
};

