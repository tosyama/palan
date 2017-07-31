#include <unordered_map>
#include <boost/assert.hpp>
#include "PlnModel.h"
#include "PlnX86_64Generator.h"

using namespace std;

// Basic types
static bool is_initialzed_type = false;
static unordered_map<string, PlnType*> basic_types;

PlnModule::PlnModule() 
{
	if (is_initialzed_type) return;
	
	PlnType* t = new PlnType();
	t->type = TP_INT8;
	t->name = "int";
	t->size = 8;
	basic_types[t->name] = t;

	is_initialzed_type = true;

}

PlnType* PlnModule::getType(const string& type_name)
{
	unordered_map<string, PlnType*>::const_iterator t = basic_types.find(type_name);
	if (t != basic_types.end())
		return t->second;
	else
		return NULL;
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
	os << indent << " Readonly Data: " << readonlydata.size() << endl;
	os << indent << " Functions: " << functions.size() << endl;
	for (auto f: functions)
		f->dump(os, indent+"  ");
}

void PlnModule::gen(PlnGenerator &g)
{
	g.genSecReadOnlyData();
	for (auto rod: readonlydata)
		rod->gen(g);

	g.genSecText();
	for (auto f : functions)
		f->gen(g);
}

