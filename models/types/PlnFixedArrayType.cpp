/// Fixed array type class definition.
///
/// @file	PlnFixedArrayType.cpp
/// @copyright	2019 YAMAGUCHI Toshinobu 

#include "../PlnType.h"
#include "PlnFixedArrayType.h"
#include "PlnArrayValueType.h"

PlnFixedArrayType::PlnFixedArrayType() : PlnType(TP_FIXED_ARRAY)
{
}

PlnTypeConvCap PlnFixedArrayType::canConvFrom(PlnType *src)
{
	if (this == src)
		return TC_SAME;

	if (src == PlnType::getObject()) {
		return TC_DOWN_CAST;
	}

	if (src->type == TP_FIXED_ARRAY) {
		auto src_farr = static_cast<PlnFixedArrayType*>(src);
		if (item_type == src_farr->item_type) {
			if (!sizes[0]) {
				return TC_AUTO_CAST;
			} else if (!src_farr->sizes[0]) {
				return TC_DOWN_CAST;
			}
		}
	}

	if (src->type == TP_ARRAY_VALUE) {
		return static_cast<PlnArrayValueType*>(src)->checkCompatible(item_type, sizes);
	}

	if (src == PlnType::getReadOnlyCStr()) {
		if (item_type == PlnType::getByte() && sizes.size() == 1) {
			if (sizes[0])
				return TC_LOSTABLE_AUTO_CAST;
			else
				return TC_AUTO_CAST;

		}
	}

	return TC_CANT_CONV;
}
