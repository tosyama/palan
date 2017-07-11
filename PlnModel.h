#include <vector>
#include <string>
#include <iostream>

using std::string;
using std::vector;

class PlnGenerator;

class PlnValue {
};

enum PlnVarType {
	VT_INT,
	VT_OBJ,
	VT_IMP,
};

class PlnVariable {
public:
	PlnVarType type;
	string name;
	union {
		struct {
			bool has_default;
			PlnValue* dflt_value;
		} param;
	} inf;
};

class PlnStatement {
};

class PlnBlock {
public:
	vector<PlnStatement*> statemants;
};

enum PlnFncType {
	FT_PLN,
	FT_INLINE,
	FT_SYS,
	FC_C
};

class PlnFunction
{
public:
	string name;
	PlnFncType type;
	vector<PlnVariable*> parameters;
	vector<PlnVariable*> return_vals;
	union {
		struct {
			int id;
		} syscall;
	} inf;
	PlnBlock* implement;

	PlnFunction(const string& func_name);
	void addParam(PlnVariable& param);
	void gen(PlnGenerator& g);
};

class PlnModule
{
	bool is_main;
	vector<PlnFunction*> functions;
public:
	PlnModule();
	void addFunc(PlnFunction& func);
	void gen(PlnGenerator& g);
};

