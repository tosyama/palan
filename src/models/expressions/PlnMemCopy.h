/// PlnMemCopy Expression model class declaration.
///
/// @file	PlnMemCopy.h
/// @copyright	2018-2022 YAMAGUCHI Toshinobu 

#include "../PlnExpression.h"

class PlnMemCopy : public PlnExpression {
public:
	PlnExpression *dst_ex, *src_ex, *len_ex;
	PlnDataPlace *cp_dst_dp, *cp_src_dp, *cp_len_dp;
	int cp_unit;
	PlnMemCopy(PlnExpression *dst, PlnExpression *src, PlnExpression *len)
		: dst_ex(dst), src_ex(src), len_ex(len), cp_dst_dp(NULL), cp_src_dp(NULL), cp_len_dp(NULL),
			cp_unit(1), PlnExpression(ET_MCOPY)
	{ 
		BOOST_ASSERT(dst && src && len);
		BOOST_ASSERT(len_ex->type == ET_VALUE);
		BOOST_ASSERT(len_ex->values[0].type == VL_LIT_UINT8);

		uint64_t cp_size = len_ex->values[0].inf.uintValue;
		if ((cp_size % 8) == 0) {
			cp_size /= 8; cp_unit = 8;
		} else if ((cp_size % 4) == 0) {
			cp_size /= 4; cp_unit = 4;
		} else if ((cp_size % 2) == 0) {
			cp_size /= 2; cp_unit = 2;
		}
		len_ex->values[0].inf.uintValue = cp_size;
	}

	~PlnMemCopy() {
		delete src_ex;
		delete dst_ex;
		delete len_ex;
	}

	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override {
		da.prepareMemCopyDps(cp_dst_dp, cp_src_dp, cp_len_dp);

		src_ex->data_places.push_back(cp_src_dp);
		src_ex->finish(da, si);

		dst_ex->data_places.push_back(cp_dst_dp);
		dst_ex->finish(da, si);

		len_ex->data_places.push_back(cp_len_dp);
		len_ex->finish(da, si);
		
		da.popSrc(cp_src_dp);
		da.popSrc(cp_dst_dp);
		da.popSrc(cp_len_dp);
		da.memCopyed(cp_dst_dp, cp_src_dp, cp_len_dp);
	}

	void gen(PlnGenerator& g) override {
		src_ex->gen(g);
		dst_ex->gen(g);
		len_ex->gen(g);

		g.genLoadDp(cp_src_dp);
		g.genLoadDp(cp_dst_dp);
		g.genLoadDp(cp_len_dp);

		static string cmt = "deep copy";
		g.genMemCopy(cp_unit, cmt);
	}
};
