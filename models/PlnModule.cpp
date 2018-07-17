/// Module model class definition.
///
/// Module is root of model tree.
/// Module includes function definitions and top level code.
///
/// @file	PlnModule.cpp
/// @copyright	2017 YAMAGUCHI Toshinobu 

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
	PlnType* it = item_type.back();

	string name = PlnType::getFixedArrayName(it, sizes);
	for (auto t: types) 
		if (name == t->name) return t;

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

PlnFunction* PlnModule::getFunc(const string& func_name, vector<PlnExpression*>& args)
{
	PlnFunction* matched_func = NULL;
	int amviguous_count = 0; 
	int is_perfect_match = false; 

	for (auto f: functions) {
		if (f->name == func_name) {
			if (f->parameters.size() < args.size()) {
				// Excluded C and System call for now
				if (f->type == FT_C || f->type == FT_SYS) return f;
				goto next_func;
			}

			int i=0;
			bool do_cast = false;
			for (auto p: f->parameters) {
				if (i+1>args.size() || !args[i]) {
					if (!p->dflt_value) goto next_func;
				} else {
					PlnType* a_type = args[i]->values[0].getType();
					PlnTypeConvCap cap = p->var_type.back()->canConvFrom(a_type);
					if (cap == TC_CANT_CONV) goto next_func;
					if (p->ptr_type == PTR_PARAM_MOVE &&
						args[i]->values[0].asgn_type != ASGN_MOVE)
							goto next_func;
					if (cap != TC_SAME) do_cast = true;
				}
				++i;
			}

			if (is_perfect_match) {
				if (do_cast) goto next_func;
				// else Error: Can't decide a function.
				throw PlnCompileError(E_AmbiguousFuncCall, func_name);

			} else {
				matched_func = f;
				if (do_cast) amviguous_count++;
				else is_perfect_match = true;
			}
		}
next_func:
		;
	}
	
	if (is_perfect_match | amviguous_count == 1) return matched_func;

	if (!matched_func) throw PlnCompileError(E_UndefinedFunction, func_name);
	// if (amviguous_count >= 0)
	throw PlnCompileError(E_AmbiguousFuncCall, func_name);
}

PlnFunction* PlnModule::getFunc(const string& func_name, vector<string>& param_types, vector<string>& ret_types)
{
	for (auto f: functions) {
		if (f->name == func_name
				&& f->parameters.size() == param_types.size()
				&& f->return_vals.size() == ret_types.size()) {

			for (int i=0; i<param_types.size(); i++) {
				string& pt_name = f->parameters[i]->var_type.back()->name;
				if (pt_name != param_types[i]) {
					goto next;
				}
			}

			for (int i=0; i<ret_types.size(); i++) {
				string& rt_name = f->return_vals[i]->var_type.back()->name;
				if (rt_name != ret_types[i]) {
					goto next;
				}
			}
			return f;
		}
next:	;
	}
	
	return NULL;
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
	for (int i=0; i<save_regs.size(); ++i) {
		auto sav_e = g.getEntity(save_reg_dps[i]);
		g.genSaveReg(save_regs[i], sav_e.get());
	}
	
	toplevel->gen(g);

	g.genMainReturn();
}

