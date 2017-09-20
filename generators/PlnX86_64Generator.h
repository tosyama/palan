/// x86-64 (Linux) assembly generator class declaration.
///
/// @file	PlnX86_64Generator.h
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include "../PlnGenerator.h"

class PlnX86_64Generator : public PlnGenerator
{
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
	void genAdd(PlnGenEntity* dst, PlnGenEntity* src);
	void genSub(PlnGenEntity* dst, PlnGenEntity* src);
	void genNegative(PlnGenEntity* tgt);

	unique_ptr<PlnGenEntity> getInt(int i);
	unique_ptr<PlnGenEntity> getStackAddress(int offset, int size);
	unique_ptr<PlnGenEntity> getStrAddress(int index);
	unique_ptr<PlnGenEntity> getArgument(int i, int size);
	unique_ptr<PlnGenEntity> getSysArgument(int i);

	unique_ptr<PlnGenEntity> getPushEntity(PlnDataPlace* dp);
	unique_ptr<PlnGenEntity> getPopEntity(PlnDataPlace* dp);
	unique_ptr<PlnGenEntity> getEntity(PlnDataPlace* dp);
};

