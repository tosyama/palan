/// Loop statement model classes definition.
///
/// @file	PlnLoopStatement.cpp
/// @copyright	2018-2019 YAMAGUCHI Toshinobu 

#include "boost/assert.hpp"
#include "PlnLoopStatement.h"
#include "PlnModule.h"
#include "PlnBlock.h"
#include "../PlnScopeStack.h"
#include "../PlnGenerator.h"
#include "expressions/PlnCmpOperation.h"


PlnWhileStatement::PlnWhileStatement(PlnExpression* condition, PlnBlock* block, PlnBlock* parent)
	: cond_dp(NULL), jmp_start_id(0), jmp_end_id(0)
{
	type = ST_WHILE;
	block->owner_stmt = this;
	inf.block = block;
	this->parent = parent;

	if (condition->type == ET_CMP
		 || condition->type == ET_AND || condition->type == ET_OR) {
		this->condition = static_cast<PlnCmpExpression*>(condition);
	} else {
		this->condition = new PlnCmpOperation(new PlnExpression(int64_t(0)), condition, CMP_NE);
	}
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

	condition->finish(da, si);
	inf.block->finish(da, si);
}

void PlnWhileStatement::gen(PlnGenerator& g)
{
	g.genJumpLabel(jmp_start_id, "while");
	condition->gen(g);

	int cmp_type = condition->getCmpType();

	if (cmp_type == CMP_CONST_TRUE) 
		;	// 	do nothing.
	else if (cmp_type == CMP_CONST_FALSE)
		g.genJump(jmp_end_id, "");
	else
		g.genFalseJump(jmp_end_id, cmp_type, "");

	inf.block->gen(g);
	g.genJump(jmp_start_id, "");
	g.genJumpLabel(jmp_end_id, "end while");
}

// PlnBreakStatement
PlnBreakStatement::PlnBreakStatement(PlnStatement* target_stmt)
	: target_stmt(target_stmt)
{
	type = ST_BREAK;
}

void PlnBreakStatement::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	if (target_stmt->type == ST_WHILE) {
		jmp_id = static_cast<PlnWhileStatement*>(target_stmt)->jmp_end_id;
	} else
		BOOST_ASSERT(false);
		
	BOOST_ASSERT(jmp_id > 0);

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
	: target_stmt(target_stmt)
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

