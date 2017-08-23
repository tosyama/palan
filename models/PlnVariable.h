#include "../PlnModel.h"

// Variable: Type name
enum PlnVarAllocType {
	VA_UNKNOWN,
	VA_STACK,
	VA_RETVAL
};

class PlnVariable {
public:
	PlnVarAllocType alloc_type;
	PlnType *var_type;
	string name;
	union {
		PlnStackItem* stack_item;
		int index;
	} inf;
	unique_ptr<PlnGenEntity> genEntity(PlnGenerator& g);
};

class PlnParameter : public PlnVariable {
public:
	PlnValue* dflt_value;
};

// Variable initialization
class PlnVarInit {
public:
	PlnVarInit(vector<PlnVariable*>& vars, PlnExpression* initializer);

	vector<PlnVariable*> vars;
	PlnExpression* initializer;
	PlnBlock* parent;

	void finish();
	void gen(PlnGenerator& g);
};
