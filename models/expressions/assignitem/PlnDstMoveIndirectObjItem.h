/// Assignment Item class definition.
///
/// @file	PlnDstMoveIndirectObjItem.h
/// @copyright	2018 YAMAGUCHI Toshinobu 

class PlnDstMoveIndirectObjItem: public PlnDstItem
{
	PlnExpression *dst_ex;
	PlnDataPlace *dst_dp;
	PlnVariable *save_var;
	PlnHeapAllocator *freer;
	PlnExpression *free_ex;

public:
	PlnDstMoveIndirectObjItem(PlnExpression* ex)
			: dst_ex(ex), dst_dp(NULL), save_var(NULL), freer(NULL), free_ex(NULL) {
		BOOST_ASSERT(ex->values.size() == 1);
		BOOST_ASSERT(ex->values[0].type == VL_VAR);
		BOOST_ASSERT(ex->values[0].inf.var->ptr_type & (PTR_REFERENCE | PTR_INDIRECT_ACCESS));
	}

	PlnAsgnType getAssginType() override { return ASGN_MOVE; }

	PlnDataPlace* getInputDataPlace(PlnDataAllocator& da) override {
		dst_dp = dst_ex->values[0].getDataPlace(da);
		return dst_dp;
	}

	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override {
		BOOST_ASSERT(dst_dp);
		dst_ex->finish(da, si);
		auto var = dst_ex->values[0].inf.var;
		auto lt = si.get_lifetime(var);
		if (lt == VLT_ALLOCED || lt == VLT_INITED) {
			save_var = PlnVariable::createTempVar(da, var->var_type, var->name + "(save)");
			free_ex = PlnFreer::getFreeEx(save_var);
			da.pushSrc(save_var->place, dst_ex->values[0].getDataPlace(da));
			da.popSrc(save_var->place);
		}
		da.popSrc(dst_dp);
		if (free_ex)
			free_ex->finish(da, si);
		if (save_var)
			da.releaseData(save_var->place);
	}

	void gen(PlnGenerator& g) override {
		dst_ex->gen(g);
		if (save_var)
			g.genLoadDp(save_var->place);
		g.genLoadDp(dst_dp);

		if (free_ex)
			free_ex->gen(g);
	}
};

