/// Bool operation model class definition.
///
/// e.g.) a || b / a && b
///
/// @file	PlnBoolOperation.cpp
/// @copyright	2018-2020 YAMAGUCHI Toshinobu 

#include "boost/assert.hpp"

#include "PlnBoolOperation.h"
#include "../PlnType.h"
#include "../PlnModule.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnGenerator.h"
#include "../../PlnConstants.h"
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

	if (data_places.size()) {
		BOOST_ASSERT(data_places.size() == 1);
		ladp = da.prepareAccumulator(DT_SINT);
		radp = da.prepareAccumulator(DT_SINT);
		l->push_mode = 0;
		l->data_places.push_back(ladp);
		r->push_mode = push_mode;
		r->data_places.push_back(radp);
	}

	PlnModule* m = si.scope[0].inf.module;
	if (jmp_if == -1) {
		BOOST_ASSERT(jmp_id == -1);
		end_jmp_id = m->getJumpID();
		l->jmp_if = 0;
		l->jmp_id = end_jmp_id;
		l->finish(da, si);
		// popSrc(ladp) will be executed in l->finish()

		r->finish(da, si);
		da.popSrc(radp);

	} else if (jmp_if == 0) {
		BOOST_ASSERT(jmp_id != -1);
		l->jmp_if = 0;
		l->jmp_id = jmp_id;
		l->finish(da, si);
		r->jmp_if = 0;
		r->jmp_id = jmp_id;
		r->finish(da, si);

	} else if (jmp_if == 1) {
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
	if (jmp_if == -1) {
		l->gen(g);
		r->gen(g);
		g.genLoadDp(r->data_places[0]);
		BOOST_ASSERT(end_jmp_id >= 0);
		g.genJumpLabel(end_jmp_id, "end &&");

	} if (jmp_if == 0) {
		l->gen(g);
		r->gen(g);

	} if (jmp_if == 1) {
		l->gen(g);
		r->gen(g);
		g.genJumpLabel(end_jmp_id, "end &&");
	}
}

void PlnOrOperation::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	PlnDataPlace *ladp = NULL;
	PlnDataPlace *radp = NULL;

	if (data_places.size()) {
		BOOST_ASSERT(data_places.size() == 1);
		ladp = da.prepareAccumulator(DT_SINT);
		radp = da.prepareAccumulator(DT_SINT);
		l->push_mode = 1;
		l->data_places.push_back(ladp);
		r->push_mode = push_mode;
		r->data_places.push_back(radp);
	}

	PlnModule* m = si.scope[0].inf.module;
	if (jmp_if == -1) {
		BOOST_ASSERT(jmp_id == -1);
		end_jmp_id = m->getJumpID();
		l->jmp_if = 1;
		l->jmp_id = end_jmp_id;
		l->finish(da, si);
		// popSrc(ladp) will be executed in l->finish()

		r->finish(da, si);
		da.popSrc(radp);

	} else if (jmp_if == 0) {
		BOOST_ASSERT(jmp_id != -1);
		end_jmp_id = m->getJumpID();
		l->jmp_if = 1;
		l->jmp_id = end_jmp_id;
		l->finish(da, si);
		r->jmp_if = 0;
		r->jmp_id = jmp_id;
		r->finish(da, si);

	} else if (jmp_if == 1) {
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
	if (jmp_if == -1) {
		l->gen(g);
		r->gen(g);
		g.genLoadDp(r->data_places[0]);
		BOOST_ASSERT(end_jmp_id >= 0);
		g.genJumpLabel(end_jmp_id, "end ||");

	} else if (jmp_if == 0) {
		l->gen(g);
		r->gen(g);
		BOOST_ASSERT(end_jmp_id >= 0);
		g.genJumpLabel(end_jmp_id, "end ||");

	} else if (jmp_if == 1) {
		l->gen(g);
		r->gen(g);

	} else
		BOOST_ASSERT(false);
}

// PlnBoolOperation2
PlnBoolOperation2::PlnBoolOperation2(PlnExpression* l, PlnExpression* r, PlnExprsnType type)
	: PlnCmpExpression(type), jmp_l_id(-1), jmp_r_id(-1),
		result_dp(NULL), zero_dp(NULL)
{
	PlnValue v;
	v.type = VL_WORK;
	v.asgn_type = NO_ASGN;
	v.inf.wk_type = PlnType::getSint()->getVarType();
	values.push_back(v);

	if (l->type != ET_CMP) {
		this->l = new PlnCmpOperation2(l, new PlnExpression(int64_t(0)), CMP_NE);
	} else {
		this->l = static_cast<PlnCmpOperation2*>(l);
	}

	if (r->type != ET_CMP) {
		this->r = new PlnCmpOperation2(r, new PlnExpression(int64_t(0)), CMP_NE);
	} else {
		this->r = static_cast<PlnCmpOperation2*>(r);
	}
}

