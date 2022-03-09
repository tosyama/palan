/// Generic object utility definitions.
///
/// @file	PlnGeneralObject.cpp
/// @copyright	2018-2020 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include "../PlnConstants.h"
#include "expressions/PlnFunctionCall.h"
#include "PlnGeneralObject.h"

#include "../PlnDataAllocator.h"
#include "../PlnGenerator.h"
#include "expressions/PlnMemCopy.h"

// PlnSingleObjectCopyer
PlnExpression* PlnSingleObjectCopyer::getCopyEx(PlnExpression* dst_var, PlnExpression* src_var)
{
	return new PlnMemCopy(dst_var, src_var, new PlnExpression(len));
}

// PlnTwoParamsCopyer
PlnExpression* PlnTwoParamsCopyer::getCopyEx(PlnExpression* dst_var, PlnExpression* src_var)
{
	vector<PlnExpression*> args = { dst_var, src_var };
	return new PlnFunctionCall(copy_func, args);
}
