/// Type model class declaration.
///
/// @file	PlnType.h
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include "../PlnModel.h"

// PlnType
enum PlnTypType {
	TP_INT,
	TP_UINT,
	TP_OBJ,
	TP_IMP,
};

class PlnType {
public:
	PlnTypType	type;
	string name;
	int size;

	// static PlnType* getBasicType(const string& type_name);
	static vector<PlnType*> getBasicTypes();
};
