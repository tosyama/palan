/// Loop statement model classes declaration.
///
/// @file	PlnLoopStatement.h
/// @copyright	2018 YAMAGUCHI Toshinobu 

#include "PlnStatement.h"

class PlnCmpOperation;
class PlnWhileStatement : public PlnStatement
{
public:
	PlnCmpOperation* condition;
	int jmp_start_id, jmp_end_id;
	PlnDataPlace* cond_dp;

	PlnWhileStatement(PlnExpression* condition, PlnBlock* block, PlnBlock* parent);

	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void dump(ostream& os, string indent="") override;
	void gen(PlnGenerator& g) override;
};


