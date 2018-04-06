/// Assignment Item class definition.
///
/// @file	PlnDstCopyObjectItem.h
/// @copyright	2018 YAMAGUCHI Toshinobu 

class PlnDstCopyObjectItem : public PlnDstItem
{
	PlnExpression *dst_ex;
	PlnDataPlace *cp_src_dp, *cp_dst_dp;
	int copy_size;

public:
	PlnDstCopyObjectItem(PlnExpression* ex)
			: dst_ex(ex), cp_src_dp(NULL), cp_dst_dp(NULL) {
		BOOST_ASSERT(ex->values.size() == 1);
		BOOST_ASSERT(ex->values[0].type == VL_VAR);
		BOOST_ASSERT(ex->values[0].inf.var->ptr_type & PTR_REFERENCE);
		PlnType *t = ex->values[0].getType();
		if (t->inf.obj.is_fixed_size) {
			copy_size = t->inf.obj.alloc_size;
		} else BOOST_ASSERT(false);
	}

	bool ready() override { return true; }

	PlnDataPlace* getInputDataPlace(PlnDataAllocator& da) override {
		BOOST_ASSERT(!cp_src_dp);

		da.prepareMemCopyDps(cp_dst_dp, cp_src_dp);
		return cp_src_dp;
	}

	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override {
		dst_ex->data_places.push_back(cp_dst_dp);
		dst_ex->finish(da, si);
		BOOST_ASSERT(cp_src_dp && cp_dst_dp);
		da.popSrc(cp_src_dp);
		da.popSrc(cp_dst_dp);
		da.memCopyed(cp_dst_dp, cp_src_dp);
	}

	void gen(PlnGenerator& g) override {
		dst_ex->gen(g);
		g.genLoadDp(cp_src_dp);
		g.genLoadDp(cp_dst_dp);

		static string cmt = "deep copy";
		g.genMemCopy(copy_size, cmt);
	}
};

