/// Assignment Item class definition.
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

	void pushDp2ArrayVal(PlnFixedArrayType* arr_type, int depth,
			PlnDataPlace* var_dp, int &var_index,  PlnArrayValue* arr_val,
			PlnDataAllocator& da, PlnScopeInfo& si) {

		BOOST_ASSERT(arr_type->sizes.size() > depth);
		BOOST_ASSERT(arr_type->sizes[depth] == arr_val->item_exps.size());

		if (arr_type->sizes.size()-1 == depth) {
			auto item_type = arr_type->item_type;
			static string cmt = "[]";
			for (auto exp: arr_val->item_exps) {
				PlnDataPlace *item_dp = new PlnDataPlace(item_type->size, item_type->data_type);

				auto base_dp = da.prepareObjBasePtr();
				auto index_dp = da.prepareObjIndexPtr(var_index);
				da.setIndirectObjDp(item_dp, base_dp, index_dp);

				da.pushSrc(base_dp, var_dp, false);
				item_dp->comment = &cmt;
				exp->data_places.push_back(item_dp);

				arr_item_dps.push_back(item_dp);
				var_index++;
			}

		} else {
			for (auto exp: arr_val->item_exps) {
				BOOST_ASSERT(exp->type == ET_ARRAYVALUE);
				pushDp2ArrayVal(arr_type, depth+1, var_dp, var_index, static_cast<PlnArrayValue*>(exp), da, si);
			}
		}
	}

	void finishS(PlnDataAllocator& da, PlnScopeInfo& si) override {
	 	auto var_dp = da.getSeparatedDp(dst_ex->values[0].inf.var->place);
		auto arr_type = static_cast<PlnFixedArrayType*>(dst_ex->values[0].inf.var->var_type);
		auto item_type = arr_type->item_type;
		static string cmt = "[]";

		int var_index = 0;
		pushDp2ArrayVal(arr_type, 0, var_dp, var_index, src_ex, da, si);

		src_ex->finish(da, si);
		da.releaseDp(var_dp);
	}

	void finishD(PlnDataAllocator& da, PlnScopeInfo& si) override {
		for (auto item_dp: arr_item_dps) {
			da.popSrc(item_dp);
			da.releaseDp(item_dp);
		}
	}

	void genS(PlnGenerator& g) override {
		src_ex->gen(g);
	}

	void genD(PlnGenerator& g) override {
		for (auto item_dp: arr_item_dps)
			g.genLoadDp(item_dp);
	}
};

