/// Type casting class definition.
///
/// @file	PlnTypeCast.cpp
/// @copyright	2023 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include "../../PlnConstants.h"
#include "PlnTypeCast.h"

PlnTypeCast::PlnTypeCast(PlnExpression* exp, vector<PlnVarType*> var_types)
	: PlnExpression(exp->type)
{
}

PlnTypeCast::~PlnTypeCast()
{
}
	
void PlnTypeCast::finish(PlnDataAllocator& da, PlnScopeInfo& si)
{
}

void PlnTypeCast::gen(PlnGenerator& g)
{
}
