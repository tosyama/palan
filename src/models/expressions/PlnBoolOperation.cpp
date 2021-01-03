/// Bool operation model class definition.
///
/// e.g.) a || b / a && b
///
/// @file	PlnBoolOperation.cpp
/// @copyright	2018-2020 YAMAGUCHI Toshinobu 

#include "boost/assert.hpp"

#include "../../PlnConstants.h"
#include "PlnBoolOperation.h"
#include "../PlnType.h"
#include "../PlnModule.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnGenerator.h"
#include "../../PlnScopeStack.h"

#define CREATE_CHECK_FLAG(ex)	bool is_##ex##_int = false, is_##ex##_uint = false, is_##ex##_flo = false;	\
	union {int64_t i; uint64_t u; double d;} ex##val; \
	if (ex->type == ET_VALUE) { \
		switch (ex->values[0].type) { \
			case VL_LIT_INT8: is_##ex##_int = true; \
				ex##val.i = ex->values[0].inf.intValue; break;\
			case VL_LIT_UINT8: is_##ex##_uint = true; \
				ex##val.u = ex->values[0].inf.uintValue; break; \
			case VL_LIT_FLO8: is_##ex##_flo = true; \
				ex##val.d = ex->values[0].inf.floValue; break; \
		} \
	} \
	bool is_##ex##_num_lit = is_##ex##_int || is_##ex##_uint || is_##ex##_flo;

PlnExpression* PlnBoolOperation::create(PlnExpression* l, PlnExpression* r, PlnExprsnType type)
{
	CREATE_CHECK_FLAG(l);
	CREATE_CHECK_FLAG(r);

	if (is_l_num_lit && is_r_num_lit) {
		bool bl = is_l_int ? (lval.i != 0):
			is_l_uint ? (lval.u != 0):
			(lval.d != 0.0);	// is_l_flo

		bool br = is_r_int ? (rval.i != 0):
			is_r_uint ? (rval.u != 0):
			(rval.d != 0.0);	// is_r_flo

		int64_t result;
		if (type == ET_AND) {
			result = (bl && br) ? 1 : 0;
		} else if (type == ET_OR) {
			result = (bl || br) ? 1 : 0;
		} else
			BOOST_ASSERT(false);

		delete l;
		delete r;
		return new PlnExpression(result);
	}

	if (is_l_num_lit) {	// && !is_r_num_lit
		bool bl = is_l_int ? (lval.i != 0):
			is_l_uint ? (lval.u != 0):
			(lval.d != 0.0);	// is_l_flo

		if ((!bl) && type == ET_AND) { // false && expression
			delete l;
			delete r;
			return new PlnExpression(int64_t(0));
		}
		
		if (bl && type == ET_OR) { // true || expression
			delete l;
			delete r;
			return new PlnExpression(int64_t(1));
		}

		delete l;
		return PlnBoolExpression::create(r);
	}

	if (type == ET_AND)
		return new PlnAndOperation(PlnBoolExpression::create(l), PlnBoolExpression::create(r));
	else 
		return new PlnOrOperation(PlnBoolExpression::create(l), PlnBoolExpression::create(r));
}

PlnExpression* PlnBoolOperation::createNot(PlnExpression* e)
{
	PlnBoolExpression* be = PlnBoolExpression::create(e);
	be->is_not = !be->is_not;
	return be;
}

PlnBoolOperation::PlnBoolOperation(PlnBoolExpression* l, PlnBoolExpression* r, PlnExprsnType type)
	: PlnBoolExpression(type), l(l), r(r), end_jmp_id(-1)
{
	PlnValue v;
	v.type = VL_WORK;
	v.asgn_type = NO_ASGN;
	v.inf.wk_type = PlnType::getSint()->getVarType();
	values.push_back(v);
}

PlnBoolOperation::~PlnBoolOperation()
{
	delete l;
	delete r;
}

