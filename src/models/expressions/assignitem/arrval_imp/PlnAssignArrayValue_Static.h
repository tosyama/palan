/// Assignment Item class definition.
///
/// @file	PlnAssignArrayValue_Static.h
/// @copyright	2021 YAMAGUCHI Toshinobu 

class PlnAssignArrayValue_Static : public PlnAssignItem {
	PlnArrayValue* src_ex;
	PlnExpression* dst_ex;
	PlnDstItem *dst_item;
	PlnDataPlace *output_place;

public:
	PlnAssignArrayValue_Static(PlnExpression* ex) : dst_item(NULL), output_place(NULL), dst_ex(NULL)  {
		BOOST_ASSERT(ex->type = ET_ARRAYVALUE);
		src_ex = static_cast<PlnArrayValue*>(ex);
	}

	~PlnAssignArrayValue_Static() {
		delete dst_item;
	}
	
	void addDstEx(PlnExpression* ex, bool need_save) override {
		BOOST_ASSERT(ex->values[0].type == VL_VAR);
		BOOST_ASSERT(ex->values[0].getVarType()->data_type() == DT_OBJECT_REF
				|| ex->values[0].getVarType()->data_type() == DT_OBJECT);
		BOOST_ASSERT(!need_save);

		dst_ex = ex;
	}

	int addDataPlace(vector<PlnDataPlace*> &data_places, int start_ind) override {
		output_place = data_places[start_ind];
		return start_ind + 1;
	}

	void finishS(PlnDataAllocator& da, PlnScopeInfo& si) override {
		BOOST_ASSERT(dst_ex->values[0].asgn_type != ASGN_MOVE);

		dst_item = PlnDstItem::createDstItem(dst_ex, false);
		dst_item->place = output_place;
		dst_item->setSrcEx(da, si, src_ex);
		src_ex->finish(da, si);
	}

	void finishD(PlnDataAllocator& da, PlnScopeInfo& si) override {
		dst_item->finish(da,si);
	}

	void genS(PlnGenerator& g) override {
		src_ex->gen(g);
	}

	void genD(PlnGenerator& g) override {
		dst_item->gen(g);
	}
};

