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

// PlnDivOperation
PlnExpression* PlnDivOperation::create(PlnExpression* l, PlnExpression* r)
{
	if (l->type == ET_VALUE && l->values[0].type == VL_LIT_INT8) {
		if (r->type == ET_VALUE && r->values[0].type == VL_LIT_INT8) {
			// e.g.) 5/2 => 2
			l->values[0].inf.intValue /= r->values[0].inf.intValue;
			delete r;
			return l;
		}
	} else if (l->type == ET_DIV && static_cast<PlnDivOperation*>(l)->div_type == DT_DIV) {
		PlnDivOperation* po = static_cast<PlnDivOperation*>(l);
		if (po->r->type == ET_VALUE
				&& po->r->values[0].type == VL_LIT_INT8) {
			if (r->type == ET_VALUE) {
				if (r->values[0].type == VL_LIT_INT8) {
					// e.g.) a/2/3 => a/6
					po->r->values[0].inf.intValue *= r->values[0].inf.intValue;
					return po;
				}
			} 
		}
	}

	return new PlnDivOperation(l,r, DT_DIV);
}

PlnExpression* PlnDivOperation::create_mod(PlnExpression* l, PlnExpression* r)
{
	if (l->type == ET_VALUE && l->values[0].type == VL_LIT_INT8) {
		if (r->type == ET_VALUE && r->values[0].type == VL_LIT_INT8) {
			// e.g.) 5%2 => 1
			l->values[0].inf.intValue %= r->values[0].inf.intValue;
			delete r;
			return l;
		}
	} 

	return new PlnDivOperation(l,r, DT_MOD);
}

PlnDivOperation::PlnDivOperation(PlnExpression* l, PlnExpression* r, PlnDivType dt)
	: PlnExpression(ET_DIV), l(l), r(r), div_type(dt)
{
	PlnValue v;
	v.type = VL_WK_INT8;
	if (div_type == DT_DIV) {
		values.push_back(v);
		values.push_back(v);	// for remainder
	} else	// DT_MOD
		values.push_back(v);
}

void PlnDivOperation::finish(PlnDataAllocator& da)
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
		da.allocData(8, rdp);	
		da.releaseData(rdp);
	}
	da.releaseAccumulator(ldp);
	da.divided(&quotient, &remainder);
}

void PlnDivOperation::dump(ostream& os, string indent)
{
	os << indent << (div_type==DT_DIV ? "DIV" : "MOD") << endl;
	l->dump(os, indent+" ");
	r->dump(os, indent+" ");
}

void PlnDivOperation::gen(PlnGenerator& g)
{
	l->gen(g);
	r->gen(g);

	auto le = g.getPopEntity(l->data_places[0]);
	auto re = g.getPopEntity(r->data_places[0]);

	string cmt=l->data_places[0]->cmt() + " / " + r->data_places[0]->cmt();
	g.genDiv(le.get(), re.get(), cmt);
	
	if (data_places.size() > 0) {
		if (div_type == DT_DIV) {
			auto rpe = g.getPushEntity(data_places[0]);
			auto qe = g.getPopEntity(quotient);
			g.genMove(rpe.get(), qe.get(), "");
			if (data_places.size() > 1) {
				auto rpe2 = g.getPushEntity(data_places[1]);
				auto rme = g.getPopEntity(remainder);
				g.genMove(rpe2.get(), rme.get(), "");
			}
		} else { // div_type == DT_MOD
			auto rpe = g.getPushEntity(data_places[0]);
			auto rme = g.getPopEntity(remainder);
			g.genMove(rpe.get(), rme.get(), "");
		}
	}
}

