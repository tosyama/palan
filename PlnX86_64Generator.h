class PlnGenerator
{
public:
	virtual void genSecText(std::ostream &os)=0;
};

class PlnX86_64Generator : public PlnGenerator
{
public:
	void genSecText(std::ostream &os);
};
