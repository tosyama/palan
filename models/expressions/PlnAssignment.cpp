/// Assignment model class definition.
///
/// PlnAssignment store values to variables.
/// e.g.) a = 2;
///
/// @file	PlnAssignment.cpp
/// @copyright	2017 YAMAGUCHI Toshinobu 

#include <iostream>
#include <boost/assert.hpp>
#include "PlnAssignment.h"
#include "../PlnVariable.h"
#include "../PlnType.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnGenerator.h"
#include "../../PlnConstants.h"
#include "../../PlnScopeStack.h"

// PlnAssignment
PlnAssignment::PlnAssignment(vector<PlnExpression*>& lvals, vector<PlnExpression*>& exps)
	: lvals(move(lvals)), PlnExpression(ET_ASSIGN), expressions(move(exps))
{
	for (auto e: this->lvals) {
		BOOST_ASSERT(e->values.size() == 1);
		BOOST_ASSERT(e->type == ET_VALUE || e->type == ET_ARRAYITEM);
		BOOST_ASSERT(e->values[0].type == VL_VAR);
		values.push_back(e->values[0]);
	}
}

enum {	// AssignInf.type
	AI_PRIMITIVE,
	AI_MCOPY,
	AI_MOVE
};

inline union PlnAssignInf getDeepCopyAssignInf(PlnDataAllocator& da, PlnValue& dst_val, PlnValue& src_val)
{
	union PlnAssignInf as_inf;

	as_inf.type = AI_MCOPY;
	da.prepareMemCopyDps(as_inf.mcopy.dst, as_inf.mcopy.src);

	// If src is work objects, should be free after copied. 
	if (src_val.type == VL_WORK)
		as_inf.mcopy.free_dp = new PlnDataPlace(8, DT_OBJECT_REF);
	else
		as_inf.mcopy.free_dp = NULL;

	PlnType *t = dst_val.getType();
	if (t->inf.obj.is_fixed_size) 
		as_inf.mcopy.cp_size = t->inf.obj.alloc_size;
	else BOOST_ASSERT(false);

	return as_inf;
}

inline union PlnAssignInf getMoveOwnerAssignInf(PlnDataAllocator& da, PlnScopeInfo& si, PlnValue& dst_val, PlnValue& src_val)
{
	union PlnAssignInf as_inf;

	as_inf.type = AI_MOVE;
	as_inf.move.dst = dst_val.inf.var->place;

	if (src_val.type == VL_WORK) {
		// work var
		static string cmt = "save";
		as_inf.move.src = new PlnDataPlace(8, DT_OBJECT_REF);
		as_inf.move.src->comment = &cmt;
		as_inf.move.do_clear_var = NULL; 
		as_inf.move.save_indirect = NULL;

	} else {
		// normal var
		BOOST_ASSERT(src_val.type == VL_VAR);
		as_inf.move.src = src_val.inf.var->place;
		as_inf.move.do_clear_var = src_val.inf.var;
		if (dst_val.inf.var->ptr_type & PTR_INDIRECT_ACCESS) {
			auto save_dp = new PlnDataPlace(8, DT_OBJECT_REF);
			auto base_dp = da.prepareObjBasePtr();

			da.setIndirectObjDp(save_dp, base_dp, NULL);
			as_inf.move.save_indirect = save_dp;

		} else {
			as_inf.move.save_indirect = NULL;
		}

		auto lt = si.get_lifetime(dst_val.inf.var);
		if (lt == VLT_ALLOCED || lt == VLT_INITED)
			as_inf.move.do_free_dst = true;
		else
			as_inf.move.do_free_dst = false;

	}

	return as_inf;
}

