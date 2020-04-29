/// PlnBoolExpression model class declaration.
///
/// @file	PlnBoolExpression
/// @copyright	2020 YAMAGUCHI Toshinobu 

#include "../PlnExpression.h"
#include "../../PlnConstants.h"

class PlnBoolExpression : public PlnExpression {
public:
	int jmp_if;	// -1 (no jump), 0 (jump if false), 1 (jump if true)
	int jmp_id;
	int push_mode;	// data_places -1 (push result of cmp), 0 (push 0 before jump), 1 (push 1 before jump)
	bool is_not;	// true: reverse result
	PlnBoolExpression(PlnExprsnType type) : PlnExpression(type), jmp_if(-1), jmp_id(-1), push_mode(-1), is_not(false)
		{}
	static PlnBoolExpression* create(PlnExpression* e);
};

class PlnTrueExpression : public PlnBoolExpression
{
public:
	PlnTrueExpression();
	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void gen(PlnGenerator& g) override;
};

class PlnFalseExpression : public PlnBoolExpression
{
public:
	PlnFalseExpression();
	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void gen(PlnGenerator& g) override;
};
