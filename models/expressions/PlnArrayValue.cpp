/// Object Array value class difinition.
///
/// @file	PlnArrayValue.cpp
/// @copyright	2019 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include "../PlnType.h"
#include "PlnArrayValue.h"
#include "../types/PlnArrayValueType.h"
#include "../types/PlnFixedArrayType.h"
#include "../PlnObjectLiteral.h"
#include "../../PlnConstants.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnGenerator.h"

static PlnArrayValue* convertArrLit2Value(PlnArrayLiteral* arr_lit)
{
	for (auto& exp: arr_lit->exps) {
		if (exp->values[0].type == VL_LIT_ARRAY) {
			auto newexp = convertArrLit2Value(exp->values[0].inf.arrValue);
			delete exp;
			exp = newexp;
		}
	}
	return new PlnArrayValue(arr_lit->exps);
}

PlnArrayValue::PlnArrayValue(vector<PlnExpression*> &exps)
	: PlnExpression(ET_ARRAYVALUE), item_exps(move(exps))
{
	PlnValue aval;
	aval.type = VL_WORK;
	aval.inf.wk_type = new PlnArrayValueType(this);
	values.push_back(aval);

	// Exchange array lit -> array value
	for (auto& exp: item_exps) {
		if (exp->values[0].type == VL_LIT_ARRAY) {
			auto newexp = convertArrLit2Value(exp->values[0].inf.arrValue);
			delete exp;
			exp = newexp;
		}
	}
}

/// return true - items is aixed array, false - not fixed array
/// sizes - Detected array sizes. Note added 0 last. [2,3] is [2,3,0]
/// item_type - Detected array element type. 
/// depth - for internal process (recursive call)
bool PlnArrayValue::isFixedArray(const vector<PlnExpression*> &items, vector<int> &fixarr_sizes, int &item_type, int depth)
{
	if (depth >= fixarr_sizes.size()) {	// this is first element.
		fixarr_sizes.push_back(items.size());
		if (items[0]->type != ET_ARRAYVALUE) {
			item_type = items[0]->values[0].getType()->data_type;
			fixarr_sizes.push_back(0);
		}
	}

	if (fixarr_sizes[depth] != items.size()) {
		return false;
	}
	
	for (auto& item: items) {
		if (item->type == ET_ARRAYVALUE) {
			auto arr = static_cast<PlnArrayValue*>(item);
			if (!isFixedArray(arr->item_exps, fixarr_sizes, item_type, depth+1)) {
				return false;
			}

		} else {
			if (depth+2 != fixarr_sizes.size() || fixarr_sizes[depth+1]>0) {
				return false;
			}
			PlnType *it = item->values[0].getType();
			if (it->data_type == DT_SINT) {
				if (item_type == DT_UINT)
					item_type = DT_SINT;

			} else if (it->data_type == DT_UINT) {
				// Do nothing
			} else if (it->data_type == DT_FLOAT) {
				if (it->data_type == DT_SINT || it->data_type == DT_UINT)
					item_type = DT_FLOAT;
			} else {
				BOOST_ASSERT(false);
			}
		}
	}

	return true;
}

PlnExpression* PlnArrayValue::adjustTypes(const vector<PlnType*> &types)
{
	BOOST_ASSERT(types.size() == 1);
	delete values[0].inf.wk_type;

	values[0].inf.wk_type = types[0];

	return this;
}

static void pushDp2ArrayVal(PlnFixedArrayType* arr_type, int depth,
		PlnDataPlace* var_dp, int &var_index,  PlnArrayValue* arr_val,
		PlnDataAllocator& da, PlnScopeInfo& si) {

	BOOST_ASSERT(arr_type->sizes.size() > depth);
	BOOST_ASSERT(arr_type->sizes[depth] == arr_val->item_exps.size());

	if (arr_type->sizes.size()-1 == depth) {
		auto item_type = arr_type->item_type;
		static string cmt = "[]";
		for (auto exp: arr_val->item_exps) {
			PlnDataPlace *item_dp = new PlnDataPlace(item_type->size, item_type->data_type);

			auto base_dp = da.prepareObjBasePtr();
			auto index_dp = da.prepareObjIndexPtr(var_index);
			da.setIndirectObjDp(item_dp, base_dp, index_dp);

			da.pushSrc(base_dp, var_dp, false);
			item_dp->comment = &cmt;
			exp->data_places.push_back(item_dp);

			arr_val->arr_item_dps.push_back(item_dp);
			var_index++;
		}

	} else {
		for (auto exp: arr_val->item_exps) {
			BOOST_ASSERT(exp->type == ET_ARRAYVALUE);
			pushDp2ArrayVal(arr_type, depth+1, var_dp, var_index, static_cast<PlnArrayValue*>(exp), da, si);
		}
	}
}

void PlnArrayValue::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	finishS(da, si);
	finishD(da, si);	
}

void PlnArrayValue::finishS(PlnDataAllocator& da, PlnScopeInfo& si)
{
	if (data_places.size()) {
		BOOST_ASSERT(data_places.size() == 1);
	 	auto var_dp = data_places[0];
		int var_index = 0;
		auto arr_type = static_cast<PlnFixedArrayType*>(values[0].inf.wk_type);

		pushDp2ArrayVal(arr_type, 0, var_dp, var_index, this, da, si);
	}

	for (auto exp: item_exps)
		exp->finish(da, si);
}

void PlnArrayValue::finishD(PlnDataAllocator& da, PlnScopeInfo& si)
{
	for (auto item_dp: arr_item_dps) {
		da.popSrc(item_dp);
		da.releaseDp(item_dp);
	}
}

void PlnArrayValue::gen(PlnGenerator& g)
{
	genS(g);
	genD(g);
}

void PlnArrayValue::genS(PlnGenerator& g)
{
	for (auto exp: item_exps)
		exp->gen(g);
}

void PlnArrayValue::genD(PlnGenerator& g)
{
	for (auto item_dp: arr_item_dps)
		g.genLoadDp(item_dp);
}