inline PlnDataPlace* prepareAssignInf(PlnDataAllocator& da, PlnScopeInfo& si, union PlnAssignInf* as_inf, PlnValue& dst_val, PlnValue& src_val)
{
	if (dst_val.inf.var->ptr_type == NO_PTR) {
		as_inf->type = AI_PRIMITIVE;
		as_inf->inf.dp = dst_val.getDataPlace(da);
		return as_inf->inf.dp;

	}
	
	// (ptr_type & PTR_REFERNCE)
	BOOST_ASSERT(dst_val.getType()->data_type == DT_OBJECT_REF);
	BOOST_ASSERT(src_val.getType()->data_type == DT_OBJECT_REF);

	PlnDataPlace *ret_dp;
	switch (dst_val.lval_type) {
		case LVL_COPY:
			*as_inf = getDeepCopyAssignInf(da, dst_val, src_val);
			ret_dp = as_inf->mcopy.src;
			break;

		case LVL_MOVE: // Move ownership: Free dst var -> Copy address(src->dst) -> Clear src (if not work var)
			*as_inf = getMoveOwnerAssignInf(da, si, dst_val, src_val);
			ret_dp = as_inf->move.src;
			break;

		case LVL_REF:
			as_inf->type = AI_PRIMITIVE;
			as_inf->inf.dp = dst_val.getDataPlace(da);
			ret_dp = as_inf->inf.dp;
			break;	

		default: BOOST_ASSERT(false);
	}
	return ret_dp;
}

void PlnAssignment::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	int vcnt=0;
	int vi=0;
	for (auto e: expressions) {
		for (auto src_val: e->values) {
			if (vcnt >= values.size()) break;
			auto& dst_val = values[vcnt];
			BOOST_ASSERT(dst_val.type == VL_VAR);

			union PlnAssignInf as_inf;
			auto dst_dp = prepareAssignInf(da, si, &as_inf, dst_val, src_val);
			e->data_places.push_back(dst_dp);
			assign_inf.push_back(as_inf);

			vcnt++;
		}

		e->finish(da, si);

		// for save returned reference.
		for (int i=vi; i<vcnt; i++) {
			auto &as_inf = assign_inf[i];
			switch (as_inf.type) {
				case AI_PRIMITIVE:
					break;
				case AI_MCOPY: 
					da.allocDp(as_inf.mcopy.src);	// would be released by memCopyed.
					break;

				case AI_MOVE:
					if (!as_inf.move.do_clear_var) 
						da.allocData(as_inf.move.src); // for save work.
					break;
			}
		}

		for (int i=vi; i<vcnt; i++)
			lvals[i]->finish(da, si);

		// assign process
		for (int i=vi; i<vcnt; i++) {
			auto& as_inf = assign_inf[i];
			switch(as_inf.type) {
				case AI_PRIMITIVE:
					da.popSrc(as_inf.inf.dp);
					break;

				case AI_MCOPY:
					da.popSrc(as_inf.mcopy.src);
					da.allocDp(as_inf.mcopy.dst);	// would be released by memCopyed.
					if (as_inf.mcopy.free_dp) {
						da.allocData(as_inf.mcopy.free_dp);
						da.memCopyed(as_inf.mcopy.dst, as_inf.mcopy.src);
						da.memFreed();
						da.releaseData(as_inf.mcopy.free_dp);
					} else 
						da.memCopyed(as_inf.mcopy.dst, as_inf.mcopy.src);
					break;
				
				case AI_MOVE:
					da.popSrc(as_inf.move.src);
					if (as_inf.move.save_indirect) {
						da.allocDp(as_inf.move.save_indirect->data.indirect.base_dp);
						da.memFreed();
						da.releaseData(as_inf.move.save_indirect->data.indirect.base_dp);
					} else {
						da.memFreed();
					}

					if (auto var = as_inf.move.do_clear_var) {
						if (si.exists_current(var))
							si.set_lifetime(var, VLT_FREED);
					} else {
						da.releaseData(as_inf.move.src);
					}

					auto dst_var = values[i].inf.var;
					if (si.exists_current(dst_var))
						si.set_lifetime(dst_var, VLT_INITED);

					break;
			}
		}
		vi=vcnt;
	}
}

