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
	struct DstInf {PlnDstItem* item; PlnVariable* save_src_var; PlnExpression* free_ex; };
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

		dsts.push_back( {PlnDstItem::createDstItem(ex), NULL} );
	}

	void finishS(PlnDataAllocator& da, PlnScopeInfo& si) override {
		int i=0;
		for (auto &di: dsts) {
			auto v = src_ex->values[i];
			if (v.getType()->data_type == DT_OBJECT_REF && di.item->getAssginType() == ASGN_COPY) {
				di.save_src_var = PlnVariable::createTempVar(da, *v.inf.wk_type, "save src");
				di.free_ex = PlnFreer::getFreeEx(di.save_src_var);

				src_ex->data_places.push_back(di.save_src_var->place);
				da.pushSrc(di.item->getInputDataPlace(da), di.save_src_var->place, false);

			} else {
				src_ex->data_places.push_back(di.item->getInputDataPlace(da));
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


