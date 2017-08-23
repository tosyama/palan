/// Module model class definition.
///
/// Module is root of model tree.
/// Module includes function definitions.
///
/// @file	PlnModule.cpp
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include <unordered_map>
#include <boost/assert.hpp>
#include "PlnModule.h"
#include "PlnFunction.h"
#include "PlnType.h"
#include "PlnVariable.h"
#include "../PlnGenerator.h"

using namespace std;

PlnModule::PlnModule() 
{
}

PlnType* PlnModule::getType(const string& type_name)
{
	return PlnType::getBasicType(type_name);
}

PlnFunction* PlnModule::getFunc(const string& func_name, vector<PlnExpression*>& args)
{
	for (auto f: functions)
		if (f->name == func_name) {
			// Check arguments.
			if (f->parameters.size()==0 && args.size()==0)
				return f;
			int i=0;
			bool ng = false; 
			for (auto p: f->parameters) {
				if (i+1>args.size() || !args[i]) {
					if (!p->dflt_value) {
						ng = true; break;
					}
				} else {
					//TODO: type check.
				}
				++i;
			}
			if (!ng) {
				// Set default.
				i=0;
				for (auto p: f->parameters) {
					if (i+1>args.size()) 
						args.push_back(new PlnExpression(*p->dflt_value));
					else if(!args[i])
						args[i] = new PlnExpression(*p->dflt_value);
					++i;
				}
				return f;
			}
		}
	
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

void PlnModule::finish()
{
	for (auto f: functions)
		f->finish();
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

