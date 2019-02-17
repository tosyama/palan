/// Object literal class declaration.
///
/// @file	PlnObjectLiteral.h
/// @copyright	2019 YAMAGUCHI Toshinobu 

enum PlnObjLitItemType {
	OLI_ARRAY,
	OLI_SINT,
	OLI_UINT,
	OLI_FLO,
//	OLI_STR
};

class PlnArrayLiteral;
class PlnObjectLiteralItem
{
public:
	PlnObjLitItemType type;
	union {
		PlnArrayLiteral* a;
		int64_t i;
		uint64_t u;
		double f;
	} data;

	PlnObjectLiteralItem(const PlnObjectLiteralItem &src);
	PlnObjectLiteralItem(PlnArrayLiteral *arr)
		: type(OLI_ARRAY) { data.a = arr; };
	PlnObjectLiteralItem(int64_t i)
		: type(OLI_SINT) { data.i = i; };
	PlnObjectLiteralItem(uint64_t u)
		: type(OLI_UINT) { data.u = u; };
	PlnObjectLiteralItem(double f)
		: type(OLI_FLO) { data.f = f; };
};

class PlnArrayLiteral
{
public:
	PlnType* arr_type;
	vector<PlnObjectLiteralItem> arr;

	PlnArrayLiteral(vector<PlnObjectLiteralItem> &arr)
		: arr_type(NULL) { this->arr = move(arr); }

	PlnArrayLiteral(const PlnArrayLiteral& src);

	vector<PlnType*> getDefaultType(PlnModule *module);
	vector<int> getArraySizes();

	void adjustTypes(const vector<PlnType*> &types);
	PlnType* getType();
	PlnDataPlace* getDataPlace(PlnDataAllocator& da);
};
