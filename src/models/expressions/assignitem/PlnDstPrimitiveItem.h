/// Assignment Item class definition.
///
/// Dst value don't need to be free.
/// Target: Integer variable(direct/indirect),
///         Refernce copy.
///
/// @file	PlnDstPrimitiveItem.h
/// @copyright	2018-2020 YAMAGUCHI Toshinobu 

class PlnDstPrimitiveItem : public PlnDstItem {
	PlnExpression *dst_ex;
	PlnDataPlace *dst_dp;
	PlnVariable *save_src_var;

public:
	PlnDstPrimitiveItem(PlnExpression* ex)
			: dst_ex(ex), dst_dp(NULL), save_src_var(NULL) {
		BOOST_ASSERT(ex->values[0].type == VL_VAR);
		BOOST_ASSERT(ex->values[0].asgn_type != ASGN_MOVE);
	 }

	 ~PlnDstPrimitiveItem() {
		 delete save_src_var;
	 }

	PlnAsgnType getAssginType() override { return dst_ex->values[0].asgn_type; }

	PlnFinishRole setSrcEx(PlnDataAllocator &da, PlnScopeInfo &si, PlnExpression *src_ex) override {
		dst_dp = dst_ex->values[0].getDataPlace(da);
		if (place == NULL) {
			int val_ind = src_ex->data_places.size();
			src_ex->data_places.push_back(dst_dp);
			if (src_ex->getDataType(val_ind) != DT_OBJECT_REF) {
				// DT_SINT/DT_UINT/DT_FLOAT
				if (dst_dp->data_type == DT_OBJECT_REF) {
					auto allocmode = src_ex->values[0].inf.var->var_type->mode[ALLOC_MD];
					if (allocmode != 'r') {
						dst_dp->load_address = true;
					}
				}
			}

		} else {
			if (src_ex->type == ET_VALUE) {
				BOOST_ASSERT(src_ex->data_places.size() == 0);
			}

			if (dst_ex->type == ET_VALUE) {
				src_ex->data_places.push_back(dst_dp);

			} else {	// e.g. ET_ARRAYITEM. save_src_var is use also return value.
				PlnVarType* t = dst_ex->values[0].inf.var->var_type;
				save_src_var = PlnVariable::createTempVar(da, t, "(save src)");
				src_ex->data_places.push_back(save_src_var->place);
			}
		}
		return FINISH_BY_ASSIGNITEM;
	}

	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override {
		BOOST_ASSERT((!save_src_var && dst_dp->src_place)
			|| (save_src_var && save_src_var->place->src_place));
		dst_ex->finish(da, si);

		if (save_src_var) {
			BOOST_ASSERT(place);
			// 1. src -> save_src_var
			// 2. save_src_var -> dst
			// 3. save_src_var -> parent dst

			da.popSrc(save_src_var->place);
			da.pushSrc(dst_dp, save_src_var->place, false);
			da.popSrc(dst_dp);
			da.releaseDp(dst_dp);
			da.pushSrc(place, save_src_var->place);

		} else {
			if (need_save)
				da.allocSaveData(dst_dp, dst_dp->push_src_step, dst_dp->release_step);

			da.popSrc(dst_dp);
			if (place)
				da.pushSrc(place, dst_dp);
			else
				da.releaseDp(dst_dp);
		}
	}

	void gen(PlnGenerator& g) override {
		dst_ex->gen(g);

		if (save_src_var) {
			g.genLoadDp(save_src_var->place);
			g.genLoadDp(dst_dp);
			g.genSaveSrc(place);

		} else {
			g.genLoadDp(dst_dp);
			if (place) g.genSaveSrc(place);
		}
	}
};

