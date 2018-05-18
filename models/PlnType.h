/// Type model class declaration.
///
/// @file	PlnType.h
/// @copyright	2017 YAMAGUCHI Toshinobu 

#include "../PlnModel.h"

class PlnAllocator {
public:
	virtual PlnExpression* getAllocEx() = 0;
	static PlnExpression* getAllocEx(PlnVariable* var);
};

class PlnFreer {
public:
	virtual PlnExpression* getFreeEx(PlnExpression* free_var) = 0;
	static PlnExpression* getFreeEx(PlnVariable* var);
};

class PlnCopyer {
public:
	virtual PlnExpression* getCopyEx(PlnExpression* dst_var, PlnExpression* src_var) = 0;
};

class PlnType {
public:
	int	data_type;
	string name;
	int size;
	union {
		struct {
			bool is_fixed_size;
			int alloc_size;
		} obj;

		struct {
			bool is_fixed_size;
			int alloc_size;
			int item_size;
			vector<int>* sizes;
		} fixedarray;

	} inf;

	PlnAllocator *allocator;
	PlnFreer *freer;
	PlnCopyer *copyer;

	PlnType();

	static vector<PlnType*> getBasicTypes();
	static PlnType* getByte();
	static PlnType* getSint();
	static PlnType* getUint();
	static PlnType* getReadOnlyCStr();
	static PlnType* getObject();
	static PlnType* getRawArray();

	static string getFixedArrayName(PlnType* item_type, vector<int>& sizes);
};
