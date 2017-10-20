/// x86-64 (Linux) assembly generator class declaration.
///
/// @file	PlnX86_64Generator.h
/// @copyright	2017- YAMAGUCHI Toshinobu 

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
	void genEntryFunc();
	void genLocalVarArea(int size);
	void genFreeLocalVarArea(int size);
	void genCCall(string& cfuncname);
	void genSysCall(int id, const string& comment);
	void genReturn();
	void genMainReturn();
	void genStringData(int index, const string& str);
	void genMove(const PlnGenEntity* dst, const PlnGenEntity* src, string comment);
	void genAdd(PlnGenEntity* tgt, PlnGenEntity* second);
	void genSub(PlnGenEntity* tgt, PlnGenEntity* second);
	void genNegative(PlnGenEntity* tgt);
	void genMul(PlnGenEntity* tgt, PlnGenEntity* second);
	void genDiv(PlnGenEntity* tgt, PlnGenEntity* second, string comment);

	void genNullClear(vector<unique_ptr<PlnGenEntity>> &refs);
	void genMemAlloc(PlnGenEntity* ref, int al_size, string& comment);
	void genMemFree(PlnGenEntity* ref, string& comment, bool doNull = true);
	void genMemCopy(int cp_size, string& comment);

	unique_ptr<PlnGenEntity> getPushEntity(PlnDataPlace* dp);
	unique_ptr<PlnGenEntity> getPopEntity(PlnDataPlace* dp);
	unique_ptr<PlnGenEntity> getEntity(PlnDataPlace* dp);
};

