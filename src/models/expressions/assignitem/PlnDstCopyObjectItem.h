/// Assignment Item class definition.
///
/// A single Dst variables of object reference for deep copy. 
///
/// @file	PlnDstCopyObjectItem.h
/// @copyright	2018-2022 YAMAGUCHI Toshinobu 

class PlnDstCopyObjectItem : public PlnDstItem
{
	PlnExpression *dst_ex;
	PlnExpression *cpy_ex;

	PlnVariable *src_tmp_var;
	PlnVariable *dst_tmp_var;

public:
	PlnDstCopyObjectItem(PlnExpression* ex)
		: dst_ex(ex), cpy_ex(NULL), //cpy_dst_dp(NULL), tmp_var(NULL),
			src_tmp_var(NULL), dst_tmp_var(NULL) {
		BOOST_ASSERT(ex->values.size() == 1);
		BOOST_ASSERT(ex->values[0].type == VL_VAR);
		BOOST_ASSERT(ex->getDataType(0) == DT_OBJECT || ex->getDataType(0) == DT_OBJECT_REF);
		auto var = dst_ex->values[0].inf.var;
	}

	~PlnDstCopyObjectItem() {
		//delete cpy_ex; // TODO: problem: duplicate delete at MEMCOPY
		delete src_tmp_var;
		delete dst_tmp_var;
	}

	PlnAsgnType getAssginType() override { return ASGN_COPY; }

	PlnFinishRole setSrcEx(PlnDataAllocator &da, PlnScopeInfo& si, PlnExpression *src_ex) override {
		PlnFinishRole src_finish_role = FINISH_BY_ASSIGNITEM;
		PlnExpression *cpy_src_ex = src_ex;
		PlnExpression *cpy_dst_ex = dst_ex;

		if (src_ex->type == ET_VALUE || src_ex->type == ET_CLONE) {
			BOOST_ASSERT(src_ex->values[0].type == VL_VAR
				|| src_ex->values[0].type == VL_LIT_ARRAY
				|| src_ex->values[0].type == VL_LIT_STR
				);
			src_finish_role = FINISH_BY_DSTITEM;

		} else {
			int val_ind = src_ex->data_places.size();
			PlnVarType *tmp_vartype = src_ex->values[val_ind].getVarType()->getVarType("rir");
			src_tmp_var = PlnVariable::createTempVar(da, tmp_vartype, "(src tmp var)");
			cpy_src_ex = new PlnExpression(src_tmp_var);

			src_ex->data_places.push_back(src_tmp_var->place);
		}

		if (dst_ex->type != ET_VALUE && place) {
			PlnVarType *tmp_vartype = dst_ex->values[0].inf.var->var_type->typeinf->getVarType("rir");

			dst_tmp_var = PlnVariable::createTempVar(da, tmp_vartype, "(dst tmp var)");
			cpy_dst_ex = new PlnExpression(dst_tmp_var);
		}

		vector<PlnExpression*> args;
		dst_ex->values[0].getVarType()->getAllocArgs(args);
		cpy_ex = dst_ex->values[0].getVarType()->getCopyEx(cpy_dst_ex, cpy_src_ex, args);

		if (!cpy_ex) {
			PlnCompileError err(E_CantCopyType, dst_ex->values[0].getVarType()->name());
			err.loc = dst_ex->loc;
			throw err;
		}

		return src_finish_role;
	}

	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override {
		PlnVariable* var = dst_ex->values[0].inf.var;
		if (!var->is_tmpvar && var->var_type->mode[ALLOC_MD] == 'h') {
			PlnVarLifetime lt = si.get_lifetime(var);
			BOOST_ASSERT(lt != VLT_UNKNOWN); 
			if (lt == VLT_FREED) {
				PlnCompileError err(E_CantCopyFreedVar, var->name);
				err.loc = dst_ex->loc;
				throw err;
			}
		}

		if (src_tmp_var) {
			da.popSrc(src_tmp_var->place);
		}

		if (dst_tmp_var) {
			BOOST_ASSERT(place);

			dst_ex->data_places.push_back(dst_tmp_var->place);
			dst_tmp_var->place->do_clear_src = place->do_clear_src;
			place->do_clear_src = false;

			dst_ex->finish(da, si);
			da.popSrc(dst_tmp_var->place);
		}

		cpy_ex->finish(da, si);
		if (src_tmp_var) {
			da.releaseDp(src_tmp_var->place);
		}

		if (place) {
			if (dst_tmp_var) {
				da.pushSrc(place, dst_tmp_var->place);
			} else {
				PlnDataPlace *dp = dst_ex->values[0].getDataPlace(da);
				da.pushSrc(place, dp);
			}
		}
	}

	void gen(PlnGenerator& g) override {
		if (src_tmp_var) 
			g.genLoadDp(src_tmp_var->place);

		dst_ex->gen(g);

		if (dst_tmp_var) 
			g.genLoadDp(dst_tmp_var->place);

		cpy_ex->gen(g);

		if (place)
			g.genSaveDp(place);

		return;
	}
};

