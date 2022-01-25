/// Fixed array type class definition.
///
/// @file	PlnFixedArrayType.cpp
/// @copyright	2019-2022 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include "../../PlnConstants.h"
#include "../PlnGeneralObject.h"
#include "../PlnModule.h"
#include "../PlnStatement.h"
#include "../PlnBlock.h"
#include "../PlnFunction.h"
#include "../expressions/PlnFunctionCall.h"
#include "PlnFixedArrayType.h"
#include "PlnArrayValueType.h"
#include "../PlnArray.h"
#include "../../PlnMessage.h"
#include "../../PlnException.h"
#include "../expressions/PlnMulOperation.h"
#include "../expressions/PlnArrayItem.h"
#include "../../PlnTreeBuildHelper.h"
#include "../expressions/PlnAssignment.h"

static PlnFunction* createObjRefArrayAllocFunc(string func_name, PlnFixedArrayTypeInfo* arr_typeinf, PlnBlock *block)
{
	PlnVarType* it = arr_typeinf->item_type;
	PlnFunction* f = new PlnFunction(FT_PLN, func_name);
	f->parent = block;	// TODO: ????? Top level? or same as item_type

	string s1 = "__r1";
	PlnVarType* ret_vtype = arr_typeinf->getVarType("wmr");
	static_cast<PlnFixedArrayVarType*>(ret_vtype)->sizes.push_back(0);
	PlnVariable* ret_var = f->addRetValue(s1, ret_vtype);

	PlnVariable* item_num_var;
	vector<PlnExpression*> next_args;
	vector<PlnVarType*> param_types = arr_typeinf->getAllocParamTypes();
	BOOST_ASSERT(param_types.size());

	BOOST_ASSERT(param_types[0]->data_type() == DT_UINT);
	item_num_var = f->addParam("__item_num", param_types[0], PIO_INPUT, FPM_IN_BYVAL, NULL);

	for (int i=1; i<param_types.size(); i++) {
		string pname = "__p" + to_string(i);
		next_args.push_back(
				new PlnExpression(f->addParam(pname, param_types[i], PIO_INPUT, FPM_IN_BYVAL, NULL))
			);
	}

	f->implement = new PlnBlock();
	f->implement->setParent(f);

	int64_t item_size = arr_typeinf->item_type->size();
	PlnExpression* alloc_size_ex = PlnMulOperation::create(new PlnExpression(item_num_var), new PlnExpression(item_size));
	palan::malloc(f->implement, ret_var, alloc_size_ex);

	// add alloc code.
	PlnVariable* var_i = palan::declareUInt(f->implement, "__i", 0);
	PlnBlock* wblock = palan::whileLess(f->implement, var_i, new PlnExpression(item_num_var));
	{
		PlnExpression* arr_item = palan::rawArrayItem(ret_var, var_i, block);
		arr_item->values[0].asgn_type = ASGN_COPY_REF;
		vector<PlnExpression*> lvals = { arr_item };

		PlnExpression* alloc_ex = it->getAllocEx(next_args);
		vector<PlnExpression*> exps = { alloc_ex };

		auto assign = new PlnAssignment(lvals, exps);
		wblock->statements.push_back(new PlnStatement(assign, wblock));

		palan::incrementUInt(wblock, var_i, 1);
	}

	return f;
}

