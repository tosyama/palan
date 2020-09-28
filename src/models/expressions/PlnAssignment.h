/// PlnAssignment model class declaration.
///
/// @file	PlnAssignment.h
/// @copyright	2017-2020 YAMAGUCHI Toshinobu 

#include "../PlnExpression.h"

class PlnAssignItem;

// Assignment: lvals Expression
class PlnAssignment : public PlnExpression
{
public:
	PlnAssignment(vector<PlnExpression*>& dst_vals, vector<PlnExpression*>& exps); // throw PlnCompileError
	~PlnAssignment();
	vector<PlnExpression*> lvals;
	vector<PlnExpression*> expressions;
	vector<PlnAssignItem*> assgin_items;

	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void gen(PlnGenerator& g) override;
};

