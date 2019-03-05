/// Array value type class declaration.
///
/// @file	PlnArrayValueType.h
/// @copyright	2019 YAMAGUCHI Toshinobu 

class PlnArrayLiteral;
class PlnArrayValue;
class PlnArrayValueType : public PlnType {
public:
	PlnArrayLiteral *arr_lit;
	PlnArrayValue *arr_val;
	PlnArrayValueType(PlnArrayLiteral* arr_lit);
	PlnArrayValueType(PlnArrayValue* arr_val);
	PlnTypeConvCap canConvFrom(PlnType *src) override;

	PlnTypeConvCap checkCompatible(PlnType* item_type, const vector<int>& sizes);
};

