/// Fixed array type class definition.
///
/// @file	PlnFixedArrayType.cpp
/// @copyright	2019 YAMAGUCHI Toshinobu 

#include <boost/assert.hpp>
#include "../../PlnConstants.h"
#include "../PlnGeneralObject.h"
#include "../PlnModule.h"
#include "../PlnBlock.h"
#include "../PlnFunction.h"
#include "PlnFixedArrayType.h"
#include "PlnArrayValueType.h"
#include "../PlnArray.h"

PlnFixedArrayType::PlnFixedArrayType(string &name, PlnVarType* item_type, vector<int>& sizes, PlnBlock* parent)
	: PlnType(TP_FIXED_ARRAY), item_type(item_type)
{
	int alloc_size = item_type->size();
	for (int s: sizes)
		alloc_size *= s;

	this->name = name;
	this->data_type = DT_OBJECT_REF;
	this->size = 8;
	this->inf.obj.is_fixed_size = true;
	this->inf.obj.alloc_size = alloc_size;
	this->sizes = move(sizes);

	auto it = this->item_type;
	
	if (alloc_size == 0) {
		// raw array reference.
		freer = new PlnSingleObjectFreer();
		// TODO: confirm it's OK when data_type is object.
		return;
	}

	if (it->mode[ALLOC_MD] != 'h') {
		allocator = new PlnSingleObjectAllocator(alloc_size);
		freer = new PlnSingleObjectFreer();
		copyer = new PlnSingleObjectCopyer(alloc_size);

	} else {
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

		// copyer
		{
			string fname = PlnBlock::generateFuncName("cpy", {}, {this,this});
			PlnFunction* copy_func = PlnArray::createObjArrayCopyFunc(fname, this, parent);
			parent->parent_module->functions.push_back(copy_func);
			copyer = new PlnTwoParamsCopyer(copy_func);
		}
	}
}

PlnTypeConvCap PlnFixedArrayType::canCopyFrom(const string& mode, PlnVarType *src)
{
	if (this == src->typeinf)
		return TC_SAME;

	if (src->typeinf == PlnType::getObject()) {
		return TC_DOWN_CAST;
	}

	if (src->typeinf->type == TP_FIXED_ARRAY) {
		auto src_farr = static_cast<PlnFixedArrayType*>(src->typeinf);
		if (item_type == src_farr->item_type) {
			if (!sizes[0]) {	// xx[n] -> xx[?]
				return TC_AUTO_CAST; // It has risk to break data.
			} else if (!src_farr->sizes[0]) {	// xx[?] -> xx[n]
				BOOST_ASSERT(false);
				// return TC_DOWN_CAST;
			}
		} else {	// Difference only mode of item.
			if (sizes == src_farr->sizes) {
				// return item_type->typeinf->canConvFrom(item_type->mode, src_farr->item_type);
				if (item_type->typeinf == src_farr->item_type->typeinf)
					return TC_AUTO_CAST;

				if (item_type->typeinf->type == TP_FIXED_ARRAY)
					return item_type->canCopyFrom(src_farr->item_type);
			}
		}
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
	}

	return TC_CANT_CONV;
}
