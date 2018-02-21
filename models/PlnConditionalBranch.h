/// Conditi Branch model classes declaration.
///
/// @file	PlnConditionalBranch.h
/// @copyright	2018 YAMAGUCHI Toshinobu 

#include "PlnStatement.h"

class PlnCmpOperation;
class PlnIfStatement : public PlnStatement
{
public:
	PlnCmpOperation* condition;
	int jmp_next_id, jmp_end_id;
	PlnDataPlace* cond_dp;
	PlnStatement* next;

	PlnIfStatement(PlnExpression* condition, PlnBlock* block, PlnStatement* next, PlnBlock* parent);

	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void dump(ostream& os, string indent="") override;
	void gen(PlnGenerator& g) override;
};



