/// Register/Stack allocation class declaration.
///
/// @file	PlnDataAllocator.h
/// @copyright	2017- YAMAGUCHI Toshinobu

#include <vector>
#include <stdint.h>
using std::vector;

class PlnDataPlace;
class PlnParameter;
class PlnVariable;

enum {	// Function call type.
	DPF_PLN,
	DPF_C,
	DPF_SYS
};

class PlnDataAllocator
{
protected:
	int regnum;
	int step;
public:
	int stack_size;

	vector<PlnDataPlace*> data_stack;
	vector<PlnDataPlace*> arg_stack;
	vector<PlnDataPlace*> regs;
	vector<PlnDataPlace*> all;

	void reset();
	PlnDataAllocator(int regnum);

	PlnDataPlace* allocDataWithDetail(int size, int alloc_step, int release_step);
	PlnDataPlace* allocData(int size);

	void allocSaveData(PlnDataPlace* dp);
	void releaseData(PlnDataPlace* dp);

	virtual vector<PlnDataPlace*> allocArgs(vector<PlnParameter*>& params, vector<PlnVariable*>& rets, int func_type = DPF_PLN) = 0;
	virtual void funcCalled(vector<PlnDataPlace*>& args, vector<PlnVariable*>& rets, int func_type = DPF_PLN) = 0;

	void finish();
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

