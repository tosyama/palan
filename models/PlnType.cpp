/// Type model class definition.
///
/// @file	PlnType.cpp
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include "PlnType.h"
#include "../PlnConstants.h"

using namespace std;

// Basic types
static bool is_initialzed_type = false;
static vector<PlnType*> basic_types;
static PlnType* int_type = NULL;
static PlnType* uint_type = NULL;
static PlnType* ro_cstr_type = NULL;

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

	is_initialzed_type = true;
}

vector<PlnType*> PlnType::getBasicTypes()
{
	if (!is_initialzed_type) initBasicTypes();
	return basic_types;
}

PlnType* PlnType::getSint()
{
	BOOST_ASSERT(int_type != NULL);
	return int_type;
}

PlnType* PlnType::getUint()
{
	BOOST_ASSERT(uint_type != NULL);
	return uint_type;
}

PlnType* PlnType::getReadOnlyCStr()
{
	BOOST_ASSERT(ro_cstr_type != NULL);
	return ro_cstr_type;
}
