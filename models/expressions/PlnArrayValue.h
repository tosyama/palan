/// Object Array value class declaration.
///
/// @file	PlnArrayValue.h
/// @copyright	2019 YAMAGUCHI Toshinobu 

#include "../PlnExpression.h"

class PlnArrayValue : public PlnExpression
{
public:
	vector<PlnExpression *> item_exps;
	bool isLiteral;
	bool doCopyFromStaticBuffer;
	
	PlnArrayValue(vector<PlnExpression*> &exps, bool isLiteral);
	PlnArrayValue(const PlnArrayValue& src);

	PlnExpression* adjustTypes(const vector<PlnVarType*> &types) override;

	void finish(PlnDataAllocator& da, PlnScopeInfo& si) override;
	void gen(PlnGenerator& g) override;

	vector<PlnExpression*> getAllItems();
	PlnDataPlace* getROArrayDp(PlnDataAllocator& da);	// for PlnExpression

	/// return true - items is aixed array, false - not fixed array
	/// sizes - Detected array sizes. Note added 0 last. [2,3] is [2,3,0]
	/// item_type - Detected array element type. 
	/// depth - for internal process (recursive call)
	static bool isFixedArray(const vector<PlnExpression*> &items, vector<int> &fixarr_sizes, int &item_type, int depth=0);
};

