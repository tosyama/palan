/// Assignment Item class definition.
///
/// Src value don't need to be clear.
///
/// @file	PlnAssignPrimitiveItem.h
/// @copyright	2018 YAMAGUCHI Toshinobu 

class PlnAssignPrimitiveItem : public PlnAssignItem {
	PlnExpression* src_ex;
	PlnDstItem* dst_item;

public:
	PlnAssignPrimitiveItem(PlnExpression* ex) : src_ex(ex), dst_item(NULL) {
	}
	
	void addDstEx(PlnExpression* ex, bool need_save) override {
		BOOST_ASSERT(dst_item == NULL);
		BOOST_ASSERT(ex->values[0].type == VL_VAR);
		BOOST_ASSERT(ex->values[0].getType()->data_type != DT_OBJECT_REF);
		dst_item = PlnDstItem::createDstItem(ex, need_save);
	}

	int addDataPlace(vector<PlnDataPlace*> &data_places, int start_ind) override {
		dst_item->place = data_places[start_ind];
		return start_ind + 1;
	}

	void finishS(PlnDataAllocator& da, PlnScopeInfo& si) override {
		dst_item->setSrcEx(da, si, src_ex);
		src_ex->finish(da, si);
	}

	void finishD(PlnDataAllocator& da, PlnScopeInfo& si) override {
		dst_item->finish(da, si);
	}

	void genS(PlnGenerator& g) override {
		src_ex->gen(g);
	}

	void genD(PlnGenerator& g) override {
		dst_item->gen(g);
	}
};

