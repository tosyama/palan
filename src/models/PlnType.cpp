/// Type model class definition.
///
/// @file	PlnType.cpp
/// @copyright	2017-2021 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include "../PlnConstants.h"
#include "PlnType.h"
#include "types/PlnFixedArrayType.h"
#include "PlnVariable.h"
#include "PlnExpression.h"

using namespace std;

// Basic types
static bool is_initialzed_type = false;
static vector<PlnType*> basic_types;
static PlnType* byte_type = NULL;
static PlnType* int_type = NULL;
static PlnType* uint_type = NULL;
static PlnType* flo32_type = NULL;
static PlnType* flo64_type = NULL;
static PlnType* ro_cstr_type = NULL;
static PlnType* object_type = NULL;
static PlnType* any_type = NULL;

// PlnAllocator
PlnExpression* PlnAllocator::getAllocEx(PlnVariable* var)
{
	return var->var_type->getAllocEx();
}

// PlnInternalAllocator
PlnExpression* PlnInternalAllocator::getInternalAllocEx(PlnVariable* var)
{
	return var->var_type->getInternalAllocEx(new PlnExpression(var));
}
 
// PlnFreer
PlnExpression* PlnFreer::getFreeEx(PlnVariable* var)
{
	return var->var_type->getFreeEx(new PlnExpression(var));
}

PlnExpression* PlnFreer::getInternalFreeEx(PlnVariable* var)
{
	return var->var_type->getInternalFreeEx(new PlnExpression(var));
}
// PlnType

PlnType::PlnType(PlnTypeType type)
	: type(type), allocator(NULL), internal_allocator(NULL),
	freer(NULL), internal_freer(NULL), copyer(NULL)
{
}

PlnType::~PlnType()
{
	delete allocator;
	delete internal_allocator;
	delete freer;
	delete copyer;
}

void PlnType::initBasicTypes()
{
	if (is_initialzed_type)
		return;

	is_initialzed_type = true;

	PlnType* sbt = new PlnType();
	sbt->name = "sbyte";
	sbt->default_mode = "wis";
	sbt->data_type = DT_SINT;
	sbt->data_size = 1;
	basic_types.push_back(sbt);

	PlnType* bt = new PlnType();
	bt->name = "byte";
	bt->default_mode = "wis";
	bt->data_type = DT_UINT;
	bt->data_size = 1;
	basic_types.push_back(bt);
	byte_type = bt;

	PlnType* i16t = new PlnType();
	i16t->name = "int16";
	i16t->default_mode = "wis";
	i16t->data_type = DT_SINT;
	i16t->data_size = 2;
	basic_types.push_back(i16t);

	PlnType* u16t = new PlnType();
	u16t->name = "uint16";
	u16t->default_mode = "wis";
	u16t->data_type = DT_UINT;
	u16t->data_size = 2;
	basic_types.push_back(u16t);
	
	PlnType* i32t = new PlnType();
	i32t->name = "int32";
	i32t->default_mode = "wis";
	i32t->data_type = DT_SINT;
	i32t->data_size = 4;
	basic_types.push_back(i32t);

	PlnType* u32t = new PlnType();
	u32t->name = "uint32";
	u32t->default_mode = "wis";
	u32t->data_type = DT_UINT;
	u32t->data_size = 4;
	basic_types.push_back(u32t);

	PlnType* i64t = new PlnType();
	i64t->name = "int64";
	i64t->default_mode = "wis";
	i64t->data_type = DT_SINT;
	i64t->data_size = 8;
	basic_types.push_back(i64t);
	int_type = i64t;

	PlnType* u64t = new PlnType();
	u64t->name = "uint64";
	u64t->default_mode = "wis";
	u64t->data_type = DT_UINT;
	u64t->data_size = 8;
	basic_types.push_back(u64t);
	uint_type = u64t;

	PlnType* f32t = new PlnType();
	f32t->name = "flo32";
	f32t->default_mode = "wis";
	f32t->data_type = DT_FLOAT;
	f32t->data_size = 4;
	basic_types.push_back(f32t);
	flo32_type = f32t;

	PlnType* f64t = new PlnType();
	f64t->name = "flo64";
	f64t->default_mode = "wis";
	f64t->data_type = DT_FLOAT;
	f64t->data_size = 8;
	basic_types.push_back(f64t);
	flo64_type = f64t;

	PlnType* t = new PlnType();
	t->name = "_ro_cstr";
	t->default_mode = "rir";
	t->data_type = DT_OBJECT;
	t->data_size = 0;
	basic_types.push_back(t);
	ro_cstr_type = t;

	t = new PlnType();
	t->name = "object";
	t->default_mode = "wmr";
	t->data_type = DT_OBJECT;
	t->data_size = 0;
	basic_types.push_back(t);
	object_type = t;

	t = new PlnType();
	t->name = "";
	t->default_mode = "wmi";
	t->data_type = DT_UNKNOWN;
	t->data_size = 0;
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

vector<PlnType*>& PlnType::getBasicTypes()
{
	return basic_types;
}

PlnType* PlnType::getByte()
{
 	return byte_type;
}

PlnType* PlnType::getSint()
{
	return int_type;
}

PlnType* PlnType::getUint()
{
	return uint_type;
}

PlnType* PlnType::getFlo32()
{
	return flo32_type;
}

PlnType* PlnType::getFlo64()
{
	return flo64_type;
}

PlnType* PlnType::getReadOnlyCStr()
{
	return ro_cstr_type;
}

PlnType* PlnType::getObject()
{
	return object_type;
}

PlnType* PlnType::getAny()
{
	return any_type;
}

string PlnType::getFixedArrayName(PlnVarType* item_type, vector<int>& sizes)
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
	if (item_type->mode == "rir")
		item_name = "@" + item_name;
	
	return arr_name + item_name;
}

PlnTypeConvCap PlnType::canCopyFrom(const string& mode, PlnVarType *src, PlnAsgnType copymode)
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

PlnTypeConvCap PlnType::lowCapacity(PlnTypeConvCap l, PlnTypeConvCap r)
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

	if (l == TC_CANT_CONV)
		return TC_CANT_CONV;

	BOOST_ASSERT(false);
}

PlnVarType* PlnType::getVarType(const string& mode)
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
