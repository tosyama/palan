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

enum GenEttyType {
	GE_STRING,
	GE_INT,
	GE_FLO
};

class PlnOperandInfo {
public:
	int type;
	PlnOperandInfo(int opetype) : type(opetype) { };
	virtual char* str(char* buf) { buf[0] = 0; return buf; };
	virtual ~PlnOperandInfo() {};
};

class PlnGenEntity {
public:
	PlnGenEntity() : buf(NULL), ope(NULL) { }
	char type;
	char alloc_type;
	char data_type;
	int size;

	mutable union {
		string* str;
		int64_t i;
		double f;
	} data;
	mutable char* buf;
	PlnOperandInfo* ope;
	~PlnGenEntity()
	{
		if (buf) delete buf;
		switch (type) {
			case GE_STRING:
				delete data.str;
				break;
		}
		if (ope)
			delete ope;
	}
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

	virtual void genCCall(string& cfuncname, vector<int> &arg_dtypes, int va_arg_start=-1)=0;
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

	void genLoadDp(PlnDataPlace* dp, bool load_save=true);

	// Save value to save area or store directory by case.
	void genSaveSrc(PlnDataPlace* dp);
	// Only save value to save area if indicated.
	void genSaveDp(PlnDataPlace* dp);

	virtual unique_ptr<PlnGenEntity> getEntity(PlnDataPlace* dp)=0;
};

