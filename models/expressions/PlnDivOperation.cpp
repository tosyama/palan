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
	v.inf.wk_type = isUnsigned ? PlnType::getUint() : PlnType::getSint();
	if (div_type == DV_DIV) {
		values.push_back(v);
		values.push_back(v);	// for remainder
	} else	// DT_MOD
		values.push_back(v);
}

void PlnDivOperation::finish(PlnDataAllocator& da)
{
	// l => RAX
	PlnDataPlace* ldp = new PlnDataPlace(8, l->getDataType());
	l->data_places.push_back(ldp);
	l->finish(da);
	da.allocAccumulator(ldp);

	if (r->type == ET_VALUE) {
		r->data_places.push_back(r->values[0].getDataPlace(da));
		r->finish(da);
		r->data_places.back()->popSrc();
	} else {
		PlnDataPlace* rdp = new PlnDataPlace(8, r->getDataType());
		static string cmt="(temp)";
		rdp->comment = &cmt;
		r->data_places.push_back(rdp);
		r->finish(da);
		da.allocData(rdp);
		rdp->popSrc();
		da.releaseData(rdp);
	}
	ldp->popSrc();
	da.releaseAccumulator(ldp);
	da.divided(&quotient, &remainder);

	if (data_places.size()) {
		data_places[0]->pushSrc(quotient);
		if (data_places.size() >= 2)
			data_places[1]->pushSrc(remainder);
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

	auto le = g.getPopEntity(l->data_places[0]);
	auto re = g.getPopEntity(r->data_places[0]);

	string cmt=l->data_places[0]->cmt() + " / " + r->data_places[0]->cmt();
	g.genDiv(le.get(), re.get(), cmt);
	
	if (data_places.size() > 0) {
		if (div_type == DV_DIV) {
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

