/// Array value type class declaration.
///
/// @file	PlnArrayValueType.cpp
/// @copyright	2019-2021 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include "../../PlnConstants.h"
#include "../PlnType.h"
#include "../PlnBlock.h"
#include "PlnArrayValueType.h"
#include "../expressions/PlnArrayValue.h"
#include "../../PlnMessage.h"
#include "../../PlnException.h"

PlnArrayValueType::PlnArrayValueType(PlnArrayValue* arr_val)
	: PlnType(TP_ARRAY_VALUE), arr_val(arr_val)
{
	this->default_mode = "rir";
	this->name = PlnMessage::arrayValue();
}

PlnVarType* PlnArrayValueType::getDefaultType(PlnBlock *block)
{
	BOOST_ASSERT(arr_val);
	vector<int> fixarr_sizes;
	int val_item_type;
	if (PlnArrayValue::isFixedArray(arr_val->item_exps, fixarr_sizes, val_item_type)) {
		BOOST_ASSERT(fixarr_sizes.back() == 0);
		fixarr_sizes.pop_back();
		PlnVarType* itype;
		switch(val_item_type) {
			case DT_SINT:
				itype = PlnType::getSint()->getVarType(); break;
			case DT_UINT:
				itype = PlnType::getUint()->getVarType(); break;
			case DT_FLOAT:
				itype = PlnType::getFlo64()->getVarType(); break;
			default:
				BOOST_ASSERT(false);
		}

		return block->getFixedArrayType(itype, fixarr_sizes, "---");
	}

	PlnCompileError err(E_UnsupportedGrammer, "use only fixed array here.");
	throw err;
}

// LCOV_EXCL_START
PlnTypeConvCap PlnArrayValueType::canCopyFrom(const string& mode, PlnVarType *src, PlnAsgnType copymode) {
	BOOST_ASSERT(false);
}
// LCOV_EXCL_STOP

static PlnTypeConvCap checkFixedArrayItemTypes(PlnArrayValue* arr_val, PlnVarType* item_type, const vector<int>& sizes, int depth)
{
	int items_num = sizes[depth];
	if (arr_val->item_exps.size() != items_num)
		return TC_CANT_CONV;

	PlnTypeConvCap result  = TC_SAME;
	if (sizes.size() == (depth+1) ) {
		// check item type conpatible 
		for (auto exp: arr_val->item_exps) {
			result = PlnType::lowCapacity(result, item_type->canCopyFrom(exp->values[0].getVarType(), ASGN_COPY));
			if (result == TC_CANT_CONV)
				return TC_CANT_CONV;
		}
		return result;

	} else { 
		for (auto exp: arr_val->item_exps) {
			if (exp->type != ET_ARRAYVALUE)
				return TC_CANT_CONV;

			PlnArrayValue* item_arr_val = static_cast<PlnArrayValue*>(exp);
			result = PlnType::lowCapacity(result, checkFixedArrayItemTypes(item_arr_val, item_type, sizes, depth+1));
			if (result == TC_CANT_CONV)
				return TC_CANT_CONV;
		}

		return result;
	}
}

PlnTypeConvCap PlnArrayValueType::checkCompatible(PlnVarType* item_type, const vector<int>& sizes)
{
	BOOST_ASSERT(arr_val);
	return checkFixedArrayItemTypes(arr_val, item_type, sizes, 0);
}

vector<int> PlnArrayValueType::getArraySizes()
{
	BOOST_ASSERT(arr_val);
	vector<int> fixarr_sizes;
	int item_type;
	if (PlnArrayValue::isFixedArray(arr_val->item_exps, fixarr_sizes, item_type)) {
		BOOST_ASSERT(fixarr_sizes.back() == 0);
		fixarr_sizes.pop_back();

		return fixarr_sizes;
	}
	BOOST_ASSERT(false);
}	// LCOV_EXCL_LINE
