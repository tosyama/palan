#include "PlnModel.h"
#include "PlnX86_64Generator.h"

using namespace std;

PlnModule::PlnModule() :is_main(false)
{}

void PlnModule::addFunc(PlnFunction &func)
{
	if (func.name == "main") is_main = true;
	functions.push_back(&func);
}

void PlnModule::gen(PlnGenerator &g)
{
	g.genSecText();
	if (is_main) g.genEntryPoint("_start");
	for (auto f : functions)
		f->gen(g);
}
