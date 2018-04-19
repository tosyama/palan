/// Type model class declaration.
///
/// @file	PlnType.h
/// @copyright	2017 YAMAGUCHI Toshinobu 

#include "../PlnModel.h"

class PlnFreer {
public:
	virtual PlnExpression* getFreeEx(PlnExpression* free_var) = 0;
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

	PlnFreer *freer;

	PlnType();

	static vector<PlnType*> getBasicTypes();
	static PlnType* getByte();
	static PlnType* getSint();
	static PlnType* getUint();
	static PlnType* getReadOnlyCStr();
	static PlnType* getObject();
	static PlnType* getRawArray();
};
