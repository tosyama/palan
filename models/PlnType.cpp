/// Type model class definition.
///
/// @file	PlnType.cpp
/// @copyright	2017-2019 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include "PlnType.h"
#include "types/PlnFixedArrayType.h"
#include "PlnVariable.h"
#include "PlnExpression.h"
#include "../PlnConstants.h"

using namespace std;

// Basic types
static bool is_initialzed_type = false;
static vector<PlnType*> basic_types;
static PlnType* byte_type = NULL;
static PlnType* int_type = NULL;
static PlnType* uint_type = NULL;
static PlnType* flo_type = NULL;
static PlnType* ro_cstr_type = NULL;
static PlnType* object_type = NULL;
static PlnType* any_type = NULL;

// PlnAllocator
PlnExpression* PlnAllocator::getAllocEx(PlnVariable* var)
{
	PlnAllocator* allocator= var->var_type->allocator;
	BOOST_ASSERT(allocator);
	return allocator->getAllocEx();
}

// PlnFreer
PlnExpression* PlnFreer::getFreeEx(PlnVariable* var)
{
	PlnFreer* freer = var->var_type->freer;
	BOOST_ASSERT(freer);
	return freer->getFreeEx(new PlnExpression(var));
}

// PlnType
PlnType::PlnType(PlnTypeType type)
	: type(type), allocator(NULL), freer(NULL), copyer(NULL)
{
}

class AnyType : public PlnType {
public:
	AnyType() : PlnType(TP_PRIMITIVE) {}
	PlnTypeConvCap canConvFrom(PlnType *src) override { BOOST_ASSERT(false); }
};

static void initBasicTypes()
{
	PlnType* sbt = new PlnType();
	sbt->name = "sbyte";
	sbt->data_type = DT_SINT;
	sbt->size = 1;
	basic_types.push_back(sbt);

	PlnType* bt = new PlnType();
	bt->name = "byte";
	bt->data_type = DT_UINT;
	bt->size = 1;
	basic_types.push_back(bt);
	byte_type = bt;

	PlnType* i16t = new PlnType();
	i16t->name = "int16";
	i16t->data_type = DT_SINT;
	i16t->size = 2;
	basic_types.push_back(i16t);

	PlnType* u16t = new PlnType();
	u16t->name = "uint16";
	u16t->data_type = DT_UINT;
	u16t->size = 2;
	basic_types.push_back(u16t);
	
	PlnType* i32t = new PlnType();
	i32t->name = "int32";
	i32t->data_type = DT_SINT;
	i32t->size = 4;
	basic_types.push_back(i32t);

	PlnType* u32t = new PlnType();
	u32t->name = "uint32";
	u32t->data_type = DT_UINT;
	u32t->size = 4;
	basic_types.push_back(u32t);

	PlnType* i64t = new PlnType();
	i64t->name = "int64";
	i64t->data_type = DT_SINT;
	i64t->size = 8;
	basic_types.push_back(i64t);
	int_type = i64t;

	PlnType* u64t = new PlnType();
	u64t->name = "uint64";
	u64t->data_type = DT_UINT;
	u64t->size = 8;
	basic_types.push_back(u64t);
	uint_type = u64t;

	PlnType* f32t = new PlnType();
	f32t->name = "flo32";
	f32t->data_type = DT_FLOAT;
	f32t->size = 4;
	basic_types.push_back(f32t);

	PlnType* f64t = new PlnType();
	f64t->name = "flo64";
	f64t->data_type = DT_FLOAT;
	f64t->size = 8;
	basic_types.push_back(f64t);
	flo_type = f64t;

	PlnType* t = new PlnType();
	t->name = "_ro_cstr";
	t->data_type = DT_OBJECT_REF;
	t->size = 8;
	basic_types.push_back(t);
	ro_cstr_type = t;

	t = new PlnType();
	t->name = "object";
	t->data_type = DT_OBJECT_REF;
	t->size = 8;
	basic_types.push_back(t);
	object_type = t;

	t = new AnyType();
	t->name = "";
	t->data_type = DT_UNKNOWN;
	t->size = 0;
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

	is_initialzed_type = true;
}

vector<PlnType*> PlnType::getBasicTypes()
{
	if (!is_initialzed_type) initBasicTypes();
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

PlnType* PlnType::getFlo()
{
	return flo_type;
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

string PlnType::getFixedArrayName(PlnType* item_type, vector<int>& sizes)
{
	string arr_name = "[";
	for (int s: sizes) {
		if (s)
			arr_name = arr_name + to_string(s) + ",";
		else
			arr_name = arr_name + "?,";
	}
	arr_name.back() = ']';

	PlnType *it = item_type;
	while (it->type == TP_FIXED_ARRAY) {
		it = static_cast<PlnFixedArrayType*>(it)->item_type;
	}
	string& item_name = it->name;
	string item_suffix = item_type->name.substr(item_name.size());

	return item_name + arr_name + item_suffix;
}

PlnTypeConvCap PlnType::canConvFrom(PlnType *src)
{
	if (this == src)
		return TC_SAME;
	
	for (auto ci: conv_inf)
		if (ci.type == src)
			return ci.capacity;

	return TC_CANT_CONV;
}

