/// Loop statement model classes declaration.
///
/// @file	PlnLoopStatement.h
/// @copyright	2018-2019 YAMAGUCHI Toshinobu 

#include "PlnStatement.h"

class PlnCmpExpression;
class PlnWhileStatement : public PlnStatement
{
public:
	PlnCmpExpression* condition;
	int jmp_start_id, jmp_end_id;
	PlnDataPlace* cond_dp;

	PlnWhileStatement(PlnExpression* condition, PlnBlock* block, PlnBlock* parent);
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
