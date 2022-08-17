/// Array value type class declaration.
///
/// @file	PlnArrayValueType.h
/// @copyright	2019-2020 YAMAGUCHI Toshinobu 

class PlnArrayValue;
class PlnArrayValueTypeInfo : public PlnTypeInfo {
public:
	PlnArrayValue *arr_val;
	PlnArrayValueTypeInfo(PlnArrayValue* arr_val);
	PlnTypeConvCap canCopyFrom(const string &mode, PlnVarType *src, PlnAsgnType copymode) override;

	PlnVarType* getDefaultType(PlnBlock *block);
	vector<int> getArraySizes();

	PlnTypeConvCap checkCompatible(PlnVarType* item_type, const vector<int>& sizes);
};
