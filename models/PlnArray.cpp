/// Array model class definition.
///
/// @file	PlnArray.cpp
/// @copyright	2018-2019 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>

#include "../PlnConstants.h"
#include "PlnType.h"
#include "../PlnTreeBuildHelper.h"
#include "PlnModule.h"
#include "types/PlnFixedArrayType.h"
#include "PlnArray.h"
#include "PlnBlock.h"
#include "PlnFunction.h"
#include "PlnStatement.h"
#include "PlnVariable.h"
#include "PlnConditionalBranch.h"
#include "expressions/PlnArrayItem.h"
#include "expressions/PlnAssignment.h"

PlnFunction* PlnArray::createObjArrayAllocFunc(string func_name, PlnFixedArrayType* arr_type, vector<PlnType*> &arr_type2, PlnModule* module)
{
	PlnType* t = arr_type;
	PlnType* it = arr_type->item_type;
	int item_num = t->inf.obj.alloc_size / t->inf.fixedarray.item_size;

	PlnFunction* f = new PlnFunction(FT_PLN, func_name);
	string s1 = "__p1";
	PlnVariable* ret_var = f->addRetValue(s1, t, false);

	f->implement = new PlnBlock();
	f->implement->setParent(f);
	
	palan::malloc(f->implement, ret_var, t->inf.obj.alloc_size);

	// add alloc code.
	PlnVariable* i = palan::declareUInt(f->implement, "__i", 0);
	PlnBlock* wblock = palan::whileLess(f->implement, i, item_num);
	{
		BOOST_ASSERT(it->allocator);
		PlnExpression* arr_item = palan::rawArrayItem(ret_var, i, module);
		arr_item->values[0].asgn_type = ASGN_COPY_REF;
		vector<PlnExpression*> lvals = { arr_item };
		PlnExpression* alloc_ex = it->allocator->getAllocEx();
		vector<PlnExpression*> exps = { alloc_ex };

		auto assign = new PlnAssignment(lvals, exps);
		wblock->statements.push_back(new PlnStatement(assign, wblock));

		palan::incrementUInt(wblock, i, 1);
	}

	return f;
}

PlnFunction* PlnArray::createObjArrayFreeFunc(string func_name, vector<PlnType*> &arr_type2, PlnModule *module)
{
	PlnType* arr_type = arr_type2.back();
	PlnType* it = arr_type2[arr_type2.size()-2];
	int item_num = arr_type->inf.obj.alloc_size / arr_type->inf.fixedarray.item_size;

	PlnFunction* f = new PlnFunction(FT_PLN, func_name);
	string s1 = "p1";
	f->addParam(s1, arr_type, FPM_REF, NULL);

	f->implement = new PlnBlock();
	f->implement->setParent(f);

	// Return if object address is 0.
	auto ifblock = new PlnBlock();
	auto if_obj = new PlnIfStatement(new PlnExpression(f->parameters[0]), ifblock, NULL, f->implement);
	f->implement->statements.push_back(if_obj);

	// Add free code.
	PlnVariable* i = palan::declareUInt(ifblock, "i", 0);
	PlnBlock* wblock = palan::whileLess(ifblock, i, item_num);
	{
		BOOST_ASSERT(it->freer);
		PlnExpression* arr_item = palan::rawArrayItem(f->parameters[0], i, module);
		PlnExpression* free_item = it->freer->getFreeEx(arr_item);
		wblock->statements.push_back(new PlnStatement(free_item, wblock));

		palan::incrementUInt(wblock, i, 1);
	}

	palan::free(ifblock, f->parameters[0]);

	return f;
}

PlnFunction* PlnArray::createObjArrayCopyFunc(string func_name, vector<PlnType*> &arr_type, PlnModule *module)
{
	PlnType* t = arr_type.back();
	PlnType* it = arr_type[arr_type.size()-2];
	int item_num = t->inf.obj.alloc_size / t->inf.fixedarray.item_size;

	vector<PlnType*> src_arr_type = arr_type;
	PlnFunction* f = new PlnFunction(FT_PLN, func_name);
	string s1 = "p1", s2 = "p2";
	f->addParam(s1, t, FPM_REF, NULL);
	f->addParam(s2, src_arr_type.back(), FPM_REF, NULL);

	f->implement = new PlnBlock();
	f->implement->setParent(f);

	// Add copy code.
	PlnVariable* i = palan::declareUInt(f->implement, "i", 0);
	PlnBlock* wblock = palan::whileLess(f->implement, i, item_num);
	{
		PlnExpression* dst_arr_item = palan::rawArrayItem(f->parameters[0], i, module);
		PlnExpression* src_arr_item = palan::rawArrayItem(f->parameters[1], i, module);
		if (it->copyer) {
			PlnExpression* copy_item = it->copyer->getCopyEx(dst_arr_item, src_arr_item);
			wblock->statements.push_back(new PlnStatement(copy_item, wblock));
		}

		palan::incrementUInt(wblock, i, 1);
	}

	return f;
}
