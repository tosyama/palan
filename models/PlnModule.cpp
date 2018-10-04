/// Module model class definition.
///
/// Module is root of model tree.
/// Module includes function definitions and top level code.
///
/// @file	PlnModule.cpp
/// @copyright	2017-2018 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include <boost/algorithm/string.hpp>
#include "PlnModule.h"
#include "PlnBlock.h"
#include "PlnFunction.h"
#include "PlnVariable.h"
#include "PlnStatement.h"
#include "PlnArray.h"
#include "PlnGeneralObject.h"
#include "../PlnDataAllocator.h"
#include "../PlnGenerator.h"
#include "../PlnScopeStack.h"
#include "../PlnConstants.h"
#include "../PlnMessage.h"
#include "../PlnException.h"
#include "../PlnTreeBuildHelper.h"

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

string createFuncName(string fname, vector<PlnType*> ret_types, vector<PlnType*> arg_types)
{
	string ret_name, arg_name;
	for (auto t: ret_types)
		ret_name += "_" + t->name;
	for (auto t: arg_types)
		arg_name += "_" + t->name;
	boost::replace_all(fname, "_", "__");

	fname = "_" + fname + ret_name + + "_" +arg_name;
	boost::replace_all(fname, "[", "A_");
	boost::replace_all(fname, "]", "_");
	boost::replace_all(fname, ",", "_");
	return fname;
}

PlnType* PlnModule::getFixedArrayType(vector<PlnType*> &item_type, vector<int>& sizes)
{
	string name = PlnType::getFixedArrayName(item_type, sizes);
	for (auto t: types) 
		if (name == t->name) return t;

	PlnType* it = item_type.back();
	int item_size = it->size;
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

	// set allocator & freer.
	if (it->data_type != DT_OBJECT_REF) {
		t->allocator = new PlnSingleObjectAllocator(alloc_size);
		t->freer = new PlnSingleObjectFreer();
		t->copyer = new PlnSingleObjectCopyer(alloc_size);

	} else {
		// allocator
		{
			string fname = createFuncName("new", {t}, {});
			PlnFunction* alloc_func = NULL;
			for (auto f: functions) {
				if (f->name == fname) {
					alloc_func = f;
					break;
				}
			}
			if (!alloc_func) {
				vector<PlnType*> type_def = item_type;
				type_def.push_back(t);
				alloc_func = PlnArray::createObjArrayAllocFunc(fname, type_def);
				functions.push_back(alloc_func);
			}

			t->allocator = new PlnNoParamAllocator(alloc_func);
		}

		// freer
		{
			string fname = createFuncName("del", {}, {t});
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
				free_func = PlnArray::createObjArrayFreeFunc(fname, type_def);
				functions.push_back(free_func);
			}
			t->freer = new PlnSingleParamFreer(free_func);
		}

		// copyer
		{
			string fname = createFuncName("cpy", {}, {t,t});
			PlnFunction* copy_func = NULL;
			for (auto f: functions) {
				if (f->name == fname) {
					copy_func = f;
					break;
				}
			}
			if (!copy_func) {
				vector<PlnType*> type_def = item_type;
				type_def.push_back(t);
				copy_func = PlnArray::createObjArrayCopyFunc(fname, type_def);
				functions.push_back(copy_func);
			}
			t->copyer = new PlnTwoParamsCopyer(copy_func);
			
		}
	}

	types.push_back(t);

	return t;
}

int PlnModule::getJumpID()
{
	return ++max_jmp_id;
}

PlnReadOnlyData* PlnModule::getReadOnlyData(const string &str)
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
	palan::exit(toplevel, 0);
	toplevel->finish(da, si);
	da.finish(save_regs, save_reg_dps);
	stack_size = da.stack_size;
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
	// No need to save reg at top level.
/*	for (int i=0; i<save_regs.size(); ++i) {
		auto sav_e = g.getEntity(save_reg_dps[i]);
		g.genSaveReg(save_regs[i], sav_e.get());
	} */
	
	toplevel->gen(g);

	g.genMainReturn();
}

