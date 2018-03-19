/// Interface of assembly generator class.
///
/// @file	PlnGenerator.h
/// @copyright	2017 YAMAGUCHI Toshinobu 

#include <memory>

using std::vector;
using std::string;
using std::ostream;
using std::unique_ptr;

class PlnDataPlace;

enum GenEttyType {
	GE_STRING,
	GE_INT
};

class PlnGenEntity {
public:
	char type;
	char alloc_type;
	char data_type;
	int size;

	union {
		string* str;
		int64_t i;
	}data;
	~PlnGenEntity()
	{
		switch (type) {
			case GE_STRING:
				delete data.str;
				break;
		}
	}
};

class PlnGenerator
{
protected:
	ostream& os;
public:
	PlnGenerator(ostream& ostrm) : os(ostrm) {};
	void comment(const string s) { os << "#" << s << endl; }
	void blank() { os << endl; }

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
	virtual void genFreeLocalVarArea(int size)=0;
	
	virtual void genSaveReg(int reg, PlnGenEntity* dst)=0;
	virtual void genLoadReg(int reg, PlnGenEntity* src)=0;

	virtual void genCCall(string& cfuncname)=0;
	virtual void genSysCall(int id, const string& comment)=0;
	virtual void genReturn()=0;
	virtual void genMainReturn()=0;
	virtual void genStringData(int index, const string& str)=0;
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
	virtual void genMemAlloc(PlnGenEntity* ref, int al_size, string comment)=0;
	virtual void genMemFree(PlnGenEntity* ref, string& comment, bool doNull=true)=0;
	virtual void genMemCopy(int cp_size, string& comment)=0;

	void genLoadDp(PlnDataPlace* dp);
	void genSaveSrc(PlnDataPlace* dp);

	virtual unique_ptr<PlnGenEntity> getEntity(PlnDataPlace* dp)=0;
};

