/// Assignment Item class definition.
///
/// @file	PlnDstMoveIndirectObjItem.h
/// @copyright	2019-2020 YAMAGUCHI Toshinobu 

class PlnDstMoveIndirectObjItem: public PlnDstItem
{
	PlnExpression *dst_ex;
	PlnDataPlace *dst_dp;

	PlnVariable *addr_var;
	PlnVariable *save4free_var;
	PlnDataPlace *free_dp;
	PlnExpression *free_ex;

public:
	PlnDstMoveIndirectObjItem(PlnExpression* ex)
		: dst_ex(ex), dst_dp(NULL), addr_var(NULL), save4free_var(NULL), free_dp(NULL), free_ex(NULL)
	{
		BOOST_ASSERT(ex->values.size() == 1);
		BOOST_ASSERT(ex->values[0].type == VL_VAR);
		BOOST_ASSERT(ex->getDataType(0) == DT_OBJECT_REF);
		BOOST_ASSERT(ex->values[0].inf.var->is_indirect);
	}

	~PlnDstMoveIndirectObjItem() {
		delete addr_var;
		delete save4free_var;
		delete free_ex;
	}

	PlnAsgnType getAssginType() override { return ASGN_MOVE; }

	PlnFinishRole setSrcEx(PlnDataAllocator &da, PlnScopeInfo& si, PlnExpression *src_ex) override {
		auto lt = si.get_lifetime(dst_ex->values[0].inf.var);
		if (lt == VLT_ALLOCED || lt == VLT_INITED || lt == VLT_PARTLY_FREED) {
			// set destination address to save place.
			PlnVarType* t = dst_ex->values[0].inf.var->var_type;
			addr_var = PlnVariable::createTempVar(da, t, "(save addr)");
			addr_var->place->load_address = true;
			dst_ex->data_places.push_back(addr_var->place);
			
			// for free.
			save4free_var = PlnVariable::createTempVar(da, t, "(save for free)");
			free_ex = save4free_var->getFreeEx();

			free_dp = new PlnDataPlace(8, DT_OBJECT_REF);
			free_dp->comment = &addr_var->name;
			da.setIndirectObjDp(free_dp, da.prepareObjBasePtr(), NULL, 0);

			// for receiving value from src.
			dst_dp = new PlnDataPlace(8, DT_OBJECT_REF);
			dst_dp->comment = &addr_var->name;
			dst_dp->do_clear_src = true;
			src_ex->data_places.push_back(dst_dp);

		} else {
			// This case not exitsts currently.
			BOOST_ASSERT(false);
			/*
			dst_dp = dst_ex->values[0].getDataPlace(da);
			dst_dp->do_clear_src = true;
			src_ex->data_places.push_back(dst_dp);
			*/
		}
		return FINISH_BY_ASSIGNITEM;
	}

	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override {
		if (addr_var) {
			if (need_save)
				da.allocSaveData(dst_dp, dst_dp->push_src_step, dst_dp->release_step);

			dst_ex->finish(da, si);	
			da.popSrc(addr_var->place); // save addr.
			
			// save old dst for free.
			da.pushSrc(free_dp->data.indirect.base_dp, addr_var->place, false);
			da.pushSrc(save4free_var->place, free_dp);
			da.popSrc(free_dp->data.indirect.base_dp);
			da.popSrc(save4free_var->place);

			// execute free.	
			free_ex->finish(da, si);
			da.releaseDp(save4free_var->place);

			// move src to dst
			da.setIndirectObjDp(dst_dp, da.prepareObjBasePtr(), NULL, 0);
			da.pushSrc(dst_dp->data.indirect.base_dp, addr_var->place, false);
			da.popSrc(dst_dp->data.indirect.base_dp);
			da.popSrc(dst_dp);
			
		} else {
			// This case not exitsts currently.
			BOOST_ASSERT(false);
			/*
			dst_ex->finish(da, si);
			da.popSrc(dst_dp);
			*/
		}

		da.releaseDp(addr_var->place);
		if (place)
			da.pushSrc(place, dst_dp);
		else
			da.releaseDp(dst_dp);
	}

	void gen(PlnGenerator& g) override {
		if (addr_var) {
			dst_ex->gen(g);
			g.genLoadDp(addr_var->place);

			g.genSaveSrc(free_dp->data.indirect.base_dp);
			g.genLoadDp(free_dp->data.indirect.base_dp);
			g.genLoadDp(save4free_var->place);
			free_ex->gen(g);
			
			g.genLoadDp(dst_dp->data.indirect.base_dp);
			g.genLoadDp(dst_dp);

		} else {
			// This case not exitsts currently.
			BOOST_ASSERT(false);
			/*
			dst_ex->gen(g);
			g.genLoadDp(dst_dp);
			*/
		}
		if (place)
			g.genSaveSrc(place);
	}
};

