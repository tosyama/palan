/// Structure type class declaration.
///
/// @file	PlnStructType.h
/// @copyright	2019 YAMAGUCHI Toshinobu 

class PlnStructMemberDef {
public:
	PlnVarType* type;
	string name;
	int offset;
	bool is_ro_ref;

	PlnStructMemberDef(PlnVarType *type, string name, bool is_ro_ref);
};

class PlnStructType : public PlnType {
public:
	vector<PlnStructMemberDef*> members;

	PlnStructType(const string &name, vector<PlnStructMemberDef*> &members, PlnBlock* parent, const string& default_mode);
	PlnStructType(const PlnStructType* src, const string& mode);
	~PlnStructType();
	PlnTypeConvCap canConvFrom(PlnType *src) override;
	PlnType* getTypeWithMode(const string& mode) override;
};

