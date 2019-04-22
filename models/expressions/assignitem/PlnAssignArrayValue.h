/// Assignment Item class definition.
///
/// @file	PlnAssignArrayValue.h
/// @copyright	2019 YAMAGUCHI Toshinobu 

class PlnAssignArrayValue : public PlnAssignItem {
	PlnArrayValue* src_ex;
	PlnExpression* dst_ex;
	PlnDataPlace* var_dp;
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

		if (src_ex->isLiteral) {
			dst_ex = ex;
			dst_item = PlnDstItem::createDstItem(ex, need_save);

		} else {
			if (ex->type == ET_VALUE) {
				dst_ex = ex;

			} else {
				BOOST_ASSERT(ex->type == ET_ARRAYITEM);
				dst_ex = ex;
				dst_item = PlnDstItem::createDstItem(ex, need_save);
			}
		}
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

		if (src_ex->isLiteral) {
			dst_item->setSrcEx(da, si, src_ex);

		} else {
			if (dst_ex->type == ET_VALUE) {
				PlnVariable* var = dst_ex->values[0].inf.var;
				var_dp = da.getSeparatedDp(var->place);
				src_ex->data_places.push_back(var_dp);

			} else if (dst_ex->type == ET_ARRAYITEM) {
				tmp_var = PlnVariable::createTempVar(da, dst_ex->values[0].inf.var->var_type, "tmp var");
				alloc_ex = tmp_var->var_type->allocator->getAllocEx();
				alloc_ex->data_places.push_back(tmp_var->place);
				alloc_ex->finish(da, si);
				da.popSrc(tmp_var->place);
				var_dp = tmp_var->place;
				free_ex = PlnFreer::getFreeEx(tmp_var);
				tmp_var_ex = new PlnExpression(tmp_var);
				dst_item->setSrcEx(da, si, tmp_var_ex);
				src_ex->data_places.push_back(var_dp);

			} else
				BOOST_ASSERT(false);
		}

		src_ex->finish(da, si);
	}

	void finishD(PlnDataAllocator& da, PlnScopeInfo& si) override {
		if (src_ex->isLiteral) {
			dst_item->finish(da,si);
			return;
		}
		if (!dst_item) dst_ex->finish(da, si);

		// src_ex->finishD(da, si);
		if (dst_item) {
			tmp_var_ex->finish(da,si);
			dst_item->finish(da,si);
		}

		if (free_ex) free_ex->finish(da, si);
		da.releaseDp(var_dp);
	}

	void genS(PlnGenerator& g) override {
		if (alloc_ex) {
			alloc_ex->gen(g);
			g.genLoadDp(tmp_var->place);
		}
		src_ex->gen(g);
	}

	void genD(PlnGenerator& g) override {
		if (src_ex->isLiteral) {
			dst_item->gen(g);
			return;
		}
		if (!dst_item) dst_ex->gen(g);

		if (dst_item) {
			tmp_var_ex->gen(g);
			dst_item->gen(g);
		}
		if (free_ex) free_ex->gen(g);
	}
};

