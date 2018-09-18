/// PlnFunctionCall model class declaration.
///
/// @file	PlnFunctionCall.h
/// @copyright	2017-2018 YAMAGUCHI Toshinobu 

#include "../PlnExpression.h"

enum PlnInternalFuncType {
	IFUNC_MALLOC,
	IFUNC_FREE,
	IFUNC_EXIT,
	IFUNC_NUM
};

class PlnClone;

// FunctionCall: Function Arguments;
class PlnFunctionCall : public PlnExpression
{
	vector<PlnExpression*> arguments;
	vector<PlnDataPlace*> ret_dps;
	vector<PlnVariable*> free_vars;
	vector<PlnExpression*> free_exs;
	vector<PlnClone*> clones;

public:
	PlnFunction* function;
	vector<PlnDataPlace*> arg_dps;

	PlnFunctionCall(PlnFunction* f);
	PlnFunctionCall(PlnFunction* f, vector<PlnExpression*>& args);

	void loadArgDps(PlnDataAllocator& da, vector<int> arg_data_types);

	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void gen(PlnGenerator& g) override;

	static PlnFunction* getInternalFunc(PlnInternalFuncType func_type);
};

