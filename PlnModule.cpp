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

PlnFunction* PlnModule::getFunc(const string& func_name)
{
	for (auto f: functions)
		if (f->name == func_name) return f;
	
	return NULL;
}

PlnReadOnlyData* PlnModule::getReadOnlyData(string &str)
{
	for (auto d: readonlydata)
		if (d->type == RO_LIT_STR && d->name == str)
			return d;

	PlnReadOnlyData* rodata = new PlnReadOnlyData();
	rodata->type = RO_LIT_STR;
	rodata->index = readonlydata.size();
	rodata->name = str;
	readonlydata.push_back(rodata);

	return rodata;
}

void PlnModule::dump(ostream& os, string indent)
{
	os << indent << "Module: " << endl;
	os << indent << " Functions: " << functions.size() << endl;
	for (auto f: functions)
		f->dump(os, indent+"  ");
	os << indent << " Readonly Data: " << readonlydata.size() << endl;
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

