/// Helper functions for building model tree directly.
///
/// @file	PlnBuildTreeHelper.h
/// @copyright	2018 YAMAGUCHI Toshinobu 

#include "PlnModel.h"

namespace palan
{
	PlnVariable* declareUInt(PlnBlock* block, string name, uint64_t init_i);
	void incrementUInt(PlnBlock* block, PlnVariable *var, uint64_t inc);
	void free(PlnBlock* block, PlnVariable* var);
}

