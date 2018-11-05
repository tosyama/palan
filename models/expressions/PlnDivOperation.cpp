/// Div operation model class definition.
///
/// PlnDivOperation calculate division and remainder.
/// e.g.) a / b
///
/// @file	PlnDivOperation.cpp
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>

#include "PlnDivOperation.h"
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
			// e.g.) 5/2 => 2
			dv->r->values[0].inf.uintValue = dvrval.u * rval.u;
		} else {
			dv->r->values[0].type = VL_LIT_INT8;
			dv->r->values[0].inf.intValue = dvrval.i * rval.i;
		}
		delete r;
		return dv;
	}

	return new PlnDivOperation(l, r, DV_DIV);
}

PlnExpression* PlnDivOperation::create_mod(PlnExpression* l, PlnExpression* r)
{
	int l_num_type, r_num_type;
	if (l->isLitNum(l_num_type) && r->isLitNum(r_num_type)) {
		PlnExpression* new_val;
		// e.g.) 5%2 => 1
		if (l_num_type == VL_LIT_UINT8 && r_num_type == VL_LIT_UINT8) {
			uint64_t ui = l->values[0].inf.uintValue % r->values[0].inf.uintValue;
			new_val = new PlnExpression(ui);
		} else {
			int64_t i = l->values[0].inf.intValue % r->values[0].inf.intValue;
			new_val = new PlnExpression(i);
		}
		delete l; delete r;
		return new_val;
	}

	return new PlnDivOperation(l,r, DV_MOD);
}

PlnDivOperation::PlnDivOperation(PlnExpression* l, PlnExpression* r, PlnDivType dt)
	: PlnExpression(ET_DIV), l(l), r(r), div_type(dt)
{
	bool isUnsigned = (l->getDataType() == DT_UINT && r->getDataType() == DT_UINT);
	bool isFloat = (l->getDataType() == DT_FLOAT || r->getDataType() == DT_FLOAT);
	
	PlnValue v;
	v.type = VL_WORK;
	v.inf.wk_type = new vector<PlnType*>();
	if (isFloat) {
		v.inf.wk_type->push_back(PlnType::getFlo());
	} else if (isUnsigned) {
		v.inf.wk_type->push_back(PlnType::getUint());
	} else {
		v.inf.wk_type->push_back(PlnType::getSint());
	}
	values.push_back(v);
}

void PlnDivOperation::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	PlnDataPlace *ldp, *rdp;
	// l => RAX
	ldp = da.prepareAccumulator(values[0].getType()->data_type);

	if (r->type == ET_VALUE) {
		rdp = r->values[0].getDataPlace(da);
	} else {
		static string cmt="(temp)";
		rdp = da.prepareLocalVar(8, r->getDataType());
		rdp->comment = &cmt;
	}

	l->data_places.push_back(ldp);
	l->finish(da, si);
	
	r->data_places.push_back(rdp);
	r->finish(da, si);
	
	da.popSrc(rdp);
	da.popSrc(ldp);
	da.divided(&quotient, &remainder, ldp, rdp);

	if (data_places.size()) {
		if (div_type == DV_DIV)  {
			da.pushSrc(data_places[0], quotient);
			da.releaseDp(remainder);
		} else {	// DV_MOD
			da.releaseDp(quotient);
			da.pushSrc(data_places[0], remainder);
		}
	} else {
		da.releaseDp(quotient);
		da.releaseDp(remainder);
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
	g.genDiv(le.get(), re.get(), cmt);
	
	if (data_places.size() > 0) {
		if (div_type == DV_DIV) {
			g.genSaveSrc(data_places[0]);
			if (data_places.size() > 1)
				g.genSaveSrc(data_places[1]);
			
		} else { // div_type == DT_MOD
			g.genSaveSrc(data_places[0]);
		}
	}
}

