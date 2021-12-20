/// Object Array value class difinition.
///
/// @file	PlnArrayValue.cpp
/// @copyright	2019-2021 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include "../../PlnConstants.h"
#include "../PlnType.h"
#include "PlnArrayValue.h"
#include "../types/PlnArrayValueType.h"
#include "../types/PlnFixedArrayType.h"
#include "../types/PlnStructType.h"
#include "../../PlnDataAllocator.h"
#include "../../PlnGenerator.h"
#include "../../PlnMessage.h"
#include "../../PlnException.h"

PlnArrayValue::PlnArrayValue(vector<PlnExpression*> &exps, bool isLiteral)
	: PlnExpression(ET_ARRAYVALUE), item_exps(move(exps)), isLiteral(isLiteral)
{
	PlnValue aval;
	aval.type = VL_WORK;
	PlnType* atype = new PlnArrayValueType(this);
	atype->default_mode = "rni";
	aval.inf.wk_type = atype->getVarType();
	values.push_back(aval);
	doCopyFromStaticBuffer = false;

	// Exchange array lit -> array value
	for (auto& exp: item_exps) {
		if (!exp->values.size()) {
			PlnCompileError err(E_ValueRequired);
			err.loc = exp->loc;
			throw err;
		}
		if (exp->values[0].type == VL_LIT_ARRAY) {
			auto newexp = exp->values[0].inf.arrValue;
			exp->values[0].inf.arrValue = NULL;
			delete exp;
			exp = newexp;
		}
	}
}

PlnArrayValue::PlnArrayValue(const PlnArrayValue& src)
	: PlnExpression(ET_ARRAYVALUE)
{
	for (auto& exp: src.item_exps) {
		PlnExpression *new_exp;
		if (exp->type == ET_VALUE) {
			PlnValue v = exp->values[0];
			switch (v.type) {
				case VL_LIT_INT8:
					new_exp = new PlnExpression(v.inf.intValue);
					break;
				case VL_LIT_UINT8:
					new_exp = new PlnExpression(v.inf.uintValue);
					break;
				case VL_LIT_FLO8:
					new_exp = new PlnExpression(v.inf.floValue);
					break;
				case VL_LIT_STR:
					new_exp = new PlnExpression(*v.inf.strValue);
					break;
				default:
					BOOST_ASSERT(false);
			}

		} else if (exp->type == ET_ARRAYVALUE) {
			new_exp = new PlnArrayValue(*static_cast<PlnArrayValue*>(exp));

		} else
			BOOST_ASSERT(false);
		new_exp->loc = exp->loc;
		item_exps.push_back(new_exp);
	}

	PlnValue aval;
	aval.type = VL_WORK;
	PlnType* atype = new PlnArrayValueType(this);
	aval.inf.wk_type = atype->getVarType();
	isLiteral = src.isLiteral;
	doCopyFromStaticBuffer = src.doCopyFromStaticBuffer;
	values.push_back(aval);
}

bool PlnArrayValue::isFixedArray(const vector<PlnExpression*> &items, vector<int> &fixarr_sizes, int &item_type, int depth)
{
	if (depth >= fixarr_sizes.size()) {	// this is first element.
		fixarr_sizes.push_back(items.size());
		if (items[0]->type != ET_ARRAYVALUE) {
			item_type = items[0]->values[0].getVarType()->data_type();
			fixarr_sizes.push_back(0);
		}
	}

	if (fixarr_sizes[depth] != items.size()) {
		return false;
	}
	
	for (auto& item: items) {
		if (item->type == ET_ARRAYVALUE) {
			auto arr = static_cast<PlnArrayValue*>(item);
			if (!isFixedArray(arr->item_exps, fixarr_sizes, item_type, depth+1)) {
				return false;
			}

		} else {
			if (depth+2 != fixarr_sizes.size() || fixarr_sizes[depth+1]>0) {
				return false;
			}
			int dt = item->values[0].getVarType()->data_type();
			if (dt == DT_SINT) {
				if (item_type == DT_UINT)
					item_type = DT_SINT;

			} else if (dt == DT_UINT) {
				// Do nothing
			} else if (dt == DT_FLOAT) {
				if (item_type == DT_SINT || item_type == DT_UINT)
					item_type = DT_FLOAT;
			} else {
				// BOOST_ASSERT(false);
				BOOST_ASSERT(dt == DT_OBJECT_REF);
				item_type = DT_OBJECT_REF;
			}
		}
	}

	return true;
}

