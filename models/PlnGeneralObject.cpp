/// Generic object utility definitions.
///
/// @file	PlnGeneralObject.h
/// @copyright	2018 YAMAGUCHI Toshinobu 

#include "../PlnConstants.h"
#include "PlnGeneralObject.h"
#include "expressions/PlnFunctionCall.h"

#include <boost/assert.hpp>
#include "../PlnDataAllocator.h"
#include "../PlnGenerator.h"
#include "expressions/PlnMemCopy.h"

PlnExpression* PlnSingleObjectAllocator::getAllocEx()
{
	PlnFunction *alloc_func = PlnFunctionCall::getInternalFunc(IFUNC_MALLOC);
	vector<PlnExpression*> args = { new PlnExpression(alloc_size) };
	PlnFunctionCall *alloc_call = new PlnFunctionCall(alloc_func, args);

	return alloc_call;
}

PlnExpression* PlnSingleObjectFreer::getFreeEx(PlnExpression* free_var)
{
	PlnFunction *free_func = PlnFunctionCall::getInternalFunc(IFUNC_FREE);
	vector<PlnExpression*> args = { free_var };
	PlnFunctionCall *free_call = new PlnFunctionCall(free_func, args);

	return free_call;
}

PlnExpression* PlnSingleObjectCopyer::getCopyEx(PlnExpression* dst_var, PlnExpression* src_var)
{
	PlnMemCopy *mcopy = new PlnMemCopy(dst_var, src_var, new PlnExpression(len));
	return mcopy;
}

PlnExpression* PlnNoParamAllocator::getAllocEx()
{
	vector<PlnExpression*> args;
	PlnFunctionCall *alloc_call = new PlnFunctionCall(alloc_func, args);

	return alloc_call;
}

PlnExpression* PlnSingleParamFreer::getFreeEx(PlnExpression* free_var)
{
	vector<PlnExpression*> args = { free_var };
	PlnFunctionCall *free_call = new PlnFunctionCall(free_func, args);

	return free_call;
}

PlnExpression* PlnTwoParamsCopyer::getCopyEx(PlnExpression* dst_var, PlnExpression* src_var)
{
	vector<PlnExpression*> args = { dst_var, src_var };
	PlnFunctionCall *free_call = new PlnFunctionCall(copy_func, args);

	return free_call;
}
