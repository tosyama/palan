#include <vector>
#include <string>
#include <iostream>

class PlnGenerator;

class PlnFunction
{
public:
	std::string name;
	PlnFunction(const std::string& func_name);
	void gen(PlnGenerator &g);
};

class PlnModule
{
	bool is_main;
	std::vector<PlnFunction*> functions;
public:
	PlnModule();
	void addFunc(PlnFunction &func);
	void gen(PlnGenerator &g);
};
