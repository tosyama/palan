/// Module model class declaration.
///
/// @file	PlnModule.h
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include <iostream>
#include <string>
#include <vector>
#include "PlnExpression.h"

// Module: Type, ReadOnlyData, Functions
class PlnModule
{
public:
	PlnBlock* toplevel;
	int stack_size;
	vector<PlnFunction*> functions;
	vector<PlnReadOnlyData*> readonlydata;
	vector<PlnType*> types;
	vector<PlnType*> fixedarray_types;

	PlnModule();

	PlnType* getType(const string& type_name);
	PlnType* getFixedArrayType(int item_size, vector<int>& sizes);
	PlnFunction* getFunc(const string& func_name, vector<PlnExpression*>& args);
	PlnReadOnlyData* getReadOnlyData(string &str);

	void finish(PlnDataAllocator& da);

	void dump(ostream& os, string indent="");
	void gen(PlnGenerator& g);
};

