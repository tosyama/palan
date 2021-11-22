/// Div operation model class definition.
///
/// PlnDivOperation calculate division and remainder.
/// e.g.) a / b
///
/// @file	PlnDivOperation.cpp
/// @copyright	2017-2021 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>

#include "../../PlnConstants.h"
#include "PlnDivOperation.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnGenerator.h"
#include "../../PlnMessage.h"
#include "../../PlnException.h"
#include "../PlnType.h"
#include "PlnCalcOperationUtils.h"

// PlnDivOperation
PlnExpression* PlnDivOperation::create(PlnExpression* l, PlnExpression* r)
{
	CREATE_CHECK_FLAG(l);
	CREATE_CHECK_FLAG(r);
	 
	// e.g.) 5/2 => 2
	if (is_l_uint && is_r_uint) {
		l->values[0].inf.uintValue = lval.u / rval.u;
		delete r;
		return l;
	}

	if (is_l_flo && is_r_num_lit) {
		double d =	is_r_int ? rval.i:
					is_r_uint ? rval.u:
					rval.d;
		l->values[0].inf.floValue = lval.d / d;
		delete r;
		return l;
	}

	if (is_l_num_lit && is_r_flo) { // is_l_flo == false
		double d = is_l_int ? lval.i : lval.u;
		r->values[0].inf.floValue = d / rval.d;
	 	delete l;
		return r;
	}

	if (is_l_num_lit && is_r_num_lit) {	// promote to integer.
		delete l; delete r;
		return new PlnExpression(lval.i / rval.i);
	}

	if (l->type == ET_DIV && (is_r_int || is_r_uint)) {
		PlnDivOperation* dv = static_cast<PlnDivOperation*>(l);
		PlnExpression* dvr = dv->r;
		CREATE_CHECK_FLAG(dvr);
		if (dv->div_type == DV_DIV && (is_dvr_int || is_dvr_uint)) {
			if (is_dvr_uint && is_r_uint) {
				// e.g.) 5/2 => 2
				dv->r->values[0].inf.uintValue = dvrval.u * rval.u;
			} else {
				dv->r->values[0].type = VL_LIT_INT8;
				dv->r->values[0].inf.intValue = dvrval.i * rval.i;
			}
		}
		delete r;
		return dv;
	}

	return new PlnDivOperation(l, r, DV_DIV);
}

PlnExpression* PlnDivOperation::create_mod(PlnExpression* l, PlnExpression* r)
{
	CREATE_CHECK_FLAG(l);
	CREATE_CHECK_FLAG(r);

	// e.g.) 5%2 => 1
	if (is_l_uint && is_r_uint) {
		l->values[0].inf.uintValue = lval.u % rval.u;
		delete r;
		return l;
	}

	if (is_l_num_lit && is_r_num_lit) {
		delete l; delete r;
		return new PlnExpression(lval.i % rval.i);
	}

	return new PlnDivOperation(l,r, DV_MOD);
}

PlnDivOperation::PlnDivOperation(PlnExpression* l, PlnExpression* r, PlnDivType dt)
	: PlnExpression(ET_DIV), l(l), r(r), div_type(dt), quotient(NULL), remainder(NULL)
{
	PlnValue v;
	v.type = VL_WORK;
	v.inf.wk_type = binaryOperationType(l, r);

	if (div_type == DV_MOD && v.inf.wk_type->data_type() == DT_FLOAT) {
	 	PlnCompileError err(E_CantUseOperatorHere, PlnMessage::floatNumber());
	 	throw err;
	}

	values.push_back(v);
}

PlnDivOperation::~PlnDivOperation()
{
	delete l;
	delete r;
}

void PlnDivOperation::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	PlnDataPlace *ldp, *rdp;
	// l => RAX
	ldp = da.prepareAccumulator(values[0].getVarType()->data_type(), values[0].getVarType()->size());

	if (r->type == ET_VALUE) {
		rdp = r->values[0].getDataPlace(da);
	} else {
		static string cmt="(temp)";
		rdp = da.prepareLocalVar(r->values[0].getVarType()->size(), r->getDataType());
		rdp->comment = &cmt;
	}

	l->data_places.push_back(ldp);
	l->finish(da, si);
	
	r->data_places.push_back(rdp);
	r->finish(da, si);
	
	da.popSrc(rdp);
	da.popSrc(ldp);
	PlnDataPlace* result_dp = da.divided(ldp, rdp, div_type == DV_MOD);

	if (data_places.size()) {
		BOOST_ASSERT(data_places.size() == 1);
		da.pushSrc(data_places[0], result_dp);
	} else {
		da.releaseDp(result_dp);
	}
}

void PlnDivOperation::gen(PlnGenerator& g)
{
	l->gen(g);
	r->gen(g);

	auto ldp = l->data_places[0];
	auto rdp = r->data_places[0];

	g.genLoadDp(rdp);
	g.genLoadDp(ldp);

	auto le = g.getEntity(l->data_places[0]);
	auto re = g.getEntity(r->data_places[0]);

	string cmt=ldp->cmt() + " / " + rdp->cmt();
	if (div_type == DV_DIV) {
		g.genDiv(le.get(), re.get(), cmt);
	} else {
		BOOST_ASSERT(div_type == DV_MOD);
		g.genMod(le.get(), re.get(), cmt);
	}
	
	if (data_places.size()) {
		g.genSaveSrc(data_places[0]);
	}
}

