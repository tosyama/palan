/// PlnFunctionCall model class declaration.
///
/// @file	PlnFunctionCall.h
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include "../PlnExpression.h"

enum PlnInternalFuncType {
	IFUNC_MALLOC,
	IFUNC_FREE,
	IFUNC_NUM
};

// FunctionCall: Function Arguments;
class PlnFunctionCall : public PlnExpression
{
public:
	PlnFunction* function;
	vector<PlnExpression*> arguments;
	vector<PlnDataPlace*> arg_dps;
	vector<int> clone_size;
	vector<PlnDataPlace*> ret_dps;
	vector<PlnVariable*> free_vars;
	vector<PlnExpression*> free_exs;

	PlnFunctionCall(PlnFunction* f, vector<PlnExpression*>& args);

	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void gen(PlnGenerator& g) override;

	static PlnFunction* getInternalFunc(PlnInternalFuncType func_type);
};

