/// Type model class declaration.
///
/// @file	PlnType.h
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include "../PlnModel.h"

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

	static vector<PlnType*> getBasicTypes();
	static PlnType* getSint();
	static PlnType* getUint();
	static PlnType* getReadOnlyCStr();
};
