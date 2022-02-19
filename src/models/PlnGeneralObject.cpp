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

PlnDeepCopyExpression* PlnSingleObjectCopyer::getCopyEx()
{
	return new PlnMemCopy(NULL, NULL, new PlnExpression(len));
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
