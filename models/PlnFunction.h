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
	FPM_MOVEOWNER
};

class PlnFunction
{
public:
	string name;
	int type;
	vector<PlnParameter*> parameters;
	vector<PlnVariable*> return_vals;
	PlnVarInit *retval_init;
	vector<int> save_regs;
	vector<PlnDataPlace*> save_reg_dps;
	
	union {
		struct {
			int id;
		} syscall;
		struct {
			int stack_size;
		} pln;
	} inf;
	PlnBlock* implement;
	PlnFncPrntType parent_type;
	union {
		PlnModule *module;
	} parent;

	PlnFunction(int func_type, const string& func_name);
	void setParent(PlnModule* parent_mod);
	void setRetValues(vector<PlnVariable*>& vars);
	PlnParameter* addParam(string& pname, vector<PlnType*>* ptype, PlnPassingMethod pass_method, PlnValue* defaultVal = NULL);

	void finish(PlnDataAllocator& da, PlnScopeInfo& si);

	void dump(ostream& os, string indent="");
	void gen(PlnGenerator& g);
};

