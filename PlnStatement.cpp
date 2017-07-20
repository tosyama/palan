#include <boost/assert.hpp>
#include "PlnModel.h"

using std::endl;

void PlnBlock::dump(ostream& os, string indent)
{
	os << indent << "Block: " << statements.size() << endl;
	for (auto s: statements)
		s->dump(os, indent+" ");
}

void PlnBlock::gen(PlnGenerator& g)
{
	for (auto s: statements)
		s->gen(g);
}

void PlnBlock::addStatement(PlnStatement& statement)
{
	statements.push_back(&statement);
}

void PlnStatement::dump(ostream& os, string indent)
{
	switch (type) {
		case ST_EXPRSN:
			inf.expression->dump(os, indent+" ");
			break;
		case ST_BLOCK:
			inf.block->dump(os, indent+" ");
			break;
	}
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
