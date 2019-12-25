/// Module model class definition.
///
/// Module is root of model tree.
/// Module includes function definitions and top level code.
///
/// @file	PlnModule.cpp
/// @copyright	2017-2019 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include "PlnModule.h"
#include "PlnBlock.h"
#include "PlnFunction.h"
#include "PlnVariable.h"
#include "PlnStatement.h"
#include "PlnType.h"
#include "../PlnDataAllocator.h"
#include "../PlnGenerator.h"
#include "../PlnScopeStack.h"
#include "../PlnConstants.h"
#include "../PlnTreeBuildHelper.h"

using namespace std;

PlnModule::PlnModule()
{
	toplevel = new PlnBlock();
	PlnType::initBasicTypes();
	toplevel->parent_module = this;
	max_jmp_id = -1;
}

PlnModule::~PlnModule()
{
	for (auto f: functions)
		delete f;
}

int PlnModule::getJumpID()
{
	return ++max_jmp_id;
}

void PlnModule::gen(PlnDataAllocator& da, PlnGenerator& g)
{
	for (auto f : functions)
		f->genAsmName();

	PlnScopeInfo si;
	si.scope.push_back(PlnScopeItem(this));

	g.genSecReadOnlyData();
	g.genSecText();

	palan::exit(toplevel, 0);
	toplevel->finish(da, si);
	da.finish(save_regs, save_reg_dps);
	int stack_size = da.stack_size;

	// gen toplevel as main function
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
	da.reset();
	g.genMainReturn();
	g.comment("end of toplevel");
	g.comment("");
	g.genEndFunc();

	// Generate assembly of only the functions called.
	// Note: call_count will be incremented at FunctionCall finising.
	while(true) {
		PlnFunction *f = NULL;
		for (auto func : functions) {
			if (!func->generated && func->call_count > 0) {
				f = func;
				break;
			}
		}

		if (!f) break;

		f->finish(da, si);
		f->gen(g);
		// BOOST_ASSERT(f->name!="f1");
		f->clear();
		da.reset();
		f->generated = true;
	}

	for (auto f : functions) {
		if (!f->generated) {
			// Do only finishing to detect code error
			f->finish(da, si);
			f->clear();
			da.reset();
		}
	}
	
	BOOST_ASSERT(si.scope.size() == 1);
	BOOST_ASSERT(si.owner_vars.size() == 0);

	delete toplevel;
	toplevel = NULL;
}

