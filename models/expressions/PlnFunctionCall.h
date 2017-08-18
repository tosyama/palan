/// PlnFunctionCall model class declaration.
///
/// @file	PlnFunctionCall.h
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include "../PlnExpression.h"

// FunctionCall: Function Arguments;
class PlnFunctionCall : public PlnExpression
{
public:
	PlnFunction* function;
	vector<PlnExpression*> arguments;

	PlnFunctionCall(PlnFunction* f, vector<PlnExpression*>& args);

	void finish();	// override
	void dump(ostream& os, string indent="");	// override
	void gen(PlnGenerator& g);	// override
};

