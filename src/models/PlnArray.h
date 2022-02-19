/// Array model class declaration.
///
/// @file	PlnArray.h
/// @copyright	2018-2021 YAMAGUCHI Toshinobu 

#include "../PlnModel.h"

class PlnArray
{
public:
	static PlnFunction* createObjRefArrayCopyFunc(string func_name, PlnFixedArrayTypeInfo* arr_type, PlnBlock *block);
};
