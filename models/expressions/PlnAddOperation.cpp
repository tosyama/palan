/// Add operation model class definition.
///
/// PlnAddOperation calculate addtion and subtraction.
/// e.g.) a + b / a - b
///
/// @file	PlnAddOperation.cpp
/// @copyright	2017- YAMAGUCHI Toshinobu 

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
	v.inf.wk_type = isUnsigned ? PlnType::getUint() : PlnType::getSint();
	values.push_back(v);
}

void PlnAddOperation::finish(PlnDataAllocator& da)
{
	// l => RAX
	PlnDataPlace* ldp = new PlnDataPlace(8, l->getDataType());
	l->data_places.push_back(ldp);
	l->finish(da);
	da.allocAccumulator(ldp);

	if (r->type == ET_VALUE) {
		r->data_places.push_back(r->values[0].getDataPlace(da));
		r->finish(da);
	} else {
		PlnDataPlace* rdp = new PlnDataPlace(8, r->getDataType());
		static string cmt="(temp)";
		rdp->comment = &cmt;
		r->data_places.push_back(rdp);
		r->finish(da);
		da.allocData(rdp);	
		da.releaseData(rdp);
	}
	da.releaseAccumulator(ldp);
}

void PlnAddOperation::dump(ostream& os, string indent)
{
	if (is_add) os << indent << "ADD" << endl;
	else os << indent << "SUB" << endl;
	l->dump(os, indent+" ");
	r->dump(os, indent+" ");
}

static string gen_cmt(bool is_add, PlnDataPlace* l, PlnDataPlace* r, PlnDataPlace* result)
{
	const char* ope = "+";
	if (!is_add)
		ope = "-";
	
	return l->cmt() + ope + r->cmt() + " -> " + result->cmt();
}

void PlnAddOperation::gen(PlnGenerator& g)
{
	l->gen(g);
	r->gen(g);

	auto le = g.getPopEntity(l->data_places[0]);
	auto re = g.getPopEntity(r->data_places[0]);
	if (is_add) g.genAdd(le.get(), re.get());
	else g.genSub(le.get(), re.get());

	if (data_places.size() > 0) {
		auto rpe = g.getPushEntity(data_places[0]);
		g.genMove(rpe.get(), le.get(), gen_cmt(is_add, l->data_places[0],r->data_places[0],data_places[0]));
	}
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
	v.inf.wk_type = e->values[0].getType();
	values.push_back(v);
}

void PlnNegative::finish(PlnDataAllocator& da)
{
	PlnDataPlace* dp = new PlnDataPlace(8, DT_SINT);
	e->data_places.push_back(dp);
	e->finish(da);
	da.allocAccumulator(dp);
	da.releaseAccumulator(dp);
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

	auto ne = g.getPopEntity(e->data_places[0]);
	
	g.genNegative(ne.get());
	if (data_places.size() > 0) {
		auto rpe = g.getPushEntity(data_places[0]);
		g.genMove(rpe.get(), ne.get(), gen_n_cmt(e->data_places[0], data_places[0]));
	}
}
