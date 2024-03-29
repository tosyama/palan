/// Fixed array type class definition.
///
/// @file	PlnFixedArrayType.cpp
/// @copyright	2019-2022 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include "../../PlnConstants.h"
#include "../PlnModule.h"
#include "../PlnType.h"
#include "../PlnStatement.h"
#include "../PlnBlock.h"
#include "../PlnFunction.h"
#include "../expressions/PlnFunctionCall.h"
#include "PlnFixedArrayType.h"
#include "PlnArrayValueType.h"
#include "../../PlnMessage.h"
#include "../../PlnException.h"
#include "../expressions/PlnMulOperation.h"
#include "../expressions/PlnArrayItem.h"
#include "../../PlnTreeBuildHelper.h"
#include "../expressions/PlnAssignment.h"
#include "../PlnConditionalBranch.h"
#include "../../PlnGenerator.h"
#include "../../PlnDataAllocator.h"
#include "../expressions/PlnMemCopy.h"

static PlnFunction* registerObjectArrayAllocFunc(string func_name, PlnFixedArrayTypeInfo* arr_typeinf, PlnBlock *block)
{
	PlnVarType* it = arr_typeinf->item_type;
	PlnFunction* f = new PlnFunction(FT_PLN, func_name);
	
	f->parent = block->getTypeDefinedBlock(it);

	string s1 = "__r1";
	vector<PlnExpression*> dummy;
	PlnVarType* ret_vtype = arr_typeinf->getVarType("wmr", dummy);
	PlnVariable* ret_var = f->addRetValue(s1, ret_vtype);

	PlnVariable* item_num_var;
	vector<PlnExpression*> next_args;
	vector<PlnVarType*> param_types = { PlnVarType::getUint() };

	BOOST_ASSERT(param_types[0]->data_type() == DT_UINT);
	item_num_var = f->addParam("__item_num", param_types[0], PIO_INPUT, FPM_IN_BYVAL, NULL);

	it->getAllocArgs(next_args);

	f->implement = new PlnBlock();
	f->implement->setParent(f);

	int64_t item_size = arr_typeinf->item_type->size();
	PlnExpression* alloc_size_ex = PlnMulOperation::create(new PlnExpression(item_num_var), new PlnExpression(item_size));
	palan::malloc(f->implement, ret_var, alloc_size_ex);

	// Add item allococation code in loop.
	PlnVariable* var_i = palan::declareUInt(f->implement, "__i", 0);
	PlnBlock* wblock = palan::whileLess(f->implement, var_i, new PlnExpression(item_num_var));
	{
		PlnExpression* arr_item = palan::rawArrayItem(ret_var, var_i);

		if (it->data_type() == DT_OBJECT_REF) {
			arr_item->values[0].asgn_type = ASGN_COPY_REF;
			vector<PlnExpression*> lvals = { arr_item };

			PlnExpression* alloc_ex = it->getAllocEx(next_args);
			vector<PlnExpression*> exps = { alloc_ex };

			auto assign = new PlnAssignment(lvals, exps);
			wblock->statements.push_back(new PlnStatement(assign, wblock));
			
		} else {
			BOOST_ASSERT(it->data_type() == DT_OBJECT);
			PlnExpression* alloc_ex = it->getInternalAllocEx(arr_item, next_args);
			wblock->statements.push_back(new PlnStatement(alloc_ex, wblock));

		}

		palan::incrementUInt(wblock, var_i, 1);
	}

	f->parent->parent_module->functions.push_back(f);

	return f;
}

static PlnFunction* registerObjectArrayInternalAllocFunc(string func_name, PlnFixedArrayTypeInfo* arr_typeinf, PlnBlock *block)
{
	PlnVarType* it = arr_typeinf->item_type;
	PlnFunction* f = new PlnFunction(FT_PLN, func_name);
	
	f->parent = block->getTypeDefinedBlock(it);

	// first paramater
	vector<PlnExpression*> dummy;
	auto arr_var = f->addParam("__arr_var", arr_typeinf->getVarType("wmr", dummy), PIO_INPUT, FPM_IN_BYREF, NULL);
	vector<PlnExpression*> next_args;

	vector<PlnVarType*> param_types = { PlnVarType::getUint() };
	PlnVariable* item_num_var = f->addParam("__item_num", param_types[0], PIO_INPUT, FPM_IN_BYVAL, NULL);

	it->getAllocArgs(next_args);

	f->implement = new PlnBlock();
	f->implement->setParent(f);

	// Add item allococation code in loop.
	PlnVariable* var_i = palan::declareUInt(f->implement, "__i", 0);
	PlnBlock* wblock = palan::whileLess(f->implement, var_i, new PlnExpression(item_num_var));
	{
		PlnExpression* arr_item = palan::rawArrayItem(arr_var, var_i);

		if (it->data_type() == DT_OBJECT_REF) {
			arr_item->values[0].asgn_type = ASGN_COPY_REF;
			vector<PlnExpression*> lvals = { arr_item };

			PlnExpression* alloc_ex = it->getAllocEx(next_args);
			vector<PlnExpression*> exps = { alloc_ex };

			auto assign = new PlnAssignment(lvals, exps);
			wblock->statements.push_back(new PlnStatement(assign, wblock));
			
		} else {
			BOOST_ASSERT(it->data_type() == DT_OBJECT);
			PlnExpression* alloc_ex = it->getInternalAllocEx(arr_item, next_args);
			wblock->statements.push_back(new PlnStatement(alloc_ex, wblock));

		}

		palan::incrementUInt(wblock, var_i, 1);
	}

	f->parent->parent_module->functions.push_back(f);

	return f;
}

