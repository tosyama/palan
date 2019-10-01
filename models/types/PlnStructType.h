/// Structure type class declaration.
///
/// @file	PlnStructType.h
/// @copyright	2019 YAMAGUCHI Toshinobu 

class PlnStructMemberDef {
public:
	PlnType* type;
	string name;
	int offset;

	PlnStructMemberDef(PlnType *type, string name);
};

class PlnStructType : public PlnType {
public:
	vector<PlnStructMemberDef*> members;

	PlnStructType(const string &name, vector<PlnStructMemberDef*> &members, PlnBlock* parent);
	~PlnStructType();
	PlnTypeConvCap canConvFrom(PlnType *src) override;
};

