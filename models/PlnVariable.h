/// Variable model class declaration.
///
/// @file	PlnVariable.cpp
/// @copyright	2017 YAMAGUCHI Toshinobu 

#include "../PlnModel.h"

enum {
	NO_PTR,
	PTR_OWNERSHIP,
	PTR_REFERENCE
};

class PlnVariable {
public:
	PlnType *var_type;
	string name;
	PlnDataPlace* place;
	int ptr_type;
	union {
		int index;
		PlnArray *arr;
	} inf;
};

class PlnParameter : public PlnVariable {
public:
	PlnValue* dflt_value;
	PlnDataPlace* load_place;
};

// Variable initialization
class PlnVarInit {
public:
	PlnVarInit(vector<PlnVariable*>& vars);
	PlnVarInit(vector<PlnVariable*>& vars, vector<PlnExpression*>& initializer);

	vector<PlnVariable*> vars;
	vector<PlnExpression*> initializer;
	PlnBlock* parent;

	void finish(PlnDataAllocator& da, PlnScopeInfo& si);
	void gen(PlnGenerator& g);
};
