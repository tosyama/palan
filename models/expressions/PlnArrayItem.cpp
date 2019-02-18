/// Array item model class definition.
///
/// PlnArrayItem returns item of array
/// that indicated by index.
/// e.g.) a[3]
///
/// @file	PlnArrayItem.cpp
/// @copyright	2017-2019 YAMAGUCHI Toshinobu 

#include "boost/assert.hpp"
#include "PlnArrayItem.h"
#include "PlnMulOperation.h"
#include "PlnAddOperation.h"
#include "../PlnVariable.h"
#include "../PlnType.h"
#include "../types/PlnFixedArrayType.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnConstants.h"
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

PlnArrayItem::PlnArrayItem(PlnExpression *array_ex, vector<PlnExpression*> item_ind)
	: PlnArrayItem(array_ex, item_ind, array_ex->values[0].inf.var->var_type)
{
}

// Can be any array type.
PlnArrayItem::PlnArrayItem(PlnExpression *array_ex, vector<PlnExpression*> item_ind,
	PlnType* arr_type)
	: PlnExpression(ET_ARRAYITEM), array_ex(array_ex)
{
	BOOST_ASSERT(item_ind.size());
	BOOST_ASSERT(array_ex->values[0].type == VL_VAR);

	auto var = new PlnVariable();
	auto array_var = array_ex->values[0].inf.var;

	if (arr_type->type != TP_FIXED_ARRAY) {
		PlnCompileError err(E_CantUseIndexHere, array_var->name);
		throw err;
	}
	PlnFixedArrayType *farr_type = static_cast<PlnFixedArrayType*>(arr_type);

	var->name = array_var->name + "[]";
	var->var_type = farr_type->item_type;

	if (var->var_type->data_type == DT_OBJECT_REF) {
		var->ptr_type = PTR_REFERENCE | PTR_OWNERSHIP | PTR_INDIRECT_ACCESS;
	} else {
		var->ptr_type = NO_PTR | PTR_INDIRECT_ACCESS;
	}
	if (array_var->container)
		var->container = array_var->container;
	else
		var->container = array_var;

	values.push_back(PlnValue(var));

	auto arr_sizes = arr_type->inf.fixedarray.sizes;
	BOOST_ASSERT(arr_sizes->size() == item_ind.size());
	index_ex = getIndexExpression(item_ind.size()-1, 1, item_ind,*arr_sizes);
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
		item_var->place = new PlnDataPlace(item_var->var_type->size, item_var->var_type->data_type);
		item_var->place->comment = &item_var->name;
	}

	auto base_dp = da.prepareObjBasePtr();
	array_ex->data_places.push_back(base_dp);
	array_ex->finish(da, si);

	PlnDataPlace *index_dp;
	if (index_ex->type == ET_VALUE
		&& index_ex->values[0].type == VL_LIT_INT8) {
		index_dp = da.prepareObjIndexPtr(index_ex->values[0].inf.intValue);
		delete index_ex;
		index_ex = NULL;

	} else {
		index_dp = da.prepareObjIndexPtr();
		index_ex->data_places.push_back(index_dp);
		index_ex->finish(da, si);
	}

	auto item_dp = item_var->place;
	da.setIndirectObjDp(item_dp, base_dp, index_dp);
	
	if (data_places.size()) {
		da.pushSrc(data_places[0], item_dp);
	}
}

void PlnArrayItem::gen(PlnGenerator& g)
{
	// for lval & rval
	array_ex->gen(g);
	if (index_ex)
		index_ex->gen(g);
	
	// rval
	if (data_places.size()) {
		g.genSaveSrc(data_places[0]);
	}
}

