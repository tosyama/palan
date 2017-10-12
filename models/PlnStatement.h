/// PlnStatement model class declaration.
///
/// @file	PlnStatement.h
/// @copyright	2017- YAMAGUCHI Toshinobu 

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

	virtual void finish(PlnDataAllocator& da, PlnScopeInfo& si);
	virtual void dump(ostream& os, string indent="");
	virtual void gen(PlnGenerator& g);
};

class PlnReturnStmt : public PlnStatement
{
public:
	PlnFunction *function;
	vector<PlnExpression*> expressions;
	vector<PlnVariable*> to_free_vars;
	PlnDataPlace *late_pop_dp;

	PlnReturnStmt(vector<PlnExpression*> &retexp, PlnBlock* parent);

	void finish(PlnDataAllocator& da, PlnScopeInfo& si);
	void dump(ostream& os, string indent="");
	void gen(PlnGenerator& g);
};

