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

class PlnDeepCopyExpression;
class PlnCopyer {
public:
	virtual PlnExpression* getCopyEx(PlnExpression* dst_var, PlnExpression* src_var) = 0;
	virtual PlnDeepCopyExpression* getCopyEx() = 0;
};

enum PlnTypeConvCap {
	TC_CANT_CONV = 0,
	TC_SAME = 1,
	TC_AUTO_CAST = 2,
	TC_LOSTABLE_AUTO_CAST = 4,
	TC_UP_CAST = 8,
	TC_DOWN_CAST = 16,
	TC_CONV_OK = TC_SAME | TC_AUTO_CAST | TC_LOSTABLE_AUTO_CAST | TC_UP_CAST | TC_DOWN_CAST
};

enum PlnTypeConvCap {
	TC_CANT_CONV = 0,
	TC_SAME = 1,
	TC_AUTO_CAST = 2,
	TC_LOSTABLE_AUTO_CAST = 4,
	TC_UP_CAST = 8,
	TC_DOWN_CAST = 16,
	TC_CONV_OK = TC_SAME | TC_AUTO_CAST | TC_LOSTABLE_AUTO_CAST | TC_UP_CAST | TC_DOWN_CAST
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

	struct PlnTypeConvInf {
		PlnType *type;
		PlnTypeConvCap capacity;
		PlnTypeConvInf(PlnType* t, PlnTypeConvCap cap) : type(t), capacity(cap) { }
	};
	vector<PlnTypeConvInf> conv_inf;

	PlnType();
	PlnTypeConvCap canConvFrom(PlnType *src);

	static vector<PlnType*> getBasicTypes();
	static PlnType* getByte();
	static PlnType* getSint();
	static PlnType* getUint();
	static PlnType* getReadOnlyCStr();
	static PlnType* getObject();
	static PlnType* getRawArray();

	static string getFixedArrayName(PlnType* item_type, vector<int>& sizes);
};
