/// Loop statement model classes declaration.
///
/// @file	PlnLoopStatement.h
/// @copyright	2018-2019 YAMAGUCHI Toshinobu 

#include "PlnStatement.h"

class PlnBoolExpression;
class PlnWhileStatement : public PlnStatement
{
public:
	PlnBoolExpression* condition;

	int jmp_start_id, jmp_end_id;
	PlnDataPlace* cond_dp;

	PlnWhileStatement(PlnExpression* condition, PlnBlock* block, PlnBlock* parent);
	PlnWhileStatement(const PlnWhileStatement &) = delete;
	~PlnWhileStatement();

	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void gen(PlnGenerator& g) override;
};

class PlnBreakStatement : public PlnStatement
{
public:
	int jmp_id;
	PlnStatement* target_stmt;
	vector<PlnExpression*> free_vars;

	PlnBreakStatement(PlnStatement* target_stmt);

	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void gen(PlnGenerator& g) override;
};

class PlnContinueStatement : public PlnStatement
{
public:
	int jmp_id;
	PlnStatement* target_stmt;
	vector<PlnExpression*> free_vars;

	PlnContinueStatement(PlnStatement* target_stmt);

	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void gen(PlnGenerator& g) override;
};

