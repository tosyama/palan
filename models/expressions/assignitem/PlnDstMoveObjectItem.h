/// Assignment Item class definition.
///
/// Single Dst object value need to be free
///	before move ownership.
///
/// @file	PlnDstMoveObjectItem.h
/// @copyright	2018-2019 YAMAGUCHI Toshinobu 

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
		BOOST_ASSERT(ex->type == ET_VALUE);
		BOOST_ASSERT(ex->values[0].type == VL_VAR);
		BOOST_ASSERT(ex->getDataType(0) == DT_OBJECT_REF);

		if (ex->values[0].getVarType()->mode[IDENTITY_MD] != 'm') {
			PlnCompileError err(E_CantUseMoveOwnershipTo, ex->values[0].inf.var->name);
			throw err;
		}
	}

	~PlnDstMoveObjectItem() {
		delete free_ex;
	}

	PlnAsgnType getAssginType() override { return ASGN_MOVE; }

	void setSrcEx(PlnDataAllocator &da, PlnScopeInfo& si, PlnExpression *src_ex) override {
		int index = src_ex->data_places.size();

		auto src_type = src_ex->values[index].getVarType();
		auto dst_type = dst_ex->values[0].getVarType();
		if (src_type->typeinf != dst_type->typeinf) {
			PlnCompileError err(E_CantUseMoveOwnershipTo, dst_ex->values[0].inf.var->name);
			err.loc = dst_ex->loc;
			throw err;
		}
		
		dst_dp = dst_ex->values[0].getDataPlace(da);
		if (src_ex->values[index].type == VL_VAR
				&& src_ex->values[index].inf.var != dst_ex->values[0].inf.var) {
			dst_dp->do_clear_src = true;
		}
		src_ex->data_places.push_back(dst_dp);
	}

	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override {
		if (need_save) {
			da.allocSaveData(dst_dp, dst_dp->push_src_step, dst_dp->release_step);
		}

		dst_ex->finish(da, si);
		auto var = dst_ex->values[0].inf.var;
		auto lt = si.get_lifetime(var);
		if (lt == VLT_ALLOCED || lt == VLT_INITED || lt == VLT_PARTLY_FREED) {
			free_ex = PlnFreer::getFreeEx(var);
			free_ex->finish(da, si);
		}

		BOOST_ASSERT(dst_dp->src_place);
		da.popSrc(dst_dp);
		si.set_lifetime(var, VLT_INITED);

		if (place) {
			da.pushSrc(place, dst_dp);

		} else {
			da.releaseDp(dst_dp);
		}
	}

	void gen(PlnGenerator& g) override {
		dst_ex->gen(g);
		if (free_ex)
			free_ex->gen(g);
		g.genLoadDp(dst_dp);
		if (place) {
			g.genSaveDp(place);
		}
	}
};

