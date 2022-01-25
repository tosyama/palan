/// Array model class declaration.
///
/// @file	PlnArray.h
/// @copyright	2018-2021 YAMAGUCHI Toshinobu 

#include "../PlnModel.h"

class PlnArray
{
public:
	static PlnFunction* createObjRefArrayFreeFunc(string func_name, PlnFixedArrayTypeInfo* arr_type, PlnBlock *block);
	static PlnFunction* createObjRefArrayCopyFunc(string func_name, PlnFixedArrayTypeInfo* arr_type, PlnBlock *block);
	static PlnFunction* createObjRefArrayInternalAllocFunc(string func_name, PlnFixedArrayTypeInfo* arr_type, PlnBlock *block);
	static PlnFunction* createObjRefArrayInternalFreeFunc(string func_name, PlnFixedArrayTypeInfo* arr_type, PlnBlock *block);

	static PlnFunction* createObjArrayAllocFunc(string func_name, PlnFixedArrayTypeInfo* arr_type, PlnBlock *block);
	static PlnFunction* createObjArrayFreeFunc(string func_name, PlnFixedArrayTypeInfo* arr_type, PlnBlock *block);
};
