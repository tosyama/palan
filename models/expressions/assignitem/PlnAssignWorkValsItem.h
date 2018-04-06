/// Assignment Item class definition.
///
/// Src item returns multiple work values expression.
/// The work valuses don't need to be cleared.
/// Exist two cases need to free.
/// 1) After object was copied.
/// 2) Didn't assign any variable.
///
/// @file	PlnAssignWorkValsItem.h
/// @copyright	2018 YAMAGUCHI Toshinobu 

class PlnAssignWorkValsItem : public PlnAssignItem
{
	PlnExpression* src_ex;
	struct DstInf {PlnDstItem* item; PlnDataPlace* cp_obj_dp; };
	vector<DstInf> dsts;
	vector<PlnDataPlace*> for_free_dps;

public:
	PlnAssignWorkValsItem(PlnExpression* ex) : src_ex(ex) {
	}

	bool ready() override {
		for(auto di: dsts)
			if (!di.item->ready()) return false;
		return true;
	};

	void addDstEx(PlnExpression* ex) override {
		BOOST_ASSERT(ex->values.size() == 1);
		auto v = ex->values[0];

		BOOST_ASSERT(v.type == VL_VAR);

		dsts.push_back( {PlnDstItem::createDstItem(ex), NULL} );
	}

	void finishS(PlnDataAllocator& da, PlnScopeInfo& si) override {
		int i=0;
		for (auto &di: dsts) {
			auto v = src_ex->values[i];
			if (v.getType()->data_type == DT_OBJECT_REF && !di.item->isMoveOwnership()) {
				static string cmt = "save src";
				PlnDataPlace *cp_src_dp = da.prepareLocalVar(8, DT_OBJECT_REF);
				cp_src_dp->comment = &cmt;
				di.cp_obj_dp = cp_src_dp;

				src_ex->data_places.push_back(cp_src_dp);
				da.pushSrc(di.item->getInputDataPlace(da), cp_src_dp, false);

			} else {
				src_ex->data_places.push_back(di.item->getInputDataPlace(da));
			}
			i++;
		}

		for (; i<src_ex->values.size(); i++) {
			auto v = src_ex->values[i];
			if (v.getType()->data_type == DT_OBJECT_REF) {
				PlnDataPlace *for_free_dp = da.prepareLocalVar(8, DT_OBJECT_REF);
				for_free_dps.push_back(for_free_dp);

				src_ex->data_places.push_back(for_free_dp);
				
			} else {
				src_ex->data_places.push_back(NULL);
			}
		}

		src_ex->finish(da, si);
	}

	void finishD(PlnDataAllocator& da, PlnScopeInfo& si) override {
		for (auto &di: dsts) {
			if (di.cp_obj_dp) {
				da.popSrc(di.cp_obj_dp);
				di.item->finish(da, si);
				da.memFreed();
				da.releaseData(di.cp_obj_dp);
			} else {
				di.item->finish(da, si);
			}
		}

		for (auto dp: for_free_dps) {
			da.popSrc(dp);
			da.memFreed();
			da.releaseData(dp);
		}
	}

	void genS(PlnGenerator& g) override {
		src_ex->gen(g);
	}

	void genD(PlnGenerator& g) override {
		for (auto di: dsts) {
			if (di.cp_obj_dp) {
				g.genLoadDp(di.cp_obj_dp);
				di.item->gen(g);
				auto fe = g.getEntity(di.cp_obj_dp);
				g.genMemFree(fe.get(), di.cp_obj_dp->cmt(), false);

			} else { 
				di.item->gen(g);
			}
		}

		for (auto dp: for_free_dps) {
			auto fe = g.getEntity(dp);
			g.genMemFree(fe.get(), dp->cmt(), false);
		}
	}
};


