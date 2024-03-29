/// Array item model class definition.
///
/// PlnArrayItem returns item of array
/// that indicated by index.
/// e.g.) a[3]
///
/// @file	PlnArrayItem.cpp
/// @copyright	2017-2019 YAMAGUCHI Toshinobu 

#include "boost/assert.hpp"
#include "../../PlnConstants.h"
#include "PlnArrayItem.h"
#include "PlnMulOperation.h"
#include "PlnAddOperation.h"
#include "../PlnVariable.h"
#include "../PlnType.h"
#include "../types/PlnFixedArrayType.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnGenerator.h"
#include "../../PlnConstants.h"
#include "../../PlnMessage.h"
#include "../../PlnException.h"

static PlnExpression* getIndexExpression(
	int index, int offset,
	vector<PlnExpression*> &item_ind,
	vector<int> &arr_sizes)
{
	PlnExpression *ex = item_ind[index];
	if (offset > 1) {
		auto oex = new PlnExpression(PlnValue((int64_t)offset));
		ex = PlnMulOperation::create(ex, oex);
	}

	if (index) {
		int next_offset = offset * arr_sizes[index];
		auto base_ex = getIndexExpression(index-1, next_offset, item_ind, arr_sizes);
		ex = PlnAddOperation::create(base_ex, ex);
	}

	return ex;
}

PlnArrayItem::PlnArrayItem(PlnExpression *array_ex, const vector<PlnExpression*>& item_ind)
	: PlnArrayItem(array_ex, item_ind, array_ex->values[0].inf.var->var_type)
{
}

static PlnVariable* getArrayVar(PlnVarType *item_type, PlnExpression* array_ex)
{
	BOOST_ASSERT(array_ex->values[0].type == VL_VAR);
	auto var = new PlnVariable();
	auto array_var = array_ex->values[0].inf.var;
	var->name = array_var->name + "[]";
	var->var_type = item_type;

	var->is_indirect = true;
	if (array_var->container)
		var->container = array_var->container;
	else
		var->container = array_var;
	var->is_tmpvar = var->container->is_tmpvar;
	return var;
}

// Can be any array type.
PlnArrayItem::PlnArrayItem(PlnExpression *array_ex, vector<PlnExpression*> item_ind,
	PlnVarType* arr_type)
	: PlnExpression(ET_ARRAYITEM), array_ex(array_ex)
{
	BOOST_ASSERT(item_ind.size());
	BOOST_ASSERT(array_ex->values[0].type == VL_VAR);

	auto array_var = array_ex->values[0].inf.var;

	if (arr_type->typeinf->type != TP_FIXED_ARRAY) {
		PlnCompileError err(E_CantUseIndexHere, array_var->name);
		throw err;
	}
	PlnFixedArrayTypeInfo *farr_type = static_cast<PlnFixedArrayTypeInfo*>(arr_type->typeinf);
	values.push_back(getArrayVar(farr_type->item_type, array_ex));

	auto& arr_sizes = static_cast<PlnFixedArrayVarType*>(arr_type)->sizes;
	BOOST_ASSERT(arr_sizes.size() == item_ind.size());
	index_ex = getIndexExpression(item_ind.size()-1, 1, item_ind, arr_sizes);
	if (index_ex->type == ET_VALUE) {
		if (index_ex->values[0].type == VL_LIT_UINT8) {
			int64_t i = index_ex->values[0].inf.uintValue;
			index_ex->values[0] = PlnValue(i);
		} else if (index_ex->values[0].type == VL_LIT_FLO8) {
			int64_t i = index_ex->values[0].inf.floValue;
			index_ex->values[0] = PlnValue(i);
		}
	}
}

PlnArrayItem::PlnArrayItem(PlnExpression *array_ex, PlnExpression* index_ex, PlnVarType* item_type)
	: PlnExpression(ET_ARRAYITEM), array_ex(array_ex), index_ex(index_ex)
{
	values.push_back(getArrayVar(item_type, array_ex));
}

PlnArrayItem::~PlnArrayItem()
{
	if (values.size())
		delete values[0].inf.var;
	delete array_ex;
	delete index_ex;
}

void PlnArrayItem::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	auto item_var = values[0].inf.var;
	// PlnValue::getDataPlace may alloc dp.
	if (!item_var->place) {
		item_var->place = new PlnDataPlace(item_var->var_type->size(), item_var->var_type->data_type());
		item_var->place->comment = &item_var->name;
	}

	auto base_dp = da.prepareObjBasePtr();
	array_ex->data_places.push_back(base_dp);
	array_ex->finish(da, si);

	PlnDataPlace *index_dp;
	if (index_ex->type == ET_VALUE
		&& (index_ex->values[0].type == VL_LIT_INT8 || index_ex->values[0].type == VL_LIT_UINT8)) {
		index_dp = da.prepareObjIndexPtr(index_ex->values[0].inf.intValue);
		delete index_ex;
		index_ex = NULL;

	} else {
		index_dp = da.prepareObjIndexPtr();
	}

	auto item_dp = item_var->place;
	int need_to_mul = da.setIndirectObjDp(item_dp, base_dp, index_dp, 0);
//	BOOST_ASSERT(need_to_mul == 1);

	if (need_to_mul != 1) {
		auto oex = new PlnExpression(PlnValue((int64_t)need_to_mul));
		index_ex = PlnMulOperation::create(index_ex, oex);
	}

	if (index_ex) {
		index_ex->data_places.push_back(index_dp);
		index_ex->finish(da, si);
	}
	
	da.popSrc(base_dp);

	if (index_ex) {
		da.popSrc(index_dp);
	}
	
	if (data_places.size()) {
		if (item_dp->data_type == DT_OBJECT) {
			data_places[0]->load_address = true;
		}
		da.pushSrc(data_places[0], item_dp);
	}
}

void PlnArrayItem::gen(PlnGenerator& g)
{
	// for lval & rval
	PlnDataPlace* base_dp = array_ex->data_places[0];
	array_ex->gen(g);

	PlnDataPlace* index_dp = NULL;
	if (index_ex) {
		index_ex->gen(g);
		index_dp = index_ex->data_places[0];
	}

	g.genLoadDp(base_dp, false);
	if (index_dp) g.genLoadDp(index_dp, false);

	g.genSaveDp(base_dp);
	if (index_dp) g.genSaveDp(index_dp);

	// rval
	if (data_places.size()) {
		g.genSaveSrc(data_places[0]);
	}
}

vector<PlnExpression*> PlnArrayItem::getAllArrayItems(PlnVariable* var)
{
	BOOST_ASSERT(var->var_type->typeinf->type == TP_FIXED_ARRAY);
	vector<PlnExpression*> items;
	PlnFixedArrayVarType *atype = static_cast<PlnFixedArrayVarType*>(var->var_type);
	vector<int> &sizes = atype->sizes;
	int totalsize = 1;
	for (int i=0; i<sizes.size(); i++) {
		totalsize *= sizes[i];
	}

	for (uint64_t i=0; i<totalsize; i++) {
		PlnExpression *item_ex, *array_ex, *index_ex;
		array_ex = new PlnExpression(var);
		index_ex = new PlnExpression(i);
		item_ex = new PlnArrayItem(array_ex, index_ex, atype->item_type());
		items.push_back(item_ex);
	}

	return items;
}