static void adjustFixedArrayType(PlnArrayValue* arr_val, PlnFixedArrayType* atype, int depth=0)
{
	BOOST_ASSERT(depth < atype->sizes.size());
	if (arr_val->item_exps.size() != atype->sizes[depth]) {
		PlnCompileError err(E_IncompatibleTypeAssign, PlnMessage::arrayValue(), atype->name);
		err.loc = arr_val->loc;
		throw err;
	}

	if (depth == atype->sizes.size()-1) { // check item
		vector<PlnVarType*> types = { atype->item_type };
		for (auto& exp: arr_val->item_exps) {
			exp = exp->adjustTypes(types);
		}

	} else {
		for (auto exp: arr_val->item_exps) {
			if (exp->type != ET_ARRAYVALUE) {
				PlnCompileError err(E_IncompatibleTypeAssign, PlnMessage::arrayValue(), atype->name);
				err.loc = arr_val->loc;
				throw err;
			}
			adjustFixedArrayType(static_cast<PlnArrayValue*>(exp),atype,depth+1);
		}
	}
}

static void adjustStructType(PlnArrayValue* arr_val, PlnStructType* stype)
{
	if (arr_val->item_exps.size() != stype->members.size()) {
		PlnCompileError err(E_IncompatibleTypeAssign, PlnMessage::arrayValue(), stype->name);
		err.loc = arr_val->loc;
		throw err;
	}

	int size = arr_val->item_exps.size();
	for (int i=0; i<size; ++i) {
		PlnVarType* t =  stype->members[i]->type;
		vector<PlnVarType*> types = { t };
		PlnExpression* &exp = arr_val->item_exps[i];
		exp = exp->adjustTypes(types);
	}
}

static bool isIncludesObjectRefMember(PlnVarType* item_type)
{
	if (item_type->typeinf->type == TP_FIXED_ARRAY) {
		PlnFixedArrayType *farr_type = static_cast<PlnFixedArrayType*>(item_type->typeinf);
		PlnVarType* sub_item_type = farr_type->item_type;
		
		if (sub_item_type->data_type() == DT_OBJECT_REF)
			return true;
		else if (sub_item_type->data_type() == DT_OBJECT)
			return isIncludesObjectRefMember(sub_item_type);

	} else if (item_type->typeinf->type == TP_STRUCT) {
		PlnStructType *strct_type = static_cast<PlnStructType*>(item_type->typeinf);
		for (auto member_def: strct_type->members) {
			if (member_def->type->data_type() == DT_OBJECT_REF) {
				return true;
			} else if (member_def->type->data_type() == DT_OBJECT) {
				return isIncludesObjectRefMember(member_def->type);
			}
		}

	} else {
		BOOST_ASSERT(false);
	}
	
	return false;
}

PlnExpression* PlnArrayValue::adjustTypes(const vector<PlnVarType*> &types)
{
	BOOST_ASSERT(types.size() == 1);
	PlnType* type = types[0]->typeinf;
	if (type->type == TP_FIXED_ARRAY)  {
		PlnFixedArrayType* atype = static_cast<PlnFixedArrayType*>(type);
		adjustFixedArrayType(this, atype);

		delete values[0].inf.wk_type->typeinf;	// PlnArrayValueType

		if (isLiteral && atype->item_type->data_type() != DT_OBJECT_REF) {
			if (atype->item_type->data_type() == DT_OBJECT) {
				// Check recursively if item_type includes DT_OBJECT_REF.
				if (isIncludesObjectRefMember(atype->item_type)) {
					values[0].inf.wk_type = types[0]->typeinf->getVarType("rni");

				} else {	// Dynamic allcation only object.
					doCopyFromStaticBuffer = true;
					values[0].inf.wk_type = types[0]->typeinf->getVarType("rir");
				}

			} else {	// item data_type == DT_INT .. DT_FLOAT
				doCopyFromStaticBuffer = true;
				values[0].inf.wk_type = types[0]->typeinf->getVarType("rir");
			}
		} else {
			values[0].inf.wk_type = types[0]->typeinf->getVarType("rni");
		}
		return this;

	} else if (type->type == TP_STRUCT) {
		PlnStructType* stype = static_cast<PlnStructType*>(type);
		adjustStructType(this, stype);

		delete values[0].inf.wk_type->typeinf;	// PlnArrayValueType

		if (isLiteral && !isIncludesObjectRefMember(types[0])) {
			doCopyFromStaticBuffer = true;
			values[0].inf.wk_type = types[0]->typeinf->getVarType("rir");
		} else {
			values[0].inf.wk_type = types[0]->typeinf->getVarType("rni");
		}

		return this;

	} else {
		PlnCompileError err(E_IncompatibleTypeAssign, PlnMessage::arrayValue(), type->name);
		err.loc = loc;
		throw err;
	}
}

