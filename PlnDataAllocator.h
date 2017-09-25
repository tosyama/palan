/// Register/Stack allocation class declaration.
///
/// @file	PlnDataAllocator.h
/// @copyright	2017- YAMAGUCHI Toshinobu

#include <cstddef>
#include <vector>
#include <stdint.h>
#include <string>
using std::vector;
using std::string;

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

	PlnDataPlace* allocDataWithDetail(int size, int alloc_step, int release_step, PlnDataPlace* new_dp);
	virtual PlnDataPlace* createArgDp(int func_type, int index, bool is_callee) = 0;

public:
	int stack_size;

	vector<PlnDataPlace*> data_stack;
	vector<PlnDataPlace*> arg_stack;
	vector<PlnDataPlace*> regs;
	vector<PlnDataPlace*> all;

	void reset();
	PlnDataAllocator(int regnum);

	PlnDataPlace* allocData(int size, PlnDataPlace* new_dp = NULL);

	void allocSaveData(PlnDataPlace* dp);
	void releaseData(PlnDataPlace* dp);

	void allocDp(PlnDataPlace *Dp);
	vector<PlnDataPlace*> prepareArgDps(int ret_num, int arg_num, int func_type, bool is_callee);
	vector<PlnDataPlace*> prepareRetValDps(int ret_num, int func_type, bool is_callee);
	virtual void funcCalled(vector<PlnDataPlace*>& args, vector<PlnVariable*>& rets, int func_type) = 0;
	virtual void returnedValues(vector<PlnDataPlace*>& ret_dps, int func_type) = 0;

	virtual PlnDataPlace* allocAccumulator(PlnDataPlace* dp) = 0;
	virtual void releaseAccumulator(PlnDataPlace* dp) = 0;
	virtual bool isAccumulator(PlnDataPlace* dp) = 0;
	virtual void multiplied() = 0;
	virtual void divided() = 0;

	PlnDataPlace* getLiteralIntDp(int intValue);
	PlnDataPlace* getReadOnlyDp(int index);

	void finish();
};

enum {
	DP_STK_BP,
	DP_STK_SP,
	DP_REG,
	DP_BYTES,

	DP_LIT_INT,
	DP_RO_DATA
};

enum {
	DS_RELEASED,
	DS_CALLEE_PAR,
	DS_ASSIGNED,
	DS_ASSIGNED_SOME,
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
		int intValue;
		int index;
	} data;

	PlnDataPlace* previous;
	PlnDataPlace* save_place;
	string* comment;

	PlnDataPlace();
	string cmt() { return (!save_place) ? *comment : *comment + *save_place->comment; }
	int allocable_size();
	void access();
};

