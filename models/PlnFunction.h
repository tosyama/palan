/// Function model declaration.
///
/// @file	PlnFunction.h
/// @copyright	2017-2019 YAMAGUCHI Toshinobu 

#include "../PlnModel.h"

enum PlnPassingMethod {
	FPM_COPY,
	FPM_MOVEOWNER,
	FPM_REF
};

enum {
	PIO_UNKNOWN = 0,
	PIO_INPUT = 1,
	PIO_OUTPUT = 2,
//	PIO_IO = 3,	// == PIO_INPUT | PIO_OUTPUT unused
};

class PlnParameter {
public:
	PlnVariable* var;
	PlnExpression* dflt_value;
	int iomode;
	bool force_move;
};

class PlnReturnValue {
public:
	PlnVariable* local_var;
	bool is_share_with_param;

	PlnReturnValue(PlnVariable *var, bool is_share_with_param)
		: local_var(var), is_share_with_param(is_share_with_param) {}
};

class PlnFunction
{
public:
	string name;
	string asm_name;
	int type;
	vector<PlnParameter*> parameters;
	vector<PlnReturnValue> return_vals;
	PlnVarInit *retval_init;
	vector<int> ret_dtypes, arg_dtypes;
	vector<int> save_regs;
	vector<PlnDataPlace*> save_reg_dps;
	PlnLoc loc;
	union {
		struct {
			int id;
		} syscall;
		struct {
			int stack_size;
		} pln;
	} inf;
	PlnBlock* implement;
	PlnBlock* parent;
	bool has_va_arg;
	int num_in_param, num_out_param;
	int call_count;
	bool generated;

	PlnFunction(int func_type, const string& func_name);
	PlnVariable* addRetValue(const string& rname, PlnVarType* rtype);
	PlnVariable* addParam(const string& pname, PlnVarType* ptype, int iomode, PlnPassingMethod pass_method, PlnExpression* defaultVal);

	vector<string> getParamStrs() const;

	void genAsmName();
	void finish(PlnDataAllocator& da, PlnScopeInfo& si);	// throw PlnCompileError;
	void gen(PlnGenerator& g);
	void clear();
};

