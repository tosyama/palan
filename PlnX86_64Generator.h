using std::string;
using std::ostream;

class PlnGenerator
{
protected:
	ostream& os;
public:
	PlnGenerator(ostream& ostrm) : os(ostrm) {};
	virtual void genSecText()=0;
	virtual void genEntryPoint(const string& entryname)=0;
	virtual void genLabel(const string& label)=0;
	virtual void genSysCall(int id, const string& comment)=0;
};

class PlnX86_64Generator : public PlnGenerator
{
public:
	PlnX86_64Generator(ostream& ostrm);
	void genSecText();
	void genEntryPoint(const string& entryname);
	void genLabel(const string& label);
	void genSysCall(int id, const string& comment);
};