PlnBoolOperation2::~PlnBoolOperation2()
{
	delete l;
	delete r;
}

inline void initConstType(PlnExprsnType type, int& definite_const, int& proxy_const)
{
	switch (type) {
		case ET_AND:
			definite_const = CMP_CONST_FALSE;
			proxy_const = CMP_CONST_TRUE;
			break;
		case ET_OR:
			definite_const = CMP_CONST_TRUE;
			proxy_const = CMP_CONST_FALSE;
			break;
	}
}

inline int getConstValue(int const_type) {
	BOOST_ASSERT(const_type == CMP_CONST_TRUE || const_type ==  CMP_CONST_FALSE);
	return const_type == CMP_CONST_TRUE ? 1 : 0;
}

// Case 1) l:definite_const, r:- -> definite_const
// Case 2) l:proxy_const, r:definite_const -> definite_const
// Case 3) l:proxy_const, r:proxy_const -> proxy_const
// Case 4) l:proxy_const, r:cmp -> r:cmp 
// Case 5) l:cmp, r:definite_const -> definite_const
// Case 6) l:cmp, r:proxy_const -> l:cmp
// Case 7) l:cmp = r:cmp -> cmp = r:cmp
// Case 8: l:cmp != r:cmp
//   >> Can't deside one jump condition. So, unifiy return cmp type to ne.
// Case 8-1) l:cmp == ne r:cmp != ne -> cmp = ne
// Case 8-2) l:cmp != ne r:cmp == ne -> cmp = ne
// Case 8-3) l:cmp != ne r:cmp != ne -> cmp = ne
void PlnBoolOperation2::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	int definite_const, proxy_const;
	initConstType(type, definite_const, proxy_const);

	l->finish(da, si);
	r->finish(da, si);
	int lcmp_type = l->getCmpType();
	int rcmp_type = r->getCmpType();
	if (lcmp_type == definite_const || rcmp_type == definite_const) {
		// Case 1,2,5)
		gen_cmp_type = definite_const;
		if (data_places.size()) {
			result_dp = da.getLiteralIntDp(getConstValue(definite_const));
			da.pushSrc(data_places[0], result_dp, true);
		}
		return;
	} 
	
	if (lcmp_type == proxy_const) {
		if (rcmp_type == proxy_const) {
			// Case 3)
			gen_cmp_type = proxy_const;
			if (data_places.size()) {
				result_dp = da.getLiteralIntDp(getConstValue(proxy_const));
				da.pushSrc(data_places[0], result_dp, true);
			}
			return;
		}

		// Case 4)
		if (data_places.size()) {
			result_dp = da.prepareAccumulator(DT_SINT);
			da.allocDp(result_dp);
			da.pushSrc(data_places[0], result_dp, true);
		}
		return;
	} 

	if (rcmp_type == proxy_const) {
		// Case 6)
		if (data_places.size()) {
			result_dp = da.prepareAccumulator(DT_SINT);
			da.allocDp(result_dp);
			da.pushSrc(data_places[0], result_dp, true);
		}
		return;
	}

	// Case 7,8
	PlnModule* m = si.scope[0].inf.module;
	jmp_l_id = m->getJumpID();
	jmp_r_id = m->getJumpID();
	result_dp = da.prepareAccumulator(DT_SINT);
	da.allocDp(result_dp);
	zero_dp = da.getLiteralIntDp(0);
	if (data_places.size()) {
		da.pushSrc(data_places[0], result_dp, true);
	} else {
		da.releaseDp(result_dp);
	}
}

