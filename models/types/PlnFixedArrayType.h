/// Fixed array type class declaration.
///
/// @file	PlnFixedArrayType.h
/// @copyright	2019 YAMAGUCHI Toshinobu 

class PlnFixedArrayType : public PlnType {
public:
	PlnVarType* item_type;
	vector<int> sizes;

	PlnFixedArrayType(string &name, PlnVarType* item_type, vector<int>& sizes, PlnBlock* parent);
//	PlnFixedArrayType(const PlnFixedArrayType* src, const string& mode);
	PlnTypeConvCap canConvFrom(const string& mode, PlnVarType *src) override;
};
