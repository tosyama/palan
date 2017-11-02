/// Module model class definition.
///
/// Module is root of model tree.
/// Module includes function definitions.
///
/// @file	PlnModule.cpp
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include <algorithm>
#include <boost/assert.hpp>
#include "PlnModule.h"
#include "PlnBlock.h"
#include "PlnFunction.h"
#include "PlnType.h"
#include "PlnVariable.h"
#include "../PlnDataAllocator.h"
#include "../PlnGenerator.h"
#include "../PlnScopeStack.h"
#include "../PlnConstants.h"

using namespace std;

PlnModule::PlnModule()
{
	types = PlnType::getBasicTypes();
	toplevel = new PlnBlock();
	stack_size = 0;
}

PlnType* PlnModule::getType(const string& type_name)
{
	auto t = std::find_if(types.begin(), types.end(),
		[type_name](PlnType* t) { return t->name == type_name; });
	return (t != types.end()) ? *t : NULL; 
}

PlnType* PlnModule::getFixedArrayType(int item_size, vector<int>& sizes)
{
	for (auto t: fixedarray_types) {
		auto as = t->inf.fixedarray.sizes;
		if (item_size == t->inf.fixedarray.item_size && sizes.size() == as->size()) {
			bool found = true;
			for (int i=0; i<sizes.size(); ++i) {
				if (sizes[i] != (*as)[i]) {
					found = false;
					break;
				}
			}
			if (found) return t;
		}
	}

	int alloc_size = item_size;
	for (int s: sizes)
		alloc_size *= s;

	auto t = new PlnType();
	t->name = "[]";
	t->data_type = DT_OBJECT_REF;
	t->size = 8;
	t->inf.obj.is_fixed_size = true;
	t->inf.obj.alloc_size = alloc_size;
	t->inf.fixedarray.sizes = new vector<int>(move(sizes));
	t->inf.fixedarray.item_size = item_size;

	fixedarray_types.push_back(t);

	return t;
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

void PlnModule::finish(PlnDataAllocator& da)
{
	PlnScopeInfo si;
	for (auto f: functions) {
		f->finish(da, si);
		da.reset();
	}
	BOOST_ASSERT(si.scope.size() == 0);
	BOOST_ASSERT(si.owner_vars.size() == 0);
	toplevel->finish(da, si);
	da.finish();
	stack_size = da.stack_size;
}

void PlnModule::dump(ostream& os, string indent)
{
	os << indent << "Module: " << endl;
	os << indent << " Readonly Data: " << readonlydata.size() << endl;
	os << indent << " Functions: " << functions.size() << endl;
	for (auto f: functions)
		f->dump(os, indent+"  ");
	os << indent << " Top Level Code: " << endl;
	toplevel->dump(os, indent+ "  "); 
}

void PlnModule::gen(PlnGenerator &g)
{
	g.genSecReadOnlyData();
	for (auto rod: readonlydata)
		rod->gen(g);

	g.genSecText();
	for (auto f : functions)
		f->gen(g);
	
	string s="";
	g.genEntryPoint(s);
	g.genLabel(s);
	g.genEntryFunc();
	g.genLocalVarArea(stack_size);
	
	toplevel->gen(g);

	g.genMainReturn();
}	

