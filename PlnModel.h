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
class PlnType;
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

	PlnType* getType(const string& type_name);
	PlnFunction* getFunc(const string& func_name);
	PlnReadOnlyData* getReadOnlyData(string &str);

	void finish();

	void dump(ostream& os, string indent="");
	void gen(PlnGenerator& g);
};

// Function: Name Paramaters ReturnValues Block
enum PlnFncType {
	FT_PLN,
	FT_INLINE,
	FT_C,
	FT_SYS
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
	PlnParameter* addParam(string& pname, PlnType* ptype, PlnValue* defaultVal = NULL);
	void setParent(PlnScopeItem& scope);
	void finish();

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

	PlnVariable* declareVariable(string& var_name, PlnType* var_type=NULL);
	void setParent(PlnScopeItem& scope);

	void finish();
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
		PlnVarInit* var_init;
		PlnBlock *block;
//		vector<PlnExpression*> *return_vals;
	} inf;

	PlnStatement() {};
	PlnStatement(PlnExpression *exp, PlnBlock* parent);
	PlnStatement(PlnVarInit* var_init, PlnBlock* parent);
	PlnStatement(PlnBlock* block, PlnBlock* parent);

	virtual void finish();
	virtual void dump(ostream& os, string indent="");
	virtual void gen(PlnGenerator& g);
};

class PlnReturnStmt : public PlnStatement
{
public:
	PlnFunction *function;
	
	PlnReturnStmt(PlnExpression *retexp, PlnBlock* parent);

	void finish();
	void dump(ostream& os, string indent="");
	void gen(PlnGenerator& g);
};

// Value (rval)
enum PlnValType {
	VL_LIT_INT8,
	VL_WK_INT8,
	VL_RO_DATA,
	VL_VAR,
};

class PlnValue {
public:
	PlnValType type;
	union {
		int index;
		int64_t intValue;
		PlnReadOnlyData* rod;
		PlnVariable* var;
	} inf;

	PlnValue() {};
	PlnValue(int intValue);
	PlnValue(PlnReadOnlyData* rod);
	PlnValue(PlnVariable* var);

	PlnGenEntity* genEntity(PlnGenerator& g);
};

enum PlnRtnPlcType {
	RP_NULL,
	RP_TEMP,
	RP_WORK,
	RP_VAR,
	RP_AS_IS,
	RP_ARGPLN,
	RP_ARGSYS
};

class PlnReturnPlace
{
public:
	PlnRtnPlcType type;
	union {
		int index;
		PlnVariable* var;
		PlnValue* as_is;
	}	inf;

	string commentStr();
	void dump(ostream& os, string indent="");
	PlnGenEntity* genEntity(PlnGenerator& g);
};

// Expression: FunctionCall
enum PlnExprsnType {
	ET_VALUE,
	ET_MULTI,
	ET_FUNCCALL,
	ET_ADD,
	ET_ASSIGN
};

class PlnExpression {
public:
	PlnExprsnType type;
	vector<PlnReturnPlace> ret_places;
	vector<PlnValue> values;

	PlnExpression(PlnExprsnType type) : type(type) {};
	PlnExpression(PlnValue value);

	virtual void finish();
	virtual void dump(ostream& os, string indent="");
	virtual void gen(PlnGenerator& g);
};

// MultiExpression
class PlnMultiExpression : public PlnExpression
{
public:
	vector<PlnExpression*> exps;

	PlnMultiExpression();
	PlnMultiExpression(PlnExpression* first, PlnExpression *second);
	void append(PlnExpression *exp);

	void finish();	// override
	void dump(ostream& os, string indent="");	// override
	void gen(PlnGenerator& g);	// override
};

// FunctionCall: Function Arguments;
class PlnFunctionCall : public PlnExpression
{
public:
	PlnFunction* function;
	vector<PlnExpression*> arguments;

	PlnFunctionCall(PlnFunction* f, vector<PlnExpression*>& args);

	void finish();	// override
	void dump(ostream& os, string indent="");	// override
	void gen(PlnGenerator& g);	// override
};

class PlnAddOperation : public PlnExpression
{
public:
	PlnExpression* l;
	PlnExpression* r;

	PlnAddOperation(PlnExpression* l, PlnExpression* r);
	static PlnExpression* create(PlnExpression* l, PlnExpression* r);
	
	void finish();	// override
	void dump(ostream& os, string indent="");	// override
	void gen(PlnGenerator& g);	// override
};

// Assignment: lvals Expression
class PlnAssignment : public PlnExpression
{
public:
	PlnAssignment(vector<PlnValue>& lvals, PlnExpression* exp);
	PlnExpression* expression;

	void finish();	// override
	void dump(ostream& os, string indent="");	// override
	void gen(PlnGenerator& g);	// override
};

// PlnType
enum PlnTypType {
	TP_INT8,
	TP_UINT8,
	TP_OBJ,
	TP_IMP,
};

class PlnType {
public:
	PlnTypType	type;
	string name;
	int size;
};

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
		struct {
			int pos_from_base;
		} stack;
		int index;
	} inf;
	PlnGenEntity* genEntity(PlnGenerator& g);
};

class PlnParameter : public PlnVariable {
public:
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
	PlnVarInit(vector<PlnVariable*>& vars, PlnExpression* initializer);

	vector<PlnVariable*> vars;
	PlnExpression* initializer;
	PlnBlock* parent;

	void finish();
	void gen(PlnGenerator& g);
};
