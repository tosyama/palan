/// Assignment model class definition.
///
/// PlnAssignment store values to variables.
/// e.g.) a = 2;
///
/// @file	PlnAssignment.cpp
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include <iostream>
#include <boost/assert.hpp>
#include "PlnAssignment.h"
#include "../PlnVariable.h"
#include "../PlnType.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnGenerator.h"
#include "../../PlnConstants.h"

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
	AI_MCOPY,
	AI_MOVE
};

void PlnAssignment::finish(PlnDataAllocator& da)
{
	int vi=0, i=0, ei=0, ai=0;
	int vcnt=0;
	for (auto e: expressions) {
		vcnt += e->values.size();
		if (vcnt > lvals.size()) vcnt = lvals.size();

		for (auto v: e->values) {
			if (i >= values.size()) break;
			BOOST_ASSERT(values[i].type == VL_VAR);

			if (values[i].inf.var->ptr_type == NO_PTR) {
				auto dp = values[i].getDataPlace(da);
				e->data_places.push_back(dp);

			} else {	// (ptr_type & PTR_REFERNCE)
				BOOST_ASSERT(values[i].getType()->data_type == DT_OBJECT_REF);
				BOOST_ASSERT(v.getType()->data_type == DT_OBJECT_REF);

				union PlnAssignInf as_inf;
				as_inf.inf.val_ind = i;

				if (values[i].lval_type == LVL_COPY)  {
					// Deep copy. if src is work objects, should be free after copied. 
					as_inf.type = AI_MCOPY;
					da.prepareMemCopyDps(as_inf.mcopy.dst, as_inf.mcopy.src);
					e->data_places.push_back(as_inf.mcopy.src);

					if (v.type == VL_WORK)
						as_inf.mcopy.free_dp = new PlnDataPlace(8, DT_OBJECT_REF);
					else
						as_inf.mcopy.free_dp = NULL;

					PlnType *t = values[i].getType();
					if (t->inf.obj.is_fixed_size) 
						as_inf.mcopy.cp_size = t->inf.obj.alloc_size;
					else BOOST_ASSERT(false);

				} else if (values[i].lval_type == LVL_MOVE)  {
					// Move ownership: Free dst var -> Copy address(src->dst) -> Clear src (if not work var)
					as_inf.type = AI_MOVE;
					as_inf.move.dst = values[i].inf.var->place;

					if (v.type == VL_WORK) {
						// work var
						static string cmt = "save";
						as_inf.move.src = new PlnDataPlace(8, DT_OBJECT_REF);
						as_inf.move.src->comment = &cmt;
						as_inf.move.do_clear = false; 

					} else {
						// normal var
						BOOST_ASSERT(v.type == VL_VAR);
						as_inf.move.src = v.inf.var->place;
						as_inf.move.do_clear = true;
					}

					e->data_places.push_back(as_inf.move.src);

				} else BOOST_ASSERT(false);

				assign_inf.push_back(as_inf);
			}
			i++;
		}

		e->finish(da);

		// for save returned reference.
		for (int aii = ai; aii<assign_inf.size(); aii++) {
			auto &as_inf = assign_inf[aii];
			switch (as_inf.type) {
				case AI_MCOPY: 
					da.allocDp(as_inf.mcopy.src);	// would be released by memCopyed.
					break;

				case AI_MOVE:
					if (!as_inf.move.do_clear) 
						da.allocData(as_inf.move.src);
					break;
			}
		}


		for (; vi < vcnt; vi++)
			lvals[vi]->finish(da);

		for (auto sdp: e->data_places)
			da.popSrc(sdp);

		// assign process
		for (; ai<assign_inf.size(); ai++) {
			auto& as_inf = assign_inf[ai];
			switch(as_inf.type) {
				case AI_MCOPY:
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
					da.memFreed();
					if (!as_inf.move.do_clear)
						da.releaseData(as_inf.move.src);
					break;
			}
		}

		ei++;
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

static void genMemoryCopy4Assign(PlnGenerator &g, PlnAssignInf &as, PlnDataPlace *var_dp);

void PlnAssignment::gen(PlnGenerator& g)
{
	int vi=0, ai=0;
	int vcnt = 0;
	for (int i=0; i<expressions.size(); ++i) {
		vcnt += expressions[i]->values.size();
		if (vcnt > lvals.size()) vcnt = lvals.size();

		expressions[i]->gen(g);

		vector<unique_ptr<PlnGenEntity>> clr_es;
		for (int di=0; vi < vcnt; vi++,di++) {
			lvals[vi]->gen(g);
			
			if (ai < assign_inf.size() && assign_inf[ai].inf.val_ind == vi) {
				if (assign_inf[ai].type == AI_MCOPY)
					genMemoryCopy4Assign(g, assign_inf[ai], values[i].inf.var->place);

				else if (assign_inf[ai].type == AI_MOVE) {
					auto as_inf = assign_inf[ai].move;

					g.genLoadDp(as_inf.src);

					auto dste = g.getEntity(as_inf.dst);
					static string cmt = "lost ownership";
					g.genMemFree(dste.get(), cmt, false);
					auto srce = g.getEntity(as_inf.src);
					g.genMove(dste.get(), srce.get(), *as_inf.src->comment + " -> " + *as_inf.dst->comment);

					if (as_inf.do_clear)
						clr_es.push_back(g.getEntity(as_inf.src));

				} else BOOST_ASSERT(false);

				if (clr_es.size())
					g.genNullClear(clr_es);

				ai++;
			} else {	// Normal copy
				g.genLoadDp(expressions[i]->data_places[di]);
			}
		}
	}
	PlnExpression::gen(g);
}

void genMemoryCopy4Assign(PlnGenerator &g, PlnAssignInf &as, PlnDataPlace *var_dp)
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

