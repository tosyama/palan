/// Create clone expression class declaration.
///
/// @file	PlnClone.h
/// @copyright	2017 YAMAGUCHI Toshinobu 

#include "../PlnExpression.h"

class PlnClone : public PlnExpression
{
public:
	PlnExpression* clone_src;
	PlnExpression *alloc_ex, *copy_ex;
	PlnVariable* clone_var;

	PlnClone(PlnExpression* src);
	
	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void dump(ostream& os, string indent="") override;
	void gen(PlnGenerator& g) override;
};

