/// Block model declaration.
///
/// @file	PlnBlock.h
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include "../PlnModel.h"

// Block: Statements
enum PlnBlkPrntType {
	BP_FUNC,
	BP_BLOCK
};

class PlnBlock {
public:
	vector<PlnStatement*> statements;
	vector<PlnVariable*> variables;
	PlnBlkPrntType parent_type;
	union {
		PlnFunction* function;
		PlnBlock* block;
	} parent;
	int cur_stack_size;

	PlnBlock();

	int totalStackSize();
	PlnVariable* getVariable(string& var_name);

	PlnVariable* declareVariable(string& var_name, PlnType* var_type=NULL);
	void setParent(PlnScopeItem& scope);

	void finish();
	void dump(ostream& os, string indent="");
	void gen(PlnGenerator& g);
};
