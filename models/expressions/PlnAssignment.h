/// PlnAssignment model class declaration.
///
/// @file	PlnAssignment.h
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include "../PlnExpression.h"

union PlnAssignInf
{
	int16_t	type;
	struct {
		int16_t	type;
		int16_t val_ind;
	} inf;

	struct {
		int16_t	type;
		int16_t val_ind;

		int32_t cp_size;
		PlnDataPlace *src, *dst;
		PlnDataPlace *free_dp;
	} mcopy;
	struct {
		int16_t	type;
		int16_t val_ind;

		PlnDataPlace *src, *dst;
		bool do_clear;
	} move;
};

// Assignment: lvals Expression
class PlnAssignment : public PlnExpression
{
public:
	PlnAssignment(vector<PlnExpression*>& lvals, vector<PlnExpression*>& exps);
	vector<PlnExpression*> lvals;
	vector<PlnExpression*> expressions;
	vector<PlnAssignInf> assign_inf;

	void finish(PlnDataAllocator& da);	// override
	void dump(ostream& os, string indent="");	// override
	void gen(PlnGenerator& g);	// override
};

