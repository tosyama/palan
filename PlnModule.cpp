#include <boost/assert.hpp>
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

void PlnModule::addReadOnlyData(PlnReadOnlyData& rodata)
{
	rodata.index = readonlydata.size();
	readonlydata.push_back(&rodata);
}

void PlnModule::gen(PlnGenerator &g)
{
	g.genSecReadOnlyData();
	for (auto rod: readonlydata)
		rod->gen(g);

	g.genSecText();
	if (is_main) g.genEntryPoint("_start");
	for (auto f : functions)
		f->gen(g);
}

