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

	void allocDataWithDetail(PlnDataPlace* dp, int alloc_step, int release_step);
	virtual PlnDataPlace* createReturnDp(int func_type, const vector<int> &ret_dtypes, const vector<int> &arg_dtypes, int index, bool is_callee) = 0;
	virtual vector<int> getRegsNeedSave()=0;
	bool isDestroyed(PlnDataPlace* dp);

public:
	int step;
	int stack_size;

	vector<PlnDataPlace*> data_stack;
	vector<PlnDataPlace*> arg_stack;
	vector<PlnDataPlace*> regs;
	vector<PlnDataPlace*> sub_dbs;
	vector<PlnDataPlace*> all;

	void reset();
	PlnDataAllocator(int regnum);

	PlnDataPlace* prepareLocalVar(int size, int data_type);
	PlnDataPlace* allocData(int size, int data_type);
	void allocData(PlnDataPlace* new_dp);
	void allocSaveData(PlnDataPlace* dp, int alloc_step, int release_step);

	void allocDp(PlnDataPlace *Dp, bool proceed_step = true);
	void releaseDp(PlnDataPlace* dp);
	virtual PlnDataPlace* createArgDp(int func_type, const vector<int> &ret_dtypes, const vector<int> &arg_dtypes, int index, bool is_callee) = 0;
	virtual void funcCalled(vector<PlnDataPlace*>& args, vector<PlnVariable*>& rets, int func_type) = 0;
	vector<PlnDataPlace*> prepareArgDps(int func_type, const vector<int> &ret_dtypes, const vector<int> &arg_dtypes, bool is_callee);
	vector<PlnDataPlace*> prepareRetValDps(int func_type, vector<int> &ret_dtypes, vector<int> &arg_dtypes, bool is_callee);

	// Process register data may be breaken by this process.
	virtual void prepareMemCopyDps(PlnDataPlace* &dst, PlnDataPlace* &src, PlnDataPlace* &len) = 0;
	virtual void memCopyed(PlnDataPlace* dst, PlnDataPlace* src, PlnDataPlace* len) = 0;

	virtual PlnDataPlace* prepareAccumulator(int data_type) = 0;
	virtual PlnDataPlace* added(PlnDataPlace* ldp, PlnDataPlace *rdp) = 0;
	virtual PlnDataPlace* multiplied(PlnDataPlace* ldp, PlnDataPlace* rdp) = 0;
	virtual void divided(PlnDataPlace** quotient, PlnDataPlace** reminder, PlnDataPlace* ldp, PlnDataPlace* rdp) = 0;

	// for array item
	virtual PlnDataPlace* prepareObjBasePtr() = 0;
	virtual PlnDataPlace* prepareObjIndexPtr() = 0;
	virtual void setIndirectObjDp(PlnDataPlace* dp, PlnDataPlace* base_dp, PlnDataPlace* index_dp);

	PlnDataPlace* getLiteralIntDp(int64_t intValue);
	PlnDataPlace* getReadOnlyDp(int index);
	PlnDataPlace* getSeparatedDp(PlnDataPlace* dp);

	void pushSrc(PlnDataPlace* dp, PlnDataPlace* src_dp, bool release_src_pop=true);
	void popSrc(PlnDataPlace* dp);

	void finish(vector<int>& save_regs, vector<PlnDataPlace*>& save_reg_dps);

	// for debug
	virtual void checkDataLeak();
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
	DS_UNKNOWN,
	DS_RELEASED,
	DS_CALLEE_PAR,
	DS_ASSIGNED,
	DS_ASSIGNED_SOME,
	DS_READY_ASSIGN
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
	int32_t push_src_step;
	bool release_src_pop;
	bool load_address;
	bool do_clear_src;

	union {
		struct {int32_t idx; int32_t offset;} stack;
		struct {int32_t idx; int32_t offset; PlnDataPlace* parent_dp; } bytes;
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
	unsigned int getAllocBytesBits(int alloc_step, int release_step);
	bool tryAllocBytes(PlnDataPlace* dp);

	string cmt() { return *comment; }
	void updateBytesDpStatus();
	void access();
};
