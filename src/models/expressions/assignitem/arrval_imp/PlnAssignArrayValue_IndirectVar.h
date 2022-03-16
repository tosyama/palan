/// Assignment Item class definition.
///
/// @file	PlnAssignArrayValue_IndirectVar.h
/// @copyright	2019-2021 YAMAGUCHI Toshinobu 

class PlnAssignArrayValue_IndirectVar : public PlnAssignItem {
	PlnArrayValue* src_ex;
	PlnExpression* dst_ex;

	vector<PlnAssignItem*> assign_items;

	PlnVariable* tmp_var;
	PlnExpression* alloc_ex;
	PlnExpression *free_ex;
	PlnDstItem *dst_item;
	PlnExpression *tmp_var_ex;

	PlnDataPlace *output_place;
	PlnAssignItem *assign_item_imp;

public:
	PlnAssignArrayValue_IndirectVar(PlnExpression* ex)
		: tmp_var(NULL), alloc_ex(NULL), dst_item(NULL), free_ex(NULL), output_place(NULL),
		  assign_item_imp(NULL) {
		BOOST_ASSERT(ex->type = ET_ARRAYVALUE);
		src_ex = static_cast<PlnArrayValue*>(ex);
	}

	~PlnAssignArrayValue_IndirectVar() {
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
		BOOST_ASSERT (ex->type == ET_ARRAYITEM || ex->type == ET_STRUCTMEMBER);
		BOOST_ASSERT(ex->values[0].inf.var->var_type->typeinf->type == TP_FIXED_ARRAY
				|| ex->values[0].inf.var->var_type->typeinf->type == TP_STRUCT);

		dst_ex = ex;
	}

	int addDataPlace(vector<PlnDataPlace*> &data_places, int start_ind) override {
		output_place = data_places[start_ind];
		return start_ind + 1;
	}

	void finishS(PlnDataAllocator& da, PlnScopeInfo& si) override {
		BOOST_ASSERT(dst_ex->values[0].asgn_type != ASGN_MOVE);

		PlnVarType *tmp_vartype = dst_ex->values[0].inf.var->var_type;

		if (tmp_vartype->data_type() == DT_OBJECT) {
			// ex) [3]#[2][1]int32 a9  = [[1][2]][[3][4]][[5][6]];
			tmp_vartype = tmp_vartype->getVarType();
		}

		tmp_var = PlnVariable::createTempVar(da, tmp_vartype, "(tmp var)");
		BOOST_ASSERT(tmp_var->place->data_type == DT_OBJECT_REF);
		BOOST_ASSERT(tmp_var->place->size == 8);

		vector<PlnExpression*> args;
		tmp_var->var_type->getAllocArgs(args);
		alloc_ex = tmp_var->var_type->getAllocEx(args);
		alloc_ex->data_places.push_back(tmp_var->place);
		alloc_ex->finish(da, si);
		da.popSrc(tmp_var->place);

		tmp_var_ex = new PlnExpression(tmp_var);
		dst_item = PlnDstItem::createDstItem(dst_ex, false);
		if (output_place)
			dst_item->place = output_place;
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
	}

	void finishD(PlnDataAllocator& da, PlnScopeInfo& si) override {
		tmp_var_ex->finish(da,si);
		dst_item->finish(da,si);

		free_ex = tmp_var->getFreeEx();
		free_ex->finish(da, si);
		da.releaseDp(tmp_var->place);
	}

	void genS(PlnGenerator& g) override {
		alloc_ex->gen(g);
		g.genLoadDp(tmp_var->place);

		for (auto ai: assign_items) {
			ai->genS(g);
			ai->genD(g);
		}
	}

	void genD(PlnGenerator& g) override {
		tmp_var_ex->gen(g);
		dst_item->gen(g);
		free_ex->gen(g);
	}
};

