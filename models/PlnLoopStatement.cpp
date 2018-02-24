/// Loop statement model classes definition.
///
/// @file	PlnLoopStatement.cpp
/// @copyright	2018 YAMAGUCHI Toshinobu 

#include "PlnLoopStatement.h"
#include "PlnModule.h"
#include "PlnBlock.h"
#include "../PlnScopeStack.h"
#include "../PlnGenerator.h"
#include "expressions/PlnCmpOperation.h"

#include "boost/assert.hpp"

PlnWhileStatement::PlnWhileStatement
	(PlnExpression* condition, PlnBlock* block, PlnBlock* parent)
	: cond_dp(NULL), jmp_start_id(0), jmp_end_id(0)
{
	type = ST_WHILE;
	inf.block = block;
	this->parent = parent;

	if (condition->type != ET_CMP) {
		this->condition = new PlnCmpOperation(new PlnExpression(int64_t(0)), condition, CMP_NE);
	} else
		this->condition = static_cast<PlnCmpExpression*>(condition);
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

void PlnWhileStatement::dump(ostream& os, string indent)
{
	os << indent << "While: " << endl;
	condition->dump(os, indent+" ");
	inf.block->dump(os, indent+" ");
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
