/// Type model class definition.
///
/// @file	PlnType.cpp
/// @copyright	2017-2021 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include "../PlnConstants.h"
#include "PlnType.h"
#include "types/PlnFixedArrayType.h"
#include "types/PlnStructType.h"
#include "PlnVariable.h"
#include "PlnExpression.h"

using namespace std;

// Basic types
static bool is_initialzed_type = false;
static vector<PlnTypeInfo*> basic_types;
static PlnTypeInfo* byte_type = NULL;
static PlnTypeInfo* int_type = NULL;
static PlnTypeInfo* uint_type = NULL;
static PlnTypeInfo* flo32_type = NULL;
static PlnTypeInfo* flo64_type = NULL;
static PlnTypeInfo* ro_cstr_type = NULL;
static PlnTypeInfo* object_type = NULL;
static PlnTypeInfo* any_type = NULL;

// PlnType
PlnTypeInfo::PlnTypeInfo(PlnTypeType type)
	: type(type)
{
}

PlnTypeInfo::~PlnTypeInfo()
{
}

void PlnTypeInfo::initBasicTypes()
{
	if (is_initialzed_type)
		return;

	is_initialzed_type = true;

	PlnTypeInfo* sbt = new PlnTypeInfo();
	sbt->name = "sbyte";
	sbt->default_mode = "wis";
	sbt->data_type = DT_SINT;
	sbt->data_size = 1;
	sbt->data_align = 1;
	basic_types.push_back(sbt);

	PlnTypeInfo* bt = new PlnTypeInfo();
	bt->name = "byte";
	bt->default_mode = "wis";
	bt->data_type = DT_UINT;
	bt->data_size = 1;
	bt->data_align = 1;
	basic_types.push_back(bt);
	byte_type = bt;

	PlnTypeInfo* i16t = new PlnTypeInfo();
	i16t->name = "int16";
	i16t->default_mode = "wis";
	i16t->data_type = DT_SINT;
	i16t->data_size = 2;
	i16t->data_align = 2;
	basic_types.push_back(i16t);

	PlnTypeInfo* u16t = new PlnTypeInfo();
	u16t->name = "uint16";
	u16t->default_mode = "wis";
	u16t->data_type = DT_UINT;
	u16t->data_size = 2;
	u16t->data_align = 2;
	basic_types.push_back(u16t);
	
	PlnTypeInfo* i32t = new PlnTypeInfo();
	i32t->name = "int32";
	i32t->default_mode = "wis";
	i32t->data_type = DT_SINT;
	i32t->data_size = 4;
	i32t->data_align = 4;
	basic_types.push_back(i32t);

	PlnTypeInfo* u32t = new PlnTypeInfo();
	u32t->name = "uint32";
	u32t->default_mode = "wis";
	u32t->data_type = DT_UINT;
	u32t->data_size = 4;
	u32t->data_align = 4;
	basic_types.push_back(u32t);

	PlnTypeInfo* i64t = new PlnTypeInfo();
	i64t->name = "int64";
	i64t->default_mode = "wis";
	i64t->data_type = DT_SINT;
	i64t->data_size = 8;
	i64t->data_align = 8;
	basic_types.push_back(i64t);
	int_type = i64t;

	PlnTypeInfo* u64t = new PlnTypeInfo();
	u64t->name = "uint64";
	u64t->default_mode = "wis";
	u64t->data_type = DT_UINT;
	u64t->data_size = 8;
	u64t->data_align = 8;
	basic_types.push_back(u64t);
	uint_type = u64t;

	PlnTypeInfo* f32t = new PlnTypeInfo();
	f32t->name = "flo32";
	f32t->default_mode = "wis";
	f32t->data_type = DT_FLOAT;
	f32t->data_size = 4;
	f32t->data_align = 4;
	basic_types.push_back(f32t);
	flo32_type = f32t;

	PlnTypeInfo* f64t = new PlnTypeInfo();
	f64t->name = "flo64";
	f64t->default_mode = "wis";
	f64t->data_type = DT_FLOAT;
	f64t->data_size = 8;
	f64t->data_align = 8;
	basic_types.push_back(f64t);
	flo64_type = f64t;

	PlnTypeInfo* t = new PlnTypeInfo();
	t->name = "_ro_cstr";
	t->default_mode = "rir";
	t->data_type = DT_OBJECT;
	t->data_size = 0;
	t->data_align = 1;
	basic_types.push_back(t);
	ro_cstr_type = t;

	t = new PlnTypeInfo();
	t->name = "object";
	t->default_mode = "wmr";
	t->data_type = DT_OBJECT;
	t->data_size = 0;
	t->data_align = 8;
	basic_types.push_back(t);
	object_type = t;

	t = new PlnTypeInfo();
	t->name = "";
	t->default_mode = "wmi";
	t->data_type = DT_UNKNOWN;
	t->data_size = 0;
	t->data_align = 8;
	basic_types.push_back(t);
	any_type = t;

	// Set type conversion info.
	// sbyte
	sbt->conv_inf.emplace_back(bt, TC_LOSTABLE_AUTO_CAST);
	sbt->conv_inf.emplace_back(u16t, TC_LOSTABLE_AUTO_CAST);
	sbt->conv_inf.emplace_back(u32t, TC_LOSTABLE_AUTO_CAST);
	sbt->conv_inf.emplace_back(u64t, TC_LOSTABLE_AUTO_CAST);
	sbt->conv_inf.emplace_back(i16t, TC_LOSTABLE_AUTO_CAST);
	sbt->conv_inf.emplace_back(i32t, TC_LOSTABLE_AUTO_CAST);
	sbt->conv_inf.emplace_back(i64t, TC_LOSTABLE_AUTO_CAST);
	sbt->conv_inf.emplace_back(f32t, TC_LOSTABLE_AUTO_CAST);
	sbt->conv_inf.emplace_back(f64t, TC_LOSTABLE_AUTO_CAST);

	// byte
	bt->conv_inf.emplace_back(u16t, TC_LOSTABLE_AUTO_CAST);
	bt->conv_inf.emplace_back(u32t, TC_LOSTABLE_AUTO_CAST);
	bt->conv_inf.emplace_back(u64t, TC_LOSTABLE_AUTO_CAST);
	bt->conv_inf.emplace_back(sbt, TC_LOSTABLE_AUTO_CAST);
	bt->conv_inf.emplace_back(i16t, TC_LOSTABLE_AUTO_CAST);
	bt->conv_inf.emplace_back(i32t, TC_LOSTABLE_AUTO_CAST);
	bt->conv_inf.emplace_back(i64t, TC_LOSTABLE_AUTO_CAST);
	bt->conv_inf.emplace_back(f32t, TC_LOSTABLE_AUTO_CAST);
	bt->conv_inf.emplace_back(f64t, TC_LOSTABLE_AUTO_CAST);

	// int16
	i16t->conv_inf.emplace_back(bt, TC_AUTO_CAST);
	i16t->conv_inf.emplace_back(u16t, TC_LOSTABLE_AUTO_CAST);
	i16t->conv_inf.emplace_back(u32t, TC_LOSTABLE_AUTO_CAST);
	i16t->conv_inf.emplace_back(u64t, TC_LOSTABLE_AUTO_CAST);
	i16t->conv_inf.emplace_back(sbt, TC_AUTO_CAST);
	i16t->conv_inf.emplace_back(i32t, TC_LOSTABLE_AUTO_CAST);
	i16t->conv_inf.emplace_back(i64t, TC_LOSTABLE_AUTO_CAST);
	i16t->conv_inf.emplace_back(f32t, TC_LOSTABLE_AUTO_CAST);
	i16t->conv_inf.emplace_back(f64t, TC_LOSTABLE_AUTO_CAST);

	// uint16
	u16t->conv_inf.emplace_back(bt, TC_AUTO_CAST);
	u16t->conv_inf.emplace_back(u32t, TC_LOSTABLE_AUTO_CAST);
	u16t->conv_inf.emplace_back(u64t, TC_LOSTABLE_AUTO_CAST);
	u16t->conv_inf.emplace_back(sbt, TC_LOSTABLE_AUTO_CAST);
	u16t->conv_inf.emplace_back(i16t, TC_LOSTABLE_AUTO_CAST);
	u16t->conv_inf.emplace_back(i32t, TC_LOSTABLE_AUTO_CAST);
	u16t->conv_inf.emplace_back(i64t, TC_LOSTABLE_AUTO_CAST);
	u16t->conv_inf.emplace_back(f32t, TC_LOSTABLE_AUTO_CAST);
	u16t->conv_inf.emplace_back(f64t, TC_LOSTABLE_AUTO_CAST);

	// int32
	i32t->conv_inf.emplace_back(bt, TC_AUTO_CAST);
	i32t->conv_inf.emplace_back(u16t, TC_AUTO_CAST);
	i32t->conv_inf.emplace_back(u32t, TC_LOSTABLE_AUTO_CAST);
	i32t->conv_inf.emplace_back(u64t, TC_LOSTABLE_AUTO_CAST);
	i32t->conv_inf.emplace_back(sbt, TC_AUTO_CAST);
	i32t->conv_inf.emplace_back(i16t, TC_AUTO_CAST);
	i32t->conv_inf.emplace_back(i64t, TC_LOSTABLE_AUTO_CAST);
	i32t->conv_inf.emplace_back(f32t, TC_LOSTABLE_AUTO_CAST);
	i32t->conv_inf.emplace_back(f64t, TC_LOSTABLE_AUTO_CAST);

	// uint32
	u32t->conv_inf.emplace_back(bt, TC_AUTO_CAST);
	u32t->conv_inf.emplace_back(u16t, TC_AUTO_CAST);
	u32t->conv_inf.emplace_back(u64t, TC_LOSTABLE_AUTO_CAST);
	u32t->conv_inf.emplace_back(sbt, TC_LOSTABLE_AUTO_CAST);
	u32t->conv_inf.emplace_back(i16t, TC_LOSTABLE_AUTO_CAST);
	u32t->conv_inf.emplace_back(i32t, TC_LOSTABLE_AUTO_CAST);
	u32t->conv_inf.emplace_back(i64t, TC_LOSTABLE_AUTO_CAST);
	u32t->conv_inf.emplace_back(f32t, TC_LOSTABLE_AUTO_CAST);
	u32t->conv_inf.emplace_back(f64t, TC_LOSTABLE_AUTO_CAST);

	// int64
	i64t->conv_inf.emplace_back(bt, TC_AUTO_CAST);
	i64t->conv_inf.emplace_back(u16t, TC_AUTO_CAST);
	i64t->conv_inf.emplace_back(u32t, TC_AUTO_CAST);
	i64t->conv_inf.emplace_back(u64t, TC_LOSTABLE_AUTO_CAST);
	i64t->conv_inf.emplace_back(sbt, TC_AUTO_CAST);
	i64t->conv_inf.emplace_back(i16t, TC_AUTO_CAST);
	i64t->conv_inf.emplace_back(i32t, TC_AUTO_CAST);
	i64t->conv_inf.emplace_back(f32t, TC_LOSTABLE_AUTO_CAST);
	i64t->conv_inf.emplace_back(f64t, TC_LOSTABLE_AUTO_CAST);

	// uint64
	u64t->conv_inf.emplace_back(bt, TC_AUTO_CAST);
	u64t->conv_inf.emplace_back(u16t, TC_AUTO_CAST);
	u64t->conv_inf.emplace_back(u32t, TC_AUTO_CAST);
	u64t->conv_inf.emplace_back(sbt, TC_LOSTABLE_AUTO_CAST);
	u64t->conv_inf.emplace_back(i16t, TC_LOSTABLE_AUTO_CAST);
	u64t->conv_inf.emplace_back(i32t, TC_LOSTABLE_AUTO_CAST);
	u64t->conv_inf.emplace_back(i64t, TC_LOSTABLE_AUTO_CAST);
	u64t->conv_inf.emplace_back(f32t, TC_LOSTABLE_AUTO_CAST);
	u64t->conv_inf.emplace_back(f64t, TC_LOSTABLE_AUTO_CAST);

	// flo32
	f32t->conv_inf.emplace_back(sbt, TC_AUTO_CAST);
	f32t->conv_inf.emplace_back(bt, TC_AUTO_CAST);
	f32t->conv_inf.emplace_back(i16t, TC_AUTO_CAST);
	f32t->conv_inf.emplace_back(u16t, TC_AUTO_CAST);
	f32t->conv_inf.emplace_back(i32t, TC_LOSTABLE_AUTO_CAST);
	f32t->conv_inf.emplace_back(u32t, TC_LOSTABLE_AUTO_CAST);
	f32t->conv_inf.emplace_back(i64t, TC_LOSTABLE_AUTO_CAST);
	f32t->conv_inf.emplace_back(u64t, TC_LOSTABLE_AUTO_CAST);
	f32t->conv_inf.emplace_back(f64t, TC_LOSTABLE_AUTO_CAST);

	// flo64
	f64t->conv_inf.emplace_back(sbt, TC_AUTO_CAST);
	f64t->conv_inf.emplace_back(bt, TC_AUTO_CAST);
	f64t->conv_inf.emplace_back(i16t, TC_AUTO_CAST);
	f64t->conv_inf.emplace_back(u16t, TC_AUTO_CAST);
	f64t->conv_inf.emplace_back(i32t, TC_AUTO_CAST);
	f64t->conv_inf.emplace_back(u32t, TC_AUTO_CAST);
	f64t->conv_inf.emplace_back(i64t, TC_LOSTABLE_AUTO_CAST);
	f64t->conv_inf.emplace_back(u64t, TC_LOSTABLE_AUTO_CAST);
	f64t->conv_inf.emplace_back(f32t, TC_AUTO_CAST);

}

