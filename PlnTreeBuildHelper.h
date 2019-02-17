/// Helper functions for building model tree directly.
///
/// @file	PlnBuildTreeHelper.h
/// @copyright	2018 YAMAGUCHI Toshinobu 

#include "PlnModel.h"

namespace palan
{
	PlnVariable* declareUInt(PlnBlock* block, string name, uint64_t init_i);
	void incrementUInt(PlnBlock* block, PlnVariable *var, uint64_t inc);

	void malloc(PlnBlock* block, PlnVariable* var, uint64_t alloc_size);
	void free(PlnBlock* block, PlnVariable* var);
	void exit(PlnBlock* block, uint64_t result);

	PlnArrayItem* rawArrayItem(PlnVariable* var, PlnVariable* index, PlnModule *module);

	PlnBlock* whileLess(PlnBlock* block, PlnVariable *var, uint64_t i);
}

