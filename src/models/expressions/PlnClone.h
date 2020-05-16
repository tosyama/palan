/// PlnClone model class declaration.
///
/// @file	PlnClone.h
/// @copyright	2018-2019 YAMAGUCHI Toshinobu 

class PlnClone : public PlnExpression
{
	PlnExpression *alloc_ex, *free_ex;
	PlnDataPlace* copy_dst_dp;
	PlnDeepCopyExpression *copy_ex;
	PlnExpression *src_ex;
	vector<PlnAssignItem*> assign_items;

public:
	bool keep_var;
	bool directAssign;
	PlnVariable* var;

	PlnClone(PlnDataAllocator& da, PlnExpression *src_ex, PlnVarType* var_type, bool keep_var);
	~PlnClone();

	void finishAlloc(PlnDataAllocator& da, PlnScopeInfo& si);
	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void finishFree(PlnDataAllocator& da, PlnScopeInfo& si);

	void genAlloc(PlnGenerator& g);
	void gen(PlnGenerator& g) override;
	void genFree(PlnGenerator& g);
};
