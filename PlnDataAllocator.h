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

class PlnDataAllocator
{
protected:
	int regnum;
	int step;

	void allocDataWithDetail(PlnDataPlace* dp, int alloc_step, int release_step);
	virtual PlnDataPlace* createArgDp(int func_type, int index, bool is_callee) = 0;
	virtual vector<int> getRegsNeedSave()=0;

public:
	int stack_size;

	vector<PlnDataPlace*> data_stack;
	vector<PlnDataPlace*> arg_stack;
	vector<PlnDataPlace*> regs;
	vector<PlnDataPlace*> sub_dbs;
	vector<PlnDataPlace*> all;

	vector<PlnDataPlace*> release_stmt_end;

	void reset();
	PlnDataAllocator(int regnum);

	PlnDataPlace* allocData(int size, int data_type);
	void allocData(PlnDataPlace* new_dp);

	void allocSaveData(PlnDataPlace* dp);
	void releaseData(PlnDataPlace* dp);

	void allocDp(PlnDataPlace *Dp);
	vector<PlnDataPlace*> prepareArgDps(int ret_num, int arg_num, int func_type, bool is_callee);
	vector<PlnDataPlace*> prepareRetValDps(int ret_num, int func_type, bool is_callee);
	virtual void funcCalled(vector<PlnDataPlace*>& args, vector<PlnVariable*>& rets, int func_type) = 0;
	virtual void returnedValues(vector<PlnDataPlace*>& ret_dps, int func_type) = 0;

	// Process register data may be breaken by this process.
	virtual void memAlloced() = 0;
	virtual void memFreed() = 0;
	virtual void prepareMemCopyDps(PlnDataPlace* &dst, PlnDataPlace* &src) = 0;
	virtual void memCopyed(PlnDataPlace* dst, PlnDataPlace* src) = 0;

	virtual PlnDataPlace* allocAccumulator(PlnDataPlace* dp) = 0;
	virtual void releaseAccumulator(PlnDataPlace* dp) = 0;
	virtual bool isAccumulator(PlnDataPlace* dp) = 0;
	virtual PlnDataPlace* multiplied(PlnDataPlace* tgt) = 0;
	virtual void divided(PlnDataPlace** quotient, PlnDataPlace** reminder) = 0;

	// for array item
	virtual PlnDataPlace* prepareObjBasePtr() = 0;
	virtual PlnDataPlace* prepareObjIndexPtr() = 0;
	virtual void setIndirectObjDp(PlnDataPlace* dp, PlnDataPlace* base_dp, PlnDataPlace* index_dp);

	PlnDataPlace* getLiteralIntDp(int64_t intValue);
	PlnDataPlace* getReadOnlyDp(int index);
	PlnDataPlace* getSeparatedDp(PlnDataPlace* dp);

	void pushSrc(PlnDataPlace* dp, PlnDataPlace* src_dp);
	void popSrc(PlnDataPlace* dp);

	void finish(vector<int>& save_regs, vector<PlnDataPlace*>& save_reg_dps);
};

enum {
	DP_UNKNOWN,
	DP_STK_BP,
	DP_STK_SP,
	DP_REG,
	DP_BYTES,

	DP_INDRCT_OBJ,
	DP_LIT_INT,
	DP_RO_DATA,

	DP_SUBDP
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
	char type;
	char size;
	char data_type;
	char status;

	int32_t accessCount;

	int32_t alloc_step;
	int32_t release_step;

	union {
		struct {int32_t idx; int32_t offset;} stack;
		struct {int32_t idx; int32_t offset;} bytes;
		struct {int32_t id; int32_t offset;} reg;
		struct {int32_t displacement; PlnDataPlace* base_dp; PlnDataPlace* index_dp;
				int16_t base_id; int16_t index_id; } indirect;
		vector<PlnDataPlace*> *bytesData;
		int64_t intValue;
		int index;
		PlnDataPlace *originalDp;
	} data;

	PlnDataPlace* previous;
	PlnDataPlace* save_place;
	PlnDataPlace* src_place;
	string* comment;

	PlnDataPlace(int size, int data_type);
	unsigned int getAllocBytesBits();
	bool tryAllocBytes(PlnDataPlace* dp);

	string cmt() { return *comment; }
	int allocable_size();
	void access();
};
