/// Generic object utility declaration.
///
/// @file	PlnGeneralObject.h
/// @copyright	2018 YAMAGUCHI Toshinobu 

#include "../PlnModel.h"
#include "PlnType.h"

class PlnSingleObjectCopyer : public PlnCopyer {
	uint64_t len;
public:
	PlnSingleObjectCopyer(uint64_t len) : len(len) { }
	PlnExpression* getCopyEx(PlnExpression* dst_var, PlnExpression* src_var) override;
	PlnDeepCopyExpression* getCopyEx() override;
};

class PlnTwoParamsCopyer : public PlnCopyer
{
	PlnFunction *copy_func;
public:
	PlnTwoParamsCopyer(PlnFunction *f) : copy_func(f) { }
	PlnExpression* getCopyEx(PlnExpression* dst_var, PlnExpression* src_var);
	PlnDeepCopyExpression* getCopyEx() override;
};

