/// Bool operation model class definition.
///
/// e.g.) a || b / a && b
///
/// @file	PlnBoolOperation.cpp
/// @copyright	2018 YAMAGUCHI Toshinobu 

#include "PlnBoolOperation.h"
#include "PlnCmpOperation.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnGenerator.h"
#include "../../PlnConstants.h"
#include "../PlnType.h"

#include "boost/assert.hpp"

PlnAndOperation::PlnAndOperation(PlnExpression* l, PlnExpression* r)
	: PlnExpression(ET_AND)
{
	PlnValue v;
	v.type = VL_WORK;
	v.lval_type = NO_LVL;
	v.inf.wk_type = PlnType::getSint();
	values.push_back(v);

	if (l->type != ET_CMP) {
		this->l = new PlnCmpOperation(new PlnExpression(int64_t(0)), l, CMP_NE);
	} else {
		this->l = static_cast<PlnCmpOperation*>(l);
	}

	if (r->type != ET_CMP) {
		this->r = new PlnCmpOperation(new PlnExpression(int64_t(0)), r, CMP_NE);
	} else {
		this->r = static_cast<PlnCmpOperation*>(r);
	}
}

void PlnAndOperation::finish(PlnDataAllocator& da)
{
	l->finish(da);
	int lcmp_type = l->getCmpType();
	if (lcmp_type == CMP_CONST_FALSE) {
		if (data_places.size()) {
			result_dp = da.getLiteralIntDp(0);
			da.pushSrc(data_places[0], result_dp, true);
		}
		return;
	}

	if (data_places.size()) {
		result_dp = da.prepareAccumulator(DT_SINT);
		da.allocDp(result_dp);
		da.pushSrc(data_places[0], result_dp, true);
	}
}

void PlnAndOperation::dump(ostream& os, string indent)
{
	os << indent << "AND" << endl;
	l->dump(os, indent+" ");
	r->dump(os, indent+" ");
}

void PlnAndOperation::gen(PlnGenerator& g)
{
	l->gen(g);

	if (data_places.size()) {
		auto e = g.getEntity(result_dp);
	//	g.genMoveCmpFlag(e.get(), gen_cmp_type, "cmpflg -> " + result_dp->cmt());
		g.genSaveSrc(data_places[0]);
	}
}

