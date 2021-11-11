/// Assignment Item class definition.
///
/// A single Dst variables of object reference for deep copy. 
///
/// @file	PlnDstCopyObjectItem.h
/// @copyright	2018-2019 YAMAGUCHI Toshinobu 

class PlnDstCopyObjectItem : public PlnDstItem
{
	PlnExpression *dst_ex;
	PlnDeepCopyExpression *cpy_ex;
	PlnDataPlace *cpy_dst_dp;
	PlnVariable *tmp_var;

public:
	PlnDstCopyObjectItem(PlnExpression* ex)
		: dst_ex(ex), cpy_ex(NULL), cpy_dst_dp(NULL), tmp_var(NULL) {
		BOOST_ASSERT(ex->values.size() == 1);
		BOOST_ASSERT(ex->values[0].type == VL_VAR);
		BOOST_ASSERT(ex->getDataType(0) == DT_OBJECT || ex->getDataType(0) == DT_OBJECT_REF);
		auto var = dst_ex->values[0].inf.var;
	}

	~PlnDstCopyObjectItem() {
		delete cpy_ex;
		delete tmp_var;
	}

	PlnAsgnType getAssginType() override { return ASGN_COPY; }

	void setSrcEx(PlnDataAllocator &da, PlnScopeInfo& si, PlnExpression *src_ex) override {
		cpy_ex = dst_ex->values[0].getVarType()->getCopyEx();
		if (!cpy_ex) {
			PlnCompileError err(E_CantCopyType, dst_ex->values[0].getVarType()->name());
			err.loc = dst_ex->loc;
			throw err;
		}
		src_ex->data_places.push_back(cpy_ex->srcDp(da));
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

		cpy_dst_dp = cpy_ex->dstDp(da);

		if (place && dst_ex->type != ET_VALUE) {
			PlnVarType *tmp_vartype = dst_ex->values[0].inf.var->var_type;

			if (tmp_vartype->data_type() == DT_OBJECT) {
				tmp_vartype = tmp_vartype->typeinf->getVarType();
			}

			tmp_var = PlnVariable::createTempVar(da, tmp_vartype, "tmp var");
			dst_ex->data_places.push_back(tmp_var->place);
			tmp_var->place->do_clear_src = place->do_clear_src;
			place->do_clear_src = false;

		} else {
			dst_ex->data_places.push_back(cpy_dst_dp);
		}

		dst_ex->finish(da, si);

		if (tmp_var) {
			da.popSrc(tmp_var->place);
			da.pushSrc(cpy_dst_dp, tmp_var->place, false);
		}

		cpy_ex->finish(da, si);

		if (place) {
			if (tmp_var) {
				da.pushSrc(place, tmp_var->place);
			} else {
				PlnDataPlace *dp = dst_ex->values[0].getDataPlace(da);
				da.pushSrc(place, dp);
			}
		} else {
			// no this case.
			/* if (tmp_var)
				da.releaseDp(tmp_var->place); */
		}
	}

	void gen(PlnGenerator& g) override {
		dst_ex->gen(g);
		
		if (tmp_var) 
			g.genLoadDp(tmp_var->place);

		cpy_ex->gen(g);

		if (place)
			g.genSaveDp(place);
	}
};

