/// PlnAssignment model class declaration.
///
/// @file	PlnAssignment.h
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include "../PlnExpression.h"

class PlnHeapAllocator;

class PlnAssignItem;

union PlnAssignInf
{
	int16_t	type;
	struct {
		int16_t	type;
		PlnDataPlace *dp;
	} inf;

	struct {
		int16_t	type;
		int16_t cp_size;

		PlnDataPlace *src, *dst;
		PlnDataPlace *free_dp;
	} mcopy;

	struct {
		int16_t	type;

		PlnHeapAllocator* free_dst;
		PlnVariable* do_clear_var;
		PlnDataPlace *src, *dst;
		PlnDataPlace *save_indirect;
	} move;

	struct {
		int16_t type;
		PlnAssignItem* ai;
	} item;
};


// Assignment: lvals Expression
class PlnAssignment : public PlnExpression
{
public:
	PlnAssignment(vector<PlnExpression*>& lvals, vector<PlnExpression*>& exps);
	vector<PlnExpression*> lvals;
	vector<PlnExpression*> expressions;
	vector<PlnAssignInf> assign_inf;
	vector<PlnAssignItem*> assgin_items;

	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void dump(ostream& os, string indent="") override;
	void gen(PlnGenerator& g) override;
};

