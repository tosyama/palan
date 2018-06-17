/// PlnClone expression class definition.
///
/// @file	PlnClone.cpp
/// @copyright	2017 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include "PlnClone.h"
#include "../PlnType.h"
#include "../PlnVariable.h"
#include "../../PlnConstants.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnGenerator.h"

PlnClone::PlnClone(PlnExpression* src)
	: PlnExpression(ET_CLONE), clone_src(src),
	  alloc_ex(NULL), copy_ex(NULL), clone_var(NULL)
{
	BOOST_ASSERT(src->values.size() == 1);
	BOOST_ASSERT(src->values[0].type == VL_VAR); 
	PlnValue val;
	val.type = VL_WORK;
	val.inf.wk_type = new vector<PlnType*>(src->values[0].inf.var->var_type);
	val.asgn_type = NO_ASGN;

	values.push_back(val);
}

void PlnClone::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	BOOST_ASSERT(data_places.size()==1);

	clone_var = PlnVariable::createTempVar(da, *values[0].inf.wk_type, "clone");
	alloc_ex = values[0].getType()->allocator->getAllocEx();
	alloc_ex->data_places.push_back(clone_var->place);
	alloc_ex->finish(da, si);
	da.popSrc(clone_var->place);

	copy_ex = values[0].getType()->copyer->getCopyEx(new PlnExpression(clone_var), clone_src);
	copy_ex->finish(da, si);

	da.pushSrc(data_places[0], clone_var->place);
}

void PlnClone::gen(PlnGenerator& g)
{
	alloc_ex->gen(g);
	g.genLoadDp(clone_var->place);
	copy_ex->gen(g);
	g.genSaveSrc(data_places[0]);
}

