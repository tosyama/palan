/// Generic object utility declaration.
///
/// @file	PlnGeneralObject.h
/// @copyright	2018 YAMAGUCHI Toshinobu 

#include "../PlnModel.h"
#include "PlnType.h"

// for single object alloc/free/copy.
class PlnSingleObjectAllocator : public PlnAllocator {
	uint64_t alloc_size;
public:
	PlnSingleObjectAllocator(uint64_t alloc_size) : alloc_size(alloc_size) { }
	PlnExpression* getAllocEx() override;
};

class PlnSingleObjectFreer : public PlnFreer {
public:
	PlnExpression* getFreeEx(PlnExpression* free_var) override;
};

class PlnSingleObjectCopyer : public PlnCopyer {
	uint64_t len;
public:
	PlnSingleObjectCopyer(uint64_t len) : len(len) { }
	PlnExpression* getCopyEx(PlnExpression* dst_var, PlnExpression* src_var) override;
};

// for custom function call alloc/free/copy.
class PlnNoParamAllocator: public PlnAllocator
{
	PlnFunction *alloc_func;
public:
	PlnNoParamAllocator(PlnFunction *f) : alloc_func(f) { }
	PlnExpression* getAllocEx() override;
};

class PlnSingleParamFreer : public PlnFreer
{
	PlnFunction *free_func;
public:
	PlnSingleParamFreer(PlnFunction *f) : free_func(f) { }

	PlnExpression* getFreeEx(PlnExpression* free_var) override;
};

class PlnTwoParamsCopyer : public PlnCopyer
{
	PlnFunction *copy_func;
public:
	PlnTwoParamsCopyer(PlnFunction *f) : copy_func(f) { }
	PlnExpression* getCopyEx(PlnExpression* dst_var, PlnExpression* src_var);
};
