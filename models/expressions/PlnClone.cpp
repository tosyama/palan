/// PlnClone model class definition.
///
/// @file	PlnClone.cpp
/// @copyright	2018-2019 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include "../PlnExpression.h"
#include "../PlnVariable.h"
#include "../PlnType.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnGenerator.h"
#include "PlnClone.h"
#include "PlnArrayValue.h"
#include "PlnArrayItem.h"
#include "PlnStructMember.h"
#include "assignitem/PlnAssignItem.h"

PlnClone::PlnClone(PlnDataAllocator& da, PlnExpression* src_ex, PlnVarType* var_type, bool keep_var)
	: PlnExpression(ET_CLONE), src_ex(NULL), free_ex(NULL), copy_ex(NULL), keep_var(keep_var)
{
	var_type = var_type->typeinf->getVarType();
	var = PlnVariable::createTempVar(da, var_type, "(clone)");
	alloc_ex = var_type->getAllocEx();
	alloc_ex->data_places.push_back(var->place);

	directAssign = (src_ex->type == ET_ARRAYVALUE && !static_cast<PlnArrayValue*>(src_ex)->doCopyFromStaticBuffer);
 
	if (!directAssign) {
		copy_ex = var_type->getCopyEx();
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
		BOOST_ASSERT(src_ex->type == ET_ARRAYVALUE);
		vector<PlnExpression*> val_items = static_cast<PlnArrayValue*>(src_ex)->getAllItems();
		vector<PlnExpression*> dst_items;
		if (var->var_type->typeinf->type == TP_FIXED_ARRAY) {
			dst_items = PlnArrayItem::getAllArrayItems(var);
		} else if (var->var_type->typeinf->type == TP_STRUCT) {
			dst_items = PlnStructMember::getAllStructMembers(var);
		} else
			BOOST_ASSERT(false);

		for (int i=0; i<val_items.size(); i++) {
			PlnAssignItem *ai = PlnAssignItem::createAssignItem(val_items[i]);
			dst_items[i]->values[0].asgn_type = ASGN_COPY;
			ai->addDstEx(dst_items[i], false);
			assign_items.push_back(ai);
		}

		// src_ex->data_places.push_back(var->place);
	}
}

void PlnClone::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	BOOST_ASSERT(data_places.size() == 1);
	if (directAssign) {
		for (auto ai: assign_items) {
			ai->finishS(da, si);
			ai->finishD(da, si);
		}

	} else {
		da.pushSrc(copy_dst_dp, var->place, false);
		copy_ex->finish(da, si);

	}

	da.pushSrc(data_places[0], var->place, !keep_var);
}

void PlnClone::finishFree(PlnDataAllocator& da, PlnScopeInfo& si)
{
	BOOST_ASSERT(keep_var);
	free_ex = PlnFreer::getFreeEx(var);
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
	if (directAssign) {
		for (auto ai: assign_items) {
			ai->genS(g);
			ai->genD(g);
		}
	} else {
		g.genSaveSrc(copy_dst_dp);
		copy_ex->gen(g);
	}
	g.genSaveSrc(data_places[0]);
}

void PlnClone::genFree(PlnGenerator& g)
{
	free_ex->gen(g);
}

