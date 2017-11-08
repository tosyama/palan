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
#include "../PlnVariable.h"
#include "../PlnType.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnConstants.h"

PlnArrayItem::PlnArrayItem(PlnExpression *array_ex, vector<int> item_ind)
	: PlnExpression(ET_ARRAYITEM), array_ex(array_ex)
{
	auto var = new PlnVariable();
	auto array_var = array_ex->values[0].inf.var;
	var->name = array_var->name + "[]";
	var->var_type = array_var->var_type;
	var->var_type.pop_back();
	values.push_back(PlnValue(var));

	index_ex = new PlnExpression(PlnValue(int64_t(item_ind[0])));
}


void PlnArrayItem::finish(PlnDataAllocator& da)
{
	auto base_dp = da.prepareObjBasePtr();
	array_ex->data_places.push_back(base_dp);
	array_ex->finish(da);
	da.allocDp(base_dp);

	auto index_dp = da.prepareObjIndexPtr();
	index_ex->data_places.push_back(index_dp);
	index_ex->finish(da);
	da.allocDp(index_dp);

	auto item_var = values[0].inf.var;
	auto item_dp = da.getIndirectObjDp(item_var->var_type.back()->size, DT_SINT, base_dp,index_dp);
	item_dp->comment = &item_var->name;
	values[0].inf.var->place = item_dp;
	PlnExpression::finish(da);
	
	da.release_stmt_end.push_back(item_dp);
	da.release_stmt_end.push_back(base_dp);
	da.release_stmt_end.push_back(index_dp);
}

void PlnArrayItem::gen(PlnGenerator& g)
{
	// for lval & rval
	array_ex->gen(g);
	index_ex->gen(g);

	// rval
	if (data_places.size()) {
		BOOST_ASSERT(false);
	}
}

