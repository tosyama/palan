/// x86-64 (Linux) assembly generator class declaration.
///
/// @file	PlnX86_64Generator.h
/// @copyright	2017 YAMAGUCHI Toshinobu 

#include "../PlnGenerator.h"

class PlnX86_64Generator : public PlnGenerator
{
	void moveMemToReg(const PlnGenEntity* mem, int reg);

public:
	PlnX86_64Generator(ostream& ostrm);
	void genSecReadOnlyData();
	void genSecText();
	void genEntryPoint(const string& entryname);
	void genLabel(const string& label);
	void genJumpLabel(int id, string comment);
	void genJump(int id, string comment);
	void genFalseJump(int id, int cmp_type, string comment);
	void genEntryFunc();
	void genLocalVarArea(int size);
	void genFreeLocalVarArea(int size);

	void genSaveReg(int reg, PlnGenEntity* dst);
	void genLoadReg(int reg, PlnGenEntity* src);

	void genCCall(string& cfuncname);
	void genSysCall(int id, const string& comment);
	void genReturn();
	void genMainReturn();
	void genStringData(int index, const string& str);
	void genMove(const PlnGenEntity* dst, const PlnGenEntity* src, string comment);
	void genAdd(PlnGenEntity* tgt, PlnGenEntity* second, string comment);
	void genSub(PlnGenEntity* tgt, PlnGenEntity* second, string comment);
	void genNegative(PlnGenEntity* tgt, string comment);
	void genMul(PlnGenEntity* tgt, PlnGenEntity* second, string comment);
	void genDiv(PlnGenEntity* tgt, PlnGenEntity* second, string comment);
	int genCmp(PlnGenEntity* first, PlnGenEntity* second, int cmp_type, string comment);
	int genMoveCmpFlag(PlnGenEntity* tgt, int cmp_type, string comment);

	void genNullClear(vector<unique_ptr<PlnGenEntity>> &refs);
	void genMemAlloc(PlnGenEntity* ref, int al_size, string& comment);
	void genMemFree(PlnGenEntity* ref, string& comment, bool doNull = true);
	void genMemCopy(int cp_size, string& comment);

	unique_ptr<PlnGenEntity> getPushEntity(PlnDataPlace* dp);
	unique_ptr<PlnGenEntity> getPopEntity(PlnDataPlace* dp);
	unique_ptr<PlnGenEntity> getEntity(PlnDataPlace* dp);
};

