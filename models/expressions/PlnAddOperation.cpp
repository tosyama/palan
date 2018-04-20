/// Add operation model class definition.
///
/// PlnAddOperation calculate addition and subtraction.
/// e.g.) a + b / a - b
///
/// @file	PlnAddOperation.cpp
/// @copyright	2017 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>

#include "PlnAddOperation.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnGenerator.h"
#include "../../PlnConstants.h"
#include "../PlnType.h"

// PlnAddOperation
PlnExpression* PlnAddOperation::create(PlnExpression* l, PlnExpression* r)
{
	int l_num_type, r_num_type;
	if (l->isLitNum(l_num_type)) {
		if (r->isLitNum(r_num_type)) {
			// e.g.) 1+2 => 3
			if (l_num_type == VL_LIT_UINT8 && r_num_type == VL_LIT_UINT8) {
				l->values[0].inf.uintValue += r->values[0].inf.uintValue;
				delete r;
				return l;
			} else {
				int64_t i = l->values[0].inf.intValue + r->values[0].inf.intValue;
				delete l; delete r;
				return new PlnExpression(i);
			}
		} else {
			// e.g.) 1+a => a+1
			PlnExpression *t;
			t = l; l = r; r = t;
		}
	} else if (l->type == ET_ADD) {
		PlnAddOperation* ad = static_cast<PlnAddOperation*>(l);
		if (ad->r->isLitNum(l_num_type) && r->isLitNum(r_num_type)) {
			// e.g.) a+1+2 => a+3
			if (l_num_type == VL_LIT_UINT8 && r_num_type == VL_LIT_UINT8) {
				ad->r->values[0].inf.uintValue += r->values[0].inf.uintValue;
				delete r;
				return ad;
			} else {
				int64_t val = ad->r->values[0].inf.intValue + r->values[0].inf.intValue;
				delete r;
				r = new PlnExpression(val);
				l = ad->l;
				ad->l = NULL; delete ad;
			}
		}
	}

	return new PlnAddOperation(l,r);
}

PlnExpression* PlnAddOperation::create_sub(PlnExpression* l, PlnExpression* r)
{
	int l_num_type, r_num_type;
	if (l->isLitNum(l_num_type)) {
		if (r->isLitNum(r_num_type)) {
			// e.g.) 1-2 => -1
			if (l_num_type == VL_LIT_UINT8 && r_num_type == VL_LIT_UINT8) {
				l->values[0].inf.uintValue -= r->values[0].inf.uintValue;
				delete r;
				return l;
			} else {
				int64_t i = l->values[0].inf.intValue - r->values[0].inf.intValue;
				delete l; delete r;
				return new PlnExpression(i);
			}
		} 
	} else if (l->type == ET_ADD) {
		PlnAddOperation* ad = static_cast<PlnAddOperation*>(l);
		if (ad->r->isLitNum(l_num_type) && r->isLitNum(r_num_type)) {
			// e.g.) a+(-1)-2 => a+(-3)
			if (l_num_type == VL_LIT_UINT8 && r_num_type == VL_LIT_UINT8) {
				ad->r->values[0].inf.uintValue -= r->values[0].inf.uintValue;
				delete r;
				return ad;
			} else {
				int64_t val = ad->r->values[0].inf.intValue - r->values[0].inf.intValue;
				delete r;
				r = new PlnExpression(val);
				l = ad->l;
				ad->l = NULL; delete ad;
				return new PlnAddOperation(l,r);
			}
		}
	}

	if (r->isLitNum(r_num_type)) {
		r->values[0].inf.intValue *= -1;
		return new PlnAddOperation(l,r);
	}

	return new PlnAddOperation(l,r,false);
}

PlnAddOperation::PlnAddOperation(PlnExpression* l, PlnExpression* r, bool is_add)
	: PlnExpression(ET_ADD), l(l), r(r), is_add(is_add)
{
	bool isUnsigned = (l->getDataType() == DT_UINT && r->getDataType() == DT_UINT);

	PlnValue v;
	v.type = VL_WORK;
	v.inf.wk_type = new vector<PlnType*>();
	v.inf.wk_type->push_back(isUnsigned ? PlnType::getUint() : PlnType::getSint());
	values.push_back(v);
}

void PlnAddOperation::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	PlnDataPlace *ldp, *rdp;
	// l => RAX
	ldp = da.prepareAccumulator(l->getDataType());

	if (r->type == ET_VALUE) {
		rdp = r->values[0].getDataPlace(da);
	} else {
		rdp = da.prepareLocalVar(8, r->getDataType());
		static string cmt="(temp@add)";
		rdp->comment = &cmt;
	}

	l->data_places.push_back(ldp);
	l->finish(da, si);
	
	r->data_places.push_back(rdp);
	r->finish(da, si);

	da.popSrc(rdp);
	da.popSrc(ldp);

	auto result_dp = da.added(ldp, rdp);
	if (data_places.size())
		da.pushSrc(data_places[0], result_dp, true);
	else
		da.releaseData(result_dp);
}

void PlnAddOperation::dump(ostream& os, string indent)
{
	if (is_add) os << indent << "ADD" << endl;
	else os << indent << "SUB" << endl;
	l->dump(os, indent+" ");
	r->dump(os, indent+" ");
}

void PlnAddOperation::gen(PlnGenerator& g)
{
	l->gen(g);
	r->gen(g);

	auto ldp = l->data_places[0];
	auto rdp = r->data_places[0];

	g.genLoadDp(rdp);
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
	if (e->type == ET_VALUE && e->values[0].type == VL_LIT_INT8) {
		e->values[0].inf.intValue = - e->values[0].inf.intValue;
		return e;
	}
	return new PlnNegative(e); 
}

PlnNegative::PlnNegative(PlnExpression* e)
	: PlnExpression(ET_NEG), e(e)
{
	PlnValue v;
	v.type = VL_WORK;
	v.inf.wk_type = new vector<PlnType*>();
	v.inf.wk_type->push_back(e->values[0].getType());
	values.push_back(v);
}

void PlnNegative::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	auto dp = da.prepareAccumulator(DT_SINT);
	e->data_places.push_back(dp);
	e->finish(da, si);
	da.popSrc(dp);

	if (data_places.size())
		da.pushSrc(data_places[0], dp, true);
	else
		da.releaseData(dp);
		
}

void PlnNegative::dump(ostream& os, string indent)
{
	os << indent << "Negative:" << endl;
	e->dump(os, indent+" ");
}

static string gen_n_cmt(PlnDataPlace* dp, PlnDataPlace* result)
{
	return string("-") + dp->cmt() + " -> " + result->cmt();
}

void PlnNegative::gen(PlnGenerator& g)
{
	e->gen(g);
	auto dp = e->data_places[0];
	g.genLoadDp(dp);
	
	auto ne = g.getEntity(dp);
	g.genNegative(ne.get(), "-" + dp->cmt());
	if (data_places.size() > 0)
		g.genSaveSrc(data_places[0]);
}
