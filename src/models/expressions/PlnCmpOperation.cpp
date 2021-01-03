/// Compare operation model class definition.
///
/// PlnCmpOperation compare values .
/// e.g.) a == b / a > b
///
/// @file	PlnCmpOperation.cpp
/// @copyright	2018-2020 YAMAGUCHI Toshinobu 

#include "../../PlnConstants.h"
#include "PlnCmpOperation.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnGenerator.h"
#include "../PlnType.h"

#include "boost/assert.hpp"

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

template <typename T1, typename T2>
int64_t static_cmp(T1 lval, T2 rval, int cmp_type)
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
	return  result ? 1 : 0;
}

PlnExpression* PlnCmpOperation::create(PlnExpression* l, PlnExpression* r, PlnCmpType cmp_type)
{
	CREATE_CHECK_FLAG(l);
	CREATE_CHECK_FLAG(r);

	if (!is_l_num_lit || !is_r_num_lit) {
		if (is_l_num_lit) {
			// swap r and l
			PlnExpression *tmp = l;
			l = r;
			r = tmp;
			switch (cmp_type) {
				case CMP_L: cmp_type = CMP_G; break;
				case CMP_G: cmp_type = CMP_L; break;
				case CMP_LE: cmp_type = CMP_GE; break;
				case CMP_GE: cmp_type = CMP_LE; break;
				default:
					break;
			}
		}
		return new PlnCmpOperation(l, r, cmp_type);
	}

	int64_t cmp_result;
	if (is_l_uint && is_r_uint)
		cmp_result = static_cmp(lval.u, rval.u, cmp_type);
	else if ((is_l_int || is_l_uint) && (is_r_int || is_r_uint))
		cmp_result = static_cmp(lval.i, rval.i, cmp_type);
	else if (is_l_flo && is_r_flo)
		cmp_result = static_cmp(lval.d, rval.d, cmp_type);
	else if (is_l_flo && is_r_int)
		cmp_result = static_cmp(lval.d, rval.i, cmp_type);
	else if (is_l_int && is_r_flo)
		cmp_result = static_cmp(lval.i, rval.d, cmp_type);
	else if (is_l_flo && is_r_uint)
		cmp_result = static_cmp(lval.d, rval.u, cmp_type);
	else if (is_l_uint && is_r_flo)
		cmp_result = static_cmp(lval.u, rval.d, cmp_type);
	else
		BOOST_ASSERT(false);
	
	delete l;
	delete r;
	
	return new PlnExpression(cmp_result);
}

PlnCmpOperation::PlnCmpOperation(PlnExpression* l, PlnExpression* r, PlnCmpType cmp_type)
	: PlnBoolExpression(ET_CMP), l(l), r(r), cmp_type(cmp_type), result_dp(NULL)
{
	BOOST_ASSERT(!(l->type == ET_VALUE && l->values[0].type == VL_LIT_INT8));
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

	PlnDataPlace *ldp, *rdp;
	int acm_data_type = l->getDataType();
	if (r->getDataType() == DT_FLOAT)
		acm_data_type = DT_FLOAT;

	ldp = da.prepareAccumulator(acm_data_type, 8);

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
		BOOST_ASSERT(data_places.size() == 1);
		if (push_mode == -1) {
			result_dp = da.prepareAccumulator(DT_SINT, 8);
			da.allocDp(result_dp);
			da.pushSrc(data_places[0], result_dp, true);

		} else if (push_mode == 0) {
			da.pushSrc(data_places[0], da.getLiteralIntDp(0));
		} else if (push_mode == 1) {
			da.pushSrc(data_places[0], da.getLiteralIntDp(1));
		}

		if (push_mode != -1) {
			BOOST_ASSERT(jmp_if != -1);
			da.popSrc(data_places[0]);
		}
	}
}

void PlnCmpOperation::gen(PlnGenerator& g)
{
	l->gen(g);
	r->gen(g);

	auto ldp = l->data_places[0];
	auto rdp = r->data_places[0];

	g.genLoadDp(rdp);
	g.genLoadDp(ldp);

	auto re = g.getEntity(rdp);
	auto le = g.getEntity(ldp);

	int actual_cmp_type = cmp_type; 
	if (is_not) {
		// swap cmp type
		switch (cmp_type) {
			case CMP_EQ: actual_cmp_type = CMP_NE; break;
			case CMP_NE: actual_cmp_type = CMP_EQ; break;
			case CMP_L: actual_cmp_type = CMP_GE; break;
			case CMP_G: actual_cmp_type = CMP_LE; break;
			case CMP_LE: actual_cmp_type = CMP_G; break;
			case CMP_GE: actual_cmp_type = CMP_L; break;
			default:
				BOOST_ASSERT(false);
		}
	}

	// for comment
	string cmp_str;
	switch (actual_cmp_type) {
		case CMP_EQ: cmp_str = " == "; break;
		case CMP_NE: cmp_str = " != "; break;
		case CMP_L: cmp_str = " < "; break;
		case CMP_G: cmp_str = " > "; break;
		case CMP_LE: cmp_str = " <= "; break;
		case CMP_GE: cmp_str = " >= "; break;
		default:
				 BOOST_ASSERT(false);
	}

	int gen_cmp_type = g.genCmp(le.get(), re.get(), actual_cmp_type, ldp->cmt() + cmp_str + rdp->cmt());

	if (data_places.size()) {
		if (push_mode == -1) {
			auto e = g.getEntity(result_dp);
			g.genMoveCmpFlag(e.get(), gen_cmp_type, "cmpflg -> " + result_dp->cmt());
		}

		g.genSaveSrc(data_places[0]);
		if (push_mode != -1) {
			g.genLoadDp(data_places[0]);
		}
	}

	if (jmp_if == 1) {
		g.genTrueJump(jmp_id, gen_cmp_type, "jump if cmp is true");
	} else if (jmp_if == 0) {
		g.genFalseJump(jmp_id, gen_cmp_type, "jump if cmp is false");
	} 
}

