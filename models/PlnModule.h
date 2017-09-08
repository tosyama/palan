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
	vector<PlnFunction*> functions;
	vector<PlnReadOnlyData*> readonlydata;
	PlnModule();

	PlnType* getType(const string& type_name);
	PlnFunction* getFunc(const string& func_name, vector<PlnExpression*>& args);
	PlnReadOnlyData* getReadOnlyData(string &str);

	void finish(PlnDataAllocator& da);

	void dump(ostream& os, string indent="");
	void gen(PlnGenerator& g);
};

