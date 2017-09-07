#include "../PlnModel.h"

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
	} inf;

	PlnStatement() {};
	PlnStatement(PlnExpression *exp, PlnBlock* parent);
	PlnStatement(PlnVarInit* var_init, PlnBlock* parent);
	PlnStatement(PlnBlock* block, PlnBlock* parent);

	virtual void finish(PlnDataAllocator& da);
	virtual void dump(ostream& os, string indent="");
	virtual void gen(PlnGenerator& g);
};

class PlnReturnStmt : public PlnStatement
{
public:
	PlnFunction *function;
	
	PlnReturnStmt(PlnExpression *retexp, PlnBlock* parent);

	void finish(PlnDataAllocator& da);
	void dump(ostream& os, string indent="");
	void gen(PlnGenerator& g);
};

