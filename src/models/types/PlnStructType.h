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

class PlnStructType : public PlnType {
public:
	vector<PlnStructMemberDef*> members;
	bool has_object_member;

	PlnStructType(const string &name, vector<PlnStructMemberDef*> &members, PlnBlock* parent, const string& default_mode);
	~PlnStructType();
	PlnTypeConvCap canCopyFrom(const string& mode, PlnVarType *src, PlnAsgnType copymode) override;
};

