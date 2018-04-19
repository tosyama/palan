/// Module model class definition.
///
/// Module is root of model tree.
/// Module includes function definitions.
///
/// @file	PlnModule.cpp
/// @copyright	2017- YAMAGUCHI Toshinobu 

#include <algorithm>
#include <boost/assert.hpp>
#include <boost/algorithm/string.hpp>
#include "PlnModule.h"
#include "PlnBlock.h"
#include "PlnFunction.h"
#include "PlnType.h"
#include "PlnVariable.h"
#include "PlnStatement.h"
#include "../PlnDataAllocator.h"
#include "../PlnGenerator.h"
#include "../PlnScopeStack.h"
#include "../PlnConstants.h"
#include "../PlnBuildTreeHelper.h"
#include "expressions/PlnFunctionCall.h"
#include "expressions/PlnArrayItem.h"

using namespace std;

PlnModule::PlnModule()
{
	types = PlnType::getBasicTypes();
	toplevel = new PlnBlock();
	stack_size = 0;
	max_jmp_id = -1;
}

PlnType* PlnModule::getType(const string& type_name)
{
	auto t = std::find_if(types.begin(), types.end(),
		[type_name](PlnType* t) { return t->name == type_name; });
	return (t != types.end()) ? *t : NULL; 
}

class PlnSingleOjectFreer : public PlnFreer {
public:
	PlnExpression* getFreeEx(PlnExpression* free_var) override {
		PlnFunction *free_func = PlnFunctionCall::getInternalFunc(IFUNC_FREE);
		vector<PlnExpression*> args = { free_var };
		PlnFunctionCall *free_call = new PlnFunctionCall(free_func, args);
		
		return free_call;
	}
};

string getFreeFuncName(PlnType *t)
{

	string fname = t->name;
	boost::replace_all(fname, "_", "__");
	boost::replace_all(fname, "[", "A_");
	boost::replace_all(fname, "]", "_");
	boost::replace_all(fname, ",", "_");
	fname = "_fr_" + fname;
	return fname;
}

PlnFunction* createObjArrayFreeFunc(string func_name, vector<PlnType*> &arr_type)
{
	PlnType* t = arr_type.back();
	PlnType* it = arr_type[arr_type.size()-2];
	int item_num = t->inf.obj.alloc_size / t->inf.fixedarray.item_size;

	PlnFunction* f = new PlnFunction(FT_PLN, func_name);
	string s1 = "p1";
	f->addParam(s1, &arr_type, FPM_REF, NULL);

	f->implement = new PlnBlock();
	f->implement->setParent(f);

	// add free code.
	PlnVariable* i = palan::declareUInt(f->implement, "i", 0);
	PlnBlock* wblock = palan::whileLess(f->implement, i, item_num);
	{
		BOOST_ASSERT(it->freer);
		PlnExpression* arr_item = palan::rawArrayItem(f->parameters[0], i);
		PlnExpression* free_item = it->freer->getFreeEx(arr_item);
		wblock->statements.push_back(new PlnStatement(free_item, wblock));

		palan::incrementUInt(wblock, i, 1);
	}

	palan::free(f->implement, f->parameters[0]);

	return f;
}

class PlnSingleParamFreer : public PlnFreer
{
	PlnFunction *free_func;

public:
	PlnSingleParamFreer(PlnFunction *f)
		: free_func(f) {
	}

	PlnExpression* getFreeEx(PlnExpression* free_var) override {
		vector<PlnExpression*> args = { free_var };
		PlnFunctionCall *free_call = new PlnFunctionCall(free_func, args);
		
		return free_call;
	}
};

PlnType* PlnModule::getFixedArrayType(vector<PlnType*> &item_type, vector<int>& sizes)
{
	PlnType* it = item_type.back();
	int item_size = it->size;
	string name = "]";
	for (int s: sizes) {
		name = "," + to_string(s) + name;
	}
	name.front() = '[';
	name = it->name + name;

	for (auto t: fixedarray_types) 
		if (name == t->name) return t;

	int alloc_size = item_size;
	for (int s: sizes)
		alloc_size *= s;

	auto t = new PlnType();
	t->name = name;
	t->data_type = DT_OBJECT_REF;
	t->size = 8;
	t->inf.obj.is_fixed_size = true;
	t->inf.obj.alloc_size = alloc_size;
	t->inf.fixedarray.sizes = new vector<int>(move(sizes));
	t->inf.fixedarray.item_size = item_size;

	// set freer
	if (it->data_type != DT_OBJECT_REF) {
		t->freer = new PlnSingleOjectFreer();
	} else {
		string fname = getFreeFuncName(t);
		PlnFunction* free_func = NULL;
		for (auto f: functions) {
			if (f->name == fname) {
				free_func = f;
				break;
			}
		}
		if (!free_func) {
			vector<PlnType*> type_def = item_type;
			type_def.push_back(t);
			free_func = createObjArrayFreeFunc(fname, type_def);
			functions.push_back(free_func);
		}
		t->freer = new PlnSingleParamFreer(free_func);
	}

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

int PlnModule::getJumpID()
{
	return ++max_jmp_id;
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
	si.scope.push_back(PlnScopeItem(this));
	
	for (auto f: functions) {
		f->finish(da, si);
		da.reset();
	}

	BOOST_ASSERT(si.scope.size() == 1);
	BOOST_ASSERT(si.owner_vars.size() == 0);
	toplevel->finish(da, si);
	da.finish(save_regs, save_reg_dps);
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
	for (int i=0; i<save_regs.size(); ++i) {
		auto sav_e = g.getEntity(save_reg_dps[i]);
		g.genSaveReg(save_regs[i], sav_e.get());
	}
	
	toplevel->gen(g);

	g.genMainReturn();
}	

