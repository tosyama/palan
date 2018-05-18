/// PlnMemCopy Expression model class declaration.
///
/// @file	PlnMemCopy.h
/// @copyright	2018 YAMAGUCHI Toshinobu 

#include "../PlnExpression.h"

class PlnMemCopy : public PlnExpression {
public:
	PlnExpression *dst_ex, *src_ex, *len_ex;
	PlnDataPlace *cp_dst_dp, *cp_src_dp;
	PlnMemCopy(PlnExpression *dst, PlnExpression *src, PlnExpression *len)
		: dst_ex(dst), src_ex(src), len_ex(len), PlnExpression(ET_MCOPY)
	{ 
		BOOST_ASSERT(len_ex->type == ET_VALUE);
		BOOST_ASSERT(len_ex->values[0].type == VL_LIT_UINT8);
	}

	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override {
		da.prepareMemCopyDps(cp_dst_dp, cp_src_dp);

		src_ex->data_places.push_back(cp_src_dp);
		src_ex->finish(da, si);

		dst_ex->data_places.push_back(cp_dst_dp);
		dst_ex->finish(da, si);
		da.popSrc(cp_src_dp);
		da.popSrc(cp_dst_dp);
		da.memCopyed(cp_dst_dp, cp_src_dp);
	}

	void gen(PlnGenerator& g) override {
		src_ex->gen(g);
		dst_ex->gen(g);
		g.genLoadDp(cp_src_dp);
		g.genLoadDp(cp_dst_dp);

		int copy_size = len_ex->values[0].inf.uintValue;

		static string cmt = "deep copy";
		g.genMemCopy(copy_size, cmt);
	}
};
