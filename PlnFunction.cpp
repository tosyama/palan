#include "PlnModel.h"
#include "PlnX86_64Generator.h"

using std::string;

PlnFunction::PlnFunction(const string &func_name)
	: name(func_name)
{
}

void PlnFunction::addParam(PlnVariable& param)
{
	parameters.push_back(&param);
}

void PlnFunction::gen(PlnGenerator &g)
{
	g.genLabel(name);
}
