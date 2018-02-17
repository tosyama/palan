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

// PlnBoolOperation
PlnBoolOperation::PlnBoolOperation(PlnExpression* l, PlnExpression* r, PlnExprsnType type)
	: PlnExpression(type), jmp_end_id(-1), lcmp_type(-1),
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

void PlnBoolOperation::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	// Current implementation expect return a result.
	BOOST_ASSERT(data_places.size()==1);

	PlnModule* m = si.scope[0].inf.module;

	int lskip_const, lthrogh_const;
	int lskip_value;
	switch (type) {
		case ET_AND:
			lskip_const = CMP_CONST_FALSE;
			lthrogh_const = CMP_CONST_TRUE;
			lskip_value = 0;
			break;
		case ET_OR:
			lskip_const = CMP_CONST_TRUE;
			lthrogh_const = CMP_CONST_FALSE;
			lskip_value = 1;
			break;
		default:
			BOOST_ASSERT(false);
	}

	l->finish(da, si);
	lcmp_type = l->getCmpType();
	if (lcmp_type == lskip_const) {
		if (data_places.size()) {
			result_dp = da.getLiteralIntDp(lskip_value);
			da.pushSrc(data_places[0], result_dp, true);
		}
		return;
	}

	if (lcmp_type != lthrogh_const) {
		jmp_end_id = m->getJumpID();
	}

	r->finish(da, si);
	rcmp_type = r->getCmpType();

	bool is_rconst = (rcmp_type == CMP_CONST_TRUE || rcmp_type == CMP_CONST_FALSE);
	if (lcmp_type == lthrogh_const && is_rconst) {
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

void PlnBoolOperation::dump(ostream& os, string indent)
{
	if (type == ET_AND)
		os << indent << "AND" << endl;
	else
		os << indent << "OR" << endl;

	l->dump(os, indent+" ");
	r->dump(os, indent+" ");
}

void PlnBoolOperation::gen(PlnGenerator& g)
{
	bool do_eval_r = true;
	int lskip_const, lthrogh_const;
	switch (type) {
		case ET_AND:
			lskip_const = CMP_CONST_FALSE;
			lthrogh_const = CMP_CONST_TRUE;
			break;
		case ET_OR:
			lskip_const = CMP_CONST_TRUE;
			lthrogh_const = CMP_CONST_FALSE;
			break;
		default:
			BOOST_ASSERT(false);
	}

	if (lcmp_type == lskip_const) {
		do_eval_r = false;
		// Constant value is already set to src of data_place[0].
	} else if (lcmp_type == lthrogh_const) {
		// Result is as right expression.
	} else {
		// Skip right expression if result is false.
		l->gen(g);
		lcmp_type = l->getCmpType();
		auto e = g.getEntity(result_dp);
		g.genMoveCmpFlag(e.get(), lcmp_type, "cmpflg -> " + result_dp->cmt());
		if (type == ET_AND)
			g.genFalseJump(jmp_end_id, lcmp_type, "&&");
		else // ET_OR
			g.genTrueJump(jmp_end_id, lcmp_type, "||");
	}

	if (do_eval_r) {
		if (rcmp_type == CMP_CONST_TRUE || rcmp_type == CMP_CONST_FALSE) {
			if (lcmp_type == lthrogh_const) {
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
		g.genJumpLabel(jmp_end_id, type==ET_AND ? "end &&" : "end ||");

	if (data_places.size()) {
		g.genSaveSrc(data_places[0]);
	}
}

PlnExpression* PlnBoolOperation::getNot(PlnExpression *e)
{
	if (e->type == ET_CMP) {
		auto ce = static_cast<PlnCmpOperation*>(e);
		PlnCmpType cmp_type;
		switch (ce->cmp_type) {
			case CMP_EQ: cmp_type=CMP_NE; break;
			case CMP_NE: cmp_type=CMP_EQ; break;
			case CMP_L: cmp_type=CMP_GE; break;
			case CMP_G: cmp_type=CMP_LE; break;
			case CMP_LE: cmp_type=CMP_G; break;
			case CMP_GE: cmp_type=CMP_L; break;
			case CMP_A: cmp_type=CMP_BE; break;
			case CMP_B: cmp_type=CMP_AE; break;
			case CMP_AE: cmp_type=CMP_B; break;
			case CMP_BE: cmp_type=CMP_A; break;
			case CMP_CONST_TRUE: cmp_type=CMP_CONST_FALSE; break;
			case CMP_CONST_FALSE: cmp_type=CMP_CONST_TRUE; break;
			default: BOOST_ASSERT(false);
		}
		ce->cmp_type = cmp_type;
		return ce;
	}
	return new PlnCmpOperation(new PlnExpression(int64_t(0)), e, CMP_EQ);
}
