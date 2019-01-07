/// Array value model class definition.
///
/// PlnArrayValue returns array that litten directly to code.
/// e.g.) [1,2,3,4,5,6]
///
/// @file	PlnArrayValue.cpp
/// @copyright	2018-2019 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include "PlnArrayValue.h"
#include "../PlnType.h"
#include "../PlnModule.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnGenerator.h"
#include "../../PlnConstants.h"
#include "../../PlnMessage.h"
#include "../../PlnException.h"

PlnArrayValue::PlnArrayValue(vector<PlnExpression*> &elements)
	: PlnExpression(ET_ARRAYVALUE), elements(move(elements)),
		arrval_type(AVT_UNKNOWN)
{
	PlnValue val;
	val.type = VL_WORK;
	val.inf.wk_type = new vector<PlnType*>({PlnType::getRawArray()});
	values.push_back(val);
}

PlnArrayValue::PlnArrayValue(const PlnArrayValue& src)
	: PlnExpression(ET_ARRAYVALUE)
{
	values = src.values;
	arrval_type = src.arrval_type;

	for (PlnExpression* e: src.elements) {
		if (e->type == ET_VALUE) {
			elements.push_back(new PlnExpression(e->values[0]));
		} else if (e->type == ET_ARRAYVALUE) {
			elements.push_back(new PlnArrayValue(*static_cast<PlnArrayValue*>(e)));
		} else
			BOOST_ASSERT(false);
	}
}

bool PlnArrayValue::isLiteral()
{
	BOOST_ASSERT(arrval_type == AVT_UNKNOWN);
	for (PlnExpression* e: elements) {
		if (e->type == ET_VALUE) {
			PlnValType vtype = e->values[0].type;
			if (vtype != VL_LIT_INT8 && vtype != VL_LIT_UINT8 && vtype != VL_LIT_FLO8) {
				return false;
			}

		} else if (e->type == ET_ARRAYVALUE) {
			if (!static_cast<PlnArrayValue*>(e)->isLiteral()) {
				return false;
			}

		} else
			return false;
	}
	return true;
}

// true: success, false: NG
static bool addElements(vector<PlnExpression*> &dst, vector<int> sizes, vector<PlnExpression*> &src)
{
	if (sizes[0] != src.size())
		return false;

	if (sizes.size() >= 2) {
		sizes.erase(sizes.begin());
		for (auto e: src) {
			BOOST_ASSERT(e->type == ET_ARRAYVALUE);
			bool success = addElements(dst, sizes, static_cast<PlnArrayValue*>(e)->elements);
			if (!success)
				return false;
		}
	} else {
		for (auto e: src)
			dst.push_back(e);
	}
	return true;
}

// return ElementType
static PlnType* setFixedArrayInfo(PlnExpression* ex, vector<int>& sizes)
{
	if (ex->type == ET_VALUE) {
		if (ex->values[0].type == VL_LIT_INT8) {
			return PlnType::getSint();
		} else if (ex->values[0].type == VL_LIT_UINT8) {
			return PlnType::getUint();
		} else if (ex->values[0].type == VL_LIT_FLO8) {
			return PlnType::getFlo();
		} else
			BOOST_ASSERT(false);

	} else if (ex->type == ET_ARRAYVALUE) {
		PlnArrayValue* av = static_cast<PlnArrayValue*>(ex);
		sizes.push_back(av->elements.size());
		return setFixedArrayInfo(av->elements[0], sizes);
	} else
		BOOST_ASSERT(false);
}

void PlnArrayValue::setDefaultType(PlnModule* module)
{
	vector<int> sizes;
	PlnType* element_type = setFixedArrayInfo(this, sizes);

	vector<PlnType*> var_type = { element_type };
	PlnType* array_type = module->getFixedArrayType(var_type, sizes);
	var_type.push_back(array_type);
	
	BOOST_ASSERT(var_type.size() == 2);

	(*values[0].inf.wk_type) = move(var_type);
}

