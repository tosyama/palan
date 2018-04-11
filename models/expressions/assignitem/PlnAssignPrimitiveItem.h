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
	
	void addDstEx(PlnExpression* ex) override {
		BOOST_ASSERT(dst_item == NULL);
		BOOST_ASSERT(ex->values[0].type == VL_VAR);
		BOOST_ASSERT(ex->values[0].getType()->data_type != DT_OBJECT_REF);
		dst_item = PlnDstItem::createDstItem(ex);
	}

	void finishS(PlnDataAllocator& da, PlnScopeInfo& si) override {
		src_ex->data_places.push_back(dst_item->getInputDataPlace(da));
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

