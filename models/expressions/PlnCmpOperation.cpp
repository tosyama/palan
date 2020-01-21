/// Compare operation model class definition.
///
/// PlnCmpOperation compare values .
/// e.g.) a == b / a > b
///
/// @file	PlnCmpOperation.cpp
/// @copyright	2018-2020 YAMAGUCHI Toshinobu 

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
	v.inf.wk_type = PlnType::getSint()->getVarType();
	values.push_back(v);
}

PlnCmpOperation::~PlnCmpOperation()
{
	delete l;
	delete r;
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

	PlnDataPlace *ldp, *rdp;
	int acm_data_type = l->getDataType();
	if (r->getDataType() == DT_FLOAT)
		acm_data_type = DT_FLOAT;

	ldp = da.prepareAccumulator(acm_data_type);

	if (r->type == ET_VALUE) {
		rdp = r->values[0].getDataPlace(da);
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

template <typename T1, typename T2>
int static_comp(T1 lval, T2 rval, int cmp_type)
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

#define CREATE_CHECK_FLAG(ex)	bool is_##ex##_int = false, is_##ex##_uint = false, is_##ex##_flo = false;	\
	union {int64_t i; uint64_t u; double d;} ex##val; \
	if (ex->type == ET_VALUE) { \
		switch (ex->values[0].type) { \
			case VL_LIT_INT8: is_##ex##_int = true; \
				ex##val.i = ex->values[0].inf.intValue; break;\
			case VL_LIT_UINT8: is_##ex##_uint = true; \
				ex##val.u = ex->values[0].inf.uintValue; break; \
			case VL_LIT_FLO8: is_##ex##_flo = true; \
				ex##val.d = ex->values[0].inf.floValue; break; \
		} \
	} \
	bool is_##ex##_num_lit = is_##ex##_int || is_##ex##_uint || is_##ex##_flo;

bool PlnCmpOperation::isConst()
{
	CREATE_CHECK_FLAG(l);
	CREATE_CHECK_FLAG(r);

	if (!is_l_num_lit || !is_r_num_lit)
		return false;

	if (is_l_uint && is_r_uint)
		gen_cmp_type = static_comp(lval.u, rval.u, cmp_type);
	else if ((is_l_int || is_l_uint) && (is_r_int || is_r_uint))
		gen_cmp_type = static_comp(lval.i, rval.i, cmp_type);
	else if (is_l_flo && is_r_flo)
		gen_cmp_type = static_comp(lval.d, rval.d, cmp_type);
	else if (is_l_flo && is_r_int)
		gen_cmp_type = static_comp(lval.d, rval.i, cmp_type);
	else if (is_l_int && is_r_flo)
		gen_cmp_type = static_comp(lval.i, rval.d, cmp_type);
	else if (is_l_flo && is_r_uint)
		gen_cmp_type = static_comp(lval.d, rval.u, cmp_type);
	else if (is_l_uint && is_r_flo)
		gen_cmp_type = static_comp(lval.u, rval.d, cmp_type);
	else
		BOOST_ASSERT(false);
	
	return true;
}
