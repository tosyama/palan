/// Heap allocator class declaration.
///
/// @file	PlnHeapAllocator.h
/// @copyright	2018 YAMAGUCHI Toshinobu 

#include "PlnExpression.h"

class PlnHeapAllocator : public PlnExpression
{
public:
	PlnHeapAllocator();

	static PlnHeapAllocator* createHeapAllocation(PlnValue var_val);
	static PlnHeapAllocator* createHeapFree(PlnVariable* var);
};

