/// Conditinal branch model classes definition.
///
/// @file	PlnConditionalBranch.cpp
/// @copyright	2018-2020 YAMAGUCHI Toshinobu 

#include "boost/assert.hpp"
#include "PlnConditionalBranch.h"
#include "PlnModule.h"
#include "PlnBlock.h"
#include "../PlnScopeStack.h"
#include "../PlnGenerator.h"
#include "expressions/PlnCmpOperation.h"

extern bool migrate;

PlnIfStatement::PlnIfStatement
	(PlnExpression* condition, PlnBlock* block, PlnStatement* next, PlnBlock* parent)
	: cond_dp(NULL), jmp_next_id(-1), jmp_end_id(-1), next(next)
{
	type = ST_IF;
	inf.block = block;
	this->parent = parent;

	if (migrate) {
		this->condition = PlnBoolExpression::create(condition);
		this->condition2 = NULL;

	} else {
		this->condition = NULL;
		if (condition->type == ET_CMP
				|| condition->type == ET_AND || condition->type == ET_OR) {
			this->condition2 = static_cast<PlnCmpExpression*>(condition);
		} else {
			this->condition2 = new PlnCmpOperation2(new PlnExpression(int64_t(0)), condition, CMP_NE);
		}
	}
}

PlnIfStatement::~PlnIfStatement()
{
	delete condition;
	delete condition2;
	delete inf.block;
	if (next)
		delete next;
}

void PlnIfStatement::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	BOOST_ASSERT(si.scope[0].type == SC_MODULE);
	PlnModule* m = si.scope[0].inf.module;

	jmp_next_id = m->getJumpID();

	if (migrate) {
		condition->jmp_if = 0;
		condition->jmp_id = jmp_next_id;
		condition->finish(da, si);
	} else {
		condition2->finish(da, si);
	}
	inf.block->finish(da, si);

	if (next) {
		next->finish(da, si);
		if (next->type == ST_IF) {
			jmp_end_id = static_cast<PlnIfStatement*>(next)->jmp_end_id;
		} else {
			jmp_end_id = m->getJumpID();
		}

	} else {
		jmp_end_id = jmp_next_id;
	}

}

void PlnIfStatement::gen(PlnGenerator& g)
{
	if (migrate) {
		condition->gen(g);
	} else {
		condition2->gen(g);
		int cmp_type = condition2->getCmpType();
		if (cmp_type == CMP_CONST_TRUE) 
			g.comment(" if true");	// 	do nothing.
		
		else if (cmp_type == CMP_CONST_FALSE)
			g.genJump(jmp_next_id, "if false");
		else
			g.genFalseJump(jmp_next_id, cmp_type, "if");
	}

	inf.block->gen(g);

	if (next) {
		g.genJump(jmp_end_id, "end if");
		g.genJumpLabel(jmp_next_id, "else");
		next->gen(g);
		if (next->type != ST_IF)	// else statement.
			g.genJumpLabel(jmp_end_id, "end else");

	} else {	// No additional else statement.
		g.genJumpLabel(jmp_next_id, "end if");
	}
}

