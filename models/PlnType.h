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
			vector<int>* sizes;
			int item_size;
		} fixedarray;
	} inf;

	static vector<PlnType*> getBasicTypes();
	static PlnType* getSint();
	static PlnType* getUint();
	static PlnType* getReadOnlyCStr();
};