static PlnFunction* registerObjectArrayFreeFunc(string func_name, PlnFixedArrayTypeInfo* arr_typeinf, PlnBlock *block, bool is_direct_obj)
{
	PlnVarType* it = arr_typeinf->item_type;
	PlnFunction* f = new PlnFunction(FT_PLN, func_name);

	f->parent = block->getTypeDefinedBlock(it);

	// first paramater
	vector<PlnExpression*> dummy;
	auto to_free_var = f->addParam("__to_free_var", arr_typeinf->getVarType("wmr", dummy), PIO_INPUT, FPM_IN_BYREF, NULL);
	PlnVariable* item_num_var = f->addParam("__item_num", PlnVarType::getUint(), PIO_INPUT, FPM_IN_BYVAL, NULL);

	vector<PlnExpression*> next_args;

	f->implement = new PlnBlock();
	f->implement->setParent(f);

	PlnBlock* wrap_block = f->implement;

	if (!is_direct_obj) {
		// exit if object is already freed.
		wrap_block = new PlnBlock();
		auto if_obj = new PlnIfStatement(new PlnExpression(to_free_var), wrap_block, NULL, f->implement);
		f->implement->statements.push_back(if_obj);
	}

	// Add item free code in loop.
	PlnVariable* var_i = palan::declareUInt(wrap_block, "__i", 0);
	PlnBlock* wblock = palan::whileLess(wrap_block, var_i, new PlnExpression(item_num_var));
	{
		PlnExpression* arr_item = palan::rawArrayItem(to_free_var, var_i);

		PlnExpression* free_ex = NULL;
		if (it->data_type() == DT_OBJECT_REF) {
			free_ex = it->getFreeEx(arr_item);
		} else {
			BOOST_ASSERT(it->data_type() == DT_OBJECT);
			free_ex = it->getInternalFreeEx(arr_item, next_args);
		}

		wblock->statements.push_back(new PlnStatement(free_ex, wblock));

		palan::incrementUInt(wblock, var_i, 1);
	}

	if (!is_direct_obj) {
		palan::free(wrap_block, to_free_var);
	}

	f->parent->parent_module->functions.push_back(f);
	return f;
}

static PlnFunction* registerObjectArrayCopyFunc(string func_name, PlnFixedArrayTypeInfo* arr_typeinf, PlnBlock *block)
{
	PlnVarType* it = arr_typeinf->item_type;
	PlnFunction* f = new PlnFunction(FT_PLN, func_name);

	f->parent = block->getTypeDefinedBlock(it);

	// first paramater
	vector<PlnExpression*> dummy;
	auto src_var = f->addParam("__src_var", arr_typeinf->getVarType("wmr", dummy), PIO_INPUT, FPM_IN_BYREF, NULL);
	auto dst_var = f->addParam("__dst_var", arr_typeinf->getVarType("rmr", dummy), PIO_INPUT, FPM_IN_BYREF, NULL);

	PlnVariable* item_num_var;
	vector<PlnExpression*> next_args;
	vector<PlnVarType*> param_types = { PlnVarType::getUint() };

	BOOST_ASSERT(param_types[0]->data_type() == DT_UINT);
	item_num_var = f->addParam("__item_num", param_types[0], PIO_INPUT, FPM_IN_BYVAL, NULL);

	it->getAllocArgs(next_args);
	f->implement = new PlnBlock();
	f->implement->setParent(f);

	// Add copy code.
	PlnVariable* i = palan::declareUInt(f->implement, "__i", 0);
	PlnBlock* wblock = palan::whileLess(f->implement, i, new PlnExpression(item_num_var));
	{
		PlnExpression* dst_arr_item = palan::rawArrayItem(dst_var, i);
		PlnExpression* src_arr_item = palan::rawArrayItem(src_var, i);
		PlnExpression* copy_item = it->getCopyEx(dst_arr_item, src_arr_item, next_args);
		if (copy_item) {
			wblock->statements.push_back(new PlnStatement(copy_item, wblock));
		}

		palan::incrementUInt(wblock, i, 1);
	}
	f->parent->parent_module->functions.push_back(f);
	return f;
}

