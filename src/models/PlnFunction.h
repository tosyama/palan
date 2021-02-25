/// Function model declaration.
///
/// @file	PlnFunction.h
/// @copyright	2017-2021 YAMAGUCHI Toshinobu 

#include "../PlnModel.h"

enum PlnPassingMethod {
	FPM_UNKNOWN,
	FPM_VAR_COPY,
	FPM_VAR_REF,
	FPM_OBJ_CLONE,
	FPM_OBJ_MOVEOWNER,
	FPM_OBJ_GETOWNER,
	FPM_ANY_IN,
	FPM_ANY_OUT,

	FPM_IN_BYVAL,	// primitive:default, object:#
	FPM_IN_BYREF,	// primitive:@, object:@
	FPM_IN_BYREF_CLONE,	// object:default
	FPM_IN_BYREF_MOVEOWNER,	// object:>>
	FPM_IN_VARIADIC,	// primitive:byVal, object:byRef
	FPM_OUT_BYREF,	// primitive:default, object:default
	FPM_OUT_BYREFADDR_GETOWNER, // object:>>
	FPM_OUT_VARIADIC, // primitive:byRef, object:byRef
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
	int index;
	int iomode;
	PlnPassingMethod passby;
	PlnPassingMethod passby2;
};

class PlnReturnValue {
public:
	PlnVariable* local_var;
	PlnVarType* var_type;
	bool is_share_with_param;

	PlnReturnValue(PlnVariable *var, PlnVarType* var_type, bool is_share_with_param)
		: local_var(var), var_type(var_type), is_share_with_param(is_share_with_param) {}
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
	int call_count;
	bool generated;
	bool do_opti_regalloc = true;
	bool never_return = false;

	PlnFunction(int func_type, const string& func_name);
	PlnVariable* addRetValue(const string& rname, PlnVarType* rtype);
	PlnVariable* addParam(const string& pname, PlnVarType* ptype, int iomode, PlnPassingMethod pass_method, PlnPassingMethod pass_method2, PlnExpression* defaultVal);

	vector<string> getParamStrs() const;
	vector<PlnDataPlace*> createArgDps();
	vector<PlnDataPlace*> createRetValDps();

	void genAsmName();
	void finish(PlnDataAllocator& da, PlnScopeInfo& si);	// throw PlnCompileError;
	void gen(PlnGenerator& g);
	void clear();
};

