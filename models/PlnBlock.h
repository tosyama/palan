/// Block model declaration.
///
/// @file	PlnBlock.h
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include "../PlnModel.h"

// Block: Statements

class PlnBlock {
public:
	vector<PlnStatement*> statements;
	vector<PlnVariable*> variables;
	PlnFunction* parent_func;
	PlnBlock* parent_block;

	PlnBlock();
	void setParent(PlnFunction* f);
	void setParent(PlnBlock* b);
	void startParse();
	void endParse();

	PlnVariable* declareVariable(string& var_name, PlnType* var_type=NULL);
	PlnVariable* getVariable(string& var_name);

	void finish();
	void dump(ostream& os, string indent="");
	void gen(PlnGenerator& g);
};
