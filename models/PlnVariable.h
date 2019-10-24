/// Variable model class declaration.
///
/// @file	PlnVariable.h
/// @copyright	2017-2019 YAMAGUCHI Toshinobu 

#include "../PlnModel.h"

enum {
	NO_PTR = 1,
	PTR_REFERENCE = 2,

	PTR_OWNERSHIP = 4,	// for reference

	PTR_INDIRECT_ACCESS = 8,	// for struct member / array item.
	PTR_READONLY = 16,
	PTR_CLONE = 32,	// for parameter
	PTR_PARAM_MOVE = PTR_REFERENCE | PTR_OWNERSHIP,
	PTR_PARAM_COPY = PTR_REFERENCE | PTR_OWNERSHIP | PTR_CLONE
};

class PlnVariable {
public:
	PlnType* var_type2;
	string name;
	PlnVarType* var_type;
	PlnDataPlace* place;
	PlnVariable* container;	// for indirect variable. a[2] -> container is a.
	uint64_t ptr_type;
	bool is_tmpvar;
	PlnLoc loc;

	PlnVariable(): var_type2(NULL), var_type(NULL), place(NULL), container(NULL), is_tmpvar(false) {}

	static PlnVariable* createTempVar(PlnDataAllocator& da, PlnVarType* var_type, string name);
};

enum {
	PRT_PARAM = 1,
	PRT_RETVAL = 2
};

class PlnParameter : public PlnVariable {
public:
	PlnExpression* dflt_value;
	int param_type;
	int iomode;
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
