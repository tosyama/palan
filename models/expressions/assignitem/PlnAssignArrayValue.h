/// Assignment Item class definition.
///
///
/// @file	PlnAssignArrayValue.h
/// @copyright	2019 YAMAGUCHI Toshinobu 

class PlnAssignArrayValue : public PlnAssignItem {
	PlnArrayValue* src_ex;
	PlnExpression* dst_ex;
	vector<PlnDataPlace*> arr_item_dps;
	vector<PlnDataPlace*> index_dps;
	vector<PlnDataPlace*> base_dps;

public:
	PlnAssignArrayValue(PlnExpression* ex) {
		BOOST_ASSERT(ex->type = ET_ARRAYVALUE);
		src_ex = static_cast<PlnArrayValue*>(ex);
	}

	~PlnAssignArrayValue() {
	}
	
	void addDstEx(PlnExpression* ex, bool need_save) override {
		BOOST_ASSERT(ex->values[0].type == VL_VAR);
		BOOST_ASSERT(ex->values[0].getType()->data_type == DT_OBJECT_REF);

		BOOST_ASSERT(ex->type == ET_VALUE);
		BOOST_ASSERT(ex->values[0].inf.var->var_type->type == TP_FIXED_ARRAY);

		dst_ex = ex;
	}

	int addDataPlace(vector<PlnDataPlace*> &data_places, int start_ind) override {
		// dst_item->place = data_places[start_ind];
		return start_ind + 1;
	}

	void finishS(PlnDataAllocator& da, PlnScopeInfo& si) override {
	 	auto var_dp = da.getSeparatedDp(dst_ex->values[0].inf.var->place);
		auto arr_type = static_cast<PlnFixedArrayType*>(dst_ex->values[0].inf.var->var_type);
		auto item_type = arr_type->item_type;
		
		PlnDataPlace *item_dp = new PlnDataPlace(8, DT_SINT);
		
		auto base_dp = da.prepareObjBasePtr();
		auto index_dp = da.prepareObjIndexPtr(0);
		da.setIndirectObjDp(item_dp, base_dp, index_dp);
		
		da.pushSrc(base_dp, var_dp, true);
		item_dp->comment = new string("[0]");
		src_ex->item_exps[0]->data_places.push_back(item_dp);

		arr_item_dps.push_back(item_dp);

		src_ex->finish(da, si);
	}

	void finishD(PlnDataAllocator& da, PlnScopeInfo& si) override {
 		da.popSrc(arr_item_dps.back());
		da.releaseDp(arr_item_dps.back());
	}

	void genS(PlnGenerator& g) override {
		src_ex->gen(g);
	}

	void genD(PlnGenerator& g) override {
		g.genLoadDp(arr_item_dps.back());
	}
};

