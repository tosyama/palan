/// Type model class definition.
///
/// @file	PlnType.cpp
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include "PlnType.h"
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
static PlnType* ro_cstr_type = NULL;
static PlnType* object_type = NULL;
static PlnType* raw_array_type = NULL;

// PlnAllocator
PlnExpression* PlnAllocator::getAllocEx(PlnVariable* var)
{
	PlnAllocator* allocator= var->var_type.back()->allocator;
	BOOST_ASSERT(allocator);
	return allocator->getAllocEx();
}

// PlnFreer
PlnExpression* PlnFreer::getFreeEx(PlnVariable* var)
{
	PlnFreer* freer = var->var_type.back()->freer;
	BOOST_ASSERT(freer);
	return freer->getFreeEx(new PlnExpression(var));
}

// PlnType
PlnType::PlnType()
	: allocator(NULL), freer(NULL)
{
}

static void initBasicTypes()
{
	PlnType* t = new PlnType();
	t->name = "sbyte";
	t->data_type = DT_SINT;
	t->size = 1;
	basic_types.push_back(t);

	t = new PlnType();
	t->name = "byte";
	t->data_type = DT_UINT;
	t->size = 1;
	basic_types.push_back(t);
	byte_type = t;

	t = new PlnType();
	t->name = "int16";
	t->data_type = DT_SINT;
	t->size = 2;
	basic_types.push_back(t);

	t = new PlnType();
	t->name = "uint16";
	t->data_type = DT_UINT;
	t->size = 2;
	basic_types.push_back(t);
	
	t = new PlnType();
	t->name = "int32";
	t->data_type = DT_SINT;
	t->size = 4;
	basic_types.push_back(t);

	t = new PlnType();
	t->name = "uint32";
	t->data_type = DT_UINT;
	t->size = 4;
	basic_types.push_back(t);

	t = new PlnType();
	t->name = "int64";
	t->data_type = DT_SINT;
	t->size = 8;
	basic_types.push_back(t);
	int_type = t;

	t = new PlnType();
	t->name = "uint64";
	t->data_type = DT_UINT;
	t->size = 8;
	basic_types.push_back(t);
	uint_type = t;

	t = new PlnType();
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

	t = new PlnType();
	t->name = "[?]";
	t->data_type = DT_OBJECT_REF;
	t->size = 8;
	t->inf.fixedarray.is_fixed_size = false;
	t->inf.fixedarray.alloc_size = 0;
	t->inf.fixedarray.item_size = 0;
	t->inf.fixedarray.sizes = new vector<int>();
	t->inf.fixedarray.sizes->push_back(0);
	basic_types.push_back(t);
	raw_array_type = t;

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

PlnType* PlnType::getReadOnlyCStr()
{
	return ro_cstr_type;
}

PlnType* PlnType::getObject()
{
	return object_type;
}

PlnType* PlnType::getRawArray()
{
	return raw_array_type;
}

string PlnType::getFixedArrayName(PlnType* item_type, vector<int>& sizes)
{
	string name = "]";
	for (int s: sizes) {
		name = "," + to_string(s) + name;
	}
	name.front() = '[';
	name = item_type->name + name;

	return name;
}

