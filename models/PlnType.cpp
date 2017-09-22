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
	// Note: should sort by name
	PlnType* t = new PlnType();
	t->name = "byte";
	t->type = TP_UINT;
	t->size = 1;
	basic_types.push_back(t);
	
	t = new PlnType();
	t->name = "int64";
	t->type = TP_INT;
	t->size = 8;
	basic_types.push_back(t);

	is_initialzed_type = true;
}

vector<PlnType*> PlnType::getBasicTypes()
{
	if (!is_initialzed_type) initBasicTypes();
	return basic_types;
}

/*PlnType* PlnType::getBasicType(const string& type_name)
{
	if (!is_initialzed_type) initBasicTypes();

	auto t = basic_types.find(type_name);
	if (t != basic_types.end())
		return t->second;
	else
		return NULL;
}
*/
