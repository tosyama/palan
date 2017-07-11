#include <vector>
#include <string>
#include <iostream>

using std::string;
using std::vector;

class PlnGenerator;
class PlnFunction;
class PlnBlock;
class PlnStatement;
class PlnExpression;
class PlnVariable;
class PlnValue;

// Module: Functions
class PlnModule
{
	bool is_main;
	vector<PlnFunction*> functions;
public:
	PlnModule();
	void addFunc(PlnFunction& func);
	void gen(PlnGenerator& g);
};

// Function: Name Paramaters ReturnValues Block
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

// Block: Statements
class PlnBlock {
public:
	vector<PlnStatement*> statemants;
};


// Statement: Expression | Block
enum PlnStmtType {
	ST_EXPRSN,
	ST_BLOCK
};

class PlnStatement {
public:
	PlnStmtType type;
	union {
		PlnExpression* expression;
		PlnBlock *block;
	} inf;
};

// Expression: FunctionCall
enum PlnExprsnType {
	ET_FUNCCALL,
};

class PlnExpression {
public:
	PlnExprsnType type;
};

// FunctionCall: Function arguments;
class PlnFunctionCall : public PlnExpression
{
public:
	PlnFunction* function;
	vector<PlnExpression*> arguments;
};

// Variable: Type name
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

// Value (rval)
class PlnValue {
};

