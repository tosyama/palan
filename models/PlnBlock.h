/// Block model declaration.
///
/// @file	PlnBlock.h
/// @copyright	2017 YAMAGUCHI Toshinobu 

#include "../PlnModel.h"

// Block: Statements
class PlnBlock {
public:
	vector<PlnStatement*> statements;
	vector<PlnVariable*> variables;
	vector<PlnExpression*> free_vars;
	PlnFunction* parent_func;
	PlnBlock* parent_block;

	PlnBlock();
	void setParent(PlnFunction* f);
	void setParent(PlnBlock* b);

	PlnVariable* declareVariable(string& var_name, vector<PlnType*>& var_types, bool is_owner);
	PlnVariable* getVariable(string& var_name);

	void finish(PlnDataAllocator& da, PlnScopeInfo& si);
	void dump(ostream& os, string indent="");
	void gen(PlnGenerator& g);
};
