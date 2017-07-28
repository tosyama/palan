#include <boost/assert.hpp>
#include <boost/format.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include "PlnModel.h"
#include "PlnScopeStack.h"
#include "PlnX86_64Generator.h"

using std::endl;
using boost::format;
using boost::adaptors::reverse;

inline PlnFunction* getFunction(PlnBlock *b)
{
	BOOST_ASSERT(b);
	while (b->parent_type == BP_BLOCK) {
		b = b->parent.block;
	}
	BOOST_ASSERT(b->parent_type == BP_FUNC);
	return b->parent.function;
}

inline int getBasePos(PlnBlock *b)
{
	BOOST_ASSERT(b);
	int pos = 0;
	while (b->parent_type == BP_BLOCK) {
		b = b->parent.block;
		pos += b->cur_stack_size;
	}
	BOOST_ASSERT(b->parent_type == BP_FUNC);
	return b->parent.function->inf.pln.stack_size + pos;
}

PlnBlock::PlnBlock() : cur_stack_size(0)
{
}

PlnVariable* PlnBlock::getVariable(string& var_name)
{
	PlnBlock* b = this;
	for(;;) {
		for (auto v: b->variables)
			if (v->name == var_name)
				return v;
		if (b->parent_type == BP_BLOCK)
			b = b->parent.block;
		else {
			PlnFunction* f=b->parent.function;
			// TODO: search param.
			return NULL;
		}
	}
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

int PlnBlock::totalStackSize()
{
	int sz;
	int maxsz = 0;
	for (auto v: reverse(variables)) {
		if (v->alloc_type == VA_STACK) {
			maxsz = v->inf.stack.pos_from_base;
			break;
		}
	}
	
	for (auto s: statements) {
		if (s->type == ST_BLOCK) {
			if ((sz = s->inf.block->totalStackSize()) > maxsz) {
				maxsz = sz;
			}
		}
	}
	return maxsz;
}

PlnVariable* PlnBlock::declareVariable(string& var_name, PlnType* var_type)
{
	for (auto v: variables)
		if (v->name == var_name) return NULL;
	
	PlnVariable* v = new PlnVariable();
	v->name = var_name;

	if (var_type) v->var_type = var_type;
	else v->var_type = variables.back()->var_type;

	cur_stack_size += v->var_type->size;

	v->alloc_type = VA_STACK;
	v->inf.stack.pos_from_base = getBasePos(this)+cur_stack_size;
	variables.push_back(v);

	return v;
}

void PlnBlock::dump(ostream& os, string indent)
{
	os << indent << "Block: " << statements.size() << endl;
	for (auto v: variables)
		os << format("%1% Variable: %2% %3%(%4%)")
				% indent % v->var_type->name % v->name % v->inf.stack.pos_from_base << endl;

	for (auto s: statements)
		s->dump(os, indent+" ");
}

void PlnBlock::gen(PlnGenerator& g)
{
	for (auto s: statements)
		s->gen(g);
}

// PlnStatement
PlnStatement::PlnStatement(PlnExpression *exp, PlnBlock* parent)
	: type(ST_EXPRSN), parent(parent)
{
	inf.expression = exp;
}

PlnStatement::PlnStatement(vector<PlnVarInit*> &var_inits, PlnBlock* parent)
	: type(ST_VARINIT), parent(parent)
{
	inf.var_inits = new vector<PlnVarInit*>();
	*(inf.var_inits) = move(var_inits);
}

PlnStatement::PlnStatement(PlnBlock* block, PlnBlock* parent)
	: type(ST_BLOCK), parent(parent)
{
	inf.block = block;
}

bool PlnStatement::isEmpty()
{
	if (type == ST_VARINIT && inf.var_inits->size() == 0) return true;
	else if (type == ST_EXPRSN && inf.expression == NULL) return true;
	
	return false;
}

void PlnStatement::dump(ostream& os, string indent)
{
	switch (type) {
		case ST_EXPRSN:
			inf.expression->dump(os, indent);
			break;

		case ST_VARINIT:
			os << indent << "Initialize: ";
			for (auto vi: *inf.var_inits)
				os << vi->vars[0]->name << " ";
			os << endl;
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
		case ST_VARINIT:
			for (auto vi: *inf.var_inits)
				vi->gen(g);
			break;
		case ST_BLOCK:
			inf.block->gen(g);
			break;
		case ST_RETURN:
		{
			vector<PlnGenEntity*> gen_rets;
			for (auto r: *inf.return_vals) 
				gen_rets.push_back(r->values.front().genEntity(g));
			PlnFunction* f = getFunction(parent);

			if (f->name == "main") 
				g.genMainRetun(gen_rets);	
			
			for (auto gr: gen_rets)
				PlnGenEntity::freeEntity(gr);
		}
			break;
		default:
			BOOST_ASSERT(false);	
			break;
	}
}

void PlnVarInit::gen(PlnGenerator& g)
{
	initializer->gen(g);
	BOOST_ASSERT(initializer->values.size() >= vars.size());
	int i=0;
	for (auto var: vars) {
		PlnGenEntity *src_en = initializer->values[i].genEntity(g);
		PlnGenEntity *dst_en = var->genEntity(g);
		g.genMove(dst_en, src_en, var->name);
		PlnGenEntity::freeEntity(src_en);
		PlnGenEntity::freeEntity(dst_en);
		++i;
	}
}
