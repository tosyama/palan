/// x86-64 (Linux) assembly generator class declaration.
///
/// @file	PlnX86_64Generator.h
/// @copyright	2017-2019 YAMAGUCHI Toshinobu 

#include "../PlnGenerator.h"

enum PlnGenConstType {
	GCT_FLO32,
	GCT_FLO64,
	GCT_INT8_ARRAY,
	GCT_INT16_ARRAY,
	GCT_INT32_ARRAY,
	GCT_INT64_ARRAY,
	GCT_STRING
};

class PlnX86_64Generator : public PlnGenerator
{
	bool require_align;
	struct ConstInfo {
		PlnGenConstType type;
		union {
			double d; int64_t q;
			int64_t* q_arr; int32_t* l_arr; int16_t* s_arr; int8_t* b_arr;
			string* str;
		} data;
		bool generated;
		int size;
	};
	vector<ConstInfo> const_buf;
	
	void moveMemToReg(const PlnGenEntity* mem, int reg);
	void moveRegTo(int reg, const PlnGenEntity* dst);

	void genMoveFReg(const PlnGenEntity* src, const PlnGenEntity* dst);
	void genConvFMem(const PlnGenEntity* src, const PlnGenEntity* dst);
	void genConvIMem2FMem(const PlnGenEntity* src, const PlnGenEntity* dst);
	void genConvFMem2IMem(const PlnGenEntity* src, const PlnGenEntity* dst);
	void genConvFMem2IReg(const PlnGenEntity* src, const PlnGenEntity* dst);

	PlnOperandInfo* genPreFloOperation(PlnGenEntity* tgt, PlnGenEntity* scnd, const char** res);

	void genCmpImmFRegMem(const PlnGenEntity* first, const PlnGenEntity* second);
	void genCmpFMem(const PlnGenEntity* first, const PlnGenEntity* second);
	void genCmpFRegFMem(const PlnGenEntity* first, const PlnGenEntity* second);

	int genCmpI2F(const PlnGenEntity* first, const PlnGenEntity* second, int cmp_type);
	void genCmpIMemFRegMem(const PlnGenEntity* first, const PlnGenEntity* second);
	void genCmpIRegMemFImm(const PlnGenEntity* first, const PlnGenEntity* second);

	int registerConst(const PlnGenEntity* constValue);
	int registerConstArray(vector<int64_t> &int_array, int item_size);
	int registerString(string &string);

public:
	PlnX86_64Generator(ostream& ostrm);
	void comment(const string s) override;

	void genSecReadOnlyData() override;
	void genSecText() override;
	void genEntryPoint(const string& entryname) override;
	void genLabel(const string& label) override;
	void genJumpLabel(int id, string comment) override;
	void genJump(int id, string comment) override;
	void genTrueJump(int id, int cmp_type, string comment) override;
	void genFalseJump(int id, int cmp_type, string comment) override;
	void genEntryFunc() override;
	void genLocalVarArea(int size) override;
	void genEndFunc() override;

	void genSaveReg(int reg, PlnGenEntity* dst) override;
	void genLoadReg(int reg, PlnGenEntity* src) override;

	void genCCall(string& cfuncname, vector<int> &arg_dtypes, int va_arg_start) override;
	void genSysCall(int id, const string& comment) override;
	void genReturn() override;
	void genMainReturn() override;
	void genMove(const PlnGenEntity* dst, const PlnGenEntity* src, string comment) override;
	void genLoadAddress(const PlnGenEntity* dst, const PlnGenEntity* src, string comment);

	void genAdd(PlnGenEntity* tgt, PlnGenEntity* second, string comment) override;
	void genSub(PlnGenEntity* tgt, PlnGenEntity* second, string comment) override;
	void genNegative(PlnGenEntity* tgt, string comment) override;
	void genMul(PlnGenEntity* tgt, PlnGenEntity* second, string comment) override;
	void genDiv(PlnGenEntity* tgt, PlnGenEntity* second, string comment) override;
	int genCmp(PlnGenEntity* first, PlnGenEntity* second, int cmp_type, string comment) override;
	int genMoveCmpFlag(PlnGenEntity* tgt, int cmp_type, string comment) override;

	void genNullClear(vector<unique_ptr<PlnGenEntity>> &refs) override;
	void genMemCopy(int cp_unit, string& comment) override;

	unique_ptr<PlnGenEntity> getEntity(PlnDataPlace* dp) override;
};

