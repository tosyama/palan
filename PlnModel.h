#include <vector>
#include <string>
#include <iostream>
#include <cstdint>

using std::string;
using std::vector;
using std::ostream;
using std::move;

class PlnGenerator;
class PlnGenEntity;

class PlnFunction;
class PlnBlock;
class PlnStatement;
class PlnExpression;
class PlnVariable;
class PlnValue;
class PlnReadOnlyData;

// Module: Functions
class PlnModule
{
	bool is_main;
	vector<PlnFunction*> functions;
	vector<PlnReadOnlyData*> readonlydata;
	
public:
	PlnModule();
	void addFunc(PlnFunction& func);
	void addReadOnlyData(PlnReadOnlyData& rodata);	// obsolute

	PlnFunction* getFunc(const string& func_name);
	PlnReadOnlyData* getReadOnlyData(string &str);

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

	void dump(ostream& os, string indent="");
	void gen(PlnGenerator& g);
};

// Block: Statements
class PlnBlock {
public:
	vector<PlnStatement*> statements;

	void addStatement(PlnStatement &statement);

	void dump(ostream& os, string indent="");
	void gen(PlnGenerator& g);
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

	void dump(ostream& os, string indent="");
	void gen(PlnGenerator& g);
};

// Value (rval)
enum PlnValType {
	VL_LIT_INT8,
	VL_RO_DATA
};

class PlnValue {
public:
	PlnValType type;
	union {
		int64_t intValue;
		PlnReadOnlyData* rod;
	} inf;
	PlnGenEntity* genEntity(PlnGenerator& g);
};

// Expression: FunctionCall
enum PlnExprsnType {
	ET_VALUE,
	ET_FUNCCALL
};

class PlnExpression {
public:
	PlnExprsnType type;
	PlnValue value;

	virtual void dump(ostream& os, string indent="");
	virtual void gen(PlnGenerator& g);
};

// FunctionCall: Function Arguments;
class PlnFunctionCall : public PlnExpression
{
public:
	PlnFunction* function;
	vector<PlnExpression*> arguments;

	void addArgument(PlnExpression& arg);	// obsolute

	void gen(PlnGenerator& g);
};

// Variable: Type name
enum PlnVarType {
	VT_INT8,
	VT_UINT8,
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

