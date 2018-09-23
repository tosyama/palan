/// Assignment Item class definition.
///
/// @file	PlnDstMoveIndirectObjItem.h
/// @copyright	2018 YAMAGUCHI Toshinobu 

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
		BOOST_ASSERT(ex->values[0].inf.var->ptr_type & (PTR_REFERENCE | PTR_INDIRECT_ACCESS));
	}

	PlnAsgnType getAssginType() override { return ASGN_MOVE; }

	void setSrcEx(PlnDataAllocator &da, PlnScopeInfo& si, PlnExpression *src_ex) override {
		auto lt = si.get_lifetime(dst_ex->values[0].inf.var);
		if (lt == VLT_ALLOCED || lt == VLT_INITED) {
			// set destination address to save place.
			vector<PlnType*> t = dst_ex->values[0].inf.var->var_type;
			addr_var = PlnVariable::createTempVar(da, t, "(save addr)");
			addr_var->place->load_address = true;
			dst_ex->data_places.push_back(addr_var->place);
			
			// for free.
			save4free_var = PlnVariable::createTempVar(da, t, "(save for free)");
			free_ex = PlnFreer::getFreeEx(save4free_var);

			free_dp = new PlnDataPlace(8, DT_OBJECT_REF);
			free_dp->comment = &addr_var->name;
			da.setIndirectObjDp(free_dp, da.prepareObjBasePtr(), NULL);

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
			da.popSrc(save4free_var->place);

			// execute free.	
			free_ex->finish(da, si);
			da.releaseDp(save4free_var->place);

			// move src to dst
			da.setIndirectObjDp(dst_dp, da.prepareObjBasePtr(), NULL);
			da.pushSrc(dst_dp->data.indirect.base_dp, addr_var->place, false);
			da.popSrc(dst_dp);
			
		} else {
			// This case not exitsts currently.
			BOOST_ASSERT(false);
			/*
			dst_ex->finish(da, si);
			da.popSrc(dst_dp);
			*/
		}

		if (place) {
			dst_dp->data.indirect.base_dp->release_src_pop = true;
			da.pushSrc(place, dst_dp);

		} else {
			da.releaseDp(addr_var->place);
			da.releaseDp(dst_dp);
		}
	}

	void gen(PlnGenerator& g) override {
		if (addr_var) {
			dst_ex->gen(g);
			g.genLoadDp(addr_var->place);

			g.genSaveSrc(free_dp->data.indirect.base_dp);
			g.genLoadDp(save4free_var->place);
			free_ex->gen(g);
			
			g.genSaveSrc(dst_dp->data.indirect.base_dp);
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

