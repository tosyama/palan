/// Array value type class declaration.
///
/// @file	PlnArrayValueType.h
/// @copyright	2019 YAMAGUCHI Toshinobu 

class PlnArrayValue;
class PlnArrayValueType : public PlnType {
public:
	PlnArrayValue *arr_val;
	PlnArrayValueType(PlnArrayValue* arr_val);
	PlnTypeConvCap canConvFrom(PlnType *src) override;

	PlnType* getDefaultType(PlnModule *module);
	vector<int> getArraySizes();

	PlnTypeConvCap checkCompatible(PlnType* item_type, const vector<int>& sizes);
};
