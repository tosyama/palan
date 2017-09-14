#include "../PlnModel.h"

class PlnVariable {
public:
	PlnType *var_type;
	string name;
	PlnDataPlace* place;
	union {
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

	void finish(PlnDataAllocator& da);
	void gen(PlnGenerator& g);
};