void PlnBoolOperation2::gen(PlnGenerator& g)
{
	int definite_const, proxy_const;
	initConstType(type, definite_const, proxy_const);

	l->gen(g);
	int lcmp_type = l->getCmpType();

	if (gen_cmp_type == definite_const) {
		// Case 1,2,5)
		// Constant value is already set to src of data_place[0].
		if (data_places.size())
			g.genSaveSrc(data_places[0]);
		return;
	} 
	
	if (lcmp_type == proxy_const) {
		// Case 3,4)
		r->gen(g);
		gen_cmp_type = r->getCmpType();

		if (data_places.size()) {
			if (gen_cmp_type == proxy_const) {
				// Case 3)
				// Constant value is already set to result_dp in finish().

			} else {
				// Case 4)
				auto e = g.getEntity(result_dp);
				g.genMoveCmpFlag(e.get(), gen_cmp_type, "cmpflg -> " + result_dp->cmt());
			}
			g.genSaveSrc(data_places[0]);
		}
		return;
	}

	int rcmp_type = r->getCmpType();

	if (rcmp_type == proxy_const) {
		// Case 6)
		gen_cmp_type = lcmp_type;
		if (data_places.size()) {
			auto e = g.getEntity(result_dp);
			g.genMoveCmpFlag(e.get(), lcmp_type, "cmpflg -> " + result_dp->cmt());
			g.genSaveSrc(data_places[0]);
		}
		return;
	}

	// Case 7,8)
	auto result_e = g.getEntity(result_dp);

	if (type == ET_AND) g.genFalseJump(jmp_l_id, lcmp_type, "&&");
	else g.genTrueJump(jmp_l_id, lcmp_type, "||");

	r->gen(g);
	rcmp_type = r->getCmpType();

	if (lcmp_type == rcmp_type) {
		// Case 7)
		gen_cmp_type = lcmp_type;
		g.genJumpLabel(jmp_l_id, type==ET_AND ? "end &&" : "end ||");
		if (data_places.size())
			g.genMoveCmpFlag(result_e.get(), rcmp_type, "cmpflg -> " + result_dp->cmt());
			
		if (data_places.size())
			g.genSaveSrc(data_places[0]);

		return;
	}

	// Case 8)
	auto ze = g.getEntity(zero_dp);

	if (lcmp_type == CMP_NE) {
		// Case 8-1)
		g.genMoveCmpFlag(result_e.get(), rcmp_type, "cmpflg -> " + result_dp->cmt());
		gen_cmp_type = g.genCmp(result_e.get(), ze.get(), CMP_NE, result_dp->cmt() + " != 0");
		g.genJumpLabel(jmp_l_id, type==ET_AND ? "end &&" : "end ||");

	} else if (rcmp_type == CMP_NE) {
		// Case 8-2)
		g.genJump(jmp_r_id, "");
		g.genJumpLabel(jmp_l_id, "");
		g.genMoveCmpFlag(result_e.get(), lcmp_type, "cmpflg -> " + result_dp->cmt());
		gen_cmp_type = g.genCmp(result_e.get(), ze.get(), CMP_NE, result_dp->cmt() + " != 0");
		g.genJumpLabel(jmp_r_id, type==ET_AND ? "end &&" : "end ||");

	} else {
		// Case 8-3)
		g.genMoveCmpFlag(result_e.get(), rcmp_type, "cmpflg -> " + result_dp->cmt());
		g.genJump(jmp_r_id, "");
		g.genJumpLabel(jmp_l_id, "");
		g.genMoveCmpFlag(result_e.get(), lcmp_type, "cmpflg -> " + result_dp->cmt());
		g.genJumpLabel(jmp_r_id, type==ET_AND ? "end &&" : "end ||");
		gen_cmp_type = g.genCmp(result_e.get(), ze.get(), CMP_NE, result_dp->cmt() + " != 0");
	}
	BOOST_ASSERT(gen_cmp_type == CMP_NE);

	if (data_places.size()) {
		g.genMoveCmpFlag(result_e.get(), CMP_NE, "cmpflg -> " + result_dp->cmt());
		g.genSaveSrc(data_places[0]);
	}
}

PlnExpression* PlnBoolOperation2::getNot(PlnExpression *e)
{
	if (e->type == ET_CMP) {
		auto ce = static_cast<PlnCmpOperation2*>(e);
		PlnCmpType cmp_type;
		switch (ce->cmp_type) {
			case CMP_EQ: cmp_type=CMP_NE; break;
			case CMP_NE: cmp_type=CMP_EQ; break;
			case CMP_G: cmp_type=CMP_LE; break;
			case CMP_L: cmp_type=CMP_GE; break;
			case CMP_GE: cmp_type=CMP_L; break;
			case CMP_LE: cmp_type=CMP_G; break;
			/*
			// CMP_A/B will be generated by g.genCmp().
			case CMP_A: cmp_type=CMP_BE; break;
			case CMP_B: cmp_type=CMP_AE; break;
			case CMP_AE: cmp_type=CMP_B; break;
			case CMP_BE: cmp_type=CMP_A; break;
			*/
			/*
			// CMP_CONST_XXXX are available in finish phase.
			case CMP_CONST_TRUE: cmp_type=CMP_CONST_FALSE; break;
			case CMP_CONST_FALSE: cmp_type=CMP_CONST_TRUE; break;
			*/
			default: BOOST_ASSERT(false);
		}
		ce->cmp_type = cmp_type;
		return ce;
	}
	return new PlnCmpOperation2(e, new PlnExpression(int64_t(0)), CMP_EQ);
}
