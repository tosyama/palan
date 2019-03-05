/// Object literal class definitiins.
///
/// PlnArrayLiteral returns array that written directly to code.
/// e.g.) [1,2,3,4,5,6]
//
/// @file	PlnObjectLiteral.h
/// @copyright	2019 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include "PlnType.h"
#include "PlnModule.h"
#include "PlnObjectLiteral.h"
#include "../PlnConstants.h"
#include "../PlnDataAllocator.h"
#include "../PlnMessage.h"
#include "../PlnException.h"
#include "types/PlnFixedArrayType.h"
#include "types/PlnArrayValueType.h"

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
PlnArrayLiteral::PlnArrayLiteral(vector<PlnExpression*> &exps)
{
	for (PlnExpression *exp: exps) {
		BOOST_ASSERT(exp->type == ET_VALUE);
		PlnValue v = exp->values[0];
		switch (v.type) {
		case VL_LIT_INT8:
			arr.push_back(v.inf.intValue);
			delete exp;
			break;
		case VL_LIT_UINT8:
			arr.push_back(v.inf.uintValue);
			delete exp;
			break;
		case VL_LIT_FLO8:
			arr.push_back(v.inf.floValue);
			delete exp;
			break;
		case VL_LIT_ARRAY:
			arr.push_back(v.inf.arrValue);
			v.inf.arrValue = NULL;
			delete exp;
			break;
		default:
			BOOST_ASSERT(false);
		}
	}

	arr_type = new PlnArrayValueType(this);
}

PlnArrayLiteral::PlnArrayLiteral(const PlnArrayLiteral& src)
{
	for (auto &i: src.arr)
		arr.push_back(i);
	if (src.arr_type->type == TP_ARRAY_VALUE) {
		arr_type = new PlnArrayValueType(this);
	} else {
		arr_type = src.arr_type;
	}
}

PlnArrayLiteral::~PlnArrayLiteral()
{
	if (arr_type->type == TP_ARRAY_VALUE) {
		BOOST_ASSERT(static_cast<PlnArrayValueType*>(arr_type)->arr_lit == this);
		delete arr_type;
	}
}

bool PlnArrayLiteral::isFixedArray(const vector<PlnObjectLiteralItem> &items, vector<int> &fixarr_sizes, PlnObjLitItemType &item_type, int depth)
{
	if (depth >= fixarr_sizes.size()) {	// this is first element.
		fixarr_sizes.push_back(items.size());
		if (items[0].type != OLI_ARRAY) {
			item_type = items[0].type;
			fixarr_sizes.push_back(0);
		}
	}

	if (fixarr_sizes[depth] != items.size()) {
		return false;
	}
	
	for (auto& item: items) {
		if (item.type == OLI_ARRAY) {
			if (!isFixedArray(item.data.a->arr, fixarr_sizes, item_type, depth+1)) {
				return false;
			}

		} else {
			if (depth+2 != fixarr_sizes.size() || fixarr_sizes[depth+1]>0) {
				return false;
			}
			switch (item.type) {
				case OLI_SINT:
					if (item_type == OLI_UINT)
						item_type = OLI_SINT;
					break;
				case OLI_UINT:
					break;
				case OLI_FLO:
					if (item_type == OLI_SINT || item_type == OLI_UINT)
						item_type = OLI_FLO;
					break;
				defalut:
					BOOST_ASSERT(false);
			}
		}
	}

	return true;
}

