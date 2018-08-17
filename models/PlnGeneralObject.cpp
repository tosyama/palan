/// Generic object utility definitions.
///
/// @file	PlnGeneralObject.cpp
/// @copyright	2018 YAMAGUCHI Toshinobu 

#include "../PlnConstants.h"
#include "expressions/PlnFunctionCall.h"
#include "PlnGeneralObject.h"

#include <boost/assert.hpp>
#include "../PlnDataAllocator.h"
#include "../PlnGenerator.h"
#include "expressions/PlnMemCopy.h"

// PlnSingleObjectAllocator
PlnExpression* PlnSingleObjectAllocator::getAllocEx()
{
	PlnFunction *alloc_func = PlnFunctionCall::getInternalFunc(IFUNC_MALLOC);
	vector<PlnExpression*> args = { new PlnExpression(alloc_size) };
	PlnFunctionCall *alloc_call = new PlnFunctionCall(alloc_func, args);

	return alloc_call;
}

// PlnSingleObjectFreer
PlnExpression* PlnSingleObjectFreer::getFreeEx(PlnExpression* free_var)
{
	PlnFunction *free_func = PlnFunctionCall::getInternalFunc(IFUNC_FREE);
	vector<PlnExpression*> args = { free_var };
	PlnFunctionCall *free_call = new PlnFunctionCall(free_func, args);

	return free_call;
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
PlnExpression* PlnNoParamAllocator::getAllocEx()
{
	vector<PlnExpression*> args;
	PlnFunctionCall *alloc_call = new PlnFunctionCall(alloc_func, args);

	return alloc_call;
}

// PlnSingleParamFreer
PlnExpression* PlnSingleParamFreer::getFreeEx(PlnExpression* free_var)
{
	vector<PlnExpression*> args = { free_var };
	PlnFunctionCall *free_call = new PlnFunctionCall(free_func, args);

	return free_call;
}

// PlnDeepCopyFuncCall
class PlnDeepCopyFuncCall : public PlnDeepCopyExpression {
	PlnFunctionCall* fcall;
public:
	PlnDeepCopyFuncCall(PlnFunction* f)
		: PlnDeepCopyExpression(ET_FUNCCALL) {
			fcall = new PlnFunctionCall(f);
	}

	PlnDataPlace* dstDp(PlnDataAllocator &da) override {
		if (!fcall->arg_dps.size()) {
			BOOST_ASSERT(false);	// Should be call srcDp first.
//			vector<int> dtypes = { DT_OBJECT_REF, DT_OBJECT_REF };
//			fcall->loadArgDps(da, dtypes);
		}
		return fcall->arg_dps[0];
	}

	PlnDataPlace* srcDp(PlnDataAllocator &da) override {
		if (!fcall->arg_dps.size()) {
			vector<int> dtypes = { DT_OBJECT_REF, DT_OBJECT_REF };
			fcall->loadArgDps(da, dtypes);
		}
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
	PlnFunctionCall *free_call = new PlnFunctionCall(copy_func, args);

	return free_call;
}

PlnDeepCopyExpression* PlnTwoParamsCopyer::getCopyEx()
{
	return new PlnDeepCopyFuncCall(copy_func);
}
