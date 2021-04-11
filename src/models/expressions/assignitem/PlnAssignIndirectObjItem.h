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
	PlnClone* src_save;
	PlnDstItem* dst_item;

public:
	PlnAssignIndirectObjItem(PlnExpression* ex)
			: src_ex(ex), src_save(NULL), dst_item(NULL) {
		BOOST_ASSERT(ex->values[0].type == VL_VAR);
		BOOST_ASSERT(ex->values[0].inf.var->var_type->data_type() == DT_OBJECT_REF);
		BOOST_ASSERT(ex->values[0].inf.var->is_indirect);
	}

	~PlnAssignIndirectObjItem() {
		delete src_save;
		delete dst_item;
	}
	
	void addDstEx(PlnExpression* ex, bool need_save) override {
		BOOST_ASSERT(dst_item == NULL);
		BOOST_ASSERT(ex->values[0].type == VL_VAR);
		BOOST_ASSERT(ex->values[0].inf.var->var_type->data_type() == DT_OBJECT_REF
				|| ex->values[0].inf.var->var_type->data_type() == DT_OBJECT);

		dst_item = PlnDstItem::createDstItem(ex, need_save);
	}

	int addDataPlace(vector<PlnDataPlace*> &data_places, int start_ind) override {
		dst_item->place = data_places[start_ind];
		return start_ind + 1;
	} 

	void finishS(PlnDataAllocator& da, PlnScopeInfo& si) override {
		if (dst_item->getAssginType() == ASGN_COPY) {
			if (dst_item->need_save) {
				src_save = new PlnClone(da, src_ex, src_ex->values[0].inf.var->var_type, true);
				dst_item->setSrcEx(da, si, src_save);
				src_save->finishAlloc(da, si);
				src_ex->finish(da, si);
				src_save->finish(da, si);

			} else {
				dst_item->setSrcEx(da, si, src_ex);
				if (src_ex->data_places.size()) {
					src_ex->finish(da, si);
				} else
					;	// use cop_ex case.
			}

		} else if (dst_item->getAssginType() == ASGN_MOVE) {
			dst_item->setSrcEx(da, si, src_ex);
			src_ex->finish(da, si);

		} else if (dst_item->getAssginType() == ASGN_COPY_REF) {
			dst_item->setSrcEx(da, si, src_ex);
			src_ex->finish(da, si);

		} else {
			BOOST_ASSERT(false);
		}
	}

	void finishD(PlnDataAllocator& da, PlnScopeInfo& si) override {
		dst_item->finish(da, si);

		if (dst_item->getAssginType() == ASGN_MOVE) {
		 	auto container_var = src_ex->values[0].inf.var->container;
			BOOST_ASSERT(container_var);
			if (!si.exists_current(container_var)) {
				si.push_owner_var(container_var);
			}
			si.set_lifetime(container_var, VLT_PARTLY_FREED);
		}

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

