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

// PlnDivOperation
PlnExpression* PlnDivOperation::create(PlnExpression* l, PlnExpression* r)
{
	int r_num_type;

	if (r->isLitNum(r_num_type)) {
		int l_num_type;
		if (l->isLitNum(l_num_type)) { 
			PlnExpression* new_val;

			// e.g.) 5/2 => 2
			if (l_num_type == VL_LIT_UINT8 && r_num_type == VL_LIT_UINT8) {
				uint64_t val = l->values[0].inf.uintValue / r->values[0].inf.uintValue;
				new_val = new PlnExpression(val);
			} else {
				int64_t val = l->values[0].inf.intValue / r->values[0].inf.intValue;
				new_val = new PlnExpression(val);
			}
			delete l; delete r;
			return new_val;

		} else if (l->type == ET_DIV) {
			PlnDivOperation* dv = static_cast<PlnDivOperation*>(l);
			int lr_num_type;
			if (dv->div_type == DV_DIV && dv->r->isLitNum(lr_num_type)) {

				// e.g.) a/2/3 => a/6
				if (lr_num_type == VL_LIT_UINT8 && r_num_type == VL_LIT_UINT8) {
					dv->r->values[0].inf.uintValue *= r->values[0].inf.uintValue;
					delete r;
					return dv;
				} else {
					int64_t val = dv->r->values[0].inf.intValue * r->values[0].inf.intValue;
					delete r;
					r = new PlnExpression(val);
					l = dv->l;
					dv->l = NULL; delete dv;
				}
			} 
		}
	} 

	return new PlnDivOperation(l,r, DV_DIV);
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
	
	PlnValue v;
	v.type = VL_WORK;
	v.inf.wk_type = new vector<PlnType*>();
	v.inf.wk_type->push_back(isUnsigned ? PlnType::getUint() : PlnType::getSint());
	if (div_type == DV_DIV) {
		values.push_back(v);
		values.push_back(v);	// for remainder
	} else	// DT_MOD
		values.push_back(v);
}

void PlnDivOperation::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	PlnDataPlace *ldp, *rdp;
	// l => RAX
	ldp = da.prepareAccumulator(l->getDataType());

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
			if (data_places.size() >= 2 && data_places[1] != NULL)
				da.pushSrc(data_places[1], remainder);
			else
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

void PlnDivOperation::dump(ostream& os, string indent)
{
	os << indent << (div_type==DV_DIV ? "DIV" : "MOD") << endl;
	l->dump(os, indent+" ");
	r->dump(os, indent+" ");
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

