/// Add operation model class definition.
///
/// PlnAddOperation calculate addition and subtraction.
/// e.g.) a + b / a - b
///
/// @file	PlnAddOperation.cpp
/// @copyright	2017-2020 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>

#include "PlnAddOperation.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnGenerator.h"
#include "../../PlnConstants.h"
#include "../PlnType.h"
#include "../../PlnMessage.h"
#include "../../PlnException.h"

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

// PlnAddOperation
PlnExpression* PlnAddOperation::create(PlnExpression* l, PlnExpression* r)
{
	if (l->getDataType() == DT_OBJECT_REF || r->getDataType() == DT_OBJECT_REF) {
		auto exp = (l->getDataType() == DT_OBJECT_REF) ? l : r;
		string tname = exp->values[0].getVarType()->typeinf->name;
		PlnCompileError err(E_CantUseOperatorHere, tname);
		err.loc = exp->loc;
		throw err;
	}

	CREATE_CHECK_FLAG(l);
	CREATE_CHECK_FLAG(r);

	// e.g.) 1+2 => 3
	if (is_l_uint && is_r_uint) {
		l->values[0].inf.uintValue = lval.u + rval.u;
		delete r;
		return l;
	}

	if (is_l_flo && is_r_num_lit) {
		double d =	is_r_int ? rval.i:
					is_r_uint ? rval.u:
					rval.d;
		l->values[0].inf.floValue = lval.d + d;
		delete r;
		return l;
	}

	if (is_l_num_lit && is_r_flo) { // is_l_flo == false
		double d = is_l_int ? lval.i : lval.u;
		r->values[0].inf.floValue = d + rval.d;
	 	delete l;
		return r;
	}

	if (is_l_num_lit && is_r_num_lit) {	// promote to integer.
		delete l; delete r;
		return new PlnExpression(lval.i + rval.i);
	}

	if (is_l_num_lit) {
		// e.g.) 1+a => a+1
		// l <-> r
		return new PlnAddOperation(r, l);
	}

	if (l->type == ET_ADD && (is_r_int || is_r_uint)) {
		PlnAddOperation* ad = static_cast<PlnAddOperation*>(l);
		PlnExpression* adr = ad->r;
		CREATE_CHECK_FLAG(adr);

		if (is_adr_int || is_adr_uint) {
			// e.g.) a+1+2 => a+3
			if (is_adr_uint && is_r_uint) {
				uint64_t u = ad->is_add ? adrval.u + rval.u : adrval.u - rval.u;
				ad->r->values[0].inf.uintValue = u;
			} else {
				int64_t i = ad->is_add ? adrval.i + rval.i : adrval.i - rval.i;
				ad->r->values[0].type = VL_LIT_INT8;
				ad->r->values[0].inf.intValue = i;
			}
			delete r;
			return ad;
		}
	}

	return new PlnAddOperation(l, r);
}

PlnExpression* PlnAddOperation::create_sub(PlnExpression* l, PlnExpression* r)
{
	CREATE_CHECK_FLAG(l);
	CREATE_CHECK_FLAG(r);
	
	// e.g.) 1-2 => -1
	if (is_l_uint && is_r_uint) {
		l->values[0].inf.uintValue = lval.u - rval.u;
		delete r;
		return l;
	}

	if (is_l_flo && is_r_num_lit) {
		double d =	is_r_int ? rval.i:
					is_r_uint ? rval.u:
					rval.d;
		l->values[0].inf.floValue = lval.d - d;
		delete r;
		return l;
	}

	if (is_l_num_lit && is_r_flo) { // is_l_flo == false
		double d = is_l_int ? lval.i : lval.u;
		r->values[0].inf.floValue = d - rval.d;
	 	delete l;
		return r;
	}

	if (is_l_num_lit && is_r_num_lit) {	// promote to integer.
		delete l; delete r;
		return new PlnExpression(lval.i - rval.i);
	}

	if (l->type == ET_ADD && (is_r_int || is_r_uint)) {
		PlnAddOperation* ad = static_cast<PlnAddOperation*>(l);
		PlnExpression* adr = ad->r;
		CREATE_CHECK_FLAG(adr);

		if (is_adr_int || is_adr_uint) {
			// e.g.) a+(-1)-2 => a+(-3)
			if (is_adr_uint && is_r_uint) {
				uint64_t u = ad->is_add ? adrval.u - rval.u : adrval.u + rval.u;
				adr->values[0].inf.uintValue = u;
			} else {
				int64_t i = ad->is_add ? adrval.i - rval.i : adrval.i + rval.i;
				ad->r->values[0].type = VL_LIT_INT8;
				ad->r->values[0].inf.intValue = i;
			}
			delete r;
			return ad;
		}
	}

	return new PlnAddOperation(l, r, false);
}

