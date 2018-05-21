/// Compare operation model class definition.
///
/// PlnCmpOperation compare values .
/// e.g.) a == b / a > b
///
/// @file	PlnCmpOperation.cpp
/// @copyright	2018 YAMAGUCHI Toshinobu 

#include "PlnCmpOperation.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnGenerator.h"
#include "../../PlnConstants.h"
#include "../PlnType.h"

#include "boost/assert.hpp"

PlnCmpOperation::PlnCmpOperation(PlnExpression* l, PlnExpression* r, PlnCmpType cmp_type)
	:result_dp(NULL), l(l), r(r), cmp_type(cmp_type), PlnCmpExpression(ET_CMP)
{
	PlnValue v;
	v.type = VL_WORK;
	v.asgn_type = NO_ASGN;
	v.inf.wk_type = new vector<PlnType*>();
	v.inf.wk_type->push_back(PlnType::getSint());
	values.push_back(v);
}

void PlnCmpOperation::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	BOOST_ASSERT(data_places.size() <= 1);

	if (isConst()) {
		if (data_places.size()) {
			result_dp = da.getLiteralIntDp(gen_cmp_type==CMP_CONST_TRUE?1:0);
			da.pushSrc(data_places[0], result_dp, true);
		}
		return;
	}

	bool alloc_acm = false;
	PlnDataPlace *ldp, *rdp;

	if (l->type == ET_VALUE) {
		ldp = l->values[0].getDataPlace(da);
	} else {
		ldp = da.prepareAccumulator(l->getDataType());
		alloc_acm = true;
	}

	if (r->type == ET_VALUE) {
		rdp = r->values[0].getDataPlace(da);
	} else if (!alloc_acm) {
		rdp = da.prepareAccumulator(r->getDataType());
	} else {
		rdp = da.prepareLocalVar(8, r->getDataType());
		static string cmt="(temp)";
		rdp->comment = &cmt;
	}

	l->data_places.push_back(ldp);
	l->finish(da, si);
	
	r->data_places.push_back(rdp);
	r->finish(da, si);

	da.popSrc(rdp);
	da.popSrc(ldp);

	da.releaseDp(rdp);
	da.releaseDp(ldp);

	if (data_places.size()) {
		result_dp = da.prepareAccumulator(DT_SINT);
		da.allocDp(result_dp);
		da.pushSrc(data_places[0], result_dp, true);
	}
}

void PlnCmpOperation::gen(PlnGenerator& g)
{
	if (gen_cmp_type == CMP_CONST_TRUE
			|| gen_cmp_type == CMP_CONST_FALSE) {
		if (data_places.size())
			g.genSaveSrc(data_places[0]);
		return;
	}

	l->gen(g);
	r->gen(g);

	auto ldp = l->data_places[0];
	auto rdp = r->data_places[0];

	g.genLoadDp(rdp);
	g.genLoadDp(ldp);

	auto re = g.getEntity(rdp);
	auto le = g.getEntity(ldp);

	gen_cmp_type = g.genCmp(le.get(), re.get(), cmp_type, ldp->cmt() + " == " + rdp->cmt());

	if (data_places.size()) {
		auto e = g.getEntity(result_dp);
		g.genMoveCmpFlag(e.get(), gen_cmp_type, "cmpflg -> " + result_dp->cmt());
		g.genSaveSrc(data_places[0]);
	}
}

template <typename T>
int static_comp(T lval, T rval, int cmp_type)
{
	bool result;
	switch (cmp_type) {
		case CMP_EQ: result = (lval == rval); break;
		case CMP_NE: result = (lval != rval); break;
		case CMP_L: result = (lval < rval); break;
		case CMP_G: result = (lval > rval); break;
		case CMP_LE: result = (lval <= rval); break;
		case CMP_GE: result = (lval >= rval); break;
		default:
			BOOST_ASSERT(false);
	}
	return  result ? CMP_CONST_TRUE : CMP_CONST_FALSE;
}

bool PlnCmpOperation::isConst()
{
	if (l->type != ET_VALUE || r->type != ET_VALUE)
		return false;

	int lval_t = l->values[0].type;
	int rval_t = r->values[0].type;
	auto lval = l->values[0].inf;
	auto rval = r->values[0].inf;

	if ((lval_t == VL_LIT_INT8 && (rval_t == VL_LIT_INT8 || rval_t == VL_LIT_UINT8))
			|| (lval_t == VL_LIT_UINT8 && rval_t == VL_LIT_INT8))
		gen_cmp_type = static_comp(lval.intValue, rval.intValue, cmp_type);

	else if (lval_t == VL_LIT_UINT8 && rval_t == VL_LIT_UINT8)
		gen_cmp_type = static_comp(lval.uintValue, rval.uintValue, cmp_type);

	else
		return false;

	return true;
}
