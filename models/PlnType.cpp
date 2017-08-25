/// Type model class definition.
///
/// @file	PlnType.cpp
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include <unordered_map>
#include "PlnType.h"

using namespace std;

// Basic types
static bool is_initialzed_type = false;
static unordered_map<string, PlnType*> basic_types;

static void initBasicTypes()
{
	PlnType* t = new PlnType();
	t->type = TP_INT;
	t->name = "int64";
	t->size = 8;
	basic_types[t->name] = t;

	t = new PlnType();
	t->type = TP_UINT;
	t->name = "byte";
	t->size = 1;
	basic_types[t->name] = t;

	is_initialzed_type = true;
}

PlnType* PlnType::getBasicType(const string& type_name)
{
	if (!is_initialzed_type) initBasicTypes();

	auto t = basic_types.find(type_name);
	if (t != basic_types.end())
		return t->second;
	else
		return NULL;
}

