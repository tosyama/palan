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
#include "../PlnType.h"

// PlnMulOperation
PlnExpression* PlnMulOperation::create(PlnExpression* l, PlnExpression* r)
{
	if (l->type == ET_VALUE && l->values[0].type == VL_LIT_INT8) {
		if (r->type == ET_VALUE && r->values[0].type == VL_LIT_INT8) {
			// e.g.) 2*3 => 6
			l->values[0].inf.intValue *= r->values[0].inf.intValue;
			delete r;
			return l;
		} else {
			// e.g.) 2*a => a*2
			PlnExpression *t;
			t = l;
			l = r;
			r = t;
		}
	} else if (l->type == ET_MUL) {
		PlnMulOperation* po = static_cast<PlnMulOperation*>(l);
		if (po->r->type == ET_VALUE
				&& po->r->values[0].type == VL_LIT_INT8) {
			if (r->type == ET_VALUE) {
				if (r->values[0].type == VL_LIT_INT8) {
					// e.g.) a*1*2 => a*2
					po->r->values[0].inf.intValue *= r->values[0].inf.intValue;
					return po;
				}
			} 
		}
	}

	return new PlnMulOperation(l,r);
}

PlnMulOperation::PlnMulOperation(PlnExpression* l, PlnExpression* r)
	: PlnExpression(ET_MUL), l(l), r(r)
{
	PlnValue v;
	v.type = VL_WORK;
	v.inf.wk_type = l->values[0].getType();
	values.push_back(v);
}

void PlnMulOperation::finish(PlnDataAllocator& da)
{
	// l => RAX
	PlnDataPlace* ldp = new PlnDataPlace();
	l->data_places.push_back(ldp);
	l->finish(da);
	da.allocAccumulator(ldp);

	if (r->type == ET_VALUE) {
		r->data_places.push_back(r->values[0].getDataPlace(da));
		r->finish(da);
	} else {
		PlnDataPlace* rdp = new PlnDataPlace();
		static string cmt="(temp)";
		rdp->comment = &cmt;
		r->data_places.push_back(rdp);
		r->finish(da);
		auto tp = r->values[0].getType();
		da.allocData(tp->size, tp->data_type, rdp);	
		da.releaseData(rdp);
	}
	da.releaseAccumulator(ldp);
	product = da.multiplied(ldp);
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
	auto le = g.getPopEntity(l->data_places[0]);
	auto re = g.getPopEntity(r->data_places[0]);
	g.genMul(le.get(), re.get());

	if (data_places.size() > 0) {
		string cmt=l->data_places[0]->cmt() + "*" + r->data_places[0]->cmt()
			+ "->" + data_places[0]->cmt();
		auto rpe = g.getPushEntity(data_places[0]);
		auto pe = g.getPopEntity(product);
		g.genMove(rpe.get(), pe.get(), cmt);
	}
}

