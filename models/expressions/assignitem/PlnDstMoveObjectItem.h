/// Assignment Item class definition.
///
/// Single Dst object value need to be free
///	before move ownership.
///
/// @file	PlnDstMoveObjectItem.h
/// @copyright	2018 YAMAGUCHI Toshinobu 

class PlnDstMoveObjectItem : public PlnDstItem
{
	PlnExpression *dst_ex;
	PlnDataPlace *dst_dp;
	PlnExpression *free_ex;

public:
	PlnDstMoveObjectItem(PlnExpression* ex)
			: dst_ex(ex), dst_dp(NULL), free_ex(NULL)
	{
		BOOST_ASSERT(ex->values.size() == 1);
		BOOST_ASSERT(ex->values[0].type == VL_VAR);
		BOOST_ASSERT(ex->values[0].inf.var->ptr_type & PTR_REFERENCE);
	}

	PlnAsgnType getAssginType() override { return ASGN_MOVE; }

	void setSrcEx(PlnDataAllocator &da, PlnExpression *src_ex) override {
		dst_dp = dst_ex->values[0].getDataPlace(da);
		src_ex->data_places.push_back(dst_dp);
	}

	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override {
		dst_ex->finish(da, si);
		auto var = dst_ex->values[0].inf.var;
		auto lt = si.get_lifetime(var);
		if (lt == VLT_ALLOCED || lt == VLT_INITED) {
			free_ex = PlnFreer::getFreeEx(var);
			free_ex->finish(da, si);
		}

		BOOST_ASSERT(dst_dp->src_place);
		da.popSrc(dst_dp);
		si.set_lifetime(var, VLT_INITED);
	}

	void gen(PlnGenerator& g) override {
		dst_ex->gen(g);
		if (free_ex)
			free_ex->gen(g);
		g.genLoadDp(dst_dp);
	}
};

