#include <boost/assert.hpp>
#include "PlnModel.h"
#include "PlnScopeStack.h"
#include "PlnX86_64Generator.h"

using std::endl;

inline PlnFunction* getFunction(PlnBlock *b)
{
	BOOST_ASSERT(b);
	while (b->parent_type == BP_BLOCK) {
		b = b->parent.block;
	}
	BOOST_ASSERT(b->parent_type == BP_FUNC);
	return b->parent.function;
}

void PlnBlock::setParent(PlnScopeItem& scope)
{
	switch(scope.type) {
		case SC_BLOCK:
			parent_type = BP_BLOCK;
			parent.block = scope.inf.block;
			break;
		case SC_FUNCTION:
			parent_type = BP_FUNC;
			parent.function = scope.inf.function;
			break;
		default:
			BOOST_ASSERT(false);
	}
}

int PlnBlock::stackSize()
{
	int sz;
	int maxsz = 0;
	for (auto s: statements) {
		if (s->type == ST_BLOCK) {
			if ((sz = s->inf.block->stackSize()) > maxsz) {
				maxsz = sz;
			}
		}
	}
	return variables.size() * 8 + maxsz;
}

int PlnBlock::declareVariable(string& var_name, string type_name)
{
	PlnVariable* v = new PlnVariable();
	if (type_name == "int") {
		v->type = VT_INT8;
	} else if (type_name == "") {
		v->type = variables.back()->type;
	} else
		return 1;

	variables.push_back(v);

	return 0;
}

void PlnBlock::dump(ostream& os, string indent)
{
	os << indent << "Block: " << statements.size() << endl;
	for (auto v: variables)
		os << indent+" " << "Variable: " << v->name << "(" << v->type << ")" << endl;

	for (auto s: statements)
		s->dump(os, indent+" ");
}

void PlnBlock::gen(PlnGenerator& g)
{
	for (auto s: statements)
		s->gen(g);
}

void PlnStatement::dump(ostream& os, string indent)
{
	switch (type) {
		case ST_EXPRSN:
			inf.expression->dump(os, indent);
			break;
		case ST_DECLR:
			os << indent << "Declare:" << endl;
			break;
		case ST_BLOCK:
			inf.block->dump(os, indent);
			break;
		case ST_RETURN:
			os << indent << "Return: " << inf.return_vals->size() << endl;
			break;
		default:
			os << indent << "Unknown type" << endl;
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
		case ST_RETURN:
		{
			vector<PlnGenEntity*> gen_rets;
			for (auto r: *inf.return_vals) 
				gen_rets.push_back(r->value.genEntity(g));
			PlnFunction* f = getFunction(parent);

			if (f->name == "main") 
				g.genMainRetun(gen_rets);	
			
			for (auto gr: gen_rets)
				PlnGenEntity::freeEntity(gr);
		}
			break;
		case ST_DECLR:
			break;
		default:
			BOOST_ASSERT(false);	
			break;
	}
}
