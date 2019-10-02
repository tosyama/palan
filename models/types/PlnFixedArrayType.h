/// Fixed array type class declaration.
///
/// @file	PlnFixedArrayType.h
/// @copyright	2019 YAMAGUCHI Toshinobu 

class PlnFixedArrayType : public PlnType {
public:
	PlnType* item_type;
	vector<int> sizes;

	PlnFixedArrayType(string &name, PlnType* item_type, vector<int>& sizes, PlnBlock* parent);
	PlnTypeConvCap canConvFrom(PlnType *src) override;
};
