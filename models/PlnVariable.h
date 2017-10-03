#include "../PlnModel.h"

class PlnVariable {
public:
	PlnType *var_type;
	string name;
	PlnDataPlace* place;
	union {
		int index;
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

	void finish(PlnDataAllocator& da);
	void gen(PlnGenerator& g);
};
