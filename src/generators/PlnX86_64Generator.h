/// x86-64 (Linux) assembly generator class declaration.
///
/// @file	PlnX86_64Generator.h
/// @copyright	2017-2020 YAMAGUCHI Toshinobu 

#include "../PlnGenerator.h"
#include "PlnX86_64RegisterMachine.h"

class PlnX86_64Generator : public PlnGenerator
{
	PlnX86_64RegisterMachine m;
	bool require_align;
	int max_const_id;
	struct ConstInfo {
		int id;	// -1: no id
		int generated;	// 0: not generated, >0: generaed
		int8_t size;	// integer:1,2,3,4,8  // string:0 
		int8_t alignment;
		union {
			int64_t i;
			string *str;
		} data;
		string* comment;

		ConstInfo(int8_t size, int8_t alignment, int64_t data)
			: id(-1), generated(0), comment(NULL), size(size), alignment(alignment)
			{ this->data.i = data; }
		ConstInfo(int id, string* str)
			: id(id), generated(0), comment(NULL), size(0), alignment(8)
			{ this->data.str = str; }
	};
	vector<ConstInfo> const_buf;
	int func_stack_size;
	
	int registerString(string &string);
	int registerConstData(vector<PlnRoData> &rodata);

public:
	PlnX86_64Generator(ostream& ostrm);
	~PlnX86_64Generator();
	void comment(const string& s) override;

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

	void genCCall(string& cfuncname, vector<int> &arg_dtypes, bool has_va_arg) override;
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

