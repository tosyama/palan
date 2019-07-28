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
	toplevel->types = PlnType::getBasicTypes();
	toplevel->parent_module = this;
	// stack_size = 0;
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
	PlnScopeInfo si;
	si.scope.push_back(PlnScopeItem(this));

	g.genSecReadOnlyData();
	g.genSecText();

	for (auto f : functions)
		f->genAsmName();

	for (auto f : functions) {
		f->finish(da, si);
		f->gen(g);
		f->clear();
		da.reset();
	}
	
	BOOST_ASSERT(si.scope.size() == 1);
	BOOST_ASSERT(si.owner_vars.size() == 0);
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
	delete toplevel;
	toplevel = NULL;

	g.genMainReturn();
	g.genEndFunc();
}

