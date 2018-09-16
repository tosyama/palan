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
	
	void addDstEx(PlnExpression* ex, bool need_save) override {
		BOOST_ASSERT(dst_item == NULL);
		BOOST_ASSERT(ex->values[0].type == VL_VAR);
		BOOST_ASSERT(ex->values[0].inf.var->ptr_type & PTR_REFERENCE);

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
	}
};