PlnFixedArrayTypeInfo::PlnFixedArrayTypeInfo(string &name, PlnVarType* item_type, vector<int>& sizes, PlnBlock* parent)
	: PlnTypeInfo(TP_FIXED_ARRAY), item_type(item_type), alloc_func(NULL)
{
	int alloc_size = item_type->size();
	for (int s: sizes)
		alloc_size *= s;
	
	this->name = name;
	this->data_type = DT_OBJECT;
	this->data_size = alloc_size;
	this->data_align = item_type->align();

	auto it = this->item_type;

	if (alloc_size == 0) {	// 0(?) size exists.
		// only support free. alloc and copy is not supported because size is undefined.
		// e.g.) raw array reference.
		if (item_type->data_type() != DT_OBJECT_REF) {
			freer = new PlnSingleObjectFreer();
		}
		return;
	}

	if (it->mode[ALLOC_MD] == 'h') {
		has_heap_member = true;

	} else if (it->data_type() == DT_OBJECT) {
		has_heap_member = it->has_heap_member();

	} else {
		has_heap_member = false;
	}

	if (it->mode[ALLOC_MD] != 'h') {
		// Direct allocation case.
		if (!it->has_heap_member()) {
			BOOST_ASSERT(!it->typeinf->internal_allocator);
			freer = new PlnSingleObjectFreer();
			copyer = new PlnSingleObjectCopyer(alloc_size);
	
			BOOST_ASSERT(!alloc_func);

		} else {	// Array item has original allocator. The case item type is object. (DT_OBJECT)
			// allocator
			{
				string fname = PlnBlock::generateFuncName("new", {this}, {});
				PlnFunction* alloc_func = PlnArray::createObjArrayAllocFunc(fname, this, parent);
				parent->parent_module->functions.push_back(alloc_func);

				allocator = new PlnNoParamAllocator(alloc_func);
			}

			// freer
			{
				string fname = PlnBlock::generateFuncName("del", {}, {this});
				PlnFunction* free_func = PlnArray::createObjArrayFreeFunc(fname, this, parent);
				parent->parent_module->functions.push_back(free_func);

				freer = new PlnSingleParamFreer(free_func);
			}

			// temp
			copyer = new PlnSingleObjectCopyer(alloc_size);
		}

	} else {	// The case item type is object reference. (DT_OBJECT_REF)
		// allocator
		{
			string fname = PlnBlock::generateFuncName("new", {this}, {});
			PlnFunction* alloc_func_x = createObjRefArrayAllocFunc(fname, this, parent);
			parent->parent_module->functions.push_back(alloc_func_x);
			this->alloc_func = alloc_func_x;
		}

		// freer
		{
			string fname = PlnBlock::generateFuncName("del", {}, {this});
			PlnFunction* free_func = PlnArray::createObjRefArrayFreeFunc(fname, this, parent);
			parent->parent_module->functions.push_back(free_func);

			freer = new PlnSingleParamFreer(free_func);
		}

		// copyer
		{
			string fname = PlnBlock::generateFuncName("cpy", {}, {this,this});
			PlnFunction* copy_func = PlnArray::createObjRefArrayCopyFunc(fname, this, parent);
			parent->parent_module->functions.push_back(copy_func);

			copyer = new PlnTwoParamsCopyer(copy_func);
		}

		if (item_type->data_type() == DT_OBJECT_REF) {
			// internal_allocator
			{
				string fname = PlnBlock::generateFuncName("internal_new", {this}, {});
				PlnFunction* internal_alloc_func = PlnArray::createObjRefArrayInternalAllocFunc(fname, this, parent);
				parent->parent_module->functions.push_back(internal_alloc_func);

				internal_allocator = new PlnSingleParamInternalAllocator(internal_alloc_func);
			}

			// internal_freer
			{
				string fname = PlnBlock::generateFuncName("internal_del", {}, {this});
				PlnFunction* internal_free_func = PlnArray::createObjRefArrayInternalFreeFunc(fname, this, parent);
				parent->parent_module->functions.push_back(internal_free_func);

				internal_freer = new PlnSingleParamFreer(internal_free_func);
			}
		}
	}
}

PlnTypeConvCap PlnFixedArrayTypeInfo::canCopyFrom(const string& mode, PlnVarType *src, PlnAsgnType copymode)
{
	BOOST_ASSERT(false);
}

PlnVarType* PlnFixedArrayTypeInfo::getVarType(const string& mode)
{
	string omode = mode;
	if (mode[0] == '-') omode[0] = default_mode[0];
	if (mode[1] == '-') omode[1] = default_mode[1];
	if (mode[2] == '-') omode[2] = default_mode[2];

	PlnVarType* var_type = new PlnFixedArrayVarType(this, omode);

	return var_type;
}

vector<PlnVarType*> PlnFixedArrayTypeInfo::getAllocParamTypes()
{
	vector<PlnVarType*> param_types = { PlnVarType::getUint() };

	if (item_type->mode[ALLOC_MD]=='h') {
		vector<PlnVarType*> param_type2 = item_type->typeinf->getAllocParamTypes();
		param_types.insert(param_types.end(), param_type2.begin(), param_type2.end());
	}

	return param_types;
}

PlnVarType* PlnFixedArrayVarType::getVarType(const string& mode)
{
	PlnFixedArrayVarType *vtype = static_cast<PlnFixedArrayVarType*>(typeinf->getVarType(mode));
	vtype->sizes = sizes;

	return vtype;
}

