/// Fixed array type class declaration.
///
/// @file	PlnFixedArrayType.h
/// @copyright	2019 YAMAGUCHI Toshinobu 

class PlnFixedArrayType : public PlnType {
public:
	PlnFixedArrayType();
	PlnType* item_type;
	PlnTypeConvCap canConvFrom(PlnType *src) override;
};