/// Assignment Item class definition.
///
/// A single Src variables of indirect object reference.
/// Need to clear reference to null after move ownership. 
///
/// @file	PlnAssignIndirectObjItem.h
/// @copyright	2018 YAMAGUCHI Toshinobu 

class PlnAssignIndirectObjItem : public PlnAssignItem
{
	PlnExpression* src_ex;
	PlnDstItem* dst_item;

public:
	PlnAssignIndirectObjItem(PlnExpression* ex) : src_ex(ex), dst_item(NULL) {
		BOOST_ASSERT(ex->values[0].type == VL_VAR);
		BOOST_ASSERT(ex->values[0].inf.var->ptr_type & PTR_REFERENCE);
		BOOST_ASSERT(ex->values[0].inf.var->ptr_type & PTR_INDIRECT_ACCESS);

	}
	
	void addDstEx(PlnExpression* ex) override {
		BOOST_ASSERT(dst_item == NULL);
		BOOST_ASSERT(ex->values[0].type == VL_VAR);
		BOOST_ASSERT(ex->values[0].inf.var->ptr_type & PTR_REFERENCE);

		dst_item = PlnDstItem::createDstItem(ex);
	}

	void finishS(PlnDataAllocator& da, PlnScopeInfo& si) override {
		if (dst_item->getAssginType() == ASGN_COPY) {
			dst_item->setSrcEx(da, si, src_ex);
			if (src_ex->data_places.size()) {
				src_ex->finish(da, si);
			}
		} else {
			BOOST_ASSERT(false);
		}
	}

	void finishD(PlnDataAllocator& da, PlnScopeInfo& si) override {
		dst_item->finish(da, si);
		if (dst_item->getAssginType() == ASGN_MOVE) {
			// Mark as freed variable.
			auto var = src_ex->values[0].inf.var;
			if (si.exists_current(var))
				si.set_lifetime(var, VLT_FREED);
		}
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

