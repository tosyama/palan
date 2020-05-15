/// Conditi Branch model classes declaration.
///
/// @file	PlnConditionalBranch.h
/// @copyright	2018-2020 YAMAGUCHI Toshinobu 

#include "PlnStatement.h"

class PlnBoolExpression;
class PlnIfStatement : public PlnStatement
{
public:
	PlnBoolExpression* condition;
	int jmp_next_id, jmp_end_id;
	PlnDataPlace* cond_dp;
	PlnStatement* next;

	PlnIfStatement(PlnExpression* condition, PlnBlock* block, PlnStatement* next, PlnBlock* parent);
	PlnIfStatement(const PlnIfStatement &) = delete;
	~PlnIfStatement();

	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void gen(PlnGenerator& g) override;
};

