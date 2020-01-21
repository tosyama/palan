/// Variable model class declaration.
///
/// @file	PlnVariable.h
/// @copyright	2017-2019 YAMAGUCHI Toshinobu 

#include "../PlnModel.h"

class PlnVariable {
public:
	string name;
	PlnVarType* var_type;
	PlnDataPlace* place;
	PlnVariable* container;	// for indirect variable. a[2] -> container is a.
	bool is_tmpvar;
	bool is_indirect;
	bool is_global;

	PlnLoc loc;

	PlnVariable(): var_type(NULL), place(NULL), container(NULL),
		is_tmpvar(false), is_indirect(false), is_global(false) {}

	static PlnVariable* createTempVar(PlnDataAllocator& da, PlnVarType* var_type, string name);
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
