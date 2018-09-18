/// PlnClone model class declaration.
///
/// @file	PlnClone.h
/// @copyright	2018 YAMAGUCHI Toshinobu 

class PlnClone : public PlnExpression
{
	PlnExpression *alloc_ex, *free_ex;
	PlnDataPlace* copy_dst_dp;
	PlnDeepCopyExpression *copy_ex;

public:
	bool keep_var;
	PlnVariable* var;
	PlnDataPlace *src_dp;

	PlnClone(PlnDataAllocator& da, vector<PlnType*> &var_type, bool keep_var);
	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void finishFree(PlnDataAllocator& da, PlnScopeInfo& si);
	void gen(PlnGenerator& g) override;
	void genFree(PlnGenerator& g);
};
