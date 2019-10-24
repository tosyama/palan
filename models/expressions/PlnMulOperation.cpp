/// Mul operation model class definition.
///
/// PlnMulOperation calculate multiplication.
/// e.g.) a * b
///
/// @file	PlnMulOperation.cpp
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>

#include "PlnMulOperation.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnGenerator.h"
#include "../../PlnConstants.h"
#include "../PlnType.h"

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

// PlnMulOperation
PlnExpression* PlnMulOperation::create(PlnExpression* l, PlnExpression* r)
{
	CREATE_CHECK_FLAG(l);
	CREATE_CHECK_FLAG(r);

	// e.g.) 2*3 => 6
	if (is_l_uint && is_r_uint) {
		l->values[0].inf.uintValue = rval.u * lval.u;
		delete r;
		return l;
	}

	// e.g.) 2.1*3 => 6.3
	if (is_l_flo && is_r_num_lit) {
		double d =	is_r_int ? rval.i:
					is_r_uint ? rval.u:
					rval.d;
		l->values[0].inf.floValue = lval.d * d;
		delete r;
		return l;
	}

	if (is_l_num_lit && is_r_flo) { // is_l_flo == false
		double d = is_l_int ? lval.i : lval.u;
		r->values[0].inf.floValue = d * rval.d;
	 	delete l;
		return r;
	}

	if (is_l_num_lit && is_r_num_lit) {	// promote to integer.
		delete l; delete r;
		return new PlnExpression(lval.i * rval.i);
	}

	if (is_l_num_lit) {
		// e.g.) 2*a => a*2
		// l <-> r
		return new PlnMulOperation(r, l);
	}

	if (l->type == ET_MUL && (is_r_int || is_r_uint)) {
		PlnMulOperation* mul = static_cast<PlnMulOperation*>(l);
		PlnExpression* mulr = mul->r;
		CREATE_CHECK_FLAG(mulr);

		if (is_mulr_int || is_mulr_uint) {
			// e.g.) a*1*2 => a*6
			if (is_mulr_uint && is_r_uint) {
				mul->r->values[0].inf.uintValue = mulrval.u * rval.u;
			} else {
				mul->r->values[0].type = VL_LIT_INT8;
				mul->r->values[0].inf.intValue = mulrval.i * rval.i;
			}
			delete r;
			return mul;
		}
	}

	return new PlnMulOperation(l,r);
}

PlnMulOperation::PlnMulOperation(PlnExpression* l, PlnExpression* r)
	: PlnExpression(ET_MUL), l(l), r(r)
{
	bool isUnsigned = (l->getDataType() == DT_UINT && r->getDataType() == DT_UINT);
	bool isFloat = (l->getDataType() == DT_FLOAT || r->getDataType() == DT_FLOAT);
	PlnValue v;
	v.type = VL_WORK;
	if (isFloat) {
		v.inf.wk_type = PlnType::getFlo()->getVarType("---");
	} else if (isUnsigned) {
		v.inf.wk_type = PlnType::getUint()->getVarType("---");
	} else {
		v.inf.wk_type = PlnType::getSint()->getVarType("---");
	}
	values.push_back(v);
}

PlnMulOperation::~PlnMulOperation()
{
	delete l;
	delete r;
}

void PlnMulOperation::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	PlnDataPlace *ldp, *rdp;
	// l => RAX
	ldp = da.prepareAccumulator(values[0].getType()->data_type());

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

	auto product = da.multiplied(ldp, rdp);

	if (data_places.size())
		da.pushSrc(data_places[0], product);
	else
		da.releaseDp(product);
}

void PlnMulOperation::gen(PlnGenerator& g)
{
	l->gen(g);
	r->gen(g);

	auto ldp = l->data_places[0];
	auto rdp = r->data_places[0];
	
	g.genLoadDp(rdp);
	g.genLoadDp(ldp);

	auto le = g.getEntity(ldp);
	auto re = g.getEntity(rdp);
	g.genMul(le.get(), re.get(), ldp->cmt() + " * " + rdp->cmt());

	if (data_places.size() > 0)
		g.genSaveSrc(data_places[0]);	// src_place : product
}

