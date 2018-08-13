/// Block model declaration.
///
/// @file	PlnBlock.h
/// @copyright	2017 YAMAGUCHI Toshinobu 

#include "../PlnModel.h"
#include "PlnExpression.h"

// Block: Statements
class PlnBlock {
public:
	vector<PlnStatement*> statements;
	vector<PlnVariable*> variables;
	vector<PlnExpression*> free_vars;
	struct PlnConst {
		string name;
		PlnValue value;
	};
	vector<PlnConst> consts;
	
	PlnFunction* parent_func;
	PlnBlock* parent_block;
	PlnLoc loc;

	PlnBlock();
	void setParent(PlnFunction* f);
	void setParent(PlnBlock* b);

	PlnVariable* declareVariable(const string& var_name, vector<PlnType*>& var_types, bool is_owner);
	PlnVariable* getVariable(const string& var_name);

	// true: success, false: duplicate
	bool declareConst(const string& name, PlnValue value);
	PlnExpression* getConst(const string& name);

	void finish(PlnDataAllocator& da, PlnScopeInfo& si);
	void gen(PlnGenerator& g);
};
