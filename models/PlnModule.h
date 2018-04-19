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
	vector<int> save_regs;
	vector<PlnDataPlace*> save_reg_dps;
	int max_jmp_id;

	int stack_size;
	vector<PlnFunction*> functions;
	vector<PlnReadOnlyData*> readonlydata;
	vector<PlnType*> types;
	vector<PlnType*> fixedarray_types;

	PlnModule();

	PlnType* getType(const string& type_name);
	PlnType* getFixedArrayType(vector<PlnType*> &item_type, vector<int>& sizes);
	PlnFunction* getFunc(const string& func_name, vector<PlnExpression*>& args);
	int getJumpID();
	PlnReadOnlyData* getReadOnlyData(string &str);

	void finish(PlnDataAllocator& da);

	void dump(ostream& os, string indent="");
	void gen(PlnGenerator& g);
};

