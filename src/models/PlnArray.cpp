/// Array model class definition.
///
/// @file	PlnArray.cpp
/// @copyright	2018-2022 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>

#include "../PlnConstants.h"
#include "PlnType.h"
#include "../PlnTreeBuildHelper.h"
#include "types/PlnFixedArrayType.h"
#include "PlnArray.h"
#include "PlnBlock.h"
#include "PlnFunction.h"
#include "PlnStatement.h"
#include "PlnVariable.h"
#include "PlnConditionalBranch.h"
#include "expressions/PlnArrayItem.h"
#include "expressions/PlnAssignment.h"

static int item_num_of(vector<int> & sizes)
{
	int n = 1;
	for (int i: sizes) {
		n *= i;
	}
	return n;
}

PlnFunction* PlnArray::createObjRefArrayAllocFunc(string func_name, PlnFixedArrayTypeInfo* arr_type, vector<int> &sizes, PlnBlock* block)
{
	PlnVarType* it = arr_type->item_type;
	// int item_num = arr_type->data_size / it->size();
	int item_num = item_num_of(sizes);

	PlnFunction* f = new PlnFunction(FT_PLN, func_name);
	f->parent = block;
	string s1 = "__p1";
	PlnVarType* ret_vtype = arr_type->getVarType("wmr");
	static_cast<PlnFixedArrayVarType*>(ret_vtype)->sizes = sizes;
	PlnVariable* ret_var = f->addRetValue(s1, ret_vtype);

	f->implement = new PlnBlock();
	f->implement->setParent(f);
	
	// palan::malloc(f->implement, ret_var, arr_type->data_size);
	palan::malloc(f->implement, ret_var, item_num * it->size());

	// add alloc code.
	PlnVariable* i = palan::declareUInt(f->implement, "__i", 0);
	PlnBlock* wblock = palan::whileLess(f->implement, i, item_num);
	{
		PlnExpression* arr_item = palan::rawArrayItem(ret_var, i, block);
		arr_item->values[0].asgn_type = ASGN_COPY_REF;
		vector<PlnExpression*> lvals = { arr_item };
		PlnExpression* alloc_ex = it->getAllocEx();
		BOOST_ASSERT(alloc_ex);
		vector<PlnExpression*> exps = { alloc_ex };

		auto assign = new PlnAssignment(lvals, exps);
		wblock->statements.push_back(new PlnStatement(assign, wblock));

		palan::incrementUInt(wblock, i, 1);
	}

	return f;
}

PlnFunction* PlnArray::createObjRefArrayInternalAllocFunc(string func_name, PlnFixedArrayTypeInfo* arr_type, PlnBlock *block)
{
	PlnVarType* it = arr_type->item_type;
	int item_num = arr_type->data_size / it->size();

	PlnFunction* f = new PlnFunction(FT_PLN, func_name);
	f->parent = block;
	string s1 = "__p1";
	PlnVariable* param_var = f->addParam(s1, arr_type->getVarType("wir"), PIO_INPUT, FPM_IN_BYREF, NULL);

	f->implement = new PlnBlock();
	f->implement->setParent(f);

	PlnVariable* i = palan::declareUInt(f->implement, "__i", 0);
	PlnBlock* wblock = palan::whileLess(f->implement, i, item_num);
	{
		PlnExpression* arr_item = palan::rawArrayItem(param_var, i, block);
		arr_item->values[0].asgn_type = ASGN_COPY_REF;
		vector<PlnExpression*> lvals = { arr_item };
		PlnExpression* alloc_ex = it->getAllocEx();
		BOOST_ASSERT(alloc_ex);
		vector<PlnExpression*> exps = { alloc_ex };

		auto assign = new PlnAssignment(lvals, exps);
		wblock->statements.push_back(new PlnStatement(assign, wblock));

		palan::incrementUInt(wblock, i, 1);
	}

	return f;
}

PlnFunction* PlnArray::createObjRefArrayFreeFunc(string func_name, PlnFixedArrayTypeInfo* arr_type, PlnBlock *block)
{
	PlnVarType* it = arr_type->item_type;
	int item_num = arr_type->data_size / it->size();

	PlnFunction* f = new PlnFunction(FT_PLN, func_name);
	f->parent = block;
	string s1 = "__p1";
	f->addParam(s1, arr_type->getVarType("wir"), PIO_INPUT, FPM_IN_BYREF, NULL);

	f->implement = new PlnBlock();
	f->implement->setParent(f);

	// Return if object address is 0.
	auto p1_var = f->parameters[0]->var;
	auto ifblock = new PlnBlock();
	auto if_obj = new PlnIfStatement(new PlnExpression(p1_var), ifblock, NULL, f->implement);
	f->implement->statements.push_back(if_obj);

	// Add free code.
	PlnVariable* i = palan::declareUInt(ifblock, "__i", 0);
	PlnBlock* wblock = palan::whileLess(ifblock, i, item_num);
	{
		PlnExpression* arr_item = palan::rawArrayItem(p1_var, i, block);
		PlnExpression* free_item = it->getFreeEx(arr_item);
		BOOST_ASSERT(free_item);
		wblock->statements.push_back(new PlnStatement(free_item, wblock));

		palan::incrementUInt(wblock, i, 1);
	}

	palan::free(ifblock, p1_var);

	return f;
}

