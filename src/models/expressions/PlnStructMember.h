/// PlnStructMember model class declaration.
///
/// @file	PlnStructMember.h
/// @copyright	2019 YAMAGUCHI Toshinobu 

#include "../PlnExpression.h"

class PlnStructMemberDef;
class PlnStructMember : public PlnExpression
{
public:
	PlnExpression* struct_ex;
	PlnStructMemberDef* def;

	PlnStructMember(PlnExpression* sturct_ex, string member_name);
	~PlnStructMember();
	
	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void gen(PlnGenerator& g) override;

	static vector<PlnExpression*> getAllStructMembers(PlnVariable* var);
};

