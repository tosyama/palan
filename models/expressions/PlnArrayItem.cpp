/// Array item model class definition.
///
/// PlnArrayItem returns item of array
/// that indicated by index.
/// e.g.) a[3]
///
/// @file	PlnArrayItem.cpp
/// @copyright	2017 YAMAGUCHI Toshinobu 

#include "boost/assert.hpp"
#include "PlnArrayItem.h"
#include "PlnMulOperation.h"
#include "PlnAddOperation.h"
#include "../PlnVariable.h"
#include "../PlnType.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnConstants.h"
#include "../../PlnGenerator.h"
#include "../../PlnConstants.h"

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

	if (item_ind.size() > (index+1)) {
		int next_offset = offset * arr_sizes[index];
		auto base_ex = getIndexExpression(index+1, next_offset, item_ind, arr_sizes);
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
	vector<PlnType*> arr_type)
	: PlnExpression(ET_ARRAYITEM), array_ex(array_ex)
{
	BOOST_ASSERT(item_ind.size());
	BOOST_ASSERT(array_ex->type == ET_VALUE);
	BOOST_ASSERT(array_ex->values[0].type == VL_VAR);

	auto var = new PlnVariable();
	auto array_var = array_ex->values[0].inf.var;
	var->name = array_var->name + "[]";
	var->var_type = arr_type;
	var->var_type.pop_back();
	// TODO: Move place init to finish.
	// Note: why here now? -> prepareAssignInf(NO_PTR case) will clash.
	var->place = new PlnDataPlace(var->var_type.back()->size, var->var_type.back()->data_type);
	var->place->comment = &var->name;
	if (var->var_type.back()->data_type == DT_OBJECT_REF) {
		var->ptr_type = PTR_REFERENCE | PTR_OWNERSHIP | PTR_INDIRECT_ACCESS;
	} else {
		var->ptr_type = NO_PTR;
	}
	var->container = array_var;

	values.push_back(PlnValue(var));

	auto arr_sizes = arr_type.back()->inf.fixedarray.sizes;
	BOOST_ASSERT(arr_sizes->size() == item_ind.size());
	index_ex = getIndexExpression(0,1,item_ind,*arr_sizes);
}

void PlnArrayItem::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	auto base_dp = da.prepareObjBasePtr();
	array_ex->data_places.push_back(base_dp);
	array_ex->finish(da, si);

	auto index_dp = da.prepareObjIndexPtr();
	index_ex->data_places.push_back(index_dp);
	index_ex->finish(da, si);

	da.allocDp(base_dp);
	da.popSrc(base_dp);
	da.allocDp(index_dp);
	da.popSrc(index_dp);

	auto item_var = values[0].inf.var;
	auto item_dp = values[0].inf.var->place;

	da.setIndirectObjDp(item_dp, base_dp, index_dp);

	PlnExpression::finish(da, si);	// pushSrc
	
	da.releaseData(base_dp);
	da.releaseData(index_dp);
	if (!data_places.size())
		da.releaseData(item_dp);
}

void PlnArrayItem::gen(PlnGenerator& g)
{
	// for lval & rval
	array_ex->gen(g);
	index_ex->gen(g);
	
	g.genLoadDp(array_ex->data_places[0]);
	g.genLoadDp(index_ex->data_places[0]);

	// rval
	if (data_places.size()) 
		g.genSaveSrc(data_places[0]);
}

