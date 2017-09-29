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
#include "../PlnType.h"

// PlnAddOperation
PlnExpression* PlnAddOperation::create(PlnExpression* l, PlnExpression* r)
{
	if (l->type == ET_VALUE && l->values[0].type == VL_LIT_INT8) {
		if (r->type == ET_VALUE && r->values[0].type == VL_LIT_INT8) {
			// e.g.) 1+2 => 3
			l->values[0].inf.intValue += r->values[0].inf.intValue;
			delete r;
			return l;
		} else {
			// e.g.) 1+a => a+1
			PlnExpression *t;
			t = l;
			l = r;
			r = t;
		}
	} else if (l->type == ET_ADD) {
		PlnAddOperation* po = static_cast<PlnAddOperation*>(l);
		if (po->r->type == ET_VALUE
				&& po->r->values[0].type == VL_LIT_INT8) {
			if (r->type == ET_VALUE) {
				if (r->values[0].type == VL_LIT_INT8) {
					// e.g.) a+1+2 => a+3
					po->r->values[0].inf.intValue += r->values[0].inf.intValue;
					return po;
				}
			} 
		}
	}

	return new PlnAddOperation(l,r);
}

PlnExpression* PlnAddOperation::create_sub(PlnExpression* l, PlnExpression* r)
{
	if (l->type == ET_VALUE && l->values[0].type == VL_LIT_INT8) {
		if (r->type == ET_VALUE && r->values[0].type == VL_LIT_INT8) {
			// e.g.) 1-2 => -1
			l->values[0].inf.intValue -= r->values[0].inf.intValue;
			delete r;
			return l;
		}
	} else if (l->type == ET_ADD) {
		PlnAddOperation* po = static_cast<PlnAddOperation*>(l);
		if (po->r->type == ET_VALUE
				&& po->r->values[0].type == VL_LIT_INT8) {
			if (r->type == ET_VALUE) {
				if (r->values[0].type == VL_LIT_INT8) {
					// e.g.) a+1-2 => a+(-1)
					po->r->values[0].inf.intValue -= r->values[0].inf.intValue;
					return po;
				}
			} 
		}
	}
	if (r->type == ET_VALUE) {
		if (r->values[0].type == VL_LIT_INT8) {
			// e.g.) a-1 => a+(-1)
			r->values[0].inf.intValue *= -1;
			return new PlnAddOperation(l,r);
		}
	}
	return new PlnAddOperation(l,r,false);
}

PlnAddOperation::PlnAddOperation(PlnExpression* l, PlnExpression* r, bool is_add)
	: PlnExpression(ET_ADD), l(l), r(r), is_add(is_add)
{
	PlnValue v;
	v.type = VL_WORK;
	v.inf.wk_type = l->values[0].getType();
	values.push_back(v);
}

void PlnAddOperation::finish(PlnDataAllocator& da)
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
	PlnDataPlace* dp = new PlnDataPlace();
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
