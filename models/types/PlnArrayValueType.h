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

	//PlnType* getDefaultType(PlnModule *module);
	PlnType* getDefaultType(PlnBlock *block);
	vector<int> getArraySizes();

	PlnTypeConvCap checkCompatible(PlnVarType* item_type, const vector<int>& sizes);
};
