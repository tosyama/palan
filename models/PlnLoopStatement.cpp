/// Loop statement model classes definition.
///
/// @file	PlnLoopStatement.cpp
/// @copyright	2018-2020 YAMAGUCHI Toshinobu 

#include "boost/assert.hpp"
#include "PlnLoopStatement.h"
#include "PlnModule.h"
#include "PlnBlock.h"
#include "../PlnScopeStack.h"
#include "../PlnGenerator.h"
#include "expressions/PlnCmpOperation.h"

PlnWhileStatement::PlnWhileStatement(PlnExpression* condition, PlnBlock* block, PlnBlock* parent)
	: cond_dp(NULL), jmp_start_id(-1), jmp_end_id(-1)
{
	type = ST_WHILE;
	block->owner_stmt = this;
	inf.block = block;
	this->parent = parent;
	this->condition = PlnBoolExpression::create(condition);
}

PlnWhileStatement::~PlnWhileStatement()
{
	delete condition;
	delete inf.block;
}

void PlnWhileStatement::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	BOOST_ASSERT(si.scope[0].type == SC_MODULE);
	PlnModule* m = si.scope[0].inf.module;

	jmp_start_id = m->getJumpID();
	jmp_end_id = m->getJumpID();

	condition->jmp_if = 0;
	condition->jmp_id = jmp_end_id;
	condition->finish(da, si);
	inf.block->finish(da, si);
}

void PlnWhileStatement::gen(PlnGenerator& g)
{
	g.genJumpLabel(jmp_start_id, "while");
	condition->gen(g);
	inf.block->gen(g);
	g.genJump(jmp_start_id, "");
	g.genJumpLabel(jmp_end_id, "end while");
}

// PlnBreakStatement
PlnBreakStatement::PlnBreakStatement(PlnStatement* target_stmt)
	: target_stmt(target_stmt), jmp_id(-1)
{
	type = ST_BREAK;
}

void PlnBreakStatement::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	if (target_stmt->type == ST_WHILE) {
		jmp_id = static_cast<PlnWhileStatement*>(target_stmt)->jmp_end_id;
	} else
		BOOST_ASSERT(false);
		
	BOOST_ASSERT(jmp_id >= 0);

	bool valid=false;
	for (auto sitem=si.scope.rbegin(); sitem!=si.scope.rend(); sitem++) {
		if (sitem->type == SC_BLOCK) {
			sitem->inf.block->addFreeVars(free_vars, da, si);
			if (sitem->inf.block->owner_stmt == target_stmt) {
				valid = true;
				break;
			}
		}
	}
	BOOST_ASSERT(valid);
}

void PlnBreakStatement::gen(PlnGenerator& g)
{
	for (auto free_var: free_vars)
		free_var->gen(g);

	g.genJump(jmp_id, "break");
}

// PlnContinueStatement
PlnContinueStatement::PlnContinueStatement(PlnStatement* target_stmt)
	: target_stmt(target_stmt), jmp_id(-1)
{
	type = ST_CONTINUE;
}

void PlnContinueStatement::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	if (target_stmt->type == ST_WHILE) {
		jmp_id = static_cast<PlnWhileStatement*>(target_stmt)->jmp_start_id;
	} else
		BOOST_ASSERT(false);
		
	BOOST_ASSERT(jmp_id >= 0);

	bool valid=false;
	for (auto sitem=si.scope.rbegin(); sitem!=si.scope.rend(); sitem++) {
		if (sitem->type == SC_BLOCK) {
			sitem->inf.block->addFreeVars(free_vars, da, si);
			if (sitem->inf.block->owner_stmt == target_stmt) {
				valid = true;
				break;
			}
		}
	}
	BOOST_ASSERT(valid);
}

void PlnContinueStatement::gen(PlnGenerator& g)
{
	for (auto free_var: free_vars)
		free_var->gen(g);

	g.genJump(jmp_id, "continue");
}

