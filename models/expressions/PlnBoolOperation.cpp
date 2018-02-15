/// Bool operation model class definition.
///
/// e.g.) a || b / a && b
///
/// @file	PlnBoolOperation.cpp
/// @copyright	2018 YAMAGUCHI Toshinobu 

#include "PlnBoolOperation.h"
#include "PlnCmpOperation.h"
#include "../PlnType.h"
#include "../PlnModule.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnGenerator.h"
#include "../../PlnConstants.h"
#include "../../PlnScopeStack.h"

#include "boost/assert.hpp"

PlnAndOperation::PlnAndOperation(PlnExpression* l, PlnExpression* r)
	: PlnExpression(ET_AND), jmp_end_id(-1), lcmp_type(-1),
		result_dp(NULL), rconst_dp(NULL)
{
	PlnValue v;
	v.type = VL_WORK;
	v.lval_type = NO_LVL;
	v.inf.wk_type = PlnType::getSint();
	values.push_back(v);

	if (l->type != ET_CMP) {
		this->l = new PlnCmpOperation(new PlnExpression(int64_t(0)), l, CMP_NE);
	} else {
		this->l = static_cast<PlnCmpOperation*>(l);
	}

	if (r->type != ET_CMP) {
		this->r = new PlnCmpOperation(new PlnExpression(int64_t(0)), r, CMP_NE);
	} else {
		this->r = static_cast<PlnCmpOperation*>(r);
	}
}

void PlnAndOperation::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	// Current implementation expect return a result.
	BOOST_ASSERT(data_places.size()==1);

	PlnModule* m = si.scope[0].inf.module;

	l->finish(da, si);
	lcmp_type = l->getCmpType();
	if (lcmp_type == CMP_CONST_FALSE) {
		if (data_places.size()) {
			result_dp = da.getLiteralIntDp(0);
			da.pushSrc(data_places[0], result_dp, true);
		}
		return;
	}

	if (lcmp_type != CMP_CONST_TRUE) {
		jmp_end_id = m->getJumpID();
	}

	r->finish(da, si);
	rcmp_type = r->getCmpType();

	bool is_rconst = (rcmp_type == CMP_CONST_TRUE || rcmp_type == CMP_CONST_FALSE);
	if (lcmp_type == CMP_CONST_TRUE && is_rconst) {
		result_dp = da.getLiteralIntDp(rcmp_type == CMP_CONST_TRUE ? 1 : 0);
		da.pushSrc(data_places[0], result_dp, true);
		return;
	}

	if (is_rconst) {
		rconst_dp = da.getLiteralIntDp(rcmp_type == CMP_CONST_TRUE ? 1 : 0);
	}

	if (data_places.size()) {
		result_dp = da.prepareAccumulator(DT_SINT);
		da.allocDp(result_dp);
		da.pushSrc(data_places[0], result_dp, true);
	}
}

void PlnAndOperation::dump(ostream& os, string indent)
{
	os << indent << "AND" << endl;
	l->dump(os, indent+" ");
	r->dump(os, indent+" ");
}

void PlnAndOperation::gen(PlnGenerator& g)
{
	bool do_eval_r = true;

	if (lcmp_type == CMP_CONST_FALSE) {
		do_eval_r = false;
		// Constant value is already set to src of data_place[0].
	} else if (lcmp_type == CMP_CONST_TRUE) {
		// Result is as right expression.
	} else {
		// Skip right expression if result is false.
		l->gen(g);
		lcmp_type = l->getCmpType();
		auto e = g.getEntity(result_dp);
		g.genMoveCmpFlag(e.get(), lcmp_type, "cmpflg -> " + result_dp->cmt());
		g.genFalseJump(jmp_end_id, lcmp_type, "left &&");
	}

	if (do_eval_r) {
		if (rcmp_type == CMP_CONST_TRUE || rcmp_type == CMP_CONST_FALSE) {
			if (lcmp_type == CMP_CONST_TRUE) {
				// Constant value is already set to src of data_place[0].

			} else {
				// Set constant value.
				BOOST_ASSERT(rconst_dp);
				auto const_e = g.getEntity(rconst_dp);
				auto result_e = g.getEntity(result_dp);
				g.genMove(result_e.get(), const_e.get(), "");
			}

		} else {
			r->gen(g);
			rcmp_type = r->getCmpType();
			auto e = g.getEntity(result_dp);
			g.genMoveCmpFlag(e.get(), rcmp_type, "cmpflg -> " + result_dp->cmt());
		}
	}

	if (jmp_end_id >= 0)
		g.genJumpLabel(jmp_end_id, "end &&");

	if (data_places.size()) {
		g.genSaveSrc(data_places[0]);
	}
}

