/// Type Casting class declaration.
///
/// @file	PlnTypeCast.h
/// @copyright	2023 YAMAGUCHI Toshinobu 

#include "../PlnExpression.h"

class PlnTypeCast : public PlnExpression
{
public:
	PlnExpression* exp;

	PlnTypeCast(PlnExpression* exp, vector<PlnVarType*> var_types);
	~PlnTypeCast();
	
	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void gen(PlnGenerator& g) override;
};
