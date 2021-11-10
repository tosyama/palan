/// Assignment Item class definition.
///
/// @file	PlnAssignArrayValue.h
/// @copyright	2019-2021 YAMAGUCHI Toshinobu 

#include "arrval_imp/PlnAssignArrayValue_Static.h"
#include "arrval_imp/PlnAssignArrayValue_Var.h"
#include "arrval_imp/PlnAssignArrayValue_IndirectVar.h"

class PlnAssignArrayValue : public PlnAssignItem {
	PlnArrayValue* src_ex;
	PlnExpression* dst_ex;
	PlnAssignItem *assign_item_imp;

public:
	PlnAssignArrayValue(PlnExpression* ex) : assign_item_imp(NULL) {
		BOOST_ASSERT(ex->type = ET_ARRAYVALUE);
		src_ex = static_cast<PlnArrayValue*>(ex);
	}

	~PlnAssignArrayValue() {
		if (assign_item_imp)
			delete assign_item_imp;
	}
	
	void addDstEx(PlnExpression* ex, bool need_save) override {
		BOOST_ASSERT(ex->values[0].type == VL_VAR);
		BOOST_ASSERT(ex->values[0].getVarType()->data_type() == DT_OBJECT_REF
				|| ex->values[0].getVarType()->data_type() == DT_OBJECT);
		BOOST_ASSERT(!need_save);

		dst_ex = ex;
		// create implementation object
		if (src_ex->doCopyFromStaticBuffer) {
			assign_item_imp = new PlnAssignArrayValue_Static(src_ex);

		} else {
			PlnVarType* t = dst_ex->values[0].inf.var->var_type;
			if (t->data_type() == DT_OBJECT_REF && t->mode[ALLOC_MD] != 'h') {
				// case @[3]int32 a = [1,x];
				PlnCompileError err(E_CantUseDynamicVal, dst_ex->values[0].inf.var->name);
				err.loc =  dst_ex->loc;
				throw err;
			}

			if (dst_ex->type == ET_VALUE) {
				assign_item_imp = new PlnAssignArrayValue_Var(src_ex);

			} else if (dst_ex->type == ET_ARRAYITEM || dst_ex->type == ET_STRUCTMEMBER) {
				assign_item_imp = new PlnAssignArrayValue_IndirectVar(src_ex);

			} else {
				BOOST_ASSERT(false);
			}
		}

		assign_item_imp->addDstEx(ex, need_save);
	}

	int addDataPlace(vector<PlnDataPlace*> &data_places, int start_ind) override {
		BOOST_ASSERT(assign_item_imp);
		return assign_item_imp->addDataPlace(data_places, start_ind);
	}

	void finishS(PlnDataAllocator& da, PlnScopeInfo& si) override {
		BOOST_ASSERT(assign_item_imp);
		BOOST_ASSERT(dst_ex->values[0].asgn_type != ASGN_MOVE);

		// Check if distination is freed variable.
		if (dst_ex->type == ET_VALUE) {
			PlnVariable* var = dst_ex->values[0].inf.var;
			if (var->var_type->mode[2] == 'h' && si.get_lifetime(var) == VLT_FREED) {
				PlnCompileError err(E_CantCopyFreedVar, var->name);
				err.loc = dst_ex->loc;
				throw err;
			}
		}

		assign_item_imp->finishS(da, si);
	}

	void finishD(PlnDataAllocator& da, PlnScopeInfo& si) override {
		assign_item_imp->finishD(da, si);
	}

	void genS(PlnGenerator& g) override {
		assign_item_imp->genS(g);
	}

	void genD(PlnGenerator& g) override {
		assign_item_imp->genD(g);
	}
};