PlnAddOperation::PlnAddOperation(PlnExpression* l, PlnExpression* r, bool is_add)
	: PlnExpression(ET_ADD), l(l), r(r), ldp(NULL), rdp(NULL), is_add(is_add), do_cross(false)
{
	bool isUnsigned = (l->getDataType() == DT_UINT && r->getDataType() == DT_UINT);
	bool isFloat = (l->getDataType() == DT_FLOAT || r->getDataType() == DT_FLOAT);

	PlnValue v;
	v.type = VL_WORK;
	if (isFloat) {
		v.inf.wk_type = PlnType::getFlo()->getVarType();
	} else if (isUnsigned) {
		v.inf.wk_type = PlnType::getUint()->getVarType();
	} else {
		v.inf.wk_type = PlnType::getSint()->getVarType();
	}
	values.push_back(v);
}

PlnAddOperation::~PlnAddOperation()
{
	delete l;
	delete r;
}

void PlnAddOperation::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	// Optimize assumption
	//	- lval:reg and rval:local var can be good performace.
	// e.g.) value: local var or literal. x: other
	//   case 1: x + value => x + value
	//   case 2: value + x => x + value
	//   case 3: x1 + x2 => x1->temp, x2 + temp  

	// l => RAX
	ldp = da.prepareAccumulator(values[0].getVarType()->data_type());

	if (r->type == ET_VALUE) {	// case 1
		rdp = r->values[0].getDataPlace(da);
		l->data_places.push_back(ldp);
		r->data_places.push_back(rdp);

	} else if (is_add && l->type == ET_VALUE)  {	// case 2
		PlnExpression *temp = l;
		l = r;
		r = temp;
		rdp = r->values[0].getDataPlace(da);

		l->data_places.push_back(ldp);
		r->data_places.push_back(rdp);
		
	} else {
		rdp = da.prepareLocalVar(8, r->getDataType());
		static string cmt="(temp@add)";
		rdp->comment = &cmt;

		if (is_add) {	// case 3
			l->data_places.push_back(rdp);
			r->data_places.push_back(ldp);
			do_cross = true;
		} else {
			l->data_places.push_back(ldp);
			r->data_places.push_back(rdp);
		}
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

	auto result_dp = da.added(ldp, rdp);
	if (data_places.size())
		da.pushSrc(data_places[0], result_dp, true);
	else
		da.releaseDp(result_dp);
}

void PlnAddOperation::gen(PlnGenerator& g)
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

	auto re = g.getEntity(rdp);
	auto le = g.getEntity(ldp);
	if (is_add) g.genAdd(le.get(), re.get(), ldp->cmt() + " + " + rdp->cmt());
	else g.genSub(le.get(), re.get(), ldp->cmt() + " - " + rdp->cmt());

	if (data_places.size() > 0)
		g.genSaveSrc(data_places[0]);
}

// PlnNegative
PlnExpression* PlnNegative::create(PlnExpression *e)
{
	CREATE_CHECK_FLAG(e);
	if (is_e_int || is_e_uint) {
		e->values[0].inf.intValue = - e->values[0].inf.intValue;
		return e;

	} else if (is_e_flo) {
		e->values[0].inf.floValue = - e->values[0].inf.floValue;
		return e;
	}

	return new PlnNegative(e); 
}

PlnNegative::PlnNegative(PlnExpression* e)
	: PlnExpression(ET_NEG), e(e)
{
	PlnValue v;
	v.type = VL_WORK;
	v.inf.wk_type = e->values[0].getVarType();
	values.push_back(v);
}

PlnNegative::~PlnNegative()
{
	delete e;
}

void PlnNegative::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	auto dp = da.prepareAccumulator(values[0].getVarType()->data_type());
	e->data_places.push_back(dp);
	e->finish(da, si);
	da.popSrc(dp);

	if (data_places.size())
		da.pushSrc(data_places[0], dp, true);
	else
		da.releaseDp(dp);
		
}

void PlnNegative::gen(PlnGenerator& g)
{
	e->gen(g);
	auto dp = e->data_places[0];
	g.genLoadDp(dp);
	
	auto ne = g.getEntity(dp);
	g.genNegative(ne.get(), "-" + dp->cmt());
	if (data_places.size())
		g.genSaveSrc(data_places[0]);
}
