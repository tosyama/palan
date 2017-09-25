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
			l->values[0].inf.intValue *= r->values[0].inf.intValue;
			delete r;
			return l;
		}
	} else if (l->type == ET_MUL) {
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

	return new PlnDivOperation(l,r);
}

PlnDivOperation::PlnDivOperation(PlnExpression* l, PlnExpression* r)
	: PlnExpression(ET_MUL), l(l), r(r)
{
	PlnValue v;
	v.type = VL_WK_INT8;
	values.push_back(v);
	values.push_back(v);	// for remainder
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
	da.divided();
}

void PlnDivOperation::dump(ostream& os, string indent)
{
	os << indent << "DIV" << endl;
	l->dump(os, indent+" ");
	r->dump(os, indent+" ");
}

void PlnDivOperation::gen(PlnGenerator& g)
{
	l->gen(g);
	r->gen(g);

	auto le = g.getPopEntity(l->data_places[0]);
	auto re = g.getPopEntity(r->data_places[0]);
	auto rpe = g.getPushEntity(data_places[0]);

	string cmt=l->data_places[0]->cmt() + "/" + r->data_places[0]->cmt()
			+ "->" + data_places[0]->cmt();
	g.genDiv(le.get(), re.get());
	g.genMove(rpe.get(), le.get(), cmt);
}

