/// Array value type class declaration.
///
/// @file	PlnArrayValueType.h
/// @copyright	2019 YAMAGUCHI Toshinobu 

class PlnArrayLiteral;
class PlnArrayValueType : public PlnType {
public:
	PlnArrayLiteral* arr_lit;
	PlnArrayValueType(PlnArrayLiteral* arr_lit);
	PlnTypeConvCap canConvFrom(PlnType *src) override;

	PlnTypeConvCap checkCompatible(PlnType* item_type, const vector<int>& sizes);
};

