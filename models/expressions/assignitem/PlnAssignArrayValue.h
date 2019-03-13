/// Assignment Item class definition.
///
/// @file	PlnAssignArrayValue.h
/// @copyright	2019 YAMAGUCHI Toshinobu 

class PlnAssignArrayValue : public PlnAssignItem {
	PlnArrayValue* src_ex;
	PlnExpression* dst_ex;
	PlnDataPlace* var_dp;

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
	 	var_dp = da.getSeparatedDp(dst_ex->values[0].inf.var->place);
		src_ex->data_places.push_back(var_dp);
		src_ex->finishS(da, si);
	}

	void finishD(PlnDataAllocator& da, PlnScopeInfo& si) override {
		src_ex->finishD(da, si);
		da.releaseDp(var_dp);
	}

	void genS(PlnGenerator& g) override {
		src_ex->genS(g);
	}

	void genD(PlnGenerator& g) override {
		src_ex->genD(g);
	}
};

