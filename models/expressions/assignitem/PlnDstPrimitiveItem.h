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
	}

	bool ready() override { return true; }

	PlnAsgnType getAssginType() override { return dst_ex->values[0].asgn_type; }

	PlnDataPlace* getInputDataPlace(PlnDataAllocator& da) override {
		dst_dp = dst_ex->values[0].getDataPlace(da);
		return dst_dp;
	}

	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override {
		BOOST_ASSERT(dst_dp->src_place);
		dst_ex->finish(da, si);
		da.popSrc(dst_dp);
	}

	void gen(PlnGenerator& g) override {
		dst_ex->gen(g);
		g.genLoadDp(dst_dp);
	}
};

