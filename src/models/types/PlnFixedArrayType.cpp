/// Fixed array type class definition.
///
/// @file	PlnFixedArrayType.cpp
/// @copyright	2019-2021 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include "../../PlnConstants.h"
#include "../PlnGeneralObject.h"
#include "../PlnModule.h"
#include "../PlnBlock.h"
#include "../PlnFunction.h"
#include "PlnFixedArrayType.h"
#include "PlnArrayValueType.h"
#include "../PlnArray.h"
#include "../../PlnMessage.h"
#include "../../PlnException.h"

PlnFixedArrayType::PlnFixedArrayType(string &name, PlnVarType* item_type, vector<int>& sizes, PlnBlock* parent)
	: PlnType(TP_FIXED_ARRAY), item_type(item_type)
{
	int alloc_size = item_type->size();
	for (int s: sizes)
		alloc_size *= s;

	this->name = name;
	this->data_type = DT_OBJECT;
	this->data_size = alloc_size;
	this->data_align = item_type->align();
	this->sizes = move(sizes);

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
			allocator = new PlnSingleObjectAllocator(alloc_size);
			freer = new PlnSingleObjectFreer();
			copyer = new PlnSingleObjectCopyer(alloc_size);

		} else {	// Array item has original allocator. The case item type is object.
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

	} else {	// The case item type is object reference.
		// allocator
		{
			string fname = PlnBlock::generateFuncName("new", {this}, {});
			PlnFunction* alloc_func = PlnArray::createObjRefArrayAllocFunc(fname, this, parent);
			parent->parent_module->functions.push_back(alloc_func);

			allocator = new PlnNoParamAllocator(alloc_func);
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

PlnTypeConvCap PlnFixedArrayType::canCopyFrom(const string& mode, PlnVarType *src, PlnAsgnType copymode)
{
	if (this == src->typeinf && mode == src->mode)
		return TC_SAME;
	
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

	if (src->typeinf == PlnType::getObject()) {
		return TC_DOWN_CAST;
	}

	if (src->typeinf->type == TP_FIXED_ARRAY) {
		auto src_farr = static_cast<PlnFixedArrayType*>(src->typeinf);
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
				if (sizes.size() != src_farr->sizes.size()) {
					return TC_CANT_CONV;
				}
				for (int i=1; i<sizes.size(); i++) {
					if (sizes[i] != src_farr->sizes[i]) {
						return TC_CANT_CONV;
					}
				}
			}
			cap = TC_UP_CAST;

		} else if (src_farr->sizes[0] == 0) { // Downcast case [?]->[9]
			if (src_farr->sizes.size() != 1) {
				if (sizes.size() != src_farr->sizes.size()) {
					return TC_CANT_CONV;
				}
				for (int i=1; i<sizes.size(); i++) {
					if (sizes[i] != src_farr->sizes[i]) {
						return TC_CANT_CONV;
					}
				}
			}
			cap = TC_DOWN_CAST;

		} else if (sizes != src_farr->sizes) { // size case
			return TC_CANT_CONV;

		}

		// Item type check
		if (item_type->data_type() == DT_OBJECT_REF) {
			return item_type->canCopyFrom(src_farr->item_type, ASGN_COPY);

		} else if (item_type == src_farr->item_type) {
			return cap;

		} else {
			return TC_CANT_CONV;
		}

		return TC_CANT_CONV;
	}

	if (src->typeinf->type == TP_ARRAY_VALUE) {
		return static_cast<PlnArrayValueType*>(src->typeinf)->checkCompatible(item_type, sizes);
	}

	if (src == PlnType::getReadOnlyCStr()->getVarType()) {
		if (item_type->typeinf == PlnType::getByte() && sizes.size() == 1) {
			if (sizes[0])
				return TC_LOSTABLE_AUTO_CAST;
			else // byte[?]
				return TC_AUTO_CAST;

		}
		return TC_CANT_CONV;
	}

	return TC_CANT_CONV;
}
