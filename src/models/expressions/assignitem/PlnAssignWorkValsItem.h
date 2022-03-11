/// Assignment Item class definition.
///
/// Src item returns multiple work values expression.
/// The work valuses don't need to be cleared.
/// Exist two cases need to free.
/// 1) After object was copied.
/// 2) Didn't assign any variable. (freed by src_ex)
///
/// @file	PlnAssignWorkValsItem.h
/// @copyright	2018-2019 YAMAGUCHI Toshinobu 

class PlnAssignWorkValsItem : public PlnAssignItem
{
	PlnExpression* src_ex;
	struct DstInf {
		PlnDstItem* item;
		PlnVariable* save_src_var;
		PlnExpression* copy_src_ex, *free_ex;
		PlnFinishRole fr;
	};
	vector<DstInf> dsts;
	vector<PlnExpression*> free_exs;

public:
	PlnAssignWorkValsItem(PlnExpression* ex) : src_ex(ex) {
		auto v = src_ex->values[0];
		BOOST_ASSERT(v.type == VL_WORK);
	}

	~PlnAssignWorkValsItem() {
		for (auto di: dsts) {
			delete di.item;
			delete di.save_src_var;
			delete di.copy_src_ex;
			delete di.free_ex;
		}
	}

	void addDstEx(PlnExpression* ex, bool need_save) override {
		BOOST_ASSERT(ex->values.size() == 1);
		BOOST_ASSERT(src_ex->values.size() > dsts.size());

		auto& dstv = ex->values[0];
		BOOST_ASSERT(dstv.type == VL_VAR);

		auto& srcv = src_ex->values[dsts.size()];
		if (srcv.getVarType()->mode[IDENTITY_MD] != 'm' && dstv.asgn_type == ASGN_MOVE) {
			// For the case that return value is readonly ref but move ownership value.
			// e.g.) struct_tm t <<= localtime(); // ccall localtime->struct_tm@
			BOOST_ASSERT(srcv.type == VL_WORK);
			PlnCompileError err(E_CantUseMoveOwnershipFrom, "source value");
			throw err;
		}

		dsts.push_back( {PlnDstItem::createDstItem(ex, need_save), NULL, NULL, NULL} );
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
			auto& v = src_ex->values[i];
			if (v.getVarType()->mode[ALLOC_MD] == 'h'
					&& di.item->getAssginType() == ASGN_COPY) {
				di.save_src_var = PlnVariable::createTempVar(da, v.inf.wk_type, "(save src)");
				di.free_ex = di.save_src_var->getFreeEx();

				src_ex->data_places.push_back(di.save_src_var->place);

				di.copy_src_ex = new PlnExpression(di.save_src_var);
				di.fr = di.item->setSrcEx(da, si, di.copy_src_ex);

			} else {
				PlnFinishRole fr = di.item->setSrcEx(da, si, src_ex);
				BOOST_ASSERT(fr == FINISH_BY_ASSIGNITEM);
			}
			i++;
		}

		src_ex->finish(da, si);
	}

	void finishD(PlnDataAllocator& da, PlnScopeInfo& si) override {
		for (auto &di: dsts) {
			if (di.save_src_var) {	// ASGN_COPY
				da.popSrc(di.save_src_var->place);
				if (di.fr == FINISH_BY_ASSIGNITEM) {
					di.copy_src_ex->finish(da, si);
				}
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


