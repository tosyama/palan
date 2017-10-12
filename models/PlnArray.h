/// Array model class declaration.
///
/// @file	PlnArray.h
/// @copyright	2017 YAMAGUCHI Toshinobu 

#include "../PlnModel.h"

class PlnArray
{
public:
	int dim;
	vector<int>	ar_sizes;
	vector<PlnType*> ar_types;
	PlnDataPlace* item_num_dp;

	void finish(PlnDataAllocator& da);
};
