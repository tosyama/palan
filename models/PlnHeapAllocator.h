/// Heap allocator class declaration.
///
/// @file	PlnHeapAllocator.h
/// @copyright	2018 YAMAGUCHI Toshinobu 

#include "PlnExpression.h"

class PlnHeapAllocator : public PlnExpression
{
public:
	PlnHeapAllocator();

	static PlnHeapAllocator* createHeapAllocation(vector<PlnType*> &var_type);
	static PlnHeapAllocator* createHeapFree(PlnVariable* var);
};

