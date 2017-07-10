#include <vector>
#include <string>
#include <iostream>

class PlnGenerator;

class PlnFunction
{
	std::string m_name;
public:
	PlnFunction(const std::string& name);
};

class PlnModule
{
	std::vector<PlnFunction*> functions;
public:
	void addFunc(PlnFunction &func);
	void gen(std::ostream &os, PlnGenerator &g);
};
