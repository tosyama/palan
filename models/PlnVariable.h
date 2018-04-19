/// Variable model class declaration.
///
/// @file	PlnVariable.cpp
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
};

class PlnParameter : public PlnVariable {
public:
	PlnValue* dflt_value;
	PlnDataPlace* load_place;
};

class PlnHeapAllocator;

// Variable initialization
class PlnVarInit {
public:
	PlnVarInit();
	PlnVarInit(vector<PlnValue>& vars);
	PlnVarInit(vector<PlnValue>& vars, vector<PlnExpression*>& inits);

	vector<PlnValue> vars;
	vector<PlnExpression*> initializer;
	vector<PlnHeapAllocator*> allocators;

	void addVar(PlnValue var);

	void finish(PlnDataAllocator& da, PlnScopeInfo& si);
	void gen(PlnGenerator& g);
};