vector<PlnTypeInfo*>& PlnTypeInfo::getBasicTypes()
{
	return basic_types;
}



string PlnTypeInfo::getFixedArrayName(PlnVarType* item_type, vector<int>& sizes)
{
	string arr_name = "[";
	for (int s: sizes) {
		if (s)
			arr_name = arr_name + to_string(s) + ",";
		else
			arr_name = arr_name + "?,";
	}
	arr_name.back() = ']';

	string item_name = item_type->name();
	if (item_type->mode == "rir") {
		item_name = "@" + item_name;
	} else if (item_type->data_type() == DT_OBJECT) {
		item_name = "#" + item_name;
	}
	
	return arr_name + item_name;
}

PlnTypeConvCap PlnTypeInfo::canCopyFrom(const string& mode, PlnVarType *src, PlnAsgnType copymode)
{
	if (this == src->typeinf)
		return TC_SAME;
	
	// reference should be same type.
	if (mode[ALLOC_MD] == 'r')
		return TC_CANT_CONV;

	for (auto ci: conv_inf)
		if (ci.type == src->typeinf)
			return ci.capacity;

	return TC_CANT_CONV;
}

PlnTypeConvCap PlnTypeInfo::lowCapacity(PlnTypeConvCap l, PlnTypeConvCap r)
{
	if (l == TC_SAME) {
		return r;
	}

	if (l == TC_AUTO_CAST) {
		if (r != TC_SAME) return r;
		return TC_AUTO_CAST;
	}

	if (l == TC_LOSTABLE_AUTO_CAST) {
		if (r == TC_CANT_CONV) return TC_CANT_CONV;
		return TC_LOSTABLE_AUTO_CAST;
	}

	BOOST_ASSERT(l == TC_CANT_CONV);
	return TC_CANT_CONV;
}

