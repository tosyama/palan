/// Assignment Item class definition.
///
/// @file	PlnAssignArrayValue.h
/// @copyright	2019-2021 YAMAGUCHI Toshinobu 

class PlnAssignArrayValue : public PlnAssignItem {
	PlnArrayValue* src_ex;
	PlnExpression* dst_ex;

	vector<PlnAssignItem*> assign_items;

	PlnVariable* tmp_var;
	PlnExpression* alloc_ex;
	PlnExpression *free_ex;
	PlnDstItem *dst_item;
	PlnExpression *tmp_var_ex;

	PlnDataPlace *place;

public:
	PlnAssignArrayValue(PlnExpression* ex)
		: tmp_var(NULL), alloc_ex(NULL), dst_item(NULL), free_ex(NULL), place(NULL) {
		BOOST_ASSERT(ex->type = ET_ARRAYVALUE);
		src_ex = static_cast<PlnArrayValue*>(ex);
	}

	~PlnAssignArrayValue() {
		if (tmp_var) {
			delete tmp_var;
			delete alloc_ex;
			delete free_ex;
			delete tmp_var_ex;
		}
		delete dst_item;
		for (auto ai: assign_items)
			delete ai;
	}
	
	void addDstEx(PlnExpression* ex, bool need_save) override {

		BOOST_ASSERT(ex->values[0].type == VL_VAR);
		BOOST_ASSERT(ex->values[0].getVarType()->data_type() == DT_OBJECT_REF
				|| ex->values[0].getVarType()->data_type() == DT_OBJECT);
		BOOST_ASSERT(!need_save);

		dst_ex = ex;
	}

	int addDataPlace(vector<PlnDataPlace*> &data_places, int start_ind) override {
		place = data_places[start_ind];
		return start_ind + 1;
	}

	void finishS(PlnDataAllocator& da, PlnScopeInfo& si) override {
		BOOST_ASSERT(dst_ex->values[0].asgn_type != ASGN_MOVE);

		if (dst_ex->type == ET_VALUE) {
			PlnVariable* var = dst_ex->values[0].inf.var;
			if (var->var_type->mode[2] == 'h' && si.get_lifetime(var) == VLT_FREED) {
				PlnCompileError err(E_CantCopyFreedVar, var->name);
				err.loc = dst_ex->loc;
				throw err;
			}
		}

		if (src_ex->doCopyFromStaticBuffer) {
			dst_item = PlnDstItem::createDstItem(dst_ex, false);
			dst_item->setSrcEx(da, si, src_ex);
			dst_item->place = place;
			src_ex->finish(da, si);

		} else {
			PlnVarType* t = dst_ex->values[0].inf.var->var_type;
			if (t->data_type() == DT_OBJECT_REF && t->mode[ALLOC_MD] != 'h') {
				// case @[3]int32 a = [1,x];
				PlnCompileError err(E_CantUseDynamicVal, dst_ex->values[0].inf.var->name);
				err.loc =  dst_ex->loc;
				throw err;
			}

			if (place)
				dst_ex->data_places.push_back(place);

			if (dst_ex->type == ET_VALUE) {
				PlnVariable* dst_var = dst_ex->values[0].inf.var;
				vector<PlnExpression*> val_items = src_ex->getAllItems();
				vector<PlnExpression*> dst_items;
				if (dst_var->var_type->typeinf->type == TP_FIXED_ARRAY) {
					dst_items = PlnArrayItem::getAllArrayItems(dst_ex->values[0].inf.var);

				} else if (dst_var->var_type->typeinf->type == TP_STRUCT) {
					dst_items = PlnStructMember::getAllStructMembers(dst_ex->values[0].inf.var);

				} else
					BOOST_ASSERT(false);

				BOOST_ASSERT(val_items.size() == dst_items.size());

				for (int i=0; i<val_items.size(); i++) {
					PlnAssignItem *ai = PlnAssignItem::createAssignItem(val_items[i]);
					PlnVarType* dt = dst_items[i]->values[0].getVarType();
					if (dt->data_type() == DT_OBJECT_REF && dt->mode[2] != 'h')
						dst_items[i]->values[0].asgn_type = ASGN_COPY_REF;
					else
						dst_items[i]->values[0].asgn_type = ASGN_COPY;
					ai->addDstEx(dst_items[i], false);
					assign_items.push_back(ai);
				}

				for (auto ai: assign_items) {
					ai->finishS(da, si);
				}

			} else if (dst_ex->type == ET_ARRAYITEM
					|| dst_ex->type == ET_STRUCTMEMBER) {
				tmp_var = PlnVariable::createTempVar(da, dst_ex->values[0].inf.var->var_type, "tmp var");
				alloc_ex = tmp_var->var_type->getAllocEx();
				alloc_ex->data_places.push_back(tmp_var->place);
				alloc_ex->finish(da, si);
				da.popSrc(tmp_var->place);

				tmp_var_ex = new PlnExpression(tmp_var);
				dst_item = PlnDstItem::createDstItem(dst_ex, false);
				dst_item->setSrcEx(da, si, tmp_var_ex);

				vector<PlnExpression*> val_items = src_ex->getAllItems();
				vector<PlnExpression*> dst_items;
				if (tmp_var->var_type->typeinf->type == TP_FIXED_ARRAY) {
					dst_items = PlnArrayItem::getAllArrayItems(tmp_var);

				} else if (tmp_var->var_type->typeinf->type == TP_STRUCT) {
					dst_items = PlnStructMember::getAllStructMembers(tmp_var);

				} else
					BOOST_ASSERT(false);

				BOOST_ASSERT(val_items.size() == dst_items.size());
				for (int i=0; i<val_items.size(); i++) {
					PlnAssignItem *ai = PlnAssignItem::createAssignItem(val_items[i]);
					if (dst_items[i]->values[0].getVarType()->mode[ALLOC_MD] == 'r') {
						BOOST_ASSERT(dst_items[i]->values[0].getVarType()->mode == "rir");
						dst_items[i]->values[0].asgn_type = ASGN_COPY_REF;
					} else {
						dst_items[i]->values[0].asgn_type = ASGN_COPY;
					}
				
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

			} else if (dst_ex->type == ET_ARRAYITEM || dst_ex->type == ET_STRUCTMEMBER) {
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