template<typename T>
static void addAllNumerics(PlnArrayValue* arr_val, T& num_arr)
{
	for (auto& exp: arr_val->item_exps) {
		if (exp->type == ET_ARRAYVALUE) {
			addAllNumerics(static_cast<PlnArrayValue*>(exp), num_arr);
		} else if (exp->type == ET_VALUE) {
			PlnValue& val = exp->values[0];
			switch (val.type) {
				case VL_LIT_INT8:
					num_arr.push_back(val.inf.intValue); break;
				case VL_LIT_UINT8:
					num_arr.push_back(val.inf.uintValue); break;
				case VL_LIT_FLO8:
					num_arr.push_back(val.inf.floValue); break;
				default:
					BOOST_ASSERT(false);
			}
		} else
			BOOST_ASSERT(false);
	}
}

void PlnArrayValue::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
	// Assign each element
	if (doCopyFromStaticBuffer) {
		BOOST_ASSERT(data_places.size() == 1);
		PlnType* type = values[0].inf.wk_type->typeinf;

		PlnDataPlace* dp = NULL;
		if (type->type == TP_FIXED_ARRAY)
			dp = getROArrayDp(da);
		else if (type->type == TP_STRUCT)
			dp = getROStructDp(da);
		else 
			BOOST_ASSERT(false);

		da.pushSrc(data_places[0], dp);
		return;

	} else {
		// Assign each element
		for (auto exp: item_exps) {
			exp->finish(da, si);
			BOOST_ASSERT(!exp->data_places.size());
		}
	}
}

void PlnArrayValue::gen(PlnGenerator& g)
{
	if (!data_places.size())
		for (auto exp: item_exps)
			exp->gen(g);
}

static void pushArrayValItemExp(PlnArrayValue* arr_val, vector<int> &sizes, vector<PlnExpression*> &exps, int depth=0)
{
	BOOST_ASSERT(sizes.size()>depth);
	BOOST_ASSERT(sizes[depth] == arr_val->item_exps.size());

	if (depth == sizes.size()-1) {
		exps.insert(exps.end(), arr_val->item_exps.begin(), arr_val->item_exps.end());

	} else {
		for (PlnExpression* item: arr_val->item_exps) {
			BOOST_ASSERT(item->type == ET_ARRAYVALUE);
			pushArrayValItemExp(static_cast<PlnArrayValue*>(item), sizes, exps, depth+1);
			delete item;
		}

	}
	arr_val->item_exps.clear();
}

vector<PlnExpression*> PlnArrayValue::getAllItems()
{
	vector<PlnExpression*> items;
	if (values[0].inf.wk_type->typeinf->type == TP_FIXED_ARRAY) {
		PlnFixedArrayType *farr_type = static_cast<PlnFixedArrayType*>(values[0].inf.wk_type->typeinf);
		pushArrayValItemExp(this, farr_type->sizes, items);

	} else if (values[0].inf.wk_type->typeinf->type == TP_STRUCT) {
		PlnStructType *struct_type = static_cast<PlnStructType*>(values[0].inf.wk_type->typeinf);
		vector<int> sizes = { static_cast<int>(struct_type->members.size()) };
		pushArrayValItemExp(this, sizes, items);

	} else
		BOOST_ASSERT(false);

	return items;
}