PlnFunction* PlnArray::createObjRefArrayInternalFreeFunc(string func_name, PlnFixedArrayTypeInfo* arr_type, PlnBlock *block)
{
	PlnVarType* it = arr_type->item_type;
	int item_num = arr_type->data_size / it->size();

	PlnFunction* f = new PlnFunction(FT_PLN, func_name);
	f->parent = block;
	string s1 = "__p1";
	f->addParam(s1, arr_type->getVarType("wir"), PIO_INPUT, FPM_IN_BYREF, NULL);

	f->implement = new PlnBlock();
	f->implement->setParent(f);

	// Return if object address is 0.
	auto p1_var = f->parameters[0]->var;
	auto ifblock = new PlnBlock();
	auto if_obj = new PlnIfStatement(new PlnExpression(p1_var), ifblock, NULL, f->implement);
	f->implement->statements.push_back(if_obj);

	// Add free code.
	PlnVariable* i = palan::declareUInt(ifblock, "__i", 0);
	PlnBlock* wblock = palan::whileLess(ifblock, i, item_num);
	{
		PlnExpression* arr_item = palan::rawArrayItem(p1_var, i, block);
		PlnExpression* free_item = it->getFreeEx(arr_item);
		BOOST_ASSERT(free_item);
		wblock->statements.push_back(new PlnStatement(free_item, wblock));

		palan::incrementUInt(wblock, i, 1);
	}

	return f;
}

PlnFunction* PlnArray::createObjRefArrayCopyFunc(string func_name, PlnFixedArrayTypeInfo* arr_type, PlnBlock *block)
{
	PlnVarType* it = arr_type->item_type;
	int item_num = arr_type->data_size / it->size();

	PlnFunction* f = new PlnFunction(FT_PLN, func_name);
	f->parent = block;
	string s1 = "__p1", s2 = "__p2";

	f->addParam(s1, arr_type->getVarType("wir"), PIO_INPUT, FPM_IN_BYREF, NULL);
	f->addParam(s2, arr_type->getVarType("rir"), PIO_INPUT, FPM_IN_BYREF, NULL);

	f->implement = new PlnBlock();
	f->implement->setParent(f);

	// Add copy code.
	PlnVariable* i = palan::declareUInt(f->implement, "__i", 0);
	PlnBlock* wblock = palan::whileLess(f->implement, i, item_num);
	{
		PlnExpression* dst_arr_item = palan::rawArrayItem(f->parameters[0]->var, i, block);
		PlnExpression* src_arr_item = palan::rawArrayItem(f->parameters[1]->var, i, block);
		PlnExpression* copy_item = it->getCopyEx(dst_arr_item, src_arr_item);
		if (copy_item) {
			wblock->statements.push_back(new PlnStatement(copy_item, wblock));
		}

		palan::incrementUInt(wblock, i, 1);
	}

	return f;
}

PlnFunction* PlnArray::createObjArrayAllocFunc(string func_name, PlnFixedArrayTypeInfo* arr_type, PlnBlock *block)
{
	PlnVarType* it = arr_type->item_type;
	int item_num = arr_type->data_size / it->size();

	PlnFunction* f = new PlnFunction(FT_PLN, func_name);
	f->parent = block;
	string s1 = "__p1";
	PlnVariable* ret_var = f->addRetValue(s1, arr_type->getVarType("wmr"));

	f->implement = new PlnBlock();
	f->implement->setParent(f);
	
	palan::malloc(f->implement, ret_var, arr_type->data_size);

	PlnVariable* i = palan::declareUInt(f->implement, "__i", 0);
	PlnBlock* wblock = palan::whileLess(f->implement, i, item_num);
	{
		PlnExpression* arr_item = palan::rawArrayItem(ret_var, i, block);

		PlnExpression* alloc_ex = it->getInternalAllocEx(arr_item);
		BOOST_ASSERT(alloc_ex);
		wblock->statements.push_back(new PlnStatement(alloc_ex, wblock));
		palan::incrementUInt(wblock, i, 1);
	}

	return f;
}

PlnFunction* PlnArray::createObjArrayFreeFunc(string func_name, PlnFixedArrayTypeInfo* arr_type, PlnBlock *block)
{
	PlnVarType* it = arr_type->item_type;
	int item_num = arr_type->data_size / it->size();

	PlnFunction* f = new PlnFunction(FT_PLN, func_name);
	f->parent = block;
	string s1 = "__p1";
	f->addParam(s1, arr_type->getVarType("wir"), PIO_INPUT, FPM_IN_BYREF, NULL);

	f->implement = new PlnBlock();
	f->implement->setParent(f);

	// Return if object address is 0.
	auto p1_var = f->parameters[0]->var;
	auto ifblock = new PlnBlock();
	auto if_obj = new PlnIfStatement(new PlnExpression(p1_var), ifblock, NULL, f->implement);
	f->implement->statements.push_back(if_obj);

	// Add free code.
	PlnVariable* i = palan::declareUInt(ifblock, "__i", 0);
	PlnBlock* wblock = palan::whileLess(ifblock, i, item_num);
	{
		PlnExpression* arr_item = palan::rawArrayItem(p1_var, i, block);
		PlnExpression* free_item = it->getInternalFreeEx(arr_item);
		BOOST_ASSERT(free_item);
		wblock->statements.push_back(new PlnStatement(free_item, wblock));

		palan::incrementUInt(wblock, i, 1);
	}

	palan::free(ifblock, p1_var);

	return f;
}

