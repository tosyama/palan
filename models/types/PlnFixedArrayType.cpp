/// Fixed array type class definition.
///
/// @file	PlnFixedArrayType.cpp
/// @copyright	2019 YAMAGUCHI Toshinobu 

#include "../PlnType.h"
#include "PlnFixedArrayType.h"

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

	return TC_CANT_CONV;
}
