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
	v.type = VL_WK_INT8;
	values.push_back(v);
}

void PlnAddOperation::finish(PlnDataAllocator& da)
{
	BOOST_ASSERT(ret_places.size()==1);
	int index = 0;
	int size = 8;
	if (ret_places[0].type == RP_WORK) {
		index = ret_places[0].inf.wk.index;
	}
	PlnReturnPlace rp;
	rp.type = RP_WORK;
	rp.inf.wk.index = index;
	l->ret_places.push_back(rp);

	// l => RAX
	PlnDataPlace* ldp = new PlnDataPlace();
	l->data_places.push_back(ldp);
	l->finish(da);
	da.allocAccumulator(ldp);

	if (r->type == ET_VALUE) {
		rp.type = RP_AS_IS; // del
		r->ret_places.push_back(rp); // del
		r->data_places.push_back(r->values[0].getDataPlace(da));
		r->finish(da);
	} else {
		rp.inf.wk.index = index+1; // del
		r->ret_places.push_back(rp); // del
		PlnDataPlace* rdp = new PlnDataPlace;
		r->data_places.push_back(rdp);
		r->finish(da);
		da.allocData(8, rdp);	
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

void PlnAddOperation::gen(PlnGenerator& g)
{
	l->gen(g);
	r->gen(g);

//	auto le = l->ret_places[0].genEntity(g);
//	auto re = r->ret_places[0].genEntity(g);
//	auto rpe = ret_places[0].genEntity(g);
	auto le = g.getPopEntity(l->data_places[0]);
	auto re = g.getPopEntity(r->data_places[0]);
	auto rpe = g.getPushEntity(data_places[0]);
	if (is_add) g.genAdd(le.get(), re.get());
	else g.genSub(le.get(), re.get());
	g.genMove(rpe.get(), le.get(), ret_places[0].commentStr());
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
	v.type = VL_WK_INT8;
	values.push_back(v);
}

void PlnNegative::finish(PlnDataAllocator& da)
{
	PlnDataPlace* dp = new PlnDataPlace();
	e->data_places.push_back(dp);
	//--
	int index = 0;
	if (ret_places[0].type == RP_WORK) {
		index = ret_places[0].inf.wk.index;
	}
	PlnReturnPlace rp;
	rp.type = RP_WORK;
	rp.inf.wk.index = index;
	e->ret_places.push_back(rp);
	// --
	e->finish(da);
	da.allocAccumulator(dp);
	da.releaseAccumulator(dp);
}

void PlnNegative::dump(ostream& os, string indent)
{
	os << indent << "Negative:" << endl;
	e->dump(os, indent+" ");
}

void PlnNegative::gen(PlnGenerator& g)
{
	e->gen(g);

//	auto ne = e->ret_places[0].genEntity(g);
//	auto rpe = ret_places[0].genEntity(g);

	auto ne = g.getPopEntity(e->data_places[0]);
	auto rpe = g.getPushEntity(data_places[0]);
	
	g.genNegative(ne.get());
	g.genMove(rpe.get(), ne.get(), ret_places[0].commentStr());
}
