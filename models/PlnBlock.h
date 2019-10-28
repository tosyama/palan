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

	//PlnVariable* declareVariable(const string& var_name, PlnVarType* var_type, bool readonly, bool is_owner, bool do_check_ancestor_blocks);
	PlnVariable* declareVariable(const string& var_name, PlnVarType* var_type, bool do_check_ancestor_blocks);
	PlnVariable* getVariable(const string& var_name);

	void declareConst(const string& name, PlnExpression *ex);	// throw PlnCompileError
	PlnExpression* getConst(const string& name);

	void declareType(const string& type_name);
	void declareType(const string& type_name, vector<PlnStructMemberDef*>& members);
	void declareAliasType(const string& type_name, PlnType* orig_type);

	PlnVarType* getType(const string& type_name, const string& mode);
	PlnVarType* getFixedArrayType(PlnVarType* item_type, vector<int>& sizes, const string& mode);

	PlnFunction* getFunc(const string& func_name, vector<PlnValue*> &arg_vals, vector<PlnValue*> &out_arg_vals); // throw PlnCompileError
	PlnFunction* getFuncProto(const string& func_name, vector<string>& param_types);

	void addFreeVars(vector<PlnExpression*> &free_vars, PlnDataAllocator& da, PlnScopeInfo& si);

	void finish(PlnDataAllocator& da, PlnScopeInfo& si);
	void gen(PlnGenerator& g);

	static string generateFuncName(string fname, vector<PlnType*> ret_types, vector<PlnType*> arg_types);
};
