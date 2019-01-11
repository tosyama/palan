/// Variable model class declaration.
///
/// @file	PlnVariable.h
/// @copyright	2017-2019 YAMAGUCHI Toshinobu 

#include "../PlnModel.h"

enum {
	NO_PTR = 1,
	PTR_REFERENCE = 2,
	PTR_OWNERSHIP = 4,
	PTR_INDIRECT_ACCESS = 8,	// for class member / array item.
	PTR_CLONE = 16,	// for parameter
	PTR_PARAM_MOVE = PTR_REFERENCE | PTR_OWNERSHIP,
	PTR_PARAM_COPY = PTR_REFERENCE | PTR_OWNERSHIP | PTR_CLONE
};

class PlnVariable {
public:
	vector<PlnType*> var_type;
	string name;
	PlnDataPlace* place;
	PlnVariable* container;	// for indirect variable. a[2] -> container is a.
	int ptr_type;
	PlnLoc loc;

	static PlnVariable* createTempVar(PlnDataAllocator& da, const vector<PlnType*> &var_type, string name);
};

enum {
	PRT_PARAM = 1,
	PRT_RETVAL = 2
};

class PlnParameter : public PlnVariable {
public:
	PlnExpression* dflt_value;
	int param_type;
};

class PlnAssignItem;

// Variable initialization
class PlnVarInit {
	struct VarInitInf {PlnVariable* var; PlnExpression* alloc_ex; };
	vector<VarInitInf> varinits;
	vector<PlnAssignItem*> assgin_items;

public:
	PlnVarInit(vector<PlnValue>& vars, vector<PlnExpression*> *inits=NULL);
	~PlnVarInit();

	void finish(PlnDataAllocator& da, PlnScopeInfo& si);
	void gen(PlnGenerator& g);
};
