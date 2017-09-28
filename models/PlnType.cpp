/// Type model class definition.
///
/// @file	PlnType.cpp
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include "PlnType.h"

using namespace std;

// Basic types
static bool is_initialzed_type = false;
static vector<PlnType*> basic_types;

static void initBasicTypes()
{
	PlnType* t = new PlnType();
	t->name = "sbyte";
	t->type = TP_INT;
	t->size = 1;
	basic_types.push_back(t);

	t = new PlnType();
	t->name = "byte";
	t->type = TP_UINT;
	t->size = 1;
	basic_types.push_back(t);

	t = new PlnType();
	t->name = "int16";
	t->type = TP_INT;
	t->size = 2;
	basic_types.push_back(t);

	t = new PlnType();
	t->name = "uint16";
	t->type = TP_UINT;
	t->size = 2;
	basic_types.push_back(t);
	
	t = new PlnType();
	t->name = "int32";
	t->type = TP_INT;
	t->size = 4;
	basic_types.push_back(t);

	t = new PlnType();
	t->name = "uint32";
	t->type = TP_UINT;
	t->size = 4;
	basic_types.push_back(t);

	t = new PlnType();
	t->name = "int64";
	t->type = TP_INT;
	t->size = 8;
	basic_types.push_back(t);

	t = new PlnType();
	t->name = "uint64";
	t->type = TP_UINT;
	t->size = 8;
	basic_types.push_back(t);

	is_initialzed_type = true;
}

vector<PlnType*> PlnType::getBasicTypes()
{
	if (!is_initialzed_type) initBasicTypes();
	return basic_types;
}

