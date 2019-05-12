/// Assignment Item class definition.
///
/// @file	PlnAssignArrayValue.h
/// @copyright	2019 YAMAGUCHI Toshinobu 

class PlnAssignArrayValue : public PlnAssignItem {
	PlnArrayValue* src_ex;
	PlnExpression* dst_ex;

	vector<PlnAssignItem*> assign_items;

	PlnVariable* tmp_var;
	PlnExpression* alloc_ex;
	PlnExpression *free_ex;
	PlnDstItem *dst_item;
	PlnExpression *tmp_var_ex;

public:
	PlnAssignArrayValue(PlnExpression* ex)
		: dst_ex(NULL), alloc_ex(NULL), dst_item(NULL), free_ex(NULL) {
		BOOST_ASSERT(ex->type = ET_ARRAYVALUE);
		src_ex = static_cast<PlnArrayValue*>(ex);
	}

	~PlnAssignArrayValue() {
		// TODO: delete some.
	}
	
	void addDstEx(PlnExpression* ex, bool need_save) override {

		BOOST_ASSERT(ex->values[0].type == VL_VAR);
		BOOST_ASSERT(ex->values[0].getType()->data_type == DT_OBJECT_REF);
		BOOST_ASSERT(!need_save);

		dst_ex = ex;
	}

	int addDataPlace(vector<PlnDataPlace*> &data_places, int start_ind) override {
		dst_ex->data_places.push_back(data_places[start_ind]);
		return start_ind + 1;
	}

	void finishS(PlnDataAllocator& da, PlnScopeInfo& si) override {
		if (dst_ex->values[0].asgn_type == ASGN_MOVE) {
			PlnCompileError err(E_CantUseMoveOwnership, PlnMessage::arrayValue());
			err.loc = src_ex->loc;
			throw err;
		}

		if (dst_ex->type == ET_VALUE) {
			PlnVariable* var = dst_ex->values[0].inf.var;
			BOOST_ASSERT(var->var_type->type == TP_FIXED_ARRAY);
			if (si.get_lifetime(var) == VLT_FREED) {
				PlnCompileError err(E_CantCopyFreedVar, var->name);
				err.loc = dst_ex->loc;
				throw err;
			}
		}

		if (src_ex->doCopyFromStaticBuffer) {
			dst_item = PlnDstItem::createDstItem(dst_ex, false);
			dst_item->setSrcEx(da, si, src_ex);
			src_ex->finish(da, si);

		} else {
			if (dst_ex->type == ET_VALUE) {
				vector<PlnExpression*> val_items = src_ex->getAllItems();
				vector<PlnExpression*> dst_items = PlnArrayItem::getAllArrayItems(dst_ex->values[0].inf.var);
				BOOST_ASSERT(val_items.size() == dst_items.size());

				for (int i=0; i<val_items.size(); i++) {
					PlnAssignItem *ai = PlnAssignItem::createAssignItem(val_items[i]);
					dst_items[i]->values[0].asgn_type = ASGN_COPY;
					ai->addDstEx(dst_items[i], false);
					assign_items.push_back(ai);
				}

				for (auto ai: assign_items) {
					ai->finishS(da, si);
				}

			} else if (dst_ex->type == ET_ARRAYITEM) {
				tmp_var = PlnVariable::createTempVar(da, dst_ex->values[0].inf.var->var_type, "tmp var");
				alloc_ex = tmp_var->var_type->allocator->getAllocEx();
				alloc_ex->data_places.push_back(tmp_var->place);
				alloc_ex->finish(da, si);
				da.popSrc(tmp_var->place);

				tmp_var_ex = new PlnExpression(tmp_var);
				dst_item = PlnDstItem::createDstItem(dst_ex, false);
				dst_item->setSrcEx(da, si, tmp_var_ex);

				vector<PlnExpression*> val_items = src_ex->getAllItems();
				vector<PlnExpression*> dst_items = PlnArrayItem::getAllArrayItems(tmp_var);
				BOOST_ASSERT(val_items.size() == dst_items.size());
				for (int i=0; i<val_items.size(); i++) {
					PlnAssignItem *ai = PlnAssignItem::createAssignItem(val_items[i]);
					dst_items[i]->values[0].asgn_type = ASGN_COPY;
					ai->addDstEx(dst_items[i], false);
					assign_items.push_back(ai);
				}

				for (auto ai: assign_items) {
					ai->finishS(da, si);
					ai->finishD(da, si);
				}

			} else
				BOOST_ASSERT(false);
		}
	}

	void finishD(PlnDataAllocator& da, PlnScopeInfo& si) override {
		if (src_ex->doCopyFromStaticBuffer) {
			dst_item->finish(da,si);

		} else {
			if (dst_ex->type == ET_VALUE) {
				for (auto ai: assign_items) {
					ai->finishD(da, si);
				}
				dst_ex->finish(da, si);

			} else if (dst_ex->type == ET_ARRAYITEM) {
				tmp_var_ex->finish(da,si);
				dst_item->finish(da,si);

				free_ex = PlnFreer::getFreeEx(tmp_var);
				free_ex->finish(da, si);
				da.releaseDp(tmp_var->place);
			}
		}
	}

	void genS(PlnGenerator& g) override {
		if (src_ex->doCopyFromStaticBuffer) {
			src_ex->gen(g);

		} else {
			if (dst_ex->type == ET_VALUE) {
				for (auto ai: assign_items) {
					ai->genS(g);
				}

			} else {
				alloc_ex->gen(g);
				g.genLoadDp(tmp_var->place);

				for (auto ai: assign_items) {
					ai->genS(g);
					ai->genD(g);
				}
			}
		}
	}

	void genD(PlnGenerator& g) override {
		if (src_ex->doCopyFromStaticBuffer) {
			dst_item->gen(g);
			return;
		} else {
			if (dst_ex->type == ET_VALUE) {
				for (auto ai: assign_items) {
					ai->genD(g);
				}
				dst_ex->gen(g);

			} else {
				tmp_var_ex->gen(g);
				dst_item->gen(g);
				free_ex->gen(g);
			}
		}
	}
};

