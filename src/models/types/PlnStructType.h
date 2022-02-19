/// Structure type class declaration.
///
/// @file	PlnStructType.h
/// @copyright	2019-2022 YAMAGUCHI Toshinobu 

class PlnStructMemberDef {
public:
	PlnVarType* type;
	string name;
	int offset;

	PlnStructMemberDef(PlnVarType *type, const string& name);
};

class PlnStructTypeInfo : public PlnTypeInfo {
public:
	vector<PlnStructMemberDef*> members;
	bool has_heap_member;
	PlnFunction* alloc_func;
	PlnFunction* internal_alloc_func;
	PlnFunction* free_func;
	PlnFunction* internal_free_func;

	PlnStructTypeInfo(const string &name, vector<PlnStructMemberDef*> &members, PlnBlock* parent, const string& default_mode);
	~PlnStructTypeInfo();

	PlnTypeConvCap canCopyFrom(const string& mode, PlnVarType *src, PlnAsgnType copymode) override;
	PlnVarType* getVarType(const string& mode) override;
};

class PlnStructVarType : public PlnVarType {
public:
	PlnStructVarType(PlnTypeInfo* typeinf, const string &mode) : PlnVarType(typeinf, mode) {}
	~PlnStructVarType() {};

	PlnExpression *getAllocEx(vector<PlnExpression*> &args) override;
	PlnExpression *getInternalAllocEx(vector<PlnExpression*> &args) override;
	PlnExpression *getFreeEx(vector<PlnExpression*> &args) override; 
	PlnExpression *getInternalFreeEx(vector<PlnExpression*> &args) override; 
};
