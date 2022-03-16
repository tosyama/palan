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

PlnFunction* PlnArray::createObjRefArrayCopyFunc(string func_name, PlnFixedArrayTypeInfo* arr_type, PlnBlock *block)
{
	PlnVarType* it = arr_type->item_type;
	uint64_t item_num = arr_type->data_size / it->size();

	PlnFunction* f = new PlnFunction(FT_PLN, func_name);
	f->parent = block;
	string s1 = "__p1", s2 = "__p2";

	f->addParam(s1, arr_type->getVarType("wir"), PIO_INPUT, FPM_IN_BYREF, NULL);
	f->addParam(s2, arr_type->getVarType("rir"), PIO_INPUT, FPM_IN_BYREF, NULL);

	f->implement = new PlnBlock();
	f->implement->setParent(f);

	// Add copy code.
	PlnVariable* i = palan::declareUInt(f->implement, "__i", 0);
	PlnBlock* wblock = palan::whileLess(f->implement, i, new PlnExpression(item_num));
	{
		PlnExpression* dst_arr_item = palan::rawArrayItem(f->parameters[0]->var, i, block);
		PlnExpression* src_arr_item = palan::rawArrayItem(f->parameters[1]->var, i, block);
		vector<PlnExpression*> args;
		it->getAllocArgs(args);
		PlnExpression* copy_item = it->getCopyEx(dst_arr_item, src_arr_item, args);
		if (copy_item) {
			wblock->statements.push_back(new PlnStatement(copy_item, wblock));
		}

		palan::incrementUInt(wblock, i, 1);
	}

	return f;
}

