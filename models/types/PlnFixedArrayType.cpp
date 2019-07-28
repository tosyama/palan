/// Fixed array type class definition.
///
/// @file	PlnFixedArrayType.cpp
/// @copyright	2019 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include "../../PlnConstants.h"
#include "../PlnType.h"
#include "PlnFixedArrayType.h"
#include "PlnArrayValueType.h"

PlnFixedArrayType::PlnFixedArrayType(string &name, PlnType* item_type, vector<int>& sizes)
	: PlnType(TP_FIXED_ARRAY), item_type(item_type)
{
	int alloc_size = item_type->size;
	for (int s: sizes)
		alloc_size *= s;

	this->name = name;
	this->data_type = DT_OBJECT_REF;
	this->size = 8;
	this->inf.obj.is_fixed_size = true;
	this->inf.obj.alloc_size = alloc_size;
	this->sizes = move(sizes);
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
			else // byte[?]
				return TC_AUTO_CAST;

		}
	}

	return TC_CANT_CONV;
}
