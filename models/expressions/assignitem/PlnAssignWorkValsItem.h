/// Assignment Item class definition.
///
/// Src item returns multiple work values expression.
/// The work valuses don't need to be cleared.
/// Exist two cases need to free.
/// 1) After object was copied.
/// 2) Didn't assign any variable. (not impemented)
///
/// @file	PlnAssignWorkValsItem.h
/// @copyright	2018 YAMAGUCHI Toshinobu 

class PlnAssignWorkValsItem : public PlnAssignItem
{
	PlnExpression* src_ex;
	struct DstInf {
		PlnDstItem* item;
		PlnVariable* save_src_var;
		PlnExpression* copy_src_ex, *free_ex;
	};
	vector<DstInf> dsts;

public:
	PlnAssignWorkValsItem(PlnExpression* ex) : src_ex(ex) {
		auto v = src_ex->values[0];
		BOOST_ASSERT(v.type == VL_WORK);
	}

	void addDstEx(PlnExpression* ex) override {
		BOOST_ASSERT(ex->values.size() == 1);
		auto v = ex->values[0];

		BOOST_ASSERT(v.type == VL_VAR);

		dsts.push_back( {PlnDstItem::createDstItem(ex), NULL, NULL, NULL} );
	}

	int addDataPlace(vector<PlnDataPlace*> &data_places, int start_ind) override {
		for (auto &di: dsts) {
			di.item->place = data_places[start_ind];
			start_ind++;
			if (start_ind >= data_places.size())
				break;
		}
		return start_ind;
	}

	void finishS(PlnDataAllocator& da, PlnScopeInfo& si) override {
		int i=0;
		for (auto &di: dsts) {
			auto v = src_ex->values[i];
			if (v.getType()->data_type == DT_OBJECT_REF && di.item->getAssginType() == ASGN_COPY) {
				di.save_src_var = PlnVariable::createTempVar(da, *v.inf.wk_type, "save src");
				di.free_ex = PlnFreer::getFreeEx(di.save_src_var);

				src_ex->data_places.push_back(di.save_src_var->place);

				di.copy_src_ex = new PlnExpression(di.save_src_var);
				di.item->setSrcEx(da, si, di.copy_src_ex);

			} else {
				di.item->setSrcEx(da, si, src_ex);
			}
			i++;
		}

		for (; i<src_ex->values.size(); i++) {
			BOOST_ASSERT(false);	// not impemented
		} 

		src_ex->finish(da, si);
	}

	void finishD(PlnDataAllocator& da, PlnScopeInfo& si) override {
		for (auto &di: dsts) {
			if (di.save_src_var) {	// ASGN_COPY
				da.popSrc(di.save_src_var->place);
				di.copy_src_ex->finish(da, si);
				di.item->finish(da, si);
				di.free_ex->finish(da, si);
				da.releaseDp(di.save_src_var->place);
			} else {
				di.item->finish(da, si);
			}
		}
	}

	void genS(PlnGenerator& g) override {
		src_ex->gen(g);
	}

	void genD(PlnGenerator& g) override {
		for (auto di: dsts) {
			if (di.save_src_var) {	// ASGN_COPY
				g.genLoadDp(di.save_src_var->place);
				di.item->gen(g);
				di.free_ex->gen(g);

			} else { 
				di.item->gen(g);
			}
		}
	}
};