void PlnAndOperation::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	PlnDataPlace *ladp = NULL;
	PlnDataPlace *radp = NULL;

	PlnModule* m = si.scope[0].inf.module;
	if (jmp_if == -1) {
		BOOST_ASSERT(data_places.size() == 1);
		BOOST_ASSERT(jmp_id == -1);
		BOOST_ASSERT(push_mode == -1);

		ladp = da.prepareAccumulator(DT_SINT, 8);
		radp = da.prepareAccumulator(DT_SINT, 8);
		l->data_places.push_back(ladp);
		r->data_places.push_back(radp);
		l->push_mode = is_not ? 1:0;
		r->push_mode = -1;

		end_jmp_id = m->getJumpID();
		l->jmp_if = 0;
		l->jmp_id = end_jmp_id;
		l->finish(da, si);
		// popSrc(ladp) will be executed in l->finish()

		if (is_not)
			r->is_not = !r->is_not;
		r->finish(da, si);
		da.popSrc(radp);

	} else if ((jmp_if == 0 && !is_not)
			|| (jmp_if == 1 && is_not)) {
		BOOST_ASSERT(jmp_id != -1);
		if (data_places.size()) {
			BOOST_ASSERT(push_mode != -1);
			ladp = da.prepareAccumulator(DT_SINT, 8);
			radp = da.prepareAccumulator(DT_SINT, 8);
			l->data_places.push_back(ladp);
			r->data_places.push_back(radp);
			l->push_mode = push_mode;
			r->push_mode = push_mode;
		}
		l->jmp_if = 0;
		l->jmp_id = jmp_id;
		l->finish(da, si);

		r->jmp_if = 0;
		r->jmp_id = jmp_id;
		r->finish(da, si);

	} else if ((jmp_if == 0 && is_not)
			|| (jmp_if == 1 && !is_not)){
		if (data_places.size()) {
			BOOST_ASSERT(push_mode != -1);
			radp = da.prepareAccumulator(DT_SINT, 8);
			r->data_places.push_back(radp);
			r->push_mode = push_mode;
		}
		end_jmp_id = m->getJumpID();
		l->jmp_if = 0;
		l->jmp_id = end_jmp_id;
		l->finish(da, si);

		r->jmp_if = 1;
		r->jmp_id = jmp_id;
		r->finish(da, si);

	} else
		BOOST_ASSERT(false);

	if (data_places.size()) {
		da.pushSrc(data_places[0], radp);
	}
}

void PlnAndOperation::gen(PlnGenerator& g)
{
	l->gen(g);
	r->gen(g);
	if (jmp_if == -1)
		g.genLoadDp(r->data_places[0]);

	if (end_jmp_id >= 0)
		g.genJumpLabel(end_jmp_id, "end &&");
}

void PlnOrOperation::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	PlnDataPlace *ladp = NULL;
	PlnDataPlace *radp = NULL;

	PlnModule* m = si.scope[0].inf.module;
	if (jmp_if == -1) {
		BOOST_ASSERT(data_places.size() == 1);
		BOOST_ASSERT(jmp_id == -1);

		ladp = da.prepareAccumulator(DT_SINT, 8);
		radp = da.prepareAccumulator(DT_SINT, 8);
		l->data_places.push_back(ladp);
		r->data_places.push_back(radp);
		l->push_mode = is_not ? 0 : 1;
		r->push_mode = -1;

		end_jmp_id = m->getJumpID();
		l->jmp_if = 1;
		l->jmp_id = end_jmp_id;
		l->finish(da, si);
		// popSrc(ladp) will be executed in l->finish()

		if (is_not)
			r->is_not = !r->is_not;
		r->finish(da, si);
		da.popSrc(radp);

	} else if ((jmp_if == 0 && !is_not)
			|| (jmp_if == 1 && is_not)) {
		BOOST_ASSERT(jmp_id != -1);
		if (data_places.size()) {
			BOOST_ASSERT(push_mode != -1);
			radp = da.prepareAccumulator(DT_SINT, 8);
			r->data_places.push_back(radp);
			r->push_mode = push_mode;
		}
		end_jmp_id = m->getJumpID();
		l->jmp_if = 1;
		l->jmp_id = end_jmp_id;
		l->finish(da, si);

		r->jmp_if = 0;
		r->jmp_id = jmp_id;
		r->finish(da, si);

	} else if ((jmp_if == 0 && is_not)
			|| (jmp_if == 1 && !is_not)) {
		if (data_places.size()) {
			BOOST_ASSERT(push_mode != -1);
			ladp = da.prepareAccumulator(DT_SINT, 8);
			radp = da.prepareAccumulator(DT_SINT, 8);
			l->data_places.push_back(ladp);
			r->data_places.push_back(radp);
			l->push_mode = push_mode;
			r->push_mode = push_mode;
		}
		l->jmp_if = 1;
		l->jmp_id = jmp_id;
		l->finish(da, si);

		r->jmp_if = 1;
		r->jmp_id = jmp_id;
		r->finish(da, si);

	} else {
		BOOST_ASSERT(false);
	}

	if (data_places.size()) {
		da.pushSrc(data_places[0], radp);
	}
}

void PlnOrOperation::gen(PlnGenerator& g)
{
	l->gen(g);
	r->gen(g);
	if (jmp_if == -1)
		g.genLoadDp(r->data_places[0]);

	if (end_jmp_id >= 0)
		g.genJumpLabel(end_jmp_id, "end ||");
}

