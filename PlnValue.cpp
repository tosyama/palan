#include <boost/assert.hpp>
#include "PlnModel.h"
#include "PlnX86_64Generator.h"

inline int getBasePos(PlnBlock *b)
{
	BOOST_ASSERT(b);
	int pos = b->cur_stack_size;
	while (b->parent_type == BP_BLOCK) {
		b = b->parent.block;
		pos += b->cur_stack_size;
	}
	BOOST_ASSERT(b->parent_type == BP_FUNC);
	return b->parent.function->inf.pln.stack_size + pos;
}

//PlnVariable
PlnGenEntity* PlnVariable::genEntity(PlnGenerator& g)
{
	if (alloc_type == VA_STACK)
		return g.getStackAddress(inf.stack.pos_from_base);

	BOOST_ASSERT(false);
	return NULL;
}

// PlnVarInit
PlnVarInit::PlnVarInit(vector<PlnVariable*>& vars, PlnExpression* initializer)
	: vars(move(vars)), initializer(initializer)
{
}

void PlnVarInit::finish()
{
	for (auto v: vars)
		if (v->alloc_type == VA_UNKNOWN) {
			v->alloc_type = VA_STACK;
			parent->cur_stack_size += v->var_type->size;
			v->inf.stack.pos_from_base = getBasePos(parent);
		}

	if (initializer) {
		PlnReturnPlace rp;
		rp.type = RP_VAR;
		for (auto v: vars) {
			rp.inf.var = v;
			initializer->ret_places.push_back(rp);
		}
		initializer->finish();
	}
}

void PlnVarInit::gen(PlnGenerator& g)
{
	if (initializer) {
		initializer->gen(g);
		BOOST_ASSERT(initializer->values.size() >= vars.size());
	}
}

// PlnValue
PlnValue::PlnValue(int intValue)
	: type(VL_LIT_INT8)
{
	inf.intValue = intValue;
}

PlnValue::PlnValue(PlnReadOnlyData* rod)
	: type(VL_RO_DATA)
{
	inf.rod = rod;
}

PlnValue::PlnValue(PlnVariable* var)
	: type(VL_VAR)
{
	inf.var = var;
}

PlnGenEntity* PlnValue::genEntity(PlnGenerator& g)
{
	switch (type) {
		case VL_LIT_INT8:
			return g.getInt(inf.intValue);
		case VL_RO_DATA:
			return inf.rod->genEntity(g);
		case VL_VAR:
			return inf.var->genEntity(g);
	}
	BOOST_ASSERT(false);
}

void PlnReadOnlyData::gen(PlnGenerator &g)
{
	switch (type) {
		case RO_LIT_STR:
			g.genStringData(index, name); 
			break;
		default:
			BOOST_ASSERT(false);
	}
}

PlnGenEntity* PlnReadOnlyData::genEntity(PlnGenerator &g)
{
	switch (type) {
		case RO_LIT_STR:
			return g.getStrAddress(index); 
		default:
			BOOST_ASSERT(false);
	}
}
