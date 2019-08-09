/// Block model declaration.
///
/// @file	PlnBlock.h
/// @copyright	2017-2019 YAMAGUCHI Toshinobu 

#include "../PlnModel.h"
#include "PlnExpression.h"

class PlnStructMemberDef;

// Block: Statements
class PlnBlock {
public:
	vector<PlnStatement*> statements;
	vector<PlnVariable*> variables;
	vector<PlnExpression*> free_vars;
	struct PlnConst {
		string name;
		PlnExpression *ex;
	};
	vector<PlnConst> consts;
	vector<PlnType*> types;
	vector<PlnFunction*> funcs;
	
	PlnModule* parent_module;
	PlnFunction* parent_func;
	PlnBlock* parent_block;
	PlnStatement* owner_stmt;
	PlnLoc loc;

	PlnBlock();
	~PlnBlock();

	void setParent(PlnFunction* f);
	void setParent(PlnBlock* b);

	PlnVariable* declareVariable(const string& var_name, PlnType* var_type, bool is_owner);
	PlnVariable* getVariable(const string& var_name);

	void declareConst(const string& name, PlnExpression *ex);	// throw PlnCompileError
	PlnExpression* getConst(const string& name);

	void declareType(const string& type_name);
	void declareType(const string& type_name, vector<PlnStructMemberDef*>& members);
	PlnType* getType(const string& type_name);
	PlnType* getFixedArrayType(PlnType* item_type, vector<int>& sizes);

	PlnFunction* getFunc(const string& func_name, vector<PlnValue*> &arg_vals, vector<PlnValue*> &out_arg_vals); // throw PlnCompileError
	PlnFunction* getFuncProto(const string& func_name, vector<string>& param_types);

	void addFreeVars(vector<PlnExpression*> &free_vars, PlnDataAllocator& da, PlnScopeInfo& si);

	void finish(PlnDataAllocator& da, PlnScopeInfo& si);
	void gen(PlnGenerator& g);
};
