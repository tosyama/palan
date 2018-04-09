/// Assignment Item class definition.
///
/// A single Src variables of object reference (not indirect access).
/// Need to clear reference to null after move ownership. 
///
/// @file	PlnAssignObjectRefItem.h
/// @copyright	2018 YAMAGUCHI Toshinobu 

class PlnAssignObjectRefItem : public PlnAssignItem
{
	PlnExpression* src_ex;
	PlnDstItem* dst_item;

public:
	PlnAssignObjectRefItem(PlnExpression* ex) : src_ex(ex), dst_item(NULL) {
		BOOST_ASSERT(ex->values[0].type == VL_VAR);
		BOOST_ASSERT(ex->values[0].inf.var->ptr_type & PTR_REFERENCE);
		BOOST_ASSERT(!(ex->values[0].inf.var->ptr_type & PTR_INDIRECT_ACCESS));

	}
	
	bool ready() override { return dst_item->ready(); };

	void addDstEx(PlnExpression* ex) override {
		BOOST_ASSERT(dst_item == NULL);
		BOOST_ASSERT(ex->values[0].type == VL_VAR);
		BOOST_ASSERT(ex->values[0].inf.var->ptr_type & PTR_REFERENCE);

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
		if (dst_item->getAssginType() == ASGN_MOVE) {
			vector<unique_ptr<PlnGenEntity>> clr_es;
			PlnDataPlace* dp = src_ex->values[0].inf.var->place;
			clr_es.push_back(g.getEntity(dp));
			g.genNullClear(clr_es);
		}
	}
};

