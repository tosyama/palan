/// PlnClone model class definition.
///
/// @file	PlnClone.cpp
/// @copyright	2018 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include "../PlnExpression.h"
#include "../PlnVariable.h"
#include "../PlnType.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnGenerator.h"
#include "PlnClone.h"
#include "PlnArrayValue.h"

PlnClone::PlnClone(PlnDataAllocator& da, PlnExpression* src_ex, PlnType* var_type, bool keep_var)
	: PlnExpression(ET_CLONE), src_ex(NULL), free_ex(NULL), copy_ex(NULL), keep_var(keep_var)
{
	var = PlnVariable::createTempVar(da, var_type, "(clone)");
	alloc_ex = var_type->allocator->getAllocEx();
	alloc_ex->data_places.push_back(var->place);

	directAssign = (src_ex->type == ET_ARRAYVALUE && !static_cast<PlnArrayValue*>(src_ex)->doCopyFromStaticBuffer);
 
	if (!directAssign) {
		copy_ex = var_type->copyer->getCopyEx();
		src_ex->data_places.push_back(copy_ex->srcDp(da));
		copy_dst_dp = copy_ex->dstDp(da);
	}
	this->src_ex = src_ex;
}

PlnClone::~PlnClone()
{
	delete alloc_ex;
	delete copy_ex;
	delete free_ex;
}

void PlnClone::finishAlloc(PlnDataAllocator& da, PlnScopeInfo& si)
{
	alloc_ex->finish(da, si);
	da.popSrc(var->place);
	if (directAssign) {
		src_ex->data_places.push_back(var->place);
	}
}

void PlnClone::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	BOOST_ASSERT(data_places.size() == 1);
	if (!directAssign) {
		da.pushSrc(copy_dst_dp, var->place, false);
		copy_ex->finish(da, si);
	}

	da.pushSrc(data_places[0], var->place, !keep_var);
}

void PlnClone::finishFree(PlnDataAllocator& da, PlnScopeInfo& si)
{
	BOOST_ASSERT(keep_var);
	free_ex = var->var_type->freer->getFreeEx(var);
	free_ex->finish(da, si);
	da.releaseDp(var->place);
}

void PlnClone::genAlloc(PlnGenerator& g)
{
	alloc_ex->gen(g);
	g.genLoadDp(var->place);
}

void PlnClone::gen(PlnGenerator& g)
{
	if (!directAssign) {
		g.genSaveSrc(copy_dst_dp);
		copy_ex->gen(g);
	}
	g.genSaveSrc(data_places[0]);
}

void PlnClone::genFree(PlnGenerator& g)
{
	free_ex->gen(g);
}