void PlnArrayLiteral::adjustTypes(const vector<PlnType*> &types)
{
	BOOST_ASSERT(types.size() == 1);
	PlnType* type = types[0];
	if (type->type != TP_FIXED_ARRAY) {
		PlnCompileError err(E_IncompatibleTypeAssign, PlnMessage::arrayValue(), type->name);
		throw err;
	}
	PlnFixedArrayType* atype = static_cast<PlnFixedArrayType*>(type);
	if (atype->item_type->data_type == DT_OBJECT_REF) {
		PlnCompileError err(E_IncompatibleTypeAssign, PlnMessage::arrayValue(), type->name);
		throw err;
	}

	vector<int> fixarr_sizes;
	PlnObjLitItemType lit_item_type;
	if (isFixedArray(arr, fixarr_sizes, lit_item_type, 0)) {
		fixarr_sizes.pop_back();
		if (type->obj_type == OT_FIXED_ARRAY) {
			// check size
			vector<int> target_sizes = *(type->inf.fixedarray.sizes);
			bool isCompatible = true;
			if (target_sizes.size() == fixarr_sizes.size()) {
				for (int i=0; i<target_sizes.size(); i++) {
					if (target_sizes[i] != fixarr_sizes[i]) {
						isCompatible = false;
						break;
					}
				}

			} else
				isCompatible = false;

			if (!isCompatible) {
				PlnCompileError err(E_IncompatibleTypeAssign, PlnMessage::arrayValue(), type->name);
				throw err;
			}

			// check float
			if (static_cast<PlnFixedArrayType*>(type)->item_type->data_type != DT_FLOAT && lit_item_type == OLI_FLO) {
				PlnCompileError err(E_AllowedOnlyInteger);
				throw err;
			}
		}
	} else {
		PlnCompileError err(E_IncompatibleTypeAssign, PlnMessage::arrayValue(), type->name);
		throw err;
	}
	arr_type = types[0];
}

PlnType* PlnArrayLiteral::getDefaultType(PlnModule *module)
{
	BOOST_ASSERT(arr.size());
	BOOST_ASSERT(arr_type->type==TP_ARRAY_VALUE);

	vector<int> fixarr_sizes;
	PlnObjLitItemType item_type;
	if (isFixedArray(arr, fixarr_sizes, item_type)) {
		BOOST_ASSERT(fixarr_sizes.back() == 0);
		fixarr_sizes.pop_back();

		PlnType* itype;
		switch(item_type) {
			case OLI_SINT:
				itype = PlnType::getSint(); break;
			case OLI_UINT:
				itype = PlnType::getUint(); break;
			case OLI_FLO:
				itype = PlnType::getFlo(); break;
			default:
				BOOST_ASSERT(false);
		}

		return module->getFixedArrayType(itype, fixarr_sizes);
	}

	PlnCompileError err(E_UnsuppotedGrammer, "use only fixed array here.");
	throw err;
}

vector<int> PlnArrayLiteral::getArraySizes()
{
	vector<int> fixarr_sizes;
	PlnObjLitItemType item_type;
	if (isFixedArray(arr, fixarr_sizes, item_type, 0)) {
		BOOST_ASSERT(fixarr_sizes.back() == 0);
		fixarr_sizes.pop_back();
		return fixarr_sizes;
	}
	BOOST_ASSERT(false);
	// fixarr_sizes.resize(1);
	// fixarr_sizes[0] = arr.size();
	// return fixarr_sizes;
}

PlnType* PlnArrayLiteral::getType()
{
	BOOST_ASSERT(arr_type);
	return arr_type;
}

template<typename T>
static void addAllNumerics(const PlnArrayLiteral* arr_lit, T& num_arr)
{
	for (auto& item: arr_lit->arr) {
		switch (item.type) {
			case OLI_ARRAY:
				addAllNumerics(item.data.a, num_arr); break;
			case OLI_SINT:
				num_arr.push_back(item.data.i); break;
			case OLI_UINT:
				num_arr.push_back(item.data.u); break;
			case OLI_FLO:
				num_arr.push_back(item.data.f); break;
			default:
				BOOST_ASSERT(false);
		}
	}
}

PlnDataPlace* PlnArrayLiteral::getDataPlace(PlnDataAllocator& da)
{
	PlnType* item_type = static_cast<PlnFixedArrayType*>(arr_type)->item_type;
	int ele_type = item_type->data_type;
	int ele_size = item_type->size;

	if (ele_type == DT_SINT || ele_type == DT_UINT) {
		vector<int64_t> int_arr;
		addAllNumerics(this, int_arr);
		return da.getROIntArrayDp(int_arr, ele_size);	

	} else if (ele_type == DT_FLOAT) {
		vector<double> flo_arr;
		addAllNumerics(this, flo_arr);
		return da.getROFloArrayDp(flo_arr, ele_size);	
	} else
		BOOST_ASSERT(false);
}
