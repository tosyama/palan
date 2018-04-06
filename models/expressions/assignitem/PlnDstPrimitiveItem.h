/// Assignment Item class definition.
///
/// @file	PlnDstPrimitiveItem.h
/// @copyright	2018 YAMAGUCHI Toshinobu 

class PlnDstPrimitiveItem : public PlnDstItem {
	PlnExpression *dst_ex;
	PlnDataPlace* dst_dp;

public:
	PlnDstPrimitiveItem(PlnExpression* ex) : dst_ex(ex), dst_dp(NULL) {
		BOOST_ASSERT(ex->values[0].type == VL_VAR);
		BOOST_ASSERT(ex->values[0].inf.var->ptr_type == NO_PTR);
	}

	bool ready() override { return true; }

	PlnDataPlace* getInputDataPlace(PlnDataAllocator& da) override {
		dst_dp = dst_ex->values[0].getDataPlace(da);
		return dst_dp;
	}

	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override {
		BOOST_ASSERT(dst_dp->src_place);
		da.popSrc(dst_dp);
		dst_ex->finish(da, si);
	}

	void gen(PlnGenerator& g) override {
		g.genLoadDp(dst_dp);
		dst_ex->gen(g);
	}
};

