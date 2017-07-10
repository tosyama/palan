class PlnGenerator
{
protected:
	std::ostream& os;
public:
	PlnGenerator(std::ostream& ostrm) : os(ostrm) {};
	virtual void genSecText()=0;
	virtual void genEntryPoint(const std::string& entryname)=0;
	virtual void genLabel(const std::string& label)=0;
};

class PlnX86_64Generator : public PlnGenerator
{
public:
	PlnX86_64Generator(std::ostream& ostrm);
	void genSecText();
	void genEntryPoint(const std::string& entryname);
	void genLabel(const std::string& label);
};
