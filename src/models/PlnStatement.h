/// PlnStatement model class declaration.
///
/// @file	PlnStatement.h
/// @copyright	2017-2019 YAMAGUCHI Toshinobu 
#pragma once
#include "../PlnModel.h"

// Statement: Expression | Block
enum PlnStmtType {
	ST_EXPRSN,
	ST_VARINIT,
	ST_BLOCK,
	ST_RETURN,
	ST_WHILE,
	ST_BREAK,
	ST_CONTINUE,
	ST_IF
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
	PlnLoc loc;

	PlnStatement() {};
	PlnStatement(PlnExpression *exp, PlnBlock* parent);
	PlnStatement(PlnVarInit* var_init, PlnBlock* parent);
	PlnStatement(PlnBlock* block, PlnBlock* parent);
	virtual ~PlnStatement();

	virtual void finish(PlnDataAllocator& da, PlnScopeInfo& si);
	virtual void gen(PlnGenerator& g);
};

class PlnClone;
class PlnReturnStmt : public PlnStatement
{
public:
	PlnFunction *function;
	vector<PlnExpression*> expressions;
	vector<PlnClone*> clones;
	vector<PlnDataPlace*> dps;
	vector<PlnExpression*> free_vars;

	PlnReturnStmt(vector<PlnExpression*> &retexp, PlnBlock* parent);	// throw PlnCompileError
	~PlnReturnStmt();

	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void gen(PlnGenerator& g) override;
};

