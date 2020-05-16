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

PlnIfStatement::PlnIfStatement
	(PlnExpression* condition, PlnBlock* block, PlnStatement* next, PlnBlock* parent)
	: cond_dp(NULL), jmp_next_id(-1), jmp_end_id(-1), next(next)
{
	type = ST_IF;
	inf.block = block;
	this->parent = parent;

	this->condition = PlnBoolExpression::create(condition);
}

PlnIfStatement::~PlnIfStatement()
{
	delete condition;
	delete inf.block;
	if (next)
		delete next;
}

void PlnIfStatement::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	BOOST_ASSERT(si.scope[0].type == SC_MODULE);
	PlnModule* m = si.scope[0].inf.module;

	jmp_next_id = m->getJumpID();

	condition->jmp_if = 0;
	condition->jmp_id = jmp_next_id;
	condition->finish(da, si);
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
	condition->gen(g);
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

