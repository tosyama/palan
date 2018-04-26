/// Variable model class declaration.
///
/// @file	PlnVariable.h
/// @copyright	2017 YAMAGUCHI Toshinobu 

#include "../PlnModel.h"

enum {
	NO_PTR = 0,
	PTR_REFERENCE = 1,
	PTR_OWNERSHIP = 2,
	PTR_INDIRECT_ACCESS = 4,	// for class member / array item.
	PTR_CLONE = 8	// for parameter
};

class PlnVariable {
public:
	vector<PlnType*> var_type;
	string name;
	PlnDataPlace* place;
	PlnVariable* container;	// for indirect variable. a[2] -> container is a.
	int ptr_type;

	static PlnVariable* createTempVar(PlnDataAllocator& da, const vector<PlnType*> &var_type, string name);
};

enum {
	PRT_PARAM = 1,
	PRT_RETVAL = 2
};

class PlnParameter : public PlnVariable {
public:
	PlnValue* dflt_value;
	int param_type;
};

// Variable initialization
class PlnVarInit {
	struct VarInitInf {PlnVariable* var; PlnExpression* alloc_ex; };
	vector<VarInitInf> varinits;

public:
	PlnVarInit(vector<PlnValue>& vars);
	PlnVarInit(vector<PlnValue>& vars, vector<PlnExpression*>& inits);

	vector<PlnValue> vars;
	vector<PlnExpression*> initializer;

	void finish(PlnDataAllocator& da, PlnScopeInfo& si);
	void gen(PlnGenerator& g);
};
