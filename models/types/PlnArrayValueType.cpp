/// Array value type class declaration.
///
/// @file	PlnArrayValueType.cpp
/// @copyright	2019 YAMAGUCHI Toshinobu 

#include "../PlnType.h"
#include "PlnArrayValueType.h"
#include "../PlnObjectLiteral.h"

PlnArrayValueType::PlnArrayValueType(PlnArrayLiteral* arr_lit)
	: PlnType(TP_ARRAY_VALUE), arr_lit(arr_lit)
{
}

PlnTypeConvCap PlnArrayValueType::canConvFrom(PlnType *src)
{
	return TC_CANT_CONV;
}

PlnTypeConvCap PlnArrayValueType::checkCompatible(PlnType* item_type, const vector<int>& sizes)
{
	vector<int> fixarr_sizes;
	PlnObjLitItemType lit_item_type;
	if (PlnArrayLiteral::isFixedArray(arr_lit->arr, fixarr_sizes, lit_item_type)) {
		fixarr_sizes.pop_back();
		if (sizes.size() != fixarr_sizes.size())
			return TC_CANT_CONV;
		
		for (int i=0; i<sizes.size(); i++) {
			if (sizes[i] != fixarr_sizes[i]) {
				return TC_CANT_CONV;
			}
		}

		return TC_AUTO_CAST;
	} else {
		return TC_CANT_CONV;
	}
}

