/// Module model class declaration.
///
/// @file	PlnModule.h
/// @copyright	2017-2020 YAMAGUCHI Toshinobu 

#include <string>
#include <vector>
#include "PlnExpression.h"

// Module: Type, ReadOnlyData, Functions
class PlnCompileError;
class PlnModule
{
public:
	PlnBlock* toplevel;
	int max_jmp_id;
	bool do_opti_regalloc = true;
	vector<PlnFunction*> functions;

	PlnModule();
	PlnModule(const PlnModule&) = delete;
	~PlnModule();

	int getJumpID();

	void gen(PlnDataAllocator& da, PlnGenerator& g);
};

