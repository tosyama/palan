#include <boost/assert.hpp>
#include "PlnModel.h"
#include "PlnX86_64Generator.h"
//PlnVariable

PlnGenEntity* PlnVariable::genEntity(PlnGenerator& g)
{
	if (alloc_type == VA_STACK)
		return g.getStackAddress(inf.stack.pos_from_base);

	BOOST_ASSERT(false);
	return NULL;
}

// PlnValue
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