void PlnArrayValue::setVarType(vector<PlnType*> var_type)
{
	if (var_type.size() <= 1) {
		PlnCompileError err(E_IncompatibleTypeAssign, PlnMessage::arrayValue(), var_type.back()->name);
		err.loc = this->loc;
		throw err;
	}

	PlnType* t = var_type.back();

	if (t->obj_type == OT_FIXED_ARRAY) {
		vector<int> sizes = *t->inf.fixedarray.sizes;
		int total_num = sizes[0];
		for (int i=1; i<sizes.size(); i++) 
			total_num *= sizes[i];

		if (sizes.size() >= 2) {
			vector<PlnExpression*> exps;
			bool success = addElements(exps, sizes, elements);
			if (!success) {
				PlnCompileError err(E_IncompatibleTypeAssign, PlnMessage::arrayValue(), var_type.back()->name);
				err.loc = this->loc;
				throw err;
			}
			elements = move(exps);
		}
		if (total_num != elements.size()) {
			PlnCompileError err(E_IncompatibleTypeAssign, PlnMessage::arrayValue(), var_type.back()->name);
			err.loc = this->loc;
			throw err;
		}

	} else {
		BOOST_ASSERT(false);
	}

	element_type = var_type;
	element_type.pop_back();

	bool is_int = true;
	bool is_flo = true;
	for (auto e: elements) {
		if (e->type == ET_VALUE) {
			PlnValType vtype = e->values[0].type;
			is_int = is_int && (vtype == VL_LIT_INT8 || vtype == VL_LIT_UINT8);
			is_flo = is_flo && (vtype == VL_LIT_FLO8 || vtype == VL_LIT_INT8 || vtype == VL_LIT_UINT8);
		} else
			BOOST_ASSERT(false);
	}

	if (!is_int && !is_flo) {
		PlnCompileError err(E_CantUseDynamicValue, PlnMessage::arrayValue());
		err.loc = this->loc;
		throw err;
	}

	if (element_type.back()->data_type == DT_SINT
			|| element_type.back()->data_type == DT_UINT) {
		if (is_int) {
			arrval_type = AVT_INT_LIT_ARRAY;
		} else {
			PlnCompileError err(E_AllowedOnlyInteger);
			err.loc = this->loc;
			throw err;
		}

	} else if (element_type.back()->data_type == DT_FLOAT) {
		if (is_int || is_flo) {
			arrval_type = AVT_FLO_LIT_ARRAY;
		} else {
			BOOST_ASSERT(false);
		}
	} else {
		PlnCompileError err(E_IncompatibleTypeAssign, PlnMessage::arrayValue(), var_type.back()->name);
		err.loc = this->loc;
		throw err;
	}

	(*values[0].inf.wk_type) = var_type;
}

void PlnArrayValue::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	if (data_places.size()) {
		PlnDataPlace *dp;
		if (arrval_type == AVT_INT_LIT_ARRAY) {
			vector<int64_t> int_array;
			for (auto e: elements) {
				BOOST_ASSERT(e->type == ET_VALUE);
				PlnValue val = e->values[0];
				BOOST_ASSERT(val.type == VL_LIT_INT8 || val.type == VL_LIT_UINT8);
				int_array.push_back(e->values[0].inf.intValue);
			}
			dp = da.getROIntArrayDp(int_array, element_type.back()->size);

		} else if (arrval_type == AVT_FLO_LIT_ARRAY) {
			vector<double> flo_array;
			for (auto e: elements) {
				BOOST_ASSERT(e->type == ET_VALUE);
				PlnValue val = e->values[0];
				BOOST_ASSERT(val.type == VL_LIT_INT8 || val.type == VL_LIT_UINT8 || val.type == VL_LIT_FLO8);
				if (val.type == VL_LIT_INT8)
					flo_array.push_back(val.inf.intValue);
				else if (val.type == VL_LIT_UINT8)
					flo_array.push_back(val.inf.uintValue);
				else 
					flo_array.push_back(val.inf.floValue);
			}
			dp = da.getROFloArrayDp(flo_array, element_type.back()->size);

		} else {
			BOOST_ASSERT(false);
		}
	 	da.pushSrc(data_places[0], dp);
	}
}

void PlnArrayValue::gen(PlnGenerator& g)
{
}
