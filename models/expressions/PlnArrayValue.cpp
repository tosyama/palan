/// Array value model class definition.
///
/// PlnArrayValue returns array that litten directly to code.
/// e.g.) [1,2,3,4,5,6]
///
/// @file	PlnArrayValue.cpp
/// @copyright	2018 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include "PlnArrayValue.h"
#include "../PlnType.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnGenerator.h"
#include "../../PlnConstants.h"

PlnArrayValue::PlnArrayValue(vector<PlnExpression*> &elements)
	: PlnExpression(ET_ARRAYVALUE), elements(move(elements)),
		arrval_type(AVT_OBJ_ARRAY)
{
	PlnValue val;
	val.type = VL_WORK;
	val.inf.wk_type = new vector<PlnType*>({PlnType::getRawArray()});
	values.push_back(val);
}

static void addElements(vector<PlnExpression*> &dst, vector<int> sizes, vector<PlnExpression*> &src)
{
	BOOST_ASSERT(sizes[0] == src.size());
	if (sizes.size() >= 2) {
		sizes.erase(sizes.begin());
		for (auto e: src) {
			BOOST_ASSERT(e->type == ET_ARRAYVALUE);
			addElements(dst, sizes, static_cast<PlnArrayValue*>(e)->elements);
		}
	} else {
		for (auto e: src)
			dst.push_back(e);
	}
}

void PlnArrayValue::setVarType(vector<PlnType*> var_type)
{
	BOOST_ASSERT(var_type.size()>=2);
	PlnType* t = var_type.back();

	if (t->obj_type == OT_FIXED_ARRAY) {
		vector<int> sizes = *t->inf.fixedarray.sizes;
		int total_num = sizes[0];
		for (int i=1; i<sizes.size(); i++) 
			total_num *= sizes[i];

		if (sizes.size() >= 2) {
			vector<PlnExpression*> exps;
			addElements(exps, sizes, elements);
			elements = move(exps);
		}
		BOOST_ASSERT(total_num == elements.size());

	} else {
		BOOST_ASSERT(false);
	}

	element_type = var_type;
	element_type.pop_back();

	bool is_int = true;
	bool is_flo = true;
	for (auto e: elements) {
		if (e->type == ET_ARRAYVALUE) {
			setVarType(element_type);
		}
		if (e->type == ET_VALUE) {
			PlnValType vtype = e->values[0].type;
			is_int = is_int && (vtype == VL_LIT_INT8 || vtype == VL_LIT_UINT8);
			is_flo = is_flo && (vtype == VL_LIT_FLO8 || vtype == VL_LIT_INT8 || vtype == VL_LIT_UINT8);
		}
	}

	if (element_type.back()->data_type == DT_SINT
			|| element_type.back()->data_type == DT_UINT) {
		if (is_int) {
			arrval_type = AVT_INT_LIT_ARRAY;
		} else if (is_flo) {
			BOOST_ASSERT(false);
		} else {
			BOOST_ASSERT(false);
		}
	}
}

void PlnArrayValue::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	if (data_places.size()) {
		PlnDataPlace *dp;
		if (arrval_type == AVT_INT_LIT_ARRAY) {
			vector<int64_t> int_array;
			for (auto e: elements) {
				BOOST_ASSERT(e->type == ET_VALUE && e->values[0].type == VL_LIT_INT8);
				int_array.push_back(e->values[0].inf.intValue);
			}
			dp = da.getROIntArrayDp(int_array, element_type.back()->size);
		} else {
			BOOST_ASSERT(false);
		}
	 	da.pushSrc(data_places[0], dp);
	}
}

void PlnArrayValue::gen(PlnGenerator& g)
{
}