PlnFixedArrayTypeInfo::PlnFixedArrayTypeInfo(string &name, PlnVarType* item_type, PlnBlock* parent)
	: PlnTypeInfo(TP_FIXED_ARRAY), item_type(item_type),
	  alloc_func(NULL), internal_alloc_func(NULL), free_func(NULL), internal_free_func(NULL), copy_func(NULL)
{
	this->tname = name;
	this->data_type = DT_OBJECT;
	this->data_size = 0; // undefined size;
	this->data_align = item_type->align();

	auto it = this->item_type;

	if (it->mode[ALLOC_MD] == 'h') {
		has_heap_member = true;

	} else if (it->data_type() == DT_OBJECT) {
		has_heap_member = it->has_heap_member();

	} else {
		has_heap_member = false;
	}

	if (it->typeinf->type == TP_FIXED_ARRAY) {
		vector<int> &item_sizes = static_cast<PlnFixedArrayVarType*>(it)->sizes;
		for (int s: item_sizes)
			if (s == 0) return;	// no alloc functins
	}

	if (has_heap_member) {
		// allocator
		{
			string fname = PlnBlock::generateFuncName("new", {this}, {});
			alloc_func = registerObjectArrayAllocFunc(fname, this, parent);
		}

		// freer
		{
			string fname = PlnBlock::generateFuncName("del", {}, {this});
			free_func = registerObjectArrayFreeFunc(fname, this, parent, false);
		}

		// copyer
		{
			string fname = PlnBlock::generateFuncName("cpy", {}, {this,this});
			copy_func = registerObjectArrayCopyFunc(fname, this, parent);
		}

		// internal_allocator
		{
			string fname = PlnBlock::generateFuncName("internal_new", {this}, {});
			internal_alloc_func = registerObjectArrayInternalAllocFunc(fname, this, parent);
		}

		// internal_freer
		{
			string fname = PlnBlock::generateFuncName("internal_del", {}, {this});
			internal_free_func = registerObjectArrayFreeFunc(fname, this, parent, true);
		}
	}
}

// LCOV_EXCL_START
PlnTypeConvCap PlnFixedArrayTypeInfo::canCopyFrom(const string& mode, PlnVarType *src, PlnAsgnType copymode)
{
	BOOST_ASSERT(false);
}
// LCOV_EXCL_STOP

bool debug_ok = false;
PlnVarType* PlnFixedArrayTypeInfo::getVarType(const string& mode)
{
	BOOST_ASSERT(debug_ok);
	string omode = mode;
	if (mode[0] == '-') omode[0] = default_mode[0];
	if (mode[1] == '-') omode[1] = default_mode[1];
	if (mode[2] == '-') omode[2] = default_mode[2];

	PlnVarType* var_type = new PlnFixedArrayVarType(this, omode);
	var_types.push_back(var_type);

	return var_type;
}

PlnVarType* PlnFixedArrayTypeInfo::getVarType(const string& mode, vector<PlnExpression*> init_args)
{
	debug_ok = true;
	PlnFixedArrayVarType* var_type = static_cast<PlnFixedArrayVarType*>(getVarType(mode));
	debug_ok = false;

	if (!init_args.size()) {
		BOOST_ASSERT(!var_type->sizes.size());
		var_type->sizes.push_back(0);
	} else {
		for (auto arg: init_args) {
			if (arg->type != ET_VALUE) {
				BOOST_ASSERT(false);
			} 
			//BOOST_ASSERT(arg->values[0].size() == 1);
			PlnValue &v = arg->values[0];
			if (v.type == VL_LIT_INT8 || v.type == VL_LIT_UINT8) {
				var_type->sizes.push_back(v.inf.uintValue);
			} else {
				BOOST_ASSERT(false);
			}
		}
	}

	return var_type;
}

string PlnFixedArrayVarType::tname()
{
	string base_tname = PlnTypeInfo::getFixedArrayName(static_cast<PlnFixedArrayTypeInfo*>(typeinf)->item_type, sizes);

	if (mode == "rir" || mode == "rmr") {
		return "@" + base_tname;

	} else if (mode == "wmr" || mode == "wcr") {
		return "@!" + base_tname;

	} else if (mode == "wis") {
		return "#" + base_tname;

	} else {
		BOOST_ASSERT(mode == "wmh");
		return base_tname;
	}
}