PlnVarType* PlnTypeInfo::getVarType(const string& mode)
{
	string search_mode = mode;
	if (search_mode[0] == '-') search_mode[0] = default_mode[0];
	if (search_mode[1] == '-') search_mode[1] = default_mode[1];
	if (search_mode[2] == '-') search_mode[2] = default_mode[2];

	for (PlnVarType* vt: var_types) {
		if (vt->mode == search_mode)
			return vt;
	}
	PlnVarType* var_type = new PlnVarType(this, search_mode);
	var_types.push_back(var_type);

	return var_type;
}

// PlnVarType
int PlnVarType::data_type()
{
	if (mode[ALLOC_MD] == 'r' || mode[ALLOC_MD] == 'h') {
		return DT_OBJECT_REF;
	}
	return typeinf->data_type;
}

int PlnVarType::size()
{
	if (mode[ALLOC_MD] == 'r' || mode[ALLOC_MD] == 'h') {
		return 8;	// ponter_size
	}
	return typeinf->data_size;
}

int PlnVarType::align()
{
	if (mode[ALLOC_MD] == 'r' || mode[ALLOC_MD] == 'h') {
		return 8;	// ponter_size
	}
	return typeinf->data_align;
}

bool PlnVarType::has_heap_member()
{
	if (typeinf->type == TP_FIXED_ARRAY) {
		PlnFixedArrayTypeInfo *farr_typeinf = static_cast<PlnFixedArrayTypeInfo*>(typeinf);
		return farr_typeinf->has_heap_member;

	} else if (typeinf->type == TP_STRUCT) {
		PlnStructTypeInfo *strct_type = static_cast<PlnStructTypeInfo*>(typeinf);
		return strct_type->has_heap_member;

	}
	
	return false;
}

void PlnVarType::getAllocArgs(vector<PlnExpression*> &alloc_args)
{
}

void PlnVarType::getFreeArgs(vector<PlnExpression*> &free_args)
{
}

PlnVarType* PlnVarType::getVarType(const string& mode)
{
	return typeinf->getVarType(mode);
}

PlnVarType* PlnVarType::getByte(const string& mode)
{
 	return byte_type->getVarType(mode);
}

PlnVarType* PlnVarType::getSint(const string& mode)
{
	return int_type->getVarType(mode);
}

PlnVarType* PlnVarType::getUint(const string& mode)
{
	return uint_type->getVarType(mode);
}

PlnVarType* PlnVarType::getFlo32(const string& mode)
{
	return flo32_type->getVarType(mode);
}

PlnVarType* PlnVarType::getFlo64(const string& mode)
{
	return flo64_type->getVarType(mode);
}

PlnVarType* PlnVarType::getReadOnlyCStr(const string& mode)
{
	return ro_cstr_type->getVarType(mode);
}

PlnVarType* PlnVarType::getObject(const string& mode)
{
	return object_type->getVarType(mode);
}

PlnVarType* PlnVarType::getAny(const string& mode)
{
	return any_type->getVarType(mode);
}
