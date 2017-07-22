using std::vector;
using std::string;
using std::ostream;

class PlnGenEntity {
public:
	int type;
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
	virtual void genSysCall(int id, vector<PlnGenEntity*> &args, const string& comment)=0;
	virtual void genMainRetun(vector<PlnGenEntity*> &return_vals)=0;
	virtual void genStringData(int index, const string& str)=0;

	virtual PlnGenEntity* getInt(int i)=0;
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
	void genSysCall(int id, vector<PlnGenEntity*> &args, const string& comment);
	void genMainRetun(vector<PlnGenEntity*> &return_vals);
	void genStringData(int index, const string& str);

	PlnGenEntity* getInt(int i);
	PlnGenEntity* getStrAddress(int index);
};
