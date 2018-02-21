/// Create clone expression class declaration.
///
/// @file	PlnClone.h
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include "../PlnExpression.h"


class PlnClone : public PlnExpression
{
public:
	PlnExpression* clone_src;
	PlnDataPlace* clone_dp;
	PlnDataPlace* src_dp;
	PlnDataPlace* cpy_dst_dp, *cpy_src_dp;
	int copy_size;

	PlnClone(PlnExpression* src);
	
	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void dump(ostream& os, string indent="") override;
	void gen(PlnGenerator& g) override;
};


