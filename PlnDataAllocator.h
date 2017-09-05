/// Register/Stack allocation class declaration.
///
/// @file	PlnDataAllocator.h
/// @copyright	2017- YAMAGUCHI Toshinobu

#include <vector>
#include <stdint.h>
using std::vector;

class PlnGenerator;

class PlnDataPlace;
class PlnDataDeliver;
class PlnParameter;
class PlnVariable;

class PlnDataAllocator
{
	int regnum;

public:
	vector<PlnDataPlace*> data_stack;
	vector<PlnDataPlace*> arg_stack;
	vector<PlnDataPlace*> regs;

	int step;

	PlnDataAllocator(int regnum);

	PlnDataPlace* allocData(int size);
	void releaseData(PlnDataPlace* dp);

	void prepareDataDeliver(PlnDataDeliver* dd, PlnDataPlace* dp);
	void pushData(PlnDataDeliver* dd);
	void popData(PlnDataDeliver* dd);

	virtual vector<PlnDataPlace*> allocArgs(vector<PlnParameter*>& params, vector<PlnVariable*>& rets) = 0;
	virtual void funcCalled(vector<PlnDataPlace*>& args, vector<PlnVariable*>& rets) = 0;
};

enum {
	DP_STK_BP,
	DP_STK_SP,
	DP_REG,
	DP_BYTES
};

enum {
	DS_RELEASED,
	DS_CALLEE_PAR,
	DS_ASSIGNED,
	DS_ASSIGNED_SOME,
	DS_ARGUMENT,
	DS_DESTROYED
};

class PlnDataPlace
{
public:
	int type;
	int size;

	int status;
	int accessCount;

	int alloc_step;
	int release_step;

	union {
		struct {int32_t idx; int32_t offset;} stack;
		struct {int32_t id; int32_t offset;} reg;
		vector<PlnDataPlace*> *bytesData;
	} data;

	PlnDataPlace* previous;
	PlnDataPlace* save_place;

	int allocable_size();
	void access();
};

