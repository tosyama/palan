/// Structure type class declaration.
///
/// @file	PlnStructType.h
/// @copyright	2019 YAMAGUCHI Toshinobu 

class PlnStructMember {
public:
	PlnType* type;
	string name;
	int offset;

	PlnStructMember(PlnType *type, string name);
};

class PlnStructType : public PlnType {
public:
	vector<PlnStructMember*> members;

	PlnStructType(const string &name, vector<PlnStructMember*> &members);
	PlnTypeConvCap canConvFrom(PlnType *src) override;
};

