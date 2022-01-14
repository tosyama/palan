/// Structure type class declaration.
///
/// @file	PlnStructType.h
/// @copyright	2019-2021 YAMAGUCHI Toshinobu 

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

	PlnStructTypeInfo(const string &name, vector<PlnStructMemberDef*> &members, PlnBlock* parent, const string& default_mode);
	~PlnStructTypeInfo();
	PlnTypeConvCap canCopyFrom(const string& mode, PlnVarType *src, PlnAsgnType copymode) override;
};

