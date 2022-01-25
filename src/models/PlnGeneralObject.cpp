/// Generic object utility definitions.
///
/// @file	PlnGeneralObject.cpp
/// @copyright	2018-2020 YAMAGUCHI Toshinobu 

#include "../PlnConstants.h"
#include "expressions/PlnFunctionCall.h"
#include "PlnGeneralObject.h"

#include <boost/assert.hpp>
#include "../PlnDataAllocator.h"
#include "../PlnGenerator.h"
#include "expressions/PlnMemCopy.h"

// PlnSingleObjectAllocator
PlnExpression* PlnSingleObjectAllocator::getAllocEx(vector<PlnExpression*>& args)
{
	PlnFunction *alloc_func = PlnFunctionCall::getInternalFunc(IFUNC_MALLOC);
	vector<PlnExpression*> size_arg = { new PlnExpression(alloc_size) };

	return new PlnFunctionCall(alloc_func, size_arg);
}

// PlnSingleObjectFreer
PlnExpression* PlnSingleObjectFreer::getFreeEx(PlnExpression* free_var)
{
	PlnFunction *free_func = PlnFunctionCall::getInternalFunc(IFUNC_FREE);
	vector<PlnExpression*> args = { free_var };

	return new PlnFunctionCall(free_func, args);
}

// PlnSingleObjectCopyer
PlnExpression* PlnSingleObjectCopyer::getCopyEx(PlnExpression* dst_var, PlnExpression* src_var)
{
	return new PlnMemCopy(dst_var, src_var, new PlnExpression(len));
}

PlnDeepCopyExpression* PlnSingleObjectCopyer::getCopyEx()
{
	return new PlnMemCopy(NULL, NULL, new PlnExpression(len));
}

// PlnNoParamAllocator
PlnExpression* PlnNoParamAllocator::getAllocEx(vector<PlnExpression*>& args)
{
	vector<PlnExpression*> args0;
	return new PlnFunctionCall(alloc_func, args0);
}

// PlnSingleParamInternalAllocator
PlnExpression* PlnSingleParamInternalAllocator::getInternalAllocEx(PlnExpression* base_var)
{
	vector<PlnExpression*> args = { base_var };
	return new PlnFunctionCall(alloc_func, args);
}

// PlnSingleParamFreer
PlnExpression* PlnSingleParamFreer::getFreeEx(PlnExpression* free_var)
{
	vector<PlnExpression*> args = { free_var };
	return new PlnFunctionCall(free_func, args);
}

// PlnDeepCopyFuncCall
class PlnDeepCopyFuncCall : public PlnDeepCopyExpression {
	PlnFunctionCall* fcall;
public:
	explicit PlnDeepCopyFuncCall(PlnFunction* f)
		: PlnDeepCopyExpression(ET_FUNCCALL) {
			fcall = new PlnFunctionCall(f);
	}

	PlnDeepCopyFuncCall(const PlnDeepCopyFuncCall&) = delete;

	PlnDataPlace* dstDp(PlnDataAllocator &da) override {
		BOOST_ASSERT(fcall->arg_dps.size()); // Should be call srcDp first.
		return fcall->arg_dps[0];
	}

	PlnDataPlace* srcDp(PlnDataAllocator &da) override {
		BOOST_ASSERT(!fcall->arg_dps.size());
		fcall->loadArgDps(da);
		return fcall->arg_dps[1];
	}

	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override {
		fcall->finish(da, si);
	}

	void gen(PlnGenerator& g) override {
		fcall->gen(g);
	}
};

// PlnTwoParamsCopyer
PlnExpression* PlnTwoParamsCopyer::getCopyEx(PlnExpression* dst_var, PlnExpression* src_var)
{
	vector<PlnExpression*> args = { dst_var, src_var };
	return new PlnFunctionCall(copy_func, args);
}

PlnDeepCopyExpression* PlnTwoParamsCopyer::getCopyEx()
{
	return new PlnDeepCopyFuncCall(copy_func);
}
