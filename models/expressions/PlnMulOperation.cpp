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

// PlnMulOperation
PlnExpression* PlnMulOperation::create(PlnExpression* l, PlnExpression* r)
{
	int l_num_type, r_num_type;
	if (l->isLitNum(l_num_type)) {
		if (r->isLitNum(r_num_type)) {
			// e.g.) 2*3 => 6
			if (l_num_type == VL_LIT_UINT8 && r_num_type == VL_LIT_UINT8) {
				l->values[0].inf.uintValue *= r->values[0].inf.uintValue;
				delete r;
				return l;
			} else {
				int64_t i = l->values[0].inf.intValue * r->values[0].inf.intValue;
				delete l; delete r;
				return new PlnExpression(i);
			}
		} else {
			// e.g.) 2*a => a*2
			PlnExpression *t;
			t = l; l = r; r = t;
		}

	} else if (l->type == ET_MUL) {
		PlnMulOperation* ml = static_cast<PlnMulOperation*>(l);
		if (ml->r->isLitNum(l_num_type) && r->isLitNum(r_num_type)) {
			// e.g.) a*1*2 => a*2
			if (l_num_type == VL_LIT_UINT8 && r_num_type == VL_LIT_UINT8) {
				ml->r->values[0].inf.uintValue *= r->values[0].inf.uintValue;
				delete r;
				return ml;
			} else {
				int64_t val = ml->r->values[0].inf.intValue * r->values[0].inf.intValue;
				delete r;
				r = new PlnExpression(val);
				l = ml->l;
				ml->l = NULL; delete ml;
			}
		}
	}

	return new PlnMulOperation(l,r);
}

PlnMulOperation::PlnMulOperation(PlnExpression* l, PlnExpression* r)
	: PlnExpression(ET_MUL), l(l), r(r)
{
	bool isUnsigned = (l->getDataType() == DT_UINT && r->getDataType() == DT_UINT);
	PlnValue v;
	v.type = VL_WORK;
	v.inf.wk_type = new vector<PlnType*>();
	v.inf.wk_type->push_back(isUnsigned ? PlnType::getUint() : PlnType::getSint());
	values.push_back(v);
}

void PlnMulOperation::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	PlnDataPlace *ldp, *rdp;
	// l => RAX
	ldp = da.prepareAccumulator(l->getDataType());

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

void PlnMulOperation::dump(ostream& os, string indent)
{
	os << indent << "MUL" << endl;
	l->dump(os, indent+" ");
	r->dump(os, indent+" ");
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

