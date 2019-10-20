/// Structure type class declaration.
///
/// @file	PlnStructType.h
/// @copyright	2019 YAMAGUCHI Toshinobu 

class PlnStructMemberDef {
public:
	PlnType* type;
	string name;
	int offset;
	bool is_ro_ref;

	PlnStructMemberDef(PlnType *type, string name, bool is_ro_ref);
};

class PlnStructType : public PlnType {
public:
	vector<PlnStructMemberDef*> members;

	PlnStructType(const string &name, vector<PlnStructMemberDef*> &members, PlnBlock* parent);
	PlnStructType(const PlnStructType* src, const string& mode);
	~PlnStructType();
	PlnTypeConvCap canConvFrom(PlnType *src) override;
	PlnType* getTypeWithMode(const string& mode) override;
};

