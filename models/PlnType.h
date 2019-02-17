/// Type model class declaration.
///
/// @file	PlnType.h
/// @copyright	2017-2019 YAMAGUCHI Toshinobu 

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

enum PlnTypeType {
	TP_PRIMITIVE,
	TP_FIXED_ARRAY
};

enum PlnObjectType {
	OT_UNKNOWN,
	OT_FIXED_ARRAY
};

class PlnType {
public:
	PlnTypeType type;
	int	data_type;
	string name;
	int size;
	PlnObjectType obj_type;
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

	PlnType(PlnTypeType type=TP_PRIMITIVE);
	PlnTypeConvCap canConvFrom(PlnType *src);

	static vector<PlnType*> getBasicTypes();
//	static PlnType* getByte();	// not use now
	static PlnType* getSint();
	static PlnType* getUint();
	static PlnType* getFlo();
	static PlnType* getReadOnlyCStr();
	static PlnType* getObject();
	static PlnType* getRawArray();

	static string getFixedArrayName(PlnType* item_type, vector<int>& sizes);
};
