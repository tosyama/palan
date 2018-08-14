/// Module model class declaration.
///
/// @file	PlnModule.h
/// @copyright	2017-2018 YAMAGUCHI Toshinobu 

#include <string>
#include <vector>
#include "PlnExpression.h"

// Module: Type, ReadOnlyData, Functions
class PlnCompileError;
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

	PlnModule();

	PlnType* getType(const string& type_name);
	PlnType* getFixedArrayType(vector<PlnType*> &item_type, vector<int>& sizes);
	PlnFunction* getFuncProto(const string& func_name, vector<string>& param_types, vector<string>& ret_types);
	int getJumpID();
	PlnReadOnlyData* getReadOnlyData(const string &str);

	void finish(PlnDataAllocator& da);
	void gen(PlnGenerator& g);
};

