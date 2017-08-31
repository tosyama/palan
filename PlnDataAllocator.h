/// Register allocation class declaration.
///
/// @file	PlnDataAllocator.h
/// @copyright	2017- YAMAGUCHI Toshinobu

#include<vector>
using std::vector;

class PlnGenerator;

enum PlnPlcType {
	DP_REG,
	DP_VAR_STK,
	DP_TEMP_STK,
	DP_ARG_STK
};

enum PlnDPSignType {
	DPS_SIGNED,
	DPS_UNSIGNED,
	DPS_FLOAT
};

class PlnDataPlace
{ 
public:
	PlnDataPlace(int sign, int size, int psize)
		: sign(sign), size(size), place_size(psize) { }
	int type;
	int index;
	int size;

	int place_type;
	int place_index;
	int place_size;

	int sign;	// signed int, unsigned int, float
	int use;	// var, param, arg, work

	void* genEntity(PlnGenerator& g); // for data store to place
	void* gen(PlnGenerator& g);	// for get data (it place data)
};

class PlnDataAllocator
{
public:
	virtual void reset() = 0;

	virtual void pushArgDp(int index, PlnDataPlace* place) = 0;
	virtual PlnDataPlace* popArgDp(int index)=0;
	virtual void pushSysArgDp(int index, PlnDataPlace* place) = 0;
	virtual PlnDataPlace* popSysArgDp(int index)=0;
};
