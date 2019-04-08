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
#include "../../PlnMessage.h"
#include "../../PlnException.h"

static PlnArrayValue* convertArrLit2Value(PlnArrayLiteral* arr_lit)
{
	for (auto& exp: arr_lit->exps) {
		if (exp->values[0].type == VL_LIT_ARRAY)
			BOOST_ASSERT(false);
		if (exp->values[0].type == VL_LIT_ARRAY2) {
			auto newexp = convertArrLit2Value(exp->values[0].inf.arrValue2);
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
			auto newexp = exp->values[0].inf.arrValue;
			exp->values[0].inf.arrValue = NULL;
			delete exp;
			exp = newexp;
		}
		if (exp->values[0].type == VL_LIT_ARRAY2) {
			auto newexp = convertArrLit2Value(exp->values[0].inf.arrValue2);
			delete exp;
			exp = newexp;
		}
	}
}

PlnArrayValue::PlnArrayValue(const PlnArrayValue& src)
	: PlnExpression(ET_ARRAYVALUE)
{
	for (auto& exp: src.item_exps) {
		PlnExpression *new_exp;
		if (exp->type == ET_VALUE) {
			PlnValue v = exp->values[0];
			switch (v.type) {
				case VL_LIT_INT8:
					new_exp = new PlnExpression(v.inf.intValue);
					break;
				case VL_LIT_UINT8:
					new_exp = new PlnExpression(v.inf.uintValue);
					break;
				case VL_LIT_FLO8:
					new_exp = new PlnExpression(v.inf.floValue);
					break;
				case VL_LIT_ARRAY:
					{
						PlnArrayValue *new_arr_val = new PlnArrayValue(*v.inf.arrValue);
						new_exp = new PlnExpression(PlnValue(new_arr_val));
						break;
					}
				default:
					BOOST_ASSERT(false);
			}

		} else if (exp->type == ET_ARRAYVALUE) {
			new_exp = new PlnArrayValue(*static_cast<PlnArrayValue*>(exp));

		} else
			BOOST_ASSERT(false);
		item_exps.push_back(new_exp);
	}

	PlnValue aval;
	aval.type = VL_WORK;
	aval.inf.wk_type = new PlnArrayValueType(this);
	values.push_back(aval);
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
				if (item_type == DT_SINT || item_type == DT_UINT)
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
	PlnType* type = types[0];
	if (type->type != TP_FIXED_ARRAY) {
		PlnCompileError err(E_IncompatibleTypeAssign, PlnMessage::arrayValue(), type->name);
		err.loc = loc;
		throw err;
	}
	PlnFixedArrayType* atype = static_cast<PlnFixedArrayType*>(type);
	if (atype->item_type->data_type == DT_OBJECT_REF) {
		PlnCompileError err(E_IncompatibleTypeAssign, PlnMessage::arrayValue(), type->name);
		throw err;
	}

	vector<int> fixarr_sizes;
	int val_item_type;
	if (PlnArrayValue::isFixedArray(item_exps, fixarr_sizes, val_item_type)) {
		BOOST_ASSERT(fixarr_sizes.back() == 0);
		fixarr_sizes.pop_back();
		if (type->type == TP_FIXED_ARRAY) {
			// check size
			vector<int> target_sizes = static_cast<PlnFixedArrayType*>(type)->sizes;
			bool isCompatible = true;
			if (target_sizes.size() == fixarr_sizes.size()) {
				for (int i=0; i<target_sizes.size(); i++) {
					if (target_sizes[i] != fixarr_sizes[i]) {
						isCompatible = false;
						break;
					}
				}

			} else
				isCompatible = false;

			if (!isCompatible) {
				PlnCompileError err(E_IncompatibleTypeAssign, PlnMessage::arrayValue(), type->name);
				err.loc = loc;
				throw err;
			}
		}

	} else {
		PlnCompileError err(E_IncompatibleTypeAssign, PlnMessage::arrayValue(), type->name);
		err.loc = loc;
		throw err;
	}

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
		PlnDataPlace *var_dp = data_places[0];
		int var_index = 0;
		auto arr_type = static_cast<PlnFixedArrayType*>(values[0].inf.wk_type);

		pushDp2ArrayVal(arr_type, 0, var_dp, var_index, this, da, si);
	}

	for (auto exp: item_exps) {
		exp->finish(da, si);
		if (exp->data_places.size()) {
			da.popSrc(exp->data_places.back());
			da.releaseDp(exp->data_places.back());
		}
	}
}

void PlnArrayValue::finishD(PlnDataAllocator& da, PlnScopeInfo& si)
{
//	for (auto item_dp: arr_item_dps) {
//		da.popSrc(item_dp);
//		da.releaseDp(item_dp);
//	}
}

void PlnArrayValue::gen(PlnGenerator& g)
{
	genS(g);
	genD(g);
}

void PlnArrayValue::genS(PlnGenerator& g)
{
	for (auto exp: item_exps) {
		exp->gen(g);
		if (exp->data_places.size())
			g.genLoadDp(exp->data_places.back());
	}
}

void PlnArrayValue::genD(PlnGenerator& g)
{
//	for (auto item_dp: arr_item_dps)
//		g.genLoadDp(item_dp);
}
