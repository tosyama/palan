/// Mul operation model class definition.
///
/// PlnMulOperation calculate multiplication.
/// e.g.) a * b
///
/// @file	PlnMulOperation.cpp
/// @copyright	2017-2021 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>

#include "../../PlnConstants.h"
#include "PlnMulOperation.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnGenerator.h"
#include "../PlnType.h"
#include "PlnCalcOperationUtils.h"

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
	: PlnExpression(ET_MUL), l(l), r(r), ldp(NULL), rdp(NULL), do_cross(false)
{
	PlnValue v;
	v.type = VL_WORK;
	v.inf.wk_type = binaryOperationType(l, r);

	values.push_back(v);
}

PlnMulOperation::~PlnMulOperation()
{
	delete l;
	delete r;
}

void PlnMulOperation::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	// Optimize assumption
	//	- lval:reg and rval:local var can be good performace.
	// e.g.) value: local var or literal. x: other
	//   case 1: x * value => x * value
	//   case 2: value * x => x * value
	//   case 3: x1 * x2 => x1->temp, x2 * temp  

	// l => RAX
	ldp = da.prepareAccumulator(values[0].getVarType()->data_type(), values[0].getVarType()->size());

	if (r->type == ET_VALUE) {	// case 1
		rdp = r->values[0].getDataPlace(da);
		l->data_places.push_back(ldp);
		r->data_places.push_back(rdp);

	} else if (l->type == ET_VALUE) {	// case 2
		PlnExpression *temp = l;
		l = r;
		r = temp;
		rdp = r->values[0].getDataPlace(da);

		l->data_places.push_back(ldp);
		r->data_places.push_back(rdp);

	} else {	// case 3
		rdp = da.prepareLocalVar(r->values[0].getVarType()->size(), r->getDataType());
		static string cmt="(temp@mul)";
		rdp->comment = &cmt;

		l->data_places.push_back(rdp);
		r->data_places.push_back(ldp);
		do_cross = true;
	}

	l->finish(da, si);
	if (do_cross) {
		da.popSrc(rdp);
		r->finish(da, si);
	} else {
		r->finish(da, si);
		da.popSrc(rdp);
	}
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
	if (do_cross) {
		g.genLoadDp(rdp);
		r->gen(g);

	} else {
		r->gen(g);
		g.genLoadDp(rdp);
	}
	g.genLoadDp(ldp);

	auto le = g.getEntity(ldp);
	auto re = g.getEntity(rdp);
	g.genMul(le.get(), re.get(), ldp->cmt() + " * " + rdp->cmt());

	if (data_places.size() > 0)
		g.genSaveSrc(data_places[0]);	// src_place : product
}

