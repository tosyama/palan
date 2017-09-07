/// Function model declaration.
///
/// @file	PlnFunction.h
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include "../PlnModel.h"

// Function: Name Paramaters ReturnValues Block
enum PlnFncType {
	FT_PLN,
	FT_INLINE,
	FT_C,
	FT_SYS
};

enum PlnFncPrntType {
	FP_MODULE
};

class PlnFunction
{
public:
	string name;
	PlnFncType type;
	vector<PlnParameter*> parameters;
	vector<PlnVariable*> return_vals;
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

	PlnFunction(PlnFncType func_type, const string& func_name);
	void setParent(PlnModule* parent_mod);
	void setRetValues(vector<PlnVariable*>& vars);
	PlnParameter* addParam(string& pname, PlnType* ptype, PlnValue* defaultVal = NULL);

	void finish(PlnDataAllocator& da);

	void dump(ostream& os, string indent="");
	void gen(PlnGenerator& g);
};

