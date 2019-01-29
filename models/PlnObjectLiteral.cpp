/// Object literal class definitiins.
///
/// @file	PlnObjectLiteral.h
/// @copyright	2019 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include "../PlnModel.h"
#include "PlnType.h"
#include "PlnObjectLiteral.h"
#include "../PlnConstants.h"
#include "../PlnDataAllocator.h"

bool nmigrate = true;

// PlnObjectLiteralItem
PlnObjectLiteralItem::PlnObjectLiteralItem(const PlnObjectLiteralItem &src)
{
	type = src.type;
	if (type == OLI_ARRAY) {
		data.a = new PlnArrayLiteral(*src.data.a);
	} else {
		data.i = src.data.i;
	}
}

// PlnArrayLiteral
PlnArrayLiteral::PlnArrayLiteral(const PlnArrayLiteral& src)
{
	for (auto &i: src.arr)
		arr.push_back(i);
}

void PlnArrayLiteral::adjustTypes(const vector<vector<PlnType*>> &types, PlnModule &module)
{
	BOOST_ASSERT(types.size() == 1);
	arr_type = types[0];
}

PlnType* PlnArrayLiteral::getType()
{
	BOOST_ASSERT(arr_type.size());
	return arr_type.back();
}

static void addAllInts(const PlnArrayLiteral* arr_lit, vector<int64_t> &int_arr)
{
	for (auto& item: arr_lit->arr) {
		switch (item.type) {
			case OLI_ARRAY:
				addAllInts(item.data.a, int_arr); break;
			case OLI_SINT:
				int_arr.push_back(item.data.i); break;
			case OLI_UINT:
				int_arr.push_back(item.data.u); break;
			case OLI_FLO:
				int_arr.push_back(item.data.f); break;
			default:
				BOOST_ASSERT(false);
		}
	}
}

PlnDataPlace* PlnArrayLiteral::getDataPlace(PlnDataAllocator& da)
{
	if (arr_type[0]->data_type == DT_SINT) {
		vector<int64_t> int_arr;
		addAllInts(this, int_arr);
		return da.getROIntArrayDp(int_arr, arr_type[0]->size);	

	} else
		BOOST_ASSERT(false);
}
