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
	vector<PlnDataPlace*> arg_dps;
	vector<int> clone_size;
	vector<PlnDataPlace*> ret_dps;
	vector<PlnDataPlace*> free_dps;

	PlnFunctionCall(PlnFunction* f, vector<PlnExpression*>& args);

	void finish(PlnDataAllocator& da);	// override
	void dump(ostream& os, string indent="");	// override
	void gen(PlnGenerator& g);	// override
};