int PlnFixedArrayVarType::size()
{
	if (mode[ALLOC_MD] == 'r' || mode[ALLOC_MD] == 'h') {
		return 8;	// pointer_size
	}
	int alloc_size = static_cast<PlnFixedArrayTypeInfo*>(typeinf)->item_type->size();
	for (int s: sizes)
		alloc_size *= s;
	return alloc_size;
}

PlnVarType* PlnFixedArrayVarType::getVarType(const string& mode)
{
	debug_ok = true;
	PlnFixedArrayVarType *vtype = static_cast<PlnFixedArrayVarType*>(typeinf->getVarType(mode));
	debug_ok = false;
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

void PlnFixedArrayVarType::getAllocArgs(vector<PlnExpression*> &alloc_args)
{
	BOOST_ASSERT(sizes.size());
	
	uint64_t sz = 1;
	for (uint64_t n: sizes) {
		sz *= n;
	}
	alloc_args.push_back(new PlnExpression(sz));
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

	if (it->mode[ALLOC_MD] != 'h' && !it->has_heap_member()) {
		// Direct allocation case. Use malloc directly.
		BOOST_ASSERT(args.size() == 1);
		PlnExpression* alloc_size_ex = PlnMulOperation::create(args[0], new PlnExpression(uint64_t(item_type()->size())));
		vector<PlnExpression*> new_args = { alloc_size_ex };
		return new PlnFunctionCall(PlnFunctionCall::getInternalFunc(IFUNC_MALLOC), new_args);

	}

	BOOST_ASSERT(arr_typeinf->alloc_func);
	return new PlnFunctionCall(arr_typeinf->alloc_func, args);
}

PlnExpression* PlnFixedArrayVarType::getInternalAllocEx(PlnExpression* alloc_var, vector<PlnExpression*> &args)
{
	auto arr_typeinf = static_cast<PlnFixedArrayTypeInfo*>(this->typeinf);
	if (!arr_typeinf->internal_alloc_func) return NULL;

	vector<PlnExpression*> alloc_func_args = { alloc_var };
	alloc_func_args.insert(alloc_func_args.end(), args.begin(), args.end());
	return new PlnFunctionCall(arr_typeinf->internal_alloc_func, alloc_func_args);
}

PlnExpression *PlnFixedArrayVarType::getFreeEx(PlnExpression* free_var)
{
	auto arr_typeinf = static_cast<PlnFixedArrayTypeInfo*>(this->typeinf);

	if (!arr_typeinf->has_heap_member) {
		vector<PlnExpression*> free_func_args = { free_var };
		BOOST_ASSERT(free_func_args.size() == 1);
		return new PlnFunctionCall(PlnFunctionCall::getInternalFunc(IFUNC_FREE), free_func_args);
	}

	uint64_t sz = 1;
	for (uint64_t n: sizes) {
		if (n == 0 && arr_typeinf->has_heap_member) {
			BOOST_ASSERT(false);	// Compile error
		}
		sz *= n;
	}

	vector<PlnExpression*> free_func_args = { free_var, new PlnExpression(sz) };
	BOOST_ASSERT(arr_typeinf->free_func);
	return new PlnFunctionCall(arr_typeinf->free_func, free_func_args);
}

PlnExpression* PlnFixedArrayVarType::getInternalFreeEx(PlnExpression* free_var, vector<PlnExpression*> &args) 
{
	auto arr_typeinf = static_cast<PlnFixedArrayTypeInfo*>(this->typeinf);
	if (!arr_typeinf->has_heap_member)
		return NULL;

	BOOST_ASSERT(arr_typeinf->internal_free_func);

	uint64_t sz = 1;
	for (uint64_t n: sizes) {
		if (n == 0 && arr_typeinf->has_heap_member) {
			BOOST_ASSERT(false);	// Compile error
		}
		sz *= n;
	}
	vector<PlnExpression*> free_func_args = { free_var, new PlnExpression(sz) };
	return new PlnFunctionCall(arr_typeinf->internal_free_func, free_func_args);
}

PlnExpression *PlnFixedArrayVarType::getCopyEx(PlnExpression* dst_var, PlnExpression* src_var, vector<PlnExpression*> &args)
{
	auto arr_typeinf = static_cast<PlnFixedArrayTypeInfo*>(this->typeinf);

	auto it = arr_typeinf->item_type;
	uint64_t alloc_size = it->size();
	BOOST_ASSERT(sizes.size());
	for (int s: sizes)
		alloc_size *= s;

	if (alloc_size == 0) return NULL;

	if (!arr_typeinf->has_heap_member) {
		return new PlnMemCopy(dst_var, src_var, new PlnExpression(alloc_size));
	}

	vector<PlnExpression*> copy_func_args = {src_var, dst_var};
	copy_func_args.insert(copy_func_args.end(), args.begin(), args.end());

	return new PlnFunctionCall(arr_typeinf->copy_func, copy_func_args);
}
