using std::vector;
using std::string;
using std::ostream;

class PlnGenEntity {
public:
	int type;
	int alloc_type;
	union {
		string* str;
	}data;
	static void freeEntity(PlnGenEntity* e);
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
	virtual void genCCall(string& cfuncname)=0;
	virtual void genSysCall(int id, const string& comment)=0;
	virtual void genReturn()=0;
	virtual void genMainReturn()=0;
	virtual void genStringData(int index, const string& str)=0;
	virtual void genMove(PlnGenEntity* dst, PlnGenEntity* src, string comment)=0;
	virtual void genAdd(PlnGenEntity* dst, PlnGenEntity* src)=0;
	virtual void genSub(PlnGenEntity* dst, PlnGenEntity* src)=0;
	virtual void genNegative(PlnGenEntity* tgt)=0;

	virtual PlnGenEntity* getNull() = 0;
	virtual PlnGenEntity* getInt(int i)=0;
	virtual PlnGenEntity* getStackAddress(int offset)=0;
	virtual PlnGenEntity* getStrAddress(int index)=0;
	virtual PlnGenEntity* getArgument(int i)=0;
	virtual PlnGenEntity* getSysArgument(int i)=0;
	virtual PlnGenEntity* getWork(int i)=0;
};


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
