/// Helper functions for building model tree directly.
///
/// @file	PlnBuildTreeHelper.h
/// @copyright	2018-2022 YAMAGUCHI Toshinobu 

#include "PlnModel.h"

namespace palan
{
	PlnVariable* declareUInt(PlnBlock* block, string name, uint64_t init_i);
	void incrementUInt(PlnBlock* block, PlnVariable *var, uint64_t inc);

	void malloc(PlnBlock* block, PlnVariable* var, PlnExpression* alloc_size_ex);
	void free(PlnBlock* block, PlnVariable* var);
	void exit(PlnBlock* block, uint64_t result);

	PlnArrayItem* rawArrayItem(PlnVariable* var, PlnVariable* index);

	PlnBlock* whileLess(PlnBlock* block, PlnVariable *var, PlnExpression* loop_num_ex);

	PlnExpression* preprocessSrcEx(PlnExpression* src_ex, PlnVarType* dst_type); 
}

