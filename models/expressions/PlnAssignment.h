/// PlnAssignment model class declaration.
///
/// @file	PlnAssignment.h
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include "../PlnExpression.h"

union PlnAssignInf
{
	struct {
		int32_t exp_ind;
		int32_t val_ind;
		int32_t cp_size;
		PlnDataPlace *src, *dst;
		PlnDataPlace *free_dp;
	} mcopy;
};

// Assignment: lvals Expression
class PlnAssignment : public PlnExpression
{
public:
	PlnAssignment(vector<PlnValue>& lvals, vector<PlnExpression*>& exps);
	vector<PlnExpression*> expressions;
	vector<PlnAssignInf> assign_inf;

	void finish(PlnDataAllocator& da);	// override
	void dump(ostream& os, string indent="");	// override
	void gen(PlnGenerator& g);	// override
};

