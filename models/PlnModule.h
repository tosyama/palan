/// Module model class declaration.
///
/// @file	PlnModule.h
/// @copyright	2017-2019 YAMAGUCHI Toshinobu 

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
	vector<PlnType*> types;

	PlnModule();

	PlnType* getType(const string& type_name);
	PlnType* getFixedArrayType(PlnType* item_type, vector<PlnType*> &item_type2, vector<int>& sizes);

	int getJumpID();

	void gen(PlnDataAllocator& da, PlnGenerator& g);
};

