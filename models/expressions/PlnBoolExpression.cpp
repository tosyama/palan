/// PlnBoolExpression model class definisson.
///
/// @file	PlnBoolExpression
/// @copyright	2020 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include "PlnCmpOperation.h"
#include "../PlnType.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnGenerator.h"

PlnBoolExpression* PlnBoolExpression::create(PlnExpression* e)
{
	PlnBoolExpression* be = dynamic_cast<PlnBoolExpression*>(e);
	if (be) return be;

	if (e->type == ET_VALUE) {
		BOOST_ASSERT(e->values.size() == 1);

		PlnValType vtype = e->values[0].type;
		if (vtype == VL_LIT_INT8 || vtype == VL_LIT_UINT8) {
			if (e->values[0].inf.intValue == 0) {
				delete e;
				return new PlnFalseExpression();
			}
			delete e;
			return new PlnTrueExpression();
		} else if (vtype  == VL_LIT_FLO8) {
			if (e->values[0].inf.floValue == 0.0) {
				delete e;
				return new PlnFalseExpression();
			}
			delete e;
			return new PlnTrueExpression();
		}
	}

	return new PlnCmpOperation(e, new PlnExpression(int64_t(0)), CMP_NE);
}

// PlnTrueExpression
PlnTrueExpression::PlnTrueExpression() : PlnBoolExpression(ET_TRUE)
{
	PlnValue v;
	v.type = VL_LIT_INT8;
	v.asgn_type = NO_ASGN;
	v.inf.intValue = 1;
	values.push_back(v);
}

void PlnTrueExpression::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	if (data_places.size()) {
		BOOST_ASSERT(data_places.size() == 1);
		if ((push_mode == -1 && !is_not) || push_mode == 1) {
			da.pushSrc(data_places[0], da.getLiteralIntDp(1));
		} else {
			da.pushSrc(data_places[0], da.getLiteralIntDp(0));
		}
		if (push_mode != -1) {
			BOOST_ASSERT(jmp_if != -1);
			da.popSrc(data_places[0]);
		}
	}
}

void PlnTrueExpression::gen(PlnGenerator& g)
{
	if (data_places.size()) {
		g.genSaveSrc(data_places[0]);
		if (push_mode != -1) {
			g.genLoadDp(data_places[0]);
		}
	}

	if (jmp_if == 1) {
		g.genJump(jmp_id, "true");
	}
}

// PlnFalseExpression
PlnFalseExpression::PlnFalseExpression() : PlnBoolExpression(ET_FALSE)
{
	PlnValue v;
	v.type = VL_LIT_INT8;
	v.asgn_type = NO_ASGN;
	v.inf.intValue = 0;
	values.push_back(v);
}

void PlnFalseExpression::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	if (data_places.size()) {
		BOOST_ASSERT(data_places.size() == 1);
		if ((push_mode == -1 && is_not) || push_mode == 1) {
			da.pushSrc(data_places[0], da.getLiteralIntDp(1));
		} else {
			da.pushSrc(data_places[0], da.getLiteralIntDp(0));
		}

		if (push_mode != -1) {
			BOOST_ASSERT(jmp_if != -1);
			da.popSrc(data_places[0]);
		}
	}
}

void PlnFalseExpression::gen(PlnGenerator& g)
{
	if (data_places.size()) {
		g.genSaveSrc(data_places[0]);
		if (push_mode != -1) {
			g.genLoadDp(data_places[0]);
		}
	}

	if (jmp_if == 0) {
		g.genJump(jmp_id, "false");
	}
}

