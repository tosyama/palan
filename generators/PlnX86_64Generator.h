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
	void genCCall(string& cfuncname);
	void genSysCall(int id, const string& comment);
	void genReturn();
	void genMainReturn();
	void genStringData(int index, const string& str);
	void genMove(PlnGenEntity* dst, PlnGenEntity* src, string comment);
	void genAdd(PlnGenEntity* dst, PlnGenEntity* src);
	void genSub(PlnGenEntity* dst, PlnGenEntity* src);
	void genNegative(PlnGenEntity* tgt);

	PlnGenEntity* getNull();
	PlnGenEntity* getInt(int i);
	PlnGenEntity* getStackAddress(int offset);
	PlnGenEntity* getStrAddress(int index);
	PlnGenEntity* getArgument(int i);
	PlnGenEntity* getSysArgument(int i);
	PlnGenEntity* getWork(int i);
};

