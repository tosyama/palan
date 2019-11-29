/// PlnFunctionCall model class declaration.
///
/// @file	PlnFunctionCall.h
/// @copyright	2017-2019 YAMAGUCHI Toshinobu 

#include "../PlnExpression.h"

enum PlnInternalFuncType {
	IFUNC_MALLOC,
	IFUNC_FREE,
	IFUNC_EXIT,
	IFUNC_NUM
};

enum PlnArgOption {
	AG_NONE = 0,
	AG_MOVE
};

class PlnClone;

class PlnArgValueInf {
public:
	PlnParameter* param;
	int iomode; 
	PlnArgOption opt;
	int va_idx; // not variable argument(va): -1, va: >=0 
	PlnArgValueInf(PlnParameter* param, int iomode)
		: param(param), iomode(iomode), opt(AG_NONE), va_idx(-1) {}
	PlnArgValueInf(int iomode)
		: param(NULL), iomode(iomode), opt(AG_NONE), va_idx(-1) {}
};

class PlnArgument {
public:
	PlnExpression* exp;
	vector<PlnArgValueInf> inf;
	PlnArgument(PlnExpression* exp) : exp(exp) {}
};

// FunctionCall: Function Arguments;
class PlnFunctionCall : public PlnExpression
{
	vector<PlnArgument> arguments;
	vector<PlnDataPlace*> ret_dps;
	vector<PlnVariable*> free_vars;
	vector<PlnExpression*> free_exs;
	vector<PlnClone*> clones;

public:
	PlnFunction* function;
	vector<PlnDataPlace*> arg_dps;

	PlnFunctionCall(PlnFunction* f);
	PlnFunctionCall(PlnFunction* f, vector<PlnArgument> &args);
	PlnFunctionCall(PlnFunction* f, vector<PlnExpression*> &args, vector<PlnExpression*> &out_args);
	~PlnFunctionCall();

	void loadArgDps(PlnDataAllocator& da, vector<int> arg_data_types);

	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void gen(PlnGenerator& g) override;

	static PlnFunction* getInternalFunc(PlnInternalFuncType func_type);
};

