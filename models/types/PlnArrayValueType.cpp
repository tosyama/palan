/// Array value type class declaration.
///
/// @file	PlnArrayValueType.cpp
/// @copyright	2019 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include "../PlnType.h"
#include "../PlnModule.h"
#include "PlnArrayValueType.h"
#include "../PlnObjectLiteral.h"
#include "../expressions/PlnArrayValue.h"
#include "../../PlnConstants.h"

PlnArrayValueType::PlnArrayValueType(PlnArrayLiteral* arr_lit)
	: PlnType(TP_ARRAY_VALUE), arr_lit(arr_lit), arr_val(NULL)
{
}

PlnArrayValueType::PlnArrayValueType(PlnArrayValue* arr_val)
	: PlnType(TP_ARRAY_VALUE), arr_lit(NULL), arr_val(arr_val)
{
}

PlnType* PlnArrayValueType::getDefaultType(PlnModule *module)
{
	BOOST_ASSERT(arr_val);
	vector<int> fixarr_sizes;
	int val_item_type;
	if (PlnArrayValue::isFixedArray(arr_val->item_exps, fixarr_sizes, val_item_type)) {
		BOOST_ASSERT(fixarr_sizes.back() == 0);
		fixarr_sizes.pop_back();
		PlnType* itype;
		switch(val_item_type) {
			case DT_SINT:
				itype = PlnType::getSint(); break;
			case DT_UINT:
				itype = PlnType::getUint(); break;
			case DT_FLOAT:
				itype = PlnType::getFlo(); break;
			default:
				BOOST_ASSERT(false);
		}

		return module->getFixedArrayType(itype, fixarr_sizes);
	}
	BOOST_ASSERT(false);
}

PlnTypeConvCap PlnArrayValueType::canConvFrom(PlnType *src)
{
	BOOST_ASSERT(false);
	//  return TC_CANT_CONV;
}

static PlnTypeConvCap checkArrValCompati(PlnArrayValue* arr_val, PlnType* item_type, const vector<int>& sizes)
{
	vector<int> fixarr_sizes;
	int val_item_type;
	if (PlnArrayValue::isFixedArray(arr_val->item_exps, fixarr_sizes, val_item_type)) {
		fixarr_sizes.pop_back();
		if (sizes.size() != fixarr_sizes.size())
			return TC_CANT_CONV;
		
		for (int i=0; i<sizes.size(); i++) {
			if (sizes[i] != fixarr_sizes[i]) {
				return TC_CANT_CONV;
			}
		}

		PlnType *def_itype;
		switch (val_item_type) {
			case DT_SINT:
				def_itype = PlnType::getSint(); break;
			case DT_UINT:
				def_itype = PlnType::getUint(); break;
			case DT_FLOAT:
				def_itype = PlnType::getFlo(); break;
			defalut:
				BOOST_ASSERT(false);
		}

		return item_type->canConvFrom(def_itype);

	} else {
		return TC_CANT_CONV;
	}
}

static PlnTypeConvCap checkArrLitCompati(PlnArrayLiteral* arr_lit, PlnType* item_type, const vector<int>& sizes)
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
		
		PlnType *def_itype;
		switch (lit_item_type) {
			case OLI_SINT:
				def_itype = PlnType::getSint(); break;
			case OLI_UINT:
				def_itype = PlnType::getUint(); break;
			case OLI_FLO:
				def_itype = PlnType::getFlo(); break;
			defalut:
				BOOST_ASSERT(false);
		}

		return item_type->canConvFrom(def_itype);

	} else {
		return TC_CANT_CONV;
	}
}

PlnTypeConvCap PlnArrayValueType::checkCompatible(PlnType* item_type, const vector<int>& sizes)
{
	if (arr_val) {
		return checkArrValCompati(arr_val, item_type, sizes);

	} else if (arr_lit) {
		return checkArrLitCompati(arr_lit, item_type, sizes);

	} else {
		BOOST_ASSERT(false);
	}
}

