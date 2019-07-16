/// Interface of assembly generator class.
///
/// @file	PlnGenerator.h
/// @copyright	2017-2019 YAMAGUCHI Toshinobu 

#include <memory>

using std::vector;
using std::string;
using std::ostream;
using std::unique_ptr;

class PlnDataPlace;

class PlnOperandInfo {
public:
	int type;
	PlnOperandInfo(int opetype) : type(opetype) { };
	virtual ~PlnOperandInfo() {};
	virtual PlnOperandInfo* clone() = 0;
	virtual const char* str(char* buf) = 0;
};

class PlnGenEntity {
public:
	char type;
	char data_type;
	int size;
	PlnOperandInfo* ope;

	PlnGenEntity() : ope(NULL) { }
	PlnGenEntity(const PlnGenEntity&) = delete;
	~PlnGenEntity() { delete ope; }
};

class PlnGenerator
{
protected:
	ostream& os;
public:
	PlnGenerator(ostream& ostrm) : os(ostrm) {}
	virtual ~PlnGenerator() {}

	virtual void comment(const string s) = 0;
	void blank() { comment(""); }

	virtual void genSecReadOnlyData()=0;
	virtual void genSecText()=0;
	virtual void genEntryPoint(const string& entryname)=0;
	virtual void genLabel(const string& label)=0;
	virtual void genJumpLabel(int id, string comment) = 0;
	virtual void genJump(int id, string comment) = 0;
	virtual void genTrueJump(int id, int cmp_type, string comment) = 0;
	virtual void genFalseJump(int id, int cmp_type, string comment) = 0;
	virtual void genEntryFunc() = 0;
	virtual void genLocalVarArea(int size)=0;
	virtual void genEndFunc() = 0;
	
	virtual void genSaveReg(int reg, PlnGenEntity* dst)=0;
	virtual void genLoadReg(int reg, PlnGenEntity* src)=0;

	virtual void genCCall(string& cfuncname, vector<int> &arg_dtypes, bool has_va_arg)=0;
	virtual void genSysCall(int id, const string& comment)=0;
	virtual void genReturn()=0;
	virtual void genMainReturn()=0;
	virtual void genMove(const PlnGenEntity* dst, const PlnGenEntity* src, string comment)=0;
	virtual void genLoadAddress(const PlnGenEntity* dst, const PlnGenEntity* src, string comment)=0;

	virtual void genAdd(PlnGenEntity* tgt, PlnGenEntity* second, string comment)=0;
	virtual void genSub(PlnGenEntity* tgt, PlnGenEntity* second, string comment)=0;
	virtual void genNegative(PlnGenEntity* tgt, string comment)=0;
	virtual void genMul(PlnGenEntity* tgt, PlnGenEntity* second, string comment)=0;
	virtual void genDiv(PlnGenEntity* tgt, PlnGenEntity* second, string comment)=0;
	virtual int genCmp(PlnGenEntity* first, PlnGenEntity* second, int cmp_type, string comment)=0;
	virtual int genMoveCmpFlag(PlnGenEntity* tgt, int cmp_type, string comment)=0;
	
	virtual void genNullClear(vector<unique_ptr<PlnGenEntity>> &refs) = 0;
	virtual void genMemCopy(int cp_unit, string& comment)=0;

	virtual unique_ptr<PlnGenEntity> getEntity(PlnDataPlace* dp)=0;

	// How to use.
	// 1. call genSaveSrc(dp) with source value dp first at expression that provide src value.
	// 2. Case 1) call genLoadDp(dp, true) to load the source value to dp immediately.
	//    Case 2) 1. genLoadDp(dp, false); load to dp that only not have save dp for preventing override registors.
	//    		  2. genSaveDp(dp, false); load to dp that only have save dp. This call don't override registors.

	// Save value to save area or store directory by case.
	void genSaveSrc(PlnDataPlace* dp);

	// Load value.
	void genLoadDp(PlnDataPlace* dp, bool load_save=true);
	// Load only from save value.
	void genSaveDp(PlnDataPlace* dp);
};

