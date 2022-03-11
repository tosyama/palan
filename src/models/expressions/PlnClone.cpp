/// PlnClone model class definition.
///
/// @file	PlnClone.cpp
/// @copyright	2018-2022 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>

#include "../../PlnConstants.h"
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
	: PlnExpression(ET_CLONE), src_ex(NULL), dup_src_ex(NULL), free_ex(NULL), copy_ex(NULL), src_tmp_var(NULL), keep_var(keep_var)
{
	var_type = var_type->getVarType();
	var = PlnVariable::createTempVar(da, var_type, "(clone)");
	values.push_back(var);

	vector<PlnExpression*> args;
	var_type->getInitExpressions(args);
	alloc_ex = var_type->getAllocEx(args);
	alloc_ex->data_places.push_back(var->place);

	directAssign = (src_ex->type == ET_ARRAYVALUE && !static_cast<PlnArrayValue*>(src_ex)->doCopyFromStaticBuffer);
 
	if (!directAssign) {
		int val_ind = src_ex->data_places.size();
		PlnExpression* dst_var_ex = new PlnExpression(var);

		if (src_ex->type == ET_VALUE && (src_ex->values[0].type == VL_VAR || src_ex->values[0].type == VL_LIT_STR)) {
			// TODO: + VL_LIT_ARRAY or other optimazation methods.
			BOOST_ASSERT(src_ex->values.size() == 1);
			if (src_ex->values[0].type == VL_VAR) {
				dup_src_ex = new PlnExpression(src_ex->values[0].inf.var);

			} else if (src_ex->values[0].type == VL_LIT_STR) {
				dup_src_ex = new PlnExpression(*src_ex->values[0].inf.strValue);
			}

			copy_ex = var_type->getCopyEx(dst_var_ex, dup_src_ex);

		} else {
			PlnVarType *tmp_vartype = src_ex->values[val_ind].getVarType()->getVarType("rir");
			src_tmp_var = PlnVariable::createTempVar(da, tmp_vartype, "(src tmp var)");
			src_ex->data_places.push_back(src_tmp_var->place);

			PlnExpression* src_var_ex = new PlnExpression(src_tmp_var);
			copy_ex = var_type->getCopyEx(dst_var_ex, src_var_ex);
		}
	}
	this->src_ex = src_ex;
}

PlnClone::~PlnClone()
{
	delete alloc_ex;
	delete copy_ex;
	delete free_ex;
	delete src_tmp_var;
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
	}
}

void PlnClone::finishCopy(PlnDataAllocator& da, PlnScopeInfo& si)
{
	if (directAssign) {
		for (auto ai: assign_items) {
			ai->finishS(da, si);
			ai->finishD(da, si);
		}

	} else if (src_tmp_var) {
		da.popSrc(src_tmp_var->place);
		copy_ex->finish(da, si);
		da.releaseDp(src_tmp_var->place);

	} else {
		copy_ex->finish(da, si);
	}

}

void PlnClone::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	BOOST_ASSERT(data_places.size() == 1);
	da.pushSrc(data_places[0], var->place, !keep_var);
}

void PlnClone::finishFree(PlnDataAllocator& da, PlnScopeInfo& si)
{
	BOOST_ASSERT(keep_var);
	free_ex = var->getFreeEx();
	free_ex->finish(da, si);
	da.releaseDp(var->place);
}

void PlnClone::genAlloc(PlnGenerator& g)
{
	alloc_ex->gen(g);
	g.genLoadDp(var->place);
}

void PlnClone::genCopy(PlnGenerator& g)
{
	if (directAssign) {
		for (auto ai: assign_items) {
			ai->genS(g);
			ai->genD(g);
		}

	} else if (src_tmp_var) {
		g.genLoadDp(src_tmp_var->place);
		copy_ex->gen(g);

	} else {
		copy_ex->gen(g);

	}
}

void PlnClone::gen(PlnGenerator& g)
{
	g.genSaveSrc(data_places[0]);
}

void PlnClone::genFree(PlnGenerator& g)
{
	free_ex->gen(g);
}

