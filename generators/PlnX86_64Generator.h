/// x86-64 (Linux) assembly generator class declaration.
///
/// @file	PlnX86_64Generator.h
/// @copyright	2017-2018 YAMAGUCHI Toshinobu 

#include "../PlnGenerator.h"

enum PlnGenConstType {
	GCT_FLO32,
	GCT_FLO64
};

class PlnX86_64Generator : public PlnGenerator
{
	bool require_align;
	struct ConstInfo {
		PlnGenConstType type;
		union { double d; uint32_t ai[2]; } data;
		bool generated;
	};
	vector<ConstInfo> const_buf;
	
	void moveMemToReg(const PlnGenEntity* mem, int reg);
	void moveRegTo(int reg, const PlnGenEntity* dst);

	void genMoveFReg(const PlnGenEntity* src, const PlnGenEntity* dst);
	void genConvFMem(const PlnGenEntity* src, const PlnGenEntity* dst);
	void genConvIMem2FMem(const PlnGenEntity* src, const PlnGenEntity* dst);
	void genConvFMem2IMem(const PlnGenEntity* src, const PlnGenEntity* dst);
	void genConvFMem2IReg(const PlnGenEntity* src, const PlnGenEntity* dst);

	const char* genPreFloOperation(PlnGenEntity* tgt, PlnGenEntity* scnd);

	void genCmpImmFRegMem(const PlnGenEntity* first, const PlnGenEntity* second);
	void genCmpFMem(const PlnGenEntity* first, const PlnGenEntity* second);
	void genCmpFRegFMem(const PlnGenEntity* first, const PlnGenEntity* second);

	int genCmpI2F(const PlnGenEntity* first, const PlnGenEntity* second, int cmp_type);
	void genCmpIMemFRegMem(const PlnGenEntity* first, const PlnGenEntity* second);
	void genCmpIRegMemFImm(const PlnGenEntity* first, const PlnGenEntity* second);

	int registerConst(const PlnGenEntity* constValue);

public:
	PlnX86_64Generator(ostream& ostrm);
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
	void genStringData(int index, const string& str) override;
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

