/// Compare operation model class definition.
///
/// PlnCmpOperation compare values .
/// e.g.) a == b / a > b
///
/// @file	PlnCmpOperation.cpp
/// @copyright	2018 YAMAGUCHI Toshinobu 

#include "PlnCmpOperation.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnGenerator.h"
#include "boost/assert.hpp"

PlnCmpOperation::PlnCmpOperation(PlnExpression* l, PlnExpression* r, PlnCmpType cmp_type)
	:l(l), r(r), cmp_type(cmp_type), gen_cmp_type(-1), PlnExpression(ET_CMP)
{
}

void PlnCmpOperation::finish(PlnDataAllocator& da)
{
	BOOST_ASSERT(data_places.size() == 0);	// not implemented return case.

	bool alloc_acm = false;
	PlnDataPlace *ldp, *rdp;

	if (l->type == ET_VALUE) {
		ldp = l->values[0].getDataPlace(da);
	} else {
		ldp = da.prepareAccumulator(l->getDataType());
	}

	if (r->type == ET_VALUE) {
		rdp = r->values[0].getDataPlace(da);
	} else if (!alloc_acm) {
		rdp = da.prepareAccumulator(r->getDataType());
	} else {
		rdp = new PlnDataPlace(8, r->getDataType());
		rdp->type = DP_STK_BP;
		rdp->status = DS_READY_ASSIGN;
		static string cmt="(temp)";
		rdp->comment = &cmt;
	}

	l->data_places.push_back(ldp);
	l->finish(da);
	
	r->data_places.push_back(rdp);
	r->finish(da);

	da.popSrc(rdp);
	da.popSrc(ldp);
}

void PlnCmpOperation::dump(ostream& os, string indent)
{
	os << indent << "CMP" << endl;
	l->dump(os, indent+" ");
	r->dump(os, indent+" ");
}

void PlnCmpOperation::gen(PlnGenerator& g)
{
	if (isConst()) return;

	l->gen(g);
	r->gen(g);

	auto ldp = l->data_places[0];
	auto rdp = r->data_places[0];

	g.genLoadDp(rdp);
	g.genLoadDp(ldp);

	auto re = g.getEntity(rdp);
	auto le = g.getEntity(ldp);

	gen_cmp_type = g.genCmp(le.get(), re.get(), cmp_type, ldp->cmt() + " == " + rdp->cmt());
}

template <typename LT, typename RT>
int static_comp(LT lval, RT rval, int cmp_type)
{
	bool result;
	switch (cmp_type) {
		case CMP_EQ: result = (lval == rval); break;
		case CMP_NE: result = (lval != rval); break;
		default:
			BOOST_ASSERT(false);
	}
	return  result ? CMP_CONST_TRUE : CMP_CONST_FALSE;
}

bool PlnCmpOperation::isConst()
{
	if (l->type != ET_VALUE || r->type != ET_VALUE)
		return false;

	int lval_t = l->values[0].type;
	int rval_t = r->values[0].type;
	auto lval = l->values[0].inf;
	auto rval = r->values[0].inf;

	if (lval_t == VL_LIT_INT8 && rval_t == VL_LIT_INT8)
		gen_cmp_type = static_comp(lval.intValue, rval.intValue, cmp_type);

	else if (lval_t == VL_LIT_INT8 && rval_t == VL_LIT_UINT8)
		gen_cmp_type = static_comp(lval.intValue, rval.uintValue, cmp_type);

	else if (lval_t == VL_LIT_UINT8 && rval_t == VL_LIT_INT8)
		gen_cmp_type = static_comp(lval.uintValue, rval.intValue, cmp_type);

	else if (lval_t == VL_LIT_UINT8 && rval_t == VL_LIT_UINT8)
		gen_cmp_type = static_comp(lval.uintValue, rval.uintValue, cmp_type);

	else
		return false;

	return true;
}

int PlnCmpOperation::getCmpType()
{
	// use after gen.
	BOOST_ASSERT(gen_cmp_type != -1);
	return gen_cmp_type;
}
