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
	vector<PlnExpression*> exps;

	PlnArrayLiteral(vector<PlnExpression*> &exps);
	PlnArrayLiteral(const PlnArrayLiteral& src);
	~PlnArrayLiteral();

	PlnType* getDefaultType(PlnModule *module);

	void adjustTypes(const vector<PlnType*> &types);
	PlnType* getType();
	PlnDataPlace* getDataPlace(PlnDataAllocator& da);

	/// return true - items is aixed array, false - not fixed array
	/// sizes - Detected array sizes. Note added 0 last. [2,3] is [2,3,0]
	/// item_type - Detected array element type. 
	/// depth - for internal process (recursive call)
	static bool isFixedArray(const vector<PlnObjectLiteralItem> &items, vector<int> &fixarr_sizes, PlnObjLitItemType &item_type, int depth=0);
};
