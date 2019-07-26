/// Array model class declaration.
///
/// @file	PlnArray.h
/// @copyright	2018-2019 YAMAGUCHI Toshinobu 

#include "../PlnModel.h"

class PlnArray
{
public:
	static PlnFunction* createObjArrayAllocFunc(string func_name, PlnFixedArrayType* arr_type, PlnModule *module);
	static PlnFunction* createObjArrayFreeFunc(string func_name, PlnFixedArrayType* arr_type, PlnModule *module);
	static PlnFunction* createObjArrayCopyFunc(string func_name, PlnFixedArrayType* arr_type, PlnModule *module);

	static PlnFunction* createObjArrayAllocFunc(string func_name, PlnFixedArrayType* arr_type, PlnBlock *block);
	static PlnFunction* createObjArrayFreeFunc(string func_name, PlnFixedArrayType* arr_type, PlnBlock *block);
	static PlnFunction* createObjArrayCopyFunc(string func_name, PlnFixedArrayType* arr_type, PlnBlock *block);
};
