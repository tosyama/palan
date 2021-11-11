/// Assignment Item class definition.
///
/// @file	PlnAssignArrayValue_Var.h
/// @copyright	2019-2021 YAMAGUCHI Toshinobu 

class PlnAssignArrayValue_Var : public PlnAssignItem {
	PlnArrayValue* src_ex;
	PlnExpression* dst_ex;

	vector<PlnAssignItem*> assign_items;
	PlnDataPlace *output_place;

public:
	PlnAssignArrayValue_Var(PlnExpression* ex)
		: dst_ex(NULL), output_place(NULL) {
		BOOST_ASSERT(ex->type = ET_ARRAYVALUE);
		src_ex = static_cast<PlnArrayValue*>(ex);
	}

	~PlnAssignArrayValue_Var() {
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
		output_place = data_places[start_ind];
		return start_ind + 1;
	}

	void finishS(PlnDataAllocator& da, PlnScopeInfo& si) override {
		BOOST_ASSERT(dst_ex->values[0].asgn_type != ASGN_MOVE);
		BOOST_ASSERT(dst_ex->type == ET_VALUE);

		if (output_place)
			dst_ex->data_places.push_back(output_place);

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
			if (dt->data_type() == DT_OBJECT_REF && dt->mode[ALLOC_MD] != 'h')
				dst_items[i]->values[0].asgn_type = ASGN_COPY_REF;
			else
				dst_items[i]->values[0].asgn_type = ASGN_COPY;
			ai->addDstEx(dst_items[i], false);
			assign_items.push_back(ai);
		}

		for (auto ai: assign_items) {
			ai->finishS(da, si);
		}
	}

	void finishD(PlnDataAllocator& da, PlnScopeInfo& si) override {
		for (auto ai: assign_items) {
			ai->finishD(da, si);
		}
		dst_ex->finish(da, si);
	}

	void genS(PlnGenerator& g) override {
		for (auto ai: assign_items) {
			ai->genS(g);
		}
	}

	void genD(PlnGenerator& g) override {
		for (auto ai: assign_items) {
			ai->genD(g);
		}
		dst_ex->gen(g);
	}
};