void PlnAssignment::dump(ostream& os, string indent)
{
	os << indent << "Assign:";
	for (auto lv: values)
		os << " " << lv.inf.var->name;
	os << endl;
	for (auto e: expressions)
		e->dump(os, indent+" ");	
}

static void genDeepCopy4Assign(PlnGenerator &g, PlnAssignInf &as, PlnDataPlace *var_dp);
static void genMoveOwner4Assign(PlnGenerator &g, PlnExpression *lval_ex, PlnAssignInf &as);

void PlnAssignment::gen(PlnGenerator& g)
{
	int vi=0;
	int vcnt = 0;
	for (auto e: expressions) {
		vcnt += e->values.size();
		if (vcnt > lvals.size()) vcnt = lvals.size();

		e->gen(g);

		for (int di=0; vi < vcnt; vi++, di++) {
		
			switch (assign_inf[vi].type) {
				case AI_PRIMITIVE:
					lvals[vi]->gen(g);
					g.genLoadDp(assign_inf[vi].inf.dp);
					break;

				case AI_MCOPY:
					lvals[vi]->gen(g);
					genDeepCopy4Assign(g, assign_inf[vi], values[vi].inf.var->place);
					break;

				case AI_MOVE:
					genMoveOwner4Assign(g, lvals[vi], assign_inf[vi]);
					break;
				default:
					BOOST_ASSERT(false);
			}
		}
	}
	PlnExpression::gen(g);
}

void genDeepCopy4Assign(PlnGenerator &g, PlnAssignInf &as, PlnDataPlace *var_dp)
{
	auto as_inf = as.mcopy;
	g.genLoadDp(as_inf.src);

	auto cpy_dste = g.getEntity(as_inf.dst);
	auto cpy_srce = g.getEntity(as_inf.src);

	// save src(work object) for free
	if (as_inf.free_dp) {
		auto fe = g.getEntity(as_inf.free_dp);
		g.genMove(fe.get(), cpy_srce.get(), *as_inf.src->comment + " -> save for free");
	}

	// get dest
	auto ve = g.getEntity(var_dp);
	g.genMove(cpy_dste.get(), ve.get(), *var_dp->comment + " -> " + *as_inf.dst->comment);

	// copy
	static string cmt = "deep copy";
	g.genMemCopy(as_inf.cp_size, cmt);

	// free work object
	if (as_inf.free_dp) {
		static string cmt = "work";
		auto fe = g.getEntity(as_inf.free_dp);
		g.genMemFree(fe.get(), cmt, false);
	}
}

void genMoveOwner4Assign(PlnGenerator &g, PlnExpression *lval_ex, PlnAssignInf &as)
{
	auto as_inf = as.move;

	g.genLoadDp(as_inf.src);

	lval_ex->gen(g);
	auto dste = g.getEntity(as_inf.dst);
	auto srce = g.getEntity(as_inf.src);

	// Free losted owner variable.
	static string cmt = "lost ownership ";
	if (as_inf.save_indirect && as_inf.do_free_dst) {
		// TODO: Use genLoadDp.
		auto savei_base_e = g.getEntity(as_inf.save_indirect->data.indirect.base_dp);
		g.genLoadAddress(savei_base_e.get(), dste.get(), "save address of " + as_inf.dst->cmt());

		auto savei_e = g.getEntity(as_inf.save_indirect);
		g.genMemFree(savei_e.get(), cmt, false);

		// assign value.
		g.genMove(savei_e.get(), srce.get(), *as_inf.src->comment + " -> " + *as_inf.dst->comment);

	} else {
		if (as_inf.do_free_dst)
			g.genMemFree(dste.get(), cmt + as_inf.dst->cmt(), false);

		// assign value.
		g.genMove(dste.get(), srce.get(), *as_inf.src->comment + " -> " + *as_inf.dst->comment);
	}

	if (as_inf.do_clear_var) {
		vector<unique_ptr<PlnGenEntity>> clr_es;
		clr_es.push_back(g.getEntity(as_inf.src));
		g.genNullClear(clr_es);
	}
}
