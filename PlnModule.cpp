#include "PlnModel.h"
#include "PlnX86_64Generator.h"

using namespace std;

void PlnModule::addFunc(PlnFunction &func)
{
	functions.push_back(&func);
}

void PlnModule::gen(std::ostream &os, PlnGenerator &g)
{
	g.genSecText(os);
}
