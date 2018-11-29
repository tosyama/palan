/// Function model declaration.
///
/// @file	PlnFunction.h
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include "../PlnModel.h"

enum PlnFncPrntType {
	FP_MODULE
};

enum PlnPassingMethod {
	FPM_COPY,
	FPM_MOVEOWNER,
	FPM_REF
};

class PlnFunction
{
public:
	string name;
	string asm_name;
	int type;
	vector<PlnParameter*> parameters;
	vector<PlnVariable*> return_vals;
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

	PlnFunction(int func_type, const string& func_name);
	void setParent(PlnModule* parent_mod);
	PlnVariable* addRetValue(const string& rname, vector<PlnType*> &rtype, bool do_init);
	PlnParameter* addParam(const string& pname, vector<PlnType*> &ptype, PlnPassingMethod pass_method, PlnValue* defaultVal = NULL);

	void genAsmName();
	void finish(PlnDataAllocator& da, PlnScopeInfo& si);	// throw PlnCompileError;
	void gen(PlnGenerator& g);
	void clear();
};