PlnTypeConvCap PlnFixedArrayVarType::canCopyFrom(PlnVarType *src, PlnAsgnType copymode)
{
	// check mode
	if (copymode == ASGN_COPY_REF) {
		if (mode[ACCESS_MD] == 'w' && src->mode[ACCESS_MD] == 'r') {
			return TC_CANT_CONV;
		}
	} else if (copymode == ASGN_MOVE) {
		if (mode[ACCESS_MD] == 'w' && src->mode[ACCESS_MD] == 'r') {
			return TC_CANT_CONV;
		}
	} else
		BOOST_ASSERT(copymode == ASGN_COPY);

	if (src->typeinf->type == TP_FIXED_ARRAY) {
		auto src_farrvt = static_cast<PlnFixedArrayVarType*>(src);
		BOOST_ASSERT(src_farrvt->sizes.size());

		if (typeinf == src->typeinf && sizes == src_farrvt->sizes && mode == src->mode) {
			return TC_SAME;
		}

		// Assumed cases
		// [9,2]int32 -> [9,2]int64: NG
		// [9,2]itemtype -> [9,2]itemtype: OK
		// [9,2]itemtype -> @[9,2]itemtype: OK
		// @[9,2]itemtype -> @[9,2]itemtype: OK
		// @[9,2]itemtype -> [9,2]itemtype: OK copy, NG if address copy ??
		// [9,2]itemtype -> [?,2]itemtype: Upcast
		// [9,2]itemtype -> [?]itemtype: Upcast
		// [?,2]itemtype -> [?,2]itemtype: OK
		// [?,2]itemtype -> [9,2]itemtype: Downcast
		// [9,2]itemtype -> [?,3]itemtype: NG
		// [9,2]itemtype -> [?,2,3]itemtype: NG
		// [9,2,3]itemtype -> [?,2]itemtype: NG
		// [9,2][2]itemtype -> [9,2][?]itemtype: Downcast (basically uses item([2]itemtype) compatibility)

		PlnTypeConvCap cap = TC_AUTO_CAST;
		if (sizes[0] == 0) { // Undefined size case. e.g. [?,2]
			if (sizes.size() != 1) {
				if (sizes.size() != src_farrvt->sizes.size()) {
					return TC_CANT_CONV;
				}
				for (int i=1; i<sizes.size(); i++) {
					if (sizes[i] != src_farrvt->sizes[i]) {
						return TC_CANT_CONV;
					}
				}
			}
			cap = TC_UP_CAST;
		} else if (src_farrvt->sizes[0] == 0) { // Downcast case [?]->[9]
			if (src_farrvt->sizes.size() != 1) {
				if (sizes.size() != src_farrvt->sizes.size()) {
					return TC_CANT_CONV;
				}
				for (int i=1; i<sizes.size(); i++) {
					if (sizes[i] != src_farrvt->sizes[i]) {
						return TC_CANT_CONV;
					}
				}
			}
			cap = TC_DOWN_CAST;

		} else if (sizes != src_farrvt->sizes) { // size case
			return TC_CANT_CONV;
		}

		// Item type check
		PlnVarType *item_type = static_cast<PlnFixedArrayTypeInfo*>(typeinf)->item_type;
		PlnVarType *src_item_type = static_cast<PlnFixedArrayTypeInfo*>(src->typeinf)->item_type;
		if (item_type->data_type() == DT_OBJECT_REF) {
			return item_type->canCopyFrom(src_item_type, ASGN_COPY);

		} else if (item_type == src_item_type) {
			return cap;

		} else {
			return TC_CANT_CONV;
		}

		return TC_CANT_CONV;
	}


	if (src->typeinf == PlnVarType::getObject()->typeinf) {
		return TC_DOWN_CAST;
	}

	if (src->typeinf->type == TP_ARRAY_VALUE) {
		PlnVarType *item_type = this->item_type();
		return static_cast<PlnArrayValueTypeInfo*>(src->typeinf)->checkCompatible(item_type, sizes);
	}

	if (src == PlnVarType::getReadOnlyCStr()) {
		PlnVarType *item_type = this->item_type();
		if (item_type->typeinf == PlnVarType::getByte()->typeinf && sizes.size() == 1) {
			if (sizes[0])
				return TC_LOSTABLE_AUTO_CAST;
			else // byte[?]
				return TC_AUTO_CAST;

		}
		return TC_CANT_CONV;
	}

	return TC_CANT_CONV;
}

void PlnFixedArrayVarType::getInitExpressions(vector<PlnExpression*> &init_exps)
{
	BOOST_ASSERT(sizes.size());
	
	uint64_t sz = 1;
	for (uint64_t n: sizes) {
		sz *= n;
	}
	init_exps.push_back(new PlnExpression(sz));
	if (item_type()->mode[ALLOC_MD]=='h') {
		item_type()->getInitExpressions(init_exps);
	}
}

PlnExpression* PlnFixedArrayVarType::getAllocEx(vector<PlnExpression*> &args)
{
	auto arr_typeinf = static_cast<PlnFixedArrayTypeInfo*>(this->typeinf);
	auto it = arr_typeinf->item_type;
	int64_t alloc_size = it->size();
	BOOST_ASSERT(sizes.size());
	for (int s: sizes)
		alloc_size *= s;

	if (alloc_size == 0) return NULL;

	if (it->mode[ALLOC_MD] != 'h') {
		// Direct allocation case.
		if (!it->has_heap_member()) {
			BOOST_ASSERT(args.size() == 1);
			PlnExpression* alloc_size_ex = PlnMulOperation::create(args[0], new PlnExpression(uint64_t(item_type()->size())));
			vector<PlnExpression*> new_args = { alloc_size_ex };
			return new PlnFunctionCall(PlnFunctionCall::getInternalFunc(IFUNC_MALLOC), new_args);

		} else {
		}

	} else {
		BOOST_ASSERT(arr_typeinf->alloc_func);
		return new PlnFunctionCall(arr_typeinf->alloc_func, args);
	}

	return typeinf->allocator->getAllocEx(args);
}

