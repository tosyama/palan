#include <boost/assert.hpp>
#include "PlnModel.h"

void PlnBlock::gen(PlnGenerator& g)
{
	for (auto s: statements)
		s->gen(g);
}

void PlnBlock::addStatement(PlnStatement& statement)
{
	statements.push_back(&statement);
}

void PlnStatement::gen(PlnGenerator& g)
{
	switch (type) {
		case ST_EXPRSN:
			inf.expression->gen(g);
			break;
		case ST_BLOCK:
			inf.block->gen(g);
			break;
		default:
			BOOST_ASSERT(false);	
	}
}
