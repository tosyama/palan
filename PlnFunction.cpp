#include "PlnModel.h"
#include "PlnX86_64Generator.h"

using namespace std;

PlnFunction::PlnFunction(const string &func_name)
	: name(func_name)
{
}

void PlnFunction::gen(PlnGenerator &g)
{
	g.genLabel(name);
}
