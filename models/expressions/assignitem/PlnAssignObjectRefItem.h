/// Assignment Item class definition.
///
/// A single Src variables of object reference (not indirect access).
/// Need to clear reference to null after move ownership. 
/// Saving src for swap assignment (e.g. a,b -> b,a).
///
/// @file	PlnAssignObjectRefItem.h
/// @copyright	2018-2019 YAMAGUCHI Toshinobu 

class PlnAssignObjectRefItem : public PlnAssignItem
{
	PlnExpression* src_ex;
	PlnClone* src_save;
	PlnDstItem* dst_item;

public:
	PlnAssignObjectRefItem(PlnExpression* ex)
			: src_ex(ex), src_save(NULL), dst_item(NULL) {
		BOOST_ASSERT((ex->values[0].type == VL_VAR && !(ex->values[0].inf.var->is_indirect))
				|| ex->values[0].type == VL_LIT_STR);
		BOOST_ASSERT(ex->getDataType(0) == DT_OBJECT_REF);
	}

	~PlnAssignObjectRefItem() {
		delete src_save;
		delete dst_item;
	}
	
	void addDstEx(PlnExpression* ex, bool need_save) override {
		BOOST_ASSERT(dst_item == NULL);

		dst_item = PlnDstItem::createDstItem(ex, need_save);
	}

	int addDataPlace(vector<PlnDataPlace*> &data_places, int start_ind) override {
		dst_item->place = data_places[start_ind];
		return start_ind + 1;
	}

	void finishS(PlnDataAllocator& da, PlnScopeInfo& si) override {
		int assin_type = dst_item->getAssginType();
		if (dst_item->need_save && assin_type == ASGN_COPY) {
			src_save = new PlnClone(da, src_ex, src_ex->values[0].inf.var->var_type, true);
			dst_item->setSrcEx(da, si, src_save);
			src_save->finishAlloc(da, si);
			src_ex->finish(da, si);
			src_save->finish(da, si);

		} else {
			dst_item->setSrcEx(da, si, src_ex);
			src_ex->finish(da, si);
		}
		if (assin_type == ASGN_MOVE) {
			// Mark as freed variable.
			auto var = src_ex->values[0].inf.var;
			if (!si.exists_current(var))
				si.push_owner_var(var);
			si.set_lifetime(var, VLT_FREED);
		}
	}

	void finishD(PlnDataAllocator& da, PlnScopeInfo& si) override {
		dst_item->finish(da, si);
		if (src_save)
			src_save->finishFree(da, si);
	}

	void genS(PlnGenerator& g) override {
		if (src_save)
			src_save->genAlloc(g);
		src_ex->gen(g);
		if (src_save)
			src_save->gen(g);
	}

	void genD(PlnGenerator& g) override {
		dst_item->gen(g);
		if (src_save)
			src_save->genFree(g);
	}
};

