/// PlnChainCall model class declaration.
///
/// @file	PlnChainCall.h
/// @copyright	2018 YAMAGUCHI Toshinobu 

#include "../PlnExpression.h"

class PlnFunctionCall;
class PlnDeepCopyExpression;
class PlnCloneArg;

class PlnChainCall : public PlnExpression
{
public:
	PlnFunctionCall* fcall;
	vector<PlnExpression*> args;
	vector<PlnCloneArg*> clones;

	PlnChainCall(PlnFunction* f, vector<PlnExpression*> &in_args, vector<PlnExpression*> &args);

	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void gen(PlnGenerator& g) override;
};
