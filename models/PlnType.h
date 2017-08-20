/// Type model class declaration.
///
/// @file	PlnType.h
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include "../PlnModel.h"

// PlnType
enum PlnTypType {
	TP_INT8,
	TP_UINT8,
	TP_OBJ,
	TP_IMP,
};

class PlnType {
public:
	PlnTypType	type;
	string name;
	int size;
};
