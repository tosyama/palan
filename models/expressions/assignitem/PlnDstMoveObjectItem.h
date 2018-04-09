/// Assignment Item class definition.
///
/// Need to free before move ownership.
///
/// @file	PlnDstMoveObjectItem.h
/// @copyright	2018 YAMAGUCHI Toshinobu 

class PlnDstMoveObjectItem : public PlnDstItem
{
	PlnExpression *dst_ex;
	PlnDataPlace *dst_dp;
	PlnHeapAllocator *freer;

public:
	PlnDstMoveObjectItem(PlnExpression* ex) : dst_ex(ex), dst_dp(NULL) {
		BOOST_ASSERT(ex->values.size() == 1);
		BOOST_ASSERT(ex->values[0].type == VL_VAR);
		BOOST_ASSERT(ex->values[0].inf.var->ptr_type & PTR_REFERENCE);
		freer = PlnHeapAllocator::createHeapFree(ex->values[0].inf.var);
	}

	bool ready() override { return true; }

	PlnAsgnType getAssginType() override { return ASGN_MOVE; }

	PlnDataPlace* getInputDataPlace(PlnDataAllocator& da) override {
		dst_dp = dst_ex->values[0].getDataPlace(da);
		return dst_dp;
	}

	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override {
		freer->finish(da, si);
		BOOST_ASSERT(dst_dp->src_place);
		da.popSrc(dst_dp);
		dst_ex->finish(da, si);
	}

	void gen(PlnGenerator& g) override {
		freer->gen(g);
		g.genLoadDp(dst_dp);
		dst_ex->gen(g);
	}
};

