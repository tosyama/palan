#include <vector>
#include <string>
#include <iostream>
#include <cstdint>

using std::string;
using std::vector;
using std::ostream;

class PlnGenerator;
class PlnGenEntity;

class PlnScopeItem;

class PlnFunction;
class PlnBlock;
class PlnStatement;
class PlnExpression;
class PlnVariable;
class PlnParameter;
class PlnValue;
class PlnReadOnlyData;

class PlnVarInit;

// Module: Functions
class PlnModule
{
public:
	vector<PlnFunction*> functions;
	vector<PlnReadOnlyData*> readonlydata;
	PlnModule();

	PlnFunction* getFunc(const string& func_name);
	PlnReadOnlyData* getReadOnlyData(string &str);

	int finish();

	void dump(ostream& os, string indent="");
	void gen(PlnGenerator& g);
};

// Function: Name Paramaters ReturnValues Block
enum PlnFncType {
	FT_PLN,
	FT_INLINE,
	FT_SYS,
	FC_C
};

enum PlnFncPrntType {
	FP_MODULE
};

class PlnFunction
{
public:
	string name;
	PlnFncType type;
	vector<PlnParameter*> parameters;
	vector<PlnVariable*> return_vals;
	union {
		struct {
			int id;
		} syscall;
		struct {
			int stack_size;
		} pln;
	} inf;
	PlnBlock* implement;
	PlnFncPrntType parent_type;
	union {
		PlnModule *module;
	} parent;

	PlnFunction(PlnFncType func_type, const string& func_name);
	void addParam(PlnParameter& param);
	void setParent(PlnScopeItem& scope);
	int finish();

	void dump(ostream& os, string indent="");
	void gen(PlnGenerator& g);
};

// Block: Statements
enum PlnBlkPrntType {
	BP_FUNC,
	BP_BLOCK
};

class PlnBlock {
public:
	vector<PlnStatement*> statements;
	vector<PlnVariable*> variables;
	PlnBlkPrntType parent_type;
	union {
		PlnFunction* function;
		PlnBlock* block;
	} parent;
	int cur_stack_size;

	PlnBlock();

	int totalStackSize();
	PlnVariable* getVariable(string& var_name);

	int declareVariable(string& var_name, string type_name="");
	void setParent(PlnScopeItem& scope);

	void dump(ostream& os, string indent="");
	void gen(PlnGenerator& g);
};

// Statement: Expression | Block
enum PlnStmtType {
	ST_EXPRSN,
	ST_VARINIT,
	ST_BLOCK,
	ST_RETURN
};

class PlnStatement {
public:
	PlnStmtType type;
	PlnBlock* parent;
	union {
		PlnExpression* expression;
		vector<PlnVarInit*> *var_inits;
		PlnBlock *block;
		vector<PlnExpression*> *return_vals;
	} inf;

	bool isEmpty();

	void dump(ostream& os, string indent="");
	void gen(PlnGenerator& g);
};

// Value (rval)
enum PlnValType {
	VL_LIT_INT8,
	VL_RO_DATA,
	VL_VAR
};

class PlnValue {
public:
	PlnValType type;
	union {
		int64_t intValue;
		PlnReadOnlyData* rod;
		PlnVariable* var;
	} inf;
	PlnGenEntity* genEntity(PlnGenerator& g);
};

// Expression: FunctionCall
enum PlnExprsnType {
	ET_VALUE,
	ET_FUNCCALL,
	ET_ASSIGN
};

class PlnExpression {
public:
	PlnExprsnType type;
	vector<PlnValue> values;

	virtual void dump(ostream& os, string indent="");
	virtual void gen(PlnGenerator& g);
};

// FunctionCall: Function Arguments;
class PlnFunctionCall : public PlnExpression
{
public:
	PlnFunction* function;
	vector<PlnExpression*> arguments;

	void dump(ostream& os, string indent="");	// override
	void gen(PlnGenerator& g);	// override
};

// Variable: Type name
enum PlnVarType {
	VT_INT8,
	VT_UINT8,
	VT_OBJ,
	VT_IMP,
};

enum PlnVarAllocType {
	VA_STACK
};

class PlnVariable {
public:
	PlnVarType type;
	string name;
	PlnVarAllocType alloc_type;
	union {
		struct {
			int pos_from_base;
		} stack;
	} inf;

	PlnGenEntity* genEntity(PlnGenerator& g);
};

class PlnParameter : public PlnVariable {
public:
	bool has_default;
	PlnValue* dflt_value;
};

// Read only data (String literal/Const)
enum PlnRodType {
	RO_LIT_STR
};

class PlnReadOnlyData {
public:
	PlnRodType type;
	int index;
	string name;
	void gen(PlnGenerator& g);
	PlnGenEntity* genEntity(PlnGenerator& g);
};

// Variable initialization
class PlnVarInit {
public:
	vector<PlnVariable*> vars;
	PlnExpression* initializer;

	void gen(PlnGenerator& g);
};
