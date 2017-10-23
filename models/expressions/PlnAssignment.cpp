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
PlnAssignment::PlnAssignment(vector<PlnValue>& lvals, vector<PlnExpression*>& exps)
	: PlnExpression(ET_ASSIGN), expressions(move(exps))
{
	values = move(lvals);
}

void PlnAssignment::finish(PlnDataAllocator& da)
{
	int i=0, ei=0, ai=0;
	for (auto e: expressions) {
		for (auto v: e->values) {
			if (i >= values.size()) break;
			if (values[i].inf.var->ptr_type == NO_PTR) {
				auto dp = values[i].inf.var->place;
				e->data_places.push_back(dp);

			} else {
				union PlnAssignInf as_inf;

				as_inf.mcopy.exp_ind = ei;
				as_inf.mcopy.val_ind = i;
				da.prepareMemCopyDps(as_inf.mcopy.dst, as_inf.mcopy.src);
				e->data_places.push_back(as_inf.mcopy.src);
				if (v.type == VL_WORK && v.inf.wk_type->data_type == DT_OBJECT_REF)
					as_inf.mcopy.free_dp = new PlnDataPlace(8, DT_OBJECT_REF);
				else
					as_inf.mcopy.free_dp = NULL;

				auto t = values[i].inf.var->var_type.back();

				if (t->name == "[]") {
					int item_size = t->inf.fixedarray.item_size;
					int asize = 0;
					for (int i: *t->inf.fixedarray.sizes)
						asize += i;
					asize *= item_size;
					as_inf.mcopy.cp_size = asize;

				} else BOOST_ASSERT(false);
				assign_inf.push_back(as_inf);
			}
			i++;
		}
		e->finish(da);
		for (int aii = ai; aii<assign_inf.size(); aii++)
			da.allocDp(assign_inf[aii].mcopy.src);

		for (; ai<assign_inf.size(); ai++) {
			auto& as_inf = assign_inf[ai];
			da.allocDp(as_inf.mcopy.dst);
			if (as_inf.mcopy.free_dp)
				da.allocData(as_inf.mcopy.free_dp);
			da.memCopyed(as_inf.mcopy.dst, as_inf.mcopy.src);
			if (as_inf.mcopy.free_dp) {
				da.memFreed();
				da.releaseData(as_inf.mcopy.free_dp);
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

void PlnAssignment::gen(PlnGenerator& g)
{
	int ai=0;
	for (int i=0; i<expressions.size(); ++i) {
		expressions[i]->gen(g);
		while(ai < assign_inf.size() && assign_inf[ai].mcopy.exp_ind <= i) {
			BOOST_ASSERT(i == ai);
			auto as_inf = assign_inf[ai].mcopy;
			auto dp = values[i].inf.var->place;
			auto cpy_dste = g.getPopEntity(as_inf.dst);
			auto cpy_srce = g.getPopEntity(as_inf.src);
			if (as_inf.free_dp) {
				auto fe = g.getPopEntity(as_inf.free_dp);
				g.genMove(fe.get(), cpy_srce.get(), *as_inf.src->comment + " -> save for free");
			}
			auto ve = g.getPopEntity(dp);
			g.genMove(cpy_dste.get(), ve.get(), *dp->comment + " -> " + *as_inf.dst->comment);
			static string cmt = "deep copy";
			g.genMemCopy(as_inf.cp_size, cmt);
			if (as_inf.free_dp) {
				static string cmt = "work";
				auto fe = g.getPopEntity(as_inf.free_dp);
				g.genMemFree(fe.get(), cmt, false);
			}

			ai++;
		}
	}
	PlnExpression::gen(g);
}