static int getInt(PlnExpression *exp) {
	BOOST_ASSERT(exp->type == ET_VALUE);
	PlnValue &val = exp->values[0];
	if (val.type == VL_LIT_INT8 || val.type == VL_LIT_UINT8) {
		return val.inf.intValue;
	} else {
		PlnCompileError err(E_IncompatibleTypeAssign, PlnMessage::floatNumber(), PlnType::getSint()->name);
		err.loc = exp->loc;
		throw err;
	}
}

static double getFloat(PlnExpression *exp) {
	BOOST_ASSERT(exp->type == ET_VALUE);
	PlnValue &val = exp->values[0];
	if (val.type == VL_LIT_INT8 || val.type == VL_LIT_UINT8) {
		return val.inf.intValue;
	}
	BOOST_ASSERT(val.type == VL_LIT_FLO8);
	return val.inf.floValue;;
}


static void setROData(vector<PlnRoData>& rodata, PlnVarType* var_type, vector<PlnExpression *> &item_exps, int index, int align, int arr_dim = 1)
{
	if (var_type->typeinf->type == TP_FIXED_ARRAY) {
		BOOST_ASSERT(var_type->data_type() == DT_OBJECT || var_type->data_type() == DT_OBJECT_REF);
		BOOST_ASSERT(index == 0);

		PlnFixedArrayType* arr_type = static_cast<PlnFixedArrayType*>(var_type->typeinf);
		PlnVarType *item_type;
		if (arr_dim < arr_type->sizes.size()) {
			item_type = var_type;
			arr_dim++;
		} else {
			item_type = arr_type->item_type;
			arr_dim = 1;
		}

		for ( ; index < item_exps.size(); index++) {
			if (item_exps[index]->type == ET_ARRAYVALUE) {
				PlnArrayValue *exp = static_cast<PlnArrayValue*>(item_exps[index]);
				setROData(rodata, item_type, exp->item_exps, 0, item_type->align(), arr_dim);

			} else {
				setROData(rodata, item_type, item_exps, index, 0, arr_dim);
			}
		}
		
	} else if (var_type->typeinf->type == TP_STRUCT) {
		PlnStructType* stype = static_cast<PlnStructType*>(var_type->typeinf);
		bool first = true;
		for (auto member: stype->members) {
			int member_align = 0;
			if (first) {
				member_align = stype->data_align;
				first = false;
			}
			if (item_exps[index]->type == ET_ARRAYVALUE) {
				PlnArrayValue *exp = static_cast<PlnArrayValue*>(item_exps[index]);
				setROData(rodata, member->type, exp->item_exps, 0, member_align);
			} else {
				setROData(rodata, member->type, item_exps, index, member_align);
			}
			index++;
		}

	} else {
		PlnRoData datainf;
		datainf.data_type = var_type->data_type();
		datainf.size = var_type->size();
		datainf.alignment = align ? align : var_type->align();	//todo


		if (datainf.data_type == DT_SINT || datainf.data_type == DT_UINT) {
			datainf.val.i = getInt(item_exps[index]);
		} else if (datainf.data_type == DT_FLOAT) {
			datainf.val.f = getFloat(item_exps[index]);
		} else {
			BOOST_ASSERT(false);
		}
		rodata.push_back(datainf);
	}
}

PlnDataPlace* PlnArrayValue::getROArrayDp(PlnDataAllocator& da)
{
	BOOST_ASSERT(doCopyFromStaticBuffer);
	BOOST_ASSERT(values[0].inf.wk_type->typeinf->type == TP_FIXED_ARRAY);

	vector<PlnRoData> rodata;
	setROData(rodata, values[0].inf.wk_type, item_exps, 0, 8);

	return da.getRODataDp(rodata);

}


PlnDataPlace* PlnArrayValue::getROStructDp(PlnDataAllocator& da)
{
	BOOST_ASSERT(doCopyFromStaticBuffer);
	BOOST_ASSERT(values[0].inf.wk_type->typeinf->type == TP_STRUCT);

	vector<PlnRoData> rodata;
	setROData(rodata, values[0].inf.wk_type, item_exps, 0, 8);

	return da.getRODataDp(rodata);
}
