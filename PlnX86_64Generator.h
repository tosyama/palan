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
	virtual void genSysCall(int id, vector<PlnGenEntity*> &args, const string& comment)=0;
	virtual void genMainRetun(vector<PlnGenEntity*> &return_vals)=0;
	virtual void genStringData(int index, const string& str)=0;
	virtual void genMove(PlnGenEntity* dst, PlnGenEntity* src, string& comment)=0;

	virtual PlnGenEntity* getInt(int i)=0;
	virtual PlnGenEntity* getStackAddress(int offset)=0;
	virtual PlnGenEntity* getStrAddress(int index)=0;
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
	void genSysCall(int id, vector<PlnGenEntity*> &args, const string& comment);
	void genMainRetun(vector<PlnGenEntity*> &return_vals);
	void genStringData(int index, const string& str);
	void genMove(PlnGenEntity* dst, PlnGenEntity* src, string& comment);

	PlnGenEntity* getInt(int i);
	PlnGenEntity* getStackAddress(int offset);
	PlnGenEntity* getStrAddress(int index);
};
