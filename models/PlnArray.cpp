/// Array model class definition.
///
/// @file	PlnArray.cpp
/// @copyright	2017 YAMAGUCHI Toshinobu 

#include "PlnArray.h"
#include "../PlnDataAllocator.h"

void PlnArray::finish(PlnDataAllocator& da)
{
	item_num_dp = da.getLiteralIntDp(ar_sizes[0]);
}
