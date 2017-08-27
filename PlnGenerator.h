/// Interface of assembly generator class.
///
/// @file	PlnGenerator.h
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include <memory>

using std::vector;
using std::string;
using std::ostream;
using std::unique_ptr;

enum GenEttyType {
	GE_STRING,
	GE_INT
};

class PlnGenEntity {
public:
	int type;
	int alloc_type;
	int size;
	union {
		string* str;
		int i;
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
	virtual void genSecReadOnlyData()=0;
	virtual void genSecText()=0;
	virtual void genEntryPoint(const string& entryname)=0;
	virtual void genLabel(const string& label)=0;
	virtual void genEntryFunc() = 0;
	virtual void genLocalVarArea(int size)=0;
	virtual void genFreeLocalVarArea(int size)=0;
	virtual void genCCall(string& cfuncname)=0;
	virtual void genSysCall(int id, const string& comment)=0;
	virtual void genReturn()=0;
	virtual void genMainReturn()=0;
	virtual void genStringData(int index, const string& str)=0;
	virtual void genMove(const PlnGenEntity* dst, const PlnGenEntity* src, string comment)=0;
	virtual void genAdd(PlnGenEntity* dst, PlnGenEntity* src)=0;
	virtual void genSub(PlnGenEntity* dst, PlnGenEntity* src)=0;
	virtual void genNegative(PlnGenEntity* tgt)=0;

	virtual unique_ptr<PlnGenEntity> getNull() = 0;
	virtual unique_ptr<PlnGenEntity> getInt(int i)=0;
	virtual unique_ptr<PlnGenEntity> getStackAddress(int offset, int size)=0;
	virtual unique_ptr<PlnGenEntity> getStrAddress(int index)=0;
	virtual unique_ptr<PlnGenEntity> getArgument(int i, int size)=0;
	virtual unique_ptr<PlnGenEntity> getSysArgument(int i)=0;
	virtual unique_ptr<PlnGenEntity> getWork(int i)=0;
};

