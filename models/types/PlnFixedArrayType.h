/// Fixed array type class declaration.
///
/// @file	PlnFixedArrayType.h
/// @copyright	2019 YAMAGUCHI Toshinobu 

class PlnFixedArrayType : public PlnType {
public:
	PlnVarType* item_type;
	vector<int> sizes;

	PlnFixedArrayType(string &name, PlnVarType* item_type, vector<int>& sizes, PlnBlock* parent);
	PlnTypeConvCap canCopyFrom(const string& mode, PlnVarType *src) override;
};
